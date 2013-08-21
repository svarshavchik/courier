/*
** Copyright 1998 - 2009 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<signal.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#if HAVE_SYS_FILE_H
#include	<sys/file.h>
#endif
#if HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<errno.h>
#include	<pwd.h>
#if TIME_WITH_SYS_TIME
#include	<sys/time.h>
#include	<time.h>
#else
#if HAVE_SYS_TIME_H
#include	<sys/time.h>
#else
#include	<time.h>
#endif
#endif
#if HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if HAVE_SYS_WAIT_H
#include	<sys/wait.h>
#endif
#include	<courier.h>
#if HAVE_SYSEXITS_H
#include	<sysexits.h>
#else
#define	EX_SOFTWARE	70
#define	EX_TEMPFAIL	75
#define	EX_NOUSER	67
#define	EX_NOPERM	77
#endif

#include	"rfc822/rfc822.h"
#include	"maildir/maildircreate.h"
#include	"maildir/maildirquota.h"
#include	"liblock/config.h"
#include	"liblock/liblock.h"
#include	"liblock/mail.h"


extern char *local_dotcourier(const char *, const char *, const char **);
extern char *local_extension();

static int chkdelto(FILE *, const char *);
static void dodel(const char *, const char *, FILE *, char *,
	const char *, const char *, const char *, const char *, const char *,
	const char *, int);

int main(int argc, char **argv)
{
const char *extension;
const char *receipient;
const char *sender;
const char *defaultext;
const char *quota;
const char *defaultmail;
char *extname;
char *ctlfile;
struct stat	stat_buf;
char		*username;
char		*userhome;

	if (argc < 6)	_exit(1);
	clog_open_syslog("deliver");

	username=argv[1];
	userhome=argv[2];

	extension=argv[3];
	receipient=argv[4];
	sender=argv[5];
	quota= argc > 6 ? argv[6]:"";
	defaultmail= argc > 7 ? argv[7]:"";

	if (argc > 8)
		set_courierdir(argv[8]);	/* Used by make check only! */

	extname=local_dotcourier(".", extension, &defaultext);

	if (extname == 0 && extension[0])
	{
		if (errno == ENOENT)
		{
			fprintf(stderr, "Unknown user.\n");
			exit(EX_NOUSER);
		}

		fprintf(stderr, "Unable to read .%s file.\n",
					local_extension());
		exit(EX_TEMPFAIL);
	}

	if (extname == 0)
	{
		ctlfile=0;
	}
	else
	{
		ctlfile=readfile(extname, &stat_buf);
		free(extname);
		extname=0;
	}

	if (ctlfile && (stat_buf.st_mode & S_IWOTH))
	{
		fprintf(stderr, ".%s file is globally writeable.\n",
			local_extension());
		free(ctlfile);
		if (extname)	free(extname);
		exit(EX_TEMPFAIL);
	}

	if (chkdelto(stdin, receipient))
	{
		fprintf(stderr,
			"Mail loop - already have my Delivered-To: header.");
		if (ctlfile)	free(ctlfile);
		if (extname)	free(extname);
		exit (EX_SOFTWARE);
	}

	dodel(username, userhome, stdin,
		ctlfile, extension, sender, receipient, defaultext, quota,
		defaultmail, 0);
	if (ctlfile)	free(ctlfile);
	if (extname)	free(extname);
	exit(0);
	return (0);
}

static int chkdelto(FILE *f, const char *delivto)
{
char	buf[BUFSIZ];
char	*p, *q;

	/* Check for mail loops, by looking for my Delivered-To: line. */

	while ((p=fgets(buf, sizeof(buf), f)) != 0)
	{
		if ((p=strchr(buf, '\n')) != 0)
			*p=0;
		else
		{
		int	c;

			while ((c=getc(f)) >= 0 && c != '\n')
				;
		}

		if (buf[0] == '\0')	break;
		if ((p=strchr(buf, ':')) == 0)	continue;
		*p++=0;
		for (q=buf; *q; q++)
			*q=tolower(*q);
		if (strcmp(buf, "delivered-to"))	continue;
		while (*p == ' ')
			++p;
		if (strcmp(p, delivto) == 0)
			return (1);
	}
	return (0);
}

static int savemessage(const char *, const char *, const char *,
		FILE *, const char *,
		const char *, const char *, const char *, const char *);

static int docommand(const char *, const char *, const char *, const char *,
		FILE *, const char *, const char *,
		const char *, const char *, const char *, const char *,
		const char *, const char *, int);

/* Skip to end of line in command file.  Follow \ - terminated lines */

static char *skip_eol(char *ctl, int zapbs)
{
char	*bs;

	while (*ctl)
	{
		if (*ctl == '\n')
		{
			*ctl++=0;
			break;
		}

		if (*ctl++ != '\\' || *ctl == '\0')
			continue;
		bs=ctl-1;
		if (*ctl != ' ' && *ctl != '\t' && *ctl != '\n')
		{
			if (*ctl)	++ctl;
			continue;
		}
		while (*ctl == ' ' || *ctl == '\t')
			++ctl;
		if (*ctl != '\n')
			continue;
		if (zapbs)
		{
			*bs=' ';
			*ctl=' ';
		}
		++ctl;
	}
	return (ctl);
}

/* Minor helper function */

static void dodel(const char *username, const char *userhome,
	FILE *c, char *ctl,
	const char *extension, const char *sender, const char *receipient,
	const char *defaultext, const char *quota, const char *defaultmail,
	int recursion_level)
{
char	*ufromline;
char	*dtline;
char	*rpline;
time_t	t;
const char *curtime;

	time(&t);
	curtime=ctime(&t);
	if ((ufromline=malloc(strlen(curtime)+strlen(sender)+30))==0||
		(dtline=malloc(strlen(receipient)+
			sizeof("Delivered-To: "))) == 0 ||
		(rpline=malloc(strlen(sender) +
			sizeof("Return-Path: <>"))) == 0)
	{
		perror("malloc");
		exit(EX_TEMPFAIL);
	}

	if ( (!ctl || !*ctl) && recursion_level == 0)
	{
	const char *p= *defaultmail ? defaultmail:config_defaultdelivery();

		if ((ctl=malloc(strlen(p)+1)) == 0)
		{
			perror("malloc");
			exit(EX_TEMPFAIL);
		}
		strcpy(ctl, p);
	}

	sprintf(ufromline, "From %s %s", sender, curtime);

	{
	char *p;

		if ((p=strchr(ufromline, '\n')) != 0)
			*p=0;
	}

	strcat(strcpy(dtline, "Delivered-To: "), receipient);
	strcat(strcat(strcpy(rpline, "Return-Path: <"), sender), ">");

	while (*ctl)
	{
		if (*ctl == '#' || *ctl == '\n')
		{
			while (*ctl)
				if (*ctl++ == '\n')	break;
			continue;
		}

		/*
		** The fd hack is needed for BSD, whose C lib doesn't like
		** mixing stdio and unistd seek calls.
		*/

		if (*ctl == '.' || *ctl == '/')
		{
		const char *filename=ctl;
		int fd_hack=dup(fileno(stdin));
		FILE *fp_hack;

			if (fd_hack < 0 || lseek(fd_hack, 0L, SEEK_SET) < 0 ||
				(fp_hack=fdopen(fd_hack, "r")) == NULL)
			{
				perror("dup");
				exit(EX_TEMPFAIL);
			}

			while (*ctl)
			{
				if (*ctl == '/' && (ctl[1] == '\n' ||
					ctl[1] == 0))
					*ctl=0; /* Strip trailing / */
				if (*ctl == '\n')
				{
					*ctl++='\0';
					break;
				}
				++ctl;
			}

			if (savemessage(extension, sender, receipient,
				fp_hack, filename,
				ufromline,
				dtline,
				rpline, quota))
				exit(EX_TEMPFAIL);
			fclose(fp_hack);
			close(fd_hack);
			continue;
		}
		if (*ctl == '|')
		{
		const char *command=++ctl;
		int	rc;
		int fd_hack=dup(fileno(stdin));
		FILE *fp_hack;

			if (fd_hack < 0 || lseek(fd_hack, 0L, SEEK_SET) < 0 ||
				(fp_hack=fdopen(fd_hack, "r")) == NULL)
			{
				perror("dup");
				exit(EX_TEMPFAIL);
			}

			ctl=skip_eol(ctl, 0);
			while (*command == ' ' || *command == '\t')
				++command;

			rc=docommand(extension, sender, receipient, defaultext,
				fp_hack, username, userhome, command,
				dtline,
				rpline,
				ufromline,
				quota, defaultmail,
				recursion_level);
			if (rc)
				exit(rc);
			fclose(fp_hack);
			close(fd_hack);
			continue;
		}

		/* Forwarding instructions, parse RFC822 addresses */

		if (*ctl == '&' || *ctl == '!')	++ctl;	/* Legacy */
		{
			const char *addresses=ctl;
			struct rfc822t *tokens;
			struct rfc822a *addrlist;
			int n;

			ctl=skip_eol(ctl, 1);
			if ((tokens=rfc822t_alloc_new(addresses, NULL,
						      NULL)) == 0 ||
				(addrlist=rfc822a_alloc(tokens)) == 0)
			{
				perror("malloc");
				exit(EX_TEMPFAIL);
			}

			for (n=0; n<addrlist->naddrs; ++n)
			{
				char *p;

				if (addrlist->addrs[n].tokens == NULL)
					continue;

				p=rfc822_display_addr_tobuf(addrlist, n,
							    NULL);

				if (!p)
				{
					perror(addresses);
					exit(EX_TEMPFAIL);
				}

				printf("%s\n", p);
				free(p);
			}
			rfc822a_free(addrlist);
			rfc822t_free(tokens);
			fflush(stdout);
		}
	}
			
	free(rpline);
	free(dtline);
	free(ufromline);
}

static void delivery_error(const char *name)
{
	if (errno == ENOSPC)
	{
		fprintf(stderr, "Maildir over quota.\n");
		exit(EX_TEMPFAIL);
	}

#if	HAVE_STRERROR

	fprintf(stderr, "%s: %s\n", name, strerror(errno));
#else
	fprintf(stderr, "%s: error %d\n", name, errno);
#endif
	exit (EX_TEMPFAIL);
}

static off_t mbox_size;
static int mbox_fd;

static RETSIGTYPE truncmbox(int signum)
{
#if	HAVE_FTRUNCATE
	if (ftruncate(mbox_fd, mbox_size) < 0)
		; /* ignore */
#else
	/* Better than nothing */
	write(mbox_fd, "\n", 1);
#endif
	_exit(0);
#if	RETSIGTYPE != void
	return (0);
#endif
}

static int savemessage(const char *extension, const char *sender,
		const char *receipient,
		FILE *f, const char *name,
		const char *ufromline,
		const char *dtline,
		const char *rpline,
		const char *quota)
{
FILE	*delivf;
char	buf[BUFSIZ];
int	c;
static unsigned counter=0;
struct	stat stat_buf;
struct maildirsize quotainfo;
struct maildir_tmpcreate_info createInfo;

	umask(077);

	if ((delivf=fopen(name, "a")) != 0)
	{
		/* Ok, perhaps this is a mailbox */

		struct ll_mail *ll=ll_mail_alloc(name);

		fclose(delivf);

		if (!ll)
		{
			delivery_error(name);	/* I Give up */
		}

		while ((mbox_fd=ll_mail_open(ll)) < 0)
		{
			switch (errno) {
			case EEXIST:
			case EAGAIN:
				sleep(5);
				continue;
				break;
			}

			delivery_error(name);
		}

		delivf=fdopen(mbox_fd, "w");

		if (delivf == NULL || fseek(delivf, 0L, SEEK_END) < 0
		    || (mbox_size=ftell(delivf)) == (off_t)-1)
		{
			if (delivf)
				fclose(delivf);
			close(mbox_fd);
			ll_mail_free(ll);
			delivery_error(name);	/* I Give up */
		}

		signal(SIGHUP, truncmbox);
		signal(SIGINT, truncmbox);
		signal(SIGQUIT, truncmbox);
		signal(SIGTERM, truncmbox);

		fprintf(delivf, "%s\n%s\n%s\n",
			ufromline,
			dtline,
			rpline);

		while (fgets(buf, sizeof(buf), f) != 0)
		{
		char	*q=buf;

			while (*q == '>')	q++;
			if (strncmp(q, "From ", 5) == 0)
				putc('>', delivf);
			fprintf(delivf, "%s", buf);
			if (strchr(buf, '\n') == 0)
			{
				while ((c=getc(f)) >= 0)
				{
					putc(c, delivf);
					if (c == '\n')	break;
				}
			}
		}

		if ( ferror(f) || fflush(delivf) || ferror(delivf)
#if	EXPLICITSYNC
			|| fsync(fileno(delivf))
#endif
			||
				(signal(SIGHUP, SIG_DFL),
				signal(SIGINT, SIG_DFL),
				signal(SIGQUIT, SIG_DFL),
				signal(SIGTERM, SIG_DFL),
				fclose(delivf))
			)
		{
#if	HAVE_FTRUNCATE
			if (ftruncate(mbox_fd, mbox_size) < 0)
				; /* ignore */
#endif
			close(mbox_fd);
			ll_mail_free(ll);
			delivery_error("mailbox.close");
		}
		close(mbox_fd);
		ll_mail_free(ll);
		return (0);
	}

	if (fstat(fileno(f), &stat_buf))
	{
		delivery_error("stat");
		return (-1);
	}

	stat_buf.st_size += strlen(dtline) + strlen(rpline) + 2;

	if (maildir_quota_add_start(name, &quotainfo, stat_buf.st_size, 1,
				    quota && *quota != 0 ? quota:NULL))
	{
		errno=ENOSPC;
		delivery_error("out of memory.");
	}

	maildir_closequotafile(&quotainfo);	/* For now. */

	sprintf(buf, "%u", ++counter);

	maildir_tmpcreate_init(&createInfo);
	createInfo.maildir=name;
	createInfo.uniq=buf;
	createInfo.msgsize=stat_buf.st_size;
	createInfo.doordie=1;

	if ((delivf=maildir_tmpcreate_fp(&createInfo)) == NULL)
	{
		snprintf(buf, BUFSIZ-1,
			 "maildir.open: %s: %s", name,
			 strerror(errno));
		buf[BUFSIZ-1] = 0;
		delivery_error(buf);
		return (-1);
	}

	fprintf(delivf, "%s\n%s\n", dtline, rpline);

	{
		char buffer[BUFSIZ];
		int n;

		while ((n=fread(buffer, 1, sizeof(buffer), f)) > 0)
			if (fwrite(buffer, 1, n, delivf)  != n)
				break;
	}

	if ( ferror(f) || fflush(delivf) || ferror(delivf)
#if	EXPLICITSYNC
			|| fsync(fileno(delivf))
#endif
			|| fclose(delivf)
			|| (delivf=0, maildir_movetmpnew(createInfo.tmpname,
							 createInfo.newname)))
	{
		snprintf(buf, BUFSIZ-1,
			 "maildir.close: %s: %s", name,
			 strerror(errno));
		buf[BUFSIZ-1] = 0;

		if (delivf)	fclose(delivf);
		unlink(createInfo.newname);
		delivery_error(buf);
		return (-1);
	}

#if EXPLICITDIRSYNC

	{
		int fd;

		strcpy(strrchr(createInfo.newname, '/')+1, ".");

		fd=open(createInfo.newname, O_RDONLY);

		if (fd >= 0)
		{
			fsync(fd);
			close(fd);
		}
	}
#endif

	maildir_tmpcreate_free(&createInfo);

	maildir_quota_deleted(name, stat_buf.st_size, 1);
	return (0);
}

static char *read_command(int);

static int docommand(const char *extension, const char *sender,
		const char *receipient, const char *defaultext,
		FILE *f, const char *username, const char *userhome,
		const char *command,
		const char *dtline,
		const char *rpline,
		const char *ufromline,
		const char *quota,
		const char *defaultmail,
		int recursion_level)
{
char	*envs[19];
const char *p;
const char *hostp;
pid_t	pid;
int	i;
int	wait_stat;
int	pipefd[2];
int	isrecursive=0;
char	*newcommand=0;
const char *maildropdefault=getenv("MAILDROPDEFAULT");
const char *shell=getenv("SHELL");

	if (!maildropdefault)
		maildropdefault="./Maildir";
	if (!shell)
		shell="/bin/sh";

	envs[0]=courier_malloc(strlen(userhome)+sizeof("HOME="));
	strcat(strcpy(envs[0], "HOME="), userhome);
	envs[1]=courier_malloc(strlen(username)+sizeof("USER="));
	strcat(strcpy(envs[1], "USER="), username);
	envs[2]=courier_malloc(strlen(sender)+sizeof("SENDER="));
	strcat(strcpy(envs[2], "SENDER="), sender);
	envs[3]=courier_malloc(strlen(receipient)+sizeof("RECIPIENT="));
	strcat(strcpy(envs[3], "RECIPIENT="), receipient);

	p=strrchr(receipient, '@');
	if (p)	hostp=p+1;
	else
	{
		hostp="localhost";
		p=receipient+strlen(receipient);
	}

	envs[4]=courier_malloc(strlen(hostp)+sizeof("HOST="));
	strcat(strcpy(envs[4], "HOST="), hostp);
	envs[5]=courier_malloc(p-receipient + sizeof("LOCAL="));
	strcpy(envs[5], "LOCAL=");
	memcpy(envs[5]+6, receipient,  p-receipient);
	envs[5][6+(p-receipient)]=0;

	envs[6]=courier_malloc(strlen(extension)+sizeof("EXT="));
	strcat(strcpy(envs[6], "EXT="), extension);

	p=strchr(extension, '-');
	if (p)	p++;

	envs[7]=courier_malloc((p ? strlen(p):0)+sizeof("EXT2="));
	strcat(strcpy(envs[7], "EXT2="), p ? p:"");

	if (p)
		p=strchr(p, '-');
	if (p)	p++;

	envs[8]=courier_malloc((p ? strlen(p):0)+sizeof("EXT3="));
	strcat(strcpy(envs[8], "EXT3="), p ? p:"");

	if (p)
		p=strchr(p, '-');
	if (p)	p++;

	envs[9]=courier_malloc((p ? strlen(p):0)+sizeof("EXT4="));
	strcat(strcpy(envs[9], "EXT4="), p ? p:"");

	envs[10]=courier_malloc((defaultext ? strlen(defaultext):0)+sizeof("DEFAULT="));
	strcat(strcpy(envs[10], "DEFAULT="), defaultext ? defaultext:"");

	envs[11]=courier_malloc(strlen(dtline)+sizeof("DTLINE="));
	strcat(strcpy(envs[11], "DTLINE="), dtline);

	envs[12]=courier_malloc(strlen(rpline)+sizeof("RPLINE="));
	strcat(strcpy(envs[12], "RPLINE="), rpline);

	envs[13]=courier_malloc(strlen(ufromline)+sizeof("UFLINE="));
	strcat(strcpy(envs[13], "UFLINE="), ufromline);

	p=getenv("PATH");
	if (!p)	p="/bin:/usr/bin:/usr/local/bin";

	envs[14]=courier_malloc(strlen(p)+sizeof("PATH="));
	strcat(strcpy(envs[14], "PATH="), p);

	envs[15]=courier_malloc(strlen(quota)+sizeof("MAILDIRQUOTA="));
	strcat(strcpy(envs[15], "MAILDIRQUOTA="), quota);

	envs[16]=courier_malloc(strlen(maildropdefault)
				+sizeof("MAILDROPDEFAULT="));
	strcat(strcpy(envs[16], "MAILDROPDEFAULT="), maildropdefault);

	envs[17]=courier_malloc(strlen(shell)
				+sizeof("SHELL="));
	strcat(strcpy(envs[17], "SHELL="), shell);
	envs[18]=0;

	if (*command == '|')
	{
		isrecursive=1;
		++command;
		while (*command == ' ' || *command == '\t')
			++command;
		if (pipe(pipefd) < 0)
		{
			clog_msg_prerrno();
			exit(EX_TEMPFAIL);
		}
	}

	if ((pid=fork()) < 0)
	{
		clog_msg_prerrno();
		exit(EX_TEMPFAIL);
	}

	if (pid == 0)
	{
	const char *args[8];

		/*
		** External commands have stdout redirected to stderr,
		** and stdin set to the message.
		*/
		dup2(2, 1);
		if (fileno(f) != 0)
		{
			dup2(fileno(f), 0);
			fclose(f);
		}

		if (isrecursive)
		{
		const char *c;

			dup2(pipefd[1], 1);
			close(pipefd[0]);
			close(pipefd[1]);
			if (recursion_level >= 3)
			{
				fprintf(stderr, "Maximum recursion level for dynamic delivery instructions exceeded.\n");
				fflush(stderr);
				_exit(EX_NOUSER);
			}

			if ((c=getenv("DYNAMICDELIVERIES")) != 0 &&
				atoi(c) == 0)
			{
				fprintf(stderr, "Dynamic delivery instructions disabled by administrator.\n");
				fflush(stderr);
				_exit(EX_NOUSER);
			}
		}

		if ((p=config_maildropmda()) != 0 && *p &&
			strcmp(p, command) == 0)
		{
			/* Special magic for maildrop */

			args[0]="maildrop";
			args[1]="-A";
			args[2]=dtline;
			args[3]="-A";
			args[4]=rpline;
			args[5]="-f";
			args[6]=sender;
			args[7]=0;
		}
		else
		{
			p=getenv("SHELL");
			if (!p || !*p)	p="/bin/sh";

			args[0]=p;
			args[1]="-c";
			args[2]=command;
			args[3]=0;
		}
		lseek(0, 0L, SEEK_SET);

		for (i=0; envs[i]; ++i)
			putenv(envs[i]);

		execv(p, (char **)args);
		fprintf(stderr, "Cannot run %s\n", p);
		_exit(EX_TEMPFAIL);
	}
	for (i=0; envs[i]; i++)
		free(envs[i]);

	if (isrecursive)
	{
		close(pipefd[1]);
		newcommand=read_command(pipefd[0]);
		close(pipefd[0]);
	}

	while (wait(&wait_stat) != pid)
		;

	if (WIFEXITED(wait_stat))
	{
	int	rc=WEXITSTATUS(wait_stat);

		if (rc == 0 || rc == 99)
		{
			if (isrecursive)
				dodel(username, userhome, f, newcommand,
					extension, sender, receipient,
					defaultext, quota, defaultmail,
					recursion_level+1);
		}
		if (newcommand)	free(newcommand);

		return (rc);
	}
	if (newcommand)	free(newcommand);

	return (EX_NOPERM);
}

static char *read_command(int fd)
{
char	buf[BUFSIZ];
int	n;
char	*p;

	n=0;
	while (n<sizeof(buf)-1)
	{
	int	c=read(fd, buf+n, sizeof(buf)-n-1);

		if (c <= 0)	break;
		n += c;
	}
	close(fd);
	buf[n]=0;
	p=strdup(buf);
	if (!p)
	{
		clog_msg_prerrno();
		exit(EX_TEMPFAIL);
	}
	return (p);
}

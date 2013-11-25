/*
** Copyright 1998 - 2004 Double Precision, Inc.
** See COPYING for distribution information.
**
*/

#include	"courier.h"
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	"comsubmitclient.h"
#include	"maxlongsize.h"
#include	"numlib/numlib.h"
#include	"sbindir.h"
#include	<pwd.h>
#include	<stdio.h>
#include	<ctype.h>
#include	<signal.h>
#if	HAVE_SYSEXITS_H
#include	<sysexits.h>
#else

#define	EX_NOUSER	67
#define	EX_TEMPFAIL	75
#define	EX_NOPERM	77
#endif

#if	HAVE_LOCALE_H
#include	<locale.h>
#endif

#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>

extern void esmtpd();


static struct passwd *mypwd()
{
static struct passwd *pwd=0;
int	first_pwd=1;

	if (first_pwd)
	{
		first_pwd=0;
		pwd=getpwuid(getuid());
	}

	return (pwd);
}

static const char *env(const char *envname)
{
const char *p=getenv(envname);

	if (p && !*p)	p=0;
	return (p);
}

static char *rewrite_env_sender(const char *from)
{
const char *host;

	if (!from)
	{
		if ((from=env("MAILSUSER")) == 0 &&
			(from=env("MAILUSER")) == 0 &&
			(from=env("LOGNAME")) == 0 &&
			(from=env("USER")) == 0)
		{
		struct passwd *pw=mypwd();

			from=pw ? pw->pw_name:"nobody";
		}

		if ((host=env("MAILSHOST")) != 0 ||
			(host=env("MAILHOST")) != 0)
		{
		char	*p=courier_malloc(strlen(from)+strlen(host)+2);

			return (strcat(strcat(strcpy(p, from), "@"), host));
		}
	}
	return (strcpy(courier_malloc(strlen(from)+1), from));
}

static char *rewrite_from(const char *, const char *, const char *,
		const char *);

static struct rfc822t *tokenize_name(const char *name)
{
struct rfc822t *p=rw_rewrite_tokenize(name);
struct rfc822token *t;

	for (t=p->tokens; t; t=t->next)
		if (t->token)	break;
	if (t == 0)	return (p);	/* Only atoms */
	if (p->tokens == 0)	return (p);

	p->tokens->token='"';
	p->tokens->next=0;
	p->tokens->ptr=name;
	p->tokens->len=strlen(name);
	return (p);
}

static char *get_gecos()
{
struct passwd *pw=mypwd();
char	*p, *q;

	if (!pw || !pw->pw_gecos)	return (0);

	p=strcpy(courier_malloc(strlen(pw->pw_gecos)+1), pw->pw_gecos);

	if ((q=strchr(p, ',')) != 0)	*q=0;
	return (p);
}

static void rewrite_headers(const char *From)
{
int	seen_from=0;
char	headerbuf[5000];
int	c, i;
const char *mailuser, *mailuser2, *mailhost;
char	*p;
char	*pfrom=From ? strcpy(courier_malloc(strlen(From)+1), From):0;

	if ((mailuser=env("MAILUSER")) == 0 &&
		(mailuser=env("LOGNAME")) == 0)
		mailuser=env("USER");
	mailuser2=env("MAILUSER");
	mailhost=env("MAILHOST");

	while (fgets(headerbuf, sizeof(headerbuf), stdin))
	{
	char	*p=strchr(headerbuf, '\n');

		if (p)
		{
			*p=0;
			if (p == headerbuf || strcmp(headerbuf, "\r") == 0)
				break;
		}

#if HAVE_STRNCASECMP
		if (strncasecmp(headerbuf, "from:", 5))
#else
		if (strnicmp(headerbuf, "from:", 5))
#endif
		{
			fprintf(submit_to, "%s", headerbuf);
			if (!p)
				while ((c=getchar()) != EOF && c != '\n')
					putc(c, submit_to);
			putc('\n', submit_to);
			continue;
		}
		if (!p)
			while ((c=getchar()) != EOF && c != '\n')
				;	/* I don't care */
		if (seen_from)	continue;	/* Screwit */
		seen_from=1;

		i=strlen(headerbuf);
		for (;;)
		{
			c=getchar();
			if (c != EOF)	ungetc(c, stdin);
			if (c == EOF || c == '\r' || c == '\n')	break;
			if (!isspace((int)(unsigned char)c))	break;
			while ((c=getchar()) != EOF && c != '\n')
			{
				if (i < sizeof(headerbuf)-1)
					headerbuf[i++]=c;
			}
			headerbuf[i]=0;
		}

		p=rewrite_from(headerbuf+5, mailuser2, mailhost, pfrom);
		fprintf(submit_to, "From: %s\n", p);
		free(p);
	}
	if (!seen_from)
	{
		if (!mailuser)
		{
		struct passwd *pw=mypwd();

			mailuser=pw ? pw->pw_name:"nobody";
		}

		if (!pfrom)
		{
			if ( !(From=env("MAILNAME")) && !(From=env("NAME")))
			{
				pfrom=get_gecos();
			}
			else	pfrom=strcpy(courier_malloc(strlen(From)+1),
                                        From);
		}

		p=rewrite_from(NULL, mailuser, mailhost, pfrom);
		fprintf(submit_to, "From: %s\n", p);
		free(p);
	}
	putc('\n', submit_to);
	if (pfrom)	free(pfrom);
}

static char *rewrite_from(const char *oldfrom, const char *newuser,
	const char *newhost, const char *newname)
{
struct rfc822t *rfct;
struct rfc822a *rfca;
struct rfc822t *usert, *hostt, *namet;
struct rfc822token attoken, **tp;
char	*p;
const char *q;
char	*gecosname=0;

	if (!oldfrom)
	{
	char	*p=courier_malloc(
			(newuser ? strlen(newuser):0)+
			(newhost ? strlen(newhost):0)+4);
		strcpy(p, "<");
		if (newuser)	strcat(p, newuser);
		if (newuser && newhost)
			strcat(strcat(p, "@"), newhost);
		strcat(p, ">");
		if (newname)
		{
		char *q, *r;

			namet=tokenize_name(newname);
			q=rfc822_gettok(namet->tokens);
			rfc822t_free(namet);
			r=courier_malloc(strlen(p)+strlen(q)+2);
			strcat(strcat(strcpy(r, q), " "), p);
			free(p);
			p=r;
			free(q);
		}
		return (p);
	}

	if ((rfct=rfc822t_alloc_new(oldfrom, NULL, NULL)) == 0 ||
		(rfca=rfc822a_alloc(rfct)) == 0)
	{
		clog_msg_errno();
		return(0);
	}

	if ((q=env("MAILNAME")) || (q=env("NAME")))
		newname=q;

	if (!newname && rfca->naddrs == 0)
		newname=gecosname=get_gecos();

	if ((rfca->naddrs == 0 || rfca->addrs[0].tokens == 0) && newuser == 0)
	{
	struct	passwd *pw=mypwd();

		if (pw)	newuser=pw->pw_name;
	}

	namet=newname ? tokenize_name(newname):0;
	usert=newuser ? rw_rewrite_tokenize(newuser):0;
	hostt=newhost ? rw_rewrite_tokenize(newhost):0;

	if (rfca->naddrs == 0 || rfca->addrs[0].tokens == 0)
	{
	struct rfc822addr a;
	struct rfc822a	fakea;

		if (hostt)
		{
		struct rfc822token *t;

			attoken.token='@';
			attoken.next=hostt->tokens;
			attoken.ptr=0;
			attoken.len=0;

			for (t=usert->tokens; t->next; t=t->next)
				;
			t->next=&attoken;
		}
		fakea.naddrs=1;
		fakea.addrs= &a;

		if (!namet)	namet=tokenize_name("");
		if (!usert)	usert=rw_rewrite_tokenize("");
		a.name=namet->tokens;
		a.tokens=usert->tokens;
		p=rfc822_getaddrs(&fakea);
	}
	else
	{
	struct	rfc822token *t, *u;

		rfca->naddrs=1;
		if (usert)
		{
			for (t=rfca->addrs[0].tokens; t; t=t->next)
				if (t->token == '@')	break;
			
			for (u=usert->tokens; u->next; u=u->next)
				;
			u->next=t;
			rfca->addrs[0].tokens=usert->tokens;;
		}

		if (hostt && rfca->addrs[0].tokens)
		{
			for (tp= &rfca->addrs[0].tokens; *tp;
				tp= &(*tp)->next)
				if ( (*tp)->token == '@')	break;
			*tp=&attoken;
			attoken.token='@';
			attoken.next=hostt->tokens;
			attoken.ptr=0;
			attoken.len=0;
		}
		if (namet)
			rfca->addrs[0].name=namet->tokens;

		p=rfc822_getaddrs(rfca);
	}

	if (!p)	clog_msg_errno();

	if (usert)	rfc822t_free(usert);
	if (hostt)	rfc822t_free(hostt);
	if (namet)	rfc822t_free(namet);
	rfc822t_free(rfct);
	rfc822a_free(rfca);
	if (gecosname)	free(gecosname);
	return (p);
}

static void exit_submit(int errcode)
{
	if (errcode == -5)
		exit(EX_NOUSER);

	exit(EX_TEMPFAIL);
}

int main(int argc, char **argv)
{
const char *from=0;
const char *From=0;
const char *ret=0, *dsn=0, *security=0, *envid=0;
int	argn, argp;
int	errflag=0;
int	c;
char	*args[6];
char	*envs[7];
int	envp;
int	tostdout=0;
char	frombuf[NUMBUFSIZE+30];
int	doverp=0;
int	bcconly=0;
char	ubuf[NUMBUFSIZE];
char	*uucprmail=0;
char	*s;
char	ret_buf[2], security_buf[40];
int     submit_errcode;

	/*
	** Immediately drop uid, force GID to MAILGID
	** The reason we can't just setgid ourselves to MAILGID is because
	** that only sets the effective uid.  We need to also set both
	** real and effective GIDs, otherwise maildrop filtering will fail,
	** because the real gid won't be trusted.
	*/
	if (setgid(MAILGID) < 0 ||
	    setuid(getuid()) < 0)
	{
		perror("setuid/setgid");
		exit(EX_NOPERM);
	}

	signal(SIGCHLD, SIG_DFL);
	signal(SIGPIPE, SIG_IGN);
	argn=1;

	putenv("AUTHMODULES="); /* See module.local/local.c */

	putenv("LANG=en_US");
	putenv("CHARSET=iso-8859-1");
	putenv("MM_CHARSET=iso-8859-1");

#if HAVE_SETLOCALE
	setlocale(LC_ALL, "C");
#endif

	/* Only uucp can run rmail (this better be installed setgid) */

	if ((s=strrchr(argv[0], '/')) != 0)	++s;
	else	s=argv[0];
	if (strcmp(s, "rmail") == 0)
	{
	struct passwd	*p=mypwd();
	char	*uu_machine, *uu_user;

		if (!p || strcmp(p->pw_name, "uucp"))
		{
			fprintf(stderr, "rmail: permission denied.\n");
			exit(EX_NOPERM);
		}

		uu_machine=getenv("UU_MACHINE");
		uu_user=getenv("UU_USER");

		if (!uu_machine || !uu_user)
		{
			fprintf(stderr,
				"rmail: UU_MACHINE!UU_USER required.\n");
			exit(EX_NOPERM);
		}
		uucprmail=malloc(strlen(uu_machine)+strlen(uu_user)+2);
		if (!uucprmail)
		{
			perror("malloc");
			exit(EX_TEMPFAIL);
		}
		strcat(strcat(strcpy(uucprmail, uu_machine), "!"), uu_user);
	}

	while (argn < argc)
	{
	const char *arg;
	int	c;

		if (argv[argn][0] != '-')	break;

		if (strcmp(argv[argn], "-") == 0)
		{
			++argn;
			break;
		}

		if (uucprmail && strncmp(argv[argn], "-f", 2)

		/*
		** Ok, obviously no UUCP version will give me the following
		** options, must I can hope, can't I?
		*/
			&& strcmp(argv[argn], "-verp")
			&& strncmp(argv[argn], "-N", 2)
			&& strncmp(argv[argn], "-R", 2)
			&& strncmp(argv[argn], "-V", 2)

			/* Ignore the ignorable */

			&& strncmp(argv[argn], "-o", 2)
			&& strncmp(argv[argn], "-t", 2)

			)
		{
			fprintf(stderr, "rmail: invalid option %s\n",
				argv[argn]);
			exit(EX_NOPERM);
		}

		if (strcmp(argv[argn], "-verp") == 0)
		{
			doverp=1;
			++argn;
			continue;
		}

		if (strcmp(argv[argn], "-bcc") == 0)
		{
			bcconly=1;
			++argn;
			continue;
		}

		if (strcmp(argv[argn], "-bs") == 0)
		{
			esmtpd();
		}
		switch (c=argv[argn][1])	{
		case 'o':
		case 'f':
		case 'F':
		case 'R':
		case 'N':
		case 'S':
		case 'V':
			break;
		case 'n':
			tostdout=1;
			++argn;
			continue;
		default:
			++argn;
			continue;
		}
		arg=argv[argn]+2;
		if (!*arg && argn+1 < argc)
			arg=argv[++argn];
		++argn;

		if (c == 'f')
			from=arg;
		else if (c == 'F')
			From=arg;
		else if (c == 'N')
			dsn=arg;
		else if (c == 'S')
		{
			char *q;

			if (strcasecmp(arg, "NONE") &&
			    strcasecmp(arg, "STARTTLS"))
			{
				fprintf(stderr, "sendmail: invalid option"
					" -S %s\n",
					arg);
				exit(EX_NOPERM);
			}
			strcpy(security_buf, arg);

			for (q=security_buf; *q; q++)
				*q=toupper((int)(unsigned char)*q);

			security=security_buf;
		}
		else if (c == 'R')
		{
			ret_buf[0]= toupper((int)(unsigned char)*arg);
			ret_buf[1]= 0;
			ret=ret_buf;
		}
		else if (c == 'V')
			envid=arg;
	}

	sprintf(frombuf, "uid %s", libmail_str_uid_t(getuid(), ubuf));
	argp=0;
	args[argp++]="submit";
	if (bcconly)
		args[argp++]="-bcc";

	if (uucprmail)
	{
		if (argn >= argc)
		{
			fprintf(stderr, "rmail: missing recipients\n");
			exit(EX_NOPERM);
		}

		args[argp++]="uucp";
		s=malloc(sizeof("unknown; uucp ()")+strlen(uucprmail));
		if (!s)
		{
			perror("malloc");
			exit(EX_TEMPFAIL);
		}
		strcat(strcat(strcpy(s, "unknown; uucp ("), uucprmail), ")");
		args[argp++]=s;
	}
	else
	{
		args[argp++]="local";
		args[argp++]="dns; localhost (localhost [127.0.0.1])";
		args[argp++]=frombuf;
	}
	args[argp++]=0;
	envp=0;

	clog_open_stderr("sendmail");
	if (ret || security || doverp)
	{
		envs[envp]=strcat(strcat(strcpy(courier_malloc(strlen(ret ?
			ret:"")+strlen(security ? security:"") + 30),
						"DSNRET="), (ret ? ret:"")),
				  (doverp ? "V":""));
		if (security)
			strcat(strcat(strcat(envs[envp], "S{"),
				      security), "}");

		++envp;
	}

	if (dsn)
	{
		envs[envp]=strcat(strcpy(courier_malloc(strlen(dsn)+11),
			"DSNNOTIFY="), dsn);
		++envp;
	}
	if (envid)
	{
		envs[envp]=strcat(strcpy(courier_malloc(strlen(envid)+10),
			"DSNENVID="), envid);
		++envp;
	}
	envs[envp++]="TCPREMOTEHOST=localhost";
	envs[envp++]="TCPREMOTEIP=127.0.0.1";

	if (!uucprmail)
		envs[envp++]=strcat(strcpy(courier_malloc(strlen(frombuf)+
				sizeof("TCPREMOTEINFO=")),
				"TCPREMOTEINFO="), frombuf);
	envs[envp]=0;

	if (!tostdout && submit_fork(args, envs, submit_print_stdout))
	{
		fprintf(stderr, "sendmail: Service temporarily unavailable.\n");
		exit(EX_TEMPFAIL);
	}

	if (tostdout)
		submit_to=stdout;
	else
	{
		if (uucprmail)
		{
			submit_write_message(from ? from:"");
		}
		else
		{
		char	*p;

			p=rewrite_env_sender(from);
			submit_write_message(p);
			free(p);
		}
		if ((submit_errcode=submit_readrcprinterr()) != 0)
		{
			exit_submit(submit_errcode);
		}
	}

	while (!tostdout && argn < argc)
	{
		submit_write_message(argv[argn]);
		if ((errflag=submit_readrcprinterr()) != 0)
		{
			fprintf(stderr, "%s: invalid address.\n", argv[argn]);
		}
		++argn;
	}
	if (errflag)	exit_submit(errflag);
	if (!tostdout)
		putc('\n', submit_to);

	if (!uucprmail)
		rewrite_headers(From);

	while ((c=getchar()) != EOF)
	{
		putc(c, submit_to);
	}
	fflush(submit_to);
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	errflag=0;
	if (ferror(submit_to) || (!tostdout && (
		fclose(submit_to) ||
		(errflag=submit_readrcprinterr()) || submit_wait()))
		)
	{
		fprintf(stderr, "sendmail: Unable to submit message.\n");
		if (errflag)
			exit_submit(errflag);
		exit(EX_TEMPFAIL);
	}
	exit(0);
}

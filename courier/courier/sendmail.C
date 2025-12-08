/*
** Copyright 1998 - 2018 S. Varshavchik.
** See COPYING for distribution information.
**
*/

#include	"courier.h"
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	"comsubmitclient.h"
#include	"maxlongsize.h"
#include	"numlib/numlib.h"
#include	"comuidgid.h"
#include	"sbindir.h"
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
#include	<pwd.h>

extern "C" void esmtpd();
extern struct passwd *mypwd();
extern std::string rewrite_env_sender(const char *from);
extern void rewrite_headers(const char *From,
			    FILE *read_from,
			    FILE *submit_to);

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

	setenv("AUTHMODULES", "", 1); /* See module.local/local.c */

#if HAVE_SETLOCALE
	setlocale(LC_ALL, "");
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
		uucprmail=(char *)malloc(strlen(uu_machine)+strlen(uu_user)+2);
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

	static char submit_str[]="submit";
	static char bcc_str[]="-bcc";

	args[argp++]=submit_str;
	if (bcconly)
		args[argp++]=bcc_str;

	if (uucprmail)
	{
		if (argn >= argc)
		{
			fprintf(stderr, "rmail: missing recipients\n");
			exit(EX_NOPERM);
		}

		static char uucp_str[]="uucp";
		args[argp++]=uucp_str;
		s=(char *)malloc(sizeof("unknown; uucp ()")+strlen(uucprmail));
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
		static char local_str[]="local";
		static char dns_str[]="dns; localhost (localhost [127.0.0.1])";
		args[argp++]=local_str;
		args[argp++]=dns_str;
		args[argp++]=frombuf;
	}
	args[argp++]=0;
	envp=0;

	clog_open_stderr("sendmail");
	if (ret || security || doverp)
	{
		envs[envp]=strcat(
			strcat(
				strcpy((char *)
					courier_malloc(strlen(ret ?
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
		envs[envp]=strcat(strcpy((char *)courier_malloc(strlen(dsn)+11),
			"DSNNOTIFY="), dsn);
		++envp;
	}
	if (envid)
	{
		envs[envp]=strcat(strcpy((char *)courier_malloc(strlen(envid)+10),
			"DSNENVID="), envid);
		++envp;
	}

	static char localhost_str[]="TCPREMOTEHOST=localhost";
	static char tcpremoteip_str[]="TCPREMOTEIP=127.0.0.1";
	envs[envp++]=localhost_str;
	envs[envp++]=tcpremoteip_str;

	if (!uucprmail)
		envs[envp++]=strcat(strcpy((char *)courier_malloc(strlen(frombuf)+
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
			auto p=rewrite_env_sender(from);
			submit_write_message(p.c_str());
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
		rewrite_headers(From, stdin, submit_to);

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

/*
** Copyright 1998 - 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier_lib_config.h"
#include	"courier.h"
#include	"libexecdir.h"
#include	"waitlib/waitlib.h"
#include	"comsubmitclient.h"

#include	<stdio.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<stdlib.h>
#include	<errno.h>
#include	<ctype.h>
#include	<string.h>
#include	<signal.h>

static void (*printfunc)(const char *);
static void (*teergrubefunc)(void)=NULL;

static pid_t submit_pid;
FILE *submit_to;
FILE *fromsubmit;
static int submit_error;
static void (*log_error_prefix_func)(void);

#define	PRINT_RESPONSE	1
#define	PRINT_CRLF	2
#define	PRINT_ERRORS	4

static RETSIGTYPE sighandler(int signum)
{
	submit_cancel_async();
	signal(signum, SIG_DFL);
	kill(getpid(), signum);
	_exit(0);
#if	RETSIGTYPE != void
	return (0);
#endif
}

void submit_set_teergrube( void (*funcarg)(void))
{
	teergrubefunc=funcarg;
}

/*
** Wait for the submit process to terminate.  NOTE: submit_to must be
** already closed.  Returns 0 if submit terminated normally, non-zero
** in all other situations.
*/

static int submit_wait_noreset()
{
int	wait_stat;
pid_t	wait_pid;

	while ((wait_pid=wait(&wait_stat)) != submit_pid)
	{
		if (wait_pid == -1 && errno == ECHILD)	return (-1);
	}
	if (WIFEXITED(wait_stat) && WEXITSTATUS(wait_stat) == 0)
		return (0);
	return (-1);
}

int submit_wait()
{
int	rc=submit_wait_noreset();

	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	return (rc);
}


/****************************************************************************
** Fork and run the submit process, creating pipes to/from its standard I/O.
** A pipe to the submit process is created in submit_to, and we maintain
** the pipe from the submit process internally.
**
** args - arguments to the submit process.
** env - submit process's environment.
**
** Returns 0 if the submit process was forked succesfully, non-zero in all
** other situations.
**
** Fork and run the submit process, creating pipes to/from its standard I/O.
*/

int submit_fork_virtual(char **args, char **env,
			void (*func)(const char *))
{
int	pipe0[2], pipe1[2];

	printfunc=func;

	submit_error=1;
	log_error_prefix_func=0;
	if (pipe(pipe0))
	{
		clog_msg_start_err();
		clog_msg_str(strerror(errno));
		clog_msg_send();
		return (-1);
	}
	if (pipe(pipe1))
	{
		close(pipe0[0]);
		close(pipe0[1]);
		clog_msg_start_err();
		clog_msg_str(strerror(errno));
		clog_msg_send();
		return (-1);
	}

	submit_pid=fork();
	if ( submit_pid == 0)
	{
	static char execfailed[]="400 Unable to submit message - service temporarily unavailable.\n";

		close(pipe0[1]);
		close(pipe1[0]);
		dup2(pipe0[0], 0);
		dup2(pipe1[1], 1);
		close(pipe0[0]);
		close(pipe1[1]);

		if (chdir(courierdir()))
			exit(1);

		execve( LIBEXECDIR "/courier/submit", args, env );

		clog_msg_start_err();
		clog_msg_str(strerror(errno));
		clog_msg_send();
		if (write(1, execfailed, sizeof(execfailed)-1) != 1)
			; /* ignore */
		exit(1);
	}
	close(pipe0[0]);
	close(pipe1[1]);

	if (submit_pid == -1)
	{
		clog_msg_start_err();
		clog_msg_str(strerror(errno));
		clog_msg_send();
		close(pipe0[1]);
		close(pipe1[0]);
		return (-1);
	}
	if (fcntl(pipe0[1], F_SETFD, FD_CLOEXEC) ||
	    (submit_to=fdopen(pipe0[1], "w")) == NULL)
	{
		clog_msg_start_err();
		clog_msg_str(strerror(errno));
		clog_msg_send();
		close(pipe0[1]);
		close(pipe1[0]);
		(void)submit_wait_noreset();
		return (-1);
	}

	if (fcntl(pipe1[0], F_SETFD, FD_CLOEXEC) ||
	    (fromsubmit=fdopen(pipe1[0], "r")) == NULL)
	{
		clog_msg_start_err();
		clog_msg_str(strerror(errno));
		clog_msg_send();
		fclose(submit_to);
		close(pipe1[0]);
		(void)submit_wait_noreset();
		return (-1);
	}
	submit_error=0;
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGHUP, sighandler);
	return (0);
}

void submit_log_error_prefix(void (*func)(void))
{
	log_error_prefix_func=func;
}

int submit_fork(char **args, char **env,
		void (*func)(const char *))
{
	size_t i, j;
	const char *p=config_get_local_vhost();
	char **newargs;
	int rc;
	char *q=0;

	if (!p || !*p)
		return submit_fork_virtual(args, env, func);

	for (i=0; args[i]; ++i)
		;

	newargs=malloc((i+2)*sizeof(char *));

	for (i=j=0; (newargs[j]=args[i]) != 0; ++i, ++j)
	{
		if (i == 0)
		{
			++j;
			strcat(strcpy((q=newargs[j]=
				       malloc(strlen(p)+10)),
				      "-vhost="), p);
		}
	}
	rc=submit_fork_virtual(newargs, env, func);

	free(newargs);
	if (q) free(q);
	return rc;
}

/*
** Read one line of response from the submit process, echoing it if so
** specified by doprint.
**
** If EOF indication is received, or if a response that does not follow
** RFC822 reply format is received, submit_error is set.
**
** errcode should point to a four character buffer, the first character
** of which must be initialized to 0 before the first call.  errcode
** will be set to the RFC822 reply numeric code received (or initialized
** to 450 if an error condition occurs on the first call).
**
** read_resp_oneline returns 0 if the reply line received from submit
** indicates the subsequent lines will follow, and will return non-0 if
** either the final response line has been received, or if an error
** condition occured.  In all cases, if doprint is set, an appropriate
** RFC822-conforming message is printed to standard out.
*/

static int read_resp_oneline(char *errcode, int doprint)
{
	static char linebuf[BUFSIZ];
	char	*c;
	int	rc;
	char	prev_errcode=errcode[0];

	if (submit_error ||
		fgets(linebuf, sizeof(linebuf), fromsubmit) == NULL)
	{
	const char *errmsg="Service temporarily unavailable.";

		if (!errcode[0])
			strcpy(errcode, "432");

		if (!submit_error)
		{
			submit_error=1;
			clog_msg_start_err();
			clog_msg_str("submitclient: EOF from submit.");
#if 0
			{
				const char *p=getenv("TCPREMOTEIP");

				if (p)
				{
					clog_msg_str(" (ip: ");
					clog_msg_str(p);
					clog_msg_str(")");
				}
			}
#endif
			clog_msg_send();
		}
		if (doprint)
		{
			(*printfunc)(errcode);
			(*printfunc)(" ");
			(*printfunc)(errmsg);
			(*printfunc)(doprint & PRINT_CRLF ? "\r\n":"\n");
			(*printfunc)(0);
		}
		return(-1);
	}

	if (!isdigit((int)(unsigned char)linebuf[0]) ||
		!isdigit((int)(unsigned char)linebuf[1]) ||
		!isdigit((int)(unsigned char)linebuf[2]) ||
		(errcode[0] && strncmp(errcode, linebuf, 3)) ||
		(linebuf[3] != ' ' && linebuf[3] != '-'))
	{
	char	*p;

		clog_msg_start_err();
		clog_msg_str("submitclient: bad response from submit: ");
		if ((p=strchr(linebuf, '\n')) != 0)	*p=0;
		clog_msg_str(linebuf);
		if (p)	*p='\n';
		clog_msg_send();

		if (!errcode[0])
			strcpy(errcode, "432");
		if (doprint)
		{
			(*printfunc)(errcode);
			(*printfunc)(" ");
		}
		submit_error=1;
		rc=1;
	}
	else
	{
		if (!errcode[0])
		{
			memcpy(errcode, linebuf, 3);
			errcode[3]=0;
		}
		rc=0;
		if (linebuf[3] == ' ')	rc=1;
	}

	if ((c=strchr(linebuf, '\n')) != NULL)	*c=0;

	switch (errcode[0])	{
	case '1':
	case '2':
	case '3':
		if (doprint & PRINT_ERRORS)
			doprint=0;
		break;
	default:
		if (prev_errcode == 0 && errcode[0] == '5' &&
		    teergrubefunc)
		{
			(*teergrubefunc)();
		}

		if (log_error_prefix_func)
		{
			clog_msg_start_err();
			(*log_error_prefix_func)();
			clog_msg_str(linebuf);
			clog_msg_send();
		}
	}

	if (doprint)
		(*printfunc)(linebuf);

	if (!c)
	{
	int	ch;
	int	n=0;
	char	buf[256];

		while ((ch=getc(fromsubmit)) != '\n')
		{
			if (ch < 0)
			{
				submit_error=1;
				clog_msg_start_err();
				clog_msg_str("submitclient: EOF from submit.");
#if 0
				{
					const char *p=getenv("TCPREMOTEIP");

					if (p)
					{
						clog_msg_str(" (ip: ");
						clog_msg_str(p);
						clog_msg_str(")");
					}
				}
#endif
				clog_msg_send();
				break;
			}
			if (doprint)
			{
				buf[n++]=ch;
				if (n == sizeof(buf)-1)
				{
					buf[n]=0;
					(*printfunc)(buf);
					n=0;
				}
			}
		}
		if (doprint)
		{
			buf[n]=0;
			(*printfunc)(buf);
		}
	}

	if (doprint)
	{
		(*printfunc)(doprint & PRINT_CRLF ? "\r\n":"\n");
		(*printfunc)(0);
	}
	return (rc);
}

/*
** Repeatedly call read_resp_online until the entire RFC822 response is
** received from submit.  Echo on standard output if so selected.
** Returns zero if a succesfull response is received, non-0 in all other
** situations.
*/

static int readresponse(int doprint)
{
char	errbuf[4];
int	rc;

	errbuf[0]=0;
	do
	{
		rc=read_resp_oneline(errbuf, doprint);
	} while (rc == 0);
	switch (errbuf[0])	{
	case '4':
		return (-4);
	case '5':
		return (-5);
	}
	return (0);
}

int submit_readrc()
{
	return (readresponse(0));
}

int submit_readrcprint()
{
	return (readresponse(PRINT_RESPONSE));
}

int submit_readrcprinterr()
{
	return (readresponse(PRINT_ERRORS));
}

int submit_readrcprintcrlf()
{
	return (readresponse(PRINT_RESPONSE|PRINT_CRLF));
}

void submit_write_message(const char *msg)
{
	if (submit_error)	return;
	for ( ; *msg; msg++)
		if (*msg != '\r' && *msg != '\n')
			putc(*msg, submit_to);
	putc('\n', submit_to);
	fflush(submit_to);
	if (ferror(submit_to))	submit_error=1;
}

static int alarmflag;

static RETSIGTYPE sigtrap(int signum)
{
	alarmflag=1;
	signal(SIGALRM, sigtrap);
	alarm(1);
#if RETSIGTYPE != void
	return (0);
#endif
}

void submit_cancel_async()
{
char	buf[256];

	kill(submit_pid, SIGTERM);
	alarmflag=0;
	signal(SIGALRM, sigtrap);
	alarm(10);
	while (read(fileno(fromsubmit), buf, sizeof(buf)) > 0)
		;
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	if (alarmflag)	kill(submit_pid, SIGKILL);
	(void)submit_wait_noreset();
}

void submit_cancel()
{
	submit_cancel_async();
	fclose(submit_to);
	fclose(fromsubmit);
}

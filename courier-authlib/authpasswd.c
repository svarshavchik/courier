/*
** Copyright 2005-2006 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdlib.h>
#include	<stdio.h>
#if	HAVE_TERMIOS_H
#include	<termios.h>
#endif
#include	<signal.h>
#include	"auth.h"

/*
**	Where possible, we turn off echo when entering the password.
**	We set up a signal handler to catch signals and restore the echo
**	prior to exiting.
*/

#if	HAVE_TERMIOS_H
static struct termios tios;
static int have_tios;

static void sighandler(int signum)
{
	if (write(1, "\n", 1) < 0)
		; /* Ignore gcc warning */
	tcsetattr(0, TCSANOW, &tios);
	_exit(0);
}
#endif

static void read_pw(char *buf)
{
int	n, c;

	n=0;
	while ((c=getchar()) != EOF && c != '\n')
		if (n < BUFSIZ-1)
			buf[n++]=c;
	if (c == EOF && n == 0)	exit(1);
	buf[n]=0;
}

int main(int argc, char **argv)
{
	char buf[BUFSIZ];
	char *p;
	char hint[100];


	strcpy(hint, "$1$");

	if (argc > 1)
	{
		sprintf(hint, "{%1.15s}", argv[1]);
	}

	/* Read the password */
#if	HAVE_TERMIOS_H

	have_tios=0;
	if (tcgetattr(0, &tios) == 0)
	{
		struct	termios tios2;
		char	buf2[BUFSIZ];

		have_tios=1;
		signal(SIGINT, sighandler);
		signal(SIGHUP, sighandler);
		tios2=tios;
		tios2.c_lflag &= ~ECHO;
		tcsetattr(0, TCSANOW, &tios2);

		for (;;)
		{
			if (write(2, "Password: ", 10) < 0)
				; /* Ignore gcc warning */
			read_pw(buf);
			if (write(2, "\nReenter password: ", 19) < 0)
				; /* Ignore gcc warning */
			read_pw(buf2);
			if (strcmp(buf, buf2) == 0)	break;
			if (write(2, "\nPasswords don't match.\n\n", 25) < 0)
				; /* Ignore gcc warning */

		}

	}
	else
#endif
		read_pw(buf);

#if	HAVE_TERMIOS_H
	if (have_tios)
	{
		if (write(2, "\n", 1) < 0)
			; /* Ignore gcc warning */
		tcsetattr(0, TCSANOW, &tios);
		signal(SIGINT, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
	}
#endif

	p=authcryptpasswd(buf, hint);

	if (p)
		printf("%s\n", p);
	return (0);
}

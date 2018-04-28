/*
** Copyright 1998 - 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	<sys/types.h>
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif
#if	HAVE_MD5
#include	"md5/md5.h"
#endif
#if	HAVE_HMAC
#include	"libhmac/hmac.h"
#endif

#if	HAVE_BCRYPT
#include	<pwd.h>
#endif
#include	<string.h>
#include	<stdio.h>
#include	<signal.h>
#include	<stdlib.h>
#if	HAVE_TERMIOS_H
#include	<termios.h>
#endif
#if	HAVE_CRYPT_H
#include	<crypt.h>
#endif

#if HAVE_CRYPT
#if NEED_CRYPT_PROTOTYPE
extern char *crypt(const char *, const char *);
#endif
#endif

extern char userdb_hex64[];

#ifdef	RANDOM
extern void userdb_get_random(char *buf, unsigned n);
#endif

#if	HAVE_MD5

char *userdb_mkmd5pw(const char *);

#endif

/*
**	Where possible, we turn off echo when entering the password.
**	We set up a signal handler to catch signals and restore the echo
**	prior to exiting.
*/

#if	HAVE_TERMIOS_H
static struct termios tios;
static int have_tios;

static RETSIGTYPE sighandler(int signum)
{
	if (write(1, "\n", 1) < 0)
		; /* ignore gcc warning */
	tcsetattr(0, TCSANOW, &tios);
	_exit(0);
#if	RETSIGTYPE != void
	return (0);
#endif
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
int	n=1;
int	md5=0;
char	buf[BUFSIZ];
char	salt[9];
#if HAVE_BCRYPT
char	*cryptsalt;
#endif
#if	HAVE_HMAC
struct hmac_hashinfo	*hmac=0;
#endif

	while (n < argc)
	{
		if (strcmp(argv[n], "-md5") == 0)
		{
			md5=1;
			++n;
			continue;
		}
#if	HAVE_HMAC
		if (strncmp(argv[n], "-hmac-", 6) == 0)
		{
		int	i;

			for (i=0; hmac_list[i] &&
				strcmp(hmac_list[i]->hh_name, argv[n]+6); i++)
				;
			if (hmac_list[i])
			{
				hmac=hmac_list[i];
				++n;
				continue;
			}
		}
#endif
		fprintf(stderr, "%s: invalid argument.\n", argv[0]);
		exit(1);
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
				; /* ignore gcc warning */
			read_pw(buf);
			if (write(2, "\nReenter password: ", 19) < 0)
				; /* ignore gcc warning */
			read_pw(buf2);
			if (strcmp(buf, buf2) == 0)	break;
			if (write(2, "\nPasswords don't match.\n\n", 25) < 0)
				; /* ignore gcc warning */
		}

	}
	else
#endif
		read_pw(buf);

#if	HAVE_TERMIOS_H
	if (have_tios)
	{
		if (write(2, "\n", 1) < 0)
			; /* ignore gcc warning */

		tcsetattr(0, TCSANOW, &tios);
		signal(SIGINT, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
	}
#endif

	/* Set the password */

#if	HAVE_HMAC
	if (hmac)
	{
	unsigned char *p=malloc(hmac->hh_L*2);
	unsigned i;

		if (!p)
		{
			perror("malloc");
			exit(1);
		}

		hmac_hashkey(hmac, buf, strlen(buf), p, p+hmac->hh_L);
		for (i=0; i<hmac->hh_L*2; i++)
			printf("%02x", (int)p[i]);
		printf("\n");
		exit(0);
	}
#endif

#if	HAVE_CRYPT

#else
	md5=1;
#endif

#if	HAVE_MD5
	if (md5)
	{

		printf("%s\n", userdb_mkmd5pw(buf));
		exit(0);
	}
#endif
#ifdef	RANDOM
	userdb_get_random(salt, 2);
	salt[0]=userdb_hex64[salt[0] & 63];
	salt[1]=userdb_hex64[salt[0] & 63];
#else
	{
	time_t	t;
	int	i;

		time(&t);
		t ^= getpid();
		salt[0]=0;
		salt[1]=0;
		for (i=0; i<6; i++)
		{
			salt[0] <<= 1;
			salt[1] <<= 1;
			salt[0] |= (t & 1);
			t >>= 1;
			salt[1] |= (t & 1);
			t >>= 1;
		}
		salt[0]=userdb_hex64[(unsigned)salt[0]];
		salt[1]=userdb_hex64[(unsigned)salt[1]];
	}
#endif

#if	HAVE_BCRYPT
	cryptsalt=bcrypt_gensalt(8);
	printf("%s\n", crypt(buf, cryptsalt));
	fflush(stdout);
#elif	HAVE_CRYPT
	printf("%s\n", crypt(buf, salt));
	fflush(stdout);
#endif
	return (0);
}

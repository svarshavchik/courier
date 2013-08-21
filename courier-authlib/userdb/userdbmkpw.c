/*
** Copyright 2001 Double Precision, Inc.
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

char userdb_hex64[]="./0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

#ifdef	RANDOM
void userdb_get_random(char *buf, unsigned n)
{
int	f=open(RANDOM, O_RDONLY);
int	l;

	if (f < 0)
	{
		perror(RANDOM);
		exit(1);
	}
	while (n)
	{
		l=read(f, buf, n);
		if (l < 0)
		{
			perror("read");
			exit(1);
		}
		n -= l;
		buf += l;
	}
	close(f);
}
#endif

#if	HAVE_MD5
char *userdb_mkmd5pw(const char *buf)
{
	int	i;
	char salt[9];

	salt[8]=0;
#ifdef	RANDOM
	userdb_get_random(salt, 8);
	for (i=0; i<8; i++)
		salt[i] = userdb_hex64[salt[i] & 63 ];

#else
	{

		struct {
#if HAVE_GETTIMEOFDAY
			struct timeval tv;
#else
			time_t	tv;
#endif
			pid_t	p;
		} s;

		MD5_DIGEST	d;
#if HAVE_GETTIMEOFDAY
		struct timezone tz;

		gettimeofday(&s.tv, &tz);
#else
		time(&s.tv);
#endif
		s.p=getpid();

		md5_digest(&s, sizeof(s), d);
		for (i=0; i<8; i++)
			salt[i]=userdb_hex64[ ((unsigned char *)d)[i] ];
	}
#endif
	return (md5_crypt(buf, salt));
}
#endif

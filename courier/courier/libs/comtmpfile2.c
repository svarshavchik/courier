/*
** Copyright 1998 - 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier_lib_config.h"
#include	"courier.h"
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<stdio.h>
#include	<fcntl.h>
#include	<time.h>
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

static const char base64[] =
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-,";

char *mktmpfilename()
{
	unsigned long seed;
	int i;
	int fd;
	char filename_buf[64];

#if HAVE_GETTIMEOFDAY
	struct timeval tv;

	gettimeofday(&tv, NULL);

	seed=tv.tv_sec;
	seed ^= tv.tv_usec << 16;
#else
	time_t t;

	time(&t);
	seed=t;
#endif
	seed ^= getpid();

	for (i=0; i<1000; i++, seed += 5000)
	{
		char *p;
		unsigned long n;

		strcpy(filename_buf, "/tmp/courier.");

		p=filename_buf + strlen(filename_buf);

		n=seed;
		*p++=base64[ n % 64 ]; n /= 64;
		*p++=base64[ n % 64 ]; n /= 64;
		*p++=base64[ n % 64 ]; n /= 64;
		*p++=base64[ n % 64 ]; n /= 64;
		*p++=base64[ n % 64 ]; n /= 64;
		*p++=base64[ n % 64 ];
		*p=0;

		if ((fd=open(filename_buf, O_RDWR | O_CREAT | O_EXCL, 0600))
		    >= 0)
		{
			char *p;

			close(fd);

			p=strdup(filename_buf);
			if (!p)
				unlink(filename_buf);
			return (p);
		}

		if (errno != EEXIST)
			break;
	}
	return (NULL);
}

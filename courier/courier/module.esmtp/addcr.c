/*
** Copyright 1998 - 2002 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdio.h>

int main()
{
char	inbuf[BUFSIZ], outbuf[BUFSIZ*2];
int	i,j,k;
int	skip=0;
 fd_set fds;

	for (;;)
	{
		FD_ZERO(&fds);
		FD_SET(0, &fds);

		if (select(1, &fds, NULL, NULL, NULL) < 0)
		{
			if (errno == EINTR)
				continue;
			perror("select");
			break;
		}

		if (!FD_ISSET(0, &fds))
			continue;

		if ((i=read(0, inbuf, sizeof(inbuf))) <= 0)
			break;

		for (j=k=0; j<i; j++)
		{
			if (inbuf[j] == '\n' && !skip)
				outbuf[k++]='\r';
			skip=(outbuf[k++]=inbuf[j]) == '\r';
		}
		for (i=0; i<k; )
		{
			FD_ZERO(&fds);
			FD_SET(1, &fds);

			if (select(2, NULL, &fds, NULL, NULL) < 0)
			{
				if (errno == EINTR)
					continue;
				perror("select");
				break;
			}
			
			if (!FD_ISSET(1, &fds))
				continue;

			j=write(1, outbuf+i, k-i);
			if (j <= 0)
				return (-1);
			i += j;
		}
	}
	return (0);
}


/*
**
** Copyright 2004 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "courier_socks_config.h"
#include "socks.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#include <errno.h>

#define INITFUNC connect_sox_init
#define NEXTFUNC connect_sox_next
#include "connectfunc.h"
#undef INITFUNC
#undef NEXTFUNC
#include "printaddr.h"

static int my_connect(int family,
		      int proto,
		      const char *service,
		      const char *port)
{
	struct connect_info ci;
	const struct sockaddr *addrptr;
	SOCKLEN_T addrlen;

	int fd;

	if (connect_sox_init(&ci, family, proto, service, port) < 0)
		return -1;

	errno=ENOENT;
	while (connect_sox_next(&ci, &addrptr, &addrlen) == 0)
	{
		int rc;

		if ((fd=socket(PF_INET, SOCK_STREAM, 0)) < 0)
		{
			perror("socket");
			return -1;
		}

		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
		{
			perror("fcntl");
			Rclose(fd);
			return -1;
		}

		rc=Rconnect(fd, addrptr, addrlen);

		if (rc == -1 && errno == EINPROGRESS)
		{
			fd_set w;

			FD_ZERO(&w);
			FD_SET(fd, &w);

			if (Rselect(fd+1, NULL, &w, NULL, NULL) <= 0 ||
			    !FD_ISSET(fd, &w))
			{
				rc=-1;
			}
			else
			{
				int errcode;
				SOCKLEN_T cnt=sizeof(errcode);

				if (Rgetsockopt(fd, SOL_SOCKET, SO_ERROR,
						&errcode, &cnt) < 0)
					rc= -1;
				else if (errcode)
				{
					errno=errcode;
					rc= -1;
				}
				else
					rc=0;
			}
		}
		if ( rc == 0)
		{
			while (connect_sox_next(&ci, &addrptr, &addrlen) == 0)
				; /* Flush all buffers */

			if (fcntl(fd, F_SETFL, 0) < 0)
			{
				perror("fcntl");
				Rclose(fd);
				return -1;
			}

			return fd;
		}
		Rclose(fd);
	}
	return -1;
}

int main(int argc, char **argv)
{
	int fd;
	FILE *fp;
	SOCKADDR_STORAGE addr;
	SOCKLEN_T addrlen;

	if (argc < 2)
		exit(0);

	if ((fd=my_connect(AF_INET, SOCK_STREAM, argv[1],
			   "http")) >= 0)
	{
		int c;

		addrlen=sizeof(addr);
		if (Rgetpeername(fd,(struct sockaddr *)&addr, &addrlen) < 0)
		{
			perror("getpeername");
			exit(1);
		}
		printf("Connected to: ");
		printaddr(stdout, &addr);
		printf(" from ");

		addrlen=sizeof(addr);
		if (Rgetsockname(fd,(struct sockaddr *)&addr, &addrlen) < 0)
		{
			perror("getsockname");
			exit(1);
		}
		printaddr(stdout, &addr);
		printf("\n\n");

		fp=fdopen(fd, "r+");

		if (!fp)
		{
			perror("fdopen");
			exit(1);
		}

		fprintf(fp, "GET /robots.txt HTTP/1.0\n\n");
		fflush(fp);

		while ((c=getc(fp)) != EOF)
			putchar(c);

		exit (0);
	}

	perror("connect");

	return 0;
}

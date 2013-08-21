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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#include <errno.h>
#include "printaddr.h"

int main(int argc, char **argv)
{
	int fd2;
	FILE *fp;
	struct sockaddr_in sin;
	SOCKADDR_STORAGE addr;
	SOCKLEN_T addrlen;
	int c;
	int port;
	int fd= -1;

	port=1023;

	while (port > 0)
	{
		struct sockaddr_in sin;

		memset(&sin, 0, sizeof(sin));
		sin.sin_family=AF_INET;
		sin.sin_port=htons(port);
		fd=socket(PF_INET, SOCK_STREAM, 0);
		if (fd < 0)
			break;
		if (Rbind(fd, (const struct sockaddr *)&sin, sizeof(sin)) >= 0)
			break;
		close(fd);
		fd= -1;
		--port;
	}

	if (fd < 0)
	{
		perror("rresvport");
		exit(1);
	}
	else
	{
		printf("port %d\n", port);

		addrlen=sizeof(addr);
		if (Rgetsockname(fd,(struct sockaddr *)&addr, &addrlen) < 0)
		{
			perror("getsockname");
			exit(1);
		}

		printf("Real port: ");
		printaddr(stdout, &addr);
		printf("\n");
	}

	if (fd < 0)
	{
		perror("socket");
		exit(1);
	}

	memset(&sin, 0, sizeof(sin));

	sin.sin_family=AF_INET;
	sin.sin_addr.s_addr=INADDR_ANY;
	sin.sin_port=htons(port);

	if (Rbind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror("bind");
		exit(1);
	}

	if (Rlisten(fd, 2) < 0)
	{
		perror("listen");
		exit(1);
	}

	addrlen=sizeof(addr);
	if (Rgetsockname(fd,(struct sockaddr *)&addr, &addrlen) < 0)
	{
		perror("getsockname");
		exit(1);
	}

	printf("Listening on: ");
	printaddr(stdout, &addr);
	printf("\n");

	addrlen=sizeof(addr);

	if ((fd2=Raccept(fd, (struct sockaddr *)&addr, &addrlen)) < 0)
	{
		perror("accept");
		exit(1);
	}

	printf("Connection from: ");
	printaddr(stdout, &addr);
	printf("\n");

	addrlen=sizeof(addr);
	if (Rgetsockname(fd2,(struct sockaddr *)&addr, &addrlen) < 0)
	{
		perror("getsockname");
		exit(1);
	}
	Rclose(fd);

	printf("getsockname: ");
	printaddr(stdout, &addr);
	printf("\n");

	addrlen=sizeof(addr);
	if (Rgetpeername(fd2,(struct sockaddr *)&addr, &addrlen) < 0)
	{
		perror("getpeername");
		exit(1);
	}

	printf("getpeername: ");
	printaddr(stdout, &addr);
	printf("\n");

	fp=fdopen(fd2, "r+");

	if (!fp)
	{
		perror("fdopen");
		exit(1);
	}

	while ((c=getc(fp)) != EOF)
		putchar(c);
	return 0;
}

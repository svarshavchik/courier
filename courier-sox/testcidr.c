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
#include <string.h>
#include "cidr.h"

int main(int argc, char **argv)
{
	CIDR c;
	struct sockaddr_in sin;
#if HAVE_IPV6
	struct sockaddr_in6 sin6;
#endif
	int rc;

	if (argc < 3)
		return 0;

	if (tocidr(&c, argv[1]) < 0)
	{
		perror("tocidr");
		exit(2);
	}

#if HAVE_IPV6
	{
		char b[INET6_ADDRSTRLEN];

		printf("%s", inet_ntop(AF_INET6, &c.addr, b, sizeof(b)));
	}
#else
	printf("%s", inet_ntoa(c.addr));
#endif

	printf("/%d: ", c.pfix);

#if HAVE_IPV6
	if (strchr(argv[2], ':'))
	{
		if (inet_pton(AF_INET6, argv[2], &sin6.sin6_addr) < 0)
		{
			perror("inet_pton");
			exit(2);
		}

		sin6.sin6_family=AF_INET6;

		rc=incidr(&c, (SOCKADDR_STORAGE *)&sin6);
	}
	else
#endif
	{
		if ((sin.sin_addr.s_addr=inet_addr(argv[2]))
		    == INADDR_NONE)
		{
			errno=EINVAL;
			perror("inet_addr");
			exit(2);
		}
		sin.sin_family=AF_INET;
		rc=incidr(&c, (SOCKADDR_STORAGE *)&sin);
	}


	if (rc == 0)
	{
		printf("match\n");
	}
	else
	{
		printf("doesn't match\n");
		exit(1);
	}
	exit(0);
}

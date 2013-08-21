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

static void printaddr(FILE *f, const SOCKADDR_STORAGE *addr)
{
	switch (((struct sockaddr_in *)addr)->sin_family) {
	case AF_INET:
		fprintf(f, "%s;%d", inet_ntoa(((struct sockaddr_in *)addr)
					      ->sin_addr),
			ntohs(((struct sockaddr_in *)addr)->sin_port));
		break;
#if HAVE_IPV6
	case AF_INET6:
		{
			char b[INET6_ADDRSTRLEN];

			fprintf(f, "%s;%d",
				inet_ntop(((struct sockaddr_in6 *)addr)
					  ->sin6_family,
					  &((struct sockaddr_in6 *)addr)
					  ->sin6_addr, b, sizeof(b)),
				ntohs(((struct sockaddr_in6 *)addr)
				      ->sin6_port));
		}
		break;
#endif
	default:
		fprintf(f, "[unknown]");
	}
}

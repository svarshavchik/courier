/*
** Copyright 2000 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"courier_auth_config.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/socket.h>
#include	<sys/un.h>
#include	<sys/time.h>
#include	<sys/wait.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>


#ifndef	SOMAXCONN
#define	SOMAXCONN	5
#endif

int main(int argc, char *argv[])
{
int	fd=socket(PF_UNIX, SOCK_STREAM, 0);
struct	sockaddr_un skun;

	if (argc < 2)	exit(1);
	if (fd < 0)	exit(1);
	skun.sun_family=AF_UNIX;
	strcpy(skun.sun_path, argv[1]);
	unlink(skun.sun_path);
	if (bind(fd, (const struct sockaddr *)&skun, sizeof(skun)) ||
		listen(fd, SOMAXCONN))
		exit(1);
	exit (0);
	return (0);
}

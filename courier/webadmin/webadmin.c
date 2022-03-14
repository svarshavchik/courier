/*
** Copyright 2001-2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "config.h"
#include "cgi/cgi.h"
#include "sockdir.h"

void run(void *ptr)
{
	struct group *g;
	struct passwd *p;

	const char *c=getenv("PATH_INFO");
	int isroot=0;

	if (c)
	{
		if (*c == '/')
			++c;
		if (strcmp(c, "save") == 0)
			isroot=1;
	}

	if (isroot)
	{
		if (setuid(0))
		{
			printf("Content-Type: text/plain\n\nsetuid failed\n");
			exit(0);
		}
	}
	else
	{
		g=getgrnam(MAILGROUP);
		if (!g)
		{
			printf("Content-Type: text/plain\n\ngetgrnam(%s) failed\n",
			       MAILGROUP);
			exit(0);
		}

		if (setgid(g->gr_gid) < 0)
		{
			printf("Content-Type: text/plain\n\nsetgid failed\n");
			exit(0);
		}

		p=getpwnam(MAILUSER);
		if (!p)
		{
			printf("Content-Type: text/plain\n\ngetpwnam(%s) failed\n",
			       MAILUSER);
			exit(0);
		}
		if (setuid(p->pw_uid) < 0)
		{
			printf("Content-Type: text/plain\n\nsetuid failed\n");
			exit(0);
		}
	}

	execl(INSTDIR "/webadmin.pl", INSTDIR "/webadmin.pl", (char *)0);
	exit(0);
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "daemon") == 0)
		cgi_daemon(0, SOCKDIR "/webadmin", NULL, run, NULL);

	cgi_connectdaemon(SOCKDIR "/webadmin", 1);
	return 0;
}

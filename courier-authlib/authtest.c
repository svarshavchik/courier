/*
** Copyright 1998 - 2004 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"auth.h"
#include	"courierauthstaticlist.h"
#include	"courierauthsasl.h"
#include	"courierauthdebug.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

#include	"debug.c"
/* libtool bork-age */


static void usage()
{
	fprintf(stderr, "Usage: authtest [-s service] userid [ password [ newpassword ] ]\n");
	exit(1);
}

static int callback_pre(struct authinfo *a, void *dummy)
{
#define PTR(x) ((x) ? (x):"(none)")

	printf("Authentication succeeded.\n\n");
	printf("     Authenticated: %s ", PTR(a->address));
	if (a->sysusername)
		printf(" (system username: %s)\n", a->sysusername);
	else if (a->sysuserid)
		printf(" (uid %lu, gid %lu)\n", (unsigned long)*a->sysuserid,
		       (unsigned long)a->sysgroupid);
	else printf(" (*** UID/GID initialization error***)\n");

	printf("    Home Directory: %s\n", PTR(a->homedir));
	printf("           Maildir: %s\n", PTR(a->maildir));
	printf("             Quota: %s\n", PTR(a->quota));
	printf("Encrypted Password: %s\n", PTR(a->passwd));
	printf("Cleartext Password: %s\n", PTR(a->clearpasswd));
	printf("           Options: %s\n", PTR(a->options));
#undef PTR

	return (0);
}

int main(int argc, char **argv)
{
int	argn;
const char *service="login";

	for (argn=1; argn<argc; argn++)
	{
	const char *argp;

		if (argv[argn][0] != '-')	break;
		if (argv[argn][1] == 0)
		{
			++argn;
			break;
		}

		argp=argv[argn]+2;

		switch (argv[argn][1])	{
		case 's':
			if (!*argp && argn+1 < argc)
				argp=argv[++argn];
			service=argp;
			break;
		default:
			usage();
		}
	}
	if (argc - argn <= 0)
		usage();

	courier_authdebug_login_level = 2;

	if (argc - argn >= 3)
	{
		if (auth_passwd(service, argv[argn],
				argv[argn+1],
				argv[argn+2]))
		{
			perror("Authentication FAILED");
			exit(1);
		}
		else
		{
			fprintf(stderr, "Password change succeeded.\n");
			exit(0);
		}
	}
	setenv("TCPREMOTEIP", "::1", 0);

	if (argc - argn >= 2)
	{
		if (auth_login_meta(NULL, service, argv[argn],
				    argv[argn+1],
				    callback_pre,
				    NULL))
		{
			perror("Authentication FAILED");
			exit(1);
		}
	}
	else if (argc - argn >= 1)
	{
		if (auth_getuserinfo_meta(NULL, service, argv[argn],
					  callback_pre,
					  NULL))
		{
			perror("Authentication FAILED");
			exit(1);
		}
	}
	exit(0);
}

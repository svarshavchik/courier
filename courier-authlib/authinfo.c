/*
** Copyright 2004 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"courier_auth_config.h"
#include	"auth.h"
#include	"pkglibdir.h"
#include	"pkgincludedir.h"
#include	"authdaemonrc.h"
#include	"authldaprc.h"
#include	"authmysqlrc.h"
#include	"authpgsqlrc.h"
#include	"sbindir.h"
#include	"mailusergroup.h"
#include	"packageversion.h"

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<pwd.h>
#include	<grp.h>


static int getmuid()
{
	struct passwd *pw=getpwnam(MAILUSER);

	if (pw == NULL)
	{
		perror("Cannot obtain information for user " MAILUSER);
		exit(1);
	}

	return (pw->pw_uid);
}

static int getmgid()
{
	struct group *gr=getgrnam(MAILGROUP);

	if (gr == NULL)
	{
		perror("Cannot obtain information for groupid " MAILGROUP);
		exit(1);
	}

	return (gr->gr_gid);
}

static unsigned getver(const char **a)
{
	unsigned n=0;
	static const char dig[]="0123456789";
	static const char *p;

	while (**a)
	{
		if (**a == '.')
		{
			++*a;
			break;
		}

		if ((p=strchr(dig, **a)) != NULL)
			n=n*10 + p-dig;

		++*a;
	}
	return n;
}

static int versioncmp(const char *a, const char *b)
{
	while (*a && *b)
	{
		unsigned va=getver(&a);
		unsigned vb=getver(&b);

		if (va < vb)
			return -1;
		if (va > vb)
			return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	int n;

	for (n=1; n<argc; n++)
	{
		if (strcmp(argv[n], "--version") == 0)
		{
			printf("%s\n", PKGVERSION);
		}

		if (strncmp(argv[n], "--version=", 10) == 0)
		{
			printf("%s\n",
			       versioncmp(argv[n]+10, PKGVERSION) <= 0
			       ? "yes":"no");
		}
			       
		if (strcmp(argv[n], "--ldflags") == 0)
		{
			printf("-L%s\n",
			       PKGLIBDIR);
		}
		if (strcmp(argv[n], "--cppflags") == 0)
		{
#if HAVE_NOSTDHEADERDIR
			printf("-I%s\n", PKGINCLUDEDIR);
#endif
			;
		}
		if (strcmp(argv[n], "--configfiles") == 0)
		{
			printf("userdb=%s\n"
			       "authdaemonrc=%s\n"
			       "authldaprc=%s\n"
			       "authmysqlrc=%s\n"
			       "authpgsqlrc=%s\n"
			       "mailuser=%s\n"
			       "mailgroup=%s\n"
			       "mailuid=%d\n"
			       "mailgid=%d\n"
			       "sbindir=%s\n",
			       USERDB,
			       AUTHDAEMONRC,
			       AUTHLDAPRC,
			       AUTHMYSQLRC,
			       AUTHPGSQLRC,
			       MAILUSER,
			       MAILGROUP,
			       getmuid(),
			       getmgid(),
			       SBINDIR);
		}
	}
	return (0);
}

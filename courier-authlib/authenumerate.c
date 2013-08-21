/*
** Copyright 2003 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"auth.h"
#include	"courierauthstaticlist.h"
#include	"courierauthsasl.h"
#include	"courierauth.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	"numlib/numlib.h"


static int with_options = 0;
static int shared_only = 0;

static void enum_cb(const char *name,
		    uid_t uid,
		    gid_t gid,
		    const char *homedir,
		    const char *maildir,
		    const char *options,
		    void *void_arg)
{
	char buf1[NUMBUFSIZE];
	char buf2[NUMBUFSIZE];

	if (name == NULL)
	{
		*(int *)void_arg=0;
		return;
	}

	if (shared_only)
	{
		char *opt = auth_getoption(options, "disableshared");
		if (opt)
		{
			int disable = atoi(opt);
			free(opt);
			if (disable)
				return;
		}
	}
	
	printf("%s\t%s\t\%s\t%s", name,
	       libmail_str_uid_t(uid, buf1),
	       libmail_str_gid_t(gid, buf2),
	       homedir);
	printf("\t%s", maildir ? maildir : "");
	if (with_options)
		printf("\t%s", options ? options : "");
	printf("\n");
}

int main(int argc, char **argv)
{
	int exit_code;

	while (argc > 1)
	{
		if (!strcmp(argv[1], "-s"))
			shared_only = 1;
		else if (!strcmp(argv[1], "-o"))
			with_options = 1;
		else
		{
			fprintf(stderr, "Usage: authenumerate [-s] [-o]\n");
			exit(1);
		}
		argv++;
		argc--;
	}

	exit_code=1;

	auth_enumerate(enum_cb, &exit_code);
	exit(exit_code);
	return (0);
}

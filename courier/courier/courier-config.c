/*
** Copyright 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"prefix.h"
#include	"exec_prefix.h"
#include	"sysconfdir.h"
#include	"localstatedir.h"
#include	"libexecdir.h"
#include	"bindir.h"
#include	"sbindir.h"
#include	"datadir.h"
#include	"uidgid.h"
#include	"configargs.h"
#include	<string.h>
#include	<stdlib.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<stdio.h>


static const char prefix[]=PREFIX;
static const char exec_prefix[]=EXEC_PREFIX;
static const char sysconfdir[]=SYSCONFDIR;
static const char localstatedir[]=LOCALSTATEDIR;
static const char libexecdir[]=LIBEXECDIR;
static const char bindir[]=BINDIR;
static const char sbindir[]=SBINDIR;
static const char datadir[]=DATADIR;

static const char mailuser[]=MAILUSER;
static const char mailgroup[]=MAILGROUP;

static const char configargs[]=CONFIGURE_ARGS;

int main(int argc, char **argv)
{
	printf("prefix=%s\n", prefix);
	printf("exec_prefix=%s\n", exec_prefix);
	printf("bindir=%s\n", bindir);
	printf("sbindir=%s\n", sbindir);
	printf("libexecdir=%s\n", libexecdir);
	printf("sysconfdir=%s\n", sysconfdir);
	printf("datadir=%s\n", datadir);
	printf("localstatedir=%s\n", localstatedir);
	printf("mailuser=%s\n", mailuser);
	printf("mailgroup=%s\n", mailgroup);
	printf("mailuid=%d\n", MAILUID);
	printf("mailgid=%d\n", MAILGID);
	printf("configure_args=\"%s\"\n", configargs);
	exit(0);
	return (0);
}

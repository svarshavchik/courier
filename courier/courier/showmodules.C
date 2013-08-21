/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"comcargs.h"
#include	<string.h>
#include	<stdlib.h>


static const char *courierdir_arg=0;

static struct courier_args arginfo[]={
	{"dir", &courierdir_arg},
	{0}
	} ;

int cppmain(int argc, char **argv)
{
	clog_open_stderr("showmodules");
	rw_init_verbose(1);
	(void)cargs(argc, argv, arginfo);

	if (courierdir_arg)
		set_courierdir(courierdir_arg);
	if (rw_init_courier(0))
		exit(1);
	exit (0);
	return (0);
}

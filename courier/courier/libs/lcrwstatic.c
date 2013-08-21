/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
#include	"lcrwstaticlist.h"
#include	<string.h>


extern struct rw_static_info rw_static[];

/*
	This module is compiled when transport modules are configured as
	static libraries, and are linked with during the link phase.

	The Makefile automatically generates lcrwstatic.h which contains
	pointers to all the rw_install and rw_init functions we link with.
	We simply go through the list, and feed each module to rw_install.
*/

int rw_init_courier(const char *name)
{
unsigned	i;
int	err=0;

	if (rw_init_verbose_flag)
	{
		clog_msg_start_info();
		clog_msg_str("Loading STATIC transport module libraries.");
		clog_msg_send();
	}
	if (rw_install_start())	return (-1);
	for (i=0; rw_static[i].name; i++)
		/* name != 0 really meaningless for static libs, but,
		** what the heck...
		*/
		if ((name == 0 || strcmp(name,
			rw_static[i].name) == 0) &&
			rw_install(rw_static[i].name,
				rw_static[i].rw_install,
				rw_static[i].rw_init))
			err=1;

	if (err || rw_install_init())
	{
		clog_msg_start_err();
		clog_msg_str("Transport module installation has FAILED.");
		clog_msg_send();
		return (-1);
	}
	return (0);
}

/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>

/*
	Install transport module libraries.

	Depending upon compilation options, transport module libraries are
	either linked with statically, or loaded as shared libraries.

	This module provides a unified interface for either installation
	method.

	The rw_init() function from each module is called, and a link list
	of struct rw_transport is created which includes a pointer to the
	result of each rw_init function call.

	First rw_install_start() is called to initialize the list.

	Then, rw_install is called with the name of each configured transport
	module, and it's rw_install and rw_init functions.  The module's
	rw_install function is called, and the result is saved in the
	rw_transport list.

	When all transport modules have been initialized, rw_install_init
	gets called, which goes through and calls the rw_init functions
	of every previously installed transport module libraries.
*/

int rw_install_start()
{
	if (rw_init_verbose_flag)
	{
		clog_msg_start_info();
		clog_msg_str(COURIER_COPYRIGHT);
		clog_msg_send();
		clog_msg_start_info();
		clog_msg_str("Installing [");
		clog_msg_uint(RW_VERLO);
		clog_msg_str("/");
		clog_msg_uint(RW_VERHI);
		clog_msg_str("]");
		clog_msg_send();
	}
	rw_transport_first=0;
	rw_transport_last=0;
	return (0);
}

int rw_install( const char *name,
	struct rw_list *(*rw_install)(const struct rw_install_info *),
	const char *(*rw_init)() )
{
struct rw_transport *t;
struct rw_install_info install_info={RW_VERLO, RW_VERHI, COURIER_COPYRIGHT};

	if (rw_init_verbose_flag)
	{
		clog_msg_start_info();
		clog_msg_str("Installing ");
		clog_msg_str(name);
		clog_msg_send();
	}

	install_info.courier_home=courierdir();
	t=(struct rw_transport *)courier_malloc(sizeof(struct rw_transport));
	t->name=(char *)courier_malloc(strlen(name)+1);
	t->udata=0;
	if ((t->rw_ptr= (*rw_install)(&install_info)) == 0)
	{
		if (t->name)	free((void *)t->name);
		if (t)		free((void *)t);
		clog_msg_start_err();
		clog_msg_str("rw_install() failed, check for messages from the module.");
		clog_msg_send();
		return (-1);
	}
	strcpy(t->name, name);
	t->init=rw_init;
	if (rw_transport_last)	rw_transport_last->next=t;
	else	rw_transport_first=t;
	rw_transport_last=t;
	t->next=0;
	if (rw_init_verbose_flag)
	{
		clog_msg_start_info();
		clog_msg_str("Installed: ");
		clog_msg_str(t->rw_ptr->module_id);
		clog_msg_send();
	}
	return (0);
}

int rw_install_init()
{
struct rw_transport *t;
const char *err;

	for (t=rw_transport_first; t; t=t->next)
	{
		if (rw_init_verbose_flag)
		{
			clog_msg_start_info();
			clog_msg_str("Initializing ");
			clog_msg_str(t->name);
			clog_msg_send();
		}
		if ((err=(*t->init)()) != 0)
		{
			clog_msg_start_err();
			clog_msg_str(err);
			clog_msg_send();
			return (-1);
		}
	}
	return (0);
}

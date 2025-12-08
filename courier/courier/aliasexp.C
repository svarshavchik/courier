/*
** Copyright 1998 - 2025 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
#include	"comcargs.h"
#include	<string.h>
#include	<signal.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<fstream>
#include	<iostream>

extern const char *srcfilename;
extern courier_args aliasexp_arginfo[];

int cppmain(int argc, char **argv)
{
	int	argn;
	const	char *module="local";
	std::ifstream	ifs;
	std::istream		*i;

	argn=cargs(argc, argv, aliasexp_arginfo);

	if (argn < argc)
		module=argv[argn++];

	if (rw_init_courier(0))	exit (1);
	clog_open_stderr("aliasexp");
	if (!srcfilename || strcmp(srcfilename, "-") == 0)
		i= &std::cin;
	else
	{
		ifs.open(srcfilename);
		if (ifs.fail())
		{
			clog_msg_start_err();
			clog_msg_str("Unable to open ");
			clog_msg_str(srcfilename);
			clog_msg_send();
			exit(1);
		}
		i= &ifs;
	}

struct rw_transport *modulep=rw_search_transport(module);

	if (!modulep)
	{
		clog_msg_start_err();
		clog_msg_str(module);
		clog_msg_str(" - not a valid module.");
		clog_msg_send();
		exit(1);
	}

	int rc=aliasexp(*i, std::cout, modulep);

	return(rc);
}

/*
** Copyright 1998 - 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "config.h"
#include	<stdio.h>
#include	<string.h>
#include	<iostream>
#include	<stdlib.h>


extern int cppmain(int, char **);

int main(int argc, char **argv)
{
int	rc;

	try
	{
		rc=cppmain(argc, argv);
	}
	catch (const char *errmsg)
	{
		std::cerr << argv[0] << ": " << errmsg << std::endl;
		exit (1);
	}
	catch (char *errmsg)
	{
		std::cerr << argv[0] << ": " << errmsg << std::endl;
		exit (1);
	}
	exit(rc);
	return (rc);
}

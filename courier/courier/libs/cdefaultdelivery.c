/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	<stdlib.h>

const char *config_defaultdelivery()
{
const	char *p=getenv("DEFAULTDELIVERY");

	if (!p)	p="./Maildir";
	return (p);
}

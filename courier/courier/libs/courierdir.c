/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"

static const char *home=COURIER_HOME;

void set_courierdir(const char *p)
{
	home=p;
}

const char *courierdir()
{
	return (home);
}

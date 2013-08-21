#ifndef	cmlmmoderate_h
#define	cmlmmoderate_h

/*
** Copyright 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/


#include	"config.h"
#include	<iostream>

int sendinitmod(afxipipestream &, const char *, const char *);
int cmdmoderate();
int postmsg(std::istream &, int (*)(std::istream &, std::ostream &));

#endif

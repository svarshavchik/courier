/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	cddelinfo_h
#define	cddelinfo_h

#include "config.h"
class delinfo;
class dlvrhost;
class msgq;
class rcptinfo;

class delinfo {
public:
    delinfo *freenext;		// List of unused delinfo in the same module.
    unsigned delid;		// My index in delinfo_list array
    dlvrhost *dlvrphost;	// Host we're delivering to
    rcptinfo *rcptlist;		// List of recipients being delivered
    delinfo();
    ~delinfo();
} ;

#endif

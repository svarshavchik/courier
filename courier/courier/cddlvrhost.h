/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	cddlvrhost_h
#define	cddlvrhost_h

#include	"config.h"
#include	<string>

class rcptinfo;
class pendelinfo;
class drvinfo;

class dlvrhost {
public:
    dlvrhost *next, *prev;	// next used on hdlvrpfree list, else
				// this is the first/last list, in MRU order
    std::string hostname;	// Name of this host
    unsigned dlvrcount;		// How many deliveries to this host are
				// in progress
    pendelinfo *pending_list;
    dlvrhost();
    ~dlvrhost();
} ;

#endif

/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef cdpendelinfo_h
#define cdpendelinfo_h

#include	"config.h"
#include	<string>
#include	<list>

class dlvrhost;
class rcptinfo;
class drvinfo;

class pendelinfo;

class pendelinfo {
public:
    std::list<pendelinfo>::iterator pos;
	// My location in hostp->mydrvinfo->pendelinfo_list

    drvinfo     *drvp;
    std::string hostname;
    std::list<rcptinfo *> receipient_list;
    dlvrhost    *hostp;     // Could be null
    pendelinfo();
    ~pendelinfo();

    pendelinfo(const pendelinfo &p) {}
} ;
#endif

/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	cddrvinfo_h
#define cddrvinfo_h

#include	"mybuf.h"
#include	<sys/types.h>
#include	<time.h>
#include	<stdio.h>

#include	<list>
#include	<vector>

struct rw_transport;

class delinfo;
class dlvrhost;
class rcptinfo;
class pendelinfo;

class drvinfo {
public:
    struct mybuf module_from;	// Pipe from the module driver.
    struct rw_transport *module;
		// Points to the rw_transport structure libcourier.a has
		// allocated for the module.
    std::vector<delinfo> delinfo_list;
		// Current delivery attempts.  Array allocated at startup,
		// and never deallocated.  Size of the array is specified
		// by maxdels from the module config file
    std::vector<dlvrhost> hosts_list;
		// Hosts where current delivery attempts are going to.
		// Array allocated at startup.  Size of the array is specified
		// by maxdels from the module config file
    unsigned maxhost, maxrcpt;     // From the config file

    delinfo *delpfreefirst;
		// Link list of unused delinfos in delinfo_list,
		// linked by freenext.

    dlvrhost *hdlvrpfree;
		// Link list of unused drvhosts in hosts_list, linked by next.

    dlvrhost *hdlvrpfirst, *hdlvrplast;
		// MRU list of drvhosts in hosts_list, linked by next and prev

    std::list<pendelinfo> pendelinfo_list;

    pid_t module_pid;	// Pid of module
    FILE *module_to;	// Pipe to the module

static drvinfo *modules;
static unsigned nmodules;
static drvinfo *module_dsn;

static void init();
static void cleanup();

    drvinfo();
    ~drvinfo();
} ;

#endif

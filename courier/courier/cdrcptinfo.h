/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	cdrcptinfo_h
#define	cdrcptinfo_h

#if	HAVE_CONFIG_H
#undef	PACKAGE
#undef	VERSION
#include	"config.h"
#endif

#include	<string>
#include	<vector>
#include	<list>

class delinfo;
class drvinfo;
class msgq;
class pendelinfo;

class rcptinfo {
public:
    msgq	*msg;	// This is the message these receipients are part of
    drvinfo	*delmodule;	// Use this module
    pendelinfo  *pending;	// Not NULL if we're not delivering these
				// recipients yet.
    std::list<rcptinfo *>::iterator
	    pendingpos;		// My position in pendelinfo

    std::string envsender;	// Envelope sender rewritten to transport
				// format.
    std::string delhost;	// Deliver to this address on the host
    std::vector<std::string> addresses;	 // Deliver to these addresses
    std::vector<unsigned>  addressesidx; // Indexes of addresses in control file

    rcptinfo();
    ~rcptinfo();

    void init(msgq *newq, drvinfo *module, std::string host,
	      std::string address);
    void init_1rcpt(msgq *newq, drvinfo *module,
		    std::string host, std::string address,
		    std::string recipient);
} ;

#endif

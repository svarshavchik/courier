/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"cdrcptinfo.h"

rcptinfo::rcptinfo() : msg(0), delmodule(0)
{
}

rcptinfo::~rcptinfo()
{
}

// rcptinfo::init() used to be inlined, until gcc 2.96 fucked it up.
// for a good measure rcptinfo::init_1rcpt() is also removed from being
// inlined, until gcc's bugs are fixed.

void rcptinfo::init(msgq *newq, drvinfo *module,
		    std::string host,
		    std::string address)
{
	msg=newq;
	delmodule=module;
	pending=0;
	delhost=host;
	addresses.clear();
	addressesidx.clear();
	envsender=address;
}

void rcptinfo::init_1rcpt(msgq *newq, drvinfo *module,
			  std::string host,
			  std::string address, std::string recipient)
{
	msg=newq;
	delmodule=module;
	pending=0;
	delhost=host;
	addresses.resize(1);
	addressesidx.resize(1);
	envsender=address;
	addresses[0]=recipient;
	addressesidx[0]=0;
}

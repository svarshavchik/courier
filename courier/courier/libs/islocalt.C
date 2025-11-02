/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rfc822/rfc822.h"
#include	<string.h>
#include	<stdlib.h>

bool configt_islocal(rfc822::tokens::iterator b,
		     rfc822::tokens::iterator e,
		     std::string &hosteddomain)
{
	std::string s;
	size_t l=rfc822::tokens::print(b, e,
				       rfc822::length_counter{} );
	s.reserve(l);
	rfc822::tokens::print(b, e, std::back_inserter(s));

	char *p=nullptr;
	bool rc=config_islocal(s.c_str(), &p);

	if (p)
	{
		hosteddomain=p;
		free(p);
	}
	return rc;
}

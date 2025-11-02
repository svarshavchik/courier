/*
** Copyright 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	<string.h>

#ifndef ACE_DOMAIN
#define ACE_DOMAIN config_defaultdomain_ace()
#endif

#ifndef I18N_DOMAIN
#define I18N_DOMAIN config_defaultdomain()
#endif

void rw_local_defaulthost(struct rw_info *p, void (*func)(struct rw_info *))
{
	auto b=p->addr.begin(), e=p->addr.end();

	if (b != e && b->type == 0 && b->str.size() == 0)
		++b;

	if (b == e)		/* Address is <> */
	{
		(*func)(p);
		return;
	}

	/*
	** Do not append the default domain to a UUCP address.
	*/

	for (auto &t:p->addr)
		if (t.type == '!' || t.type == '@')
		{
			(*func)(p);
			return;
		}

	p->addr.push_back({'@', "@"});
	p->addr.push_back(
		{0, p->mode & RW_HEADER ?
		 ACE_DOMAIN : I18N_DOMAIN});
	(*func)(p);
}

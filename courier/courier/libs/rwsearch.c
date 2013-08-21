/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
#include	<string.h>


struct rw_transport *rw_transport_first=0, *rw_transport_last=0;

struct rw_transport *rw_search_transport(const char *module)
{
struct rw_transport *p;

	for (p=rw_transport_first; p; p=p->next)
		if (strcmp(p->name, module) == 0)
			return (p);
	return (0);
}

#if 0
struct rw_list *rw_search(const char *funcname)
{
struct rw_transport *p;
struct rw_list *q;

	for (p=rw_transport_first; p; p=p->next)
		for (q=p->rw_ptr; q; q=q->next)
			if (strcmp(q->rewritename, funcname) == 0)
				return (q);
	return (0);
}
#endif

/*
** Copyright 1998 - 1999 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"rw.h"
#include	"courier.h"

void rw_info_init(struct rw_info *p, rfc822::tokens t,
		void (*err_func)(int, const char *, struct rw_info *p))
{
	p->addr=std::move(t);
	p->err_func=err_func;
}

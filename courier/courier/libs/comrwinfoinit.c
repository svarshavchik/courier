/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"rw.h"
#include	"courier.h"

void rw_info_init(struct rw_info *p, struct rfc822token *t,
		void (*err_func)(int, const char *, struct rw_info *p))
{
	p->mode=0;
	p->ptr=t;
	p->err_func=err_func;
	p->sender=0;
	p->udata=0;
	p->smodule=0;
	p->host=0;
}

/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
	/* Popular error handler - save error handler in udata pointer */

void rw_err_func(int n, const char *txt, struct rw_info *rwi)
{
	((struct rw_info_rewrite *)rwi->udata)->errmsg
			=makeerrmsgtext(n, txt);
}


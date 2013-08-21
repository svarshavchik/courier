/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	"rw.h"

/*
**	The dsn module is a noop - it is invoked automatically by courier,
**	as needed, to generate DSN messages.  By itself, it does not accept
**	any regular messages for delivery, but since it is initialized just
**	like any other transport module, it must provide a minimum
**	implementation of Courier's plug-in transport module mechanism.
*/

static void rw_dsn(struct rw_info *, void (*)(struct rw_info *));
static void rw_del_dsn(struct rw_info *, void (*)(struct rw_info *),
		void (*)(struct rw_info *, const struct rfc822token *,
			const struct rfc822token *));

static struct rw_transport *local;

struct rw_list *dsn_rw_install(const struct rw_install_info *p)
{
static struct rw_list dsn_info={0, "module.dsn - " COURIER_COPYRIGHT,
				rw_dsn, rw_del_dsn, 0};

	return (&dsn_info);
}

const char *dsn_rw_init()
{
	local=rw_search_transport("local");
	return (0);
}

static void rw_dsn(struct rw_info *p, void (*func)(struct rw_info *))
{
	if (local)
	{
		(*local->rw_ptr->rewrite)(p, func);
		return;
	}
	(*func)(p);
}

static void rw_del_dsn(struct rw_info *rwi,
			void (*nextfunc)(struct rw_info *),
		void (*delfunc)(struct rw_info *, const struct rfc822token *,
				const struct rfc822token *))
{
	delfunc=delfunc;
	(*nextfunc)(rwi);
}

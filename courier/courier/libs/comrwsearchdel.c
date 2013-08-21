/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"

struct rw_searchdel_info {
	struct rw_transport *cur_transport;
	void	*save_udata;
	void	(*save_err_func)(int, const char *, struct rw_info *);
	void	(*del_func)(struct rw_info *,
		struct rw_transport *, const struct rfc822token *,
			const struct rfc822token *);
} ;

static void do_next_rw(struct rw_info *, struct rw_searchdel_info *);

static void del_err_func(int errnum, const char *errmsg, struct rw_info *i)
{
struct rw_searchdel_info *p=(struct rw_searchdel_info *)i->udata;

	i->udata=p->save_udata;
	i->err_func=p->save_err_func;
	(*i->err_func)(errnum, errmsg, i);
	i->err_func=del_err_func;
	i->udata=(void *)p;
}

static void found_delivery(struct rw_info *i, const struct rfc822token *host,
	const struct rfc822token *addr)
{
struct rw_searchdel_info *p=(struct rw_searchdel_info *)i->udata;

	i->udata=p->save_udata;
	i->err_func=p->save_err_func;
	(*p->del_func)(i, p->cur_transport, host, addr);
	i->err_func=del_err_func;
	i->udata=(void *)p;
}

static void call_next_rw(struct rw_info *i)
{
struct rw_searchdel_info *p=(struct rw_searchdel_info *)i->udata;

	p->cur_transport=p->cur_transport->next;
	do_next_rw(i, p);
}

static void do_next_rw(struct rw_info *i, struct rw_searchdel_info *di)
{
	for ( ;di->cur_transport; di->cur_transport=di->cur_transport->next)
	{
		if (!di->cur_transport->rw_ptr)	continue;
		if (!di->cur_transport->rw_ptr->rewrite_del)	continue;
		(*di->cur_transport->rw_ptr->rewrite_del)(i, &call_next_rw,
			&found_delivery);
		break;
	}
}

void rw_searchdel(struct rw_info *i,
		void (*func)(struct rw_info *,
			struct rw_transport *, const struct rfc822token *,
			const struct rfc822token *))
{
struct rw_searchdel_info rwsdi;

	rwsdi.cur_transport=rw_transport_first;
	rwsdi.save_udata=i->udata;
	rwsdi.save_err_func=i->err_func;
	i->udata= (void *)&rwsdi;
	i->err_func=del_err_func;
	rwsdi.del_func=func;
	do_next_rw(i, &rwsdi);
	i->udata=rwsdi.save_udata;
	i->err_func=rwsdi.save_err_func;
}

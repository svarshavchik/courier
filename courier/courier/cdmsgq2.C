/*
** Copyright 1998 - 1999 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"cdpendelinfo.h"
#include	"cddlvrhost.h"
#include	"cddelinfo.h"
#include	"cddrvinfo.h"
#include	"cdrcptinfo.h"
#include	"cdmsgq.h"

#include	"courier.h"
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	<ctype.h>
#include	<errno.h>
#include	<stdlib.h>
#if HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#include	<time.h>

struct getdelinfo_struct {
public:
	getdelinfo_struct(drvinfo *d, std::string &h, std::string &a)
		: drvinfop(d), host(h), addr(a) {}
	drvinfo *drvinfop;
	std::string &host;
	std::string &addr;
	std::string errmsg;
} ;

static void gdi_err_func(int errnum, const char *p, struct rw_info *info)
{
char	*errmsg=makeerrmsgtext(errnum, p);

	((struct getdelinfo_struct *)info->udata)->errmsg=errmsg;
	free(errmsg);
}

static void found_module(struct rw_info *rwi, struct rw_transport *t,
			 const rfc822::tokens &host,
			 const rfc822::tokens &addr)
{
struct getdelinfo_struct *p=(struct getdelinfo_struct *)rwi->udata;

	if (!t->udata)	return;
	p->drvinfop=(drvinfo *)t->udata;

	size_t l=host.print( rfc822::length_counter{} );
	p->host.clear();
	p->host.reserve(l);
	host.print(std::back_inserter(p->host));

	l=addr.print( rfc822::length_counter{} );

	p->addr.clear();
	p->addr.reserve(l);
	addr.print(std::back_inserter(p->addr));
}

drvinfo *msgq::getdelinfo(rfc822::tokens sendert,
			  const char *receipient, std::string &host,
			  std::string &addr, std::string &errmsg)
{
	struct getdelinfo_struct gdis(0, host, addr);
	rw_info	rwi{RW_OUTPUT|RW_ENVRECIPIENT, std::move(sendert), {}};
	auto rfcp=rw_rewrite_tokenize(receipient);
	rw_info_init(&rwi, std::move(rfcp), gdi_err_func);
	rwi.udata= (void *)&gdis;
	rw_searchdel(&rwi, &found_module);
	if (!gdis.drvinfop && gdis.errmsg.size() == 0)
		gdis.errmsg="550 User unknown.";
	errmsg=gdis.errmsg;
	return (gdis.drvinfop);

}

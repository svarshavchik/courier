/*
** Copyright 1998 - 1999 Double Precision, Inc.
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
	const struct rfc822token *host, const struct rfc822token *addr)
{
struct getdelinfo_struct *p=(struct getdelinfo_struct *)rwi->udata;

	if (!t->udata)	return;
	p->drvinfop=(drvinfo *)t->udata;

	char *s=rfc822_gettok(host);

	if (!s)	clog_msg_errno();
	p->host=s;
	free(s);

	s=rfc822_gettok(addr);

	if (!s) clog_msg_errno();
	p->addr=s;
	free(s);
}

drvinfo *msgq::getdelinfo(struct rfc822token *sendert,
			  const char *receipient, std::string &host,
			  std::string &addr, std::string &errmsg)
{
	struct getdelinfo_struct gdis(0, host, addr);
	struct	rw_info	rwi;
	struct rfc822t *rfcp=rw_rewrite_tokenize(receipient);

	rw_info_init(&rwi, rfcp->tokens, gdi_err_func);
	rwi.sender=sendert;
	rwi.mode=RW_OUTPUT|RW_ENVRECIPIENT;
	rwi.udata= (void *)&gdis;
	rw_searchdel(&rwi, &found_module);
	rfc822t_free(rfcp);
	if (!gdis.drvinfop && gdis.errmsg.size() == 0)
		gdis.errmsg="550 User unknown.";
	errmsg=gdis.errmsg;
	return (gdis.drvinfop);

}


/*
** Copyright 1998 - 2002 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	"dbobj.h"
#include	"comfax.h"
#include	<string.h>
#include	<ctype.h>
#include	<stdlib.h>
#include	<idn2.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<algorithm>

#ifndef GETENV
#define GETENV getenv
#endif

static int acceptdomain(std::string);
static void rw_esmtp(struct rw_info *, void (*)(struct rw_info *));

static void rw_del_esmtp(struct rw_info *, void (*)(struct rw_info *),
			 void (*)(struct rw_info *, const rfc822::tokens &,
				  const rfc822::tokens &));

struct rw_list *esmtp_rw_install(const struct rw_install_info *p)
{
static struct rw_list esmtp_info={0, "module.esmtp - " COURIER_COPYRIGHT,
				 rw_esmtp, rw_del_esmtp, 0};

	return (&esmtp_info);
}

const char *esmtp_rw_init()
{
	return (0);
}

static rfc822::tokens::iterator rfc822check(
	int err_code,
	struct rw_info *info,
	bool allow_faxdomain)
{
	rfc822::tokens::iterator p, s;

	for (s=info->addr.begin(), p=info->addr.end();
	     s != info->addr.end(); ++s)
		if (s->type == '@')
			p=s;
	if (p != info->addr.end())
	{
		int	seendot=1;
		int	seen1dot=0;
		int	dummy;

		for (s=p+1; s != info->addr.end(); ++s)
		{
			if (s->type == 0)
			{
				if (!seendot)	break;
				seendot=0;
			}
			else if (s->type == '.')
			{
				if (seendot)	break;
				seendot=1;
				seen1dot=1;
			}
			else	break;
		}

		/*
		** Normally, @fax-lowres would trip the above check.
		** If rfc822check is being called to validate the recipient
		** address, relax this syntax check.
		*/

		if (s == info->addr.end() && !seen1dot && allow_faxdomain &&
		    p+1 != info->addr.end() &&
		    p+2 == info->addr.end() &&
		    p[1].type == 0 &&
		    comgetfaxopts(p[1].str, &dummy) &&
		    GETENV("FAXRELAYCLIENT") != NULL)
			return (p);

		if (s == info->addr.end() && seen1dot)	return (p);
	}

	(*info->err_func)(err_code, "Syntax error.", info);
	return (info->addr.end());
}

static int rwrecip(struct rw_info *info)
{
	if (rfc822check(513, info, true) == info->addr.end())	return (-1);

	/*
		Rewrite @foobar:foo@bar into foo@bar if @foobar is us.
	*/

	for (;;)
	{
		if (info->addr.empty() || info->addr[0].type != '@')
			break;

		auto p=std::find_if(
			info->addr.begin(),
			info->addr.end(),
			[](auto &token)
			{
				return token.type == ':';
			});

		if (p == info->addr.end()) break;

		std::string ignore;

		if (configt_islocal(info->addr.begin()+1,
				    p,
				    ignore))
		{
			info->addr.erase(info->addr.begin(), ++p);
			break;
		}

		/* foobar is nonlocal, rewrite as foo@bar@foobar */

		rfc822::tokens buf{info->addr.begin(),  p};

		info->addr.erase(info->addr.begin(), ++p);
		info->addr.insert(info->addr.end(),
				  buf.begin(),
				  buf.end());
		break;
	}

	/* Rewrite foo@bar@foobar as foo%bar@foobar */

	auto q=info->addr.begin();
	for (auto p=q; p != info->addr.end(); ++p)
		if (p->type == ':')
			q=p+1;

	auto p=info->addr.end();

	for (; q != info->addr.end(); ++q)
	{
		if (q->type == '@' || q->type == '%')
		{
			if (p != info->addr.end())
				p->type='%';
			p=q;
		}
	}

	if (p != info->addr.end())
	{
		p->type='@';
	}

	/* One more time */

	if ((p=rfc822check(513, info, true)) == info->addr.end())
		return (-1);

	/* When called from submit (initial receipt of a message),
	** either RELAYCLIENT must be set, or this must be one of our domains.
	*/

	std::string s;

	s.reserve(rfc822::tokens::print(p+1, info->addr.end(),
					rfc822::length_counter{}));
	rfc822::tokens::print(p+1, info->addr.end(),
			      std::back_inserter(s));

	if (!GETENV("RELAYCLIENT") && !acceptdomain(std::move(s)))
	{
		(*info->err_func)(513, "Relaying denied.", info);
		return (-1);
	}
	return (0);
}

static int isindomaindb_utf8(const char *address, struct dbobj *db)
{
	const char	*p;
	int	n=8;

	p=address;

	while (*p && n)
	{
		if (dbobj_exists(db, p, strlen(p)))
			return (1);
		if (*p == '.')	++p;
		while (*p && *p != '.')
			++p;
		--n;
	}
	return (0);
}

static int isindomaindb(const char *p, struct dbobj *db)
{
	char *address_utf8_str;
	std::string address_utf8;
	char *q;
	int ret;
	char *hlocal;

	if (idna_to_unicode_8z8z(p, &address_utf8_str, 0)
	    == IDNA_SUCCESS)
	{
		address_utf8=address_utf8_str;
		free(address_utf8_str);
	}
	else
	{
		address_utf8=p;
	}

	q=ualllower(address_utf8.c_str());

	hlocal=config_is_gethostname(q);
	if (hlocal &&
	    isindomaindb_utf8(hlocal, db))
		ret=1;
	else
		ret=isindomaindb_utf8(q, db);

	if (hlocal)
		free(hlocal);
	free(q);
	return ret;
}

static int ispercenthack(rfc822::tokens::iterator b, rfc822::tokens::iterator e)
{
	static char *percenthack=0;
	static struct dbobj percenthackdat;
	char *p;

	if (!percenthack)	/* First time */
	{
		static char zero=0;

		p=config_localfilename("esmtppercentrelay");
		percenthack=readfile(p, 0);
		free(p);

		if (percenthack)
			removecomments(percenthack);
		else
			percenthack=&zero;
		dbobj_init(&percenthackdat);
		p=config_localfilename("esmtppercentrelay.dat");
		dbobj_open(&percenthackdat, p, "R");
		free(p);
	}

	if (*percenthack == 0 && !dbobj_isopen(&percenthackdat))
		return (0);	/* Don't bother */

	std::string pbuf;
	pbuf.reserve(rfc822::tokens::print(b, e, rfc822::length_counter{}));
	rfc822::tokens::print(b, e, std::back_inserter(pbuf));

	if (config_is_indomain(pbuf.c_str(), percenthack) ||
		(dbobj_isopen(&percenthackdat) &&
		 isindomaindb(pbuf.c_str(), &percenthackdat)))
	{
		return (1);
	}

	return (0);
}

/*
** Transform  foo%bar@foobar into foo@bar, if foobar is a local domain,
** and bar can be found in percenthack.
*/

static void rwinput(struct rw_info *info, void (*func)(struct rw_info *))
{
	auto start=info->addr.begin();

	for (auto p=start; p != info->addr.end(); ++p)
		if (p->type == ':')
			start = p+1;

	auto p=info->addr.end(), ptr=start;
	for (; ptr != info->addr.end(); ++ptr)
	{
		if (ptr->type == '%')
			p= ptr;
		if ( ptr->type == '@' )
			break;
	}

	std::string domain;
	if (p == info->addr.end() || ptr == info->addr.end() ||
	    !configt_islocal(ptr+1, info->addr.end(), domain))
	{
		(*func)(info);
		return;
	}

	if (p != info->addr.end() && ispercenthack(p+1, ptr))
	{
		p->type='@';
		info->addr.erase(ptr, info->addr.end());
	}
	(*func)(info);
}

/* Do the opposite of rwinput */

static void rwoutput(struct rw_info *info, void (*func)(struct rw_info *))
{
	if (info->addr.empty())
	{
		(*func)(info);
		return;
	}

	auto q=info->addr.end();
	for (auto p=info->addr.begin(); p != info->addr.end(); ++p)
		if (p->type == '@')	q=p;

	if (q != info->addr.end())
	{
		if (!ispercenthack(q+1, info->addr.end()))
		{
			(*func)(info);
			return;
		}

		q->type='%';
	}

	auto tp=rw_rewrite_tokenize(config_defaultdomain());
	info->addr.reserve(info->addr.size() + tp.size()+1);

	info->addr.push_back({'@', "@"});
	info->addr.insert(info->addr.end(), tp.begin(), tp.end());

	(*func)(info);
}

static void rw_esmtp(struct rw_info *info, void (*func)(struct rw_info *))
{
	if ((info->mode & RW_SUBMIT)	/* From submit process */
		&& (info->mode & RW_OUTPUT) == 0)
				/* NOT for output rewriting */
	{
		if (info->mode & RW_ENVSENDER)
		{
			if (!info->addr.empty() &&
			    (info->addr[0].type || info->addr[0].str.size()))
				/* NULL sender OK */
			{
				if (rfc822check(517, info, false)
				    == info->addr.end())
					return;
			}
		}
		else if (info->mode & RW_ENVRECIPIENT)
		{
			if (rwrecip(info))	return;
		}
	}

	if (info->mode & RW_OUTPUT)
		rwoutput(info, func);
	else
		rwinput(info, func);
}

static void rw_del_esmtp(struct rw_info *rwi,
			 void (*nextfunc)(struct rw_info *),
			 void (*delfunc)(struct rw_info *,
					 const rfc822::tokens &,
					 const rfc822::tokens &))
{
	auto p=rwi->addr.begin();

	for (; p != rwi->addr.end(); p++)
	{
		if (p->type == '!')
		{
			(*nextfunc)(rwi);	/* We don't talk UUCP */
			return;
		}
		if (p->type == '@')	break;
	}

	if (p == rwi->addr.end())
	{
		(*nextfunc)(rwi);
		return;
	}

	std::string domain;
	if (configt_islocal(p+1, rwi->addr.end(), domain))
	{
		(*nextfunc)(rwi);
				/* Local module should handle it */
		return;
	}

	if (rwi->mode & RW_VERIFY)
	{
		(*rwi->err_func)(550, "Remote address.", rwi);
		return;
	}

	std::string ac;

	ac.reserve(rfc822::tokens::print(rwi->addr.begin(), rwi->addr.end(),
					 rfc822::length_counter{}));
	rfc822::tokens::print(rwi->addr.begin(), rwi->addr.end(),
			      std::back_inserter(ac));

	if (ac.find('@') == ac.npos)
	{
		(*nextfunc)(rwi); /* Local? */
		return;
	}

	auto c=udomainlower(ac.c_str());

	ac=c;
	free(c);

	rfc822::tokens host;

	host.push_back({0, strchr(ac.c_str(), '@')+1});

	rfc822::tokens addr;

	addr.push_back({0, ac.c_str()});

	(*delfunc)(rwi, host, addr);
}


/************************************************************************/
/*               Should we accept mail for this domain?                 */
/************************************************************************/

static int acceptdomain(std::string address)
{
int	rc;

static const char *acceptdomains=0;
static struct dbobj acceptdomainsdb;

	if (!acceptdomains)
	{
        char *filename=config_localfilename("esmtpacceptmailfor");
	char	*buf;

		buf=readfile(filename, 0);

                free(filename);
                if (!buf)
			acceptdomains=config_me();
                else
                {
                        removecomments(buf);
			acceptdomains=buf;
                }

		dbobj_init(&acceptdomainsdb);
		filename=config_localfilename("esmtpacceptmailfor.dat");
		dbobj_open(&acceptdomainsdb, filename, "R");
		free(filename);
	}

	rc=config_is_indomain(address.c_str(), acceptdomains);
	if (rc == 0 && dbobj_isopen(&acceptdomainsdb))
		rc=isindomaindb(address.c_str(), &acceptdomainsdb);

	return (rc);
}

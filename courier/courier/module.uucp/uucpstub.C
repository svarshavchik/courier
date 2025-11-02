/*
** Copyright 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	"rw.h"
#include	"uucpfuncs.h"
#include	"rfc822/rfc822.h"

#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

static void rw_uucp(struct rw_info *, void (*)(struct rw_info *));
static void rw_del_uucp(struct rw_info *, void (*)(struct rw_info *),
			void (*)(struct rw_info *, const rfc822::tokens &,
				 const rfc822::tokens &));

struct rw_list *uucp_rw_install(const struct rw_install_info *p)
{
static struct rw_list uucp_info={0, "module.uucp - " COURIER_COPYRIGHT,
				rw_uucp, rw_del_uucp, 0};

	return (&uucp_info);
}

const char *uucp_rw_init()
{
	return (0);
}

static int uucprw= -1;

static void prepend_me(struct rw_info *p, void (*func)(struct rw_info *));

static void rw_uucp(struct rw_info *p, void (*func)(struct rw_info *))
{
	/* Only rewrite headers if uucprewriteheaders is set */

	if ((p->mode & RW_HEADER) != 0)
	{
		if (uucprw < 0)
		{
			uucprw=uucprewriteheaders();
		}

		if (!uucprw)
		{
			(*func)(p);
			return;
		}
	}

	if ((p->mode & RW_OUTPUT) == 0)
	{
		/* Convert UUCP address to Internet address */

		/* First, remove me!foo!bar */

		auto b=p->addr.begin(), bb=b, e=p->addr.end();

		while (b != e)
		{
			if (b->type == '!')
				break;
			++b;
		}

		std::string s;
		size_t l=rfc822::tokens::print(bb, b,
					       rfc822::length_counter{} );
		s.reserve(l);

		rfc822::tokens::print(bb, b, std::back_inserter(s));

		if (s == uucpme() && b != e && ++b != e)
		{
			p->addr.erase(p->addr.begin(), b);
		}

		b=p->addr.begin(); e=p->addr.end();

		/*
		** If address already contains an @, assume it's already an
		** Internet address.
		*/

		for (auto &r:p->addr)
			if (r.type == '@')
			{
				(*func)(p);
				return;
			}

		for (bb=b; bb != e; ++bb)
		{
			if (bb->type == '!')
				break;
		}

		if (auto iter=bb; iter != e && ++iter != e)
		{
			uucpneighbors_init();

			std::string addr;
			size_t l=rfc822::tokens::print(
				b, bb,
				rfc822::length_counter{}
			);

			addr.reserve(l);
			rfc822::tokens::print(
				b, bb,
				std::back_inserter(addr)
			);

			size_t uunl=0;
			char *uun=uucpneighbors_fetch(
				addr.c_str(), addr.size(), &uunl, ""
			);

			if (uun)	/* We will relay this via UUCP */
			{
				free(uun);
				(*func)(p);
				return;
			}
		}
		else /* Only one node - must be a local address */
		{
			p->addr.erase(bb, p->addr.end()); // Trailing !

			rw_local_defaulthost(p, func);
			return;
		}

		/* Rewrite domain.com!foo as foo@domain.com */

		rfc822::tokens host{p->addr.begin(), bb};

		p->addr.erase(p->addr.begin(), ++bb); // Not end, checked in if
		p->addr.push_back({'@', ""});
		p->addr.insert(p->addr.end(), host.begin(), host.end());
		(*func)(p);
		return;
	}

/* From canonical to UUCP */

/* Ok, if we have user@host, rewrite it either as user, if host is us, or
** host!user */

	for (auto b=p->addr.begin(), q=b, e=p->addr.end(); q != e; ++q)
	{
		if ( q->type == '@' )
		{
			std::string hostdomain;

			auto r=q;
			if (configt_islocal(++r, e, hostdomain)
			    && hostdomain.empty())
			{
				p->addr.erase(q, e);
				break;
			}

			rfc822::tokens host{r, e};
			p->addr.erase(q, e);

			host.push_back({'!', ""});

			p->addr.insert(p->addr.begin(), host.begin(),
				       host.end());

			prepend_me(p, func);
			return;
		}
	}
	prepend_me(p, func);
}

static void prepend_me(struct rw_info *p, void (*func)(struct rw_info *))
{

	if (!p->addr.empty() && p->addr.begin()->type == 0 &&
	    p->addr.begin()->str.size() == 0)
	{
		p->addr.erase(p->addr.begin(), ++p->addr.begin());
	}

	if (p->addr.empty())
	{
		(*func)(p);
		return;
	}

	/* Before we prepend uucp! to an address, see if this address is
	** "a!b!c!d...", and we are forwarding it to "a!b!rmail", in which
	** case we'll rewrite it as "c!d!..."
	*/

	if (!p->host.empty())
	{
		auto hb=p->host.begin(), he=p->host.end();

		auto ab=p->addr.begin(), ae=p->addr.end();

		for (; hb != he && ab != ae; ++hb, ++ab)
		{
			if (hb->type != ab->type)	break;
			if (!rfc822_is_atom(hb->type))	continue;

			if (hb->str.size() != ab->str.size()) break;
			if (memcmp(hb->str.data(),
				   ab->str.data(),
				   hb->str.size()))
				break;
		}

		if (hb == he && ab != ae && ab->type == '!' && ++ab != ae)
		{
			p->addr.erase(p->addr.begin(), ab);
			(*func)(p);
			return;
		}
	}

	rfc822::token mebang[2]={
		{ 0, uucpme() },
		{ '!', "!" }
	};

	p->addr.insert(p->addr.begin(), mebang, mebang+2);
	(*func)(p);
}

static void rw_del_uucp(
	struct rw_info *rwi,
	void (*nextfunc)(struct rw_info *),
	void (*delfunc)(struct rw_info *, const rfc822::tokens &,
			const rfc822::tokens &))
{
	bool hasbang{false};

	auto ab=rwi->addr.begin(), prev=ab, ae=rwi->addr.end();

	for ( ; prev != ae; ++prev)
	{
		if (prev->type == '!')
			hasbang=true;
		if (prev->type == '@')
			break;
	}

	if (!hasbang)
	{
		(*nextfunc)(rwi);
		return;
	}

	if (auto p=prev; p != ae && ++p != ae)
	{
		std::string hostdomain;

		if (!configt_islocal(p, ae, hostdomain))
		{
			(*nextfunc)(rwi);
			return;
		}
	}

	// hasbang=true;

	for (auto p=rwi->addr.begin(); p != prev; ++p, hasbang=false)
	{
		if (p->type != '!')	continue;

		auto q=p;
		if (hasbang || ++q == prev || q->type == '!')
		{
			(*rwi->err_func)(550, "Invalid UUCP bang path.", rwi);
			return;
		}
	}

	rwi->addr.erase(prev, ae);

	uucpneighbors_init();

	ab=rwi->addr.begin(), ae=rwi->addr.end();
	auto uux_bang=ae;

	for (prev=ab; prev != ae; ++prev)
	{
		char *uun;
		size_t	uunl;
		size_t	i;

		if (prev->type != '!')	continue;

		size_t l=rfc822::tokens::print(ab, prev,
					       rfc822::length_counter{} );
		std::string addr;
		addr.reserve(l);
		rfc822::tokens::print(ab, prev, std::back_inserter(addr));

		uunl=0;

		uun=uucpneighbors_fetch(addr.c_str(),
					addr.size(),
					&uunl, "");

		if (uun == nullptr)
		{
			prev=ae;
			break;
		}

		for (i=0; i<uunl; i++)
			if (uun[i] == 'R')
			{
				/* Relay, run rmail here */

				if (uux_bang == ae)
					uux_bang= prev;
				break;
			}

		if (uux_bang != ae)
			break;
		for (i=0; i<uunl; i++)
			if (uun[i] == 'G')
				break;

		if (i < uunl) 	/* Gateway, ignore rest of path */
		{
			if (uux_bang != ae)
				uux_bang=ae;
			free(uun);
			break;
		}

		auto p=prev;

		++p;

		if (p != ae)
		{
			auto q=p;
			for (; q != ae; ++q)
				if (q->type == '!')
					break;

			if (q == ae)
			{
				uux_bang=prev;
				free(uun);
				break;
			}
		}

		/* This one must marked as F - OK to forward through here */

		for (i=0; i<uunl; i++)
			if (uun[i] == 'F')
				break;
		free(uun);

		if ( i < uunl)	break;
	}

	if (prev == ae)
	{
		(*rwi->err_func)(550, "Unknown UUCP bang path.", rwi);
		return;
	}

	if (uux_bang == ae)
	{
		for (prev=ab; prev != ae; ++prev)
			if ( prev->type == '!' )
				uux_bang= prev;
	}

	auto p=uux_bang;

	rfc822::tokens deladdr{++p, ae};

	rwi->addr.erase(uux_bang, ae);

	if (rwi->mode & RW_VERIFY)
	{
		(*rwi->err_func)(550, "Remote address.", rwi);
		return;
	}

	(*delfunc)(rwi, rwi->addr, deladdr);
}

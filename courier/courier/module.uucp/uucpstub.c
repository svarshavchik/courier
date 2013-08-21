/*
** Copyright 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	"rw.h"
#include	"dbobj.h"
#include	"rfc822/rfc822.h"
#include	"sysconfdir.h"

#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

static void rw_uucp(struct rw_info *, void (*)(struct rw_info *));
static void rw_del_uucp(struct rw_info *, void (*)(struct rw_info *),
		void (*)(struct rw_info *, const struct rfc822token *,
			const struct rfc822token *));

static const char *uucpme();

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

static struct dbobj uucpneighbors;
static int uucpneighbors_isopen=0;

static void uucpneighbors_init()
{
	if (uucpneighbors_isopen)	return;

	dbobj_init(&uucpneighbors);
	uucpneighbors_isopen=1;
	if (dbobj_open(&uucpneighbors, SYSCONFDIR "/uucpneighbors.dat", "R"))
		uucpneighbors_isopen= -1;
}

static const char *uucpme()
{
static const char *buf=0;

	if (buf == 0)
	{
	char	*f=config_localfilename("uucpme");

		buf=config_read1l(f);
		free(f);
		if (buf == 0)
		{
		const char *p=config_me();

			buf=strcpy(courier_malloc(strlen(p)+1), p);
			if ((f=strchr(buf, '.')) != 0)	*f=0;
		}
	}
	return (buf);
}

static int uucprw= -1;

static void prepend_me(struct rw_info *p, void (*func)(struct rw_info *));

static void rw_uucp(struct rw_info *p, void (*func)(struct rw_info *))
{
struct rfc822token **q;

	/* Only rewrite headers if uucprewriteheaders is set */

	if ((p->mode & RW_HEADER) != 0)
	{
		if (uucprw < 0)
		{
		char	*f=config_localfilename("uucprewriteheaders");

			uucprw= access(f, 0) == 0 ? 1:0;
			free(f);
		}

		if (!uucprw)
		{
			(*func)(p);
			return;
		}
	}

	if ((p->mode & RW_OUTPUT) == 0)
	{
	struct	rfc822token *r, *r2;
	char	*s;

		/* Convert UUCP address to Internet address */

		/* First, remove me!foo!bar */

		for (q= &p->ptr; *q; q= & (*q)->next )
			if ((*q)->token == '!')	break;
		r= *q;
		*q=0;

		s=rfc822_gettok(p->ptr);

		if (!s)	clog_msg_errno();
		if (r && strcmp(s, uucpme()) == 0)
		{
			p->ptr=r->next;
		}
		else	*q=r;
		free(s);

		/*
		** If address already contains an @, assume it's already an
		** Internet address.
		*/

		for (r= p->ptr; r; r= r->next )
			if (r->token == '@')
			{
				(*func)(p);
				return;
			}

		for (q= &p->ptr; *q; q= & (*q)->next )
		{
			if ((*q)->token == '!')
				break;
		}

		if (*q)
		{
		char	*uun;
		size_t	uunl;
		char	*addr;

			uucpneighbors_init();

			r=*q;
			*q=0;
			addr=rfc822_gettok(p->ptr);
			*q=r;

			if (!addr)	clog_msg_errno();

			uun=0;
			if (uucpneighbors_isopen)
				uun=dbobj_fetch(&uucpneighbors, addr,
					strlen(addr), &uunl, "");
			free(addr);

			if (uun)	/* We will relay this via UUCP */
			{
				free(uun);
				(*func)(p);
				return;
			}
		}
		else /* Only one node - must be a local address */
		{
			rw_local_defaulthost(p, func);
			return;
		}

		/* Rewrite domain.com!foo as foo@domain.com */

		r= *q;

		*q=0;

		for (q= &r->next; *q; q= & (*q)->next )
			;

		*q=r;
		r2=r->next;
		r->next=p->ptr;
		r->token='@';
		p->ptr=r2;
		(*func)(p);
		return;
	}

/* From canonical to UUCP */

/* Ok, if we have user@host, rewrite it either as user, if host is us, or
** host!user */

	for (q= &p->ptr; *q; q= & (*q)->next )
		if ( (*q)->token == '@' )
		{
		struct rfc822token *at, *host, **z;
		char	*hostdomain=0;

			if (configt_islocal((*q)->next,
				&hostdomain) && hostdomain == 0)
			{
				*q=0;
				break;
			}
			if (hostdomain)	free(hostdomain);

			at= *q;
			at->token='!';
			host=at->next;
			*q=0;
			at->next=p->ptr;
			p->ptr=host;

			for (z= &p->ptr; *z; z= & (*z)->next)
				;

			*z=at;

			prepend_me(p, func);
			return;
		}

	prepend_me(p, func);
}

static void prepend_me(struct rw_info *p, void (*func)(struct rw_info *))
{
struct rfc822token metoken, bangtoken;

	if (p->ptr && p->ptr->token == 0 && p->ptr->len == 0)
		p->ptr=p->ptr->next;

	if (p->ptr == 0)
	{
		(*func)(p);
		return;
	}

	/* Before we prepend uucp! to an address, see if this address is
	** "a!b!c!d...", and we are forwarding it to "a!b!rmail", in which
	** case we'll rewrite it as "c!d!..."
	*/

	if (p->host)
	{
	const struct rfc822token *q;
	struct rfc822token *r;

		for (q=p->host, r=p->ptr; q && r; q=q->next, r=r->next)
		{
			if (q->token != r->token)	break;
			if (!rfc822_is_atom(q->token))	continue;
			if (q->len != r->len)	break;
			if (memcmp(q->ptr, r->ptr, q->len))	break;
		}

		if (q == 0 && r && r->token == '!' && r->next)
		{
			p->ptr=r->next;
			(*func)(p);
			return;
		}
	}

	metoken.token=0;
	metoken.ptr=uucpme();
	metoken.len=strlen(metoken.ptr);
	metoken.next= &bangtoken;
	bangtoken.token='!';
	bangtoken.ptr="!";
	bangtoken.len=1;
	bangtoken.next=p->ptr;
	p->ptr= &metoken;
	(*func)(p);
}

static void rw_del_uucp(struct rw_info *rwi,
			void (*nextfunc)(struct rw_info *),
		void (*delfunc)(struct rw_info *, const struct rfc822token *,
				const struct rfc822token *))
{
int	hasbang=0;
struct rfc822token **prev, *p;
struct rfc822token **uux_bang;

	for (prev=&rwi->ptr; *prev; prev=&(*prev)->next)
	{
		if ((*prev)->token == '!')
			hasbang=1;
		if ((*prev)->token == '@')
			break;
	}

	if (!hasbang)
	{
		(*nextfunc)(rwi);
		return;
	}

	if ( (p=*prev) != 0 && p->next)
	{
	char	*hostdomain=0;

		if (!configt_islocal(p->next, &hostdomain))
		{
			(*nextfunc)(rwi);
			return;
		}
		if (hostdomain)	free(hostdomain);
	}

	*prev=0;

	hasbang=1;
	for (p=rwi->ptr; p; p=p->next, hasbang=0)
	{
		if (p->token != '!')	continue;
		if (hasbang || p->next == 0 || p->next->token == '!')
		{
			(*rwi->err_func)(550, "Invalid UUCP bang path.", rwi);
			return;
		}
	}

	uux_bang=0;

	uucpneighbors_init();

	for (prev=&rwi->ptr; *prev; prev=&(*prev)->next)
	{
	char	*addr;
	char	*uun;
	size_t	uunl;
	size_t	i;

		if ( (*prev)->token != '!')	continue;
		p= *prev;
		*prev=0;
		addr=rfc822_gettok(rwi->ptr);
		*prev=p;
		if (!addr)	clog_msg_errno();

		uun=0;
		uunl=0;

		if (uucpneighbors_isopen)
			uun=dbobj_fetch(&uucpneighbors, addr, strlen(addr),
				&uunl, "");
		free(addr);

		if (uun == 0)
		{
			prev=0;
			break;
		}

		for (i=0; i<uunl; i++)
			if (uun[i] == 'R')
			{
				/* Relay, run rmail here */

				if (!uux_bang)
					uux_bang= prev;
				break;
			}

		for (i=0; i<uunl; i++)
			if (uun[i] == 'G')
				break;

		if (i < uunl) 	/* Gateway, ignore rest of path */
		{
			if (!uux_bang)
				uux_bang= prev;
			free(uun);
			break;
		}

		p= (*prev)->next;

		if (p->next == 0)
		{
			free(uun);
			break;
		}

		/* This one must marked as F - OK to forward through here */

		for (i=0; i<uunl; i++)
			if (uun[i] == 'F')
				break;
		free(uun);

		if ( i < uunl)	break;
	}

	if (!prev || *prev == 0)
	{
		(*rwi->err_func)(550, "Unknown UUCP bang path.", rwi);
		return;
	}

	if (uux_bang == 0)
	{
		for (prev=&rwi->ptr; *prev; prev=&(*prev)->next)
			if ( (*prev)->token == '!' )
				uux_bang= prev;
	}

	p= (*uux_bang)->next;
	*uux_bang=0;

	if (rwi->mode & RW_VERIFY)
	{
		(*rwi->err_func)(550, "Remote address.", rwi);
		return;
	}

#if 1
	(*delfunc)(rwi, rwi->ptr, p);
#else
	{
	char *a=rfc822_gettok(rwi->ptr);
	char *b=rfc822_gettok(p);
	char *c;

		if (!a || !b || !(c=malloc(strlen(a)+strlen(b)+100)))
		{
			clog_msg_errno();
			return;
		}

		strcat(strcat(strcat(strcpy(c, "uux "), a), "!rmail "), b);
		(*rwi->err_func)(550, c, rwi);
		free(a);
		free(b);
		free(c);
	}
#endif
}

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
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

static int acceptdomain(const struct rfc822token *);
static void rw_esmtp(struct rw_info *, void (*)(struct rw_info *));

static void rw_del_esmtp(struct rw_info *, void (*)(struct rw_info *),
		void (*)(struct rw_info *, const struct rfc822token *,
			const struct rfc822token *));

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

static struct rfc822token *rfc822check(int err_code,
	struct rw_info *info, int allow_faxdomain)
{
struct rfc822token *p, *s;

	for (p=0, s=info->ptr; s; s=s->next)
		if (s->token == '@')	p=s;
	if (p)
	{
	int	seendot=1;
	int	seen1dot=0;
	int	dummy;

		for (s=p->next; s; s=s->next)
		{
			if (s->token == 0)
			{
				if (!seendot)	break;
				seendot=0;
			}
			else if (s->token == '.')
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

		if (s == 0 && !seen1dot && allow_faxdomain &&
		    p->next && !p->next->next && p->next->token == 0 &&
		    comgetfaxoptsn(p->next->ptr, p->next->len, &dummy) == 0 &&
		    getenv("FAXRELAYCLIENT") != NULL)
			return (p);
		    
		if (s == 0 && seen1dot)	return (p);
	}

	(*info->err_func)(err_code, "Syntax error.", info);
	return (0);
}

static int rwrecip(struct rw_info *info)
{
struct rfc822token *p, *q;

	if (rfc822check(513, info, 1) == 0)	return (-1);

	/*
		Rewrite @foobar:foo@bar into foo@bar if @foobar is us.
	*/

	for (;;)
	{
		if (info->ptr == 0 || info->ptr->token != '@')
			break;
		for (p=info->ptr; p->next; p=p->next)
			if (p->next->token == ':')	break;
		if (!p->next)	break;
		q=p->next;
		p->next=0;
		if (configt_islocal(info->ptr->next, 0))
		{
			info->ptr=q->next;
			continue;
		}
		/* foobar is nonlocal, rewrite as foo%bar@foobar */
		p=info->ptr;
		info->ptr=q->next;
		while (q->next)
			q=q->next;
		q->next=p;
		break;
	}

	/* Rewrite foo@bar@foobar as foo%bar@foobar */

	for (p=info->ptr, q=p; p; p=p->next)
		if (p->token == ':')	q=p->next;

	p=0;
	while (q)
	{
		if (q->token == '@' || q->token == '%')
		{
			if (p)	p->token='%';
			p=q;
		}
		q=q->next;
	}
	if (p)	p->token='@';

	/* One more time */

	if ((p=rfc822check(513, info, 1)) == 0)	return (-1);

	/* When called from submit (initial receipt of a message),
	** either RELAYCLIENT must be set, or this must be one of our domains.
	*/

	if (!getenv("RELAYCLIENT") && !acceptdomain(p->next))
	{
		(*info->err_func)(513, "Relaying denied.", info);
		return (-1);
	}
	return (0);
}

static int isindomaindb(char *address, struct dbobj *db)
{
char	*p;
int	n=8;

	for (p=address; *p; p++)
		*p=tolower(*p);

	p=address;

	while (*p && n)
	{
		if (dbobj_exists(db, p, strlen(p)))
			return (1);
		if (*p == '.')	++p;
		while (*p && *p != '.')
			++p;
	}
	return (0);
}

static int ispercenthack(struct rfc822token *ptr)
{
static char *percenthack=0;
static struct dbobj percenthackdat;
char	*p;

	if (!percenthack)	/* First time */
	{
		p=config_localfilename("esmtppercentrelay");

		percenthack=readfile(p, 0);
		free(p);

		if (percenthack)
			removecomments(percenthack);
		else
			percenthack="";
		dbobj_init(&percenthackdat);
		p=config_localfilename("esmtppercentrelay.dat");
		dbobj_open(&percenthackdat, p, "R");
		free(p);
	}

	if (*percenthack == 0 && !dbobj_isopen(&percenthackdat))
		return (0);	/* Don't bother */

	p=rfc822_gettok(ptr);
	if (config_is_indomain(p, percenthack) ||
		(dbobj_isopen(&percenthackdat) &&
			isindomaindb(p, &percenthackdat)))
	{
		free(p);
		return (1);
	}

	free(p);
	return (0);
}

/*
** Transform  foo%bar@foobar into foo@bar, if foobar is a local domain,
** and bar can be found in percenthack.
*/

static void rwinput(struct rw_info *info, void (*func)(struct rw_info *))
{
struct rfc822token *p, *q, **start, **ptr;

	for (start= &info->ptr, p=info->ptr; p; p=p->next)
		if (p->token == ':')
			start =&p->next;

	for (p=0, ptr=start; *ptr; ptr= &(*ptr)->next)
	{
		if ( (*ptr)->token == '%' )
			p= *ptr;
		if ( (*ptr)->token == '@' )
			break;
	}
	if (!p || *ptr == 0 || !configt_islocal((*ptr)->next, 0))
	{
		(*func)(info);
		return;
	}

	q= *ptr;
	*ptr=0;
	if (ispercenthack(p->next))
		p->token='@';
	else	*ptr=q;
	(*func)(info);
}

/* Do the opposite of rwinput */

static void rwoutput(struct rw_info *info, void (*func)(struct rw_info *))
{
struct rfc822token *p, *q, **r;
const char *me;
struct rfc822t	*tp;
struct rfc822token at;

	if (info->ptr == 0)
	{
		(*func)(info);
		return;
	}

	for (q=0, p=info->ptr; p; p=p->next)
		if (p->token == '@')	q=p;

	if (q)
	{
		if (!ispercenthack(q->next))
		{
			(*func)(info);
			return;
		}

		q->token='%';
	}

	for (r= &info->ptr; *r; r= &(*r)->next)
		;

	at.token='@';
	at.ptr=0;
	at.len=0;
	me=config_defaultdomain();
	tp=rw_rewrite_tokenize(me);
	at.next=tp->tokens;

	*r=&at;
	(*func)(info);
	*r=0;
	rfc822t_free(tp);
}

static void rw_esmtp(struct rw_info *info, void (*func)(struct rw_info *))
{
	if ((info->mode & RW_SUBMIT)	/* From submit process */
		&& (info->mode & RW_OUTPUT) == 0)
				/* NOT for output rewriting */
	{
		if (info->mode & RW_ENVSENDER)
		{
			if (info->ptr &&
				(info->ptr->token || info->ptr->len))
					/* NULL sender OK */
			{
				if (rfc822check(517, info, 0) == 0)
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
		void (*delfunc)(struct rw_info *, const struct rfc822token *,
			const struct rfc822token *))
{
struct rfc822token *p;
char	*c;
struct	rfc822token host, addr;

	for (p=rwi->ptr; p; p=p->next)
	{
		if (p->token == '!')
		{
			(*nextfunc)(rwi);	/* We don't talk UUCP */
			return;
		}
		if (p->token == '@')	break;
	}

	if (!p)
	{
		(*nextfunc)(rwi);
		return;
	}

	if (configt_islocal(p->next, 0))
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

	c=rfc822_gettok(rwi->ptr);
	if (!c)	clog_msg_errno();
	domainlower(c);
	host.next=0;
	host.token=0;
	host.ptr=strchr(c, '@')+1;
	host.len=strlen(host.ptr);
	addr.next=0;
	addr.token=0;
	addr.ptr=c;
	addr.len=strlen(c);

	(*delfunc)(rwi, &host, &addr);
	free(c);
}


/************************************************************************/
/*               Should we accept mail for this domain?                 */
/************************************************************************/

static int acceptdomain(const struct rfc822token *t)
{
char	*address=rfc822_gettok(t);
int	rc;

static const char *acceptdomains=0;
static struct dbobj acceptdomainsdb;

	if (!address)	clog_msg_errno();

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

	rc=config_is_indomain(address, acceptdomains);
	if (rc == 0 && dbobj_isopen(&acceptdomainsdb))
		rc=isindomaindb(address, &acceptdomainsdb);

	free(address);
	return (rc);
}

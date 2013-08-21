/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
#include	"rwint.h"
#include	<stdio.h>
#include	<ctype.h>
#include	<string.h>
#include	<stdlib.h>

void rw_rewrite_msg_init(struct rwmsginfo *rwinfo,
	int (*writefunc)(const char *p, unsigned l, void *),
	void (*rewritefunc)(struct rw_info *,
		void (*)(struct rw_info *), void *),
	void *arg)
{
	memset(rwinfo, 0, sizeof(*rwinfo));

	rwinfo->inheaders=1;
	rwinfo->writefunc=writefunc;
	rwinfo->rewritefunc=rewritefunc;
	rwinfo->arg=arg;
}

int rw_rewrite_msg_finish(struct rwmsginfo *rwinfo)
{
int	j=0;

	while (rwinfo->inheaders)
	{
		if (rw_rewrite_msgheaders("\n", 1, rwinfo))
		{
			j= -1;
			break;
		}
	}

	if (rwinfo->headerbuf)
		free(rwinfo->headerbuf);
	return (j);
}

static void add2header(const char *, unsigned, struct rwmsginfo *);
static int dorewriteheader(struct rwmsginfo *);

int rw_rewrite_msgheaders(const char *p, unsigned l,
	struct rwmsginfo *msginfo)
{
	while (msginfo->inheaders)
	{
		if (l == 0)	return (0);

		while (l)
		{
		unsigned i;

			if (*p == '\n')
			{
				if (msginfo->lastnl)
				{
					if (dorewriteheader(msginfo))
						return (-1);
					msginfo->inheaders=0;
					break;
				}
				add2header(p, 1, msginfo);
				msginfo->lastnl=1;
				++p;
				--l;
				continue;
			}

			if (msginfo->lastnl)
			{
				if (!isspace((int)(unsigned char)*p))
					if (dorewriteheader(msginfo))
						return (-1);
				msginfo->lastnl=0;
			}

			for (i=0; i < l; i++)
				if (p[i] == '\n')
					break;
			add2header(p, i, msginfo);
			p += i;
			l -= i;
		}
	}

	return ((*msginfo->writefunc)(p, l, msginfo->arg));
}

static void add2header(const char *p, unsigned l, struct rwmsginfo *msginfo)
{
	if (l + msginfo->headerbuflen > msginfo->headerbufsize)
	{
	unsigned ll=(msginfo->headerbufsize + l + 1023) & ~1023;
	char	*buf=courier_malloc(ll);

		if (msginfo->headerbufsize)
			memcpy(buf, msginfo->headerbuf, msginfo->headerbufsize);
		if (msginfo->headerbuf)	free(msginfo->headerbuf);
		msginfo->headerbuf=buf;
		msginfo->headerbufsize=ll;
	}
	memcpy(msginfo->headerbuf+msginfo->headerbuflen, p, l);
	msginfo->headerbuflen += l;
}

static int dorewriteheader(struct rwmsginfo *msginfo)
{
	add2header("", 1, msginfo);

	if ( DO_REWRITE_HEADER(msginfo->headerbuf))
	{
	char *s, *t;
	char	*errmsgbuf=0;

		s=rw_rewrite_header_func( msginfo->rewritefunc,
                        msginfo->headerbuf,
                        RW_OUTPUT|RW_HEADER,
                        0,
                        &errmsgbuf,
			msginfo->arg);

		if (errmsgbuf)  free(errmsgbuf);
		if (s)
		{
			if ((*msginfo->writefunc)(s, strlen(s),
				msginfo->arg)) return (-1);
			t=strrchr(s, '\n');
			if (!t || t[1])
				if ((*msginfo->writefunc)("\n", 1,
					msginfo->arg)) return (-1);
			free(s);
			msginfo->headerbuflen=0;
			return (0);
		}
	}
	if ((*msginfo->writefunc)(msginfo->headerbuf, msginfo->headerbuflen-1,
				msginfo->arg))
		return (-1);
	msginfo->headerbuflen=0;
	return (0);
}

/*
** Copyright 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	<string.h>

void rw_local_defaulthost(struct rw_info *p, void (*func)(struct rw_info *))
{
struct rfc822token *q, **ptr;
struct rfc822token at, name;

	if (p->ptr && p->ptr->token == 0 && p->ptr->len == 0)
		p->ptr=p->ptr->next;

	if (p->ptr == 0)		/* Address is <> */
	{
		(*func)(p);
		return;
	}

	/*
	** Do not append the default domain to a UUCP address.
	*/

	for (q=p->ptr; q; q=q->next)
		if (q->token == '!')
		{
			(*func)(p);
			return;
		}

	for (q=*(ptr=&p->ptr); q; q= *(ptr= &q->next))
		if (q->token == '@')
		{
			(*func)(p);
			return;
		}

	*ptr= &at;

	at.next= &name;
	at.token= '@';
	at.ptr=0;
	at.len=0;

	name.next=0;
	name.token=0;
	name.ptr=config_defaultdomain();
	name.len=strlen(name.ptr);
	(*func)(p);
}

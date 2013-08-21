/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>


static char *rw_rewrite_header_common(struct rw_transport *rw,
	void (*rwfunc)(struct rw_info *, void (*)(struct rw_info *), void *),
	const char *header, int mode, struct rfc822token *sender,
	char **errmsgptr, void *funcarg)
{
struct rfc822t *rfcp;
struct rfc822a *rfca;
char	**bufptrs;
int	i;
char	*new_header=0;

	rfcp=rfc822t_alloc_new(header, NULL, NULL);
	rfca=rfc822a_alloc(rfcp);
	if (!rfca)	clog_msg_errno();

	bufptrs=0;
	if (rfca->naddrs && (bufptrs=malloc(sizeof(char *) * rfca->naddrs))
		== 0)
	{
		rfc822t_free(rfcp);
		rfc822a_free(rfca);
		clog_msg_errno();
	}

	*errmsgptr=0;
	for (i=0; i<rfca->naddrs; i++)
	{
	struct	rw_info_rewrite rwr;
	struct	rw_info rwi;
	struct rfc822token *tokenp;

		if (rfca->addrs[i].tokens == NULL)
		{
			bufptrs[i]=0;
			continue;
		}

		rwr.buf=0;
		rwr.errmsg=0;

		rw_info_init(&rwi, rfca->addrs[i].tokens, rw_err_func);
		rwi.mode=mode;
		rwi.sender=sender;
		rwi.udata=(void *)&rwr;

		/*
		** If address is of the form "@", this is a delivery
		** status notification, so leave it untouched.
		*/

		if (rwi.ptr && rwi.ptr->token == '@' &&
			rwi.ptr->next == 0)
			rw_rewrite_print(&rwi);

		/* --- */

		else if (rw)
			rw_rewrite_module(rw, &rwi,
				rw_rewrite_print
			);
		else
			(*rwfunc)(&rwi,
				rw_rewrite_print
				, funcarg
				);

		if ( (*errmsgptr=((struct rw_info_rewrite *)rwi.udata)->errmsg)
			!= 0)	break;

		if ( (bufptrs[i]=((struct rw_info_rewrite *)rwi.udata)->buf)
			== 0)	continue;

		tokenp=rfca->addrs[i].tokens;

		tokenp->next=0;
		tokenp->token=0;
		tokenp->ptr=bufptrs[i];
		tokenp->len=strlen(tokenp->ptr);
	}

	new_header=0;

	if ( !*errmsgptr)
	{
	unsigned i, l;
	char	*p;

		new_header=rfc822_getaddrs_wrap(rfca, 70);
		if (!new_header)	clog_msg_errno();

		for (i=l=0; new_header[i]; i++)
			if (new_header[i] == '\n' && new_header[i+1])
				l += 2;
		p=courier_malloc(strlen(new_header)+1+l);
		for (i=l=0; new_header[i]; i++)
		{
			p[l++]=new_header[i];
			if (new_header[i] == '\n' && new_header[i+1])
			{
				p[l++]=' ';
				p[l++]=' ';
			}
		}
		p[l]=0;
		free(new_header);
		new_header=p;
	}

	for (i=0; i<rfca->naddrs; i++)
		if (bufptrs[i])	free(bufptrs[i]);

	rfc822a_free(rfca);
	rfc822t_free(rfcp);
	if (bufptrs)	free(bufptrs);

	return (new_header);
}

char	*rw_rewrite_header(struct rw_transport *rw,
	const char *header, int mode, struct rfc822token *sender,
	char **errmsgptr)
{
	return (rw_rewrite_header_common(rw, 0,
		header, mode, sender, errmsgptr, 0));
}

char	*rw_rewrite_header_func(void (*rwfunc)(
			struct rw_info *, void (*)(struct rw_info *), void *),
	const char *header, int mode, struct rfc822token *sender,
	char **errmsgptr, void *funcarg)
{
	return (rw_rewrite_header_common(0, rwfunc,
		header, mode, sender, errmsgptr, funcarg));
}

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


static char *rw_rewrite_header_common(
	struct rw_transport *rw,
	void (*rwfunc)(struct rw_info *, void (*)(struct rw_info *), void *),
	const std::string_view &header, int mode, const rfc822::tokens &sender,
	char **errmsgptr, void *funcarg)
{
	std::vector<char *> bufptrs;
	char	*new_header=0;

	rfc822::tokens rfcp{header};
	rfc822::addresses rfca{rfcp};

	bufptrs.reserve(rfca.size());

	*errmsgptr=0;

	for (auto &a:rfca)
	{
		struct	rw_info_rewrite rwr;
		struct	rw_info rwi{mode, sender, {}};

		if (a.address.empty())
			continue;

		rwr.buf=0;
		rwr.errmsg=0;

		rw_info_init(&rwi, a.address, rw_err_func);
		rwi.udata=(void *)&rwr;

		/*
		** If address is of the form "@", this is a delivery
		** status notification, so leave it untouched.
		*/

		if (rwi.addr.size() == 1 && rwi.addr[0].type == '@')
			rw_rewrite_print(&rwi);

		/* --- */

		else if (rw)
			rw_rewrite_module(rw, &rwi,
				rw_rewrite_print
			);
		else
			(*rwfunc)(&rwi,
				rw_rewrite_print, funcarg
			);

		if ( (*errmsgptr=((struct rw_info_rewrite *)rwi.udata)->errmsg)
			!= 0)	break;

		auto bufptr=((struct rw_info_rewrite *)rwi.udata)->buf;

		if (!bufptr)
			continue;

		bufptrs.push_back(bufptr);

		a.address.clear();

		a.address.push_back( {0, bufptr} );
	}

	new_header=0;

	if ( !*errmsgptr)
	{
		std::string wrapped_header;

		std::string prefix="";

		auto collect = [&]
			(std::string l)
		{
			while (!l.empty() && isspace(l.back()))
				l.pop_back();
			wrapped_header += prefix;
			wrapped_header += std::move(l);
			prefix="\n  ";
		};

		rfca.print_wrapped(rfca.begin(), rfca.end(), 70, collect);

		new_header=strdup(wrapped_header.c_str());
	}

	for (auto &b:bufptrs)
		if (b) free(b);

	return (new_header);
}

char	*rw_rewrite_header(
	struct rw_transport *rw,
	const char *header, int mode, const rfc822::tokens &sender,
	char **errmsgptr)
{
	return (rw_rewrite_header_common(rw, 0,
		header, mode, sender, errmsgptr, 0));
}

char	*rw_rewrite_header_func(
	void (*rwfunc)(struct rw_info *, void (*)(struct rw_info *), void *),
	const char *header, int mode,
	const rfc822::tokens &sender,
	char **errmsgptr, void *funcarg)
{
	return (rw_rewrite_header_common(0, rwfunc,
		header, mode, sender, errmsgptr, funcarg));
}

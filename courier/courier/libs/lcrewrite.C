/*
** Copyright 1998 - 2002 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>


/*
**    Address rewriting rules.
*/

	/* Tokenize text for RFC822 parsing */

rfc822::tokens rw_rewrite_tokenize(const char *address)
{
	return rfc822::tokens{address};
}

	/* Call a transport module's rewrite function */

void rw_rewrite_module(struct rw_transport *module,
	struct rw_info *rwi, void (*func)(struct rw_info *))
{
	if (!module || !module->rw_ptr)
		(*rwi->err_func)(450, "Internal failure.", rwi);
	else if (module->rw_ptr->rewrite_del)
		(*module->rw_ptr->rewrite)(rwi, func);
	else
		(*rwi->err_func)(550, "Invalid address.", rwi);
}

int rw_syntaxchk(const rfc822::tokens &t)
{
int	cnt;

	/*
	** The following tokens:  @ : . may NOT:
	**
	** a) appear next to each other, and b) appear at the beginning or
	** the end of the address.
	**
	*/
	if (t.empty())	return (0);	/* <> envelope sender ok */

	cnt=1;

	for (auto &token:t)
	{
		switch (token.type)	{
		case '<':
		case '>':
		case ';':
		case '(':
		case ')':
			return (-1);	/* These cannot appear anywhere */
		case '@':
		case ':':
		case '.':
			if (++cnt > 1)	return (-1);
			break;
		case 0:
		case '"':
			for (unsigned char c:token.str)
			{
				if (isspace((int)c) || c < ' ')
					return (-1);
			}
		default:
			cnt=0;
			break;
		}
	}
	if (cnt)	return (-1);
	return (0);
}

	/* Common rewrite tail function to convert RFC822 tokens back into
	** a text string.
	*/
void rw_rewrite_print(struct rw_info *info)
{
	size_t l=info->addr.print( rfc822::length_counter{} );

	char *p=(char *)malloc(l+1);

	if (!p)		clog_msg_errno();

	char *save=p;

	info->addr.print(save);
	*save=0;
	( (struct rw_info_rewrite *)info->udata)->buf=p;
}

static void do_rw_rewrite_chksyn_print(struct rw_info *info,
				       const rfc822::tokens &t)
{
	if (rw_syntaxchk(t))
	{
		static const char errmsg[]="Syntax error: ";

		size_t l=info->addr.print( rfc822::length_counter{} );

		std::string s;

		s.reserve(l+sizeof(errmsg));

		s += errmsg;

		info->addr.print(std::back_inserter(s));

		(*info->err_func)(553, s.c_str(), info);
	}
	else
		rw_rewrite_print(info);
}

void rw_rewrite_chksyn_print(struct rw_info *info)
{
	do_rw_rewrite_chksyn_print(info, info->addr);
}

void rw_rewrite_chksyn_at_ok_print(struct rw_info *info)
{
	auto b=info->addr.begin(), e=info->addr.end();

	if (b != e && b->type == '@')
		++b;

	do_rw_rewrite_chksyn_print(info, {b, e});
}

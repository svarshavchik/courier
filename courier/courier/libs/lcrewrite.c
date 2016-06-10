/*
** Copyright 1998 - 2002 Double Precision, Inc.
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

struct rfc822t *rw_rewrite_tokenize(const char *address)
{
struct rfc822t *tokens=rfc822t_alloc_new(address, NULL, NULL);
int	i;

	if (!tokens)	clog_msg_errno();
	for (i=1; i<tokens->ntokens; i++)
		tokens->tokens[i-1].next=tokens->tokens+i;
	return (tokens);
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

int rw_syntaxchk(struct rfc822token *t)
{
int	cnt;

	/*
	** The following tokens:  @ : . may NOT:
	**
	** a) appear next to each other, and b) appear at the beginning or
	** the end of the address.
	**
	*/
	if (!t)	return (0);	/* <> envelope sender ok */

	cnt=1;

	while (t)
	{
		switch (t->token)	{
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
			{
				int i;

				for (i=0; i<t->len; i++)
				{
					unsigned char c=t->ptr[i];

					if (isspace((int)c) || c < ' ')
						return (-1);
				}
			}
		default:
			cnt=0;
			break;
		}
		t=t->next;
	}
	if (cnt)	return (-1);
	return (0);
}

	/* Common rewrite tail function to convert RFC822 tokens back into
	** a text string.
	*/
void rw_rewrite_print(struct rw_info *info)
{
char	*p=rfc822_gettok(info->ptr);

	if (!p)		clog_msg_errno();

	( (struct rw_info_rewrite *)info->udata)->buf=p;
}

static void do_rw_rewrite_chksyn_print(struct rw_info *info,
				       struct rfc822token *t)
{
	if (rw_syntaxchk(t))
	{
	static const char errmsg[]="Syntax error: ";
	char	*addr=rfc822_gettok(info->ptr);
	char	*buf;

		if (!addr)	clog_msg_errno();
		buf=courier_malloc(strlen(addr)+sizeof(errmsg));
		strcat(strcpy(buf, errmsg), addr);
		free(addr);
		(*info->err_func)(553, buf, info);
		free(buf);
	}
	else
		rw_rewrite_print(info);
}

void rw_rewrite_chksyn_print(struct rw_info *info)
{
	do_rw_rewrite_chksyn_print(info, info->ptr);
}

void rw_rewrite_chksyn_at_ok_print(struct rw_info *info)
{
	struct rfc822token *t=info->ptr;

	if (t && t->token == '@')
		t=t->next;

	do_rw_rewrite_chksyn_print(info, t);
}

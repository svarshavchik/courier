/*
** Copyright 2002 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	"comfax.h"
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

#define VALIDCHAR(c) strchr("0123456789ABCD#+*-,W", toupper((unsigned char)(c)))

static void rw_fax(struct rw_info *, void (*)(struct rw_info *));
static void rw_del_fax(struct rw_info *, void (*)(struct rw_info *),
		void (*)(struct rw_info *, const struct rfc822token *,
			const struct rfc822token *));

struct rw_list *fax_rw_install(const struct rw_install_info *p)
{
static struct rw_list fax_info={0, "module.fax - " COURIER_COPYRIGHT,
				rw_fax, rw_del_fax, 0};

	return (&fax_info);
}

const char *fax_rw_init()
{
	return (0);
}

static int is_fax(struct rfc822token **headptr,
		  struct rfc822token ***atptr,
		  struct rfc822token *faxhostt)
{
	struct rfc822token **p=NULL, *q;
	int dummy;

	for ( ; *headptr; headptr=&(*headptr)->next)
		if ( (*headptr)->token == '@')
			p=headptr;

	if (!p)
	{
		return (0);
	}

	q= (*p)->next;

	if (!q || q->next || q->token != 0)
		return (0);

	if (comgetfaxoptsn(q->ptr, q->len, &dummy) == 0)
	{
		*faxhostt= *q;
		*atptr=p;
		return (1);
	}

	return (0);
}


static void rw_fax(struct rw_info *p, void (*func)(struct rw_info *))
{
	struct rfc822token **dummy, dummy2;

	if (is_fax(&p->ptr, &dummy, &dummy2))
		return;

	(*func)(p);
}

#define	MAXLEN 80

#define MATCH_PREFIX 1
#define MATCH_REPEAT 2

/*
** Poor man's pattern matcher.
*/

static int matches(const char *num, const char *pat, int *match_buf,
		   int flags)
{
	int p;

	int l=strlen(num);
	int i;

	int next_string_num=0;
	int in_string=0;

	for (i=0; i<l; i++)
		match_buf[i]= -1;

	p=0;

	while (*pat)
	{
		int c;

		if (*pat == '(')
		{
			if (in_string)
				return (0);	/* Nested ( are N/A */

			++in_string;
			++next_string_num;
			++pat;
			continue;
		}

		if (*pat == ')' && in_string > 0)
		{
			--in_string;
			++pat;
			continue;
		}

		if (!num[p])
			return (0);

		c=toupper((unsigned char)*pat);

		if (c == 'N')
		{
			if (!isdigit((int)(unsigned char)num[p]))
				return (0);
		}
		else if (c != '.')
			if (toupper((int)(unsigned char)num[p]) != c)
				return (0);

		if (in_string)
			match_buf[p]=next_string_num;
		++pat;
		++p;
	}

	if (num[p])
	{
		if (!(flags & MATCH_PREFIX))
			return (0);

		while (num[p])
		{
			match_buf[p]=9;
			++p;
		}
	}
	return (1);
}

static char *rwfax(const char *a, const char *module, char *buffer)
{
	static char *faxrc=0, *faxrcbuf;
	char *p;
	int matched_strings[MAXLEN];
	char *nextl;

	int accepted=0;

	if (!module)
		accepted=1;	/* Not called from SUBMIT, that's OK */

	/* The first time up, read the config file */

	if (!faxrc)
	{
		struct stat stat_buf;
		char *n=config_localfilename("faxrc");
		FILE *f=fopen(n, "r");

		free(n);

		if (!f)
			return (NULL);

		if (fstat(fileno(f), &stat_buf) < 0
		    || fread ( (faxrc=courier_malloc(stat_buf.st_size+1)),
			       stat_buf.st_size, 1, f) != 1)
		{
			clog_msg_prerrno();
			fclose(f);
			return (NULL);
		}
		faxrc[stat_buf.st_size] = 0;
		fclose(f);
		faxrcbuf=courier_malloc(stat_buf.st_size+1);
	}

	if (strlen(a) > MAXLEN)
		return (NULL);

	strcpy(buffer, a);

	/*
	** DTMF '#' conflicts with the comment character in faxrc.
	** Temporarily replace all #s by ~s, then restore them later.
	**
	** Convert all the dashes to ,s, pluses to @s
	*/
	for (p=buffer; *p; p++)
	{
		if (*p == '#')
			*p='~';

		if (*p == '-')
			*p=',';

		if (*p == '+')
			*p='@';
	}

	strcpy(faxrcbuf, faxrc);

	for (p=nextl=faxrcbuf; *nextl; )
	{
		char *q;

		p=nextl;

		for (nextl=p; *nextl; nextl++)
		{
			if (*nextl == '\n')
			{
				*nextl++ = 0;
				break;
			}
		}

		if ((q=strchr(p, '#')) != 0) *q=0;

		q=strtok(p, " \t\r");
		if (!q)
			continue;

		if (strncmp(q, "rw", 2) == 0)
		{
			int flags=0;
			int cnt=0;

			for (q += 2; *q; q++)
			{
				if (*q == '^')
					flags |= MATCH_PREFIX;
				else if (*q == '*')
					flags |= MATCH_REPEAT;
			}

			q=strtok(NULL, " \t\r");
			if (!q)
				continue;

			while (matches(buffer, q, matched_strings, flags))
			{
				char new_buffer[MAXLEN+1];
				int i=0;

				/* Build the replacement string */

				q=strtok(NULL, " \t\r");
				while (q && *q)
				{
					int n;
					int j;

					if (*q != '$')
					{
						if (i >= MAXLEN)
							return (NULL);
						new_buffer[i++] = *q;
						++q;
						continue;
					}
					if (!*++q)
						continue;

					n= *q - '0';
					if (n < 0 || n > 9)
					{
						++q;
						continue;
					}

					for (j=0; j<strlen(buffer); j++)
						if (matched_strings[j]  == n)
						{
							if (i >= MAXLEN)
								return(NULL);
							new_buffer[i++]=
								buffer[j];
						}
					++q;
				}

				new_buffer[i]=0;
				strcpy(buffer, new_buffer);

				if (!(flags & MATCH_REPEAT))
					break;

				if (++cnt > MAXLEN+10)
					break;
			}
		}
		else if (strncmp(q, "accept", 6) == 0)
		{
			int flags=0;

			for (q += 6; *q; q++)
			{
				if (*q == '^')
					flags |= MATCH_PREFIX;
			}

			q=strtok(NULL, " \t\r");

			if (!q)
				continue;

			if (!matches(buffer, q, matched_strings, flags))
				continue;

			q=strtok(NULL, " \t\r");

			if (q && module && strcmp(q, module) == 0)
				accepted=1;
			continue;
		}
		else if (strncmp(q, "reject", 6) == 0)
		{
			int flags=0;

			for (q += 6; *q; q++)
			{
				if (*q == '^')
					flags |= MATCH_PREFIX;
			}

			q=strtok(NULL, " \t\r");

			if (!q)
				continue;

			if (matches(buffer, q, matched_strings, flags))
			{
				q=strtok(NULL, " \t\r");

				if (q && module && strcmp(q, module) == 0)
					return (0);
			}
		}
	}

	for (p=buffer; *p; p++)
		if (*p == '~')
			*p='#';

	if (!accepted)
		return (NULL);

	return (buffer);
}

static void rw_del_fax(struct rw_info *rwi,
		       void (*nextfunc)(struct rw_info *),
		       void (*delfunc)(struct rw_info *,
				       const struct rfc822token *,
				       const struct rfc822token *))
{
	struct rfc822token **p;
	char *a, *b, *c;
	struct rfc822token t, ht;
	char curnum[MAXLEN+1];

	if (!is_fax(&rwi->ptr, &p, &ht))
	{
		(*nextfunc)(rwi);
		return;
	}

	*p=0;

	a=rfc822_gettok(rwi->ptr);

	if (!a)
	{
		clog_msg_errno();
		(*rwi->err_func)(550, "Out of memory", rwi);
		return;
	}

	for (b=c=a; *b; b++)
	{
		if (VALIDCHAR(*b))
			*c++ = *b;
	}
	*c=0;
	if (*a == 0)
	{
		free(a);
		(*rwi->err_func)(550, "Invalid fax phone number", rwi);
		return;
	}

	b=rwfax(a, rwi->mode & RW_SUBMIT ? rwi->smodule:NULL, curnum);
	free(a);

	if (!b || !*b)
	{
		(*rwi->err_func)(550, "Invalid fax phone number", rwi);
		return;
	}

	t.token=0;
	t.next=0;
	t.ptr=b;
	t.len=strlen(b);

	(*delfunc)(rwi, &ht, &t);
}

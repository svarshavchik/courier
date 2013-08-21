/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"msghash.h"
#include	<ctype.h>
#include	<string.h>


/*
		Calculate a hash of a message

Duplicate message detection works by calculating a hash on the message's
contents.  We attempt to calculate a hash in such a way as to detect some
simple attempts at hashbusting.

Obviously, this is not going to work for very long.  If this filter becomes
popular, it would become trivially easy to write a hasbuster around it.
Consider this just as an example.

msghash_init initializes the msghashinfo structure.  Then, msghash_line
gets called to feed the message contents into the hash.  msghash_line()
is called to process each individual line in the message.

msghash_line will ignore all headers except subject, and will ignore
everything except letters, which will all be converted to lowercase.

Finally, msghash_finish is called to calculate the final hashes.  Two
hashes are calculated: the "top" and the "bottom" hash.  The first one
is a hash of everything except the first couple of lines.  The last one
is a hash of everything except the last couple of lines.  If the message
is too small, both hashes are identical, and the entire message is hashed.

*/

void msghash_init(struct msghashinfo *p)
{
	p->numlines=0;
	p->inheader=1;
	p->cur_header[0]=0;
	p->headerlinebuf_size=0;
	md5_context_init(&p->c_entire);
	md5_context_init(&p->c_top);
	md5_context_init(&p->c_bot);
	p->c_entire_cnt=0;
	p->c_top_cnt=0;
	p->c_bot_cnt=0;
	p->linebuf_head=0;
	p->linebuf_tail=0;
}

static void do_msghash_line(struct msghashinfo *p, const char *l)
{
unsigned long ll;

	if (*l == 0)	return;
	ll=strlen(l);

	if (p->numlines <= MSGHASH_HIMARGIN)
			/* No need to calc all anymore */
	{
		md5_context_hashstream(&p->c_entire, l, ll);
		p->c_entire_cnt += ll;
	}

	if (p->numlines++ < MSGHASH_MARGIN)
	{
		strcpy(p->linebuf[p->linebuf_head], l);
		p->linebuf_head= (p->linebuf_head+1) % MSGHASH_MARGIN;
	}
	else
	{
	const char *s=p->linebuf[p->linebuf_tail];
	unsigned long sl=strlen(s);

		md5_context_hashstream(&p->c_top, s, sl);
		p->c_top_cnt += sl;

		strcpy(p->linebuf[p->linebuf_head], l);
		p->linebuf_head= (p->linebuf_head+1) % MSGHASH_MARGIN;
		p->linebuf_tail= (p->linebuf_tail+1) % MSGHASH_MARGIN;

		md5_context_hashstream(&p->c_bot, l, ll);
		p->c_bot_cnt += ll;
	}
}

void msghash_line(struct msghashinfo *p, const char *l)
{
unsigned long ll;

	if (!p->inheader)
	{
		p->headerlinebuf_size=0;
		ll=0;
		while (*l)
		{
			if ( isalpha((int)(unsigned char)*l) &&
				ll < sizeof(p->headerlinebuf)-1)
				p->headerlinebuf[ll++] = *l;
			++l;
		}
		p->headerlinebuf[ll]=0;

		do_msghash_line(p, p->headerlinebuf);
		return;
	}

	if (*l == 0)
	{
		p->inheader=0;
		if (strcmp(p->cur_header, "subject") == 0)
			do_msghash_line(p, p->headerlinebuf);
			/* Count the subject header */
		return;
	}
	if (!isspace(*l))
	{
		if (strcmp(p->cur_header, "subject") == 0)
			do_msghash_line(p, p->headerlinebuf);
		for (ll=0; ll<sizeof(p->cur_header)-1; ll++)
		{
			if (l[ll] == ':')	break;
			p->cur_header[ll]=tolower( (int)(unsigned char)l[ll]);
		}
		p->cur_header[ll]=0;
		p->headerlinebuf_size=0;
	}
	while (*l)
	{
		if ( isalpha((int)(unsigned char)*l) &&
			p->headerlinebuf_size <
				sizeof(p->headerlinebuf)-1)
			p->headerlinebuf[p->headerlinebuf_size
					++] = *l;
		++l;
	}
	p->headerlinebuf[p->headerlinebuf_size]=0;
}

void msghash_finish(struct msghashinfo *p)
{
	if (p->numlines < MSGHASH_HIMARGIN)
	{
		memcpy(&p->c_top, &p->c_entire, sizeof(p->c_top));
		p->c_top_cnt=p->c_entire_cnt;

		memcpy(&p->c_bot, &p->c_entire, sizeof(p->c_bot));
		p->c_bot_cnt=p->c_entire_cnt;
	}

	md5_context_endstream(&p->c_top, p->c_top_cnt);
	md5_context_endstream(&p->c_bot, p->c_bot_cnt);

	md5_context_digest(&p->c_top, p->md1);
	md5_context_digest(&p->c_bot, p->md2);
}

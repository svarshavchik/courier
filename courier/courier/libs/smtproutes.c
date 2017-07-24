/*
** Copyright 1998 - 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	"smtproutes.h"
#include	"dbobj.h"
#include	<string.h>
#include	<stdlib.h>
#include	<ctype.h>


static char *get_control_smtproutes()
{
char *filename=config_localfilename("esmtproutes");
char *buf=readfile(filename, 0);

	free(filename);
	if (!buf)	return (0);

	removecomments(buf);
	return (buf);
}

static char *fetch_smtproutes(const char *domain)
{
char	*buf=get_control_smtproutes();
const char *p=buf;

	if (!buf)
	{
	struct dbobj d;
	char	*p, *q;
	size_t	l;

		p=config_localfilename("esmtproutes.dat");

		dbobj_init(&d);
		if (dbobj_open(&d, p, "R"))
		{
			free(p);
			return (0);
		}
		free(p);
		p=strcpy(courier_malloc(strlen(domain)+1), domain);
		for (q=p; *q; q++)
			*q=tolower(*q);
		q=dbobj_fetch(&d, p, strlen(p), &l, "D");
		free(p);
		dbobj_close(&d);
		if (!q)	return (0);
		p=courier_malloc(l+1);
		memcpy(p, q, l);
		p[l]=0;
		free(q);
		return (p);
	}

	while (*p)
	{
	unsigned i;

		for (i=0; p[i] && p[i] != '\n' && p[i] != '\r' && p[i] != ':';
			++i)
			;

		if (p[i] == ':' && (i == 0 ||
			config_domaincmp(domain, p, i) == 0))
		{
		char *q;

			p += i;
			++p;
			for (i=0; p[i] && p[i] != '\n' && p[i] != '\r'; i++)
				;

			while (i && isspace((int)(unsigned char)p[i-1]))
				--i;
			while (i && isspace((int)(unsigned char)*p))
			{
				++p;
				--i;
			}
			if (i == 0)
			{
				free(buf);
				return (0);
			}

			q=courier_malloc(i+1);
			memcpy(q, p, i);
			q[i]=0;
			free(buf);
			return (q);
		}

		while (p[i])
		{
			if (p[i] == '\n' || p[i] == '\r')
			{
				++i;
				break;
			}
			++i;
		}
		p += i;
	}

	free(buf);
	return (0);
}

char *smtproutes(const char *domain, int *flags)
{
	char *p, *q, *r;

	*flags=0;

	p=fetch_smtproutes(domain);

	if (!p)
		return (0);

	if ((q=strrchr(p, '/')) != 0)
	{
		for (*q++=0; (q=strtok(q, ", \r\t\n")) != NULL; q=0)
		{
			char *r;

			for (r=q; *r; r++)
				*r=(int)(unsigned char)toupper(*r);

			if (strcmp(q, "SECURITY=STARTTLS") == 0)
				*flags |= ROUTE_STARTTLS;

			if (strcmp(q, "SECURITY=REQUIRED") == 0)
				*flags |= ROUTE_TLS_REQUIRED;

			if (strcmp(q, "SECURITY=SMTPS") == 0)
				*flags |= ROUTE_SMTPS;
		}
	}

	/* Trim trailing spaces */

	for (q=r=p; *q; q++)
	{
		if (!isspace((int)(unsigned char)*q))
		{
			r=q+1;
		}
	}
	*r=0;

	if (!*p)
	{
		free(p);
		p=0;
	}
	return (p);
}

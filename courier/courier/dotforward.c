/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"courier.h"
#include	"rfc822/rfc822.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<sysexits.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif


struct delivered_to {
	struct delivered_to *next;
	char *addr;
	} *delivtolist=0;
char *myaddr;
static int exit_code;

/*
** Extract and save all addresses in the Delivered-To: header.
*/

static void initdelivto()
{
char	buf[BUFSIZ];
char	*p, *r;
struct delivered_to *q;

	p=getenv("DTLINE");
	if (!p || !(p=strchr(p, ':')))	exit(0);
	++p;
	while (*p && isspace((int)(unsigned char)*p))	++p;
	myaddr=strdup(p);
	if (!myaddr)
	{
		perror("malloc");
		exit(EX_TEMPFAIL);
	}
	domainlower(myaddr);
	locallower(myaddr);
	if (strchr(myaddr, '@') == 0)
	{
		fprintf(stderr, "Invalid DTLINE environment variable.\n");
		exit(EX_TEMPFAIL);
	}

	while (fgets(buf, sizeof(buf), stdin))
	{
		p=strchr(buf, '\n');
		if (p)	*p=0;
		if (strncasecmp(buf, "Delivered-To:", 13))	continue;
		p=buf+13;
		while (*p && isspace((int)(unsigned char)*p))	++p;
		q=malloc(sizeof(*q)+1+strlen(p));
		if (!q)
		{
			perror("malloc");
			exit(EX_TEMPFAIL);
		}
		strcpy(q->addr=(char *)(q+1), p);
		q->next=delivtolist;
		delivtolist=q;
		domainlower(q->addr);
		r=strchr(q->addr, '@');
		if (!r || config_islocal(r+1, 0))
			locallower(q->addr);
	}
}

static void readforward(FILE *f, int n)
{
char	buf[BUFSIZ];
char	*p;
struct	rfc822t *t;
struct	rfc822a *a;
int	i;
char	*sep;

	while (fgets(buf, sizeof(buf), f))
	{
		p=strchr(buf, '\n');
		if (p)	*p=0;
		p=buf;
		while (*p && isspace((int)(unsigned char)*p))	++p;
		if (strncmp(p, ":include:",  9) == 0)
		{
		FILE	*g;

			if (n > 10)
			{
				fprintf(stderr, "dotforward: too many :include files.\n");
				exit(EX_NOUSER);
			}

			p += 9;
			while (*p && isspace((int)(unsigned char)*p))	++p;
			if (!*p)	continue;
			g=fopen(p, "r");
			if (!g)
			{
				perror(p);
				exit(EX_NOUSER);
			}
			readforward(g, n+1);
			fclose(g);
			continue;
		}
		if (*p == '|' || *p == '/' || *p == '.')
		{
			printf("%s\n", p);
			continue;
		}
		t=rfc822t_alloc_new(p, NULL, NULL);
		if (!t || !(a=rfc822a_alloc(t)))
		{
			perror("malloc");
			exit(EX_NOUSER);
		}

		for (i=0; i<a->naddrs; i++)
		{
			if (a->addrs[i].tokens &&
			    a->addrs[i].tokens->token == '"' &&
			    a->addrs[i].tokens->next == NULL)
				a->addrs[i].tokens->token=0;

			p=rfc822_getaddr(a, i);
			if (!p)
			{
				perror("malloc");
				exit(EX_NOUSER);
			}
			if (*p == '|' || *p == '/')
			{
				printf("%s\n", p);
			}
			free(p);
		}
		sep=0;
		for (i=0; i<a->naddrs; i++)
		{
		char	*q, *r;
		struct delivered_to *s;
		char	*t;
		char	*orig;

			p=rfc822_getaddr(a, i);
			if (!p)
			{
				perror("malloc");
				exit(EX_NOUSER);
			}
			if (*p == '|' || *p == '/' || *p == '.')
			{
				free(p);
				continue;
			}
			q=p;
			if (*q == '\\')
				++q;

			r=strchr(q, '@');
			if (!r || config_islocal(r+1, 0))
				locallower(q);
			domainlower(q);
			t=0;
			orig=q;

			if (strchr(q, '@') == 0)
			{
				t=malloc(strlen(q)+1+strlen(myaddr));
					/* overkill, yeah */
				if (!t)
				{
					perror("malloc");
					exit(EX_NOUSER);
				}
				strcat(strcpy(t, q), strchr(myaddr, '@'));
				q=t;
			}

			if (strcmp(myaddr, q) == 0)
			{
				exit_code=0;
				free(p);
				if (t)	free(t);
				continue;
			}

			for (s=delivtolist; s; s=s->next)
			{
				if (strcmp(s->addr, q) == 0)
					break;
			}
			if (!s)
			{
				if (sep)	printf("%s", sep);
				else	printf("!");
				sep=", ";
				printf("%s", orig);
			}
			free(p);
			if (t)	free(t);
		}
		if (sep)	printf("\n");
		rfc822a_free(a);
		rfc822t_free(t);
	}
}

int main(int argc, char **argv)
{
char	*homedir;
FILE	*f;

	homedir=getenv("HOME");
	if (!homedir)	exit(0);
	if (chdir(homedir))
	{
		perror(homedir);
		exit(EX_TEMPFAIL);
	}
	if ((f=fopen(".forward", "r")) == 0)
		exit (0);
	initdelivto();
	exit_code=99;
	readforward(f, 0);
	exit(exit_code);
}

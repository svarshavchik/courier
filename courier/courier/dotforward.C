/*
** Copyright 1998 - 2000 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"courier.h"
#include	"rfc822/rfc822.h"
#include	"rfc822/rfc2047.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<sysexits.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<unordered_set>
#include	<string>
#include	<algorithm>

std::unordered_set<std::string> delivtolist;
char *myaddr;
static int exit_code;

/*
** Extract and save all addresses in the Delivered-To: header.
*/

static void initdelivto()
{
char	buf[BUFSIZ];
char	*p, *r, *s;
int inheader=1;

	p=getenv("DTLINE");
	if (!p || !(p=strchr(p, ':')))	exit(0);
	++p;
	while (*p && isspace((int)(unsigned char)*p))	++p;

	r=udomainutf8(p);
	myaddr=ulocallower(r);
	free(r);

	if (strchr(myaddr, '@') == 0)
	{
		fprintf(stderr, "Invalid DTLINE environment variable.\n");
		exit(EX_TEMPFAIL);
	}

	while (fgets(buf, sizeof(buf), stdin))
	{
		p=strchr(buf, '\n');
		if (p)	*p=0;
		if (buf[0] == 0)
			inheader=0;
		if (!inheader)
			continue;
		if (strncasecmp(buf, "Delivered-To:", 13))	continue;
		p=buf+13;
		while (*p && isspace((int)(unsigned char)*p))	++p;

		s=udomainutf8(p);
		r=strchr(s, '@');
		if (!r || config_islocal(r+1, 0))
		{
			char *ss=ulocallower(s);

			free(s);
			s=ss;
		}

		delivtolist.insert(s);
		free(s);
	}
}

static void readforward(FILE *f, int n)
{
char	buf[BUFSIZ];
char	*p;
const char	*sep;

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
		rfc822::tokens t{p};
		rfc822::addresses a{t};

		for (auto &addr:a)
		{
			std::string p;

			addr.address.unquote(
				std::back_inserter(p)
			);

			if (p.empty())
				continue;

			switch (p[0]) {
			case '|':
			case '/':
			case '.':
				printf("%s\n", p.c_str());
			}
		}
		sep=0;
		for (auto &addr:a)
		{
			std::string p;

			addr.address.unquote(
				std::back_inserter(p)
			);

			if (p.empty())
				continue;

			switch (p[0]) {
			case '|':
			case '/':
			case '.':
				continue;
			}

			std::string q;

			addr.address.display_address(
				unicode::utf_8,
				std::back_inserter(q)
			);

			auto r=strchr(q.data(), '@');

			if (!r || config_islocal(r+1, 0))
			{
				auto ptr=ulocallower(q.data());
				q=ptr;
				free(ptr);
			}

			std::string orig=q;

			if (strchr(q.data(), '@') == 0)
			{
				q += strchr(myaddr, '@');
			}

			if (q == myaddr)
			{
				exit_code=0;
				continue;
			}

			if (delivtolist.find(q) == delivtolist.end())
			{
				if (sep)	printf("%s", sep);
				else	printf("!");
				sep=", ";
				printf("%s", orig.c_str());
			}
		}
		if (sep)	printf("\n");
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

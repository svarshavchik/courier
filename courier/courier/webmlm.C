/*
** Copyright 2007 S. Varshavchik.  See COPYING for
** distribution information.
**
*/


#include	"config.h"
#include	"sysconfdir.h"
#include	<iostream>
#include	<fstream>
#include	<algorithm>
#include	<string>
#include	<string_view>
#include	<stdio.h>
#include	<string.h>
#include	<strings.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<courier-unicode.h>
#include	"cgi/cgi.h"


int main(int argc, char **argv)
{
	const char *envdir=getenv("WEBMLMRC_DIR");
	const char *p=strrchr(argv[0], '/');

	if (!envdir || !*envdir)
		envdir= SYSCONFDIR;

	if (p == NULL) p=argv[0];
	else ++p;

	std::string q;

	q.reserve(strlen(envdir)+strlen(p)+10);

	q=envdir;
	q += "/";
	q += p;
	q += "rc";

	std::ifstream fp{q};

	if (!fp)
	{
		cginocache();

		printf("Content-Type: text/html; charset=utf-8\n\n"
		       "<html><head><title>Internal error</title></head>\n"
		       "<body><h1>Internal Error</h1>\n"
		       "<p>The webmail system is temporarily unavailable.  Missing configuration file: %s</p></body></html>\n", q.c_str());
		fflush(stdout);
		exit(0);
	}

	for (std::string buf; std::getline(fp, buf); )
	{
		std::string_view w{buf};

		size_t n=w.find_first_not_of(" \t\r");

		if (n == w.npos)
			continue;

		w.remove_prefix(n);

		if (w.size() < 5 ||
		    !std::equal(w.data(), w.data()+5, "PORT=",
				[]
				(unsigned char a, unsigned char b)
				{
					return unicode_uc(a) == unicode_uc(b);
				}))
			continue;

		w.remove_prefix(5);

		n=w.find_first_of(" \t\r");
		if (n == w.npos)
			n=w.size();

		if (n)
		{
			fp.close();
			cgi_connectdaemon(std::string{w.substr(0, n)}.c_str(),
					  CGI_PASSFD);
			return (0);
		}
		break;
	}
	fp.close();

	cginocache();

	printf("Content-Type: text/html; charset=utf-8\n\n"
	       "<html><head><title>Internal error</title></head>\n"
	       "<body><h1>Internal Error</h1>\n"
	       "<p>The webmail system is temporarily unavailable.  Missing PORT setting in %s</p></body></html>\n", q.c_str());
	fflush(stdout);
	exit(0);
}

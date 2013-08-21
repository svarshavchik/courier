/*
** Copyright 2007 Double Precision, Inc.  See COPYING for
** distribution information.
**
*/


#include	"config.h"
#include	"sysconfdir.h"
#include	<stdio.h>
#include	<string.h>
#include	<strings.h>
#include	<stdlib.h>
#include	<ctype.h>

#include	"cgi/cgi.h"


int main(int argc, char **argv)
{
	const char *envdir=getenv("WEBMLMRC_DIR");
	const char *p=strrchr(argv[0], '/');
	char *q;
	char	buf[1024];
	FILE *fp;

	if (!envdir || !*envdir)
		envdir= SYSCONFDIR;

	if (p == NULL) p=argv[0];
	else ++p;

	q=malloc(strlen(envdir)+strlen(p)+10);

	if (!q)
		exit(1);

	strcat(strcat(strcat(strcpy(q, envdir), "/"), p), "rc");

	fp=fopen(q, "r");

	if (!fp)
	{
		cginocache();

		printf("Content-Type: text/html; charset=utf-8\n\n"
		       "<html><head><title>Internal error</title></head>\n"
		       "<body><h1>Internal Error</h1>\n"
		       "<p>The webmail system is temporarily unavailable.  Missing configuration file: %s</p></body></html>\n", q);
		fflush(stdout);
		exit(0);
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		char *w=buf;

		while (*w)
		{
			if (!isspace((unsigned char)*w))
				break;

			++w;
		}

		if (strncasecmp(w, "PORT=", 5))
			continue;

		w += 5;

		w=strtok(w, " \t\r\n");

		if (w)
		{
			fclose(fp);
			cgi_connectdaemon(w, CGI_PASSFD);
			return (0);
		}
		break;
	}
	fclose(fp);

	cginocache();

	printf("Content-Type: text/html; charset=utf-8\n\n"
	       "<html><head><title>Internal error</title></head>\n"
	       "<body><h1>Internal Error</h1>\n"
	       "<p>The webmail system is temporarily unavailable.  Missing PORT setting in %s</p></body></html>\n", q);
	fflush(stdout);
	exit(0);
}

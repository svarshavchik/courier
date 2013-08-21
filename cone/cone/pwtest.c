/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tcpd/tlspasswordcache.h"

static int savefunc(const char *p, size_t n, void *vp)
{
	if (fwrite(p, n, 1, (FILE *)vp) != 1)
		return -1;

	return 0;
}

static int loadfunc(char *p, size_t n, void *vp)
{
	return (fread(p, 1, n, (FILE *)vp));
}

static void success()
{
	printf("pwtest: ok\n");
}

static void callback(const char * const *uids,
		     const char * const *pwds,
		     void *dummy)
{
	const char ***cp=(const char ***)dummy;
	const char **orig_uids=cp[0];
	const char **orig_pwds=cp[1];
	int i;

	for (i=0; orig_uids[i]; i++)
	{
		if (!uids[i] || !pwds[i] ||
		    strcmp(uids[i], orig_uids[i]) ||
		    strcmp(pwds[i], orig_pwds[i]))
			break;
	}

	if (orig_uids[i] || uids[i] || pwds[i])
		return;

	success();
}

int main()
{
	FILE *fp;

	const char *urls[3];
	const char *pwds[3];

	const char **urlpws[2];

	if (!tlspassword_init())
	{
		success();
		exit(0);
	}

	urls[0]="Mary had a little lamb, ";
	urls[1]="its fleece was white as snow, ";
	urls[2]=NULL;
	pwds[0]="and everywhere Mary went, ";
	pwds[1]="the lamb was sure to go.";
	pwds[2]=NULL;

	urlpws[0]=urls;
	urlpws[1]=pwds;

	fp=tmpfile();

	if (tlspassword_save(urls, pwds, "foobar", savefunc, fp))
	{
		perror("tlspassword_save");
		exit(1);
	}

	if (fflush(fp) || ferror(fp) || fseek(fp, 0L, SEEK_SET) < 0)
	{
		perror("test.dat");
		exit(1);
	}

	if (tlspassword_load(loadfunc, fp, "foobar", callback, urlpws))
	{
		perror("tlspassword_load");
		exit(1);
	}
	fclose(fp);

	exit(0);
	return (0);
}

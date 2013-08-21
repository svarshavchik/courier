/*
** Copyright 1998 - 1999 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<pwd.h>


static char buf[BUFSIZ];
static struct passwd pw;

struct passwd *fgetpwent(FILE *f)
{
char	*p;

	if (fgets(buf, sizeof(buf), f) == 0)	return (0);
	if ((p=strchr(buf, '\n')) != 0)	*p=0;
	else
	{
	int	c;

		while ((c=getc(f)) >= 0 && c != '\n')
			;
	}

	pw.pw_name=buf;
	if ((p=strchr(buf, ':')) != 0)
		*p++=0;

	pw.pw_passwd=p;
	if (p && (p=strchr(p, ':')) != 0)
		*p++=0;

	pw.pw_uid=atoi(p);
	if (p && (p=strchr(p, ':')) != 0)
		*p++=0;

	pw.pw_gid=atoi(p);
	if (p && (p=strchr(p, ':')) != 0)
		*p++=0;

	pw.pw_gecos=p;
	if (p && (p=strchr(p, ':')) != 0)
		*p++=0;

	pw.pw_dir=p;

	if (p && (p=strchr(p, ':')) != 0)
		*p++=0;
	pw.pw_shell=p;
	return (&pw);
}

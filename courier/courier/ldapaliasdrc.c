/*
** Copyright 2000-2003 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#if TIME_WITH_SYS_TIME
#include        <sys/time.h>
#include        <time.h>
#else
#if HAVE_SYS_TIME_H
#include        <sys/time.h>
#else
#include        <time.h>
#endif
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <sys/socket.h>
#include <sys/un.h>
#if HAVE_SYSLOG_H
#include <syslog.h>
#else
#define syslog(a,b)
#endif

#include	"ldapaliasdrc.h"
#include	"sysconfdir.h"
#include	"localstatedir.h"


static char *ldapauth=0;
static size_t ldapauth_size=0;

static int readconfigfile()
{
	FILE	*f=fopen(LDAPALIASDCONFIGFILE, "r");
	struct	stat	buf;
	size_t	i;
	if (!f)
	{
		if (errno != ENOENT)
			perror(LDAPALIASDCONFIGFILE);
		return (-1);
	}

	if (fstat(fileno(f), &buf) ||
	    (ldapauth=malloc(buf.st_size+2)) == 0)
	{
		fclose(f);
		return (-1);
	}
	if (fread(ldapauth, buf.st_size, 1, f) != 1)
	{
		free(ldapauth);
		ldapauth=0;
		fclose(f);
		return (-1);
	}
	ldapauth[ldapauth_size=buf.st_size]=0;

	for (i=0; i<ldapauth_size; i++)
		if (ldapauth[i] == '\n')
			ldapauth[i]=0;
	fclose(f);
	return (0);
}

void ldapaliasd_configchanged()
{
	if (ldapauth)
	{
		free(ldapauth);
		ldapauth=0;
		ldapauth_size=0;
	}
}

const char *ldapaliasd_config(const char *configname)
{
	size_t	i;
	int	l=strlen(configname);
	char	*p=0;

	if (!ldapauth && readconfigfile())
		return ("");

	for (i=0; i<ldapauth_size; )
	{
		p=ldapauth+i;
		if (memcmp(p, configname, l) == 0 &&
			isspace((int)(unsigned char)p[l]))
		{
			p += l;
			while (*p && *p != '\n' &&
				isspace((int)(unsigned char)*p))
				++p;
			return (p);
		}

		while (i < ldapauth_size)
			if (ldapauth[i++] == 0)	break;
	}

	return ("");
}

FILE *ldapaliasd_connect()
{
	int     fd=socket(PF_UNIX, SOCK_STREAM, 0);
	struct  sockaddr_un skun;
	FILE	*fp;

        if (fd < 0)
	{
		return (NULL);
	}

        skun.sun_family=AF_UNIX;
        strcpy(skun.sun_path, SOCKETFILE);

	if (connect(fd, (const struct sockaddr *)&skun, sizeof(skun)) < 0)
	{
		close(fd);
		return (NULL);
	}

	fp=fdopen(fd, "r+");

	if (!fp)
	{
		close(fd);
		syslog(LOG_DAEMON | LOG_CRIT, "fdopen() failed; %m");
		return (NULL);
	}
	return (fp);
}

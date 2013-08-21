/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"courier.h"
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<sys/types.h>
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

char *local_extension()
{
static char *ext=0;

	if (!ext)
	{
	char	*f=config_localfilename("dotextension");

		ext=config_read1l(f);
		free(f);
	}

	if (!ext || !*ext)
	{
		if (ext)	free(ext);
		ext="courier";
	}
	return (ext);
}

char *local_dotcourier(const char *homedir, const char *ext,
	const char **defaultptr)
{
char	*extbuf;
char	*x=local_extension();

	extbuf=(char *)courier_malloc(sizeof("." "--default")
			+strlen(x) +strlen(ext));

	strcat(strcpy( extbuf, "." ), x);
	if (defaultptr)	*defaultptr=ext;

#if LOCAL_EXTENSIONS

	if (*ext)
	{
	char	*c;
	char	*filebuf=courier_malloc(sizeof("/." "--default")
			+strlen(x) +strlen(ext)+strlen(homedir));
	int	isfirst=1;
	struct	stat	stat_buf;

		strcat(strcat(extbuf, "-"), ext);
		for (c=extbuf+1; *c; c++)
			if (*c == '.')	*c=':';

		if (defaultptr)
			*defaultptr=0;

		while (lstat(
			strcat(strcat(strcpy(filebuf, homedir), "/"),
				extbuf), &stat_buf) < 0)
		{
			/* We simply may not have the permissions to read
			** someone's home directory.  This can happen if
			** rewriting is carried out in a non-privileged
			** process which is submitting a mail message.
			**
			** Unless we have an ENOENT at this point, assume
			** that this is the case, and presume that the
			** address is valid.  The downshot is that the
			** message will be accepted, and an internal bounce
			** is later generated.
			*/

			if (errno != ENOENT)
			{
				free(extbuf);
				free(filebuf);
				return (0);
			}

			*strrchr(extbuf, '-')=0;
			if (!isfirst)
			{
				if ((c=strrchr(extbuf, '-')) == 0)
				{
					free(extbuf);
					free(filebuf);
					return (0);
				}
				*c=0;
			}
			isfirst=0;
			if (defaultptr)
				*defaultptr=ext+ (strlen(extbuf)-sizeof(".")
					-strlen(x)+1);
			strcat(extbuf, "-default");
		}
		free(filebuf);
	}
#endif
	return (extbuf);
}

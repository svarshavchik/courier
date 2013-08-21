/*
** Copyright 1998 - 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"sysconfdir.h"

#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>

static const char *local_vhost=0;

char	*config_localfilename(const char *p)
{
	char	*c=courier_malloc(sizeof(SYSCONFDIR "/")+strlen(p)
				  + (local_vhost ? strlen(local_vhost)+2:0));

	char	*fn=strcat(strcpy(c, SYSCONFDIR "/"), p);

	if (local_vhost)
	{
		size_t l=strlen(fn);

		strcat(strcat(fn, "."), local_vhost);

		if (access(fn, 0) == 0)
			return fn;
		fn[l]=0;
	}
	return fn;
}

int	config_has_vhost(const char *p)
{
	char	*filename
		=courier_malloc(sizeof(SYSCONFDIR "/vhost.")+strlen(p));
	int exists;

	strcat(strcpy(filename, SYSCONFDIR "/vhost."), p);
	exists=access(filename, 0) == 0;
	free(filename);
	return exists;
}

void	config_set_local_vhost(const char *p)
{
	local_vhost=p;
}

const char *config_get_local_vhost()
{
	return local_vhost;
}


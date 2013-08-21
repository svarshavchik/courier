/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"comconfig.h"
#include	"courier.h"
#include	<stdlib.h>
#include	<stdio.h>

int config_retrybeta()
{
char	*f=config_localfilename("retrybeta");
char	*p=config_read1l(f);
int	n=3;

	free(f);

	if (p)
	{
		n=atoi(p);
		free(p);
	}
	return (n);
}

int config_retrymaxdelta()
{
char	*f=config_localfilename("retrymaxdelta");
char	*p=config_read1l(f);
int	n=3;

	free(f);
	if (p)
	{
		n=atoi(p);
		free(p);
	}
	return (n);
}

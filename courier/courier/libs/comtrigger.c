/*
** Copyright 1998 - 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"localstatedir.h"
#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<stdlib.h>

int trigger_open(int mode)
{
int	fd;

	fd=open(TMPDIR "/trigger", mode);
	return (fd);
}

void trigger(const char *command)
{
int	fd=trigger_open(O_WRONLY | O_NDELAY);

	if (fd >= 0)
	{
		if (write( fd, command, strlen(command)))
			; /* ignore */
		close(fd);
	}
}

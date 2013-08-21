/*
** Copyright 1998 - 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

/*
	Open and read a one-line configuration file.

	Assume the line is less than 256 characters.

	Returns NULL if the configuration file does not exist.
*/

char	*config_read1l(const char *filename)
{
FILE	*fp=fopen(filename, "r");
char	buf[256];
char	*p;

	if (!fp)	return (0);
	if (fgets(buf, sizeof(buf), fp) == NULL)
		buf[0]=0;
	fclose(fp);
	if ((p=strchr(buf, '\n')) != 0)	*p=0;
	return (strcpy( (char *)courier_malloc(strlen(buf)+1), buf));
}

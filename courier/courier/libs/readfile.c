/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	<sys/stat.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

/*
	Read an entire file into memory.

*/

char	*readfile(const char *filename, struct stat *stat_buf)
{
struct	stat	my_buf;
FILE	*fp;
char	*p=0;

	if (!stat_buf)	stat_buf= &my_buf;
	if ((fp=fopen(filename, "r")) == 0)	return (0);

	if (fstat(fileno(fp), stat_buf) ||
		fread((p=(char *)courier_malloc(stat_buf->st_size+1)),
			1, stat_buf->st_size, fp) != stat_buf->st_size)
	{
		fclose(fp);
		clog_msg_errno();
	}
	fclose(fp);
	p[stat_buf->st_size]=0;
	return (p);
}

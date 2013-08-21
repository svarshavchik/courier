/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"maxlongsize.h"
#include	<stdio.h>
#include	<stdlib.h>

static char batchsize_buf[MAXLONGSIZE]="";

const char *config_batchsize()
{
char	*batchfilename;
char	*batchsizestr;
unsigned batchsize;

	if (batchsize_buf[0] == '\0')
	{
		batchfilename=config_localfilename("batchsize");
		batchsizestr=config_read1l(batchfilename);

		free(batchfilename);

		batchsize=100;
		if (batchsizestr)
		{
			batchsize=atoi(batchsizestr);
			free(batchsizestr);
			if (batchsize == 0)	batchsize=100;
						/* stupid user */
		}
		if (batchsize <= 0)	batchsize=5;
		sprintf(batchsize_buf, "%u", batchsize);
	}
	return (batchsize_buf);
}

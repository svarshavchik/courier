/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"comctlfile.h"
#include	<string.h>
#include	<stdio.h>
#include	<errno.h>
#if	HAVE_UNISTD_H
#include	"unistd.h"
#endif
#include	"numlib/numlib.h"

void clog_msg_msgid(struct ctlfile *ctf)
{
int	n=ctlfile_searchfirst(ctf, COMCTLFILE_MSGID);

	if (n >= 0)
		clog_msg_str(ctf->lines[n]+1);
}

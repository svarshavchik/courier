/*
** Copyright 2001 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"libs/courier_lib_config.h"
#include	"courier.h"
#include	"comctlfile.h"
#include	<sys/types.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>


const char *ctlfile_security(struct ctlfile *p)
{
	int i=ctlfile_searchfirst(p, COMCTLFILE_SECURITY);

	return (i < 0 ? "":p->lines[i]+1);
}

/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_lib_config.h"
#endif
#include	"courier.h"
#include	"localstatedir.h"
#include	"comqueuename.h"
#include	"maxlongsize.h"
#include	"comstrinode.h"
#include	"comstrtimestamp.h"
#include	<stdlib.h>
#include	<string.h>

const char *qmsgsdir(ino_t inum)
{
static char buf[MAXLONGSIZE + 10 + sizeof(MSGSDIR "/") ];

	return (strcat(strcpy(buf, MSGSDIR "/"),
		strinode(inum % QDIRCOUNT)));
}

static const char * qmsgsname(ino_t inum, const char *type)
{
static char buf[MAXLONGSIZE*2+20 ];

	strcpy(buf, qmsgsdir(inum));
	return (strcat(strcat(strcat(buf, "/"), type), strinode(inum)));
}

const char *qmsgsctlname(ino_t inum)
{
	return (qmsgsname(inum, "C"));
}

const char *qmsgsdatname(ino_t inum)
{
	return (qmsgsname(inum, "D"));
}

const char *qmsgqdir(time_t t)
{
static char buf[MAXLONGSIZE + 10 + sizeof(MSGQDIR "/") ];

	strcat(strcpy(buf, MSGQDIR "/"), strtimestamp(t));
	buf[strlen(buf)-4]=0;	/* I'm pretty sure it's at least 1971 */
	return (buf);
}

const char *qmsgqname(ino_t i, time_t t)
{
static char buf[MAXLONGSIZE*3 + 25 + sizeof(MSGQDIR "/") ];

	strcat(strcpy(buf, qmsgqdir(t)), "/C");
	strcat(buf, strinode(i));
	strcat(buf, ".");
	strcat(buf, strtimestamp(t));
	return (buf);
}

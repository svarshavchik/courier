/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include "messagesize.H"
#include "gettext.H"

MessageSize::MessageSize(unsigned long bytes, bool showBytes)
{
	char buf[100];

	buf[0]=0;

	if (bytes > 1024L * 1024)
		sprintf(buf, _("%.1f Mb"), bytes / (1024.0 * 1024.0));
	else if (bytes > 1024)
		sprintf(buf, _("%lu Kb"), (bytes + 512) / 1024);
	else if (bytes > 0)
		sprintf(buf, showBytes ? _("%lu bytes"): "%lu", bytes);

	buffer=buf;
}

MessageSize::~MessageSize()
{
}

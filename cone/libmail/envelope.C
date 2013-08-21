/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "envelope.H"
#include "rfcaddr.H"

LIBMAIL_START

envelope::envelope() : date(0)
{
}

envelope::~envelope()
{
}

xenvelope::xenvelope() : arrivalDate(0), messageSize(0)
{
}

xenvelope::~xenvelope()
{
}

xenvelope &xenvelope::operator=(const envelope &e)
{
	((envelope &)*this)=e;
	return *this;
}

LIBMAIL_END

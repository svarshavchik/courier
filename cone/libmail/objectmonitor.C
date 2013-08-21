/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "objectmonitor.H"

using namespace std;

mail::ptrBase::ptrBase()
{
}

mail::ptrBase::~ptrBase()
{
}

mail::obj::obj()
{
}

mail::obj::obj(const mail::obj &o)
{
}

mail::obj &mail::obj::operator=(const mail::obj &o)
{
	return *this;
}

mail::obj::~obj()
{
	set<mail::ptrBase *>::iterator b=objectBaseSet.begin(),
		e=objectBaseSet.end();

	while (b != e)
		(*b++)->ptrDestroyed();
}

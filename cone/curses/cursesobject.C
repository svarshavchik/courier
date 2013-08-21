/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "cursesobject.H"

using namespace std;

cursesPtrBase::cursesPtrBase()
{
}

cursesPtrBase::~cursesPtrBase()
{
}

CursesObj::CursesObj()
{
}

CursesObj::CursesObj(const CursesObj &o)
{
}

CursesObj &CursesObj::operator=(const CursesObj &o)
{
	return *this;
}

CursesObj::~CursesObj()
{
	set<cursesPtrBase *>::iterator b=cursesBase.begin(),
		e=cursesBase.end();

	while (b != e)
		(*b++)->ptrDestroyed();
}

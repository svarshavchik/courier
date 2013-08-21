/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "curseskeyhandler.H"

#include <functional>
#include <algorithm>

using namespace std;

list<CursesKeyHandler *> CursesKeyHandler::handlers;
bool CursesKeyHandler::handlerListModified=true;

CursesKeyHandler::CursesKeyHandler(int priorityArg)
	: priority(priorityArg)
{
	list<CursesKeyHandler *>::iterator b=handlers.begin(),
		e=handlers.end();

	while (b != e)
	{
		if ( (*b)->priority > priorityArg)
			break;
		b++;
	}

	handlers.insert(b, 1, this);

	handlerListModified=true;
}

CursesKeyHandler::~CursesKeyHandler()
{
	list<CursesKeyHandler *>::iterator me=
		find_if(handlers.begin(), handlers.end(),
			bind2nd(equal_to<CursesKeyHandler *>(), this));

	handlers.erase(me);
	handlerListModified=true;
}

bool CursesKeyHandler::listKeys( vector< pair<string, string> > &list)
{
	return false;
}

bool CursesKeyHandler::handle(const Curses::Key &key, Curses *focus)
{
	list<CursesKeyHandler *>::iterator b=handlers.begin(),
		e=handlers.end();

	while (b != e)
	{
		CursesKeyHandler *p= *b++;

		if (p->priority >= 0 && focus)
		{
			if (focus->processKeyInFocus(key))
				return true;
			focus=NULL;
		}

		if ( p->processKey(key))
			return true;
	}

	if (focus)
		if (focus->processKeyInFocus(key))
			return true;
	return false;
}

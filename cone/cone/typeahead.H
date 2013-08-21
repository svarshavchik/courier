/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef typeahead_H
#define typeahead_H

///////////////////////////////////////////////////////////////////////////////
//
// A convenient buffer where typeahead is stowed away.
//
// Keyboard input is always read.  Input that cannot be processed immediately
// would normally get discarded.  This happens when myServer::eventloop is
// waiting for a command to complete.
//
// Typeahead is natural and intuitive.  When there's an unprocessed key
// with a pending command, it gets stowed away in this buffer.  When there is
// no pending command, this typeahead buffer is slowly flushed.

#include "config.h"
#include "curses/mycurses.H"
#include <list>

class Typeahead {

	std::list<Curses::Key> buf;

public:

	Typeahead();
	~Typeahead();

	static Typeahead *typeahead; // One glob instance of this object

	void push(const Curses::Key &k)
	{
		buf.push_back(k);
	}

	bool empty()
	{
		return (buf.begin() == buf.end());
	}

	Curses::Key pop()
	{
		Curses::Key k=buf.front();
		buf.pop_front();
		return k;
	}
};

#endif

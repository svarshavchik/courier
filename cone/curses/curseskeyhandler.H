/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef curseskeyhandler_H
#define curseskeyhandler_H

#include <list>

#include "mycurses.H"

#define PRI_STATUSOVERRIDEHANDLER -4	// Override even the status line
#define PRI_STATUSHANDLER -3		// Status line
#define PRI_DIALOGHANDLER -2		// Dialog
#define PRI_PRISCREENHANDLER -1		// Normal screenwide handler,
					// but prior to any focus handling
#define PRI_SCREENHANDLER 0		// Normal screenwide handler
#define PRI_DEFAULTCTRLCHANDLER 1	// CTRL-C handler

/////////////////////////////////////////////////////////////////////////////
//
// A list of prioritized key handlers.  CursesKeyHandler objects are created
// whenever a function key shortcut should be used.  Each CursesKeyHandler
// defines a processKey() method that returns true if it has processed the
// received key input.  The object should also implement listKeys(), to append
// a list of keys it handles to the list argument.
//
// Each key handler has a defined priority. The handle() method runs
// the processKey method of all defined handlers, in priority order, until
// processKey returns true.
// The processKeyInFocus method of the specified Curses object may be called
// (if focus is not NULL), in the event that no key handler with a negative
// priority processed the key (and if processKeyInFocus also doesn't handle
// the key, any remaining non-negative keyhandlers are given a crack at this).

class CursesKeyHandler {

	int priority;

public:
	CursesKeyHandler(int priorityArg);
	virtual ~CursesKeyHandler();

	static bool handle(const Curses::Key &key, Curses *focus);
	// Returns true if the key was consumed.

protected:
	virtual bool processKey(const Curses::Key &key)=0;

	// Key handlers should subclass this and enumerate all the keys
	// they handle.  <key name, description> should be added to list.
	// Subclass should return true to ignore the rest of keyhandlers.
public:
	virtual bool listKeys( std::vector< std::pair<std::string,
			       std::string> > &list);

private:
	static std::list<CursesKeyHandler *> handlers;

public:
	static std::list<CursesKeyHandler *>::const_iterator begin()
	{
		return handlers.begin();
	}

	static std::list<CursesKeyHandler *>::const_iterator end()
	{
		return handlers.end();
	}

	static bool handlerListModified;
	// Reset to true each time a handler is added or removed, used to
	// indicate when the status line should be redrawn

};

#endif

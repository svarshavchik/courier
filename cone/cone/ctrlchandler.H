/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef cntrlchandler_h
#define cntrlchandler_h

#include "config.h"
#include "curses/mycurses.H"
#include "curses/curseskeyhandler.H"

#include <time.h>

//
// Handle CTRL-C events
//

class CtrlCHandler : public CursesKeyHandler {

public:
	CtrlCHandler();
	~CtrlCHandler();

	static bool loggingOut;	// Logout/termination in progress
	static time_t lastCtrlC; // When last ctrlc was hit

private:
	virtual bool processKey(const Curses::Key &key);
};

#endif

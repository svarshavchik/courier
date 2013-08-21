/*
** Copyright 2010, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef fkeytraphandler_H
#define fkeytraphandler_H

#include "config.h"
#include "curses/cursescontainer.H"
#include "curses/curseskeyhandler.H"


///////////////////////////////////////////////////////////////////////////
//
// Helper class for defining a macro.  A key handler that intercepts function
// keypresses and aborts the status bar prompt.
//
// Optionally, all other keypresses get dropped.

class FKeyTrapHandler : public CursesKeyHandler {

public:

	bool onlyfkeys;

	bool defineFkeyFlag;
	int fkeyNum;

	FKeyTrapHandler(bool onlyfkeysArg=false);
	~FKeyTrapHandler();

	bool processKey(const Curses::Key &key);
};

#endif

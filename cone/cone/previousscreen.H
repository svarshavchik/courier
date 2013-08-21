/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef previousscreen_H
#define previousscreen_H

#include "config.h"

//
// Certain functions need to return to the "previous" screen.
//
// This is done as follows.
//
// 1) Objects that represent a known screen that qualifies as a "previous"
//    screen are subclassed from PreviousScreen, and are instantiated with
//    an argument that points to the main screen function (invoked from
//    main()'s loop) and the screen function argument.
//
// 2) A static pointer to the last "previous" screen is set by each
//    constructor.
//
// 3) If, by some reason, the original object is destroyed, the destructor
//    resets the static pointer to a NULL.
//
// 4) To return to the "previous" screen, first myServer::nextScreen is set
//    to a default screen function (the folder listing screen is the usual
//    choice), then PreviousScreen::previousScreen() is invoked.
//
// 5) If the static ptr is not NULL, myServer::nextScreen+arg is reset.

class PreviousScreen {

	void (*screenFunction)(void *);
	void *screenFunctionArg;

	static PreviousScreen *lastScreen;

public:
	PreviousScreen(	void (*func)(void *), void *funcArg);
	void screenOpened();
	~PreviousScreen();

	static void previousScreen();
};

#endif


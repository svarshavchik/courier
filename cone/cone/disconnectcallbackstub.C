/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "disconnectcallbackstub.H"

#include "curses/mycurses.H"
#include "curses/cursesstatusbar.H"

extern CursesStatusBar *statusBar;

disconnectCallbackStub::disconnectCallbackStub()
{
}

disconnectCallbackStub::~disconnectCallbackStub()
{
}

void disconnectCallbackStub::disconnected(const char *errmsg)
{
	if (errmsg)
		statusBar->status(errmsg, statusBar->DISCONNECTED);
}

void disconnectCallbackStub::servererror(const char *errmsg)
{
	if (errmsg)
		statusBar->status(errmsg, statusBar->SERVERERROR);
}

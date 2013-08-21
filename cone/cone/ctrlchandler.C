/*
** Copyright 2003-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "ctrlchandler.H"
#include "myserver.H"
#include "gettext.H"
#include "addressbook.H"
#include "curses/cursesstatusbar.H"
#include "curses/cursesmainscreen.H"

extern CursesStatusBar *statusBar;
extern CursesMainScreen *mainScreen;

bool CtrlCHandler::loggingOut=false;
time_t CtrlCHandler::lastCtrlC=0;

CtrlCHandler::CtrlCHandler() : CursesKeyHandler(PRI_DEFAULTCTRLCHANDLER)
{
}

CtrlCHandler::~CtrlCHandler()
{
}

bool CtrlCHandler::processKey(const Curses::Key &key)
{
	if (key == '\x03')
	{
		if (loggingOut) // If logout stuck, we don't care
			LIBMAIL_THROW(_("Have a nice day."));

		if (myServer::cmdcount > 0) // Server command pending
		{
			time_t t=time(NULL);

			// Two Ctrl-Cs at least 5 seconds apart will do the
			// trick.

			if (lastCtrlC == 0 || t < lastCtrlC + 5)
			{
				lastCtrlC=t;
				statusBar->beepError();
			}
			else
			{
				LIBMAIL_THROW(_("Have a nice day."));
			}
		}
	}
	return false;
}


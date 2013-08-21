/*
** Copyright 2010, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "config.h"
#include "gettext.H"
#include "curses/cursesstatusbar.H"
#include "fkeytraphandler.H"

extern CursesStatusBar *statusBar;

extern Gettext::Key key_ABORT;

FKeyTrapHandler::FKeyTrapHandler(bool onlyfkeysArg)
	: CursesKeyHandler(PRI_STATUSHANDLER),
	  onlyfkeys(onlyfkeysArg), defineFkeyFlag(false), fkeyNum(0)
{
}

FKeyTrapHandler::~FKeyTrapHandler()
{
}

bool FKeyTrapHandler::processKey(const Curses::Key &key)
{
	if (key.fkey())
	{
		defineFkeyFlag=true;
		fkeyNum=key.fkeynum();
		statusBar->fieldAbort();
		return true;
	}

	if (onlyfkeys && key == key_ABORT)
		return false;

	return onlyfkeys;
}

/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "previousscreen.H"
#include "myserver.H"
#include "curses/mycurses.H"

PreviousScreen *PreviousScreen::lastScreen=NULL;

PreviousScreen::PreviousScreen(	void (*func)(void *), void *funcArg)
	: screenFunction(func), screenFunctionArg(funcArg)
{
	screenOpened();
}

void PreviousScreen::screenOpened()
{
	lastScreen=this;
}

PreviousScreen::~PreviousScreen()
{
	lastScreen=NULL;
}

void PreviousScreen::previousScreen()
{
	if (lastScreen)
	{
		myServer::nextScreen=lastScreen->screenFunction;
		myServer::nextScreenArg=lastScreen->screenFunctionArg;
		Curses::keepgoing=false;
	}
}

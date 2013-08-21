/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"

#include "cursesmainscreen.H"

CursesMainScreen::CursesMainScreen(CursesContainer *parent,
				   Curses *titleBarArg,
				   Curses *statusBarArg)
	: CursesVScroll(parent), titleBar(titleBarArg),
	  statusBar(statusBarArg), lockcnt(0)
{
	setRow(titleBar->getHeight());
}

CursesMainScreen::~CursesMainScreen()
{
}

int CursesMainScreen::getWidth() const
{
	return getParent()->getWidth();
}

int CursesMainScreen::getHeight() const
{
	int h=getParent()->getHeight();

	int hh=titleBar->getHeight() + statusBar->getHeight();

	return (hh < h ? h-hh:0);
}

CursesMainScreen::Lock::Lock(CursesMainScreen *p, bool noupdateArg)
	: screen(p), noupdate(noupdateArg)
{
	if (p)
		++p->lockcnt;
}

CursesMainScreen::Lock::~Lock()
{
	if (!screen.isDestroyed())
	{
		if (--screen->lockcnt == 0 && !noupdate)
		{
			screen->erase();
			screen->draw();
		}
	}
}

bool CursesMainScreen::writeText(const char *text, int row, int col,
				 const CursesAttr &attr) const
{
	char dummy=0;

	if (lockcnt > 0)
		text= &dummy;

	return CursesVScroll::writeText(text, row, col, attr);
}

bool CursesMainScreen::writeText(const std::vector<unicode_char> &text,
				 int row, int col,
				 const Curses::CursesAttr &attr) const
{
	std::vector<unicode_char> dummy;

	return CursesVScroll::writeText(lockcnt > 0 ? dummy:text,
					row, col, attr);
}

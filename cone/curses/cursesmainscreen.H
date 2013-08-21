/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesmainscreen_H
#define cursesmainscreen_H

#include "mycurses.H"
#include "cursesvscroll.H"

//
// A CursesVScroll used for the main screen.  It's parent is the CursesScreen,
// so it's size is defined as the width of its parent, and its parent's
// height minus the status bar height and the title bar height.
//

class CursesMainScreen : public CursesVScroll {

	Curses *titleBar, *statusBar;

	// A lock can be obtained that blocks out updates to the screen.
	// This is useful when we know in advance about multiple changes to
	// the screen that will occur over a period of time, which might be
	// confusing.  A Lock structure prevents changes to the main screen
	// from taking place.  Instantiating a Lock structure increments
	// lockcnt.  The destructor decrements lockcnt.
	// WriteText() is overriden and will terminate without doing anything
	// if lockcnt is non-zero.
	// When lockcnt is decrement back to 0 the entire object is redrawn.

	unsigned lockcnt;

public:

	class Lock {
		cursesPtr<CursesMainScreen> screen;

		Lock(const Lock &); // UNIMPLEMENTED
		Lock &operator=(const Lock &); // UNIMPLEMENTED

		bool noupdate;

	public:
		Lock(CursesMainScreen *, bool noupdateArg=false);
		~Lock();
	};

	friend class Lock;

	CursesMainScreen(CursesContainer *parent,
			 Curses *titleBarArg,
			 Curses *statusBarArg);
	~CursesMainScreen();

	int getWidth() const;
	int getHeight() const;

	bool writeText(const char *text, int row, int col,
		       const CursesAttr &attr) const;
	bool writeText(const std::vector<unicode_char> &text, int row, int col,
		       const Curses::CursesAttr &attr) const;
};

#endif

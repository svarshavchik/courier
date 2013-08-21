/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesvscroll_H
#define cursesvscroll_H

#include "mycurses.H"
#include "cursescontainer.H"

//
// A vertically-scrolling viewport.  The subclass must define getWidth()
// and getHeight().
//

class CursesVScroll : public CursesContainer {

	size_t firstRowShown;

public:
	CursesVScroll(CursesContainer *parent);
	~CursesVScroll();

	// When something is deleted from the viewport, jump the view port
	// to the top

	void deleteChild(Curses *child);

	// Automatically scroll the viewport to following keyboard input.
	// This is done by overriding getCursorPosition(), calling
	// scrollTo(), then simply subtracting the topmost row shown.

	int getCursorPosition(int &row, int &col);

	//
	// Adjust for vertical scroll position:
	//

	bool writeText(const char *text, int row, int col,
		       const CursesAttr &attr) const;
	bool writeText(const std::vector<unicode_char> &text,
		       int row, int col,
		       const Curses::CursesAttr &attr) const;

	//
	// Make sure the following row is visible right now.
	//
	void scrollTo(size_t rowNum);

	// Intercept getVerticalViewport(), and return reality.

	void getVerticalViewport(size_t &first_row,
				 size_t &nrows);

	void erase();

	int getWidth() const;
	size_t getFirstRowShown() const { return firstRowShown; }
	void setFirstRowShown(size_t rowNum)
	{
		firstRowShown=rowNum;
		redraw();
	}

private:
	void redraw();
};

#endif

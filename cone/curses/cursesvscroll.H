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

	void deleteChild(Curses *child) override;

	// Automatically scroll the viewport to following keyboard input.
	// This is done by overriding getCursorPosition(), calling
	// scrollTo(), then simply subtracting the topmost row shown.

	int getCursorPosition(int &row, int &col) override;

	//
	// Adjust for vertical scroll position:
	//

	bool writeText(const char *text, int row, int col,
		       const CursesAttr &attr) const override;
	bool writeText(const std::u32string &text,
		       int row, int col,
		       const Curses::CursesAttr &attr) const override;

	//
	// Make sure the following row is visible right now.
	//
	void scrollTo(size_t rowNum) override;

	// Intercept getVerticalViewport(), and return reality.

	void getVerticalViewport(size_t &first_row,
				 size_t &nrows) override;

	void erase() override;

	int getWidth() const override;
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

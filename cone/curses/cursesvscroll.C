/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "cursesvscroll.H"

CursesVScroll::CursesVScroll(CursesContainer *parent)
	: CursesContainer(parent), firstRowShown(0)
{
}

CursesVScroll::~CursesVScroll()
{
}

int CursesVScroll::getCursorPosition(int &row, int &col)
{
	scrollTo(row);

	row -= firstRowShown;

	return (CursesContainer::getCursorPosition(row, col));
}


int CursesVScroll::getWidth() const
{
	return getScreenWidth();
}

void CursesVScroll::getVerticalViewport(size_t &first_row,
					size_t &nrows)
{
	first_row=firstRowShown;
	nrows=getHeight();
}

void CursesVScroll::erase()
{
	size_t w=getWidth(), h=getHeight();

	std::vector<unicode_char> spaces;

	spaces.insert(spaces.end(), w, ' ');

	size_t i;

	for (i=0; i<h; i++)
		CursesContainer::writeText(spaces, i, 0, CursesAttr());
}

void CursesVScroll::deleteChild(Curses *child)
{
	CursesContainer::deleteChild(child);
	scrollTo(0);
}

void CursesVScroll::scrollTo(size_t row)
{
	size_t h=getHeight();

#if 0
	if (row < 0 || (size_t)row < h)
	{
		if (firstRowShown > 0)
		{
			firstRowShown=0;
			redraw();
		}
	}
	else
#endif
		if ((size_t)row < firstRowShown)
	{
		if (row + 1 < firstRowShown)
			firstRowShown=row;
		else
		{
			if (h > 6)
			{
				firstRowShown=row+5;
				
				if (firstRowShown >= h)
					firstRowShown -= h;
				else
					firstRowShown=0;
			}
			else
				firstRowShown=row;
		}
		redraw();
	}
	else if ((size_t)row >= firstRowShown + h)
	{
		if (firstRowShown + h < (size_t)row)
		{
			firstRowShown=row + 1 - h;
		}
		else
		{
			if (h > 6)
				firstRowShown=row-5;
			else
				firstRowShown=row + 1 - h;
		}
		redraw();
	}
}

bool CursesVScroll::writeText(const char *text, int row, int col,
			      const CursesAttr &attr) const
{
	if (row < 0 ||
	    (size_t)row < firstRowShown ||
	    (size_t)row >= firstRowShown + getHeight())
		return false; // This row is not shown

	row -= firstRowShown;

	return CursesContainer::writeText(text, row, col, attr);
}

bool CursesVScroll::writeText(const std::vector<unicode_char> &text,
			      int row, int col,
			      const Curses::CursesAttr &attr) const
{
	if (row < 0 ||
	    (size_t)row < firstRowShown ||
	    (size_t)row >= firstRowShown + getHeight())
		return false; // Ditto.

	row -= firstRowShown;

	return CursesContainer::writeText(text, row, col, attr);
}

//
// We just logically scrolled.  Clear the window, redraw.
//

void CursesVScroll::redraw()
{
	erase();
	draw();
}

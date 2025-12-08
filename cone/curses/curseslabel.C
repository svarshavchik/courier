/*
** Copyright 2002-2011, S. Varshavchik.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "curseslabel.H"

using namespace std;

CursesLabel::CursesLabel(CursesContainer *parent,
			 string textArg,
			 Curses::CursesAttr attributeArg)
	: Curses(parent), attribute(attributeArg)
{
	setutext(textArg);
}

CursesLabel::~CursesLabel()
{
}

void CursesLabel::setRow(int row)
{
	erase();
	Curses::setRow(row);
	draw();
}

void CursesLabel::setCol(int col)
{
	erase();
	Curses::setCol(col);
	draw();
}

void CursesLabel::setAlignment(Alignment newAlignment)
{
	erase();
	Curses::setAlignment(newAlignment);
	draw();
}

void CursesLabel::setAttribute(Curses::CursesAttr attr)
{
	attribute=attr;
	draw();
}

void CursesLabel::setText(string textArg)
{
	erase();
	setutext(textArg);
	draw();
}

void CursesLabel::setutext(const std::string &textArg)
{
	std::u32string buf;

	unicode::iconvert::convert(textArg, unicode_default_chset(), buf);

	widecharbuf wc;

	wc.init_unicode(buf.begin(), buf.end());
	wc.expandtabs(0);

	wc.tounicode(utext);
	w=wc.wcwidth(0);
}

int CursesLabel::getWidth() const
{
	return w;
}

int CursesLabel::getHeight() const
{
	return 1;
}

void CursesLabel::draw()
{
	writeText(utext, 0, 0, attribute);
}

void CursesLabel::erase()
{
	std::u32string s;

	s.insert(s.end(), getWidth(), ' ');

	writeText(s, 0, 0, CursesAttr());
}


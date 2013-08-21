/*
** Copyright 2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "cursesmultilinelabel.H"

CursesMultilineLabel::CursesMultilineLabel(CursesContainer *parent,
					   std::string textArg,
					   Curses::CursesAttr attributeArg)
	: Curses(parent), width(0), attribute(attributeArg)
{
	mail::iconvert::convert(textArg, unicode_default_chset(), text);
}

void CursesMultilineLabel::init()
{
	int w=getWidth();

	if (w < 10)
		w=10;

	lines.clear();

	std::back_insert_iterator< std::vector< std::vector<unicode_char> > >
		insert_iter(lines);

	unicodewordwrap(text.begin(), text.end(),
			unicoderewrapnone(), insert_iter, w, true);
}

CursesMultilineLabel::~CursesMultilineLabel()
{
}

void CursesMultilineLabel::setRow(int row)
{
	erase();
	Curses::setRow(row);
	draw();
}

void CursesMultilineLabel::setCol(int col)
{
	erase();
	Curses::setCol(col);
	draw();
}

void CursesMultilineLabel::setText(std::string newText)
{
	erase();
	text.clear();
	mail::iconvert::convert(newText, unicode_default_chset(), text);
	init();
	draw();
}

void CursesMultilineLabel::setAlignment(Alignment newAlignment)
{
	erase();
	Curses::setAlignment(newAlignment);
	draw();
}

void CursesMultilineLabel::setAttribute(Curses::CursesAttr attr)
{
	attribute=attr;
	draw();
}

int CursesMultilineLabel::getWidth() const
{
	return width;
}

void CursesMultilineLabel::setWidth(int w)
{
	erase();
	width=w;
	init();
	draw();
}

int CursesMultilineLabel::getHeight() const
{
	int n=(int)lines.size();

	if (n <= 0)
		n=1;
	return n;
}

void CursesMultilineLabel::resized()
{
	erase();
	init();
	draw();
}

void CursesMultilineLabel::draw()
{
	erase();

	size_t row=0;

	for ( std::vector< std::vector<unicode_char> >::iterator
		      b(lines.begin()), e(lines.end()); b != e; ++b, ++row)
	{
		writeText(*b, row, 0, attribute);
	}
}

void CursesMultilineLabel::erase()
{
	std::vector<unicode_char> uc;

	uc.insert(uc.end(), getWidth(), ' ');

	size_t i, n=getHeight();
	for (i=0; i<n; i++)
		writeText(uc, i, 0, attribute);
}


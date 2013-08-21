/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "cursestitlebar.H"
#include "cursescontainer.H"

using namespace std;

CursesTitleBar::CursesTitleBar(CursesContainer *parent, string titleArg)
	: Curses(parent), title(titleArg)
{
	setRow(0);
	setCol(0);
}

CursesTitleBar::~CursesTitleBar()
{
}

int CursesTitleBar::getHeight() const
{
	return 1;
}

int CursesTitleBar::getWidth() const
{
	return getParent()->getWidth();
}

void CursesTitleBar::setTitles(string left, string right)
{
	leftTitle=left;
	rightTitle=right;
	draw();
}

void CursesTitleBar::setAttribute(CursesAttr attr)
{
	attribute=attr;
	draw();
}

void CursesTitleBar::draw()
{
	size_t w=getWidth();

	Curses *p=getParent();

	if (p == NULL)
	    return;

	// First wipe out the status line

	string spaces="";

	spaces.append(w, ' ');

	CursesAttr rev=attribute;

	rev.setReverse();

	p->writeText(spaces, 0, 0, rev);

	// Now, write out the text strings

	p->writeText(leftTitle, 0, 1, rev);

	size_t l;

	{
		widecharbuf wc;

		wc.init_string(rightTitle);
		wc.expandtabs(0);

		l=wc.wcwidth(0);

		if (l + 1< w)
		{
			std::vector<unicode_char> uc;

			wc.tounicode(uc);

			p->writeText(uc, 0, w - 1 - l, rev);
		}
	}

	{
		widecharbuf wc;

		wc.init_string(title);
		wc.expandtabs(0);

		l=wc.wcwidth(0);

		if (l < w)
		{

			std::vector<unicode_char> uc;

			wc.tounicode(uc);

			p->writeText(uc, 0, (w-l)/2, rev);
		}
	}
}

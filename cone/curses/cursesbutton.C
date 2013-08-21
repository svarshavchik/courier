/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "cursesbutton.H"

using namespace std;

CursesButton::CursesButton(CursesContainer *parent,
			   string textArg,
			   int toggle) : CursesLabel(parent, ""),
					 toggleButton(toggle),
					 buttonName(textArg)
{
	setStyle(toggleButton ? TOGGLE:NORMAL);
}

CursesButton::~CursesButton()
{
	erase();
}

void CursesButton::setText(string textArg)
{
	buttonName=textArg;
	setStyle(currentStyle);
}

void CursesButton::setToggled(bool flag)
{
	toggleButton= flag ? -1:1;
	setStyle(TOGGLE);
}

void CursesButton::setStyle(Style style)
{
	currentStyle=style;

	switch (currentStyle) {
	case TOGGLE:
		CursesLabel::setText((toggleButton < 0 ? "[X] ":"[ ] ") + buttonName);
		break;
	case MENU:
		CursesLabel::setText(buttonName);
		break;
	default:
		CursesLabel::setText("[ " + buttonName + " ]");
		break;
	}
}

void CursesButton::draw()
{
	int fg=attribute.getFgColor();
	int bg=attribute.getBgColor();

	switch (currentStyle) {
	case TOGGLE:
		attribute=Curses::CursesAttr();
		break;
	case MENU:
		attribute=Curses::CursesAttr().setReverse(hasFocus());
		break;
	default:
		if (hasFocus())
			attribute=Curses::CursesAttr().setReverse();
		else
			attribute=Curses::CursesAttr().setHighlight();
		break;
	}

	attribute.setFgColor(fg);
	attribute.setBgColor(bg);
	CursesLabel::draw();
}

bool CursesButton::isFocusable()
{
	return true;
}

void CursesButton::focusGained()
{
	draw();
}

void CursesButton::focusLost()
{
	draw();
}

int CursesButton::getCursorPosition(int &r, int &c)
{
	r=0;

	if (currentStyle == TOGGLE)
	{
		c=1;
		CursesLabel::getCursorPosition(r, c);
		return 1;
	}

	c=CursesLabel::getWidth();

	CursesLabel::getCursorPosition(r, c);

	return 0;
}

int CursesButton::getWidth() const
{
	return CursesLabel::getWidth()+1;
}

void CursesButton::clicked()
{
}

bool CursesButton::processKeyInFocus(const Key &key)
{
	if (key == key.ENTER || (key.plain() && key.ukey == ' '))
	{
		if (toggleButton)
		{
			toggleButton= -toggleButton;
			setStyle(TOGGLE);
		}
		clicked();
		return true;
	}

	return CursesLabel::processKeyInFocus(key);
}

/*
**
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"

#include "mycurses.H"
#include "widechar.H"
#include "cursescontainer.H"
#include "curseskeyhandler.H"

#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <iostream>

bool Curses::keepgoing=false;
bool Curses::shiftmode=false;

std::string Curses::suspendCommand;

#define TABSIZE 8

const char Curses::Key::LEFT[]="LEFT",
	Curses::Key::RIGHT[]="RIGHT",
	Curses::Key::SHIFTLEFT[]="SHIFTLEFT",
	Curses::Key::SHIFTRIGHT[]="SHIFTRIGHT",
	Curses::Key::UP[]="UP",
	Curses::Key::DOWN[]="DOWN",
	Curses::Key::SHIFTUP[]="SHIFTUP",
	Curses::Key::SHIFTDOWN[]="SHIFTDOWN",
	Curses::Key::DEL[]="DEL",	
	Curses::Key::CLREOL[]="CLREOL",
	Curses::Key::BACKSPACE[]="BACKSPACE",
	Curses::Key::ENTER[]="ENTER",
	Curses::Key::PGUP[]="PGUP",
	Curses::Key::PGDN[]="PGDN",
	Curses::Key::SHIFTPGUP[]="SHIFTPGUP",
	Curses::Key::SHIFTPGDN[]="SHIFTPGDN",
	Curses::Key::HOME[]="HOME",
	Curses::Key::END[]="END",
	Curses::Key::SHIFTHOME[]="SHIFTHOME",
	Curses::Key::SHIFTEND[]="SHIFTEND",
	Curses::Key::SHIFT[]="SHIFT",
	Curses::Key::RESIZE[]="RESIZE";

bool Curses::Key::operator==(const std::vector<unicode_char> &v) const
{
	return keycode == 0 &&
		std::find(v.begin(), v.end(), ukey) != v.end();
}

Curses::Curses(CursesContainer *parentArg) : parent(parentArg),
					     row(0), col(0),
					     alignment(Curses::LEFT)
{
	if (parent != NULL)
		parent->addChild(this);
}

Curses::~Curses()
{
	CursesContainer *p=getParent();

	if (p)
		p->deleteChild(this);

	if (hasFocus())
		currentFocus=0;
}

int Curses::getScreenWidth() const
{
	const CursesContainer *p=getParent();

	while (p && p->getParent())
		p=p->getParent();

	return (p ? p->getWidth():0);
}

int Curses::getScreenHeight() const
{
	const CursesContainer *p=getParent();

	while (p && p->getParent())
		p=p->getParent();

	return (p ? p->getHeight():0);
}

int Curses::getRow() const
{
	return row;
}

int Curses::getCol() const
{
	return col;
}

void Curses::setRow(int r)
{
	row=r;
}

void Curses::scrollTo(size_t r)
{
	if (parent)
		parent->scrollTo(r + row);
}

void Curses::setCol(int c)
{
	col=c;
}

void Curses::resized()
{
	draw();
}

void Curses::getVerticalViewport(size_t &first_row,
				 size_t &nrows)
{
	CursesContainer *p=getParent();

	if (!p)
	{
		first_row=0;
		nrows=getHeight();
		return;
	}

	p->getVerticalViewport(first_row, nrows);
	size_t r=getRow();
	size_t h=getHeight();


	if (r + h <= first_row || r >= first_row + nrows)
	{
		first_row=nrows=0;
		return;
	}

	if (first_row < r)
	{
		nrows -= r-first_row;
		first_row=r;
	}

	first_row -= r;

	if (first_row + nrows > h)
		nrows=h-first_row;
}

void Curses::setAlignment(Alignment alignmentArg)
{
	alignment=alignmentArg;
}

Curses::Alignment Curses::getAlignment()
{
	return alignment;
}

int Curses::getRowAligned() const
{
	return row;
}

int Curses::getColAligned() const
{
	int c=col;

	if (alignment == PARENTCENTER)
	{
		const CursesContainer *p=getParent();

		if (p != NULL)
		{
			c=p->getWidth()/2;
			c -= getWidth()/2;
		}
	}
	else if (alignment == CENTER)
		c -= getWidth() / 2;
	else if (alignment == RIGHT)
		c -= getWidth();
	return c;
}

Curses *Curses::getDialogChild() const
{
	return NULL;
}

bool Curses::writeText(const char *text, int r, int c,
		       const CursesAttr &attr) const
{
	CursesContainer *p=parent;

	if (!isDialog() && p && p->getDialogChild())
		return false; // Parent has a dialog and it ain't us

	if (p)
		return p->writeText(text, r+getRowAligned(),
				    c+getColAligned(), attr);
	return false;
}

bool Curses::writeText(const std::vector<unicode_char> &text, int r, int c,
		       const Curses::CursesAttr &attr) const
{
	CursesContainer *p=parent;

	if (!isDialog() && p && p->getDialogChild())
		return false; // Parent has a dialog and it ain't us

	if (p)
		return p->writeText(text, r+getRowAligned(),
				    c+getColAligned(), attr);
	return false;
}


void Curses::writeText(std::string text, int row, int col,
		       const CursesAttr &attr) const
{
	writeText(text.c_str(), row, col, attr);
}

bool Curses::isDialog() const
{
	return false;
}

void Curses::erase()
{
	const CursesContainer *p=getParent();

	if (!isDialog() && p && p->getDialogChild())
		return; // Parent has a dialog and it ain't us

	size_t w=getWidth();
	size_t h=getHeight();

	std::vector<unicode_char> spaces;

	spaces.insert(spaces.end(), w, ' ');

	size_t i;

	CursesAttr attr;

	for (i=0; i<h; i++)
		writeText(spaces, i, 0, attr);
}

void Curses::beepError()
{
	CursesContainer *p=getParent();

	if (p)
		p->beepError();
}

Curses *Curses::currentFocus=0;

void Curses::requestFocus()
{
	Curses *oldFocus=currentFocus;

	currentFocus=NULL;

	cursesPtr<Curses> ptr=this;

	if (oldFocus)
		oldFocus->focusLost();

	if (!ptr.isDestroyed() && currentFocus == NULL)
	{
		currentFocus=this;
		focusGained();
	}
}

void Curses::dropFocus()
{
	Curses *oldFocus=currentFocus;

	currentFocus=NULL;

	if (oldFocus)
		oldFocus->focusLost();
}

void Curses::focusGained()
{
	draw();
}

void Curses::focusLost()
{
	draw();
}

void Curses::flush()
{
	CursesContainer *p=getParent();

	if (p)
		p->flush();
}

bool Curses::hasFocus()
{
	return currentFocus == this;
}

int Curses::getCursorPosition(int &r, int &c)
{
	r += getRowAligned();
	c += getColAligned();

	CursesContainer *parent=getParent();
	if (parent)
		return parent->getCursorPosition(r, c);

	return 1;
}


bool Curses::processKey(const Key &k)
{
	if (k == Key::RESIZE)
		return true; // No-op.

	return CursesKeyHandler::handle(k, currentFocus);
}

bool Curses::processKeyInFocus(const Key &key)
{
	if ((key.plain() && key.ukey == '\t')
	    || key == key.DOWN || key == key.SHIFTDOWN
	    || key == key.ENTER)
	{
		transferNextFocus();
		return true;
	}

	if (key == key.UP || key == key.SHIFTUP)
	{
		transferPrevFocus();
		return true;
	}

	if (key == key.PGUP || key == key.SHIFTPGUP)
	{
		int h=getScreenHeight();

		if (h > 5)
			h -= 5;

		if (h < 5)
			h=5;

		while (h)
		{
			processKey(Key(key ==
				       Key::PGUP ? Key::UP:Key::SHIFTUP));
			--h;
		}
		return true;
	}

	if (key == key.PGDN || key == key.SHIFTPGDN)
	{
		int h=getScreenHeight();

		if (h > 5)
			h -= 5;

		if (h < 5)
			h=5;

		while (h)
		{
			processKey(Key(key ==
				       Key::PGDN ? Key::DOWN:Key::SHIFTDOWN));
			--h;
		}
		return true;
	}

	return false;
}

bool Curses::isFocusable()
{
	return 0;
}

void Curses::transferNextFocus()
{
	Curses *p=this;

	do
	{
		p=p->getNextFocus();

	} while (p != NULL && !p->isFocusable());

	if (p)
		p->requestFocus();
}

void Curses::transferPrevFocus()
{
	Curses *p=this;

	do
	{
		p=p->getPrevFocus();
	} while (p != NULL && !p->isFocusable());

	if (p)
		p->requestFocus();
}

bool Curses::childPositionCompareFunc(Curses *a, Curses *b)
{
	if (a->getRow() < b->getRow())
		return true;

	if (a->getRow() == b->getRow())
	{
		if (a->getCol() < b->getCol())
			return true;
		if (a->getCol() == b->getCol())
		{
			CursesContainer *p=a->getParent();
			size_t ai, bi;

			for (ai=0; ai<p->children.size(); ai++)
				if (a == p->children[ai].child)
				{
					for (bi=0; bi<p->children.size(); bi++)
						if (b == p->children[bi].child)
							return ai < bi;
					break;
				}
		}
	}
	return false;
}

Curses *Curses::getNextFocus()
{
	CursesContainer *p=getParent();
	Curses *child=this;

	while (p != NULL)
	{
		size_t i;
		Curses *nextFocusPtr=NULL;

		for (i=0; i<p->children.size(); i++)
			if ( childPositionCompareFunc(child,
						      p->children[i].child) &&
			     (nextFocusPtr == NULL ||
			      childPositionCompareFunc(p->children[i].child,
						       nextFocusPtr)))
				nextFocusPtr=p->children[i].child;

		if (nextFocusPtr)
			return nextFocusPtr;

		if (p->isDialog())
			break; // Do not cross dialog box boundaries.

		child=p;
		p=p->getParent();
	}
	return NULL;
}

Curses *Curses::getPrevFocus()
{
	CursesContainer *p=getParent();
	Curses *child=this;

	while (p != NULL)
	{
		size_t i;

		Curses *prevFocusPtr=NULL;

		for (i=0; i<p->children.size(); i++)
			if ( childPositionCompareFunc(p->children[i].child,
						      child) &&
			     (prevFocusPtr == NULL ||
			      childPositionCompareFunc(prevFocusPtr,
						       p->children[i].child)))
				prevFocusPtr=p->children[i].child;

		if (prevFocusPtr)
			return prevFocusPtr;

		if (p->isDialog())
			break; // Do not cross dialog box boundaries.

		child=p;
		p=p->getParent();
	}
	return NULL;
}

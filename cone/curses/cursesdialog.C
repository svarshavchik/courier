/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "curses_config.h"
#include "cursesdialog.H"
#include "curseslabel.H"
#include "cursesfield.H"

using namespace std;

CursesDialog::CursesDialog(CursesContainer *parent)
	: CursesContainer(parent), max_label_width(0), max_field_width(0),
	  draw_flag(0)
{
}

CursesDialog::~CursesDialog()
{
}

// Automatically size to the largest prompt/input field combo

int CursesDialog::getWidth() const
{
	return max_label_width + max_field_width;
}

// Automatically size to the lowest input field

int CursesDialog::getHeight() const
{
	int h=0;

	vector< pair<CursesLabel *, Curses *> >::const_iterator
		b=prompts.begin(), e=prompts.end();

	while (b != e)
	{
		if (b->first && b->first->getRow() >= h)
			h=b->first->getRow()+1;

		if (b->second && b->second->getRow() >= h)
			h=b->second->getRow()+1;
		b++;

	}

	return h;
}

void CursesDialog::draw()
{
	draw_flag=1;
	CursesContainer::draw();
}

void CursesDialog::addPrompt(CursesLabel *label, Curses *field)
{
	size_t maxrow=0;

	vector< pair<CursesLabel *, Curses *> >::iterator
		b=prompts.begin(), e=prompts.end();

	while (b != e)
	{
		if ( b->first != NULL && (size_t)b->first->getRow() >= maxrow)
			maxrow=b->first->getRow()+1;

		if ( b->second != NULL && (size_t)b->second->getRow() >=maxrow)
			maxrow=b->second->getRow()+1;

		b++;
	}

	addPrompt(label, field, maxrow);
}


void CursesDialog::addPrompt(CursesLabel *label, Curses *field, size_t atRow)
{
	vector< pair<CursesLabel *, Curses *> >::iterator
		b=prompts.begin(), e=prompts.end();

	// If the new field should precede the existing fields, push them down
	// by one row.

	while (b != e)
	{
		if ( b->first != NULL && (size_t)b->first->getRow() >= atRow)
			b->first->setRow(b->first->getRow()+1);

		if ( b->second != NULL && (size_t)b->second->getRow() >= atRow)
			b->second->setRow(b->second->getRow()+1);
		b++;
	}

	if (label != NULL)
	{
		label->setAlignment(Curses::RIGHT);
		label->setRow(atRow);
		addChild(label);
	}

	if (field != NULL)
	{
		field->setRow(atRow);
		addChild(field);
	}

	prompts.push_back(make_pair(label, field));

	int w;

	if (label != NULL)
	{
		w=label->getWidth();

		if (w > max_label_width)
			max_label_width=w;
	}

	if (field != NULL)
	{
		w=field->getWidth();

		if (w > max_field_width)
			max_field_width=w;
	}

	b=prompts.begin();
	e=prompts.end();

	while (b != e)
	{
		if (b->first != NULL)
			b->first->setCol(max_label_width);
		if (b->second != NULL)
			b->second->setCol(max_label_width);
		b++;
	}
}

void CursesDialog::delPrompt(CursesLabel *label)
{
	vector< pair<CursesLabel *, Curses *> >::iterator
		b=prompts.begin(), e=prompts.end();

	while (b != e)
	{
		if (b->first == label)
		{
			delPrompt(b, b->first->getRow());
			return;
		}
		b++;
	}
}

void CursesDialog::delPrompt(Curses *field)
{
	vector< pair<CursesLabel *, Curses *> >::iterator
		b=prompts.begin(), e=prompts.end();

	while (b != e)
	{
		if (b->second == field)
		{
			delPrompt(b, b->second->getRow());
			return;
		}
		b++;
	}
}

void CursesDialog::delPrompt(vector< pair<CursesLabel *, Curses *> >::iterator
			     p, int row)
{
	prompts.erase(p);

	vector< pair<CursesLabel *, Curses *> >::iterator
		b=prompts.begin(), e=prompts.end();

	while (b != e)
	{
		if (b->first && b->first->getRow() > row)
			b->first->setRow(b->first->getRow()-1);

		if (b->second && b->second->getRow() > row)
			b->second->setRow(b->second->getRow()-1);
		b++;
	}
}

bool CursesDialog::writeText(const char *text, int row, int col,
			       const CursesAttr &attr) const
{
	if (draw_flag)
		return CursesContainer::writeText(text, row, col, attr);
	return false;
}

bool CursesDialog::writeText(const std::vector<unicode_char> &text,
			     int row, int col,
			     const Curses::CursesAttr &attr) const
{
	if (draw_flag)
		CursesContainer::writeText(text, row, col, attr);
	return false;
}


void CursesDialog::deleteChild(Curses *child)
{
	vector< pair<CursesLabel *, Curses *> >::iterator
		b=prompts.begin(), e=prompts.end();

	while (b != e)
	{
		if ( b->first != NULL &&
		     ((Curses *)b->first) == child)
			b->first=NULL;

		if ( b->second != NULL &&
		     ((Curses *)b->second) == child)
			b->second=NULL;
		b++;
	}

	CursesContainer::deleteChild(child);
}

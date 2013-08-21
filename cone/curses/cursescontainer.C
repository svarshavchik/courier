/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/


#include "cursescontainer.H"

using namespace std;

CursesContainer::CursesContainer(CursesContainer *parent)
	: Curses(parent), drawIndex(0)
{
}

int CursesContainer::getWidth() const
{
	vector<Child>::const_iterator b=children.begin(), e=children.end();

	int maxw=0;

	while (b != e)
	{
		const Child &c= *b++;

		// If the child's alignment is parent's center, child's
		// getColAligned() will call this->getWidth() again!
		// Prevent this infinite loop by considering child's
		// position at column 0

		int w= c.child->getAlignment() == PARENTCENTER
			? 0:c.child->getColAligned();

		w += c.child->getWidth();

		if (w > maxw)
			maxw=w;
	}
	return maxw;
}

int CursesContainer::getHeight() const
{
	vector<Child>::const_iterator b=children.begin(), e=children.end();

	int maxh=0;

	while (b != e)
	{
		const Child &c= *b++;

		int h=c.child->getRowAligned() + c.child->getHeight();

		if (h > maxh)
			maxh=h;
	}
	return maxh;
}

void CursesContainer::addChild(Curses *child)
{
	if (child->getParent())
		child->getParent()->deleteChild(child);

	children.push_back(Child(child));

	child->setParent(this);
}

void CursesContainer::deleteChild(Curses *child)
{
	vector<Child>::iterator b=children.begin(), e=children.end();

	while (b != e)
	{
		Child &c= *b;

		if (c.child == child)
		{
			child->setParent(0);
			children.erase(b, b+1);
			break;
		}
		b++;
	}
}

CursesContainer::~CursesContainer()
{
	vector<Child>::iterator b=children.begin(), e=children.end();

	while (b != e)
		(*b++).child->setParent(0);
}

Curses *CursesContainer::getDialogChild() const
{
	vector<Child>::const_iterator b, e;

	b=children.begin();
	e=children.end();

	while (b != e)
	{
		Curses *p=(*b++).child;

		if (p->isDialog())
			return p;
	}

	return NULL;
}

void CursesContainer::draw()
{
	vector<Child>::iterator b, e;

	Curses *p=getDialogChild();

	if (p)
	{
		p->draw();
		return;
	}

	b=children.begin();
	e=children.end();

	while (b != e)
		(*b++).child->draw();
}

void CursesContainer::erase()
{
	vector<Child>::iterator b, e;

	Curses *p=getDialogChild();

	if (p)
	{
		p->erase();
		return;
	}

	b=children.begin();
	e=children.end();

	while (b != e)
		(*b++).child->erase();
}

void CursesContainer::resized()
{
	vector<Child>::iterator b=children.begin(), e=children.end();

	while (b != e)
		(*b++).child->resized();
}

Curses *CursesContainer::getNextFocus()
{
	if (children.size() == 0)
		return Curses::getNextFocus();

	return children[0].child;
}

Curses *CursesContainer::getPrevFocus()
{
	size_t n=children.size();

	if (n == 0)
		return Curses::getNextFocus();
	--n;

	return children[n].child;
}

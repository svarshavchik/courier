/*
** Copyright 2002, S. Varshavchik.
**
** See COPYING for distribution information.
*/

#ifndef cursescontainer_H
#define cursescontainer_H

#include "../curses/curses_config.h"
#include "mycurses.H"

#include <wchar.h>
#include <vector>

////////////////////////////////////////////////////////////////////////
//
// CursesContainer is a superclass for all Curses objects that contain
// other Curses object.

class CursesContainer : public Curses {

	class Child {
	public:
		Curses *child;

		Child(Curses *childArg) : child(childArg)
		{
		}
	};

	std::vector<Child> children;

public:
	friend class Curses;

	CursesContainer(CursesContainer *parent=0);
	~CursesContainer();

	// getWidth()/getHeight() computes the largest rectangle that
	// contains all current children.

	virtual int getWidth() const override;
	virtual int getHeight() const override;

	virtual Curses *getDialogChild() const override;

	// draw/erase recursively invoke draw/erase methods of all children,
	// unless a child CursesObject's isDialog() method returns true, in
	// which case only that child's draw/erase method is called.

	virtual void draw() override;
	virtual void erase() override;

	// The default resized() method recursively invokes the resized()
	// method of all children.

	virtual void resized() override;

	// The constructor and the destructor of a Curses object automatically
	// calls addChild/deleteChild to register the new child process.

	virtual void addChild(Curses *child);
	virtual void deleteChild(Curses *child);

	// The default implementation of getNextFocus()/getPrevFocus()
	// return the pointer to the first or the last child.

	virtual Curses *getNextFocus() override;
	virtual Curses *getPrevFocus() override;
};


#endif

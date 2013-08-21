/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesbutton_H
#define cursesbutton_H

#include "mycurses.H"
#include "curseslabel.H"

////////////////////////////////////////////////////////////////////////
//
// A plain, garden variety, button.  Centered, or right-aligned, perhaps.
//
// CursesButton simply subclasses CursesLabel, makes it focusable, and
// overrides WriteText.
//

class CursesButton : public CursesLabel {

	int toggleButton;

	std::string buttonName;

public:
	// Button style:

	enum Style {
		NORMAL,
		TOGGLE,
		MENU
	};

private:
	Style currentStyle;
public:
	CursesButton(CursesContainer *parent,
		     std::string textArg, int toggle=0);
	~CursesButton();

	void setStyle(Style);
	void draw();

	bool isFocusable();
	void focusGained();
	void focusLost();
	void setText(std::string textArg);

	int getCursorPosition(int &r, int &c);

	int getWidth() const;

	bool processKeyInFocus(const Curses::Key &key);
	virtual void clicked();

	int getSelected() { return toggleButton < 0; }
	void setToggled(bool flag);
};

//////////////////////////////////////////////////////////////////////////
//
// Instead of subclassing CursesButton, here's a template to have it be a
// member of another class.  Typical usage:
//
// class X {
//
//    CursesButtonRedirect<X> button1;
//
//    void button1clicked();
// } ;
//
// X::X()
// {
//      button1=this;
//      button1=&X::button1clicked;
// }

template<class T> class CursesButtonRedirect : public CursesButton {

	T *myClass;
	void (T::*mymethod)();
public:
	CursesButtonRedirect(CursesContainer *parent,
			     std::string textArg, int toggle=0)
		: CursesButton(parent, textArg, toggle)
	{
	}

	~CursesButtonRedirect()
	{
	}

	void operator=(T *p) { myClass=p; }
	void operator=(void (T::*p)()) {mymethod=p; }

	void clicked() { (myClass->*mymethod)(); }

	T *getObject() const { return myClass; }
};

#endif

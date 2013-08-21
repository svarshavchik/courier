/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef curseschoicebutton_H
#define curseschoicebutton_H

#include "mycurses.H"
#include "cursesbutton.H"

#include <vector>

////////////////////////////////////////////////////////////////////////
//
// A button that cycles through a small list of options, each time it
// is "clicked".
//

class CursesChoiceButton : public CursesButton {

	std::vector<std::string> optionList;
	size_t selectedOption;

public:
	CursesChoiceButton(CursesContainer *parent);
	~CursesChoiceButton();

	void setOptions(const std::vector<std::string> &optionListArg,
			size_t initialOption);

	size_t getSelectedOption() const { return selectedOption; }

	void clicked();
};

//////////////////////////////////////////////////////////////////////////
//
// Instead of subclassing CursesChoiceButton, here's a template to have it be a
// member of another class.  Typical usage:
//
// class X {
//
//    CursesChoiceButtonRedirect<X> button1;
//
//    void button1clicked();
// } ;
//
// X::X()
// {
//      button1=this;
//      button1=&X::button1clicked;
// }

template<class T> class CursesChoiceButtonRedirect
	: public CursesChoiceButton {

	T *myClass;
	void (T::*mymethod)();
public:
	CursesChoiceButtonRedirect(CursesContainer *parent)
		: CursesChoiceButton(parent)
	{
	}

	~CursesChoiceButtonRedirect()
	{
	}

	void operator=(T *p) { myClass=p; }
	void operator=(void (T::*p)()) {mymethod=p; }

	void clicked()
	{
		CursesChoiceButton::clicked();
		(myClass->*mymethod)();
	}
};

#endif

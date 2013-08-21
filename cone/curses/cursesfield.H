/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesfield_H
#define cursesfield_H

#include "mycurses.H"
#include "cursesflowedline.H"
#include <wchar.h>
#include <vector>

///////////////////////////////////////////////////////////////////////////
//
// An editable field.
//

class CursesField : public Curses {

	editablewidechar text;		// Editable text.

	void getbeforeafter(widecharbuf &, widecharbuf &);

	void setselectpos();

	size_t width;			// Field width
	size_t maxlength;		// Maximum text length

	size_t shiftoffset;		// First text char shown
	size_t selectpos;		// Start of selection position

	unicode_char passwordChar;	// password overlay character
	bool yesnoField;		// True if yes/no field.

	std::vector<unicode_char> optionField; // One character list of options

	std::vector<std::pair<std::string, std::string> > optionHelp; // Option help keys
	// This field is of primary use by cursesstatusbar

	bool isUnderlined;
protected:
	Curses::CursesAttr attribute;
public:
	static std::vector<unicode_char> yesKeys, noKeys;
	static unicode_char yankKey, clrEolKey;

	static std::list<CursesFlowedLine> cutBuffer; // for cut/paste.

	CursesField(CursesContainer *parent,
		    size_t widthArg=20,
		    size_t maxlengthArg=255,
		    std::string initValue="");
	~CursesField();

	void setUnderlined(bool flag) { isUnderlined=flag; }
	void setRow(int row);
	void setCol(int col);
	void setText(std::string textArg);
	void setAttribute(Curses::CursesAttr attr);
	void setCursorPos(size_t o);
	std::string getText() const;	// Return entered text

	int getWidth() const;		// Our width is known
	int getHeight() const;		// We're one row high

	void setWidth(int);

	void setPasswordChar(unicode_char ch='*') { passwordChar=ch; }
	void setYesNo() { yesnoField=true; }
	void setOptionField(const std::vector<unicode_char> &vec)
	{
		optionField=vec;
	}

	void setOptionHelp(const std::vector< std::pair<std::string, std::string> > &help);

	const std::vector< std::pair<std::string, std::string> > &getOptionHelp() const
	{
		return optionHelp;
	}

	void draw();

	int getCursorPosition(int &row, int &col);
	bool processKeyInFocus(const Key &key);

	bool isFocusable();	// Yes we are
	void focusGained();	// Select old text automatically at entry.
	void focusLost();	// Unselect any selected text at exit.

	void erase();
private:
	void left();
	void right();
};

//////////////////////////////////////////////////////////////////////////
//
// Instead of subclassing a CursesField, provide a template to make it easier
// to have CursesField be a member of another object.  Typical usage:
//
// class X {
//
//    CursesFieldRedirect<X> field1;
//    CursesFieldRedirect<X> field2;
//
//    void field1Enter(); // ENTER key pressed in field1
//    void field2Enter(); // ENTER key pressed in field2
//
// };
//
//
// X::X()
// {
//
//       field1=this;
//       field2=this;
//
//       field1= &X::field1Enter;
//       field2= &X::field2Enter;
// }
//

template<class T> class CursesFieldRedirect : public CursesField {

public:
	T *myClass;
	void (T::*myMethod)(void);

	CursesFieldRedirect(CursesContainer *parent,
			    size_t widthArg=20,
			    size_t maxlengthArg=255,
			    std::string initValue="")
		: CursesField(parent, widthArg, maxlengthArg, initValue),
		  myClass(0), myMethod(0)
	{
	}

	~CursesFieldRedirect()
	{
	}

	void operator=(T *p) { myClass=p; }
	void operator=( void (T::*p)(void) ) { myMethod=p; }

	bool processKeyInFocus(const Key &key)
	{
		if (key == Key::ENTER && myClass && myMethod)
		{
			(myClass->*myMethod)();
			return true;
		}

		return CursesField::processKeyInFocus(key);
	}
};

#endif

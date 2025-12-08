/*
** Copyright 2002-2011, S. Varshavchik.
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

	char32_t passwordChar;	// password overlay character
	bool yesnoField;		// True if yes/no field.

	std::u32string optionField; // One character list of options

	std::vector<std::pair<std::string, std::string> > optionHelp; // Option help keys
	// This field is of primary use by cursesstatusbar

	bool isUnderlined;
protected:
	Curses::CursesAttr attribute;
public:
	static std::u32string yesKeys, noKeys;
	static char32_t yankKey, clrEolKey;

	static std::list<CursesFlowedLine> cutBuffer; // for cut/paste.

	CursesField(CursesContainer *parent,
		    size_t widthArg=20,
		    size_t maxlengthArg=255,
		    std::string initValue="");
	~CursesField();

	void setUnderlined(bool flag) { isUnderlined=flag; }
	void setRow(int row) override;
	void setCol(int col) override;
	void setText(std::string textArg);
	void setAttribute(Curses::CursesAttr attr);
	void setCursorPos(size_t o);
	std::string getText() const;	// Return entered text

	int getWidth() const override;		// Our width is known
	int getHeight() const override;		// We're one row high

	void setWidth(int);

	void setPasswordChar(char32_t ch='*') { passwordChar=ch; }
	void setYesNo() { yesnoField=true; }
	void setOptionField(const std::u32string &vec)
	{
		optionField=vec;
	}

	void setOptionHelp(const std::vector< std::pair<std::string, std::string> > &help);

	const std::vector< std::pair<std::string, std::string> > &getOptionHelp() const
	{
		return optionHelp;
	}

	void draw() override;

	int getCursorPosition(int &row, int &col) override;
	bool processKeyInFocus(const Key &key) override;

	bool isFocusable() override;	// Yes we are
	void focusGained() override;	// Select old text automatically at entry.
	void focusLost() override;	// Unselect any selected text at exit.

	void erase() override;
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

	bool processKeyInFocus(const Key &key) override
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

/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef curseslabel_H
#define curseslabel_H

#include "mycurses.H"

////////////////////////////////////////////////////////////////////////
//
// A plain, garden variety, label.  Centered, or right-aligned, perhaps.
//
// A lot of work, just for this

class CursesLabel : public Curses {

	std::u32string utext;

	size_t w;

	void setutext(const std::string &textArg);

protected:
	Curses::CursesAttr attribute;
public:
	CursesLabel(CursesContainer *parent,
		    std::string textArg,
		    Curses::CursesAttr attributeArg=Curses::CursesAttr());
	~CursesLabel();

	void setRow(int row) override;
	void setCol(int col) override;
	void setAlignment(Alignment alignment) override;
	void setAttribute(Curses::CursesAttr attr);
	virtual void setText(std::string textArg);

	int getWidth() const override;
	int getHeight() const override;

	void draw() override;
	void erase() override;
};

#endif

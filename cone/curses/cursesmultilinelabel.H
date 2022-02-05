/*
** Copyright 2004-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesmultilinelabel_H
#define cursesmultilinelabel_H

#include "mycurses.H"

////////////////////////////////////////////////////////////////////////
//
// A multiline label that's word-wrapped to a given width.
// A plain, garden variety, label.  Centered, or right-aligned, perhaps.
//
// A lot of work, just for this

class CursesMultilineLabel : public Curses {

	std::u32string text;

	std::vector< std::u32string > lines;

protected:
	int width;

	Curses::CursesAttr attribute;
public:
	void init(); // Subclass must call init() in constructor.

	CursesMultilineLabel(CursesContainer *parent,
			     std::string textArg,
			     Curses::CursesAttr attributeArg
			     =Curses::CursesAttr());
	~CursesMultilineLabel();

	void setRow(int row) override;
	void setCol(int col) override;
	void resized() override;
	void setAlignment(Alignment alignment) override;
	void setAttribute(Curses::CursesAttr attr);
	virtual void setText(std::string textArg);

	int getWidth() const override;
	int getHeight() const override;

	void setWidth(int w);

	void draw() override;
	void erase() override;
};

#endif

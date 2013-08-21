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

	std::vector<unicode_char> text;

	std::vector< std::vector<unicode_char> > lines;

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

	void setRow(int row);
	void setCol(int col);
	void resized();
	void setAlignment(Alignment alignment);
	void setAttribute(Curses::CursesAttr attr);
	virtual void setText(std::string textArg);

	int getWidth() const;
	int getHeight() const;

	void setWidth(int w);

	void draw();
	void erase();
};

#endif

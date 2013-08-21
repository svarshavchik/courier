/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursestitlebar_H
#define cursestitlebar_H

#include "mycurses.H"

#include <string>

//
// The title bar - moves itself to the first line on the parent screen,
// centers the title.
//

class CursesTitleBar : public Curses {

	std::string title;
	std::string leftTitle;
	std::string rightTitle;

	CursesAttr attribute;

public:
	CursesTitleBar(CursesContainer *parent, std::string title);
	~CursesTitleBar();

	void setTitles(std::string left, std::string right);

	void setAttribute(CursesAttr attr);

	int getWidth() const;
	int getHeight() const;
	void draw();
};

#endif

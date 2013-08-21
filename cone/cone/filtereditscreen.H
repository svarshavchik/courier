/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef filtereditscreen_H
#define filtereditscreen_H

#include "config.h"
#include "filter.H"

#include "curses/cursesdialog.H"
#include "curses/curseslabel.H"
#include "curses/cursesfield.H"
#include "curses/cursesbutton.H"
#include "curses/curseskeyhandler.H"
#include "curses/curseschoicebutton.H"

#include <list>

class Filter::editScreen
	: public CursesDialog,
	  public CursesKeyHandler {

	class Button : public CursesButton {
		Filter::editScreen *myParent;

	public:
		bool isAddButton;

		std::list<Filter::editScreen::Button *>::iterator myPos;

		Filter::Step step; // Filtering step this button represents.

		Button(Filter::editScreen *parentArg, std::string Name,
		       bool isAddButtonArg);
		~Button();
		bool processKeyInFocus(const Key &key);
		void clicked();
	};

	CursesLabel myTitle;

	std::list<Filter::editScreen::Button *> buttons;

	CursesButtonRedirect<Filter::editScreen> saveButton;

	void save();

public:
	editScreen(CursesContainer *parent);
	~editScreen();

	void init(std::string);

	void add();
	void del(std::list<Filter::editScreen::Button *>::iterator);
	void moveup(std::list<Filter::editScreen::Button *>::iterator);
	void movedn(std::list<Filter::editScreen::Button *>::iterator);

	bool doSelectFolder;
	Filter::Step::Type selectFolderFor;

	operator std::string() const;

	bool doSave;
private:
	void add(Filter::Step &newStep);
	void domoveup(std::list<Filter::editScreen::Button *>::iterator);

	// Inherited from CursesKeyHandler:

	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string, std::string> >
		       &list);
};

#endif

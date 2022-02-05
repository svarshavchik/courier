/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesmessagedisplay_H
#define cursesmessagedisplay_H

#include "config.h"
#include "curses/mycurses.H"
#include "curses/curseskeyhandler.H"
#include "mymessage.H"
#include "libmail/objectmonitor.H"

#include <string>
#include <vector>

class CursesMessage;

class CursesMessageDisplay : public Curses, public CursesKeyHandler,
			     public myMessage::FolderUpdateCallback {

	mail::ptr<CursesMessage> messageInfoPtr;

	size_t firstLineShown;
	size_t firstLineToSearch;
	std::string searchString;
	void setFirstLineShown(size_t);
	size_t getFirstLineShown() { return firstLineShown; }

public:
	CursesMessageDisplay(CursesContainer *parent,
			     CursesMessage *messageArg);
	~CursesMessageDisplay();

	void drawLine(size_t lineNum);

	// Inherited from Curses
	int getWidth() const override;
	int getHeight() const override;

	void draw() override;
private:
	void draw2();
public:
	void resized() override;

	bool isFocusable() override;

	bool processKeyInFocus(const Curses::Key &key) override;
	int getCursorPosition(int &row, int &col) override;


	// Inherited from myMessage::FolderUpdateCallback

	void folderUpdated() override;

	// Inherited from CursesKeyHandler

	bool processKey(const Curses::Key &key) override;
	bool listKeys( std::vector< std::pair<std::string, std::string> > &list) override;

};

#endif

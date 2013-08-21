/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef curseshierarchy_H
#define curseshierarchy_H

#include "config.h"

#include "curses/mycurses.H"
#include "curses/curseskeyhandler.H"
#include "hierarchy.H"

#include <string>
#include <vector>

///////////////////////////////////////////////////////////////////////
//
// A folder hierarchy shown on a curses screen

class CursesHierarchy : public Curses, public HierarchyDisplay,
			public CursesKeyHandler {

	static int currentRowNum;

	// Helper class used as a Hierarchy iterator that calls the
	// drawErase() method, below.

	class DrawIterator : public Hierarchy::EntryIterator {
		CursesHierarchy *me;
		bool doErase;
		bool all;
	public:
		DrawIterator(CursesHierarchy *meArg,
			     bool doEraseArg,	// PASSTHROUGH
			     bool allArg);	// Always return true
		~DrawIterator();

		bool visit(Hierarchy::Folder *);
		bool visit(Hierarchy::Server *);
	};

	//
	// Helper class processes key events for currently highlighted folder

	class KeyHandlerIterator : public Hierarchy::EntryIterator {
		CursesHierarchy *me;
		const Curses::Key &key;
		bool consumed;
	public:
		KeyHandlerIterator(CursesHierarchy *meArg,
				   const Curses::Key &keyArg);
		~KeyHandlerIterator();

		bool isConsumed() const { return consumed; }

		bool visit(Hierarchy::Folder *);
		bool visit(Hierarchy::Server *);

	};

public:
	friend class DrawIterator;

	bool selectingFolder;
	// True when hierarchy screen open to select a folder.

	mail::folder *folderSelected; // The selected folder
	myServer *serverSelected; // The selected server

	CursesHierarchy(Hierarchy *h, CursesContainer *parent);
	~CursesHierarchy();

	// Subclassed

	void requestFocus();

	// Inherited from HierarchyDisplay:

	void drawEraseBelow(Hierarchy::Entry *e, bool doErase);
	bool drawErase(Hierarchy::Folder *folder, bool doErase);
	bool drawErase(Hierarchy::Server *server, bool doErase);
	void visible(Hierarchy::Entry *e);


private:
	bool drawErase(std::string str1, int col1,
		       std::string str2, int col2,
		       Hierarchy::Entry *e, bool doErase);

public:
	// Inherited from Curses:

	int getWidth() const;
	int getHeight() const;
	void draw();
	bool isFocusable();
	bool processKeyInFocus(const Curses::Key &key);
	int getCursorPosition(int &row, int &col);

	bool processKeyInFocus(Hierarchy::Folder *f, const Curses::Key &key);
	bool processKeyInFocus(Hierarchy::Server *s, const Curses::Key &key);

	void openSubFolders(Hierarchy::Folder *f);
	void closeSubFolders(Hierarchy::Folder *f);

private:

	void draw(Hierarchy::Entry *e, bool doErase=false);

public:
	static bool autologin(myServer *);
private:
	// Inherited from CursesKeyHandler:

	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string, std::string> > &list);

	Hierarchy::Folder *getCurrentFolder();
public:
	void processDeletedFolder(Hierarchy::Folder *, Hierarchy::Folder *,
				  const mail::folder *);
};

#endif

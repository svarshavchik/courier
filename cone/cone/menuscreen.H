/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef menuscreen_h
#define menuscreen_h

#include "config.h"
#include "curses/curseslabel.H"
#include "curses/cursesdialog.H"
#include "curses/cursesbutton.H"
#include "curses/cursescontainer.H"
#include "curses/curseskeyhandler.H"

#include "previousscreen.H"
#include "globalkeys.H"
#include "gettext.H"

#include <vector>
#include <string>

//////////////////////////////////////////////////////////////////////////
//
// Generic code for a menu screen: a list of options, and corresponding
// keyboard shortcuts.
//
// MenuScreen needs to be subclassed.  The superclass's constructor
// should instantiate a list of menu items (MenuEntry vector), and a list
// of corresponding keyboard actions (ShortcutKey vector), then invoke
// init().
//
// The superclass should also instantiate a vector of MenuAction() objects,
// which represent the actual code callbacks.
//
//

class MenuScreen : public CursesContainer,
		   public CursesKeyHandler,
		   public PreviousScreen {

public:

	// Every action that can be invoked from the menu screen:

	class MenuAction {
	public:
		MenuAction();
		virtual ~MenuAction();
		virtual void go()=0;
	};

	// An entry on the menu screen:

	class MenuEntry {
	public:
		MenuEntry(std::string menuNameArg, MenuAction &actionArg);
		virtual ~MenuEntry();

		std::string menuName;
		MenuAction *action;
		void goStub();
	};

	// A keyboard shortcut for a menu entry.

	class ShortcutKey {
	public:
		MenuEntry *menuEntry;
		std::string keyname;
		std::string keydescr;

		Gettext::Key *shortcutKey;

		ShortcutKey(MenuEntry *, std::string,
			    std::string, Gettext::Key &);
		~ShortcutKey();
	};

	MenuScreen(CursesContainer *parent,
		   std::string menuName,
		   void (*previousScreenArg)(void *),
		   GlobalKeys::CurrentScreen currentScreenArg);
	virtual ~MenuScreen();

	// Superclass needs to define these functions:
	virtual std::vector<MenuEntry> &getMenuVector()=0;
	virtual std::vector<ShortcutKey> &getKeyVector()=0;

	void requestFocus(); // Put focus on the first button.

protected:
	void init();
private:
	GlobalKeys::CurrentScreen currentScreen;

	CursesLabel title;
	CursesDialog menuDialog;

	// Each menu entry is a button.  We take care of instantiating and
	// destroying the buttons.

	std::vector<CursesButtonRedirect<MenuEntry> *> buttons;

	bool processKey(const Curses::Key &key);
	bool listKeys( std::vector< std::pair<std::string, std::string> > &list);

public:

	// A subclass of MenuAction that implements a static function callback.

	class MenuActionStaticCallback : public MenuAction {

		void (*func)(void);

	public:
		MenuActionStaticCallback(void (*funcArg)(void));
		~MenuActionStaticCallback();
		void go();
	};

	// A template that generates callbacks to a particular method

	template<class T> class MenuActionCallback : public MenuAction {

		T *t;
		void (T::*func)(void);

	public:
		MenuActionCallback(T *tArg, void (T::*funcArg)(void))
			: t(tArg), func(funcArg)
			{
			}

		~MenuActionCallback()
			{
			}

		void go()
			{
				(t->*func)();
			}
	};

};

#endif

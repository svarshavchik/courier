/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"

#include "menuscreen.H"
#include "colors.H"

#include <errno.h>

using namespace std;

extern struct CustomColor color_misc_menuTitle;
extern struct CustomColor color_misc_menuOption;

// Default constructors and destructors

MenuScreen::MenuAction::MenuAction()
{
}

MenuScreen::MenuAction::~MenuAction()
{
}

MenuScreen::MenuEntry::MenuEntry(string menuNameArg, MenuAction &actionArg)
	: menuName(menuNameArg), action(&actionArg)
{
}

MenuScreen::MenuEntry::~MenuEntry()
{
}

void MenuScreen::MenuEntry::goStub()
{
	action->go();
}

MenuScreen::ShortcutKey::ShortcutKey(MenuEntry *menuEntryArg,
				     string keynameArg, string keydescrArg,
				     Gettext::Key &shortcutKeyArg)
	: menuEntry(menuEntryArg), keyname(keynameArg), keydescr(keydescrArg),
	  shortcutKey(&shortcutKeyArg)
{
}

MenuScreen::ShortcutKey::~ShortcutKey()
{
}

MenuScreen::MenuScreen(CursesContainer *parent,
		       string menuName,
		       void (*previousScreenArg)(void *),
		       GlobalKeys::CurrentScreen currentScreenArg)
	: CursesContainer(parent),
	  CursesKeyHandler(PRI_SCREENHANDLER),
	  PreviousScreen( previousScreenArg, NULL),
	  currentScreen(currentScreenArg),
	  title(this, menuName),
	  menuDialog(this)
	
{
	Curses::CursesAttr titleAttr;

	titleAttr.setFgColor(color_misc_menuTitle.fcolor);
	title.setAttribute(titleAttr);
}

// After the superclass is done instantiating, it calls init() to build the
// actual screen.

void MenuScreen::init()
{
	setRow(1);
	setAlignment(Curses::PARENTCENTER);
	title.setAlignment(Curses::PARENTCENTER);
	title.setRow(0);
	menuDialog.setRow(2);

	vector<MenuEntry> &menuVector=getMenuVector();

	vector<MenuEntry>::iterator b=menuVector.begin(),
		e=menuVector.end();

	size_t row=0;

	Curses::CursesAttr bbAttr;

	bbAttr.setFgColor(color_misc_menuOption.fcolor);

	while (b != e)
	{
		CursesButtonRedirect<MenuEntry> *bb=
			new CursesButtonRedirect<MenuEntry>(NULL, b->menuName);

		if (!bb)
			throw strerror(errno);

		try {
			buttons.push_back(bb);
		} catch (...) {
			delete bb;
			throw;
		}

		*bb= &*b;
		*bb= &MenuEntry::goStub;

		bb->setAttribute(bbAttr);
		bb->setStyle(CursesButton::MENU);
		menuDialog.addPrompt(NULL, bb, row);
		row += 2;

		++b;
	}
}


void MenuScreen::requestFocus()
{
	if (buttons.size() > 0)
		buttons[0]->requestFocus();
}

MenuScreen::~MenuScreen()
{
	vector<CursesButtonRedirect<MenuEntry> *>::iterator
		b=buttons.begin(),
		e=buttons.end();

	while (b != e)
	{
		if (*b)
			delete *b;
		*b=NULL;
		++b;
	}
}

bool MenuScreen::listKeys( vector< pair<string, string> > &list)
{
	GlobalKeys::listKeys(list, currentScreen);

	vector<ShortcutKey>::iterator b, e;

	vector<ShortcutKey> &keyVector=getKeyVector();

	b=keyVector.begin();
	e=keyVector.end();

	while (b != e)
	{
		list.push_back(make_pair( b->keyname, b->keydescr));
		++b;
	}

	return true;
}

bool MenuScreen::processKey(const Curses::Key &key)
{
	if (GlobalKeys::processKey(key, currentScreen, NULL))
		return true;

	vector<ShortcutKey> &keyVector=getKeyVector();
	vector<ShortcutKey>::iterator b, e;

	b=keyVector.begin();
	e=keyVector.end();

	while (b != e)
	{
		if (key == *b->shortcutKey)
		{
			vector<CursesButtonRedirect<MenuEntry> *>::iterator
				ib=buttons.begin(), ie=buttons.end();

			while (ib != ie)
			{
				if ( (*ib)->getObject()->menuName ==
				     b->menuEntry->menuName)
				{
					(*ib)->requestFocus();
					break;
				}
				++ib;
			}
			b->menuEntry->action->go();
			return true;
		}

		++b;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////
//
// An implementation of MenuAction that represents a static callback.

MenuScreen::MenuActionStaticCallback::MenuActionStaticCallback(void (*funcArg)
							       (void))
	: func(funcArg)
{
}

MenuScreen::MenuActionStaticCallback::~MenuActionStaticCallback()
{
}

void MenuScreen::MenuActionStaticCallback::go()
{
	(*func)();
}

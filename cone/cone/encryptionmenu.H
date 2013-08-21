/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef encryptionmenu_h
#define encryptionmenu_h

#include "config.h"
#include "menuscreen.H"

class EncryptionMenu : public MenuScreen {

public:
	EncryptionMenu(CursesContainer *);
	~EncryptionMenu();
	std::vector<MenuEntry> &getMenuVector();
	std::vector<ShortcutKey> &getKeyVector();

	std::vector<MenuScreen::MenuActionCallback<EncryptionMenu> > myAction;
	std::vector<MenuEntry> myEntries;
	std::vector<ShortcutKey> myKeys;

	void dummy();
	void mainmenu_s();
	void genkey_s();
	void delkey_s();
	void signkey_s();
	void editkey_s();

	class NewKey;

	class DumpFuncHelper;
};

#endif

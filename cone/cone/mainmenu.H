/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef mainmenu_h
#define mainmenu_h

#include "config.h"

#include "menuscreen.H"

#include <vector>
#include <string>


//
// The main menu screen
//

class MainMenuScreen : public MenuScreen {
public:

	// Predefined network-based mail account types

	struct AccountType {

		std::string method;
		std::string secureMethod;

		std::string name;
	};

	static struct AccountType imapAccount;
	static struct AccountType pop3Account;
	static struct AccountType pop3MaildropAccount;
	static struct AccountType nntpAccount;

	MainMenuScreen(CursesContainer *parent);
	~MainMenuScreen();

	static void listfolders_s();
	static void setupscreen_s();
	static void addaccount_s();
	static void addressbook_s();
	static void certificates_s();
	static void writescreen_s();
	static void masterpassword_s();
	static void encryptionmenu_s();


private:

	std::vector<MenuEntry> &getMenuVector();
	std::vector<ShortcutKey> &getKeyVector();

	std::vector<MenuScreen::MenuActionStaticCallback> myAction;
	std::vector<MenuEntry> myEntries;
	std::vector<ShortcutKey> myKeys;

};

#endif

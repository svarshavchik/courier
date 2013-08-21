/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef globalkeys_H
#define globalkeys_H

#include "config.h"
#include "curses/mycurses.H"

#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//
// Keys common to most screens.  Keyhandlers for those screens invoke
// processKey and listKeys defined by this class as default handlers.

class GlobalKeys {

public:

	enum CurrentScreen {
		LISTFOLDERS,
		MAINMENU,
		ENCRYPTIONMENU,
		MESSAGESCREEN,
		FOLDERINDEXSCREEN,
		ATTACHMENTSCREEN,
		WRITEMESSAGESCREEN,
		ADDRESSBOOKLISTSCREEN,
		CERTIFICATESCREEN,
		CHOOSECERTIFICATESCREEN,
		ADDRESSBOOKINDEXSCREEN,
		ADDLDAPADDRESSBOOKSCREEN,
		QUITSCREEN
	};

	static bool processKey(const Curses::Key &key,
			       CurrentScreen currentScreen,
			       class myServer *serverPtr);

	static void listKeys(std::vector< std::pair<std::string,
			     std::string> > &list,
			     CurrentScreen currentScreen);


	static bool quitNoPrompt;
};

#endif

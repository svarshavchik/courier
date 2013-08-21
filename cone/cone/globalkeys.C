/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "globalkeys.H"
#include "myserver.H"
#include "myserverpromptinfo.H"
#include "myfolder.H"
#include "mymessage.H"
#include "gettext.H"
#include "previousscreen.H"
#include "init.H"
#include "curses/cursesstatusbar.H"

using namespace std;

bool GlobalKeys::quitNoPrompt=false;

extern void hierarchyScreen(void *);
extern void mainMenu(void *);
extern void listaddressbookScreen(void *);
extern void quitScreen(void *);

extern Gettext::Key key_LESSTHAN;

extern Gettext::Key key_LISTFOLDERS;
extern Gettext::Key key_MAINMENU;
extern Gettext::Key key_WRITE;
extern Gettext::Key key_QUIT;

static struct {
	GlobalKeys::CurrentScreen screenCode;
	Gettext::Key &key;
	const char *keyname, *keydescr;

	void (*openScreen)(void *);
} keyActions[]= {
	{
		GlobalKeys::LISTFOLDERS,
		key_LISTFOLDERS,
		N_("LISTFOLDERS_K:L"),
		N_("Folder List"),
		&hierarchyScreen
	},

	{
		GlobalKeys::MAINMENU,
		key_MAINMENU,
		N_("MAINMENU_K:M"),
		N_("Main Menu"),
		&mainMenu
	},

	{
		GlobalKeys::WRITEMESSAGESCREEN,
		key_WRITE,
		N_("WRITE_K:W"),
		N_("Write Msg"),
		NULL
	},

	{
		GlobalKeys::QUITSCREEN,
		key_QUIT,
		N_("QUIT_K:Q"),
		N_("Quit"),
		&quitScreen
	}

};

bool GlobalKeys::processKey(const Curses::Key &key,
			    CurrentScreen currentScreen,
			    myServer *serverPtr)
{

	size_t i;

	if (key == key_LESSTHAN)
	{
		if (currentScreen == FOLDERINDEXSCREEN)
		{
			Curses::keepgoing=false;
			myServer::nextScreen=&hierarchyScreen;
			myServer::nextScreenArg=NULL;
			return true;
		}

		if (currentScreen == ADDRESSBOOKLISTSCREEN)
		{
			Curses::keepgoing=false;
			myServer::nextScreen=&mainMenu;
			myServer::nextScreenArg=NULL;
			PreviousScreen::previousScreen();
			return true;
		}

		if (currentScreen == CERTIFICATESCREEN)
		{
			Curses::keepgoing=false;
			myServer::nextScreen=&mainMenu;
			myServer::nextScreenArg=NULL;
			PreviousScreen::previousScreen();
			return true;
		}

		if (currentScreen == ADDRESSBOOKINDEXSCREEN)
		{
			Curses::keepgoing=false;
			myServer::nextScreen=&listaddressbookScreen;
			myServer::nextScreenArg=NULL;
			return true;
		}

		if (currentScreen == ADDLDAPADDRESSBOOKSCREEN)
		{
			Curses::keepgoing=false;
			myServer::nextScreen=&listaddressbookScreen;
			myServer::nextScreenArg=NULL;
			return true;
		}


	}

	for (i=0; i<sizeof(keyActions)/sizeof(keyActions[0]); i++)
		if (currentScreen != keyActions[i].screenCode)
			if (key == keyActions[i].key)
			{
				if (keyActions[i].screenCode ==
				    WRITEMESSAGESCREEN)
				{
					// Special logic for this key.

					mail::ptr<myFolder> f=
						serverPtr ?
						serverPtr->currentFolder:
						NULL;

					if (myMessage::checkInterrupted(false))
					{
						myMessage::newMessage(f.isDestroyed()
								      ? NULL
								      :f->getFolder(),
								      serverPtr
								      , "");
					}
					return true;
				}

				if (keyActions[i].screenCode == QUITSCREEN &&
				    !statusBar->prompting() &&
				    !quitNoPrompt)
				{
					myServer::promptInfo really=
						myServer::promptInfo
						(_("Really quit? (Y/N) "))
						.yesno();


					really=myServer::prompt(really);

					if (really.abortflag ||
					    myServer::nextScreen)
					{
						Curses::keepgoing=false;
						return true;
					}

					Curses::keepgoing=true;

					if ( (string)really != "Y")
						return true;
				}

				Curses::keepgoing=false;
				myServer::nextScreen= keyActions[i].openScreen;
				myServer::nextScreenArg=NULL;
				return true;
			}

	return false;
}

void GlobalKeys::listKeys(std::vector< std::pair<std::string,
			  std::string> > &list,
			  CurrentScreen currentScreen)
{
	size_t i;

	for (i=0; i<sizeof(keyActions)/sizeof(keyActions[0]); i++)
		if (currentScreen != keyActions[i].screenCode)
			list.push_back(make_pair(Gettext::keyname(gettext(keyActions[i].keyname)),
						 gettext(keyActions[i].keydescr)));
}

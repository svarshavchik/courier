/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"

#include "curses/cursescontainer.H"
#include "curses/curseslabel.H"
#include "curses/cursesbutton.H"
#include "curses/cursesdialog.H"
#include "curses/cursesmainscreen.H"
#include "curses/cursesstatusbar.H"
#include "curses/cursestitlebar.H"
#include "curses/curseskeyhandler.H"
#include "tcpd/tlspasswordcache.h"

#include "gettext.H"
#include "myserver.H"
#include "myserverpromptinfo.H"
#include "mainmenu.H"
#include "mymessage.H"
#include "passwordlist.H"
#include "init.H"
#include "gpg.H"
#include "libmail/misc.H"

using namespace std;

extern Gettext::Key key_IMAPACCOUNT;
extern Gettext::Key key_INBOXMBOX;
extern Gettext::Key key_NNTPACCOUNT;
extern Gettext::Key key_OTHERMBOX;
extern Gettext::Key key_POP3ACCOUNT;
extern Gettext::Key key_POP3MAILDROPACCOUNT;

extern Gettext::Key key_NEWACCOUNT;
extern Gettext::Key key_ADDRESSBOOK;
extern Gettext::Key key_CERTIFICATES;
extern Gettext::Key key_SETUPSCREEN;
extern Gettext::Key key_MASTERPASSWORD;
extern Gettext::Key key_ENCRYPTIONMENU;

extern void hierarchyScreen(void *);
extern void addAccountScreen(void *);
extern void setupScreen(void *);
extern void listaddressbookScreen(void *);
extern void certificatesScreen(void *);
extern void encryptionMenu(void *);

void mainMenu(void *dummy);

extern myServer *tryCreateAccount(string account, string url,
				  string password, string certificate);



MainMenuScreen::MainMenuScreen(CursesContainer *parent)
	: MenuScreen(parent, _("MAIN MENU"), &mainMenu, GlobalKeys::MAINMENU)
{
	myAction.push_back( MenuActionStaticCallback(&MainMenuScreen::listfolders_s));
	myAction.push_back( MenuActionStaticCallback(&MainMenuScreen::writescreen_s));
	myAction.push_back( MenuActionStaticCallback(&MainMenuScreen::addaccount_s));
	myAction.push_back( MenuActionStaticCallback(&MainMenuScreen::addressbook_s));
	myAction.push_back( MenuActionStaticCallback(&MainMenuScreen::certificates_s));
	myAction.push_back( MenuActionStaticCallback(&MainMenuScreen::setupscreen_s));
	myAction.push_back( MenuActionStaticCallback(&MainMenuScreen::masterpassword_s));

	myAction.push_back( MenuActionStaticCallback(&MainMenuScreen::encryptionmenu_s));

	myEntries.push_back( MenuScreen::MenuEntry(_("L - LIST FOLDERS   "),
						   myAction[0]));
	myEntries.push_back( MenuScreen::MenuEntry(_("W - WRITE MESSAGE  "),
						   myAction[1]));
	myEntries.push_back( MenuScreen::MenuEntry(_("N - NEW ACCOUNT    "),
						   myAction[2]));
	myEntries.push_back( MenuScreen::MenuEntry(_("A - ADDRESS BOOK   "),
						   myAction[3]));
	myEntries.push_back( MenuScreen::MenuEntry(_("C - CERTIFICATES   "),
						   myAction[4]));
	myEntries.push_back( MenuScreen::MenuEntry(_("S - SETUP          "),
						   myAction[5]));
	myEntries.push_back( MenuScreen::MenuEntry(_("P - MASTER PASSWORD"),
						   myAction[6]));

	if (GPG::gpg_installed())
		myEntries.push_back( MenuScreen
				     ::MenuEntry(_("E - ENCRYPTION MENU"),
						 myAction[7]));


	myKeys.push_back( MenuScreen::ShortcutKey(&myEntries[2],
						  Gettext::keyname(_("ADDACCOUNT_K:N")),
						  _("New Account"),
						  key_NEWACCOUNT));

	myKeys.push_back( MenuScreen::ShortcutKey(&myEntries[3],
						  Gettext::keyname(_("ADDRESSBOOK_K:A")),
						  _("Address Book"),
						  key_ADDRESSBOOK));

	myKeys.push_back( MenuScreen::ShortcutKey(&myEntries[4],
						  Gettext::keyname(_("CERTIFICATES_K:C")),
						  _("Certificates"),
						  key_CERTIFICATES));

	myKeys.push_back( MenuScreen::ShortcutKey(&myEntries[5],
						  Gettext::keyname(_("SETUPSCREEN_K:S")),
						  _("Setup"),
						  key_SETUPSCREEN));


	myKeys.push_back( MenuScreen::ShortcutKey(&myEntries[6],
						  Gettext::keyname(_("MASTERPASSWORD_K:P")),
						  _("Master Password"),
						  key_MASTERPASSWORD));


	if (GPG::gpg_installed())
		myKeys.push_back( MenuScreen
				  ::ShortcutKey(&myEntries[7],
						Gettext::keyname(_("ENCRYPTION_K:E")),
						_("Encryption"),
						key_ENCRYPTIONMENU));

	init();
}

MainMenuScreen::~MainMenuScreen()
{
}

vector<MenuScreen::MenuEntry> &MainMenuScreen::getMenuVector()
{
	return myEntries;
}

vector<MenuScreen::ShortcutKey> &MainMenuScreen::getKeyVector()
{
	return myKeys;
}

void MainMenuScreen::encryptionmenu_s()
{
	keepgoing=false;
	myServer::nextScreen= &encryptionMenu;
	myServer::nextScreenArg=NULL;
}

void MainMenuScreen::listfolders_s()
{
	keepgoing=false;
	myServer::nextScreen= &hierarchyScreen;
	myServer::nextScreenArg=NULL;
}

void MainMenuScreen::setupscreen_s()
{
	keepgoing=false;
	myServer::nextScreen= &setupScreen;
	myServer::nextScreenArg=NULL;
}

void MainMenuScreen::addressbook_s()
{
	keepgoing=false;
	myServer::nextScreen= &listaddressbookScreen;
	myServer::nextScreenArg=NULL;
}

void MainMenuScreen::certificates_s()
{
	if (PasswordList::passwordList.hasMasterPasswordFile())
	{
		keepgoing=false;
		myServer::nextScreen= &certificatesScreen;
		myServer::nextScreenArg=NULL;
		return;
	}
	statusBar->beepError();
	statusBar->status(_("Please set up a master password, first"));
}

void MainMenuScreen::writescreen_s()
{
	if (myMessage::checkInterrupted(false))
		myMessage::newMessage(NULL, NULL, "");
}

void MainMenuScreen::masterpassword_s()
{
	if (!tlspassword_init())
	{
		statusBar->beepError();
		statusBar->status(_("Not implemented (OpenSSL 0.9.7 required)")
				  );
		return;
	}

	if (PasswordList::passwordList.hasMasterPasswordFile())
	{
		myServer::promptInfo
			ask(_("Remove master password? (Y/N) "));

		ask=myServer::prompt(ask.yesno());

		if (ask.abortflag)
			return;

		if ((string)ask == "Y")
		{
			if (myServer::certs->certs.empty())
			{
				PasswordList::passwordList
					.setMasterPassword("");
				return;
			}
			statusBar->beepError();
			statusBar->status(_("Please uninstall all certificates before removing the master password"));
			return;
		}

		if (!PasswordList::passwordList.hasMasterPassword())
			return; // Master password file not succesfully open.
	}

	myServer::promptInfo response=
		myServer::prompt(myServer
				 ::promptInfo(_("New master password: "))
				 .password());

	string newPassword=response;

	if (response.abortflag || newPassword.size() == 0)
	{
		statusBar->clearstatus();
		statusBar->status(_("Master password NOT set."));
		statusBar->beepError();
		return;
	}

	response=myServer::prompt(myServer
				  ::promptInfo(_("Enter the password again: "))
				  .password());

	string newPassword2=response;

	if (response.abortflag)
	{
		statusBar->clearstatus();
		statusBar->status(_("Master password NOT set."));
		statusBar->beepError();
		return;
	}

	if ( (string)newPassword2 != newPassword)
	{
		statusBar->clearstatus();
		statusBar->status(_("Passwords don't match!"));
		statusBar->beepError();
		return;
	}

	PasswordList::passwordList
		.setMasterPassword(Gettext::toutf8(newPassword));
}

struct MainMenuScreen::AccountType MainMenuScreen::imapAccount=
	{ "imap", "imaps", _("IMAP") };

struct MainMenuScreen::AccountType MainMenuScreen::pop3Account=
	{ "pop3", "pop3s", _("POP3") };

struct MainMenuScreen::AccountType MainMenuScreen::pop3MaildropAccount=
	{ "pop3maildrop", "pop3maildrops", _("POP3 maildrop") };

struct MainMenuScreen::AccountType MainMenuScreen::nntpAccount=
	{ "nntp", "nntps", _("Netnews") };

bool isMaildir(string f)
{
	string tmp=f + "/tmp/.";
	string cur=f + "/cur/.";
	string newd=f + "/new/.";
	string homedir=mail::homedir();

	if (tmp[0] != '/')
		tmp=homedir + "/" + tmp;
	if (cur[0] != '/')
		cur=homedir + "/" + cur;
	if (newd[0] != '/')
		newd=homedir + "/" + newd;

	if (access(tmp.c_str(), X_OK) == 0 &&
	    access(cur.c_str(), X_OK) == 0 &&
	    access(newd.c_str(), X_OK) == 0)
		return true;
	return false;
}

void MainMenuScreen::addaccount_s()
{
	myServer::promptInfo result=
		myServer::prompt(myServer::promptInfo(_("Account Type: "))
				 .option(key_IMAPACCOUNT,
					 Gettext::keyname(_("IMAP_K:I")),
					 _("IMAP account"))
				 .option(key_POP3ACCOUNT,
					 Gettext::keyname(_("POP3_K:P")),
					 _("POP3 account"))
				 .option(key_POP3MAILDROPACCOUNT,
					 Gettext::keyname(_("POP3MAILDROP_K:M")
							  ),
					 _("POP3 Maildrop"))
				 .option(key_NNTPACCOUNT,
					 Gettext::keyname(_("NNTP_K:N")),
					 _("NetNews account"))
				 .option(key_INBOXMBOX,
					 Gettext::keyname(_("INBOX:S")),
					 _("System mailbox"))
				 .option(key_OTHERMBOX,
					 Gettext::keyname(_("OTHER:O")),
					 _("Other mailbox")));

	if (result.abortflag)
		return;

	unicode_char promptKey=result.firstChar();

	if (promptKey == 0)
		return;

	if (key_IMAPACCOUNT == promptKey)
	{
		Curses::keepgoing=false;
		myServer::nextScreen= &addAccountScreen;
		myServer::nextScreenArg= &imapAccount;
		return;
	}

	if (key_POP3ACCOUNT == promptKey)
	{
		Curses::keepgoing=false;
		myServer::nextScreen= &addAccountScreen;
		myServer::nextScreenArg= &pop3Account;
		return;
	}

	if (key_POP3MAILDROPACCOUNT == promptKey)
	{
		Curses::keepgoing=false;
		myServer::nextScreen= &addAccountScreen;
		myServer::nextScreenArg= &pop3MaildropAccount;
		return;
	}

	if (key_NNTPACCOUNT == promptKey)
	{
		Curses::keepgoing=false;
		myServer::nextScreen= &addAccountScreen;
		myServer::nextScreenArg= &nntpAccount;
		return;
	}

	if (key_INBOXMBOX == promptKey
	    || key_OTHERMBOX == promptKey)
	{
		string homedir= myServer::getHomeDir();
		bool isDefaultMaildir=false;
		string defaultLocation="";

		if (key_INBOXMBOX == promptKey)
		{
			string maildir=homedir + "/Maildir/.";

			if (isMaildir(maildir))
			{
				defaultLocation="Maildir";
				isDefaultMaildir=true;
			}
			else
				defaultLocation="mail";
		}

		string f;

		if (isDefaultMaildir)
			f=defaultLocation;
		else
		{
			result=myServer::
				prompt(myServer::
				       promptInfo(_("Location: "))
				       .initialValue(defaultLocation));

			if (result.abortflag || (f=result).size() == 0)
				return;
		}

		if (isMaildir(f))
			f="maildir:" + f;
		else
			f= (key_INBOXMBOX == promptKey
			    ? "inbox:":"mbox:") + f;

		result=myServer::
			prompt(myServer::
				promptInfo(_("Name: "))
				.initialValue(_("My E-mail")));

		if (result.abortflag || ((string)result).size() == 0)
			return;

		myServer *ms=tryCreateAccount(result, f, "", "");

		if (!ms)
			return;
		myServer::saveconfig();
		ms->addHierarchy(true);

		listfolders_s();
	}
}

void mainMenu(void *dummy)
{
	MainMenuScreen mainMenu(mainScreen);

	titleBar->setTitles(_("MAIN MENU"), "");

	mainMenu.requestFocus();

	myServer::eventloop();
}

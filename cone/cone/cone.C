/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>
#include <pwd.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iomanip>
#include <sstream>

#include "curses/cursesbutton.H"
#include "curses/cursesdialog.H"
#include "curses/curseslabel.H"
#include "curses/cursesfield.H"
#include "curses/cursesfilereq.H"
#include "unicode/unicode.h"

#include "liblock/config.h"
#include "liblock/liblock.h"
#include "libmail/mail.H"
#include "libmail/fd.H"
#include "libmail/logininfo.H"
#include "libmail/misc.H"
#include "messagesize.H"
#include "myserver.H"
#include "myservercallback.H"
#include "myserverpromptinfo.H"
#include "myserverlogincallback.H"
#include "myserverremoteconfig.H"
#include "mainmenu.H"
#include "passwordlist.H"
#include "typeahead.H"
#include "ctrlchandler.H"
#include "curseshierarchy.H"
#include "cursesindexdisplay.H"
#include "cursesmessage.H"
#include "cursesmessagedisplay.H"
#include "cursesattachmentdisplay.H"
#include "spellchecker.H"
#include "specialfolder.H"
#include "addressbook.H"
#include "gettext.H"
#include "helpfile.H"
#include "colors.H"
#include "gpg.H"
#include "init.H"
#include "macros.H"
#include "configscreen.H"

#include <iostream>

using namespace std;

extern struct CustomColor color_misc_titleBar;
extern struct CustomColor color_misc_statusBar;
extern struct CustomColor color_misc_hotKey;
extern struct CustomColor color_misc_hotKeyDescr;

// Special UTF-8 only chars:

char ucheck[16];
char udelete[16];
char unew[16];

unicode_char ularr;
unicode_char urarr;
unicode_char ucplus;
unicode_char ucasterisk;
unicode_char ucwrap;

unicode_char uchoriz;
unicode_char ucvert;
unicode_char ucupright;
unicode_char ucrighttee;
unicode_char ucwatch;
unicode_char ucwatchend;

static Macros *macroPtr;

extern "C" void rfc2045_error(const char *errmsg)
{
	LIBMAIL_THROW(errmsg);
}

//
// Login and Cancel buttons on the account add/edit screens
//

class myLoginButton : public CursesButton {
public:
	myLoginButton(const char *name);
	~myLoginButton();
	void clicked();
};

myLoginButton::myLoginButton(const char *name)
	: CursesButton(NULL, name)
{
}

myLoginButton::~myLoginButton()
{
}

void myLoginButton::clicked()
{
	Curses::keepgoing=false;
}

class myCancelButton : public CursesButton, public CursesKeyHandler {

	bool processKey(const Curses::Key &key);

public:
	bool cancel;
	myCancelButton();
	~myCancelButton();

	void clicked();
};


myCancelButton::myCancelButton() : CursesButton(NULL, _("CANCEL")),
				   CursesKeyHandler(PRI_SCREENHANDLER),
				   cancel(false)
{
}

myCancelButton::~myCancelButton()
{
}

void myCancelButton::clicked()
{
	cancel=1;
	Curses::keepgoing=false;
}

bool myCancelButton::processKey(const Curses::Key &key)
{
	return false;
}

//
// Changing an account name, make sure that it's unique.

static bool isDupe(string newName, myServer *oldAcct)
{
	vector<myServer *>::iterator
		b=myServer::server_list.begin(),
		e=myServer::server_list.end();

	while (b != e)
	{
		if (oldAcct && (*b)->serverName == oldAcct->serverName)
		{
			b++;
			continue;
		}

		if ((*b)->serverName == newName)
		{
			statusBar->clearstatus();
			statusBar->status(_("Pick a different account name. "
					    "This one's already used."));
			return true;
		}

		b++;
	}

	return false;
}

//
// Try to add a new account

myServer *tryCreateAccount(string account, string url, string password,
			   string certificate)
{
	if (isDupe(account, NULL))
	{
		statusBar->beepError();
		return NULL;
	}

	myServer *ms=new myServer(account, url);

	ms->certificate=certificate;

	mail::loginInfo decodeUrl;

	mail::loginUrlDecode(url, decodeUrl);

	if (decodeUrl.method == "nntp" || decodeUrl.method == "nntps"
	    || decodeUrl.method == "pop3maildrop"
	    || decodeUrl.method == "pop3maildrops")
	{
		unsigned i=0;

		// Pick a reasonable filename for a newsrc file or maildir

		for (;;)
		{
			ostringstream o;

			string s=decodeUrl.server;

			size_t n=s.find('.');

			if (n != std::string::npos)
				s=s.substr(n+1);

			n=s.find('.');

			if (n != std::string::npos)
				s=s.substr(0, n);

			size_t ss;

			for (ss=0; ss<s.size(); ss++)
				if (strchr("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-", s[ss]) == NULL)
					s[ss]='_';

			o << s;

			if (i > 0)
				o << setw(4) << setfill('0') << i;
			++i;

			o << (decodeUrl.method[0] == 'p' ?
			      ".maildir":".newsrc");

			s=o.str();

			vector<myServer *>::iterator
				b=myServer::server_list.begin(),
				e=myServer::server_list.end();

			while (b != e)
			{
				if ( (*b)->newsrc == s )
					break;
				b++;
			}

			if (b == e)
			{
				ms->newsrc=s;
				break;
			}
		}
	}

	if (!ms)
	{
		statusBar->status(strerror(errno));
		statusBar->beepError();
		return NULL;
	}

	try
	{
		if (!ms->login(password))
		{
			statusBar->beepError();
			delete ms;
			return NULL;
		}
		PasswordList::passwordList.save(url, password);
		// Make sure the password is memorized.
	
		myServer::Callback callback;

		// Get the server's top level folder list, and save it as the
		// defaults.
		ms->server->readTopLevelFolders(ms->topLevelFolders,
						callback);
		if (!myServer::eventloop(callback))
		{
			statusBar->beepError();
			delete ms;
			return NULL;
		}
	} catch (...)
	{
		delete ms;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	return ms;
}

//
// Main add new account screen
//

static myServer *addAccountPrompt(MainMenuScreen::AccountType *accountType)
{
	CursesContainer loginScreen(mainScreen);

	loginScreen.setRow(2);
	loginScreen.setAlignment(Curses::PARENTCENTER);

	CursesLabel title(&loginScreen,
			  Gettext(_("Add %1% Account")) << accountType->name);
	title.setAlignment(Curses::PARENTCENTER);


	CursesDialog loginDialog(&loginScreen);

	loginDialog.setRow(2);

	CursesLabel account_label(NULL, _("Account name: "));
	CursesLabel server_label(NULL, _("Server: "));
	CursesLabel login_label(NULL,  _("Login: "));
	CursesLabel password_label(NULL, _("Password: "));

	CursesField account_field(NULL);
	CursesField server_field(NULL);
	CursesField login_field(NULL);
	CursesField password_field(NULL);
	CursesButton cram_field(NULL, _("Do not send password in clear text"),
				1);
	CursesButton ssl_field(NULL, _("Use an encrypted connection"),
				1);
	ConfigScreen::CertificateButton cert_field("");

	myLoginButton login_button(_("LOGIN"));
	myCancelButton cancel_button;

	struct passwd *pw=getpwuid(getuid());

	if (pw)
		login_field.setText(pw->pw_name); // Default login id

	password_field.setPasswordChar();

	int row=0;

	loginDialog.addPrompt(&account_label, &account_field, row);
	loginDialog.addPrompt(&server_label, &server_field, ++row);
	loginDialog.addPrompt(&login_label, &login_field, ++row);
	loginDialog.addPrompt(&password_label, &password_field, ++row);

	loginDialog.addPrompt(NULL, &cram_field, row += 2);
	loginDialog.addPrompt(NULL, &ssl_field, row += 2);

	if (!myServer::certs->certs.empty())
		loginDialog.addPrompt(NULL, &cert_field, row += 2);

	loginDialog.addPrompt(NULL, &login_button, row += 2);
	loginDialog.addPrompt(NULL, &cancel_button, row += 2);

	titleBar->setTitles(_("ADD ACCOUNT"), "");

	account_field.requestFocus();

	for (;;)
	{
		myServer::eventloop();

		if (cancel_button.cancel)
			break;

		string account=account_field.getText();
		string server=server_field.getText();
		string userid=login_field.getText();
		string password=password_field.getText();

		if (account.size() == 0)
		{
			account_field.requestFocus();
			account_field.beepError();
			continue;
		}

		if (server.size() == 0)
		{
			server_field.requestFocus();
			server_field.beepError();
			continue;
		}

		string smethod=accountType->secureMethod;
		string method=accountType->method;

		string url=mail::loginUrlEncode(ssl_field.getSelected()
						? accountType->secureMethod
						: accountType->method,
						server +
						(cram_field.getSelected()
						 ? "/cram":""),
						userid, "");

		myServer *ms=tryCreateAccount(account, url, password,
					      cert_field.cert);

		if (!ms)
			continue;

		myServer::saveconfig();
		return ms;
	}

	return NULL;
}

//
// Main folder list hierarchy screen.  When the folder list hierarchy is
// opened for the purpose of selecting a folder to copy messages to,
// the argument is not null and points to the original folder the messages
// are copied from.  Otherwise it is NULL.

void openHierarchyScreen(std::string prompt,
			 PreviousScreen *prevScreen,
			 mail::folder **selectedFolder,
			 myServer **selectedServer)
{
	CursesHierarchy hierarchy_screen( &myServer::hierarchy, mainScreen);
	
	titleBar->setTitles(_("FOLDERS"), "");

	hierarchy_screen.requestFocus();

	if (selectedFolder)
	{
		hierarchy_screen.selectingFolder=true;
		if (prevScreen)
			prevScreen->screenOpened();
		statusBar->clearstatus();
		statusBar->status(prompt);
	}

	//
	// On the main folder screen we should have no reason to have any
	// address book accounts or remote config accounts open.
	// This is a convenient way to keep these logins from idling for
	// an excessive period of time.

	AddressBook::closeAllRemote();
	if (myServer::remoteConfigAccount)
		myServer::remoteConfigAccount->logout();

	// Update top level folder message accounts.

	vector<myServer *>::iterator
		b=myServer::server_list.begin(),
		e=myServer::server_list.end();

	while (b != e)
	{
		(*b)->cancelTimer();

		if ((*b)->server)
			(*b)->alarm();
		b++;
	}

	myServer::eventloop();

	if (selectedFolder)
		*selectedFolder=hierarchy_screen.folderSelected;
	if (selectedServer)
		*selectedServer=hierarchy_screen.serverSelected;
}

void hierarchyScreen(void *dummy)
{
	openHierarchyScreen("", NULL, NULL, NULL);
}

void addAccountScreen(void *dummy)
{
	myServer *s=addAccountPrompt((MainMenuScreen::AccountType *)dummy);

	if (s)
		s->addHierarchy(true);

	myServer::nextScreen=&hierarchyScreen;
}

/////////////////////////////////////////////////////////////////////
//
// Edit a remote account.
//

void editAccountScreen(void *voidArg)
{
	myServer *s=(myServer *)voidArg;

	CursesContainer editScreen(mainScreen);

	editScreen.setRow(2);
	editScreen.setAlignment(Curses::PARENTCENTER);

	CursesLabel title(&editScreen,_("Edit Account"));
	title.setAlignment(Curses::PARENTCENTER);

	CursesDialog loginDialog(&editScreen);

	loginDialog.setRow(2);

	CursesLabel account_label(NULL, _("Account name: "));
	CursesLabel server_label(NULL, _("Server: "));
	CursesLabel login_label(NULL,  _("Login: "));
	CursesButton cram_field(NULL, _("Do not send password in clear text"),
				1);

	CursesField account_field(NULL);
	CursesField server_field(NULL);
	CursesField login_field(NULL);
	CursesButton ssl_field(NULL, _("Use an encrypted connection"),
				1);
	ConfigScreen::CertificateButton cert_field(s->certificate);

	myLoginButton login_button(_("UPDATE"));
	myCancelButton cancel_button;

	loginDialog.addPrompt(&account_label, &account_field, 0);
	loginDialog.addPrompt(&server_label, &server_field, 1);
	loginDialog.addPrompt(&login_label, &login_field, 2);

	int row=2;

	loginDialog.addPrompt(NULL, &cram_field, row += 2);
	loginDialog.addPrompt(NULL, &ssl_field, row += 2);

	if (!myServer::certs->certs.empty())
		loginDialog.addPrompt(NULL, &cert_field, row += 2);

	loginDialog.addPrompt(NULL, &login_button, row += 2);
	loginDialog.addPrompt(NULL, &cancel_button, row += 2);

	titleBar->setTitles(_("EDIT ACCOUNT"), "");

	mail::loginInfo loginInfo;

	account_field.setText(s->serverName);

	//
	// Decode the login URL and init the option settings.
	//

	if (mail::loginUrlDecode(s->url, loginInfo))
	{
		string server=loginInfo.server;

		login_field.setText(loginInfo.uid);

		if (loginInfo.method.size() > 0 &&
		    loginInfo.method.end()[-1] == 's') // pop3s, imaps, nntps
		{
			loginInfo.method=
				loginInfo.method.substr(0,
							loginInfo.method.size()
							-1); // drop the s
			ssl_field.setToggled(1);
		}

		// Separate button for the cram field

		map<string, string>::iterator
			p=loginInfo.options.find("cram"), e;

		if (p != loginInfo.options.end())
		{
			cram_field.setToggled(1);
			loginInfo.options.erase(p);
		}

		// Put back the remaining options

		for (p=loginInfo.options.begin(),
			     e=loginInfo.options.end(); p != e; p++)
		{
			string s=p->first;

			if (p->second.size() > 0)
				s=s + "=" + p->second;

			server += "/" + s;
		}

		server_field.setText(server);
	}

	account_field.requestFocus();

	for (;;)
	{
		myServer::eventloop();

		if (cancel_button.cancel)
			break;

		string account=account_field.getText();
		string server=server_field.getText();
		string userid=login_field.getText();

		if (account.size() == 0 || isDupe(account, s))
		{
			account_field.requestFocus();
			account_field.beepError();
			continue;
		}

		if (server.size() == 0)
		{
			server_field.requestFocus();
			server_field.beepError();
			continue;
		}

		string newUrl
			=mail::loginUrlEncode(loginInfo.method +
					      (ssl_field.getSelected()
					       ? "s":""),
					      server +
					      (cram_field.getSelected()
					       ? "/cram":""),
					      userid, "");

		s->serverLogout();

		PasswordList::passwordList.remove(s->url);

		AddressBook::updateAccount(s->url, newUrl);
		SpecialFolder::updateAccount(s->url, newUrl);

		s->url=newUrl;
		s->serverName=account;
		s->certificate=cert_field.cert;
		break;
	}

	myServer::saveconfig();
	Curses::keepgoing=false;
	myServer::nextScreen= &hierarchyScreen;
	myServer::nextScreenArg=NULL;
}

//
// Folder index screen.
//
static void folderIndexScreen(mail::ptr<myFolder> &f,
			      CursesIndexDisplay::exit_action &action);

static void getMessageNums(mail::account *acct,
			   set<string> &uid_set,
			   vector<size_t> &messageNums);

void folderIndexScreen(void *dummy)
{
	myFolder *f=(myFolder *)dummy;

	mail::ptr<myFolder> folderPtr(f);

	CursesIndexDisplay::exit_action action;


	while (!folderPtr.isDestroyed())
	{
		folderIndexScreen(folderPtr, action);

		if (folderPtr.isDestroyed() ||
		    action == CursesIndexDisplay::no_action)
			break;

		myServer *s=folderPtr->getServer();

		if (s->server == NULL)
			break;

		// Exited folder index screen by the "Copy" command.
		// Show the folder listing to select the destination folder,
		// then, after copying, go back to folder index screen.

		// First, save UIDs to copy.

		set<string> uids;

		size_t n=s->server->getFolderIndexSize();
		size_t msgNum;

		for (msgNum=0; msgNum < n; msgNum++)
		{
			mail::messageInfo i=s->server
				->getFolderIndexInfo(msgNum);

			if (i.marked)
				uids.insert(i.uid);
		}

		if (action == CursesIndexDisplay::copy_single ||
		    action == CursesIndexDisplay::move_single)
			uids.clear();
		// Ignore flagged uids, select UID under the cursor

		// No flagged UIDs?  Default to UID under the cursor.

		bool resetFlags=true;

		if (uids.size() == 0 &&
		    folderPtr->getCurrentMessage() < folderPtr->size())
		{
			msgNum=folderPtr->getServerIndex(folderPtr->
							 getCurrentMessage());
			uids.insert(s->server->
				       getFolderIndexInfo(msgNum).uid);
			resetFlags=false;
		}

		mail::folder *toFolderPtr;

		openHierarchyScreen(Gettext(
					    action ==
					    CursesIndexDisplay::copy_single ||
					    action ==
					    CursesIndexDisplay::copy_batch ?
					    _("Select the folder to copy "
					      "%1% message(s) to.") :
					    _("Select the folder to move "
					      "%1% message(s) to."))
				    << uids.size(), (myFolder *)folderPtr,
				    &toFolderPtr, NULL);

		if (myServer::nextScreen ||
		    folderPtr.isDestroyed() ||
		    s->server == NULL)
			break;

		if (toFolderPtr == NULL)
		{
			statusBar->clearstatus();
			statusBar->status(_("Copy/Move cancelled."));
			continue;
		}


		// Clear the marked flag, so that copied msgs are not marked
		// in the destination folder.

		mail::ptr<mail::folder> toFolder=toFolderPtr;

		bool rc;
		string errmsg;

		{
			vector<size_t> messageNums;

			getMessageNums(s->server, uids, messageNums);

			mail::messageInfo info;

			info.marked=true;

			statusBar->clearstatus();
			statusBar->status(_("Clearing flags..."));

			myServer::Callback callback;

			s->server->
				updateFolderIndexFlags(messageNums, false,
						       false,
						       info, callback);

			rc=myServer::eventloop(callback);

			if (s->server == NULL ||
			    folderPtr.isDestroyed() ||
			    toFolder.isDestroyed())
				break;
			if (!rc)
				continue;
		}

		{
			vector<size_t> messageNums;

			getMessageNums(s->server, uids, messageNums);

			statusBar->clearstatus();
			statusBar->status(_("Copying/Moving messages..."));

			myServer::Callback callback;

			if (action == CursesIndexDisplay::copy_single ||
			    action == CursesIndexDisplay::copy_batch)
				s->server->copyMessagesTo(messageNums,
							  toFolder, callback);
			else
				s->server->moveMessagesTo(messageNums,
							  toFolder, callback);

			rc=myServer::eventloop(callback);
			if (s->server == NULL ||
			    folderPtr.isDestroyed() ||
			    toFolder.isDestroyed())
				break;

			errmsg=callback.msg;
		}

		if (resetFlags)
		{
			vector<size_t> messageNums;

			getMessageNums(s->server, uids, messageNums);

			mail::messageInfo info;

			info.marked=true;

			statusBar->clearstatus();
			statusBar->status(_("Resetting flags..."));

			myServer::Callback callback;

			s->server->
				updateFolderIndexFlags(messageNums, false,
						       true,
						       info, callback);

			if (!myServer::eventloop(callback))
			{
				errmsg=callback.msg;
				rc=false;
			}
			if (s->server == NULL ||
			    folderPtr.isDestroyed() ||
			    toFolder.isDestroyed())
				break;
		}

		if (!rc)
		{
			statusBar->clearstatus();
			statusBar->status(errmsg);
		}
	}
}

// Now, we memorized the messages UIds, convert them to message numbers

static void getMessageNums(mail::account *acct,
			   set<string> &uid_set,
			   vector<size_t> &messageNums)
{
	messageNums.clear();

	size_t n=acct == NULL ? 0:acct->getFolderIndexSize();

	size_t i;

	for (i=0; i<n; i++)
		if (uid_set.count(acct->getFolderIndexInfo(i).uid) > 0)
			messageNums.push_back(i);
}

static void folderIndexScreen(mail::ptr<myFolder> &folderPtr,
			      CursesIndexDisplay::exit_action &action)
{
	myFolder *f=folderPtr;
	myServer::Callback idleOnCallback;

	myServer *server=f->getServer();

	// Enable immediate update notification, where available.

	if (server && server->server)
	{
		idleOnCallback.noreport=true;
		server->server->updateNotify(true, idleOnCallback);
	}

	CursesIndexDisplay indexScreen(mainScreen, f);

	mainScreen->setFirstRowShown(f->saveFirstRowShown);
	indexScreen.requestFocus();

	AddressBook::closeAllRemote(); // Close address book when we're here...

	myServer::eventloop();

	action=indexScreen.action;

	// Save the next screen to go to, while we're cleaning this stuff
	// up.

	void (*s)(void *)=myServer::nextScreen;
	void *a=myServer::nextScreenArg;

	if (idleOnCallback.noreport) // Took that if branch, above.
	{
		myServer::eventloop(idleOnCallback);
		// Should be finished by now

		myServer::nextScreen=s;
		myServer::nextScreenArg=a;
	}

	if (!folderPtr.isDestroyed())
	{
		// If we're still logged on, on exit, disable update notice.

		folderPtr->saveFirstRowShown=mainScreen->getFirstRowShown();

		myServer *server=folderPtr->getServer();

		if (server && server->server)
		{
			myServer::Callback offCallback;

			offCallback.noreport=true;
			server->server->updateNotify(false, offCallback);
			myServer::eventloop(offCallback);
		}

		if (!folderPtr.isDestroyed() &&
		    (server=folderPtr->getServer()) && server->server)
		{
			myServer::nextScreenArg=a;
			myServer::nextScreen=s;
		}
	}
}

void showCurrentMessage(void *dummy)
{
	CursesMessage *curmsg=(CursesMessage *)dummy;

	curmsg->screenOpened();
	// This would be the "previous screen",
	// in case anyone wants to know.

	CursesMessageDisplay messageScreen(mainScreen, curmsg);

	messageScreen.requestFocus();
	myServer::eventloop();
}

//
// Show attachments on a message.

void showAttachments(void *dummy)
{
	CursesMessage *curmsg=(CursesMessage *)dummy;

	CursesAttachmentDisplay attachmentScreen(mainScreen, curmsg);

	attachmentScreen.requestFocus();
	myServer::eventloop();
}

//
// First time at the plate, figure out where my mail is.
//

extern bool isMaildir(string f);

static void createconfig()
{
	string h=mail::homedir();

	string d;
	struct stat stat_buf;

	if (isMaildir("Maildir"))
	{
		d="maildir:Maildir"; // This system uses maildirs.
	}
	else if ( stat((d=h + "/mail/.").c_str(), &stat_buf) == 0)
	{
		d="inbox:mail"; // This system uses mboxes.
	}
	else
	{
		mkdir((d=h + "/Mail").c_str(), 0700);
		d="inbox:Mail"; // Fallback position.
	}

	// Initialize default unicode mapping.
	myServer::setDemoronizationType(myServer::demoronizationType);

	// An account for our default mailboxes, and an account that
	// points to the folder containing online help text.

	if (tryCreateAccount(_("My E-mail"), d, "", ""))
	{
		myServer *help=tryCreateAccount(_("Online Tutorial"),
						"mbox:" HELPFILE, "", "");
		if (help)
		{
			help->logout();
		}

		myServer::saveconfig();
	}
}

static void cleanup()
{
	CursesMainScreen::Lock logoutLock(mainScreen, true);
	// Prevent any screen updates from taking place.

	myServer::logout();
	AddressBook::closeAll();
	while (!myServer::server_list.empty())
		delete myServer::server_list.end()[-1];
	if (myServer::remoteConfigAccount)
	{
		myServer::remoteConfigAccount->logout();
		delete myServer::remoteConfigAccount;
		myServer::remoteConfigAccount=NULL;
	}
}

//
// Graceful logout and termination

void quitScreen(void *dummy)
{
	CtrlCHandler::loggingOut=true;

	cleanup();

	LIBMAIL_THROW(_("Have a nice day."));
}

// Recover old config file

static void doRecovery()
{
	vector<string> configFiles;

	myServer::getBackupConfigFiles(configFiles);

	vector<string>::iterator b=configFiles.begin(), e=configFiles.end();

	bool found=false;

	while (b != e)
	{
		struct stat stat_buf;

		if (stat(b->c_str(), &stat_buf) < 0)
		{
			b++;
			continue;
		}

		found=true;
		char tbuf[128];

		if (strftime(tbuf, sizeof(tbuf)-1, "%F %X",
			     localtime(&stat_buf.st_mtime)) == 0)
			strcpy(tbuf, "N/A");


		myServer::promptInfo promptInfo=
			myServer
			::prompt(myServer
				 ::promptInfo( Gettext(_("Recover configuration"
							 " from %1%? (Y/N) "))
					       << tbuf).yesno());

		if (promptInfo.abortflag)
			LIBMAIL_THROW(((const char *)
					     _("Recovery aborted.")));

		if ( (string)promptInfo == "Y")
		{
			string c=myServer::getConfigFilename();

			unlink(c.c_str());
			if (link(b->c_str(), c.c_str()) < 0)
				LIBMAIL_THROW((strerror(errno)));
			return;
		}
		++b;
	}

	LIBMAIL_THROW(found ? (const char *)
		      _("No more configuration files.")
		      : (const char *)_("No configuration files found."));
}

extern void version();

void resetScreenColors()
{
	{
		Curses::CursesAttr attr;

		attr.setFgColor(color_misc_titleBar.fcolor);
		titleBar->setAttribute(attr);
	}

	{
		Curses::CursesAttr attr;

		attr.setFgColor(color_misc_statusBar.fcolor);

		statusBar->setStatusBarAttr(attr);
	}

	{
		Curses::CursesAttr attr;

		attr.setFgColor(color_misc_hotKey.fcolor);
		statusBar->setHotKeyAttr(attr);
	}

	{
		Curses::CursesAttr attr;

		attr.setFgColor(color_misc_hotKeyDescr.fcolor);
		statusBar->setHotKeyDescr(attr);
	}
}

Macros *Macros::getRuntimeMacros()
{
	return macroPtr;
}

int main(int argc, char *argv[])
{
	int optc;
	int recover=0;
	Macros macroBuffer;

	while ((optc=getopt(argc, argv, "vrCc:")) != -1)
	{
		switch (optc) {
		case 'v':
			version();
			break;
		case 'r':
			recover=1;
			break;
		case 'c':

			myServer::configDir=optarg;
			break;
		case 'C':

			{
				ifstream i(myServer::getConfigFilename()
					   .c_str());

				if (!i.is_open())
				{
					cerr << strerror(errno) << endl;
					exit(1);
				}

				int c;

				while ((c=i.get()) != EOF)
				{
					cout << (char)c;

					if (c == '>')
						cout << endl;
				}
				exit(0);
			}
		case '?':
			cerr << "Usage: cone [-c configdir]" << endl;
			exit(1);
		}
	}

	signal(SIGPIPE, SIG_IGN);
	macroPtr= &macroBuffer;
	init(); // Common initialization between cone and leaf.

	// Init special characters if current display can show them.

	{
		std::vector<unicode_char> ucbuf;

		ucbuf.push_back(8594);

		bool err;

		std::string s;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() > 0 && !err)
			ucplus=urarr=ucbuf[0];
		else
		{
			urarr='>';
			ucplus='+';
		}

		ucbuf[0]=8592;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() > 0 && !err)
			ularr=ucbuf[0];
		else
			ularr='<';

		ucbuf[0]=8226;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() > 0 && !err)
			ucasterisk=ucbuf[0];
		else
			ucasterisk='*';

		ucbuf[0]=8617;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() > 0 && !err)
			ucwrap=ucbuf[0];
		else
			ucwrap='<';

		ucbuf[0]=8730;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() > 0 && !err)
			strncat(ucheck, s.c_str(), sizeof(ucheck)-1);
		else
			strcpy(ucheck, "*");

		ucbuf[0]=215;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() > 0 && !err)
			strncat(udelete, s.c_str(), sizeof(udelete)-1);
		else
			strcpy(udelete, _("D"));

		ucbuf[0]=9830;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() > 0 && !err)
			strncat(unew, s.c_str(), sizeof(unew)-1);
		else
			strcpy(unew, _("N"));

		// Line drawing

		ucbuf[0]=0x2500;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() == 0 || err)
			ucbuf[0]='_';
		uchoriz=ucbuf[0];

		ucbuf[0]=0x2502;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() == 0 || err)
			ucbuf[0]='|';
		ucvert=ucbuf[0];

		ucbuf[0]=0x2514;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() == 0 || err)
			ucbuf[0]='|';
		ucupright=ucbuf[0];

		ucbuf[0]=0x251C;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() == 0 || err)
			ucbuf[0]='|';
		ucrighttee=ucbuf[0];

		// Watching

		ucbuf[0]=0x2261;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() == 0 || err)
			ucbuf[0]='o';
		ucwatch=ucbuf[0];

		ucbuf[0]=0x2022;

		s=mail::iconvert::convert(ucbuf, unicode_default_chset(), err);

		if (s.size() == 0 || err)
			ucbuf[0]='*';
		ucwatchend=ucbuf[0];
	}

	// Default location of special folders.

	SpecialFolder::folders.insert( make_pair(string(DRAFTS),
						 SpecialFolder(_("Drafts"))));
	SpecialFolder::folders.insert( make_pair(string(SENT),
						 SpecialFolder(_("Outbox"))));

	mail::fd::rootCertRequiredErrMsg=
		_("Unable to initialize an encrypted connection because root authority certificates are not installed.\n\nIf you would like to proceed without verifying the the server's encryption certificate, append \"/novalidate-cert\" to the server's name and try again.");


	{
		string homedir=myServer::getConfigDir();
		mkdir(homedir.c_str(), 0700);

		string lockfile=homedir + "/.lock";

		int fd=open(lockfile.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0600);

		if (fd < 0 || fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
		{
			perror(lockfile.c_str());
			exit(1);
		}

		if (ll_lock_ex_test(fd))
		{
			std::cerr << _("Another copy of CONE is already running.")
			     << endl;
			exit(1);
		}

	}

	string errmsg="";

	try
	{
		CursesScreen curses_screen;

		initColorGroups();

		CtrlCHandler ctrl_c_handler;

		CursesTitleBar title_bar(&curses_screen, _("CONE"));

		CursesStatusBar status_bar(&curses_screen);

		CursesMainScreen main_screen(&curses_screen,
					     &title_bar,
					     &status_bar);

		SpellChecker spell_checker("", "utf-8");

		spellCheckerBase= &spell_checker;

		Typeahead typeahead_buf;

		bool welcome=true;

		titleBar= &title_bar;
		statusBar= &status_bar;
		cursesScreen= &curses_screen;
		mainScreen= &main_screen;

		resetScreenColors();
		if (recover)
			doRecovery();

		Certificates certs;

		myServer::certs= &certs;

		// certs.load();

		statusBar->status(_("Loading GnuPG encryption keys..."));
		statusBar->flush();
		GPG::gpg.init();
		statusBar->clearstatus();

		Curses::setSuspendHook(&mail::account::resume);

		// Try to load saved configuration

		if (myServer::loadconfig() &&
		    myServer::server_list.begin()
		    != myServer::server_list.end())
		{
			myServer *s= *myServer::server_list.begin();
			string pwd;

			// Prompt for the first listed server's password,
			// then try to log in.

			if (!PasswordList::passwordList.check(s->url, pwd) ||
			    !s->login(pwd, NULL))
			{
				s->disconnect();
				PasswordList::passwordList.remove(s->url);

				// Ok, that didn't work, let's try again
				// and this time prompt for a password.

				myServerLoginCallback loginCallback;

				if (!s->login(loginCallback))
				{
					s->disconnect();
					statusBar->beepError();
					welcome=false;
				}
				else
				{
					PasswordList::passwordList
						.save(s->url, s->password);
				}
			}
			resetScreenColors();

		} else	// First batter up.
		{
			createconfig();
		}

		myServer::initializeHierarchy();
		AddressBook::init();

		if (welcome)
		{
			statusBar->clearstatus();
			statusBar->status(Gettext(_("Welcome to Cone %1% (%2%)")
						  ) << VERSION
					  << unicode_default_chset()
					  << statusBar->NORMAL);
		}

		hierarchyScreen(NULL); // Initial screen.

		while (myServer::nextScreen)
		{
			void (*s)(void *)=myServer::nextScreen;
			void *a=myServer::nextScreenArg;

			myServer::nextScreen=NULL;
			(*s)(a);
		}

		cleanup();

	} catch (const char *txt)
	{
		errmsg=txt;
	} catch (char *txt)
	{
		errmsg=txt;
	}

	if (errmsg == "RESET") // Configuration reset
	{
		char *dummyargv[2];

		dummyargv[0]=argv[0];
		dummyargv[1]=NULL;

		execvp(argv[0], dummyargv);
		perror(argv[0]);
		exit(1);
	}

	if (errmsg.size() > 0)
		std::cerr << errmsg << endl;


	return 0;
}

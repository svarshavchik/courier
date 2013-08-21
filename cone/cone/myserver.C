/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "myserver.H"
#include "myservercallback.H"
#include "myserverlogincallback.H"
#include "myserverpromptinfo.H"
#include "myfolder.H"
#include "cursesmessage.H"
#include "ctrlchandler.H"
#include "typeahead.H"
#include "hierarchy.H"
#include "curses/cursesscreen.H"
#include "curses/cursesstatusbar.H"
#include "libmail/misc.H"
#include "gettext.H"
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>

#include <iostream>

using namespace std;

extern CursesStatusBar *statusBar;
extern CursesScreen *cursesScreen;

vector<myServer *> myServer::server_list;
Hierarchy myServer::hierarchy;

unsigned myServer::cmdcount=0;
string myServer::customHeaders="";

Certificates *myServer::certs;

extern void folderIndexScreen(void *);
extern void hierarchyScreen(void *);

myServer::myServer(string name, string urlArg)
	: serverName(name), url(urlArg), mailCheckInterval(300), server(NULL),
	  currentFolder(NULL), hierarchyEntry(NULL)
{
	server_list.push_back(this); // Register ourselves.
}

myServer *myServer::getServerByUrl(string url)
{
	vector<myServer *>::iterator b=server_list.begin(),
		e=server_list.end();

	while (b != e)
	{
		myServer *s= *b++;

		if (s->url == url)
			return s;
	}
	return NULL;
}

myServer::~myServer()
{
	vector<myServer *>::iterator b=server_list.begin(),
		e=server_list.end();

	while (b != e)
	{
		if ( *b == this ) // Deregister ourselves.
		{
			server_list.erase(b, b+1);
			break;
		}
		b++;
	}
	disconnect();
}

//
// Forcible disconnect from the server.  Destroy the current folder object,
// if any, and the server object.

void myServer::disconnect()
{
	// Take care to null-out all the pointers before destroying the
	// objects, in order to prevent infinite.

	if (currentFolder)
	{
		myFolder *f=currentFolder;
		currentFolder=NULL;
		delete f;
	}

	mail::account *oldServer=server;

	server=NULL;
	cancelTimer();
	if (oldServer)
		delete oldServer;

	disconnectTasks();
}

// libmail callback -- disconnected from the server.

void myServer::disconnected(const char *errmsg)
{
	string errmsgBuf=errmsg;

	bool wasConnected=server != NULL;

	disconnect(); // Clean everything up.

	Curses::keepgoing=false;	// Terminate event loop
	nextScreen= &hierarchyScreen;
	nextScreenArg=NULL;

	if (statusBar->prompting())
		statusBar->fieldAbort(); // Abort out of any prompt.

	if (wasConnected &&  // If NULL, explicit disconnect.
	    errmsgBuf.size() > 0)
	{
		statusBar->status(Gettext(_("%1% - DISCONNECTED: %2%"))
				  << serverName
				  << errmsgBuf, statusBar->DISCONNECTED);
		statusBar->beepError();
	}

	if (hierarchyEntry)
	{
		hierarchy.drawEraseBelow(hierarchyEntry, true);
		serverDescr="";
		hierarchyEntry->clear();
		hierarchy.drawEraseBelow(hierarchyEntry);
	}
}

void myServer::servererror(const char *errmsg)
{
	string errmsgBuf=Gettext(_("%1% - ALERT: %2%"))
		<< serverName << errmsg;

	statusBar->status(errmsgBuf, statusBar->SERVERERROR);
	statusBar->beepError();
}

//
// The main event loop.  Tasks:  A) Process mail::account events,
// B) process keyboard events.

// Part B is only done if no mail::account operations are pending
// (callback is NULL).  The eventloop continues until
//
// A) A terminal keyboard event is processed, or
// B) A mail::account operation succeeded
//

void (*myServer::nextScreen)(void *);
void *myServer::nextScreenArg;

bool myServer::eventloop(myServer::Callback *callback)
{
	const char *err_flag=NULL;

	bool eventLoopCalled=false;
	// We need to make sure that mail::account::process gets called at
	// least once, so that libmail has an opportunity to process "runlater"
	// events.

	Curses::keepgoing=true;

	if (callback == NULL)
		cursesScreen->draw();
	else
		callback->interrupted=false;

	nextScreen=NULL;

	Timer throbber;

	vector<struct pollfd> fds;
	int ioTimeout;

	while (callback || Curses::keepgoing)
	{
		if (callback)
			if (callback->succeeded || callback->failed
			    || callback->interrupted)
				if (eventLoopCalled)
					break;

		if (callback && throbber.expired())
		{
			statusBar->busy();
			throbber.setTimer(1);
		}

		bool alarmCalled;

		{
			struct timeval tv=Timer::getNextTimeout(alarmCalled);

			if (tv.tv_sec == 0 && tv.tv_usec == 0)
				tv.tv_sec= 60 * 60; // No alarms scheduled.

			ioTimeout=tv.tv_sec * 1000 + tv.tv_usec / 1000;
		}

		mail::account::process(fds, ioTimeout);
		eventLoopCalled=true;

		if (!callback && !Curses::keepgoing)
			break;

		Curses::Key k1=
			(callback == NULL && !Typeahead::typeahead->empty()
			 ? Typeahead::typeahead->pop() // Saved typeahead
			 : cursesScreen->getKey());

		if (k1.nokey())
		{
			mail::pollfd pfd;

			pfd.fd=0;
			pfd.events=mail::pollread;
			pfd.revents=0;
			fds.push_back(pfd);
		}

		if (callback == NULL || (k1.plain() && k1.ukey == '\x03'))
		{
			if (!k1.nokey())
			{
				bool rc=Curses::processKey(k1);

				if (!rc)
				{
					rc=0;
				}

				if (rc)
				{
					while (k1.plain() && k1 == '\x03' &&
					       !Typeahead::typeahead->empty())
						Typeahead::typeahead->pop();
					// CTRL-C kills typeahead buffer
				}
				else if (callback != NULL &&
					 !(k1.plain() &&
					   k1.ukey == '\x03'))
				{
					// Command in progress, save typeahead

					Typeahead::typeahead->push(k1);
				}
				continue;
			}
		}
		else if (callback && !k1.nokey())
			Typeahead::typeahead->push(k1);

		if (callback && (callback->succeeded || callback->failed
				 || callback->interrupted))
			break;

		if (alarmCalled)
			continue; // Something might've happened...

		if (mail::account::poll(fds, ioTimeout) < 0)
		{
			if (errno != EINTR)
			{
				err_flag="select";
				break;
			}
		}
	}

	statusBar->notbusy();

	if (err_flag)
	{
		statusBar->status(err_flag);
		statusBar->beepError();
		return false;
	}
	else if (callback)
	{
		// If a command just finished, check for anything that got
		// expunged.

		vector<myServer *>::iterator b=server_list.begin(),
			e=server_list.end();

		while (b != e)
		{
			myServer *p= *b++;

			if (p->currentFolder)
				p->currentFolder->checkExpunged();
		}

		if (!callback->noreport && !callback->interrupted)
		{
			statusBar->status(callback->msg,
					  callback->succeeded
					  ? CursesStatusBar::NORMAL:
					  callback->getLevel());

			if (!callback->succeeded)
				statusBar->beepError();
		}

		return callback->succeeded;
	}
	return true;
}

void myServer::eventloop()
{
	eventloop(NULL);
}

bool myServer::eventloop(myServer::Callback &callback)
{
	bool rc;

	++cmdcount;

	try {
		CtrlCHandler::lastCtrlC=0;

		rc=eventloop(&callback);
		--cmdcount;
	} catch (...) {
		--cmdcount;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
	return rc;
}

void myServer::find_cert_by_id(std::vector<std::string> &certArg)
{
	find_cert_by_id(certArg, certificate);
}

void myServer::find_cert_by_id(std::vector<std::string> &certArg,
			       std::string certificate)
{
	certArg.clear();
	if (certificate.size() > 0)
	{
		std::map<std::string, Certificates::cert>::iterator
			p=certs->certs.find(certificate);

		if (p != certs->certs.end())
		{
			certArg.reserve(1);
			certArg.push_back(p->second.cert);
		}
	}
}

void myServer::find_cert_by_url(std::string url,
				std::vector<std::string> &certArg)
{
	certArg.clear();

	std::vector<myServer *>::iterator b, e;

	for (b=server_list.begin(), e=server_list.end(); b != e; ++b)
		if ( (*b)->url == url && (*b)->certificate.size() > 0)
		{
			(*b)->find_cert_by_id(certArg);
			break;
		}
}

/////////////////////////////////////////////////////////////////////////
//
// Log in to the server.  The are several available mechanism.
//
// 1) An explicit password is supplied.
//
// 2) A callaback object is provided, to prompt for userid and password

bool myServer::login(string passwordStr)
{
	return login(passwordStr, NULL);
}

bool myServer::login(myServerLoginCallback &loginPrompt)
{
	return login("", &loginPrompt);
}

bool myServer::login(string passwordStr,
		     myServerLoginCallback *loginPrompt)
{
	Callback callback(CursesStatusBar::LOGINERROR);

	if (loginPrompt)
		loginPrompt->myCallback= &callback;

	mail::account::openInfo loginInfo;

	loginInfo.url=url;
	loginInfo.pwd=passwordStr;
	find_cert_by_id(loginInfo.certificates);
	loginInfo.loginCallbackObj=loginPrompt;

	loginInfo.extraString=getConfigDir() + "/" + newsrc;

	if (loginPrompt)
	{
		loginPrompt->reset();
		loginPrompt->myCallback= &callback;
	}

	server= mail::account::open(loginInfo, callback, *this);

	if (!server)
	{
		statusBar->clearstatus();
		statusBar->status(callback.msg);
		return false;
	}

	for (;;)
	{
		if (loginPrompt && callback.interrupted) // Libmail wants pwd
		{
			loginPrompt->promptPassword(serverName, passwordStr);
		}

		statusBar->clearstatus();
		statusBar->status(Gettext(_("Connecting to %1%..."))
				  << serverName);

		bool rc=eventloop(callback);

		if (loginPrompt && callback.interrupted) // Libmail wants pwd
			continue;

		if (!rc)
			return false;
		break;
	}

	// Logged in.

	password=passwordStr; // Remember the password.

	{
		mail::loginInfo urlDecodeInfo;

		if (mail::loginUrlDecode(url, urlDecodeInfo))
		{
			map<string, string>::iterator
				p=urlDecodeInfo.options.find("check");

			if (p != urlDecodeInfo.options.end())
			{
				istringstream i(p->second);

				i >> mailCheckInterval;

				if (mailCheckInterval < 10)
					mailCheckInterval=10;

				if (mailCheckInterval > 20 * 60)
					mailCheckInterval= 20 * 60;
			}
		}
	}

	return true;
}

// Log out all accounts.

void myServer::logout()
{
	vector<myServer *>::iterator b=server_list.begin(),
		e=server_list.end();

	myServer *s;

	while (b != e)
	{
		s= *b++;

		s->serverLogout();
	}
}

// Logout this account.

void myServer::serverLogout()
{
	cancelTimer();

	if (server)
	{
		if (currentFolder)
		{
			currentFolder->quiesce();
			currentFolder->isClosing=true;
		}

		Callback callback;

		server->logout(callback);
		eventloop(callback);
		disconnect();
	}
}

string myServer::getHomeDir()
{
	string homedir=getenv("HOME");

	if (homedir.size() == 0)
	{
		struct passwd *pw=getpwuid(getuid());

		if (pw)
			homedir=pw->pw_dir;
		else
			homedir="";
	}

	return homedir;
}

bool myServer::updateServerConfiguration(string name, string value)
{
	map<string, string>::iterator i=server_configuration.find(name);

	if (i != server_configuration.end())
	{
		if (value.size() > 0 && i->second == value)
			return false;

		server_configuration.erase(i);
		if (value.size() == 0)
			return true;
	}
	else if (value.size() == 0)
		return false;

	server_configuration.insert(make_pair(name, value));
	return true;
}

string myServer::getServerConfiguration(string name)
{
	if (server_configuration.count(name) > 0)
		return server_configuration.find(name)->second;

	return "";
}

bool myServer::updateFolderConfiguration(const mail::folder *folder,
					 string name,
					 string value)
{
	return updateFolderConfiguration(folder->getPath(), name, value);
}

bool myServer::updateFolderConfiguration(string p,
					 string name,
					 string value)
{
	if (folder_configuration.count(p) == 0)
	{
		map<string, string> dummy;

		folder_configuration.insert(make_pair(p, dummy));
	}

	map<string, string> &m=folder_configuration.find(p)->second;

	map<string, string>::iterator i=m.find(name);

	if (i != m.end())
	{
		if (i->second == value)
			return false;

		m.erase(i);
	}
	m.insert(make_pair(name, value));
	return true;
}

string myServer::getFolderConfiguration(const mail::folder *folder,
					string name)
{
	return getFolderConfiguration(folder->getPath(), name);
}

string myServer::getFolderConfiguration(string p,
					string name)
{
	if (folder_configuration.count(p) > 0)
	{
		map<string, string> &h=folder_configuration.find(p)->second;

		if (h.count(name) > 0)
			return (h.find(name)->second);
	}

	return "";
}

void myServer::saveFolderConfiguration()
{
	saveconfig(mail::account::isRemoteUrl(url));
}

//
// Opened a hierarchy.  Find any stale configuration entries, for a folder
// that cannot possibly exist, and get rid of it.

void myServer::openedHierarchy(Hierarchy::Entry *todo)
{
	vector<mail::folder *> folderVector;

	vector<Hierarchy::Entry *>::iterator
		cb=todo->begin(), ce=todo->end();

	while (cb != ce)
	{
		Hierarchy::Folder *f= (*cb)->toFolder();

		if (f)
		{
			mail::folder *mf=f->getFolder();

			if (mf)
				folderVector.push_back(mf);
		}

		cb++;
	}

	openedHierarchy(todo, folderVector);
}

void myServer::openedHierarchy(Hierarchy::Entry *todo,
			       vector<mail::folder *> &folderList)
{
	bool dirty=false;

	Hierarchy::Folder *parentFolder=todo->toFolder();

	// todo may also be a Hierarchy::Server.  After opening a mail account
	// delete any stale configuration data for any folder that's not in
	// any of the top level hierarchies defined for this account.
	// In this case toFolder() returns a NULL.

	const mail::folder *pf = parentFolder ? parentFolder->getFolder():NULL;

	map<string, map<string, string> >::iterator
		b=folder_configuration.begin(),
		e=folder_configuration.end(), save;

	while (b != e)
	{
		if (pf != NULL)
		{
			if (pf->getPath() == b->first)
			{
				b++;
				continue; // 'tis OK
			}

			if (!pf->isParentOf(b->first))
			{
				b++;
				continue; // Some other hierarchy, OK
			}
		}

		vector<mail::folder *>::iterator
			cb=folderList.begin(), ce=folderList.end();

		while (cb != ce)
		{
			mail::folder *mf= *cb;

			if (mf->getPath() == b->first
			    ||
			    mf->isParentOf(b->first))
			{
				break;
			}
			cb++;
		}

		save=b;
		b++;

		if (cb == ce)
		{
			dirty=true;
			folder_configuration.erase(save);
		}
	}

	if (todo->size() > 0)
	{
		Hierarchy &h=todo->getHierarchy();

		if (h.display)
			h.display->visible(*todo->begin());
	}
	if (dirty)
		saveFolderConfiguration();
}

// New account, initialize the default set of folders.

void myServer::initializeHierarchy()
{
	hierarchy.root.clear();

	vector<myServer *>::iterator
		b=server_list.begin(), e=server_list.end();

	while (b != e)
	{
		(*b)->addHierarchy(false);

		if ( (*b)->server != NULL)
		{
			(*b)->serverDescr=
				(*b)->server
				->getCapability(LIBMAIL_SERVERDESCR);

			(*b)->openedHierarchy( (*b)->hierarchyEntry );
			// Purge any deleted top level hierarchies
		}

		b++;
	}

	saveconfig();

	Hierarchy::EntryIteratorAssignRow assign_row(1);

	hierarchy.root.prefixIterate(assign_row);
}

//
// Install server's toplevelfolders.
//

void myServer::addHierarchy(bool assignRows)
{
	myServer *lastServer=NULL;

	if (assignRows)
	{
		vector<myServer *>::iterator b, e;

		b=server_list.begin();
		e=server_list.end();

		while (b != e)
		{
			lastServer= *--e;

			if (lastServer->hierarchyEntry)
				break;
			lastServer=NULL;
		}
	}

	Hierarchy::Server *hs=new Hierarchy::Server(hierarchy,
						    &hierarchy.root, this);
	if (!hs)
		outofmemory();

	hierarchyEntry=hs;
	addTopLevelFolders();

	if (assignRows)
	{
		Hierarchy::EntryIteratorAssignRow
			assign_row( lastServer ? lastServer->hierarchyEntry
				    ->getRow(): 1);

		if (lastServer)
			lastServer->hierarchyEntry->
				prefixIterateAfter(assign_row);
		else
			hs->prefixIterate(assign_row);
	}
}

void myServer::showHierarchy()
{
	if (hierarchyEntry == NULL) // WTF???
		return;

	hierarchy.drawEraseBelow(hierarchyEntry, true);
	hierarchyEntry->clear();

	addTopLevelFolders();
	hierarchy.drawEraseBelow(hierarchyEntry);
	openedHierarchy(hierarchyEntry);
	saveFolderConfiguration();
}

//
// Folder configuration changed.

void myServer::updateHierarchy()
{
	vector<const mail::folder *> folderList;

	vector<Hierarchy::Entry *>::iterator b=hierarchyEntry->begin(),
		e=hierarchyEntry->end();

	while (b != e)
	{
		Hierarchy::Folder *f= (*b++)->toFolder();

		if (f != NULL)
			folderList.push_back(f->getFolder());
	}

	topLevelFolders.success(folderList, false);
	openedHierarchy(hierarchyEntry);
	saveconfig(mail::account::isRemoteUrl(url));
}

void myServer::addTopLevelFolderRedraw(string path)
{
	hierarchy.drawEraseBelow(hierarchyEntry, true);
	topLevelFolders.add(path);
	addTopLevelFolder(path);
	hierarchy.drawEraseBelow(hierarchyEntry, false);
	saveFolderConfiguration();
}

void myServer::addTopLevelFolders()
{
	serverDescr=server ? server->getCapability(LIBMAIL_SERVERDESCR) : "";

	if (server == NULL)
		return;

	myReadFolders::iterator fb=topLevelFolders.begin(),
		fe=topLevelFolders.end();

	while (fb != fe)
		addTopLevelFolder( *fb++ );
	setTimer(mailCheckInterval);
}

void myServer::addTopLevelFolder(string path)
{
	mail::folder *f= server->folderFromString(path);

	if (!f)
		outofmemory();

	Hierarchy::Folder *hf;

	try {
		hf=new Hierarchy::Folder(hierarchy, hierarchyEntry, f);
		delete f;
	} catch (...)
	{
		delete f;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
	if (!hf)
		outofmemory();

	hf->updateInfo(this, false);
}

void myServer::alarm()
{
	vector<Hierarchy::Entry *>::iterator b=hierarchyEntry->begin();
	vector<Hierarchy::Entry *>::iterator e=hierarchyEntry->end();

	while (b != e)
	{
		Hierarchy::Folder *f= (*b++)->toFolder();

		if (f)
			f->updateInfo(this, false);
	}

	setTimer(mailCheckInterval);
}

void myServer::openFolder(const mail::folder *f,
			  bool autoOpenDraft)
	// True to autoopen a single message in the draft folder.
{
	if (server == NULL)
	{
		statusBar->beepError();
		return;
	}

	if (!currentFolder || currentFolder->mustReopen ||
	    currentFolder->getFolder()->getPath() != f->getPath())
	{
		if (currentFolder) // EXPUNGE it
		{
			currentFolder->mustReopen=true; // Fallback, if error

			myServer::Callback expungeCallback;

			expungeCallback.noreport=true;

			statusBar->clearstatus();
			statusBar->status(Gettext(_("Purging %1%..."))
					  << currentFolder->getFolder()
					  ->getName());

			if (currentFolder->mymessage)
				delete currentFolder->mymessage;

			currentFolder->isExpungingDrafts=true; // SHUT UP

			server->updateFolderIndexInfo(expungeCallback);

			myServer::eventloop(expungeCallback);

			finishCheckingNewMail();

			if (!server)
				return; // Server crashed
			currentFolder->isClosing=true;
		}

		statusBar->clearstatus();
		statusBar->status(Gettext(_("Opening %1%..."))
				  << f->getName());

		myFolder *newFolder=new myFolder(this, f);

		if (!newFolder)
			outofmemory();

		try {
			myFolder::RestoreSnapshot restoreSnapshot(newFolder);
			// Find any saved folder snapshot

			Callback select_callback;

			f->open(select_callback, &restoreSnapshot, *newFolder);

			if (!eventloop(select_callback))
			{
				delete newFolder;
				if (currentFolder)
					currentFolder->mustReopen=true;
				return;
			}
			if (currentFolder)
				delete currentFolder;
			currentFolder=newFolder;
		} catch (...)
		{
			if (currentFolder)
				currentFolder->mustReopen=true;
			delete newFolder;
		}

		if (!currentFolder)
			return;

		if (!currentFolder->init())
		{
			if (currentFolder)
				currentFolder->mustReopen=true;
			return;
		}	
	}

	nextScreen= &folderIndexScreen;
	nextScreenArg=currentFolder;
	Curses::keepgoing=false;

	if (autoOpenDraft)
	{
		if (currentFolder->size() == 1)
		{
			Curses::keepgoing=true;
			currentFolder->goDraft();
		}
	}
}

void myServer::finishCheckingNewMail()
{
	// An EXPUNGE, or a new mail check may kick off new mail processing,
	// which we want to allow to run to its natural end.

	while (server && currentFolder && currentFolder->isCheckingNewMail())
	{
		myServer::Callback noopCallback;

		server->checkNewMail(noopCallback);
		myServer::eventloop(noopCallback);
	}
}

void myServer::checkNewMail()
{
	myServer::Callback noopCallback;

	server->checkNewMail(noopCallback);
	myServer::eventloop(noopCallback);


	finishCheckingNewMail();
}

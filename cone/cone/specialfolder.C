/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "gettext.H"
#include "myserver.H"
#include "myservercallback.H"
#include "curseshierarchy.H"
#include "opensubfolders.H"
#include "specialfolder.H"

#include "curses/cursesstatusbar.H"

#include "libmail/mail.H"
#include "libmail/objectmonitor.H"
#include "unicode/unicode.h"

#include "libmail/mail.H"

using namespace std;

map<string, SpecialFolder> SpecialFolder::folders;

extern CursesStatusBar *statusBar;

SpecialFolder::SpecialFolder(string defaultNameUTF8Arg)
	: defaultNameUTF8(defaultNameUTF8Arg)
{
}

SpecialFolder::~SpecialFolder()
{
}

void SpecialFolder::addFolder(string nameUTF8Arg, string url,
			      string path)
{
	Info newFolderInfo;

	newFolderInfo.serverUrl=url;
	newFolderInfo.serverPath=path;
	newFolderInfo.nameUTF8=nameUTF8Arg;

	folderList.push_back(newFolderInfo);
}

SpecialFolder::Info *SpecialFolder::findFolder(std::string nameUTF8Arg)
{
	vector<SpecialFolder::Info>::iterator
		b=folderList.begin(),
		e=folderList.end();

	while (b != e)
	{
		if (b->nameUTF8 == nameUTF8Arg)
			return &*b;
		b++;
	}

	return NULL;
}


void SpecialFolder::setSingleFolder(string url, string path)
{
	Info newFolderInfo;

	newFolderInfo.serverUrl=url;
	newFolderInfo.serverPath=path;
	newFolderInfo.nameUTF8=defaultNameUTF8;

	folderList.clear();
	folderList.push_back(newFolderInfo);
}

bool SpecialFolder::getSingleFolder(std::string &url, std::string &path)
{
	url="";
	path="";

	if (folderList.size() == 0)
		return false;

	Info &info=folderList[0];

	url=info.serverUrl;
	path=info.serverPath;
	return true;
}

// Suppress spurious errors

class SpecialFolder::DummyCallback : public myServer::Callback {

public:
	DummyCallback();
	~DummyCallback();

	void fail(string message);
};

SpecialFolder::DummyCallback::DummyCallback()
{
}

SpecialFolder::DummyCallback::~DummyCallback()
{
}

void SpecialFolder::DummyCallback::fail(string message)
{
	success(_("OK"));
}

mail::folder *SpecialFolder::getFolder(myServer *&s)
{
	if (folderList.size() == 0)
	{
		Info newInfo;

		newInfo.nameUTF8=defaultNameUTF8;

		folderList.push_back(newInfo);
	}

	return folderList[0].getFolder(s);
}

mail::folder *SpecialFolder::Info::getFolder(myServer *&s)
{
	if (serverUrl.size() > 0 &&
	    (s=myServer::getServerByUrl(serverUrl)) != 0)
	{
		if (CursesHierarchy::autologin(s))
		{
			mail::ptr<mail::account> server=s->server;

			mail::folder *f=server->folderFromString(serverPath);

			try {
				DummyCallback createFolder;

				f->create(false, createFolder);

				myServer::eventloop(createFolder);

				if (server.isDestroyed())
					return NULL;
			} catch (...) {
				delete f;
				LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
			}

			return f;
		}
		return NULL;
	}

	// Create special folder in the first account.

	vector<myServer *>::iterator si=myServer::server_list.begin();

	if (si == myServer::server_list.end())
	{
		statusBar->clearstatus();
		statusBar->status(_("No servers available for special folders")
				  , statusBar->SYSERROR);
		statusBar->beepError();
		return NULL;
	}

	s= *si++;

	if (!CursesHierarchy::autologin(s))
		return NULL;

	mail::ptr<mail::account> server=s->server;

	// Find first top level folder that's a folder directory.

	myReadFolders::iterator fb=s->topLevelFolders.begin(),
		fe=s->topLevelFolders.end();

	mail::folder *f=NULL;

	while (fb != fe)
	{
		f=s->server->folderFromString(*fb++);

		if (!f)
		    outofmemory();

		if (!f->hasMessages())
			break;

		delete f;
		f=NULL;
	}


	if (f == NULL)
	{
		statusBar->clearstatus();
		statusBar->status(Gettext(_("%1% does not have a hierarchy for special folders."))
				  << s->serverName, statusBar->SYSERROR);
		statusBar->beepError();
		return NULL;
	}

	string nameStr(mail::iconvert::convert(nameUTF8, "utf-8",
					       unicode_default_chset()));

	{
		OpenSubFoldersCallback tryCreateFolder;
		DummyCallback ignoresErrors;

		f->createSubFolder(nameStr, false,
				   tryCreateFolder,
				   ignoresErrors);

		if (!myServer::eventloop(ignoresErrors)
		    || server.isDestroyed())
			return NULL;

		if (tryCreateFolder.folders.size() > 0)
		{
			serverUrl= s->url;
			serverPath= tryCreateFolder.folders[0]->toString();

			f=tryCreateFolder.folders[0]->clone();

			myServer::saveconfig();

			if (!f)
				outofmemory();

			return f;
		}
	}

	OpenSubFoldersCallback tryListFolder;
	myServer::Callback callback;

	f->readSubFolders(tryListFolder, callback);

	if (!myServer::eventloop(callback) || server.isDestroyed())
		return NULL;

	vector<mail::folder *>::iterator lfb, lfe;

	lfb=tryListFolder.folders.begin();
	lfe=tryListFolder.folders.end();

	while (lfb != lfe)
	{
		f= *lfb++;

		if (f->getName() == nameStr)
		{
			serverUrl= s->url;
			serverPath=f->toString();

			myServer::saveconfig();

			f=f->clone();

			if (!f)
				outofmemory();

			return f;
		}
	}

	return NULL;
}

void SpecialFolder::updateAccount(string oldUrl, string newUrl)
{
	map<string, SpecialFolder>::iterator b=folders.begin(),
		e=folders.end();

	while (b != e)
	{
		b->second.doUpdateAccount(oldUrl, newUrl);
		b++;
	}
}

void SpecialFolder::doUpdateAccount(string oldUrl, string newUrl)
{
	vector<Info>::iterator b=folderList.begin(), e=folderList.end();

	while (b != e)
	{
		b->doUpdateAccount(oldUrl, newUrl);
		b++;
	}
}

void SpecialFolder::Info::doUpdateAccount(string oldUrl, string newUrl)
{
	if (serverUrl == oldUrl)
		serverUrl=newUrl;
}

void SpecialFolder::deleteAccount(std::string url)
{
	map<string, SpecialFolder>::iterator b=folders.begin(),
		e=folders.end();

	while (b != e)
	{
		b->second.doDeleteAccount(url);
		b++;
	}
}

void SpecialFolder::doDeleteAccount(std::string url)
{
	size_t n=0;

	while (n < folderList.size())
	{
		if (folderList[n].serverUrl == url)
		{
			folderList.erase(folderList.begin()+n);
			continue;
		}

		n++;
	}
}


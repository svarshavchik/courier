/*
** Copyright 2006, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "libmail/rfcaddr.H"
#include "libmail/misc.H"

#include "curses/cursesstatusbar.H"

#include "addressbookinterfacemail.H"
#include "myserver.H"
#include "myservercallback.H"
#include "myserverlogincallback.H"
#include "myserverpromptinfo.H"
#include "passwordlist.H"
#include "gettext.H"

#include <errno.h>
#include <stdlib.h>

extern CursesStatusBar *statusBar;

AddressBook::Interface::Mail::Mail(std::string nameArg)
	: name(nameArg), server(NULL), folder(NULL), addressBook(NULL)
{
}

AddressBook::Interface::Mail::~Mail()
{
	if (addressBook)
	{
		delete addressBook;
		addressBook=NULL;
	}

	if (folder)
	{
		delete folder;
		folder=NULL;
	}

	mail::account *s=server;

	if (s)
	{
		server=NULL;

		myServer::Callback logout_callback;

		s->logout(logout_callback);

		myServer::eventloop(logout_callback);
		delete s;
	}
}


bool AddressBook::Interface::Mail::open(std::string url,
					std::string folderStr)
{
	if (addressBook)
		return true; // Already open.

	// Log in.

	std::string pwd="";

	if (!PasswordList::passwordList.check(url, pwd) ||
	    !serverLogin(url, pwd, NULL))
	{
		PasswordList::passwordList.remove(url);

		myServerLoginCallback loginCallback;

		if (!serverLogin(url, "", &loginCallback))
			return false;
	}

	// Build the folder object, then open it.

	folder=server->folderFromString(folderStr);

	if (!folder)
	{
		statusBar->status(strerror(errno), statusBar->SERVERERROR);
		statusBar->beepError();
		return false;
	}

	addressBook=new mail::addressbook(server, folder);

	if (!addressBook)
		outofmemory();

	myServer::Callback selectCallback;
	addressBook->open(selectCallback);
	if (!myServer::eventloop(selectCallback))
	{
		delete addressBook;
		addressBook=NULL;
	}
	return addressBook != NULL;


}

bool AddressBook::Interface::Mail::serverLogin(std::string url,
					 std::string pwd,
					 myServerLoginCallback *loginPrompt)
{
	myServer::Callback login_callback;

	if (loginPrompt)
	{
		loginPrompt->myCallback= &login_callback;
	}

	mail::account::openInfo loginInfo;
	loginInfo.url=url;
	loginInfo.pwd=pwd;
	myServer::find_cert_by_url(url, loginInfo.certificates);
	loginInfo.loginCallbackObj=loginPrompt;

	server=mail::account::open(loginInfo,
				   login_callback, *this);

	if (!server)
		outofmemory();


	for (;;)
	{
		if (loginPrompt)
		{
			loginPrompt->reset();
			loginPrompt->myCallback= &login_callback;
		}

		statusBar->clearstatus();
		statusBar->status(Gettext(_("Connecting to %1%...")) << name);

		bool rc=myServer::eventloop(login_callback);

		if (loginPrompt && login_callback.interrupted)
		{
			// Using a login callback (instead of verbatim passwd)

			std::string promptStr=
				Gettext( loginPrompt->isPasswordPrompt
					 ?_("Password for %1% (CTRL-C - cancel): ")
					 :_("Login for %1% (CTRL-C - cancel): ")
					 ) << name;

			myServer::promptInfo response=
				myServer::promptInfo(promptStr);

			if (loginPrompt->isPasswordPrompt)
				response.password();

			response=myServer::prompt(response);

			if (response.aborted())
			{
				loginPrompt->callbackCancel();
				continue;
			}

			pwd=response;
			loginPrompt->callback(pwd);
			continue;
		}

		if (!rc)
		{
			if (server)
			{
				mail::account *serverPtr=server;
				server=NULL;
				delete serverPtr;
			}
			return false;
		}
		break;
	}

	// Make sure we cache the good password.
	PasswordList::passwordList.save(url, pwd);
	return true;
}

void AddressBook::Interface::Mail::close()
{
	mail::account *serverPtr=server;

	// Be polite and formally log off the server

	if (serverPtr)
		try {
			server=NULL;

			myServer::Callback logout_callback;

			serverPtr->logout(logout_callback);

			myServer::eventloop(logout_callback);

			delete serverPtr;
			if (addressBook)
			{
				delete addressBook;
				addressBook=NULL;
			}

			if (folder)
			{
				delete folder;
				folder=NULL;
			}

		} catch (...) {
			delete serverPtr;
			if (addressBook)
			{
				delete addressBook;
				addressBook=NULL;
			}

			if (folder)
			{
				delete folder;
				folder=NULL;
			}
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
}

bool AddressBook::Interface::Mail::closed()
{
	return addressBook != NULL;
}

void AddressBook::Interface::Mail::done()
{
}

bool AddressBook::Interface::Mail::add(mail::addressbook::Entry &newEntry,
				 std::string oldUid)
{
	if (!addressBook)
		return true;

	myServer::Callback add_callback;

	addressBook->addEntry(newEntry,
			      oldUid,
			      add_callback);

	return myServer::eventloop(add_callback);
}

bool AddressBook::Interface::Mail::import(std::list<mail::addressbook::Entry>
				    &newList)
{
	if (!addressBook)
		return true;

	myServer::Callback cb;

	addressBook->importEntries(newList, cb);
	return myServer::eventloop(cb);
}

bool AddressBook::Interface::Mail::del(std::string uid)
{
	if (!addressBook)
		return true;

	myServer::Callback del_callback;

	addressBook->del(uid, del_callback);

	return myServer::eventloop(del_callback);
}

bool AddressBook::Interface::Mail::rename(std::string uid,
				    std::string newnickname)
{
	if (!addressBook)
		return true;

	bool rc;
	std::vector<mail::emailAddress> addrList;

	{
		myServer::Callback get_callback;

		addressBook->getEntry(uid, addrList, get_callback);
		rc=myServer::eventloop(get_callback);
	}

	if (rc)
	{
		myServer::Callback add_callback;

		mail::addressbook::Entry newEntry;
		newEntry.nickname=newnickname;
		newEntry.addresses=addrList;
		addressBook->addEntry(newEntry, uid, add_callback);
		rc=myServer::eventloop(add_callback);
	}

	return rc;
}

bool AddressBook::Interface::Mail::searchNickname(std::string nickname,
					    std::vector<mail::emailAddress>
					    &addrListArg)
{
	if (!addressBook)
		return true;

	myServer::Callback search_callback;

	addressBook->searchNickname(nickname, addrListArg, search_callback);

	return myServer::eventloop(search_callback);
}

void AddressBook::Interface::Mail::getIndex( std::list< std::pair<std::string,
				       std::string> > &listArg)
{
	if (!addressBook)
		return;

	myServer::Callback getindex_callback;

	addressBook->getIndex(listArg, getindex_callback);

	myServer::eventloop(getindex_callback);
}

bool AddressBook::Interface::Mail::getEntry(std::string uid,
				      std::vector<mail::emailAddress>
				      &addrList)
{
	if (!addressBook)
		return true;

	myServer::Callback get_callback;

	addressBook->getEntry(uid,
			      addrList, get_callback);
	return myServer::eventloop(get_callback);
}

void AddressBook::Interface::Mail::disconnected(const char *errmsg)
{
	if (errmsg && *errmsg && server)
	{
		statusBar->status(errmsg, statusBar->DISCONNECTED);
		statusBar->beepError();
	}

	close();
}

void AddressBook::Interface::Mail::servererror(const char *errmsg)
{
	if (errmsg && *errmsg)
	{
		statusBar->status(errmsg, statusBar->SERVERERROR);
		statusBar->beepError();
	}

	close();
}

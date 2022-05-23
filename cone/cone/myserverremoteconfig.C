/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "myserverremoteconfig.H"
#include "myservercallback.H"
#include "myserverlogincallback.H"
#include "myreadfolders.H"
#include "passwordlist.H"
#include "libmail/addmessage.H"
#include "libmail/attachments.H"
#include "libmail/headers.H"
#include "gettext.H"
#include "init.H"
#include "searchcallback.H"
#include "errno.h"
#include "rfc822/encode.h"
#include <courier-unicode.h>

using namespace std;

#define SUBJMARKER "[LibMAIL-config]"
#define MACROSUBJMARKER "[LibMAIL-macros]"

#define MYTYPE "application"
#define MYSUBTYPE "x-libmail-remoteconfig"

#define CONTENTTYPE MYTYPE "/" MYSUBTYPE

myServer::remoteConfig::remoteConfig() : account(NULL), folder(NULL)
{
}

myServer::remoteConfig::~remoteConfig()
{
	if (folder)
		delete folder;

	mail::account *a=account;

	account=NULL;

	if (a)
		delete a;
}

//
// Log in and open the remote config folder, unless already logged in.
//

mail::account *myServer::remoteConfig::login()
{
	errmsg="";

	if (account)
		return account;

	if (folder)
	{
		delete folder;
		folder=NULL;
	}

	if (myServer::remoteConfigPassword.size() == 0)
	{
		if (PasswordList::passwordList.check(myServer::remoteConfigURL,
						     myServer
						     ::remoteConfigPassword))
		{
			mail::account *a=login2();

			if (a)
				return a;

			PasswordList::passwordList
				.remove(myServer::remoteConfigURL,
					_("Login failed"));
		}

		myServer::remoteConfigPassword="";
	}

	mail::account *a=login2();

	if (a)
		return a;

	return NULL;
}

mail::account *myServer::remoteConfig::login2()
{
	myServer::Callback callback(CursesStatusBar::LOGINERROR);

	mail::account::openInfo loginInfo;

	myServerLoginCallback loginCallback;

	loginInfo.url=myServer::remoteConfigURL;
	loginInfo.pwd=myServer::remoteConfigPassword;
	find_cert_by_url(myServer::remoteConfigURL,
			 loginInfo.certificates);

	loginInfo.loginCallbackObj= &loginCallback;

	loginCallback.reset();
	loginCallback.myCallback= &callback;

	account=mail::account::open(loginInfo, callback, *this);

	if (!account)
	{
		errmsg=callback.msg;
		return NULL;
	}

	for (;;)
	{
		statusBar->clearstatus();
		statusBar->status(_("Opening remote configuration..."));

		bool rc=myServer::eventloop(callback);

		if (callback.interrupted)
		{
			loginCallback.promptPassword(_("remote configuration"),
						     myServer
						     ::remoteConfigPassword);
			loginCallback.reset();
			loginCallback.myCallback= &callback;
			continue;
		}

		if (!rc)
		{
			myServer::remoteConfigPassword="";

			mail::account *a=account;

			account=NULL;
			if (a)
				delete a;
			errmsg=callback.msg;
			return NULL;
		}
		break;
	}


	// We're in, open the folder.

	try {
		myReadFolders folders;
		myServer::Callback findCallback;
		findCallback.noreport=true;

		account->findFolder(myServer::remoteConfigFolder,
				    folders,
				    findCallback);

		if (myServer::eventloop(findCallback) && folders.size() == 1)
		{
			folder=account->folderFromString(folders[0]);

			if (!folder)
			{
				statusBar->clearstatus();
				statusBar->status(strerror(errno));
				statusBar->beepError();
			}
			else
			{
				myServer::Callback openCallback;

				folder->open(openCallback, NULL, *this);

				if (myServer::eventloop(openCallback))
					return account;

				errmsg=openCallback.msg;
			}
		}
		else
		{
			errmsg=findCallback.msg;
		}
	} catch (...) {
		mail::account *a=account;

		account=NULL;

		if (a)
			delete a; // Clean up real nice
		throw;
	}

	logout();

	return NULL;
}

void myServer::remoteConfig::logout()
{
	mail::account *a=account;

	account=NULL;

	if (a)
		try {
			myServer::Callback c;

			a->logout(c);

			myServer::eventloop(c);

			delete a;
		} catch (...) {
			delete a;
			throw;
		}

	if (folder)
	{
		delete folder;
		folder=NULL;
	}
}

//
// Save new remote config.  Returns empty string on success, an error message
// otherwise.
//

std::string myServer::remoteConfig::saveconfig(std::string filename,
					       std::string macrofilename)
{
	errmsg="";

	if (login() == NULL)
	{
		if (errmsg.size() == 0)
			errmsg=_("Unable to open the remote configuration"
				 " folder.");
		return errmsg;
	}

	if (!saveconfig2(filename, SUBJMARKER, lastFilenameSaved) ||
	    !saveconfig2(macrofilename, MACROSUBJMARKER, lastMacroFilenameSaved))
	{
		if (errmsg.size() == 0)
			errmsg=strerror(errno);

		return errmsg;
	}

	return "";
}

namespace {

	struct scoped_fp {
		FILE *fp;

		scoped_fp(const char *filename)
			: fp{fopen(filename, "r")}
		{
		}

		~scoped_fp()
		{
			if (fp)
				fclose(fp);
		}
	};
}

bool myServer::remoteConfig::saveconfig2(std::string filename,
					 std::string subjmarker,
					 std::ifstream &cachedContents)
{
	if (filename.size() > 0)
	{
		scoped_fp fp{filename.c_str()};

		if (!fp.fp)
		{
			errmsg=strerror(errno);
			return false;
		}

		if (configFileUnchanged(cachedContents, fp.fp))
			return true;

		if (fseek(fp.fp, 0L, SEEK_SET) < 0)
		{
			errmsg=strerror(errno);
			return false;
		}

		myServer::Callback callback;

		mail::addMessage *add=NULL;

		try {
			add=folder->addMessage(callback);

			if (!add)
			{
				errmsg=callback.msg;
				return false;
			}

			mail::Header::list headers;

			headers << mail::Header
				::addresslist("From")(mail::address("LibMAIL configuration",
								    "nobody@localhost")
						      )
				<< mail::Header::encoded("Subject",
							 subjmarker,
							 "utf-8")
				<< mail::Header::plain("Content-Type",
						       CONTENTTYPE);

			mail::Attachment att(headers, fileno(fp.fp));

			{
				size_t dummy;
				myServer::Callback assembleCB;

				add->assembleContent(dummy, att, assembleCB);
				myServer::eventloop(assembleCB);
			}

			if (!add->assemble())
			{
				errmsg=strerror(errno);
				add->fail("Aborted");
				return false;
			}

		} catch (...) {
			if (add)
				add->fail("Aborted");
			throw;
		}
		add->go();

		if (!myServer::eventloop(callback))
		{
			errmsg=callback.msg;
			return false;
		}

		// Delete older configurations.

		myServer::Callback checkNewMailCallback;

		account->checkNewMail(checkNewMailCallback);
		// Make sure the new message is hit.

		if (!myServer::eventloop(checkNewMailCallback))
		{
			errmsg=checkNewMailCallback.msg;
			return false;
		}

		if (cachedContents.is_open())
			cachedContents.close();
		cachedContents.clear();
		cachedContents.open(filename.c_str());
	}

	{
		myServer::Callback searchCallback;
		SearchCallbackHandler searchList(searchCallback);

		mail::searchParams searchParams;

		searchParams.criteria=searchParams.subject;
		searchParams.param2=subjmarker;
		searchParams.charset="US-ASCII";
		searchParams.scope=searchParams.search_all;

		account->searchMessages(searchParams, searchList);

		if (!myServer::eventloop(searchCallback))
		{
			errmsg=searchCallback.msg;
			return false;
		}

		myServer::Callback removeCallback;

		if (searchList.messageNumbers.size() > 0 &&
		    filename.size() > 0) // Else: delete this config file
		{
			searchList.messageNumbers
				.erase(searchList.messageNumbers.begin()
				       +(searchList.messageNumbers.size()-1),
				       searchList.messageNumbers.end());
		}

		if (searchList.messageNumbers.size() > 0)
		{
			account->removeMessages(searchList.messageNumbers,
						removeCallback);
			if (!myServer::eventloop(removeCallback))
				return false;
		}
	}

	return true;
}

bool myServer::remoteConfig::configFileUnchanged(std::ifstream &savedCpy,
						 FILE *newCpy)
{
	if (!savedCpy.is_open())
		return false;

	savedCpy.clear();
	savedCpy.seekg(0);

	char buf1[BUFSIZ];
	char buf2[BUFSIZ];

	int n;

	while ((n=fread(buf1, 1, sizeof(buf1), newCpy)) > 0)
	{
		if (savedCpy.read(buf2, n).fail() || savedCpy.bad())
			return false;

		if (memcmp(buf1, buf2, n))
			return false;
	}

	if (n < 0 || savedCpy.get() != EOF)
		return false;

	return true;
}

void myServer::remoteConfig::disconnected(const char *errmsg)
{
	if (errmsg && account)
	{
		statusBar->status(errmsg, statusBar->DISCONNECTED);
		statusBar->beepError();
	}

	mail::account *a=account;

	account=NULL;

	if (a)
		delete a;
}

void myServer::remoteConfig::servererror(const char *errmsg)
{
	if (errmsg)
	{
		statusBar->status(errmsg, statusBar->SERVERERROR);
		statusBar->beepError();
	}
}


std::string myServer::remoteConfig::loadconfig(std::string filename,
					       std::string macrofilename)
{
	errmsg="";

	if (login() == NULL)
	{
		if (errmsg.size() == 0)
			errmsg=_("Unable to open the remote configuration"
				 " folder.");
		return errmsg;
	}

	if (!loadconfig2(filename, SUBJMARKER, false, lastFilenameSaved) ||
	    !loadconfig2(macrofilename, MACROSUBJMARKER, true,
			 lastMacroFilenameSaved))
	{
		if (errmsg.size() == 0)
			errmsg=strerror(errno);

		return errmsg;
	}

	return "";
}

bool myServer::remoteConfig::loadconfig2(std::string filename,
					 std::string subjmarker,
					 bool optional,
					 std::ifstream &savedContents)
{
	string notfound=
		_("Unable to find remotely-saved configuration file. "
		  "Remotely-saved configuration was probably deleted.");

	size_t messageNumber;

	if (savedContents.is_open())
		savedContents.close();

	{
		myServer::Callback searchCallback;
		SearchCallbackHandler searchList(searchCallback);

		mail::searchParams searchParams;

		searchParams.criteria=searchParams.subject;
		searchParams.param2=subjmarker;
		searchParams.charset="US-ASCII";
		searchParams.scope=searchParams.search_all;

		account->searchMessages(searchParams, searchList);

		if (!myServer::eventloop(searchCallback))
		{
			errmsg=searchCallback.msg;
			return false;
		}

		if (searchList.messageNumbers.size() == 0)
		{
			if (optional)
				return true;

			errmsg=notfound;
			return false;
		}
		messageNumber=searchList.messageNumbers.end()[-1];
	}

	mail::mimestruct mimeInfo;

	{
		myServer::Callback cb;
		loadCallback callbackInfo(cb);

		vector<size_t> mlist;

		mlist.push_back(messageNumber);
		account->readMessageAttributes(mlist,
					       account->MIMESTRUCTURE,
					       callbackInfo);

		if (!myServer::eventloop(cb))
		{
			errmsg=cb.msg;
			return false;
		}

		if (strcasecmp(callbackInfo.mimeinfo.type.c_str(),
			       MYTYPE) ||
		    strcasecmp(callbackInfo.mimeinfo.subtype.c_str(),
			       MYSUBTYPE))
		{
			errmsg=notfound;
			return false;
		}

		mimeInfo=callbackInfo.mimeinfo;
	}

	{
		myServer::Callback cb;
		loadCallback callbackInfo(cb);

		unlink(filename.c_str());

		callbackInfo.o.open(filename.c_str());

		if (!callbackInfo.o.is_open())
		{
			errmsg=strerror(errno);
			return false;
		}

		account->readMessageContentDecoded(messageNumber, false,
						   mimeInfo, callbackInfo);

		if (!myServer::eventloop(cb))
		{
			errmsg=cb.msg;
			return false;
		}

		callbackInfo.o.flush();
		if (callbackInfo.o.bad() ||
		    callbackInfo.o.fail())
		{
			errmsg=strerror(errno);
			return false;
		}
	}

	savedContents.open(filename.c_str());

	if (!savedContents.is_open())
	{
		errmsg=filename + ": " + strerror(errno);
		return false;
	}

	return true;
}

myServer::remoteConfig::loadCallback::loadCallback(myServer::Callback &cb)
	: origCallback(cb)
{
}

myServer::remoteConfig::loadCallback::~loadCallback()
{
}

void myServer::remoteConfig::loadCallback::success(std::string message)
{
	origCallback.success(message);
}

void myServer::remoteConfig::loadCallback::fail(std::string message)
{
	origCallback.fail(message);
}

void myServer::remoteConfig::loadCallback
::messageStructureCallback(size_t n,
			   const mail::mimestruct &mimeInfoArg)
{
	mimeinfo=mimeInfoArg;
}

void myServer::remoteConfig::loadCallback
::reportProgress(size_t bytesCompleted,
		 size_t bytesEstimatedTotal,

		 size_t messagesCompleted,
		 size_t messagesEstimatedTotal)
{
	mail::callback &c=origCallback;

	c.reportProgress(bytesCompleted,
			 bytesEstimatedTotal,
			 messagesCompleted,
			 messagesEstimatedTotal);
}

void myServer::remoteConfig::loadCallback
::messageTextCallback(size_t n, string text)
{
	o << text;
}


//
// Ignore folder changes.
//

void myServer::remoteConfig::messagesRemoved(vector< pair<size_t, size_t> >
					     &dummy)
{
}

void myServer::remoteConfig::messageChanged(size_t n)
{
}

void myServer::remoteConfig::newMessages()
{
}

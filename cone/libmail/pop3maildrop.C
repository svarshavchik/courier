/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "pop3maildrop.H"
#include "pop3.H"
#include "maildiradd.H"
#include "driver.H"
#include "misc.H"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

/////////////////////////////////////////////////////////////////////////////

LIBMAIL_START

static bool open_pop3maildrop(mail::account *&accountRet,
			 mail::account::openInfo &oi,
			 mail::callback &callback,
			 mail::callback::disconnect &disconnectCallback)
{
	mail::loginInfo pop3LoginInfo;

	if (!mail::loginUrlDecode(oi.url, pop3LoginInfo))
		return false;

	string pop3method;

	if (pop3LoginInfo.method == "pop3maildrop")
		pop3method="pop3";
	else if (pop3LoginInfo.method == "pop3maildrops")
		pop3method="pop3s";
	else
		return false;

	accountRet=new mail::pop3maildrop(disconnectCallback,
					  oi.loginCallbackObj,
					  callback,
					  oi.extraString,
					  pop3method +
					  oi.url.substr(oi.url.find(':')),
					  oi.pwd,
					  oi.certificates);

	return true;
}

static bool pop3maildrop_remote(string url, bool &flag)
{
	mail::loginInfo pop3LoginInfo;

	if (!mail::loginUrlDecode(url, pop3LoginInfo))
		return false;

	if (pop3LoginInfo.method != "pop3maildrop" &&
	    pop3LoginInfo.method != "pop3maildrops")
		return false;

	flag=false;
	return true;
}

driver pop3maildrop_driver= { &open_pop3maildrop, &pop3maildrop_remote };

LIBMAIL_END


class mail::pop3maildrop::checkNewMailHelper
	: public mail::callback {

	size_t messagesProcessed;

public:
	mail::pop3maildrop::pop3acct *pop3acct;
private:
	mail::pop3maildrop *maildiracct;

	mail::callback *origCallback;

	void success(std::string message);
	void fail(std::string message);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:

	checkNewMailHelper(mail::pop3maildrop::pop3acct *pop3acctArg,
			   mail::pop3maildrop *maildiracctArg,
			   mail::callback *origCallbackArg);
	~checkNewMailHelper();
};

/////////////////////////////////////////////////////////////////////////////
//
// Subclass pop3 to forego UIDL, and just push all msgs to the pop3maildrop
// maildir.

class mail::pop3maildrop::pop3acct : public mail::pop3,
	    private mail::callback {

	mail::pop3maildrop *myMaildrop;

public:
	friend class mail::pop3maildrop::checkNewMailHelper;

	pop3acct(mail::pop3maildrop *myMaildropArg,
		 std::string url, std::string passwd,
		 std::vector<std::string> &certificates,
		 mail::loginCallback *loginCallbackFunc,
		 callback &callback,
		 callback::disconnect &disconnectCallback);
	~pop3acct();

	vector<mail::addMessage *> downloadedMsgs;
	size_t messageCount;

	bool ispop3maildrop();
	void pop3maildropreset();
	mail::addMessage *newDownloadMsg();
	string commitDownloadedMsgs();

private:
	bool errflag;
	string errmsg;

	// Inherited from mail::callback

	void success(std::string message);
	void fail(std::string message);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);


};


mail::pop3maildrop::pop3acct::pop3acct(mail::pop3maildrop *myMaildropArg,
				       std::string url, std::string passwd,
				       std::vector<std::string> &certificates,
				       mail::loginCallback *loginCallbackFunc,
				       callback &callback,
				       callback::disconnect
				       &disconnectCallback)
	: pop3(url, passwd, certificates,
	       loginCallbackFunc, callback, disconnectCallback),
	  myMaildrop(myMaildropArg)
{
}

mail::pop3maildrop::pop3acct::~pop3acct()
{
	pop3maildropreset();
}

bool mail::pop3maildrop::pop3acct::ispop3maildrop()
{
	return true;
}

void mail::pop3maildrop::pop3acct::pop3maildropreset()
{
	vector<mail::addMessage *>::iterator b=downloadedMsgs.begin(),
		e=downloadedMsgs.end();

	while (b != e)
	{
		if (*b)
			(*b)->fail("Cancelled.");
		*b=NULL;
		++b;
	}

	downloadedMsgs.clear();
	errflag=false;
	return;
}

mail::addMessage *mail::pop3maildrop::pop3acct::newDownloadMsg()
{
	downloadedMsgs.push_back(NULL);

	return (downloadedMsgs.end()[-1]=new
		mail::maildir::addmessage(myMaildrop,
					  myMaildrop->path, *this));
}

string mail::pop3maildrop::pop3acct::commitDownloadedMsgs()
{
	messageCount=downloadedMsgs.size();

	vector<mail::addMessage *>::iterator b=downloadedMsgs.begin(),
		e=downloadedMsgs.end();

	while (b != e)
	{
		if (*b)
			(*b)->go();
		*b=NULL;
		++b;
	}
	downloadedMsgs.clear();
	if (!errflag)
		errmsg="";
	myMaildrop->maildir::checkNewMail(NULL);
	return errmsg;
}

void mail::pop3maildrop::pop3acct::success(std::string message)
{
}

void mail::pop3maildrop::pop3acct::fail(std::string message)
{
	errflag=true;
	errmsg=message;
}

void mail::pop3maildrop
::pop3acct::reportProgress(size_t bytesCompleted,
			   size_t bytesEstimatedTotal,
			   size_t messagesCompleted,
			   size_t messagesEstimatedTotal)
{
}

/////////////////////////////////////////////////////////////////////////////

mail::pop3maildrop::pop3maildrop(mail::callback::disconnect
				 &disconnect_callback,
				 mail::loginCallback *loginCallbackFunc,
				 mail::callback &callback,
				 std::string pathArg,
				 std::string pop3url,
				 std::string pop3pass,
				 std::vector<std::string> &certificates)
	: maildir((mail::callback::disconnect &)*this),
	  myPop3Acct(NULL)
{
	struct stat stat_buf;

	// Automatically create the maildir, if it doesn't exist.

	if (stat(pathArg.c_str(), &stat_buf) < 0 && errno == ENOENT)
	{
		if (!mail::maildir::maildirmake(pathArg, false))
		{
			callback.fail(strerror(errno));
			return;
		}
	}

	if (!init(callback, pathArg))
		return;
	maildir::ispop3maildrop=true;

	mail::loginInfo pop3LoginInfo;

	if (!mail::loginUrlDecode(pop3url, pop3LoginInfo) ||
	    (pop3LoginInfo.method != "pop3" &&
	     pop3LoginInfo.method != "pop3s"))
	{
		callback.fail("Invalid POP3 maildrop URL");
		return;
	}

	mail::pop3maildrop::checkNewMailHelper *h=
		new mail::pop3maildrop::checkNewMailHelper(NULL,
							   this,
							   &callback);
	if (!h)
	{
		callback.fail(strerror(errno));
		return;
	}

	try
	{
		myPop3Acct=new pop3acct(this,
					pop3url,
					pop3pass,
					certificates,
					loginCallbackFunc,
					*h,
					disconnect_callback);
		if (!myPop3Acct)
		{
			delete h;
			callback.fail(strerror(errno));
		}
		h->pop3acct=myPop3Acct;
	}
	catch (...)
	{
		delete h;
		throw;
	}
}


bool mail::pop3maildrop::hasCapability(string capability)
{
	if (capability == LIBMAIL_SINGLEFOLDER)
		return true;

	return maildir::hasCapability(capability);
}

string mail::pop3maildrop::getCapability(string capability)
{
	if (capability == LIBMAIL_SERVERTYPE)
	{
		return "pop3maildrop";
	}

	if (myPop3Acct)
	{
		if (capability == LIBMAIL_SERVERDESCR)
		{
			return myPop3Acct->getCapability(capability);
		}
	}

	return maildir::getCapability(capability);		
}


// Callbacks for maildir's disconnect/servererror logic:

void mail::pop3maildrop::disconnected(const char *errmsg)
{
}

void mail::pop3maildrop::servererror(const char *errmsg)
{
}

mail::pop3maildrop::~pop3maildrop()
{
	if (myPop3Acct)
		delete myPop3Acct;
}

void mail::pop3maildrop::logout(callback &callback)
{
	if (myPop3Acct)
		myPop3Acct->logout(callback);
	else
		maildir::logout(callback);
}

mail::pop3maildrop::checkNewMailHelper
::checkNewMailHelper(mail::pop3maildrop::pop3acct *pop3acctArg,
		     mail::pop3maildrop *maildiracctArg,
		     mail::callback *origCallbackArg)
	: messagesProcessed(0),
	  pop3acct(pop3acctArg),
	  maildiracct(maildiracctArg),
	  origCallback(origCallbackArg)
{
}

mail::pop3maildrop::checkNewMailHelper::~checkNewMailHelper()
{
}

void mail::pop3maildrop::checkNewMailHelper::success(std::string message)
{
	if (pop3acct->messageCount > 0)
	{
		messagesProcessed += pop3acct->messageCount;
		pop3acct->checkNewMail(*this);
		return;
	}
	mail::callback *c=origCallback;
	origCallback=NULL;
	delete this;
	c->success(message);

}

void mail::pop3maildrop::checkNewMailHelper::fail(std::string message)
{
	mail::callback *c=origCallback;
	origCallback=NULL;
	delete this;
	c->fail(message);
}

void mail::pop3maildrop::checkNewMailHelper
::reportProgress(size_t bytesCompleted,
		 size_t bytesEstimatedTotal,

		 size_t messagesCompleted,
		 size_t messagesEstimatedTotal)
{
	origCallback->reportProgress(bytesCompleted,
				     bytesEstimatedTotal,
				     messagesCompleted+messagesProcessed,
				     messagesEstimatedTotal+messagesProcessed);
}


void mail::pop3maildrop::checkNewMail(callback &callback)
{
	if (folderCallback && strcasecmp(folderPath.c_str(), "INBOX") == 0 &&
	    myPop3Acct)
	{
		mail::pop3maildrop::checkNewMailHelper *h=
			new mail::pop3maildrop::checkNewMailHelper(myPop3Acct,
								   this,
								   &callback);

		if (!h)
		{
			callback.fail(strerror(errno));
			return;
		}

		myPop3Acct->checkNewMail(*h);
		return;
	}

	mail::maildir::checkNewMail(callback);
}

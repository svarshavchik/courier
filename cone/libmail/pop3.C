/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "driver.H"
#include "mail.H"
#include "misc.H"
#include "addmessage.H"
#include "pop3.H"
#include "copymessage.H"
#include "search.H"
#include "imaphmac.H"
#include "base64.H"
#include "objectmonitor.H"
#include "libcouriertls.h"
#include "expungelist.H"
#include <sstream>
#include <iomanip>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <algorithm>
#include <vector>
#include <signal.h>
#include <fcntl.h>
#include <set>
#include <cstring>

#include "rfc2045/rfc2045.h"

using namespace std;

/////////////////////////////////////////////////////////////////////////////

LIBMAIL_START

static bool open_pop3(mail::account *&accountRet,
		      mail::account::openInfo &oi,
		      mail::callback &callback,
		      mail::callback::disconnect &disconnectCallback)
{
	mail::loginInfo pop3LoginInfo;

	if (!mail::loginUrlDecode(oi.url, pop3LoginInfo))
		return false;

	if (pop3LoginInfo.method != "pop3" &&
	    pop3LoginInfo.method != "pop3s")
		return false;

	accountRet=new mail::pop3(oi.url, oi.pwd,
				  oi.certificates,
				  oi.loginCallbackObj,
				  callback,
				  disconnectCallback);
	return true;
}

static bool pop3_remote(string url, bool &flag)
{
	mail::loginInfo pop3LoginInfo;

	if (!mail::loginUrlDecode(url, pop3LoginInfo))
		return false;

	if (pop3LoginInfo.method != "pop3" &&
	    pop3LoginInfo.method != "pop3s")
		return false;

	flag=true;
	return true;
}

driver pop3_driver= { &open_pop3, &pop3_remote };

LIBMAIL_END

/////////////////////////////////////////////////////////////////////////////
//
// Most tasks will subclass from LoggedInTask which makes sure we're
// logged in to the POP3 server if we're not currently logged in.
//
// Exported methods:
//
// LoggedInTask(mail::pop3 &server) // Constructor
//
// mail::pop3 *myserver; // Inherited from Task
//
// void loggedIn(); // Logged in to the server, begin doing what you need to do
// void loginFailed(string errmsg); // Log in failed, about to be destroyed
//
// int getTimeout(); // Inherited from Task
// void disconnected(const char *reason); // Inherited from Task
// void serverResponse(const char *message); // Inherited from task
//

class mail::pop3::LoggedInTask : public mail::pop3::Task,
		  public mail::callback {

	// Inherited from mail::callback

	void success(string);
	void fail(string);

	void installedTask();

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	LoggedInTask(mail::callback *callback, mail::pop3 &myserverArg);
	~LoggedInTask();

	virtual void loggedIn()=0;
	virtual void loginFailed(string error)=0;
};

mail::pop3::Task::Task(mail::callback *callbackArg, mail::pop3 &myserverArg)
	: callbackPtr(callbackArg), myserver(&myserverArg)
{
	time(&defaultTimeout);
	defaultTimeout += myserverArg.timeoutSetting;
}

mail::pop3::Task::~Task()
{
	mail::callback *c=callbackPtr;

	callbackPtr=NULL;

	if (c)
		c->fail("POP3 task terminated unexpectedly.");
}

void mail::pop3::Task::installedTask()
{
	resetTimeout();
}

void mail::pop3::Task::resetTimeout()
{
	time(&defaultTimeout);
	defaultTimeout += myserver->timeoutSetting;
}

int mail::pop3::Task::getTimeout()
{
	time_t now;

	time(&now);

	if (now < defaultTimeout)
		return defaultTimeout - now;

	string errmsg=myserver->socketDisconnect();

	if (errmsg.size() == 0)
		errmsg="Server timed out.";

	disconnected(errmsg.c_str());
	return 0;
}

void mail::pop3::Task::disconnected(const char *reason)
{
	if (myserver->tasks.empty() ||
	    (*myserver->tasks.begin()) != this)
		kill(getpid(), SIGABRT);

	mail::callback *c=callbackPtr;

	callbackPtr=NULL;

	try {
		myserver->tasks.erase(myserver->tasks.begin());

		if (!myserver->tasks.empty())
			(*myserver->tasks.begin())->disconnected(reason);
		delete this;

		if (c)
			c->fail(reason);
	} catch (...) {
		if (c)
			c->fail(reason);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::pop3::Task::done()
{
	mail::callback *c=callbackPtr;

	callbackPtr=NULL;

	if (myserver->tasks.empty() || (*myserver->tasks.begin()) != this)
		kill(getpid(), SIGABRT);

	myserver->tasks.erase(myserver->tasks.begin());

	mail::pop3 *s=myserver;

	bool flag=isLogoutTask();

	delete this;

	if (!s->tasks.empty())
		(*s->tasks.begin())->installedTask();
	else if (!flag)
		time(&s->lastTaskCompleted);

	if (c)
		c->fail("POP3 task aborted.");
}

bool mail::pop3::Task::isLogoutTask()
{
	return false;
}

bool mail::pop3::Task::willReconnect()
{
	return false;
}

/////////////////////////////////////////////////////////////////////////////
//
// Log into the server, and create mail::pop3::uidlmap.

class mail::pop3::LoginTask : public mail::pop3::Task,
	    public mail::loginInfo::callbackTarget {

	void fail(const char *reason);

	void (LoginTask::*currentHandler)(const char *status);

	void serverResponse(const char *message);

	void installedTask();

	void greetingHandler(const char *message);
	void capaHandler(const char *message);
	void stlsHandler(const char *message);
	void stlsCapaHandler(const char *message);

	int hmac_index;	// Next HMAC method to try.
	void hmacHandler(const char *message);
	void hmacChallengeHandler(const char *message);
	void userHandler(const char *message);
	void passHandler(const char *message);
	void listHandler(const char *message);
	void uidlHandler(const char *message);

	void addCapability(const char *message);
	void addStlsCapability(const char *message);
	void processExternalLogin(const char *message);

	void processedCapabilities();
	void loggedIn(const char *message);

	void (mail::pop3::LoginTask::*login_callback_handler)(std::string);

	void loginInfoCallback(std::string);
	void loginInfoCallbackCancel();

	void loginCallbackUid(std::string);
	void loginCallbackPwd(std::string);

	void stlsCapaDone();
	void nonExternalLogin();
public:
	LoginTask(mail::pop3 &server, mail::callback *callbackArg);
	~LoginTask();
};

mail::pop3::LoginTask::LoginTask(mail::pop3 &server,
				 mail::callback *callbackArg)
	: Task(callbackArg, server)
{
}

mail::pop3::LoginTask::~LoginTask()
{
}

void mail::pop3::LoginTask::fail(const char *reason)
{
	mail::callback *c=callbackPtr;

	callbackPtr=NULL;

	try {
		string errmsg="ERROR: ";

		if (strncmp(reason, "-ERR", 4) == 0)
		{
			reason += 4;

			while (*reason == ' ')
				reason++;
		}

		errmsg += reason;

		done();

		if (c)
			c->fail(errmsg.c_str());
	} catch (...) {
		if (c)
			c->fail("ERROR: An exception occured in mail::pop3");
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::pop3::LoginTask::serverResponse(const char *message)
{
	if (currentHandler)
		(this->*currentHandler)(message);
}

void mail::pop3::LoginTask::installedTask()
{
	Task::installedTask();

	if (myserver->getfd() >= 0)
	{
		loggedIn("+OK Already logged in."); // Already
		return;
	}

	currentHandler= &LoginTask::greetingHandler;

	myserver->pop3LoginInfo = myserver->savedLoginInfo;

        string errmsg=myserver->
		socketConnect(myserver->pop3LoginInfo, "pop3", "pop3s");

        if (errmsg.size() > 0)
        {
		fail(errmsg.c_str());
                return;
        }
}

// First message from the server.

void mail::pop3::LoginTask::greetingHandler(const char *message)
{
	if (*message != '+')
	{
		myserver->socketDisconnect();
		fail(message);
		return;
	}

	myserver->socketWrite("CAPA\r\n");
	currentHandler= &LoginTask::capaHandler;
}

// CAPA reply.

void mail::pop3::LoginTask::capaHandler(const char *message)
{
	myserver->capabilities.clear();

	if (*message == '-')
	{
		processedCapabilities(); // No capabilities
		return;
	}

	if (*message != '+')
	{
		fail(message);
		return;
	}
	currentHandler=&LoginTask::addCapability;
}

// Capability list.

void mail::pop3::LoginTask::addCapability(const char *message)
{
	if (strcmp(message, ".") == 0)
	{
		processedCapabilities();
		return;
	}

	string c=message, v="";
	size_t p=c.find(' ');

	if (p != std::string::npos)
	{
		v=c.substr(p+1);
		c=c.substr(0, p);
	}

	// Add AUTH=method capability for each SASL listed method.

	if (c == "SASL")
	{
		while ((p=v.find(' ')) != std::string::npos)
		{
			c="AUTH=" + v.substr(0, p);
			v=v.substr(p+1);
			myserver->capabilities.insert(make_pair(c,
							       string("1")));
		}
		c="AUTH=" + v;
		v="1";
	}

	if (v.size() == 0)
		v="1";

	myserver->capabilities.insert(make_pair(c, v));
}

void mail::pop3::LoginTask::processedCapabilities()
{
#if HAVE_LIBCOURIERTLS

	if (myserver->pop3LoginInfo.use_ssl
	    || myserver->pop3LoginInfo.options.count("notls") > 0
	    || !myserver->hasCapability("STLS"))
	{
		stlsCapaDone();
		return;
	}

	currentHandler=&LoginTask::stlsHandler;
	myserver->socketWrite("STLS\r\n");
#else
	stlsCapaDone();
#endif
}

void mail::pop3::LoginTask::stlsHandler(const char *message)
{
	if (*message != '+')
	{
		fail(message);
		return;
	}

#if HAVE_LIBCOURIERTLS

	myserver->pop3LoginInfo.callbackPtr=callbackPtr;

	callbackPtr=NULL;
	if (!myserver->socketBeginEncryption(myserver->pop3LoginInfo))
		return; // Can't set callbackPtr to NULL now, because
	// this object could be destroyed.

	// If beginEcnryption() succeeded, restore the callback ptr.
	callbackPtr=myserver->pop3LoginInfo.callbackPtr;
#endif
	currentHandler=&LoginTask::stlsCapaHandler;
	myserver->socketWrite("CAPA\r\n");
}

void mail::pop3::LoginTask::stlsCapaHandler(const char *message)
{
	myserver->capabilities.clear();

	if (*message == '-')
	{
		processedCapabilities(); // No capabilities
		return;
	}

	if (*message != '+')
	{
		fail(message);
		return;
	}
	currentHandler=&LoginTask::addStlsCapability;
}

void mail::pop3::LoginTask::addStlsCapability(const char *message)
{
	if (strcmp(message, ".") == 0)
	{
		stlsCapaDone();
		return;
	}
	addCapability(message);
}

void mail::pop3::LoginTask::stlsCapaDone()
{
	if (!myserver->hasCapability("AUTH=EXTERNAL"))
	{
		nonExternalLogin();
		return;
	}

	currentHandler=&LoginTask::processExternalLogin;
	myserver->socketWrite("AUTH EXTERNAL =\r\n");
}

void mail::pop3::LoginTask::processExternalLogin(const char *message)
{
	if (*message != '+')
	{
		nonExternalLogin();
		return;
	}

	loggedIn(message);
}

void mail::pop3::LoginTask::nonExternalLogin()
{
	if (!myserver->pop3LoginInfo.loginCallbackFunc)
	{
		loginCallbackPwd(myserver->pop3LoginInfo.pwd);
		return;
	}

	currentHandler=NULL;
	if (myserver->pop3LoginInfo.uid.size() > 0)
	{
		loginCallbackUid(myserver->pop3LoginInfo.uid);
		return;
	}

	login_callback_handler= &mail::pop3::LoginTask::loginCallbackUid;
	currentCallback=myserver->pop3LoginInfo.loginCallbackFunc;
	currentCallback->target=this;
	currentCallback->getUserid();
}

void mail::pop3::LoginTask::loginInfoCallback(std::string cbvalue)
{
	currentCallback=NULL;
	(this->*login_callback_handler)(cbvalue);
}


void mail::pop3::LoginTask::loginInfoCallbackCancel()
{
	currentCallback=NULL;
	fail("Login cancelled.");
}

void mail::pop3::LoginTask::loginCallbackUid(std::string uid)
{
	myserver->savedLoginInfo.uid=
		myserver->pop3LoginInfo.uid=uid;

	if (myserver->pop3LoginInfo.pwd.size() > 0)
	{
		loginCallbackPwd(myserver->pop3LoginInfo.pwd);
		return;
	}

	login_callback_handler= &mail::pop3::LoginTask::loginCallbackPwd;
	currentCallback=myserver->pop3LoginInfo.loginCallbackFunc;
	currentCallback->target=this;
	currentCallback->getPassword();
}

void mail::pop3::LoginTask::loginCallbackPwd(std::string pwd)
{
	myserver->savedLoginInfo.pwd=myserver->pop3LoginInfo.pwd=pwd;

	hmac_index=0;
	hmacHandler("-ERR Login failed"); // Not really
}

void mail::pop3::LoginTask::hmacHandler(const char *message)
{
	if (*message == '+')
	{
		loggedIn(message);
		return;
	}

	for (;;)
	{
		if (mail::imaphmac::hmac_methods[hmac_index] == NULL ||
		    myserver->pop3LoginInfo.uid.size() == 0)
		{
			if (myserver->pop3LoginInfo.options.count("cram"))
				fail(message);

			if (myserver->pop3LoginInfo.uid.size() == 0)
			{
				loggedIn("+OK No userid, assuming preauthenticated login.");
				return;
			}

			currentHandler=&LoginTask::userHandler;
			myserver->socketWrite("USER " +
					     myserver->pop3LoginInfo.uid
					     + "\r\n");
			return;
		}

		if (!myserver->hasCapability(string("AUTH=CRAM-") +
					    mail::imaphmac
					    ::hmac_methods[hmac_index]
					    ->getName()))
		{
			++hmac_index;
			continue;
		}
		break;
	}

	currentHandler=&LoginTask::hmacChallengeHandler;
	myserver->socketWrite(string("AUTH CRAM-") +
			     mail::imaphmac::hmac_methods[hmac_index]
			     ->getName() + "\r\n");
}

void mail::pop3::LoginTask::hmacChallengeHandler(const char *message)
{
	if (*message != '+')
	{
		++hmac_index;
		hmacHandler(message);
		return;
	}

	do
	{
		++message;
	} while (*message == ' ');

	string s=mail::decodebase64str(message);

	s= (*mail::imaphmac::hmac_methods[hmac_index++])
		(myserver->pop3LoginInfo.pwd, s);
	string sHex;

	{
		ostringstream hexEncode;

		hexEncode << hex;

		string::iterator b=s.begin();
		string::iterator e=s.end();

		while (b != e)
			hexEncode << setw(2) << setfill('0')
				  << (int)(unsigned char)*b++;
		sHex=hexEncode.str();
	}

	s=mail::encodebase64str(myserver->pop3LoginInfo.uid + " " + sHex);
	currentHandler=&LoginTask::hmacHandler;
	myserver->socketWrite(s + "\r\n");
}

void mail::pop3::LoginTask::userHandler(const char *message)
{
	if (*message != '+')
	{
		fail(message);
		return;
	}

	currentHandler=&LoginTask::passHandler;
	myserver->socketWrite("PASS " +
			     myserver->pop3LoginInfo.pwd + "\r\n");
}

void mail::pop3::LoginTask::passHandler(const char *message)
{
	if (*message != '+')
	{
		fail(message);
		return;
	}
	loggedIn(message);
}

void mail::pop3::LoginTask::loggedIn(const char *message)
{
	myserver->savedLoginInfo.loginCallbackFunc=
		myserver->pop3LoginInfo.loginCallbackFunc=NULL; // Got it
	currentHandler=&LoginTask::listHandler;
	myserver->listMap.clear();
	myserver->socketWrite("LIST\r\n");
}

void mail::pop3::LoginTask::listHandler(const char *message)
{
	if (*message == '-')
	{
		fail(message);
		return;
	}

	if (*message == '+')
		return; // LIST results follow

	if (strcmp(message, ".") == 0)
	{
		myserver->uidlMap.clear();

		if (!myserver->ispop3maildrop())
		{
			currentHandler=&LoginTask::uidlHandler;
			myserver->socketWrite("UIDL\r\n");
			return;
		}

		uidlHandler("."); /* Don't do UIDL for pop3 maildrops */
		return;
	}

	int messageNum=0;
	unsigned long messageSize=0;

	istringstream i(message);

	i >> messageNum;

	i >> messageSize;

	myserver->listMap.insert(make_pair(messageNum, messageSize));
}

void mail::pop3::LoginTask::uidlHandler(const char *message)
{
	if (*message == '-')
	{
		fail("-ERR This POP3 server does not appear to implement the POP3 UIDL command.  Upgrade the POP3 server software, or use the POP3 maildrop mode.");
		return;
	}

	if (*message == '+')
		return; // UIDL results follow

	if (strcmp(message, ".") == 0)
	{
		mail::callback *c=callbackPtr;

		callbackPtr=NULL;

		done();

		if (c)
		{
			c->success("Logged in.");
		}
		return;
	}

	int msgNum=0;

	while (*message && isdigit((int)(unsigned char)*message))
		msgNum=msgNum * 10 + (*message++ - '0');

	while (*message && unicode_isspace((unsigned char)*message))
		message++;

	myserver->uidlMap.insert(make_pair(string(message), msgNum));
}


//////////////////////////////////////////////////////////////////////////////

mail::pop3::LoggedInTask::LoggedInTask(mail::callback *callback,
				       mail::pop3 &myserverArg)
	: Task(callback, myserverArg)
{
}

mail::pop3::LoggedInTask::~LoggedInTask()
{
	mail::callback *c=callbackPtr;

	callbackPtr=NULL;

	if (c)
		c->fail("An exception occurred while trying to initialize this task.");
}

void mail::pop3::LoggedInTask::reportProgress(size_t bytesCompleted,
					      size_t bytesEstimatedTotal,

					      size_t messagesCompleted,
					      size_t messagesEstimatedTotal)
{
	if (callbackPtr)
		callbackPtr->
			reportProgress(bytesCompleted, bytesEstimatedTotal,
				       messagesCompleted,
				       messagesEstimatedTotal);
}

void mail::pop3::LoggedInTask::installedTask()
{
	Task::installedTask();

	if (myserver->getfd() >= 0) // Logged in, yippee!!
	{
		loggedIn();
		return;
	}

	myserver->installTask(new LoginTask( *myserver, this));

	if ((*myserver->tasks.begin()) != this)
		kill(getpid(), SIGABRT);

	myserver->tasks.erase(myserver->tasks.begin()); // myself

	(*myserver->tasks.begin())->installedTask();
	// Guaranteed something's there.
}

void mail::pop3::LoggedInTask::success(string message)
{
	myserver->installTask(this); // Try again
}

void mail::pop3::LoggedInTask::fail(string message)
{
	mail::callback *c=callbackPtr;

	callbackPtr=NULL;

	delete this;

	if (c)
		c->fail(message);
}

/////////////////////////////////////////////////////////////////////////////

class mail::pop3::CheckNewMailTask : public mail::pop3::LoggedInTask {

public:
	bool forceSaveSnapshot;

	class sortNewMail {

		map<string, int> &umap;
	public:
		sortNewMail(map<string, int> &umapArg);
		~sortNewMail();

		bool operator()(string a, string b);
	};


	CheckNewMailTask(mail::pop3 &serverArg, mail::callback &callbackArg);
	~CheckNewMailTask();

	void loggedIn();
	void loginFailed(string errmsg);

	void serverResponse(const char *message);

	void done();
private:
	void (CheckNewMailTask::*currentHandler)(const char *status);
	int next2Download; // When subclass by pop3maildrop, next msg 2 get.
	bool willReconnectFlag;

	bool willReconnect();


	void doNextDownload();
	mail::addMessage *downloadMsg;

	void retrHandler(const char *);
	void retrBodyHandler(const char *);
	void doDeleteDownloaded();
	void deleHandler(const char *);
	void quitHandler(const char *);

	size_t bytesCompleted;
	size_t bytesEstimatedTotal;

	size_t messagesEstimatedTotal;

};

mail::pop3::CheckNewMailTask::CheckNewMailTask(mail::pop3 &serverArg,
					       mail::callback &callbackArg)
	: LoggedInTask(&callbackArg, serverArg),
	  forceSaveSnapshot(false), willReconnectFlag(false)
{
}

mail::pop3::CheckNewMailTask::~CheckNewMailTask()
{
}

void mail::pop3::CheckNewMailTask::serverResponse(const char *message)
{
	if (currentHandler)
		(this->*currentHandler)(message);
}

void mail::pop3::CheckNewMailTask::loggedIn()
{
	mail::callback *myCallback=callbackPtr;

	callbackPtr=NULL;

	mail::ptr<mail::pop3> serverPtr= myserver;

	bool changedFolder=myserver->reconcileFolderIndex();

	if (forceSaveSnapshot)
		changedFolder=true; // Explicit check new mail saves snapshot

	//
	// Now, save a snapshot.
	//
	if (!serverPtr.isDestroyed() && serverPtr->folderCallback &&
	    changedFolder)
		serverPtr->folderCallback->saveSnapshot("POP3");

	if (!serverPtr.isDestroyed())
	{
		if (serverPtr->ispop3maildrop())
		{
			serverPtr->pop3maildropreset();

			callbackPtr=myCallback;

			next2Download=1;
			messagesEstimatedTotal=myserver->listMap.size();
			doNextDownload();
			return;
		}
		done();
	}

	if (myCallback)
		myCallback->success("OK");
}

void mail::pop3::CheckNewMailTask::loginFailed(string errmsg)
{
	mail::callback *c=callbackPtr;

	callbackPtr=NULL;

	done();

	if (c)
		c->fail(errmsg);
}

mail::pop3::CheckNewMailTask::sortNewMail
::sortNewMail(map<string, int> &umapArg)
	: umap(umapArg)
{
}

mail::pop3::CheckNewMailTask::sortNewMail::~sortNewMail()
{
}

bool mail::pop3::CheckNewMailTask::sortNewMail::operator()(string a,
							   string b)
{
	map<string, int>::iterator ap, bp;

	ap=umap.find(a);
	bp=umap.find(b);

	return ((ap == umap.end() ? 0:ap->second) <
		(bp == umap.end() ? 0:bp->second));
}


void mail::pop3::CheckNewMailTask::doNextDownload()
{
	std::map<int, unsigned long>::iterator p=
		myserver->listMap.find(next2Download);

	bytesCompleted=0;
	bytesEstimatedTotal=0;

	if (next2Download > 200 ||
	    // Grab max 200 msgs at a time to prevent out of file descriptors
	    p == myserver->listMap.end())
	{
		string errmsg=myserver->commitDownloadedMsgs();

		if (errmsg.size() > 0)
		{
			mail::callback *c=callbackPtr;
			callbackPtr=NULL;
			done();
			if (c)
				c->fail(errmsg.c_str());
			return;
		}
		if (callbackPtr)
			callbackPtr->reportProgress(0, 0,
						    messagesEstimatedTotal,
						    messagesEstimatedTotal);
		doDeleteDownloaded();
		return;
	}


	downloadMsg=myserver->newDownloadMsg();

	if (!downloadMsg)
	{
		mail::callback *c=callbackPtr;
		callbackPtr=NULL;
		done();
		if (c)
			c->fail(strerror(errno));
		return;
	}
	downloadMsg->messageInfo.unread=true; /* New message */

	currentHandler=&CheckNewMailTask::retrHandler;

	ostringstream o;

	bytesCompleted=0;

	if ((size_t)next2Download > messagesEstimatedTotal)
		messagesEstimatedTotal=next2Download;

	map<int, unsigned long>::iterator s=
		myserver->listMap.find(next2Download);

	bytesEstimatedTotal=s == myserver->listMap.end() ? 0:
		s->second;

	if (callbackPtr)
		callbackPtr->reportProgress(0,
					    bytesEstimatedTotal,
					    next2Download-1,
					    messagesEstimatedTotal);
	o << "RETR " << next2Download << "\r\n";

	myserver->socketWrite(o.str());
	++next2Download;
}


void mail::pop3::CheckNewMailTask::retrHandler(const char *message)
{
	if (*message != '+')
	{
		mail::callback *c=callbackPtr;
		callbackPtr=NULL;
		done();
		if (c)
			c->fail(message);
		return;
	}

	currentHandler=&CheckNewMailTask::retrBodyHandler;
}

void mail::pop3::CheckNewMailTask::retrBodyHandler(const char *message)
{
	if (strcmp(message, "."))
	{
		if (*message == '.')
			++message;

		bytesCompleted += strlen(message)+2;
		if (bytesCompleted > bytesEstimatedTotal)
			bytesCompleted=bytesEstimatedTotal;
		if (callbackPtr)
			callbackPtr->reportProgress(bytesCompleted,
						    bytesEstimatedTotal,
						    next2Download-1,
						    messagesEstimatedTotal);
		downloadMsg->saveMessageContents(string(message)+"\n");
		return;
	}

	doNextDownload();
}

void mail::pop3::CheckNewMailTask::doDeleteDownloaded()
{
	if (--next2Download <= 0)
	{
		willReconnectFlag=true;
		currentHandler=&CheckNewMailTask::quitHandler;
		myserver->socketWrite("QUIT\r\n");
		return;
	}

	currentHandler=&CheckNewMailTask::deleHandler;

	ostringstream o;

	o << "DELE " << next2Download << "\r\n";
	myserver->socketWrite(o.str());
}

void mail::pop3::CheckNewMailTask::deleHandler(const char *message)
{
	doDeleteDownloaded();
}

void mail::pop3::CheckNewMailTask::quitHandler(const char *message)
{
	if (myserver->socketEndEncryption())
		return; // Catch me on the rebound.

	mail::callback *c=callbackPtr;

	callbackPtr=NULL;

	myserver->socketDisconnect();
	myserver->orderlyShutdown=true;
	myserver->disconnect("");

	if (c)
		c->success("Ok");
}


bool mail::pop3::CheckNewMailTask::willReconnect()
{
	return willReconnectFlag;
}

void mail::pop3::CheckNewMailTask::done()
{
	if (!willReconnectFlag)
	{
		LoggedInTask::done();
		return;
	}

	mail::callback *c=callbackPtr;
	callbackPtr=NULL;
	LoggedInTask::done();
	if (c)
		c->success("Ok.");
}

/////////////////////////////////////////////////////////////////////////////

class mail::pop3::LogoutTask : public mail::pop3::Task {

	void fail(const char *reason);

	void serverResponse(const char *message);

	void installedTask();
	bool isLogoutTask();

	mail::callback *disconnectCallback;

	bool willReconnectFlag;

public:
	LogoutTask(mail::pop3 &server, mail::callback *callbackArg,
		   bool willReconnectFlagArg=false);
	~LogoutTask();

	bool willReconnect();
};

mail::pop3::LogoutTask::LogoutTask(mail::pop3 &server,
				   mail::callback *callbackArg,
				   bool willReconnectFlagArg)
	: Task(callbackArg, server),
	  disconnectCallback(NULL),
	  willReconnectFlag(willReconnectFlagArg)
{
}

bool mail::pop3::LogoutTask::willReconnect()
{
	return willReconnectFlag;
}

mail::pop3::LogoutTask::~LogoutTask()
{
	myserver->socketDisconnect();
	if (disconnectCallback)
		disconnectCallback->success("OK");
}

bool mail::pop3::LogoutTask::isLogoutTask()
{
	return true;
}

void mail::pop3::LogoutTask::installedTask()
{
	Task::installedTask();

	if (myserver->getfd() < 0)
	{
		serverResponse("OK"); // Already logged out.
		return;
	}

	myserver->socketWrite("QUIT\r\n");
}

void mail::pop3::LogoutTask::serverResponse(const char *message)
{
	const char *p=strchr(message, ' ');

	if (p)
		message=p+1;

	mail::callback *c=callbackPtr;

	callbackPtr=NULL;

	disconnectCallback=c;

	if (myserver->socketEndEncryption())
		return; // Catch me on the rebound.

	myserver->socketDisconnect();
	myserver->orderlyShutdown=true;
	myserver->disconnect("");
}

/////////////////////////////////////////////////////////////////////////////

class mail::pop3::ForceCheckNewMailTask : public mail::callback {

	mail::callback *realCallback;
	mail::pop3 *server;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	ForceCheckNewMailTask(mail::callback *realCallbackArg,
			      mail::pop3 *serverArg);
	~ForceCheckNewMailTask();

	void success(string);
	void fail(string);
};

mail::pop3::ForceCheckNewMailTask
::ForceCheckNewMailTask(mail::callback *realCallbackArg,
			mail::pop3 *serverArg)
	: realCallback(realCallbackArg), server(serverArg)
{
}

mail::pop3::ForceCheckNewMailTask::~ForceCheckNewMailTask()
{
}

void mail::pop3::ForceCheckNewMailTask
::reportProgress(size_t bytesCompleted,
		 size_t bytesEstimatedTotal,

		 size_t messagesCompleted,
		 size_t messagesEstimatedTotal)
{
	if (realCallback)
		realCallback->reportProgress(bytesCompleted,
					     bytesEstimatedTotal,
					     messagesCompleted,
					     messagesEstimatedTotal);
}

//
// checkNewMail installed a LogoutTask.  LogoutTask's destructor ends up
// calling the logout callback function, which is me.
// LogoutTask is destroyed by LogoutTask::Task::done()'s delete this.
//
// Install the CheckNewMailTask at the beginning of the task queue, so
// Task::done() ends up calling CheckNewMailTask's installedTask.

void mail::pop3::ForceCheckNewMailTask::success(string message)
{
	CheckNewMailTask *cn=new CheckNewMailTask(*server, *realCallback);

	if (!cn)
		realCallback->fail(strerror(errno));

	try {
		cn->forceSaveSnapshot=true;
		server->lastTaskCompleted=0;
		server->tasks.insert(server->tasks.begin(), cn);
		delete this;
	} catch (...) {
		delete cn;
		realCallback->fail(message);
		delete this;
	}
}

void mail::pop3::ForceCheckNewMailTask::fail(string errmsg)
{
	success(errmsg);
}

/////////////////////////////////////////////////////////////////////////////
//
// EXPUNGE - sent all the DELEs, then QUIT out of the folder, then reopen it.

class mail::pop3::UpdateTask : public mail::pop3::LoggedInTask {

	size_t updateIndex;

#if 0
	class LogoutCallback : public mail::callback {
		mail::ptr<mail::pop3> myserver;
		mail::callback *callback;
	public:
		LogoutCallback(mail::pop3 *serverArg,
			       mail::callback *callbackArg);
		~LogoutCallback();

		void success(string);
		void fail(string);
	};
#endif

public:
	UpdateTask(mail::pop3 &serverArg, mail::callback &callbackArg);
	~UpdateTask();

	void loggedIn();
	void loginFailed(string errmsg);

	void serverResponse(const char *message);
};

mail::pop3::UpdateTask::UpdateTask(mail::pop3 &serverArg,
				   mail::callback &callbackArg)
	: LoggedInTask(&callbackArg, serverArg),
	  updateIndex(0)
{
}

mail::pop3::UpdateTask::~UpdateTask()
{
}

void mail::pop3::UpdateTask::loggedIn()
{
	serverResponse("+OK");
}

void mail::pop3::UpdateTask::loginFailed(string errmsg)
{
	mail::callback *c=callbackPtr;

	callbackPtr=NULL;

	done();

	if (c)
		c->fail(errmsg);
}

void mail::pop3::UpdateTask::serverResponse(const char *message)
{
	if (*message != '+')
	{
		loginFailed(message);
		return;
	}

	// Check for deleted messages.

	while (updateIndex < myserver->currentFolderIndex.size())
	{
		string uid;

		if (myserver->currentFolderIndex[updateIndex].deleted &&
		    myserver->uidlMap.count(uid=myserver->currentFolderIndex
					   [updateIndex].uid) > 0)
		{
			int msgNum=myserver->uidlMap.find(uid)->second;

			string buffer;
			{
				ostringstream o;

				o << msgNum;
				buffer=o.str();
			}

			++updateIndex;
			myserver->socketWrite(string("DELE ") + buffer
					     + "\r\n");
			return;
		}
		++updateIndex;
	}

	// Do the usual logout logic, which will take care of this.

	ForceCheckNewMailTask *l=
		new ForceCheckNewMailTask(callbackPtr, myserver);

	if (!l)
	{
		loginFailed("Out of memory.");
		return;
	}

	callbackPtr=NULL;

	if ( (*myserver->tasks.begin()) != this)
		kill(getpid(), SIGABRT);

	try {
		LogoutTask *lo=new LogoutTask( *myserver, l, true);

		if (!lo)
			strerror(errno);

		try {
			myserver->tasks.insert(++myserver->tasks.begin(),
					      lo);
		} catch (...) {
			delete lo;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	} catch (...) {
		delete l;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
	done();
}

#if 0
mail::pop3::UpdateTask::LogoutCallback::LogoutCallback(mail::pop3 *serverArg,
						       mail::callback
						       *callbackArg)
	: myserver(serverArg), callback(callbackArg)
{
}

mail::pop3::UpdateTask::LogoutCallback::~LogoutCallback()
{
}

void mail::pop3::UpdateTask::LogoutCallback::success(string message)
{
	mail::callback *c=callback;
	mail::pop3 *s=myserver;

	callback=NULL;
	delete this;

	if (!s)
		c->success(message);
	else try {
		s->checkNewMail(*c); // Immediately check for new mail.
	} catch (...) {
		c->success(message);
	}
}

void mail::pop3::UpdateTask::LogoutCallback::fail(string message)
{
	success(message); // I'm an optimist.
}
#endif

/////////////////////////////////////////////////////////////////////////////
//
// Generic message read.

class mail::pop3::ReadMessageTask : public mail::pop3::LoggedInTask {

	string uid;
	size_t messageNum;
	bool peek;
	mail::readMode readType;

	bool seenHeaders;
	string currentHeader;
	bool expectingStatus;

	size_t expected_cnt;
	size_t received_cnt;

	mail::callback::message *msgCallback;

public:
	ReadMessageTask(mail::pop3 &serverArg,
			mail::callback::message &callbackArg,
			string uidArg,
			size_t messageNumArg,
			bool peekArg,
			mail::readMode readTypeArg);
	~ReadMessageTask();

	void loggedIn();
	void loginFailed(string errmsg);

	void serverResponse(const char *message);
};

mail::pop3::ReadMessageTask::ReadMessageTask(mail::pop3 &serverArg,
					     mail::callback::message
					     &callbackArg,
					     string uidArg,
					     size_t messageNumArg,
					     bool peekArg,
					     mail::readMode readTypeArg)
	: LoggedInTask(&callbackArg, serverArg),
	  uid(uidArg),
	  messageNum(messageNumArg),
	  peek(peekArg),
	  readType(readTypeArg),
	  msgCallback(&callbackArg)
{
}

mail::pop3::ReadMessageTask::~ReadMessageTask()
{
}

void mail::pop3::ReadMessageTask::loggedIn()
{
	map<string, int>::iterator p=myserver->uidlMap.find(uid);

	if (p == myserver->uidlMap.end())
	{
		loginFailed("Message was deleted by the POP3 server.");
		return;
	}

	int msgNum=p->second;

	seenHeaders=false;
	currentHeader="";
	expectingStatus=true;

	if (!fixMessageNumber(myserver, uid, messageNum))
	{
		expectingStatus=false;
		serverResponse(".");
		return;
	}

	string buffer;

	{
		ostringstream o;

		o << msgNum;
		buffer=o.str();
	}

	string cmd;

	expected_cnt=0;
	received_cnt=0;

	if (readType == mail::readContents ||
	    readType == mail::readBoth)
	{
		cmd=string("RETR ") + buffer + "\r\n";

		map<int, unsigned long>::iterator p
			=myserver->listMap.find(msgNum);

		if (p != myserver->listMap.end())
			expected_cnt=p->second;
	}
	else
		cmd=string("TOP ") + buffer + " 0\r\n";

	myserver->socketWrite(cmd);
}

void mail::pop3::ReadMessageTask::loginFailed(string errmsg)
{
	mail::callback *c=callbackPtr;

	callbackPtr=NULL;

	done();

	if (c)
		c->fail(errmsg);
}

void mail::pop3::ReadMessageTask::serverResponse(const char *message)
{
	if (expectingStatus)
	{
		expectingStatus=false;
		if (*message != '+')
			loginFailed(message);
		return;
	}

	time(&defaultTimeout);
	defaultTimeout += myserver->timeoutSetting;
	// Some activity -- don't time out

	if (strcmp(message, ".") == 0)
	{
		mail::callback::message *c=msgCallback;

		mail::ptr<mail::pop3> ptr= myserver;

		bool peekFlag=peek;

		if (!fixMessageNumber(myserver, uid, messageNum))
			peekFlag=true;

		if (callbackPtr == NULL)
			c=NULL;

		callbackPtr=NULL;
		msgCallback=NULL;

		if (currentHeader.size() > 0)
		{
			if (c)
				c->messageTextCallback(0,
						       currentHeader + "\n");
		}

		size_t n=messageNum;

		received_cnt=expected_cnt;
		if (c)
			c->reportProgress(expected_cnt, expected_cnt, 1, 1);

		done();

		if (c)
			c->success("OK");

		if (!peekFlag && !ptr.isDestroyed())
		{
			ptr->currentFolderIndex[n].unread=false;
			if (ptr->folderCallback)
				ptr->folderCallback
					->messageChanged(n);

			if (!ptr.isDestroyed() && ptr->folderCallback)
				ptr->folderCallback->saveSnapshot("POP3");
		}

		return;
	}

	if (*message == '.')
		++message;

	received_cnt += strlen(message)+2;

	if (callbackPtr && msgCallback)
		msgCallback->reportProgress(received_cnt, expected_cnt, 0, 1);


	if (readType == mail::readHeadersFolded)
		// Reformat headers, one per line
	{
		if (!*message) // End of headers
		{
			string s=currentHeader;

			currentHeader="";

			if (s.size() > 0 && callbackPtr && msgCallback)
				msgCallback->messageTextCallback(0, s + "\n");
			return;
		}

		if (unicode_isspace((unsigned char)*message))
		{
			while (*message &&
			       unicode_isspace((unsigned char)*message))
				++message;

			if (currentHeader.size() > 0)
				currentHeader += " ";
			currentHeader += message;
			return;
		}

		string s=currentHeader;
		currentHeader=message;

		if (callbackPtr && msgCallback && s.size() > 0)
			msgCallback->messageTextCallback(0, s + "\n");
		return;
	}

	if (readType == mail::readContents && !seenHeaders)
	{
		if (!*message)
			seenHeaders=true;
		return; // Eat the headers
	}

	string line=message;

	line += "\n";

	if (callbackPtr && msgCallback)
		msgCallback->messageTextCallback(0, line);
}


/////////////////////////////////////////////////////////////////////////////

class mail::pop3::CacheMessageTask : public mail::callback::message {

	mail::ptr<mail::pop3> myserver;

	mail::callback *callback;

	string uid;

	int *fdptr;
	struct rfc2045 **rfcptr;

	FILE *tmpfilefp;
	struct rfc2045 *rfc;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	CacheMessageTask(mail::pop3 *myserverArg,
			 mail::callback *callbackArg,
			 string uidArg,
			 int *fdArg,
			 struct rfc2045 **rfcArg);
	~CacheMessageTask();

	bool init();

	void success(string);
	void fail(string);

	void messageTextCallback(size_t n, string text);
};

mail::pop3::CacheMessageTask::CacheMessageTask(mail::pop3 *myserverArg,
					       mail::callback *callbackArg,
					       string uidArg,
					       int *fdArg,
					       struct rfc2045 **rfcArg)
	: myserver(myserverArg),
	  callback(callbackArg),
	  uid(uidArg),
	  fdptr(fdArg),
	  rfcptr(rfcArg),
	  tmpfilefp(NULL),
	  rfc(NULL)
{
}

mail::pop3::CacheMessageTask::~CacheMessageTask()
{
	if (tmpfilefp)
		fclose(tmpfilefp);

	if (rfc)
		rfc2045_free(rfc);
}

void mail::pop3
::CacheMessageTask::reportProgress(size_t bytesCompleted,
				   size_t bytesEstimatedTotal,

				   size_t messagesCompleted,
				   size_t messagesEstimatedTotal)
{
	if (callback)
		callback->reportProgress(bytesCompleted,
					 bytesEstimatedTotal,
					 messagesCompleted,
					 messagesEstimatedTotal);
}

void mail::pop3::CacheMessageTask::success(string message)
{
	if (myserver.isDestroyed())
	{
		fail("POP3 server connection aborted.");
		     return;
	}

	if (fflush(tmpfilefp) || ferror(tmpfilefp))
	{
		fail(strerror(errno));
		return;
	}

	if (myserver->genericTmpFd >= 0)
		close(myserver->genericTmpFd);

	rfc2045_parse_partial(rfc);

	if (myserver->genericTmpRfcp)
	{
		rfc2045_free(myserver->genericTmpRfcp);
		myserver->genericTmpRfcp=NULL;
	}

	myserver->cachedUid=uid;

	if ((myserver->genericTmpFd=dup(fileno(tmpfilefp))) < 0)
	{
		fail(strerror(errno));
		return;
	}

	fcntl(myserver->genericTmpFd, F_SETFD, FD_CLOEXEC);

	if (fdptr)
		*fdptr=myserver->genericTmpFd;

	myserver->genericTmpRfcp=rfc;

	if (rfcptr)
		*rfcptr=rfc;

	rfc=NULL;

	mail::callback *c=callback;

	callback=NULL;

	delete this;
	c->success(message);
}

void mail::pop3::CacheMessageTask::fail(string message)
{
	mail::callback *c=callback;

	callback=NULL;

	delete this;
	c->fail(message);
}

bool mail::pop3::CacheMessageTask::init()
{
	if ((tmpfilefp=tmpfile()) == NULL ||
	    (rfc=rfc2045_alloc()) == NULL)
	{
		fail(strerror(errno));
		return false;
	}

	return true;
}

void mail::pop3::CacheMessageTask::messageTextCallback(size_t n, string text)
{
	if (fwrite(text.c_str(), text.size(), 1, tmpfilefp) != 1)
		; // Ignore gcc warning
	rfc2045_parse(rfc, text.c_str(), text.size());
}


/////////////////////////////////////////////////////////////////////////////

#if 0
mail::pop3::AutologoutCallback::AutologoutCallback()
{
}

mail::pop3::AutologoutCallback::~AutologoutCallback()
{
}

void mail::pop3::AutologoutCallback::success(string message)
{
	delete this;
}

void mail::pop3::AutologoutCallback::fail(string message)
{
	delete this;
}
#endif


/////////////////////////////////////////////////////////////////////////////
//
// A NOOP msg - keep the server from timing out.

class mail::pop3::NoopTask : public mail::pop3::LoggedInTask {
public:
	NoopTask(mail::pop3 &myseverArg);
	~NoopTask();
	void serverResponse(const char *message);
	void loggedIn();
	void loginFailed(string errmsg);
};

mail::pop3::NoopTask::NoopTask(mail::pop3 &serverArg)
	: LoggedInTask(NULL, serverArg)
{
}

mail::pop3::NoopTask::~NoopTask()
{
}

void mail::pop3::NoopTask::loginFailed(string errmsg)
{
}

void mail::pop3::NoopTask::loggedIn()
{
	myserver->socketWrite("NOOP\r\n");
}

void mail::pop3::NoopTask::serverResponse(const char *message)
{
	done();
}


/////////////////////////////////////////////////////////////////////////////


mail::pop3::pop3MessageInfo::pop3MessageInfo()
{
}

mail::pop3::pop3MessageInfo::~pop3MessageInfo()
{
}

/////////////////////////////////////////////////////////////////////////////


mail::pop3::pop3(string url, string passwd,
		 std::vector<std::string> &certificates,
		 mail::loginCallback *callback_func,
		 mail::callback &callback,
		 mail::callback::disconnect &disconnectCallback)
	: mail::fd(disconnectCallback, certificates),
	  calledDisconnected(false),
	  orderlyShutdown(false),
	  lastTaskCompleted(0),
	  inbox(this), folderCallback(NULL),
	  genericTmpFd(-1), genericTmpRfcp(NULL)
{
	savedLoginInfo.callbackPtr=NULL;
	savedLoginInfo.loginCallbackFunc=callback_func;

	if (!loginUrlDecode(url, savedLoginInfo))
	{
		callback.fail("Invalid POP3 URL.");
		return;
	}

	timeoutSetting=getTimeoutSetting(savedLoginInfo, "timeout", 60,
					 30, 600);
	noopSetting=getTimeoutSetting(savedLoginInfo, "noop", 60,
				      5, 1800);

	savedLoginInfo.use_ssl=savedLoginInfo.method == "pop3s";
	if (passwd.size() > 0)
		savedLoginInfo.pwd=passwd;

	installTask(new CheckNewMailTask(*this, callback));
}

void mail::pop3::resumed()
{
	if (!tasks.empty())
		(*tasks.begin())->resetTimeout();
}

void mail::pop3::handler(vector<pollfd> &fds, int &ioTimeout)
{
	int fd_save=getfd();

	if (!tasks.empty())
	{
		int t=(*tasks.begin())->getTimeout() * 1000;

		if (t < ioTimeout)
			ioTimeout=t;
	}

	if (fd_save >= 0 && lastTaskCompleted)
	{
		time_t now;

		time(&now);

		if (now < lastTaskCompleted)
			lastTaskCompleted=now;

		if (now >= lastTaskCompleted + noopSetting)
		{
			lastTaskCompleted=0;

			installTask(new NoopTask(*this));
		}
		else
		{
			if (ioTimeout >=
			    (lastTaskCompleted + noopSetting - now) * 1000)
			{
				ioTimeout=(lastTaskCompleted + noopSetting
					   - now) * 1000;
			}
		}
	}

	MONITOR(mail::account);

	if (process(fds, ioTimeout) < 0)
		return;

	if (DESTROYED())
		ioTimeout=0;

	if (DESTROYED() || getfd() < 0)
	{
		size_t i;

		for (i=fds.size(); i; )
		{
			--i;

			if (fds[i].fd == fd_save)
			{
				fds.erase(fds.begin()+i, fds.begin()+i+1);
				break;
			}
		}
	}
}

int mail::pop3::socketRead(const string &readbuffer)
{
	mail::ptr<mail::pop3> myself=this;

	size_t n=0;

	string::const_iterator b=readbuffer.begin(), e=readbuffer.end(), c;

	c=b;

	while (b != e)
	{
		if (*b != '\n')
		{
			b++;
			continue;
		}

		string l=string(c, b++);

		n += b-c;
		c=b;

		size_t i;

		while ((i=l.find('\r')) != std::string::npos)
			l=l.substr(0, i) + l.substr(i+1);

		if (!tasks.empty())
			(*tasks.begin())->serverResponse(l.c_str());

		break;
	}

	if (myself.isDestroyed())
		return n;

	if (n + 16000 < readbuffer.size())
		n=readbuffer.size() - 16000;
		// SANITY CHECK - DON'T LET HOSTILE SERVER DOS us

	return n;
}

void mail::pop3::disconnect(const char *reason)
{
	if (!tasks.empty())
	{
		if ( (*tasks.begin())->willReconnect())
		{
			socketDisconnect();
			(*tasks.begin())->done();
			return;
		}
	}

	if (genericTmpFd >= 0)
	{
		close(genericTmpFd);
		genericTmpFd= -1;
	}

	if (genericTmpRfcp != NULL)
	{
		rfc2045_free(genericTmpRfcp);
		genericTmpRfcp=NULL;
	}

	string errmsg=reason ? reason:"";

	if (getfd() >= 0)
	{
		string errmsg2=socketDisconnect();

		if (errmsg2.size() > 0)
			errmsg=errmsg2;

		if (orderlyShutdown)
			errmsg="";
		else if (errmsg.size() == 0)
			errmsg="Connection closed by remote host.";
	}

	while (!tasks.empty())
		(*tasks.begin())
			->disconnected(errmsg.size() > 0 ?
				       errmsg.c_str()
				       : "POP3 server connection closed.");

	if (!calledDisconnected)
	{
		calledDisconnected=true;
		disconnect_callback.disconnected("");
	}

}

void mail::pop3::installTask(Task *t)
{
	if (!t)
		LIBMAIL_THROW("Out of memory.");

	bool wasEmpty;

	lastTaskCompleted=0;

	try {
		wasEmpty=tasks.empty();

		tasks.insert(tasks.end(), t);
	} catch (...) {
		delete t;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	if (wasEmpty)
		(*tasks.begin())->installedTask();
}
		
mail::pop3::~pop3()
{
	disconnect("POP3 server connection aborted.");
	currentFolderIndex.clear();
}

void mail::pop3::logout(mail::callback &callback)
{
	installTask(new LogoutTask(*this, &callback));
}

void mail::pop3::checkNewMail(mail::callback &callback)
{
	ForceCheckNewMailTask *reloginTask=
		new ForceCheckNewMailTask(&callback, this);

	if (!reloginTask)
		callback.fail("Out of memory");

	try {
		installTask(new LogoutTask(*this, reloginTask, true));
	} catch (...) {
		delete reloginTask;

		callback.fail("Out of memory.");
	}
}

bool mail::pop3::hasCapability(string capability)
{
	if (capability == LIBMAIL_SINGLEFOLDER)
		return true;

	return capabilities.count(capability) > 0;
}

string mail::pop3::getCapability(string capability)
{
	upper(capability);

	if (capability == LIBMAIL_SERVERTYPE)
	{
		return "pop3";
	}

	map<string, string>::iterator p;

	if (capability == LIBMAIL_SERVERDESCR)
	{
		p=capabilities.find("IMPLEMENTATION");

		return (p == capabilities.end() ?
			(ispop3maildrop() ? "POP3 maildrop":
			 "POP3 account")
			:
			(ispop3maildrop() ? "POP3 maildrop - ":"POP3 - ")
			+ p->second);
	}

	p=capabilities.find(capability);

	if (p == capabilities.end())
		return "";

	return p->second;
}

mail::folder *mail::pop3::folderFromString(string string)
{
	return inbox.clone();
}

void mail::pop3::readTopLevelFolders(mail::callback::folderList &callback1,
				     mail::callback &callback2)
{
	vector<const mail::folder *> folders;

	folders.push_back(&inbox);

	callback1.success(folders);
	callback2.success("OK");
}

void mail::pop3::findFolder(string folder,
			    mail::callback::folderList &callback1,
			    mail::callback &callback2)
{
	return readTopLevelFolders(callback1, callback2);
}

void mail::pop3::genericMessageRead(string uid,
				    size_t messageNumber,
				    bool peek,
				    mail::readMode readType,
				    mail::callback::message &callback)
{
	if (fixMessageNumber(this, uid, messageNumber) &&
	    genericCachedUid(uid))
	{
		// Optimum path

		vector<size_t> vec;
		vec.push_back(messageNumber);

		readMessageContent(vec, peek, readType, callback);
		return;
	}

	installTask(new ReadMessageTask(*this, callback,
					uid,
					messageNumber,
					peek,
					readType));
}

void mail::pop3::genericMessageSize(string uid,
				    size_t messageNumber,
				    mail::callback::message &callback)
{
	if (uidlMap.count(uid) > 0)
	{
		int messageNum=uidlMap.find(uid)->second;

		if (listMap.count(messageNum) > 0 &&
		    fixMessageNumber(this, uid, messageNumber))
		{
			callback.messageSizeCallback(messageNumber,
						     listMap.find(messageNum)
						     ->second);
		}
	}
	callback.success("OK");
}

void mail::pop3::readMessageAttributes(const vector<size_t> &messages,
				       MessageAttributes attributes,
				       mail::callback::message &callback)
{
	mail::generic::genericAttributes(this, this, messages,
					 attributes,
					 callback);
}

void mail::pop3::genericGetMessageFd(string uid,
				     size_t messageNumber,
				     bool peek,
				     int &fdRet,
				     mail::callback &callback)
{
	if (genericTmpFd >= 0 && cachedUid == uid)
	{
		fdRet=genericTmpFd;
		callback.success("OK");
		return;
	}

	if (uidlMap.count(uid) == 0)
	{
		callback.fail("Message removed from the POP3 server.");
		return;
	}

	CacheMessageTask *t=
		new CacheMessageTask(this, &callback, uid, &fdRet, NULL);

	if (!t || !t->init())
	{
		if (t)
			delete t;

		callback.fail("Out of memory.");
	}

	try {
		genericMessageRead(uid, messageNumber, peek,
				   mail::readBoth, *t);
	} catch (...) {
		delete t;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}


void mail::pop3::genericGetMessageStruct(string uid,
					 size_t messageNumber,
					 struct rfc2045 *&structRet,
					 mail::callback &callback)
{
	if (genericTmpRfcp && cachedUid == uid)
	{
		structRet=genericTmpRfcp;
		callback.success("OK");
		return;
	}

	if (uidlMap.count(uid) == 0)
	{
		callback.fail("Message removed from the POP3 server.");
		return;
	}

	CacheMessageTask *t=
		new CacheMessageTask(this, &callback, uid, NULL, &structRet);

	if (!t || !t->init())
	{
		if (t)
			delete t;

		callback.fail("Out of memory.");
	}

	try {
		genericMessageRead(uid, messageNumber, true,
				   mail::readBoth, *t);
	} catch (...) {
		delete t;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

}

bool mail::pop3::genericCachedUid(string uid)
{
	return genericTmpRfcp && cachedUid == uid;
}

void mail::pop3::readMessageContent(const vector<size_t> &messages,
				bool peek,
				enum mail::readMode readType,
				mail::callback::message &callback)
{
	genericReadMessageContent(this, this, messages, peek, readType,
				  callback);
}

void mail::pop3::readMessageContent(size_t messageNum,
				    bool peek,
				    const mail::mimestruct &msginfo,
				    enum mail::readMode readType,
				    mail::callback::message &callback)
{
	genericReadMessageContent(this, this, messageNum,
				  peek,
				  msginfo,
				  readType,
				  callback);
}

void mail::pop3::readMessageContentDecoded(size_t messageNum,
					   bool peek,
					   const mail::mimestruct &msginfo,
					   mail::callback::message &callback)
{
	genericReadMessageContentDecoded(this, this, messageNum,
					 peek, msginfo, callback);
}

size_t mail::pop3::getFolderIndexSize()
{
	return currentFolderIndex.size();
}

mail::messageInfo mail::pop3::getFolderIndexInfo(size_t n)
{
	if (n < currentFolderIndex.size())
		return currentFolderIndex[n];

	return mail::messageInfo();
}


void mail::pop3::saveFolderIndexInfo(size_t messageNum,
				     const mail::messageInfo &info,
				     mail::callback &callback)
{
	MONITOR(mail::pop3);

	if (messageNum < currentFolderIndex.size())
	{

#define DOFLAG(dummy1, field, dummy2) \
		(currentFolderIndex[messageNum].field= info.field)

		LIBMAIL_MSGFLAGS;

#undef DOFLAG

	}

	callback.success("OK");

	if (! DESTROYED() && messageNum < currentFolderIndex.size()
	    && folderCallback)
	{
		folderCallback->messageChanged(messageNum);

		if (!DESTROYED() && folderCallback)
			folderCallback->saveSnapshot("POP3");
	}

}

void mail::pop3::genericMarkRead(size_t messageNumber)
{
	if (messageNumber < currentFolderIndex.size() &&
	    currentFolderIndex[messageNumber].unread)
	{
		MONITOR(mail::pop3);

		currentFolderIndex[messageNumber].unread=false;
		if (folderCallback)
			folderCallback->messageChanged(messageNumber);
		if (! DESTROYED() && folderCallback)
			folderCallback->saveSnapshot("POP3");

	}
}

void mail::pop3::updateFolderIndexFlags(const vector<size_t> &messages,
					bool doFlip,
					bool enableDisable,
					const mail::messageInfo &flags,
					mail::callback &callback)
{
	vector<size_t>::const_iterator b, e;

	b=messages.begin();
	e=messages.end();

	size_t n=currentFolderIndex.size();

	while (b != e)
	{
		size_t i= *b++;

		if (i < n)
		{
#define DOFLAG(dummy, field, dummy2) \
			if (flags.field) \
			{ \
				currentFolderIndex[i].field=\
					doFlip ? !currentFolderIndex[i].field\
					       : enableDisable; \
			}

			LIBMAIL_MSGFLAGS;
#undef DOFLAG
		}
	}

	b=messages.begin();
	e=messages.end();

	MONITOR(mail::pop3);

	while (!DESTROYED() && b != e)
	{
		size_t i= *b++;

		if (i < n && folderCallback)
			folderCallback->messageChanged(i);
	}

	if (!DESTROYED())
		folderCallback->saveSnapshot("POP3");

	callback.success("OK");
}

void mail::pop3::getFolderKeywordInfo(size_t n, set<string> &kwSet)
{
	kwSet.clear();

	if (n < currentFolderIndex.size())
		currentFolderIndex[n].keywords.getFlags(kwSet);
}

void mail::pop3::updateKeywords(const std::vector<size_t> &messages,
				const std::set<std::string> &keywords,
				bool setOrChange,
				// false: set, true: see changeTo
				bool changeTo,
				callback &cb)
{
	MONITOR(mail::pop3);

	genericUpdateKeywords(messages, keywords, setOrChange, changeTo,
			      folderCallback, this, cb);

	if (!DESTROYED())
		folderCallback->saveSnapshot("POP3");
}

bool mail::pop3::genericProcessKeyword(size_t messageNumber,
				       updateKeywordHelper &helper)
{
	if (messageNumber < currentFolderIndex.size())
		helper.doUpdateKeyword(currentFolderIndex[messageNumber]
				       .keywords, keywordHashtable);
	return true;
}

void mail::pop3::updateFolderIndexInfo(mail::callback &callback)
{
	installTask(new UpdateTask(*this, callback));
}

void mail::pop3::removeMessages(const std::vector<size_t> &messages,
				callback &cb)
{
	genericRemoveMessages(this, messages, cb);
}

void mail::pop3::copyMessagesTo(const vector<size_t> &messages,
				mail::folder *copyTo,
				mail::callback &callback)
{
	mail::copyMessages::copy(this, messages, copyTo, callback);
}

void mail::pop3::searchMessages(const mail::searchParams &searchInfo,
				mail::searchCallback &callback)
{
	switch (searchInfo.criteria) {
	case searchParams::larger:
	case searchParams::smaller:
	case searchParams::body:
	case searchParams::text:
		callback.fail("Not implemented.");
		return;
	default:
		break;
	}
	mail::searchMessages::search(callback, searchInfo, this);
}

void mail::pop3Folder::getParentFolder(callback::folderList &callback1,
				       callback &callback2) const
{
	vector<const mail::folder *> dummy;

	dummy.push_back(this);

	callback1.success(dummy);
	callback2.success("OK");
}

void mail::pop3::readFolderInfo( mail::callback::folderInfo &callback1,
				 mail::callback &callback2)
{
	callback1.messageCount=currentFolderIndex.size();
	callback1.unreadCount=0;

	size_t i=0;

	for (i=0; i<callback1.messageCount; i++)
		if (currentFolderIndex[i].unread)
			++callback1.unreadCount;

	callback1.success();
	callback2.success("OK");
}

void mail::pop3::open(mail::callback &openCallback,
		      mail::callback::folder &folderCallbackArg,
		      mail::snapshot *restoreSnapshot)

{

	if (restoreSnapshot)
	{
		string snapshotId;
		size_t nmessages;

		folderCallback=NULL;

		restoreSnapshot->getSnapshotInfo(snapshotId, nmessages);

		if (snapshotId.size() > 0)
		{
			currentFolderIndex.clear();
			currentFolderIndex.resize(nmessages);
			restoreAborted=false;
			restoreSnapshot->restoreSnapshot(*this);

			if (!restoreAborted)
			{
				vector<pop3MessageInfo>::iterator b, e;

				b=currentFolderIndex.begin();
				e=currentFolderIndex.end();

				// Sanity check:

				while (b != e)
				{
					if ( b->uid.size() == 0)
					{
						restoreAborted=true;
						break;
					}
					b++;
				}
			}

			if (!restoreAborted)
			{
				folderCallback= &folderCallbackArg;
				reconcileFolderIndex();
				openCallback.success("OK");
				return;
			}

			// Rebuild folder index.

			currentFolderIndex.clear();
			reconcileFolderIndex();
		}
	}

	folderCallback= &folderCallbackArg;
	openCallback.success("OK");
}

void mail::pop3::restoreIndex(size_t msgNum,
			      const mail::messageInfo &info)
{
	if (msgNum < currentFolderIndex.size())
		(static_cast<mail::messageInfo &>
		 (currentFolderIndex[msgNum]))=info;
}

void mail::pop3::restoreKeywords(size_t msgNum,
				 const std::set<std::string> &kwSet)
{
	if (msgNum < currentFolderIndex.size())
		currentFolderIndex[msgNum].keywords
			.setFlags(keywordHashtable, kwSet);
}

void mail::pop3::abortRestore()
{
	restoreAborted=true;
}



//
// Reconcile the virtual folder index to reflect what's actually on the
// server.
//

bool mail::pop3::reconcileFolderIndex()
{
	set<size_t> messagesSeen;

	map<string, int>::iterator ub, ue;

	size_t i;

	bool needSnapshot=false;

	// Make a list of all msgs that exist (and don't exist any more)

	mail::expungeList removedList;

	for (i=currentFolderIndex.size(); i; )
	{
		string uid=currentFolderIndex[--i].uid;

		if (uidlMap.count( uid ) == 0)
		{
			removedList << i;
			needSnapshot=true;
		}
		else
			messagesSeen.insert(uidlMap.find(uid)->second);
	}

	// Send expunge notices

	MONITOR(mail::pop3);

	mail::expungeList::iterator expungeIterator;

	for (expungeIterator=removedList.end();
	     expungeIterator != removedList.begin(); )
	{
		--expungeIterator;
		currentFolderIndex.erase(currentFolderIndex.begin() +
					 expungeIterator->first,
					 currentFolderIndex.begin() +
					 expungeIterator->second+1);
	}

	if (folderCallback)
		removedList >> folderCallback;

	// Now check for new messages.

	if (!DESTROYED())
	{
		vector<string> newUids;

		ub=uidlMap.begin();
		ue=uidlMap.end();

		// sortNewMail() will end up sorting newUids chronologically.

		while (ub != ue)
		{
			if (messagesSeen.count(ub->second) == 0)
				newUids.push_back(ub->first);
			ub++;
		}
		sort(newUids.begin(), newUids.end(),
		     CheckNewMailTask::sortNewMail(uidlMap));

		vector<string>::iterator b, e;

		b=newUids.begin(), e=newUids.end();

		while (b != e)
		{
			pop3MessageInfo newInfo;

			// We'll have to assume that all msgs are unread.

			newInfo.unread=true;
			newInfo.uid= *b++;
			currentFolderIndex.push_back(newInfo);
		}

		if (newUids.size() > 0 && folderCallback)
			// Note folderCallback is NULL on account open!
		{
			needSnapshot=true;
			folderCallback->newMessages();
		}
	}
	return needSnapshot;
}

//////////////////////
//
// Stubs, overridden by mail::pop3maildrop::pop3acct

bool mail::pop3::ispop3maildrop()
{
	return false;
}

void mail::pop3::pop3maildropreset()
{
}

mail::addMessage *mail::pop3::newDownloadMsg()
{
	return NULL;
}

string mail::pop3::commitDownloadedMsgs()
{
	return "";
}

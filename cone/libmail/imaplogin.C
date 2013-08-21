/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "logininfo.H"
#include "imaplogin.H"
#include "imaphandler.H"
#include "base64.H"
#include "imaphmac.H"
#include "misc.H"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <list>
#include "libcouriertls.h"

using namespace std;

////////////////////////////////////////////////////////////////////////
//
// Helper class that parses the server reply to a login request.

LIBMAIL_START

class imapLoginHandler : public imapCommandHandler,
			 public loginInfo::callbackTarget {

private:
	mail::loginInfo myLoginInfo;
	int completed;
	string currentCmd;
	bool preauthenticated;

	const char *getName();
	void timedOut(const char *errmsg);

	void (imapLoginHandler::*next_login_func)(imap &imapAccount,
							string failmsg);

	size_t next_cram_func;

	string loginMethod;

	void loginInfoCallback(std::string);
	void loginInfoCallbackCancel();

	void (imapLoginHandler::*loginCallbackFunc)(std::string);

	void loginCallbackUid(std::string);
	void loginCallbackPwd(std::string);

public:
	imapLoginHandler(mail::loginInfo myLoginInfo,
			 bool preauthenticatedArg);
	~imapLoginHandler();

	void installed(imap &imapAccount);
	void installedtls(imap &imapAccount);
	void installedtlsnonext(imap &imapAccount);

	friend class imapCRAMHandler;

private:

	void logincram(imap &imapAccount, string errmsg);
	void logincmd(imap &imapAccount, string errmsg);
	void failcmd(imap &imapAccount, string message);

	bool untaggedMessage(imap &imapAccount, string name);
	bool taggedMessage(imap &imapAccount, string name, string message,
			   bool okfail, string errmsg);

	bool continuationRequest(imap &imapAccount, string msg);

private:
	list<string> cmds;
} ;

////////////////////////////////////////////////////////////////////////
//
// Helper class that handles CRAM authentication

class imapCRAMHandler : public imapCommandHandler {

	const char *getName();
	void timedOut(const char *errmsg);

	const mail::loginInfo &myLoginInfo;
	imapLoginHandler &loginHandler;
	const imaphmac &hmac;

public:
	imapCRAMHandler(	const mail::loginInfo &loginInfoArg,
				imapLoginHandler &loginHandlerArg,
				const imaphmac &hmacArg);
	~imapCRAMHandler();

	void installed(imap &imapAccount);

private:
	bool untaggedMessage(imap &imapAccount, string name);
	bool taggedMessage(imap &imapAccount, string name, string message,
			   bool okfail, string errmsg);

	bool continuationRequest(imap &imapAccount, string request);
};

////////////////////////////////////////////////////////////////////////
//
// Helper class that parses the server NAMESPACE reply.

class imapNAMESPACE : public imapHandlerStructured {

	void (imapNAMESPACE::*next_func)(imap &, Token);
	int namespace_type;

	string hier;
	int string_cnt;

public:
	imapNAMESPACE()
		: next_func(&imapNAMESPACE::start_namespace_list),
		  namespace_type(0) {}

	void installed(imap &imapAccount) {}

private:
	const char *getName() { return ("NAMESPACE"); }
	void timedOut(const char *errmsg) {}
	void process(imap &imapAccount, Token t);


	void start_namespace_list(imap &imapAccount, Token t);
	void start_namespace_entry(imap &imapAccount, Token t);
	void process_namespace_entry(imap &imapAccount, Token t);
} ;

LIBMAIL_END

//////////////////////////////////////////////////////////////////////////////
//
// The login process


// Wait for a greeting

mail::imapGreetingHandler::
imapGreetingHandler(mail::loginInfo myLoginInfoArg):
	myLoginInfo(myLoginInfoArg), capability_sent(0), completed(0),
	preauthenticated(false)
{
}

void mail::imapGreetingHandler::timedOut(const char *errmsg)
{
	completed=1;
	callbackTimedOut(*myLoginInfo.callbackPtr, errmsg);
}

const char *mail::imapGreetingHandler::getName()
{
	return "GREETINGS";
}

mail::imapGreetingHandler::~imapGreetingHandler()
{
	if (!completed)
		myLoginInfo.callbackPtr->fail("Failed connecting to server.");
}

void mail::imapGreetingHandler::installed(mail::imap &imapAccount) // Background task
{
}

void mail::imapGreetingHandler::error(mail::imap &imapAccount, string errmsg)
{
	completed=1;
	mail::callback *callback=myLoginInfo.callbackPtr;
	imapAccount.uninstallHandler(this);
	callback->fail(errmsg);
}

int mail::imapGreetingHandler::process(mail::imap &imapAccount, string &buffer)
{
	size_t p=buffer.find('\n');

	if (p == std::string::npos)
	{
		if (buffer.size() > 16000)
			return buffer.size() - 16000;
		// SANITY CHECK - DON'T LET HOSTILE SERVER DOS us

		return (0);
	}

	string buffer_cpy=buffer;

	buffer_cpy.erase(p);
	++p;

	string::iterator b=buffer_cpy.begin();
	string::iterator e=buffer_cpy.end();

	while (b != e)
	{
		if (!unicode_isspace((unsigned char)*b))
			break;
		++b;
	}

	if (b == e)
		return 0;

	if (*b == '*')
	{
		++b;

		string status=imapAccount.get_word(&b, &e);

		if (strcasecmp(status.c_str(), "PREAUTH") == 0)
		{
			status="OK";
			preauthenticated=true;
		}

		if (imapAccount.ok(status))
		{
			if (!capability_sent)
			{
				static const char courierimap[]="Double Precision, Inc.";
				static const char cyrusimap[]="Cyrus IMAP4";

				if (search(b, e, courierimap,
					   courierimap+sizeof(courierimap)-1)
				    != e)
				{
					imapAccount.setCapability(LIBMAIL_CHEAPSTATUS);
					imapAccount.servertype=IMAP_COURIER;
					imapAccount.serverdescr="Courier-IMAP server";
				}
				else if (search(b, e, cyrusimap,
						cyrusimap+sizeof(cyrusimap)-1)
				    != e)
				{
					imapAccount.setCapability(LIBMAIL_CHEAPSTATUS);
					imapAccount.servertype=IMAP_CYRUS;
					imapAccount.serverdescr="Cyrus IMAP server";
				}

				imapAccount.smap=false;

				while (b != e &&
				       unicode_isspace((unsigned char)*b))
					++b;

				if (*b == '[')
				{
					string::iterator c= ++b;

					while (c != e)
					{
						if (*c == ']')
							break;
						c++;
					}

					string caps=string(b, c);

					b=caps.begin();
					e=caps.end();

					string w=imapAccount.get_word(&b, &e);

					upper(w);

					if (w == "CAPABILITY")
						while ((w=imapAccount
							.get_word(&b, &e))
						       .size() > 0)
						{
							upper(w);
							if (w == "SMAP1")
								imapAccount.
									smap=
									true;
						}
				}
				if (myLoginInfo.options.count("imap") > 0)
					imapAccount.smap=false;

				capability_sent=1;
				imapAccount.imapcmd((imapAccount.smap
						     ? "\\SMAP1":
						     "CAPABILITY"),
						    "CAPABILITY\r\n");
			}
			return p;
		}

		upper(status);
		if (status == "CAPABILITY")
		{
			imapAccount.clearCapabilities();
			while ((status=imapAccount.get_word(&b, &e)).length() > 0)
				imapAccount.setCapability(status);
			return p;
		}
		error(imapAccount, buffer_cpy);
		return p;
	}

	string w=imapAccount.get_cmdreply(&b, &e);

	if (imapAccount.smap)
	{
		if (w == "+OK")
		{
			completed=1;
		}
		else if (w == "-ERR")
		{
			error(imapAccount, string(b, e));
			return p;
		}
	}
	else if (w == "CAPABILITY")
	{
		w=imapAccount.get_word(&b, &e);

		if (!imapAccount.ok(w))
		{
			error(imapAccount, buffer_cpy);
			return p;
		}

		if (!imapAccount.hasCapability("IMAP4REV1"))
		{
			error(imapAccount, "Server does not support IMAP4rev1.");
			return p;
		}

		completed=1;
	}

	if (completed)
	{
		mail::loginInfo l=myLoginInfo;

		bool preauthArg=preauthenticated;

		imapAccount.uninstallHandler(this);
		imapAccount
			.installForegroundTask(new
					       mail::imapLoginHandler(l,
								      preauthArg)
					       );
		return (p);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Check the LOGIN status

mail::imapLoginHandler::imapLoginHandler(mail::loginInfo myInfo,
					 bool preauthenticatedArg)
	: myLoginInfo(myInfo), completed(0),
	  preauthenticated(preauthenticatedArg)
{
}

mail::imapLoginHandler::~imapLoginHandler()
{
	if (!completed)
	{
		myLoginInfo.callbackPtr->fail("Failed connecting to server.");
	}
}

const char *mail::imapLoginHandler::getName() { return "LOGIN"; }

void mail::imapLoginHandler::timedOut(const char *errmsg)
{
	completed=1;
	callbackTimedOut(*myLoginInfo.callbackPtr, errmsg);
}

//////////////////////////////////////////////////////////////////////////
//
// Are we ready to go?

void mail::imapLoginHandler::installed(mail::imap &imapAccount)
{
	next_cram_func=0;

	currentCmd="STARTTLS";

#if HAVE_LIBCOURIERTLS

	if (!myimap->socketEncrypted() &&
	    myimap->hasCapability("STARTTLS") &&
	    myLoginInfo.options.count("notls") == 0)
	{
		myimap->imapcmd((myimap->smap ? "\\SMAP1":"STARTTLS"),
				"STARTTLS\r\n");
		return;
	}

#endif
	installedtls(imapAccount);
}

void mail::imapLoginHandler::installedtls(mail::imap &imapAccount)
{
	if (!preauthenticated && imapAccount.hasCapability("AUTH=EXTERNAL"))
	{
		currentCmd="LOGINEXT";
		imapAccount.imapcmd(imapAccount.smap ? "\\SMAP1":"LOGINEXT",
				    "AUTHENTICATE EXTERNAL =\r\n");
		return;
	}
	installedtlsnonext(imapAccount);
}

void mail::imapLoginHandler::installedtlsnonext(mail::imap &imapAccount)
{
	if (!preauthenticated && myLoginInfo.loginCallbackFunc)
	{
		loginMethod="LOGIN";

		if (myLoginInfo.uid.size() == 0)
		{
			loginCallbackFunc= &mail::imapLoginHandler
				::loginCallbackUid;

			currentCallback=myLoginInfo.loginCallbackFunc;
			currentCallback->target=this;
			currentCallback->getUserid();
			return;
		}

		if (myLoginInfo.pwd.size() == 0)
		{
			loginCallbackUid(myLoginInfo.uid);
			return;
		}
	}

	loginCallbackPwd(myLoginInfo.pwd);
}

void mail::imapLoginHandler::loginInfoCallback(std::string arg)
{
	currentCallback=NULL;
	(this->*loginCallbackFunc)(arg);
}

void mail::imapLoginHandler::loginInfoCallbackCancel()
{
	currentCallback=NULL;
	failcmd(*myimap, "Login cancelled.");
}

void mail::imapLoginHandler::loginCallbackUid(std::string arg)
{
	myLoginInfo.uid=arg;

	if (myLoginInfo.pwd.size() == 0)
	{
		loginCallbackFunc= &mail::imapLoginHandler
			::loginCallbackPwd;

		currentCallback=myLoginInfo.loginCallbackFunc;
		currentCallback->target=this;
		currentCallback->getPassword();
		return;
	}

	loginCallbackPwd(myLoginInfo.pwd);
}

void mail::imapLoginHandler::loginCallbackPwd(std::string arg)
{
	myLoginInfo.pwd=arg;

	logincram(*myimap, "Login FAILED");
}


//////////////////////////////////////////////////////////////////////////////
//
// Go!

void mail::imapLoginHandler::logincram(mail::imap &imapAccount, string errmsg)
{
	if (preauthenticated)
	{
		// Fake a succesfull login :-)

		currentCmd="LOGIN";
		if (imapAccount.smap)
			imapAccount.imapcmd("", "NOOP\n");
		else
			imapAccount.imapcmd("LOGIN", "NOOP\r\n");
		return;
	}

	while (mail::imaphmac::hmac_methods[next_cram_func])
	{
		next_login_func= &mail::imapLoginHandler::logincram;

		string s=mail::imaphmac::hmac_methods[next_cram_func]
			->getName();

		if (!imapAccount.hasCapability("AUTH=CRAM-" + s))
		{
			next_cram_func++;
			continue;
		}

		imapAccount.installBackgroundTask(new
					   mail::imapCRAMHandler(myLoginInfo,
								 *this,
								 *mail::imaphmac
								 ::
								 hmac_methods
								 [next_cram_func++]));

		currentCmd="LOGIN";
		loginMethod="CRAM-" + s;
		imapAccount.imapcmd(imapAccount.smap ? "\\SMAP1":"LOGIN",
				    "AUTHENTICATE CRAM-" + s + "\r\n");
		return;
	}
	if (myLoginInfo.options.count("cram") > 0)
	{
		failcmd(imapAccount, errmsg);
		return;
	}

	logincmd(imapAccount, errmsg);
}

void mail::imapLoginHandler::logincmd(mail::imap &imapAccount, string errmsg)
{
	next_login_func= &mail::imapLoginHandler::failcmd;
	loginMethod="LOGIN";

#if 0
	imapAccount.imapcmd("LOGIN","LOGIN " +
		     imapAccount.quoteSimple(myLoginInfo.uid) + " " +
		     imapAccount.quoteSimple(myLoginInfo.pwd) + "\r\n");
#else

	currentCmd="LOGIN";

	if (imapAccount.smap)
	{
		imapAccount.imapcmd("\\SMAP1", "LOGIN " +
				    imapAccount.quoteSMAP(myLoginInfo.uid)
				    + " " +
				    imapAccount.quoteSMAP(myLoginInfo.pwd)
				    + "\n");
		return;
	}

	ostringstream uidLen;

	uidLen << myLoginInfo.uid.size();

	ostringstream pwdLen;

	pwdLen << myLoginInfo.pwd.size();

	cmds.insert(cmds.end(), myLoginInfo.uid + " {" + pwdLen.str()
		    + "}\r\n");
	cmds.insert(cmds.end(), myLoginInfo.pwd + "\r\n");

	imapAccount.imapcmd("LOGIN", "LOGIN {" + uidLen.str() + "}\r\n");
#endif
}

bool mail::imapLoginHandler::continuationRequest(mail::imap &imapAccount, string msg)
{
	list<string>::iterator b=cmds.begin();

	if (b == cmds.end())
		return false;

	string s= *b;

	cmds.erase(b);

	imapAccount.socketWrite(s);
	return true;
}

void mail::imapLoginHandler::failcmd(mail::imap &imapAccount, string message)
{
	completed=1;
	mail::callback *callback=myLoginInfo.callbackPtr;

	message=loginMethod + ": " + message;
	imapAccount.uninstallHandler(this);
	callback->fail(message);
}

bool mail::imapLoginHandler::untaggedMessage(mail::imap &imapAccount, string name)
{
	if (name == "NAMESPACE")
	{
		imapAccount.installBackgroundTask(new mail::imapNAMESPACE);
		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////
//
// The "default namespace" for SMAP account is a top-level LIST

LIBMAIL_START

class smapNamespace : public mail::callback, public mail::callback::folderList{

	ptr<imap> myImap;

	mail::callback *origCallback;

public:
	smapNamespace(imap *imapPtr, mail::callback *origCallbackArg);
	~smapNamespace();
	void success(string);
	void fail(string);
	void success(const vector<const mail::folder *> &folders);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);
};

LIBMAIL_END

mail::smapNamespace::smapNamespace(mail::imap *imapPtr,
				   mail::callback *origCallbackArg)
	: myImap(imapPtr), origCallback(origCallbackArg)
{
}

mail::smapNamespace::~smapNamespace()
{
}

void mail::smapNamespace::success(string s)
{
	mail::callback * volatile p=origCallback;

	delete this;

	p->success(s);
}

void mail::smapNamespace::fail(string s)
{
	mail::callback * volatile p=origCallback;

	delete this;

	p->fail(s);
}

void mail::smapNamespace::success(const vector<const mail::folder *> &folders)
{
	if (!myImap.isDestroyed())
	{
		vector<const mail::folder *>::const_iterator
			b=folders.begin(), e=folders.end();

		while (b != e)
		{
			mail::imapFolder f( *myImap, (*b)->getPath(), "",
					    (*b)->getName(), 0);

			f.hasMessages( (*b)->hasMessages());
			f.hasSubFolders( (*b)->hasSubFolders());

			myImap->namespaces.push_back(f);
			b++;
		}

		// Install a dummy namespace entry that points back to the
		// root.

		mail::imapFolder f(*myImap, "", "",
				   "Other Folders",
				   0);

		f.hasMessages(false);
		f.hasSubFolders(true);
		myImap->namespaces.push_back(f);
	}
}

void mail::smapNamespace::reportProgress(size_t bytesCompleted,
					 size_t bytesEstimatedTotal,

					 size_t messagesCompleted,
					 size_t messagesEstimatedTotal)
{
}

bool mail::imapLoginHandler::taggedMessage(mail::imap &imapAccount, string name,
					   string message,
					   bool okfail,
					   string errmsg)
{
	string w;

	if (imapAccount.smap)
		name=currentCmd;

	if (name == "LOGINEXT")
	{
		if (!okfail)
		{
			installedtlsnonext(imapAccount);
			return true;
		}
		name="LOGIN";
	}

	if (!okfail)
	{
		if (name == "LOGIN")
		{
			(this->*next_login_func)(imapAccount, errmsg);
			return true;
		}

		failcmd(imapAccount, message);
		return true;
	}

#if HAVE_LIBCOURIERTLS

	if (name == "STARTTLS")
	{
		completed=1;

		if (!imapAccount.socketBeginEncryption(myLoginInfo))
			return true;

		completed=0;
		currentCmd="TLSCAPABILITY";

		imapAccount.imapcmd(imapAccount.smap ?
				    "\\SMAP1":"TLSCAPABILITY", "CAPABILITY\r\n");
		return true;
	}

#endif

	if (name == "LOGIN")
	{
		if (imapAccount.smap)
			name="CAPABILITY";
		else
		{
			imapAccount.imapcmd("CAPABILITY", "CAPABILITY\r\n");
			return true;
		}
	}

	if (name == "TLSCAPABILITY")
	{
		installedtls(imapAccount);
		return true;
	}

	if (name == "CAPABILITY")
	{
		imapAccount.namespaces.clear();

		if (imapAccount.smap)
		{
			// SMAP: LIST top level folders, that's our namespace

			smapNamespace *cb=
				new smapNamespace(&imapAccount,
						  myLoginInfo.callbackPtr);

			if (!cb)
			{
				mail::callback *callback=
					myLoginInfo.callbackPtr;
				completed=1;
				imapAccount.uninstallHandler(this);

				callback->fail(strerror(errno));
				return true;
			}

			try {
				imapAccount.readSubFolders("", *cb, *cb);
			} catch (...) {
				delete cb;

				mail::callback *callback=
					myLoginInfo.callbackPtr;
				completed=1;
				imapAccount.uninstallHandler(this);

				callback->fail(strerror(errno));
				return true;
			}

			completed=1;
			imapAccount.uninstallHandler(this);
		}
		else
		{
			imapAccount.namespaces
				.push_back(mail::imapFolder(imapAccount,
							    "INBOX", "",
							    "INBOX", -1));
			if (imapAccount.hasCapability("NAMESPACE"))
				imapAccount.imapcmd("NAMESPACE",
						    "NAMESPACE\r\n");
			else
			{
				mail::imapFolder f(imapAccount, "",
						   "",
						   "Folders",
						   0);

				f.hasMessages(false);
				f.hasSubFolders(true);
				imapAccount.namespaces.push_back(f);

				completed=1;
				mail::callback *callback=
					myLoginInfo.callbackPtr;
				imapAccount.uninstallHandler(this);
				callback->success(message);
			}
		}
		return (true);
	}

	if (name == "NAMESPACE")
	{
		vector <mail::imapFolder>::iterator b, e, save;

		b=imapAccount.namespaces.begin();
		e=imapAccount.namespaces.end();

		while (b != e)
		{
			save=b++;

			if (b == e || b->getType() != save->getType())
				switch (save->getType()) {
				case 0:
					save->setName("Folders");
					break;
				case 1:
					save->setName("Other folders");
					break;
				case 2:
					save->setName("Shared folders");
					break;
				}

			while (b != e && b->getType() == save->getType())
				save=b++;
		}
		completed=1;

		mail::callback *callback=myLoginInfo.callbackPtr;

		errmsg = loginMethod + ": " + errmsg;

		if (imapAccount.socketEncrypted())
			errmsg="SSL " + errmsg;

		if (myLoginInfo.options.count("faststatus") > 0)
			imapAccount.setCapability(LIBMAIL_CHEAPSTATUS);
		else if (myLoginInfo.options.count("slowstatus") == 0)
		{
			b=imapAccount.namespaces.begin();
			e=imapAccount.namespaces.end();

			while (b != e)
			{
				if (b->getPath() == "#news.")
					break;
				b++;
			}

			if (b != e && imapAccount.hasCapability("SCAN"))
			{
				imapAccount.servertype=IMAP_UWIMAP;
				imapAccount.serverdescr="UW-IMAP server";
			}
			else
			{
				imapAccount.setCapability(LIBMAIL_CHEAPSTATUS);
			}
		}

		imapAccount.uninstallHandler(this);

		callback->success(errmsg);
		return (true);
	}

	return (false);
}

//////////////////////////////////////////////////////////////////////////
//
// CRAM handling

mail::imapCRAMHandler::imapCRAMHandler(const mail::loginInfo &myLoginInfoArg,
					     mail::imapLoginHandler
					     &loginHandlerArg,
					     const mail::imaphmac &hmacArg)
	: myLoginInfo(myLoginInfoArg),
	  loginHandler(loginHandlerArg),
	  hmac(hmacArg)
{
}

const char *mail::imapCRAMHandler::getName()
{
	return "* AUTHENTICATE";
}


void mail::imapCRAMHandler::timedOut(const char *errmsg)
{
}

mail::imapCRAMHandler::~imapCRAMHandler()
{
}

void mail::imapCRAMHandler::installed(mail::imap &imapAccount)
{
}

bool mail::imapCRAMHandler::untaggedMessage(mail::imap &imapAccount, string name)
{
	return false;
}

bool mail::imapCRAMHandler::taggedMessage(mail::imap &imapAccount,
					  string name,
					  string message,
					  bool okfail, string errmsg)
{
	bool rc=loginHandler
		.taggedMessage(imapAccount, name, message, okfail, errmsg);
	imapAccount.uninstallHandler(this);
	return rc;
}

bool mail::imapCRAMHandler::continuationRequest(mail::imap &imapAccount,
						string message)
{
	message=message.substr(1);
	mail::decodebase64str challengeStr;

	challengeStr.decode(message);

	mail::encodebase64str responseStr;

	responseStr.encode(myLoginInfo.uid + " ");

#if 0
	cerr << "CHALLENGE: key=" << myLoginInfo.pwd
	     << ", challenge=" << challengeStr.challengeStr << endl;
#endif

	string hmacBinary=hmac(myLoginInfo.pwd, challengeStr.challengeStr);

	string hmacBinaryHex;

	{
		ostringstream hexEncode;

		hexEncode << hex;

		string::iterator b=hmacBinary.begin();
		string::iterator e=hmacBinary.end();

		while (b != e)
			hexEncode << setw(2) << setfill('0')
				  << (int)(unsigned char)*b++;

		hmacBinaryHex=hexEncode.str();
	}

	responseStr.encode(hmacBinaryHex);
	responseStr.flush();

#if 0
	{
		challenge dummyStr;

		dummyStr.decode(responseStr.responseStr);

		cerr << "Response: " << dummyStr.challengeStr << endl;
	}
#endif

	imapAccount.imapcmd("", responseStr.responseStr + "\r\n");

	imapAccount.uninstallHandler(this);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Process the NAMESPACE reply

void mail::imapNAMESPACE::process(mail::imap &imapAccount, Token t)
{
	(this->*next_func)(imapAccount, t);
}

void mail::imapNAMESPACE::start_namespace_list(mail::imap &imapAccount, Token t)
{
	if (namespace_type == 3)
	{
		if (t != EOL)
			error(imapAccount);
		return;
	}

	if (t == NIL)
	{
		++namespace_type;
		return;
	}

	if (t != '(')
	{
		error(imapAccount);
		return;
	}

	next_func= &mail::imapNAMESPACE::start_namespace_entry;
}

void mail::imapNAMESPACE::start_namespace_entry(mail::imap &imapAccount, Token t)
{
	if (t == ')')
	{
		++namespace_type;
		next_func= &mail::imapNAMESPACE::start_namespace_list;
		return;
	}

	if (t != '(')
	{
		error(imapAccount);
		return;
	}

	string_cnt=0;
	hier="";
	//	sep="";
	next_func= &mail::imapNAMESPACE::process_namespace_entry;
}

void mail::imapNAMESPACE::process_namespace_entry(mail::imap &imapAccount, Token t)
{
	if (t == ')')
	{
		next_func= &mail::imapNAMESPACE::start_namespace_entry;
		return;
	}

	if (t == ATOM || t == STRING || t == NIL)
		switch (++string_cnt) {
		case 1:
			hier=t.text;
			break;
		case 2:
			{
				mail::imapFolder f(imapAccount, hier, "", // t.text,
						   hier == ""
						   ? "Folders":hier,
						   namespace_type);

				if (t == NIL)
				{
					f.hasMessages(true);
					f.hasSubFolders(false);
				}
				else
				{
					f.hasMessages(false);
					f.hasSubFolders(true);
				}
				imapAccount.namespaces.push_back(f);
			}
			break;
		}
	else
		error(imapAccount);
}

/*
** Copyright 2003-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "imap.H"
#include "misc.H"
#include "imaphandler.H"
#include "imapidle.H"
#include "imapfetchhandler.H"
#include "imapfolder.H"
#include "imapparsefmt.H"

#include "smapopen.H"
#include "smapidle.H"
#include "smapnoopexpunge.H"
#include "smapstore.H"
#include "smapcopy.H"
#include "smapsearch.H"

#include "generic.H"
#include "copymessage.H"
#include "search.H"
#include "rfcaddr.H"
#include "envelope.H"
#include "structure.H"
#include "rfc822/rfc822.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sstream>
#include <limits.h>
#include <errno.h>

using namespace std;

mail::imapFOLDERinfo::imapFOLDERinfo(string pathArg,
				     mail::callback::folder &folderCallbackArg)
	: folderCallback(folderCallbackArg),
	  exists(0), closeInProgress(false),
	  path(pathArg)
{
}

mail::imapFOLDERinfo::~imapFOLDERinfo()
{
}


mail::imapFOLDERinfo::indexInfo::indexInfo()
{
}

mail::imapFOLDERinfo::indexInfo::~indexInfo()
{
}

void mail::imapFOLDERinfo::setUid(size_t count, string uid)
{
	if (count < index.size())
		index[count].uid=uid;
}

void mail::imapFOLDERinfo::opened()
{
}


/////////////////////////////////////////////////////////////////////////
//
// Helper class for processing the SELECT command.  It's installed when
// a SELECT command is issued.  It handles the * FLAGS, * OK, and * NO
// untagged messages, during folder open, as well as the * n EXISTS message
// ( TODO - ignore any other garbage )
//

LIBMAIL_START

class imapSELECT : public imapCommandHandler {

	mail::callback &openCallback;
	mail::callback::folder &folderCallback;

	string path;

public:
	string uidv;
	size_t exists;

	imapSELECT(string pathArg,
			 mail::callback &openCallbackArg,
			 mail::callback::folder &folderCallbackArg);

	~imapSELECT();

	void installed(imap &imapAccount);
	static const char name[];

private:
	const char *getName();
	void timedOut(const char *errmsg);

	bool untaggedMessage(imap &imapAccount, string name);
	bool taggedMessage(imap &imapAccount, string name,
			   string message,
			   bool okfail, string errmsg);
};

LIBMAIL_END

const char mail::imapSELECT::name[]="SELECT";

mail::imapSELECT::imapSELECT(string pathArg,
				   mail::callback &openCallbackArg,
				   mail::callback::folder &folderCallbackArg)
	: openCallback(openCallbackArg),
	  folderCallback(folderCallbackArg), path(pathArg),
	  uidv(""), exists(0)
{
}

mail::imapSELECT::~imapSELECT()
{
}

void mail::imapSELECT::installed(mail::imap &imapAccount)
{
	imapAccount.imapcmd("SELECT", "SELECT " +
		     imapAccount.quoteSimple(path) + "\r\n");

	imapAccount.folderFlags.clear();
	imapAccount.permanentFlags.clear();
}

const char *mail::imapSELECT::getName()
{
	return name;
}

void mail::imapSELECT::timedOut(const char *errmsg)
{
	callbackTimedOut(openCallback, errmsg);
}

//
// Helper class for processing the untagged FLAGS response
//

LIBMAIL_START

class imapSELECT_FLAGS : public imapHandlerStructured {

	void (imapSELECT_FLAGS::*next_func)(imap &, Token t);

public:
	imapSELECT_FLAGS();
	~imapSELECT_FLAGS();
	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *errmsg);

	void process(imap &imapAccount, Token t);
	void start_flags(imap &imapAccount, Token t);
	void process_flags(imap &imapAccount, Token t);
};

LIBMAIL_END

mail::imapSELECT_FLAGS::imapSELECT_FLAGS()
	: next_func(&mail::imapSELECT_FLAGS::start_flags)
{
}

mail::imapSELECT_FLAGS::~imapSELECT_FLAGS()
{
}

void mail::imapSELECT_FLAGS::installed(mail::imap &imapAccount)
{
}

const char *mail::imapSELECT_FLAGS::getName()
{
	return ("* FLAGS");
}

void mail::imapSELECT_FLAGS::timedOut(const char *errmsg)
{
}

void mail::imapSELECT_FLAGS::process(mail::imap &imapAccount, Token t)
{
	(this->*next_func)(imapAccount, t);
}

//
// Helper class for processing an untagged * #nnnn response during a SELECT
//

LIBMAIL_START

class imapSELECT_COUNT : public imapHandlerStructured {

	imapSELECT &select;

	void (imapSELECT_COUNT::*next_func)(imap &, Token t);

	size_t count;

public:
	imapSELECT_COUNT(imapSELECT &mySelect,
			       size_t myCount);
	~imapSELECT_COUNT();
	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *);
	void process(imap &imapAccount, Token t);
	void get_count(imap &imapAccount, Token t);
};

LIBMAIL_END

mail::imapSELECT_COUNT::imapSELECT_COUNT(mail::imapSELECT &mySelect,
					       size_t myCount)
	: select(mySelect),
	  next_func(&mail::imapSELECT_COUNT::get_count),
	  count(myCount)
{
}

mail::imapSELECT_COUNT::~imapSELECT_COUNT()
{
}

void mail::imapSELECT_COUNT::installed(mail::imap &imapAccount)
{
}

const char *mail::imapSELECT_COUNT::getName()
{
	return ("* #");
}

void mail::imapSELECT_COUNT::timedOut(const char *errmsg)
{
}

void mail::imapSELECT_COUNT::process(mail::imap &imapAccount, Token t)
{
	(this->*next_func)(imapAccount, t);
}

//
// Helper class for processing an untagged * OK response during a SELECT
//

LIBMAIL_START

class imapSELECT_OK : public imapHandler {

	imapSELECT &select;
	string type;
public:
	imapSELECT_OK(imapSELECT &mySelect,
			    string typeArg);
	~imapSELECT_OK();
	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *errmsg);

	int process(imap &imapAccount, string &buffer);
	void process(string &buffer);
};

LIBMAIL_END

mail::imapSELECT_OK::imapSELECT_OK(mail::imapSELECT &mySelect,
					 string typeArg)
	: select(mySelect), type(typeArg)
{
	upper(type);
}

mail::imapSELECT_OK::~imapSELECT_OK()
{
}

void mail::imapSELECT_OK::installed(mail::imap &imapAccount)
{
}

const char *mail::imapSELECT_OK::getName()
{
	return("* OK");
}

void mail::imapSELECT_OK::timedOut(const char *errmsg)
{
}

//////////////////////////////////////////////////////////////////////////
//
// After a SELECT, and any time * n EXISTS received, a SELECT FLAGS UID
// is automatically issued (either for the entire folder, or just the new
// messages).  When this command completes, the application callback methods
// will be invoked accordingly.
//

LIBMAIL_START

class imapSYNCHRONIZE : public imapCommandHandler {

	mail::callback &callback;
 
	size_t fetchedCounter;

	size_t newExists;

public:
	static size_t counter;
	// Keeps track of the # of objects in existence.

	imapSYNCHRONIZE(mail::callback &callbackArg);
	~imapSYNCHRONIZE();

	void installed(imap &imapAccount);

	void fetchedUID();

private:
	const char *getName();
	void timedOut(const char *errmsg);

	bool untaggedMessage(imap &imapAccount, string name);
	bool taggedMessage(imap &imapAccount, string name,
			   string message,
			   bool okfail, string errmsg);
};

LIBMAIL_END

const char *mail::imapSYNCHRONIZE::getName()
{
	return("SYNC");
}

void mail::imapSYNCHRONIZE::timedOut(const char *errmsg)
{
	callbackTimedOut(callback, errmsg);
}

///////////////////////////////////////////////////////////////////////
//
// Stub call from mail::imapFolder::open()
//

void mail::imap::openFolder(string path,
			    mail::snapshot *restoreSnapshot,
			    mail::callback &openCallback,
			    mail::callback::folder &folderCallback)
{
	if (!ready(openCallback))
		return;

	// If an existing folder is opened, tell the folder not to poll for
	// new mail any more.
	if (currentFolder)
		currentFolder->closeInProgress=true;

	installForegroundTask(smap ?
			      (imapHandler *)
			      new mail::smapOPEN(path,
						 restoreSnapshot,
						 openCallback,
						 folderCallback)
			      : new mail::imapSELECT(path, openCallback,
						     folderCallback));
}

//
// A manually-requested NOOP.
//

LIBMAIL_START

class imapCHECKMAIL : public imapCommandHandler {

	mail::callback &callback;

public:
	imapCHECKMAIL(mail::callback &myCallback);
	~imapCHECKMAIL();
	void installed(imap &imapAccount);

	class dummyCallback : public mail::callback {
	public:
		dummyCallback();
		~dummyCallback();

		void success(string);
		void fail(string);

		void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal);
	};

private:

	const char *getName();

	void timedOut(const char *errmsg);

	bool untaggedMessage(imap &imapAccount, string name);

	bool taggedMessage(imap &imapAccount, string name,
			   string message,
			   bool okfail, string errmsg);
};

LIBMAIL_END

mail::imapCHECKMAIL::imapCHECKMAIL(mail::callback &myCallback)
	: callback(myCallback)
{
}

mail::imapCHECKMAIL::~imapCHECKMAIL()
{
}

void mail::imapCHECKMAIL::installed(mail::imap &imapAccount)
{
	imapAccount.imapcmd("NOOP2", "NOOP\r\n");
}

const char *mail::imapCHECKMAIL::getName()
{
	return "NOOP2";
}

void mail::imapCHECKMAIL::timedOut(const char *errmsg)
{
	callbackTimedOut(callback, errmsg);
}

bool mail::imapCHECKMAIL::untaggedMessage(mail::imap &imapAccount, string name)
{
	return false;
}

bool mail::imapCHECKMAIL::taggedMessage(mail::imap &imapAccount, string name,
					string message,
					bool okfail, string errmsg)
{

	// The check for completion of a manual new mail check must be
	// done slightly differently.  That's because new mail
	// processing is handled by the regular new mail processing
	// objects, which don't really know anything about us.
	// If new mail processing got kicked off, there should be a
	// mail::imapSYNCHRONIZE object floating around somewhere.
	// If there is, what we do is we throw another checkNewMail
	// object in the queue, which should float to the beginning of
	// the queue after the current mail::imapSYNCHRONIZE object
	// goes away.

	if (name == "NOOP2")
	{
		if (mail::imapSYNCHRONIZE::counter == 0)
		{
			callback.success("DONE");
			imapAccount.uninstallHandler(this);
			return true;
		}

		imapAccount.installForegroundTask(new
					   mail::imapCHECKMAIL(callback));

		imapAccount.uninstallHandler(this);
		return true;
	}

	return false;
}

mail::imapCHECKMAIL::dummyCallback::dummyCallback()
{
}

mail::imapCHECKMAIL::dummyCallback::~dummyCallback()
{
}

void mail::imapCHECKMAIL::dummyCallback::success(string dummy)
{
	delete this;
}

void mail::imapCHECKMAIL::dummyCallback::fail(string dummy)
{
	delete this;
}

void mail::imapCHECKMAIL
::dummyCallback::reportProgress(size_t bytesCompleted,
				size_t bytesEstimatedTotal,

				size_t messagesCompleted,
				size_t messagesEstimatedTot)
{
}

void mail::imap::checkNewMail(mail::callback &callback)
{
	if (!ready(callback))
		return;

	if (currentFolder)
		currentFolder->resetMailCheckTimer();

	installForegroundTask(smap ?
			      (imapHandler *)new smapNoopExpunge("NOOP",
								 callback,
								 *this)
			      : new mail::imapCHECKMAIL(callback));
}

void mail::imapFOLDER::resetMailCheckTimer()
{
	// Reset the regular new mail checking interval
	setTimeout(mailCheckInterval);
}

//////////////////////////////////////////////////////////////////////////////
// SELECT processing.

bool mail::imapSELECT::untaggedMessage(mail::imap &imapAccount, string name)
{
	if (name == "FLAGS")
	{
		imapAccount.installBackgroundTask(new mail::imapSELECT_FLAGS()
						  );
		return true;
	}
	if (name == "OK" || name == "NO")
	{
		imapAccount.installBackgroundTask(new mail::imapSELECT_OK(*this,
								   name));
		return true;
	}

	if (isdigit((int)(unsigned char)*name.begin()))
	{
		size_t c=0;

		istringstream(name.c_str()) >> c;

		imapAccount.installBackgroundTask(new mail::imapSELECT_COUNT(*this,
								      c));
		return true;
	}

	return false;
}

void mail::imapSELECT_FLAGS::start_flags(mail::imap &imapAccount, Token t)
{
	imapAccount.folderFlags.clear();
	if (t == NIL)
	{
		done();
		return;
	}

	if (t != '(')
		error(imapAccount);
	next_func= &mail::imapSELECT_FLAGS::process_flags;
}

void mail::imapSELECT_FLAGS::process_flags(mail::imap &imapAccount, Token t)
{
	if (t == ATOM)
	{
		string name=t.text;

		upper(name);
		imapAccount.folderFlags.insert(name);
		return;
	}

	if (t != ')')
		error(imapAccount);
	done();
}

void mail::imapSELECT_COUNT::get_count(mail::imap &imapAccount, Token t)
{
	if (t != ATOM)
		error(imapAccount);
	else
	{
		if (strcasecmp(t.text.c_str(), "EXISTS") == 0)
		{
			select.exists=count;

			// Check against hostile servers

			if (select.exists > UINT_MAX
			    / sizeof(mail::messageInfo))
				select.exists=UINT_MAX
					/ sizeof(mail::messageInfo);
		}
	}
	done();
}

int mail::imapSELECT_OK::process(mail::imap &imapAccount, string &buffer)
{
	size_t p=buffer.find('\n');

	if (p == std::string::npos)
	{
		if (p + 16000 < buffer.size())
			return buffer.size() - 16000;
		// SANITY CHECK - DON'T LET HOSTILE SERVER DOS us

		return 0;
	}

	string buffer_cpy=buffer;

	buffer_cpy.erase(p);
	process(buffer_cpy);
	imapAccount.uninstallHandler(this);
	return p+1;
}

void mail::imapSELECT_OK::process(string &buffer)
{
	if (type != "OK")
		return;

	string::iterator b=buffer.begin(), e=buffer.end();

	while (b != e && unicode_isspace((unsigned char)*b))
		b++;

	if (b == e || *b != '[')
		return;
	b++;

	string w=mail::imap::get_word(&b, &e);

	upper(w);

	if (w == "UIDVALIDITY") // Memorize * OK [UIDVALIDITY]
	{
		w=mail::imap::get_word(&b, &e);
		select.uidv=w;
	}
	else if (w == "PERMANENTFLAGS")
	{
		myimap->setPermanentFlags(b, e);
	}
}

//
// After an initial SELECT succeeds, even if the subsequent resynchronization
// commands fail, we still want to indicate success.
//

LIBMAIL_START

class imapSELECTCallback : public mail::callback {

	mail::callback &origCallback;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	imapSELECTCallback(mail::callback &cb);
	~imapSELECTCallback();
	void success(string msg);
	void fail(string msg);
};
LIBMAIL_END

bool mail::imapSELECT::taggedMessage(mail::imap &imapAccount, string name,
				     string message,
				     bool okfail, string errmsg)
{
	if (name != "SELECT")
		return false;

	if (okfail && uidv.length() == 0)
	{
		okfail=false;
		errmsg="Server did not return the folder UID-validity token.";
	}

	if (okfail)
	{
		mail::imapFOLDER *f=new mail::imapFOLDER(path, uidv, exists,
							 folderCallback,
							 imapAccount);
		imapAccount.installBackgroundTask(f);
		imapAccount.currentFolder=f;

		f->exists=0;

		if (exists > 0)
		{
			mail::imapSELECTCallback *cb=
				new mail::imapSELECTCallback(openCallback);

			try {
				imapAccount.installForegroundTask
					(new mail::imapSYNCHRONIZE(*cb));
			} catch (...) {
				delete cb;
			}
		}
		else
			openCallback.success("Empty folder.");
	}
	else
	{
		imapAccount.currentFolder=NULL;
		openCallback.fail(errmsg);
	}
	imapAccount.uninstallHandler(this);
	return true;
}

mail::imapSELECTCallback::imapSELECTCallback(mail::callback &cb)
	: origCallback(cb)
{
}

mail::imapSELECTCallback::~imapSELECTCallback()
{
}

void mail::imapSELECTCallback::success(string msg)
{
	origCallback.success(msg);
	delete this;
}

void mail::imapSELECTCallback::fail(string msg)
{
	origCallback.success(msg);
	delete this;
}

void mail::imapSELECTCallback::reportProgress(size_t bytesCompleted,
					      size_t bytesEstimatedTotal,

					      size_t messagesCompleted,
					      size_t messagesEstimatedTotal)
{
	origCallback.reportProgress(bytesCompleted, bytesEstimatedTotal,
				    messagesCompleted,
				    messagesEstimatedTotal);
}

////////////////////////////////////////////////////////////////////////
//
// Helper class to resynchronize a folder


size_t mail::imapSYNCHRONIZE::counter=0;

mail::imapSYNCHRONIZE::imapSYNCHRONIZE(mail::callback &callbackArg)
	: callback(callbackArg), fetchedCounter(0)
{
	++counter;
}

void mail::imapSYNCHRONIZE::installed(mail::imap &imapAccount)
{
	ostringstream o;

	newExists=imapAccount.currentFolder ?
		imapAccount.currentFolder->index.size():0;

	if (imapAccount.currentFolder == NULL ||
	    imapAccount.currentFolder->exists >= newExists)
	{
		imapAccount.currentFolder->exists=newExists; // Insurance

		o << "NOOP\r\n";
	}
	else
	{

		o << "FETCH " << (imapAccount.currentFolder->exists+1)
		  << ":" << newExists << " (UID FLAGS)\r\n";

	}

	imapAccount.imapcmd("SYNC", o.str());
	imapAccount.currentSYNCHRONIZE=this;
}

mail::imapSYNCHRONIZE::~imapSYNCHRONIZE()
{
	if (myimap)
		myimap->currentSYNCHRONIZE=NULL;
	--counter;
}

void mail::imapSYNCHRONIZE::fetchedUID()
{
	size_t c=myimap && myimap->currentFolder ?
		myimap->currentFolder->exists:0;


	if (c >= newExists)
		c=newExists;

	if (fetchedCounter < newExists - c)
	{
		++fetchedCounter;
		callback.reportProgress(0, 0,
					fetchedCounter, newExists - c);
	}
}

bool mail::imapSYNCHRONIZE::untaggedMessage(mail::imap &imapAccount, string name)
{
	return false;
}

bool mail::imapSYNCHRONIZE::taggedMessage(mail::imap &imapAccount, string name,
					  string message,
					  bool okfail, string errmsg)
{
	if (name != "SYNC")
		return false;

	if (!okfail)
	{
		callback.fail(errmsg);
		imapAccount.uninstallHandler(this);
		return true;
	}

	mail::imapFOLDERinfo *f=imapAccount.currentFolder;

	size_t n;

	for (n=f->exists; n<f->index.size(); n++)
		if (f->index[n].uid.length() == 0)
		{
			callback.fail("Server failed to respond with message identifier.");
			imapAccount.uninstallHandler(this);
			return true;
		}

	if (f->exists < newExists)
		f->exists=newExists;


	callback.success("Folder index updated.");
	imapAccount.uninstallHandler(this);
	return true;
}


size_t mail::imap::getFolderIndexSize()
{
	if (currentFolder == NULL)
		return 0;

	return currentFolder->exists;
}

mail::messageInfo mail::imap::getFolderIndexInfo(size_t n)
{
	if (currentFolder  != NULL && n < currentFolder->exists)
		return currentFolder->index[n];

	return mail::messageInfo();
}

void mail::imap::getFolderKeywordInfo(size_t n, std::set<std::string> &kwset)
{
	if (currentFolder != NULL && n < currentFolder->exists)
	{
		currentFolder->index[n].keywords.getFlags(kwset);
	}
	else
		kwset.clear();
}

//////////////////////////////////////////////////////////////////////////
//
// Server replies when a folder is currently open.
//

const char mail::imapFOLDER::name[]="FolderHandler";

//
// Helper class for processing an untagged * #nnnn response during a SELECT
//

LIBMAIL_START

class imapFOLDER_COUNT : public imapHandlerStructured {

	void (imapFOLDER_COUNT::*next_func)(imap &, Token t);

	size_t count;
	bool body_part_reference; // Fetch References: header
	string references_buf;
	vector<string> flags;

	bool accept_largestring;

	bool wantLargeString();
public:
	imapFOLDER_COUNT(size_t myCount);
	~imapFOLDER_COUNT();
	void installed(imap &imapAccount) {}
private:
	const char *getName();
	void timedOut(const char *errmsg);
	void process(imap &imapAccount, Token t);

	void get_count(imap &imapAccount, Token t);

	void get_fetch(imap &imapAccount, Token t);
	void get_fetch_item(imap &imapAccount, Token t);
	void get_fetch_uid(imap &imapAccount, Token t);

	void get_fetch_flags(imap &imapAccount, Token t);
	void get_fetch_flag(imap &imapAccount, Token t);
	void saveflags(imap &imapAccount);

	void get_internaldate(imap &imapAccount, Token t);
	void get_rfc822size(imap &imapAccount, Token t);

	void get_envelope(imap &imapAccount, Token t);
	void get_bodystructure(imap &imapAccount, Token t);
	void get_body(imap &imapAccount, Token t);

	bool fillbodystructure(imap &imapAccount, imapparsefmt &parser,
			       mimestruct &bodystructure,
			       string mime_id);

	imapparsefmt parser;
};

LIBMAIL_END

mail::imapFOLDER::imapFOLDER(string pathArg, string uidvArg, size_t existsArg,
			     mail::callback::folder &folderCallbackArg,
			     mail::imap &myserver)
	: mail::imapCommandHandler(myserver.noopSetting),
	  imapFOLDERinfo(pathArg, folderCallbackArg),
	  existsNotify(NULL),
	  uidv(uidvArg)
{
	mailCheckInterval=myserver.noopSetting;
	setBackgroundTask(true);
	index.resize(existsArg);
}

mail::imapFOLDER::~imapFOLDER()
{
	if (existsNotify)
		existsNotify->me=NULL;
}

mail::imapFOLDER::existsCallback::existsCallback()
{
}

mail::imapFOLDER::existsCallback::~existsCallback()
{
}

void mail::imapFOLDER::existsCallback::success(string message)
{
	if (me)
	{
		mail::imapFOLDER *cb=me;
		me=NULL;
		cb->existsNotify=NULL;
		cb->folderCallback.newMessages();
	}
	delete this;
}

void mail::imapFOLDER::existsCallback
::reportProgress(size_t bytesCompleted,
		 size_t bytesEstimatedTotal,

		 size_t messagesCompleted,
		 size_t messagesEstimatedTotal)
{
}

void mail::imapFOLDER::existsCallback::fail(string message)
{
	if (me)
	{
		mail::imapFOLDER *cb=me;
		me=NULL;
		cb->existsNotify=NULL;
		cb->folderCallback.newMessages();
	}
	delete this;
}

const char *mail::imapFOLDER::getName()
{
	return name;
}

void mail::imapFOLDER::installed(mail::imap &imapAccount)
{
}

void mail::imapFOLDER::timedOut(const char *errmsg)
{
}

int mail::imapFOLDER::getTimeout(mail::imap &imapAccount)
{
	int t=mail::imapCommandHandler::getTimeout(imapAccount);

	if (t == 0)
	{
		setTimeout(t=imapAccount.noopSetting);

		imapHandler *h;

		if (!closeInProgress // Not closing the folder
		    && ((h=imapAccount.hasForegroundTask()) == NULL ||
			strcmp(h->getName(), "IDLE") == 0))
			// Something else is already in progress
		{
			imapCHECKMAIL::dummyCallback *dummy=
				new imapCHECKMAIL::dummyCallback();

			if (!dummy)
				LIBMAIL_THROW((strerror(errno)));

			try {
				imapAccount
					.installForegroundTask(new
							       imapCHECKMAIL
							       (*dummy));
			} catch (...) {
				delete dummy;
				throw;
			}
		}
	}
	return t;	// This handler never causes a timeout
}

bool mail::imapFOLDER::untaggedMessage(mail::imap &imapAccount, string name)
{
	if (isdigit((int)(unsigned char)*name.begin()))
	{
		if (imapAccount.handlers.count(mail::imapSELECT::name) > 0)
			return false;	// SELECT in progress
		size_t c=0;

		istringstream(name.c_str()) >> c;

		imapAccount.installBackgroundTask(new mail::imapFOLDER_COUNT(c));
		return true;
	}

	if (name == "FLAGS")
	{
		imapAccount
			.installBackgroundTask(new mail::imapSELECT_FLAGS());
		return true;
	}

	return false;
}

bool mail::imapFOLDER::taggedMessage(mail::imap &imapAccount, string name,
				     string message,
				     bool okfail, string errmsg)
{
	return false;
}


mail::imapFOLDER_COUNT::imapFOLDER_COUNT(size_t myCount)
	: next_func(&mail::imapFOLDER_COUNT::get_count),
	  count(myCount), body_part_reference(false),
	  accept_largestring(false)
{
}

mail::imapFOLDER_COUNT::~imapFOLDER_COUNT()
{
}

bool mail::imapFOLDER_COUNT::wantLargeString()
{
	return accept_largestring;
}

const char *mail::imapFOLDER_COUNT::getName()
{
	return ("* #");
}

void mail::imapFOLDER_COUNT::timedOut(const char *errmsg)
{
}

void mail::imapFOLDER_COUNT::process(mail::imap &imapAccount, Token t)
{
	(this->*next_func)(imapAccount, t);
}

// Seen * nnnnn

void mail::imapFOLDER_COUNT::get_count(mail::imap &imapAccount, Token t)
{
	if (t != ATOM)
	{
		error(imapAccount);
		return;
	}

	string name=t.text;

	upper(name);
	if (name == "FETCH")
	{
		next_func= &mail::imapFOLDER_COUNT::get_fetch;
		return;
	}

	if (name == "EXPUNGE")
	{
		done();

		if (imapAccount.currentFolder != NULL && count > 0 &&
		    count <= imapAccount.currentFolder->index.size())
		{
			size_t c=count-1;

			vector<imapFOLDERinfo::indexInfo> &indexRef=
				imapAccount.currentFolder->index;

			indexRef.erase(indexRef.begin() + c,
				       indexRef.begin() + c + 1);

			if (c < imapAccount.currentFolder->exists)
			{
				--imapAccount.currentFolder->exists;

				vector< pair<size_t, size_t> > a;

				a.push_back(make_pair(c, c));
				imapAccount.currentFolder->folderCallback
					.messagesRemoved(a);
			}
		}
		return;
	}
	if (name == "EXISTS")
	{
		done();

		if (imapAccount.currentFolder != NULL &&
		    !imapAccount.currentFolder->closeInProgress &&
		    count > imapAccount.currentFolder->index.size())
		{
			imapAccount.currentFolder
				->existsMore(imapAccount, count);
		}
	}

	done();
}

void mail::imapFOLDER::existsMore(mail::imap &imapAccount, size_t count)
{
	if (closeInProgress)
		return;

	if (exists > count)
		exists=count;

	index.resize(count, imapFOLDERinfo::indexInfo());

	if (existsNotify)
		existsNotify->me=NULL;

	existsNotify=new mail::imapFOLDER::existsCallback;

	if (!existsNotify)
		LIBMAIL_THROW("Out of memory.");

	existsNotify->me=this;

	imapAccount
		.installForegroundTask(new mail::imapSYNCHRONIZE(*existsNotify)
				       );
}

void mail::imapFOLDER_COUNT::get_fetch(mail::imap &imapAccount, Token t)
{
	if (t != '(')
		error(imapAccount);
	next_func= &mail::imapFOLDER_COUNT::get_fetch_item;
}

void mail::imapFOLDER_COUNT::get_fetch_item(mail::imap &imapAccount, Token t)
{
	accept_largestring=false;
	if (t == ')')
	{
		done();
		return;
	}

	if (t != ATOM)
	{
		error(imapAccount);
		return;
	}

	string name=t.text;

	upper(name);

	if (name == "UID")
	{
		next_func= &mail::imapFOLDER_COUNT::get_fetch_uid;
		return;
	}

	if (name == "FLAGS")
	{
		next_func= &mail::imapFOLDER_COUNT::get_fetch_flags;
		return;
	}

	if (name == "INTERNALDATE")
	{
		next_func= &mail::imapFOLDER_COUNT::get_internaldate;
		return;
	}

	if (name == "RFC822.SIZE")
	{
		next_func= &mail::imapFOLDER_COUNT::get_rfc822size;
		return;
	}

	if (name == "ENVELOPE")
	{
		parser.begin();
		next_func= &mail::imapFOLDER_COUNT::get_envelope;
		return;
	}

	if (name == "BODYSTRUCTURE")
	{
		parser.begin();
		next_func= &mail::imapFOLDER_COUNT::get_bodystructure;
		return;
	}

	if (strncasecmp(name.c_str(), "BODY[", 5) == 0 ||
	    strncasecmp(name.c_str(), "BODY.PEEK[", 10) == 0)
	{
		accept_largestring=true;
		next_func= &mail::imapFOLDER_COUNT::get_body;

		size_t i=name.find(']');

		if (i != name.length()-1)
		{
			error(imapAccount);
			return;
		}
		name.erase(i);
		name.erase(0, name.find('[')+1);

		body_part_reference=strncasecmp(name.c_str(),
						"HEADER.FIELDS", 13) == 0;
		references_buf="";
		// Getting the References: header.
		return;
	}
			
	error(imapAccount);
}

void mail::imapFOLDER_COUNT::get_fetch_uid(mail::imap &imapAccount, Token t)
{
	if (t != ATOM)
	{
		error(imapAccount);
		return;
	}

	if (imapAccount.currentFolder && count > 0)
		imapAccount.currentFolder->setUid(count-1, t.text);

	// Report progress
	if (imapAccount.currentSYNCHRONIZE)
		imapAccount.currentSYNCHRONIZE->fetchedUID();

	next_func= &mail::imapFOLDER_COUNT::get_fetch_item;
}

void mail::imapFOLDER::setUid(size_t count, string uid)
{
	imapFOLDERinfo::setUid(count, uidv + "/" + uid);
}

void mail::imapFOLDER_COUNT::get_fetch_flags(mail::imap &imapAccount, Token t)
{
	flags.clear();

	if (t == NIL)
	{
		saveflags(imapAccount);
		return;
	}

	if (t != '(')
	{
		error(imapAccount);
		return;
	}

	next_func= &mail::imapFOLDER_COUNT::get_fetch_flag;
}

void mail::imapFOLDER_COUNT::get_fetch_flag(mail::imap &imapAccount, Token t)
{
	if (t == ')')
	{
		saveflags(imapAccount);
		return;
	}

	if (t != ATOM)
	{
		error(imapAccount);
		return;
	}

	flags.push_back(t.text);
}

void mail::imapFOLDER_COUNT::saveflags(mail::imap &imapAccount)
{
	next_func= &mail::imapFOLDER_COUNT::get_fetch_item;

	if (imapAccount.currentFolder && count > 0 &&
	    count <= imapAccount.currentFolder->index.size())
	{
		imapFOLDERinfo::indexInfo &i=
			imapAccount.currentFolder->index[count-1];

		if (imapAccount.folderFlags.count("\\DRAFT") > 0)
			i.draft=0;

		if (imapAccount.folderFlags.count("\\ANSWERED") > 0)
			i.replied=0;

		if (imapAccount.folderFlags.count("\\FLAGGED") > 0)
			i.marked=0;

		if (imapAccount.folderFlags.count("\\DELETED") > 0)
			i.deleted=0;

		if (imapAccount.folderFlags.count("\\SEEN") > 0)
			i.unread=1;

		if (imapAccount.folderFlags.count("\\RECENT") > 0)
			i.recent=0;

		size_t n;

		mail::keywords::Message newMessage;

		for (n=0; n<flags.size(); n++)
		{
			string f=flags[n];

			upper(f);
			if (f == "\\DRAFT")
				i.draft=1;
			else if (f == "\\ANSWERED")
				i.replied=1;
			else if (f == "\\FLAGGED")
				i.marked=1;
			else if (f == "\\DELETED")
				i.deleted=1;
			else if (f == "\\SEEN")
				i.unread=0;
			else if (f == "\\RECENT")
				i.recent=1;
			else if (*flags[n].c_str() != '\\')
			{
				if (!newMessage.addFlag(imapAccount
							.keywordHashtable,
							flags[n]))
					LIBMAIL_THROW(strerror(errno));
			}
		}

		i.keywords=newMessage;

		if (count <= imapAccount.currentFolder->exists)
			imapAccount.currentFolder->
				folderCallback.messageChanged(count-1);
	}
}

static void parse_addr_list(mail::imapparsefmt *list,
			    vector<mail::address> &addresses)
{
	vector<mail::imapparsefmt *>::iterator b=list->children.begin(),
		e=list->children.end();

	while (b != e)
	{
		mail::imapparsefmt *address= *b;

		if (address->children.size() >= 4)
		{
			mail::imapparsefmt *name=address->children[0];
			mail::imapparsefmt *mailbox=address->children[2];
			mail::imapparsefmt *host=address->children[3];

			if (host->nil)
			{
				if (mailbox->nil)
					addresses.
						push_back(mail::address(";",
									""));
				else
					addresses.
						push_back(mail::address(mailbox
									->value
									+ ":",
									""));
			}
			else
			{
				addresses.
					push_back(mail::address(name->value,
								mailbox->value
								+ "@" +
								host->value));
			}
		}
		b++;
	}
}

static bool fillenvelope(mail::imapparsefmt &imapenvelope,
			 mail::envelope &envelope)
{
	if (imapenvelope.children.size() < 10)
		return false;

	vector<mail::imapparsefmt *>::iterator b=imapenvelope.children.begin();


	envelope.date=rfc822_parsedt((*b)->value.c_str()); b++;

	envelope.subject= (*b)->value; b++;

	parse_addr_list(*b, envelope.from); b++;
	parse_addr_list(*b, envelope.sender); b++;
	parse_addr_list(*b, envelope.replyto); b++;
	parse_addr_list(*b, envelope.to); b++;
	parse_addr_list(*b, envelope.cc); b++;
	parse_addr_list(*b, envelope.bcc); b++;

	envelope.inreplyto= (*b)->value; b++;
	envelope.messageid= (*b)->value;
	return true;
}

void mail::imapFOLDER_COUNT::get_internaldate(mail::imap &imapAccount, Token t)
{
	if (t != ATOM && t != STRING)
	{
		error(imapAccount);
		return;
	}

	if (imapAccount.currentFetch)
	{
		// Make IMAP date presentable for rfc822_parsedt() by
		// replacing dd-mmm-yyyy with dd mmm yyyy

		string d=t.text;

		if (d[0] == ' ')
			d[0]='0';

		size_t spacepos=d.find(' ');
		size_t n;

		while ((n=d.find('-')) < spacepos)
			d[n]=' ';

		time_t tm=rfc822_parsedt(d.c_str());

		if (tm)
			imapAccount.currentFetch
				->messageArrivalDateCallback(count-1, tm);
	}

	next_func= &mail::imapFOLDER_COUNT::get_fetch_item;
}

void mail::imapFOLDER_COUNT::get_rfc822size(mail::imap &imapAccount, Token t)
{
	if (t != ATOM)
	{
		error(imapAccount);
		return;
	}

	if (imapAccount.currentFetch)
	{
		unsigned long msgsize=0;

		istringstream(t.text.c_str()) >> msgsize;

		imapAccount.currentFetch
			->messageSizeCallback(count-1, msgsize);
	}

	next_func= &mail::imapFOLDER_COUNT::get_fetch_item;
}

void mail::imapFOLDER_COUNT::get_envelope(mail::imap &imapAccount, Token t)
{
	bool err_flag;

	if (!parser.process(imapAccount, t, err_flag))
		return;	// Not yet

	next_func= &mail::imapFOLDER_COUNT::get_fetch_item;

	mail::envelope envelope;

	if (err_flag || !fillenvelope(parser, envelope))
	{
		error(imapAccount);
		return;
	}

	if (imapAccount.currentFetch)
		imapAccount.currentFetch
			->messageEnvelopeCallback(count-1, envelope);
}


static void fill_type_parameters(mail::imapparsefmt *ptr,
				  mail::mimestruct &bodystructure)
{
	vector<mail::imapparsefmt *>::iterator
		bb=ptr->children.begin(),
		ee=ptr->children.end();

	while (bb != ee)
	{
		string name= (*bb)->value; bb++;

		if (bb != ee)
		{
			bodystructure.type_parameters.
				set_simple(name, (*bb)->value);
			bb++;
		}
	}
}


static void fill_disposition(mail::imapparsefmt *ptr,
			     mail::mimestruct &bodystructure)
{
	if (ptr->children.size() > 0)
		bodystructure.content_disposition=
			ptr->children[0]->value;

	if (ptr->children.size() < 2)
		return;

	vector<mail::imapparsefmt *>::iterator
		bb=ptr->children[1]->children.begin(),
		ee=ptr->children[1]->children.end();

	while (bb != ee)
	{
		string name= (*bb)->value; bb++;

		if (bb != ee)
		{
			bodystructure.content_disposition_parameters
				.set_simple(name, (*bb)->value);
			bb++;
		}
	}
}

bool mail::imapFOLDER_COUNT::fillbodystructure(mail::imap &imapAccount,
					       mail::imapparsefmt &parser,
					       mail::mimestruct &bodystructure,
					       string mime_id)
{
	vector<mail::imapparsefmt *>::iterator b=parser.children.begin(),
		e=parser.children.end();

	if (b == e)
	{
		error(imapAccount);
		return false;
	}

	if ( (*b)->children.size() == 0)  // Non-multipart
	{

		bodystructure.mime_id=mime_id;

		if (mime_id.length() == 0)
			mime_id="1";

		bodystructure.type= (*b)->value; b++;

		if (b != e)
		{
			bodystructure.subtype= (*b)->value;
			b++;
		}

		mail::upper(bodystructure.type);
		mail::upper(bodystructure.subtype);

		if (b != e)
		{
			fill_type_parameters( *b, bodystructure ); b++;
		}

		if (b != e)
		{
			bodystructure.content_id= (*b)->value;
			b++;
		}

		if (b != e)
		{
			bodystructure.content_description= (*b)->value;
			b++;
		}

		if (b != e)
		{
			bodystructure.content_transfer_encoding= (*b)->value;
			b++;
		}

		if (b != e)
		{
			istringstream((*b)->value.c_str()) >>
				bodystructure.content_size;
			b++;

			if (bodystructure.messagerfc822())
			{
				if (b != e)
				{
					if (!fillenvelope(**b,
							  bodystructure
							  .getEnvelope())
					    )
					{
						error(imapAccount);
						return false;
					}

					b++;
				}
				if (b != e)
				{
					if ( (*b)->children.size() == 0)
						mime_id=mime_id + ".1";

					mail::mimestruct &child_struct=
						*bodystructure.addChild();

					if (!fillbodystructure(imapAccount,
							       **b,
							       child_struct,
							       mime_id))
						return false;

					if (child_struct.getNumChildren() == 0)
						child_struct.mime_id=
							mime_id + ".1";
					// Fixup
				}

				if (b != e)
				{
					istringstream((*b)->value.c_str()) >>
						bodystructure.content_lines;
				}
			}
			else if (bodystructure.type == "TEXT")
			{
				if (b != e)
				{
					istringstream((*b)->value.c_str()) >>
						bodystructure.content_lines;
					b++;
				}
			}
		}

		if (b != e)
		{
			bodystructure.content_md5= (*b)->value;
			b++;
		}
	}
	else	// MULTIPART
	{
		size_t body_num=1;

		bodystructure.mime_id=mime_id;

		if (mime_id.length() > 0)
			mime_id=mime_id+".";
#if 0
		else
			bodystructure.mime_id="1";
#endif

		while (b != e)
		{
			if ( (*b)->children.size() == 0)
				break;

			string buffer;
			{
				ostringstream o;

				o << body_num;
				buffer=o.str();
			}
			body_num++;

			if (!fillbodystructure(imapAccount, **b,
					       *bodystructure.addChild(),
					       mime_id + buffer))
				return false;
			b++;
		}

		bodystructure.type="MULTIPART";
		bodystructure.subtype="MIXED";

		if (b != e)
		{
			bodystructure.subtype= (*b)->value;
			b++;
		}

		mail::upper(bodystructure.subtype);

		if (b != e)
		{
			fill_type_parameters( *b, bodystructure );
			b++;
		}
	}


	if (b != e)
	{
		fill_disposition( (*b), bodystructure );
		b++;
	}

	if (b != e)
	{
		bodystructure.content_language= (*b)->value;
		b++;
	}

	return true;
}

void mail::imapFOLDER_COUNT::get_bodystructure(mail::imap &imapAccount, Token t)
{
	bool err_flag;

	if (!parser.process(imapAccount, t, err_flag))
		return;	// Not yet

	next_func= &mail::imapFOLDER_COUNT::get_fetch_item;

	mail::mimestruct bodystructure;
	if (err_flag)
	{
		error(imapAccount);
		return;
	}

	if (!fillbodystructure(imapAccount, parser, bodystructure, ""))
		return;

	if (imapAccount.currentFetch)
		imapAccount.currentFetch
			->messageStructureCallback(count-1, bodystructure);

}

void mail::imapFOLDER_COUNT::get_body(mail::imap &imapAccount, Token t)
{
	if (t == STRING)
		next_func= &mail::imapFOLDER_COUNT::get_fetch_item;

	if (t == STRING || t == LARGESTRING)
	{
		string buf=t.text;

		size_t cr;

		while ((cr=buf.find('\r')) != std::string::npos)
			buf.erase(cr, 1);

		if (imapAccount.currentFetch)
		{
			imapAccount.currentFetch->messageTextEstimatedSize =
				t == LARGESTRING
				? largestringsize:t.text.size();

			imapAccount.currentFetch
				->messageTextCompleted += t.text.size();

			if (t == STRING)
			{
				imapAccount.currentFetch->messageTextCompleted
					= imapAccount.currentFetch->
					messageTextEstimatedSize;
				++imapAccount.currentFetch->messageCntDone;
			}

			if (body_part_reference)
				// Processing References: header
			{
				references_buf += buf;

				while (references_buf.size() > 10000)
					// Prevent the server from DOssing us
				{
					size_t n=references_buf.find('>');

					if (n != std::string::npos)
						++n;
					else n=references_buf.size()-10000;

					references_buf
						.erase(references_buf.begin(),
						       references_buf.begin()
						       +n);
				}

				if (t == STRING)
				{
					vector<address> address_list;
					size_t err_index;

					address::fromString(references_buf,
							    address_list,
							    err_index);

					vector<string> msgid_list;

					vector<address>::iterator
						b=address_list.begin(),
						e=address_list.end();

					while (b != e)
					{
						string s=b->getAddr();

						if (s.size() > 0)
							msgid_list.push_back
								("<" +
								 s + ">");
						++b;
					}

					if (msgid_list.size() > 0)
						imapAccount.currentFetch->
							messageReferencesCallback(count-1, msgid_list);
				}
			}
			else
				imapAccount.currentFetch
					->messageTextCallback(count-1, buf);
		}

	}
	else
		error(imapAccount);
}

/////////////////////////////////////////////////////////////////////////////
//
//  The STORE command.
//

LIBMAIL_START

class imapSTORE : public imapCommandHandler {

	mail::callback &callback;

	string storecmd;

public:
	imapSTORE(mail::callback &callbackArg, string storecmdArg);
	~imapSTORE();

	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *errmsg);

	bool untaggedMessage(imap &imapAccount, string name);
	bool taggedMessage(imap &imapAccount, string name,
			   string message,
			   bool okfail, string errmsg);
};

LIBMAIL_END

void mail::imapSTORE::timedOut(const char *errmsg)
{
	callbackTimedOut(callback, errmsg);
}

mail::imapSTORE::imapSTORE(mail::callback &callbackArg,
				 string storecmdArg)
	: callback(callbackArg), storecmd(storecmdArg)
{
}

mail::imapSTORE::~imapSTORE()
{
}

void mail::imapSTORE::installed(mail::imap &imapAccount)
{
	imapAccount.imapcmd("STORE", storecmd + "\r\n");
}

const char *mail::imapSTORE::getName()
{
	return "STORE";
}

bool mail::imapSTORE::untaggedMessage(mail::imap &imapAccount, string name)
{
	return false;
}

bool mail::imapSTORE::taggedMessage(mail::imap &imapAccount, string name,
				    string message,
				    bool okfail, string errmsg)
{
	if (name == "STORE")
	{
		if (okfail)
			callback.success(errmsg);
		else
			callback.fail(errmsg);
		imapAccount.uninstallHandler(this);
		return true;
	}
	return false;
}

bool mail::imap::keywordAllowed(string kwName)
{
	upper(kwName);

	return permanentFlags.count(kwName) > 0 ||
		permanentFlags.count("\\*") > 0;
}

void mail::imap::saveFolderIndexInfo(size_t messageNum,
				     const mail::messageInfo &indexInfo,
				     mail::callback &callback)
{
	if (!ready(callback))
		return;

	if (messageNum >= getFolderIndexSize())
	{
		callback.success("Invalid message number - ignored.");
		return;
	}

	if (smap)
	{
		string flags;

#define FLAG
#define NOTFLAG !
#define DOFLAG(NOT, field, name) \
		if (NOT indexInfo.field) flags += "," name

		LIBMAIL_SMAPFLAGS;
#undef DOFLAG
#undef NOTFLAG
#undef FLAG

		if (flags.size() > 0)
			flags=flags.substr(1);


		installForegroundTask(new smapSTORE(messageNum,
						    "FLAGS=" + flags,
						    *this,
						    callback));
		return;
	}

	bool fakeUpdateFlag;

	string flags=messageFlagStr(indexInfo,
				    &currentFolder->index[messageNum],
				    &fakeUpdateFlag);

	{
		set<string> oflags;
		currentFolder->index[messageNum].keywords.getFlags(oflags);

		set<string>::iterator b=oflags.begin(), e=oflags.end();

		while (b != e)
		{
			if (keywordAllowed(*b))
			{
				if (flags.size() > 0)
					flags += " ";
				flags += *b;
			}

			++b;
		}
	}

	string messageNumBuffer;

	{
		ostringstream o;

		o << "STORE " << (messageNum+1) << " FLAGS (";
		messageNumBuffer=o.str();
	}

	installForegroundTask(new mail::imapSTORE(callback,
						  messageNumBuffer
						  + flags + ")"));

	if (fakeUpdateFlag && messageNum < currentFolder->exists)
		currentFolder->folderCallback.messageChanged(messageNum);
}

/////////////////////////////////////////////////////////////////////////
//
//  A STORE for a list may be broken up into multiple commands.
//  Count the number of tagged STORE acknowledgments received, and run the
//  app callback only after the last STORE ack comes in.

LIBMAIL_START

class imapUpdateFlagsCallback : public mail::callback {

	mail::callback &origCallback;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	imapUpdateFlagsCallback(mail::callback &callback);
	~imapUpdateFlagsCallback();

	void success(string message);
	void fail(string message);

	bool failFlag;
	string message;

	size_t cnt;
};

LIBMAIL_END

mail::imapUpdateFlagsCallback::imapUpdateFlagsCallback(mail::callback
							     &callback)
	: origCallback(callback),
	  failFlag(false), message(""), cnt(0)
{
}

mail::imapUpdateFlagsCallback::~imapUpdateFlagsCallback()
{
}

void mail::imapUpdateFlagsCallback::success(string message)
{
	// If any failures were received, the app fail() callback will be
	// invoked.

	if (!failFlag)
		this->message=message;

	if (--cnt == 0)
	{
		if (failFlag)
			origCallback.fail(message);
		else
			origCallback.success(message);
		delete this;
	}
}

void mail::imapUpdateFlagsCallback::fail(string message)
{
	this->message=message;
	failFlag=true;
	success(message);
}

void mail::imapUpdateFlagsCallback
::reportProgress(size_t bytesCompleted,
		 size_t bytesEstimatedTotal,

		 size_t messagesCompleted,
		 size_t messagesEstimatedTotal)
{
	origCallback.reportProgress(bytesCompleted,
				    bytesEstimatedTotal,
				    messagesCompleted,
				    messagesEstimatedTotal);
}

void mail::imap::updateFolderIndexFlags(const vector<size_t> &messages,
					bool doFlip,
					bool enableDisable,
					const mail::messageInfo &flags,
					mail::callback &callback)
{
	if (doFlip)
	{
		mail::imapUpdateFlagsCallback *myCallback=
			new mail::imapUpdateFlagsCallback(callback);

		if (!myCallback)
		{
			callback.fail("Out of memory.");
			return;
		}

		//
		// Very ugly, but this is the cleanest ugly solution.
		// For each flipped flag, segregate messages whose
		// flag should be turned on, and ones that should be
		// turned off, and issue a separate STORE for each set.
		// Lather, rinse, repeat, for all flags.
		//

		try {

#define DOFLAG(NOT, field, name) { \
		vector<size_t> enabled, disabled;\
\
		vector<size_t>::const_iterator b=messages.begin(),\
			e=messages.end();\
\
		while (b != e)\
		{\
			size_t n= *b++;\
\
			if (n >= getFolderIndexSize())\
				continue;\
\
			mail::messageInfo &info=currentFolder->index[n];\
\
			if ( flags.field ) \
			{ \
				if (info.field)\
					disabled.push_back(n);\
				else\
					enabled.push_back(n);\
			}\
		}\
\
		mail::messageInfo oneFlag;\
\
		oneFlag.field=true;\
		++myCallback->cnt;\
		updateFolderIndexFlags(enabled, true, oneFlag, *myCallback);\
		++myCallback->cnt;\
		updateFolderIndexFlags(disabled, false, oneFlag, *myCallback);\
		}

			++myCallback->cnt;
#define NOTFLAG
#define FLAG
			LIBMAIL_MSGFLAGS;
#undef NOTFLAG
#undef FLAG
#undef DOFLAG
			myCallback->success("Success");

		} catch (...) {
			delete myCallback;

			callback.fail("An exception occured in updateFolderIndexFlags()");
		}
		return;
	}

	updateFolderIndexFlags(messages, enableDisable, flags, callback);
}

LIBMAIL_START

class imapMessageCallbackStub : public mail::callback::message {

	mail::callback &origCallback;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	imapMessageCallbackStub(mail::callback &callbackArg);
	~imapMessageCallbackStub();

	void success(string message);
	void fail(string message);
};

LIBMAIL_END

mail::imapMessageCallbackStub
::imapMessageCallbackStub(mail::callback &callbackArg)
	: origCallback(callbackArg)
{
}

mail::imapMessageCallbackStub::~imapMessageCallbackStub()
{
}

void mail::imapMessageCallbackStub::success(string message)
{
	origCallback.success(message);
	delete this;
}

void mail::imapMessageCallbackStub::fail(string message)
{
	origCallback.fail(message);
	delete this;
}

void mail::imapMessageCallbackStub
::reportProgress(size_t bytesCompleted,
		 size_t bytesEstimatedTotal,
		 size_t messagesCompleted,
		 size_t messagesEstimatedTotal)
{
	origCallback.reportProgress(bytesCompleted, bytesEstimatedTotal,
				    messagesCompleted, messagesEstimatedTotal);
}

void mail::imap::updateFolderIndexFlags(const vector<size_t> &messages,
					bool enableDisable,
					const mail::messageInfo &flags,
					mail::callback &callback)
{
	if (smap)
	{
		const char *plus="+FLAGS=";
		const char *minus="-FLAGS=";


		string lista;
		string listb;

#define FLAG lista
#define NOTFLAG listb
#define DOFLAG(FLAG, field, name) \
		if (flags.field) FLAG += "," name

		LIBMAIL_SMAPFLAGS;
#undef DOFLAG
#undef NOTFLAG
#undef FLAG

		if (lista.size() > 0)
			lista=(enableDisable ? plus:minus)
				+ lista.substr(1);

		if (listb.size() > 0)
			listb=(enableDisable ? minus:plus)
				+ listb.substr(1);


		installForegroundTask(new smapSTORE(messages,
						    lista + " " + listb,
						    *this,
						    callback));
		return;
	}

	const char *plusminus=NULL;

	string name="";
	
#define DOFLAG(MARK, field, n)  if ( flags.field ) { plusminus=MARK; name += " " n; }
#define FLAG "+-"
#define NOTFLAG "-+"

	LIBMAIL_MSGFLAGS;

#undef FLAG
#undef NOTFLAG
#undef DOFLAG

	if (!plusminus || messages.size() == 0)
	{
		callback.success("Ok");
		return;
	}

	name=name.substr(1);

	char buf[2];

	buf[0]=plusminus[enableDisable ? 0:1];
	buf[1]=0;

	mail::imapMessageCallbackStub *c=
		new mail::imapMessageCallbackStub(callback);

	if (!c)
	{
		callback.fail("Out of memory.");
		return;
	}

	try {
		messagecmd(messages, string(" ")
			   + buf + "FLAGS (" + name + ")",
			   "UID STORE", "STORE", *c);
	} catch (...) {
		delete c;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

class mail::imap::updateImapKeywordHelper : public mail::callback {

	mail::callback *origCallback;
	mail::ptr<mail::imap> myimap;

public:
	set<string> newflags;
	set<string> uids;

	updateImapKeywordHelper(mail::callback *origCallbackArg,
				mail::imap *myimapArg);
	~updateImapKeywordHelper();

	void success(std::string message);
	void fail(std::string message);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);
};



void mail::imap::updateKeywords(const vector<size_t> &messages,
				const set<string> &keywords,
				bool setOrChange,
				// false: set, true: see changeTo
				bool changeTo,
				callback &cb)
{
	set<string>::const_iterator b=keywords.begin(), e=keywords.end();

	while (b != e)
	{
		string::const_iterator sb=b->begin(), se=b->end();

		while (sb != se)
		{
			if (strchr(smap ? ",\t\r\n,":"() \t\r\n[]{}\\\"", *sb))
			{
				cb.fail("Invalid flag");
				return;
			}
			++sb;
		}
		++b;
	}

	b=keywords.begin();
	e=keywords.end();

	if (smap)
	{
		string setCmd= setOrChange ? changeTo
			? "+KEYWORDS=":"-KEYWORDS=":"KEYWORDS=";

		const char *comma="";

		while (b != e)
		{
			setCmd += comma;
			setCmd += *b;
			++b;
			comma=",";
		}

		installForegroundTask(new smapSTORE(messages,
						    quoteSMAP(setCmd),
						    *this, cb));
		return;
	}

	if (!setOrChange)
	{
		updateImapKeywordHelper *h=
			new updateImapKeywordHelper(&cb, this);

		if (!h)
		{
			cb.fail(strerror(errno));
			return;
		}

		try {
			h->newflags=keywords;

			map<string, string> allFlags;
			// Keyed by upper(name), value=name
			// (be nice, and preserve casing when turning off
			// flags).

			vector<size_t>::const_iterator mb=messages.begin(),
				me=messages.end();

			while (mb != me)
			{
				size_t n= *mb;

				++mb;

				if (n >= currentFolder->index.size())
					continue;

				h->uids.insert(currentFolder->index[n].uid);
				set<string> tempSet;

				currentFolder->index[n].keywords
					.getFlags(tempSet);

				set<string>::iterator
					tsb=tempSet.begin(),
					tse=tempSet.end();

				while (tsb != tse)
				{
					string flagName= *tsb;

					// IMAP kws are case insensitive

					mail::upper(flagName);
					allFlags.insert(make_pair(flagName,
								  *tsb));
					++tsb;
				}
			}

			b=keywords.begin();
			e=keywords.end();

			while (b != e)
			{
				string flagName= *b;
				++b;

				mail::upper(flagName);
				allFlags.erase(flagName);
				// No need to remove flags that are going to be
				// set anyway.
			}

			if (allFlags.empty())
				h->success("Ok - no keywords changed.");
			// Nothing to turn off, so punt it.
			else
			{
				set<string> allFlagsV;

				map<string, string>::iterator b=
					allFlags.begin(), e=allFlags.end();

				while (b != e)
				{
					allFlagsV.insert(b->second);
					++b;
				}

				allFlags.clear();

				updateImapKeywords(messages, allFlagsV,
						   false, *h);
			}
		} catch (...) {
			delete h;
		}
		return;
	}

	updateImapKeywords(messages, keywords, changeTo, cb);
}

mail::imap::updateImapKeywordHelper
::updateImapKeywordHelper(mail::callback *origCallbackArg,
			  mail::imap *myimapArg)
	: origCallback(origCallbackArg), myimap(myimapArg)
{
}

mail::imap::updateImapKeywordHelper::~updateImapKeywordHelper()
{
	if (origCallback)
		origCallback->fail("Aborted.");
}

void mail::imap::updateImapKeywordHelper::success(std::string message)
{
	vector<size_t> msgSet;

	mail::imapFOLDERinfo *f=myimap.isDestroyed() ? NULL:
		myimap->currentFolder;

	size_t n=f ? f->index.size():0;
	size_t i;

	for (i=0; i<n; i++)
		if (uids.count(f->index[i].uid) > 0)
			msgSet.push_back(i);

	try {
		myimap->updateImapKeywords(msgSet, newflags, true,
					   *origCallback);
		origCallback=NULL;
		delete this;
	} catch (...) {
		delete this;
	}
}

void mail::imap::updateImapKeywordHelper::fail(std::string message)
{
	mail::callback *c=origCallback;
	origCallback=NULL;
	delete this;

	c->fail(message);
}

void mail::imap::updateImapKeywordHelper
::reportProgress(size_t bytesCompleted,
		 size_t bytesEstimatedTotal,
		 size_t messagesCompleted,
		 size_t messagesEstimatedTotal)
{
}

void mail::imap::updateImapKeywords(const vector<size_t> &messages,
				    const set<string> &keywords,
				    bool changeTo,
				    callback &cb)
{
	set<string>::const_iterator b=keywords.begin(), e=keywords.end();
	string setCmd= changeTo	? " +FLAGS (":" -FLAGS (";
	const char *space="";

	while (b != e)
	{
		string flagname= *b;

		if (!keywordAllowed(flagname))
		{
			vector<size_t>::const_iterator mb, me;

			mb=messages.begin();
			me=messages.end();

			while (mb != me)
			{
				size_t n= *mb;
				++mb;

				if (n >= (currentFolder ?
					  currentFolder->index.size():0))
					continue;

				if (changeTo)
				{
					if (!currentFolder->index[n]
					    .keywords.addFlag(keywordHashtable,
							      flagname))
					{
						LIBMAIL_THROW(strerror(errno));
					}
				}
				else
				{
					if (!currentFolder->index[n].keywords
					    .remFlag(flagname))
						LIBMAIL_THROW(strerror(errno));
				}
			}
			++b;
			continue;
		}

		setCmd += space;
		setCmd += *b;
		++b;
		space=" ";
	}

	if (*space == 0)
	{
		MONITOR(mail::imap);

		vector<size_t>::const_iterator mb, me;

		mb=messages.begin();
		me=messages.end();

		while (!DESTROYED() && mb != me)
		{
			size_t n= *--me;

			if (n >= (currentFolder ?
				  currentFolder->index.size():0))
				continue;

			currentFolder->folderCallback.messageChanged(n);
		}

		cb.success("Ok.");
		return;
	}


	mail::imapMessageCallbackStub *c=
		new mail::imapMessageCallbackStub(cb);

	if (!c)
	{
		cb.fail("Out of memory.");
		return;
	}

	try {
		messagecmd(messages, setCmd + ")",
			   "UID STORE", "STORE", *c);
	} catch (...) {
		delete c;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

///////////////////////////////////////////////////////////////////////////


void mail::imap::copyMessagesTo(const vector<size_t> &messages,
				mail::folder *copyTo,
				mail::callback &callback)
{
	sameServerFolderPtr=NULL;

	copyTo->sameServerAsHelperFunc();

	if (!sameServerFolderPtr)
	{
		// Copy to some other server, use a generic function.

		mail::copyMessages::copy(this, messages, copyTo, callback);
		return;
	}

	if (smap)
	{
		installForegroundTask(new smapCOPY(messages, copyTo,
						   *this,
						   callback, "COPY"));
		return;
	}

	mail::imapMessageCallbackStub *c=
		new mail::imapMessageCallbackStub(callback);

	if (!c)
	{
		callback.fail("Out of memory.");
		return;
	}

	try {
		messagecmd(messages, " " + quoteSimple(copyTo->getPath()),
			   "UID COPY", "COPY", *c);
	} catch (...) {
		delete c;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

}

void mail::imap::moveMessagesTo(const vector<size_t> &messages,
				mail::folder *copyTo,
				mail::callback &callback)
{
	sameServerFolderPtr=NULL;

	copyTo->sameServerAsHelperFunc();

	if (smap && sameServerFolderPtr)
	{
		installForegroundTask(new smapCOPY(messages, copyTo,
						   *this,
						   callback, "MOVE"));
		return;
	}
	
	generic::genericMoveMessages(this, messages, copyTo, callback);
}

///////////////////////////////////////////////////////////////////////////
//
//  The search command.

LIBMAIL_START

class imapSEARCH : public imapCommandHandler {

	string searchCmd;
	bool hasStringArg;
	string stringArg;

	mail::searchCallback &callback;

	vector<size_t> found;

	class SearchResults : public imapHandlerStructured {
		vector<size_t> &found;
	public:
		SearchResults(vector<size_t> &);
		~SearchResults();

		void installed(imap &imapAccount);
	private:
		const char *getName();
		void timedOut(const char *errmsg);

		void process(imap &imapAccount, Token t);
	};

public:
	imapSEARCH(string searchCmdArg,
			 bool hasParam2,
			 string param2Arg,
			 mail::searchCallback &callbackArg);
	~imapSEARCH();

	void installed(imap &);
	const char *getName();
	void timedOut(const char *errmsg);

	bool untaggedMessage(imap &imapAccount, string name);
	bool taggedMessage(imap &imapAccount, string name,
			   string message,
			   bool okfail, string errmsg);
	bool continuationRequest(imap &imapAccount, string request);
};

LIBMAIL_END

mail::imapSEARCH::imapSEARCH(string searchCmdArg,
				   bool hasParam2,
				   string param2Arg,
				   mail::searchCallback &callbackArg)
	: searchCmd(searchCmdArg),
	  hasStringArg(hasParam2),
	  stringArg(param2Arg),
	  callback(callbackArg)
{
}

mail::imapSEARCH::~imapSEARCH()
{
}

void mail::imapSEARCH::installed(mail::imap &imapAccount)
{
	if (hasStringArg)
	{
		string::iterator b=stringArg.begin(), e=stringArg.end();

		while (b != e)
		{
			if (*b < ' ' || *b >= 127)
				break;
			b++;
		}

		if (b == e)
			searchCmd += mail::imap::quoteSimple(stringArg);
		else
		{
			ostringstream o;

			o << "{" << stringArg.size() << "}";

			searchCmd += o.str();
		}
	}
	imapAccount.imapcmd("SEARCH", "SEARCH " + searchCmd + "\r\n");
}

//
// High-8 search string must be sent via a literal.
//

bool mail::imapSEARCH::continuationRequest(mail::imap &imapAccount, string request)
{
	imapAccount.socketWrite(stringArg + "\r\n");
	return true;
}

const char *mail::imapSEARCH::getName()
{
	return "SEARCH";
}

void mail::imapSEARCH::timedOut(const char *errmsg)
{
	callback.fail(errmsg);
}

bool mail::imapSEARCH::untaggedMessage(mail::imap &imapAccount, string name)
{
	if (name == "SEARCH")
	{
		imapAccount.installBackgroundTask(new SearchResults(found));
		return true;
	}
	return false;
}

bool mail::imapSEARCH::taggedMessage(mail::imap &imapAccount, string name,
				     string message,
				     bool okfail, string errmsg)
{
	if (name != "SEARCH")
		return false;

	if (okfail)
		callback.success(found);
	else
		callback.fail(errmsg);

	imapAccount.uninstallHandler(this);
	return true;
}

mail::imapSEARCH::SearchResults::SearchResults(vector <size_t> &foundArg)
	: found(foundArg)
{
}

mail::imapSEARCH::SearchResults::~SearchResults()
{
}

void mail::imapSEARCH::SearchResults::installed(mail::imap &imapAccount)
{
}

const char *mail::imapSEARCH::SearchResults::getName()
{
	return "* SEARCH";
}

void mail::imapSEARCH::SearchResults::timedOut(const char *errmsg)
{
}

void mail::imapSEARCH::SearchResults::process(mail::imap &imapAccount, Token t)
{
	if (t == EOL)
		return;

	if (t != ATOM)
	{
		error(imapAccount);
		return;
	}

	size_t n=0;

	istringstream i(t.text.c_str());

	i >> n;

	if (n == 0 || imapAccount.currentFolder == NULL
	    || n > imapAccount.currentFolder->index.size())
	{
		error(imapAccount);
		return;
	}

	found.push_back(n-1);
}

void mail::imap::searchMessages(const class mail::searchParams &searchInfo,
				class mail::searchCallback &callback)
{
	if (smap)
	{
		installForegroundTask(new mail::smapSEARCH(searchInfo,
							   callback,
							   *this));
		return;
	}

	if (folderFlags.count("\\FLAGGED") == 0)
	{
		// Server doesn't implement \Flagged, do everything by hand

		mail::searchMessages::search(callback, searchInfo, this);
		return;
	}

	string cmd="";

	if (searchInfo.charset.size() > 0)
		cmd="CHARSET " + quoteSimple(searchInfo.charset) + " ";

	switch (searchInfo.scope) {

	case searchParams::search_all:
		cmd += "ALL";
		break;

	case searchParams::search_unmarked:
		cmd += "ALL NOT FLAGGED";
		break;

	case searchParams::search_marked:
		cmd += "ALL FLAGGED";
		break;
	case searchParams::search_range:

		{
			ostringstream o;

			o << searchInfo.rangeLo+1 << ':'
			  << searchInfo.rangeHi;

			cmd += o.str();
		}
		break;
	}

	bool notFlag=searchInfo.searchNot;

	if (searchInfo.criteria == searchInfo.unread)
		notFlag= !notFlag;

	if (notFlag)
		cmd += " NOT";

	string sizeNumStr="";

	switch (searchInfo.criteria) {
	case searchParams::larger:
	case searchParams::smaller:

		{
			unsigned long sizeNum=0;

			istringstream i(searchInfo.param2.c_str());

			i >> sizeNum;

			if (i.fail())
			{
				callback.fail("Invalid message size string.");
				return;
			}

			string buffer;

			{
				ostringstream o;

				o << sizeNum;
				buffer=o.str();
			}

			sizeNumStr=buffer;
		}
		break;
	default:
		break;
	}

	string param2="";
	bool hasParam2=false;

	string::const_iterator b, e;

	switch (searchInfo.criteria) {
	case searchParams::replied:
		cmd += " ANSWERED";
		break;
	case searchParams::deleted:
		cmd += " DELETED";
		break;
	case searchParams::draft:
		cmd += " DRAFT";
		break;
	case searchParams::unread:
		cmd += " SEEN";
		break;

		// Header match
		
	case searchParams::from:
		cmd += " FROM "; hasParam2=true; param2=searchInfo.param2;
		break;
	case searchParams::to:
		cmd += " TO "; hasParam2=true; param2=searchInfo.param2;
		break;
	case searchParams::cc:
		cmd += " CC "; hasParam2=true; param2=searchInfo.param2;
		break;
	case searchParams::bcc:
		cmd += " BCC "; hasParam2=true; param2=searchInfo.param2;
		break;
	case searchParams::subject:
		cmd += " SUBJECT "; hasParam2=true; param2=searchInfo.param2;
		break;

	case searchParams::header:

		b=searchInfo.param1.begin();
		e=searchInfo.param1.end();

		while (b != e)
		{
			if (*b < ' '|| *b >= 127)
			{
				callback.fail("Invalid header name.");
				return;
			}
		}

		cmd += " HEADER " + quoteSimple(searchInfo.param1)
			+ " "; hasParam2=true; param2=searchInfo.param2;
		break;
	case searchParams::body:
		cmd += " BODY "; hasParam2=true; param2=searchInfo.param2;
		break;
	case searchParams::text:
		cmd += " TEXT "; hasParam2=true; param2=searchInfo.param2;
		break;

		// Internal date:

	case searchParams::before:
		cmd += " BEFORE "; hasParam2=true; param2=searchInfo.param2;
		break;
	case searchParams::on:    
		cmd += " ON "; hasParam2=true; param2=searchInfo.param2;
		break;
	case searchParams::since:
		cmd += " SINCE "; hasParam2=true; param2=searchInfo.param2;
		break;

		// Sent date:

	case searchParams::sentbefore:
		cmd += " SENTBEFORE "; hasParam2=true; param2=searchInfo.param2;
		break;
	case searchParams::senton:
		cmd += " SENTON "; hasParam2=true; param2=searchInfo.param2;
		break;
	case searchParams::sentsince:
		cmd += " SENTSINCE "; hasParam2=true; param2=searchInfo.param2;
		break;

	case searchParams::larger:
		cmd += " LARGER " + sizeNumStr;
		break;
	case searchParams::smaller:
		cmd += " SMALLER " + sizeNumStr;
		break;
	default:
		callback.fail("Unknown search criteria.");
		return;
	}

	installForegroundTask(new mail::imapSEARCH(cmd, hasParam2, param2,
						   callback));
}


///////////////////////////////////////////////////////////////////////////
//
// Convert flags in mail::messageInfo to IMAP flag names.
// For those flags that are not supported on the server, update the
// flags in the folder index manually, and set the flagsUpdated flag.

string mail::imap::messageFlagStr(const mail::messageInfo &indexInfo,
				  mail::messageInfo *oldIndexInfo,
				  bool *flagsUpdated)
{
	string flags="";

	if (flagsUpdated)
		*flagsUpdated=false;

#define DOFLAG(NOT, field, name) \
	if (oldIndexInfo == NULL /* APPEND assumes all flags are present */ ||\
			folderFlags.count( name ) > 0)\
	{\
		if (NOT indexInfo.field)\
			flags += " " name;\
	}\
	else if ( oldIndexInfo->field != indexInfo.field)\
	{\
		oldIndexInfo->field=indexInfo.field;\
		*flagsUpdated=true;\
	}

#define FLAG
#define NOTFLAG !

	LIBMAIL_MSGFLAGS;

#undef FLAG
#undef DOFLAG
#undef NOTFLAG


	if (flags.size() > 0)
		flags=flags.substr(1);

	return flags;
}

////////////////////////////////////////////////////////////////////////////
//
// The IMAP EXPUNGE command.

LIBMAIL_START

class imapEXPUNGE : public imapCommandHandler {

	mail::callback &callback;

public:
	imapEXPUNGE(mail::callback &callbackArg);
	~imapEXPUNGE();

	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *errmsg);

	bool untaggedMessage(imap &imapAccount, string name);
	bool taggedMessage(imap &imapAccount, string name,
			   string message,
			   bool okfail, string errmsg);
};

LIBMAIL_END

mail::imapEXPUNGE::imapEXPUNGE(mail::callback &callbackArg)
	: callback(callbackArg)
{
}

void mail::imapEXPUNGE::timedOut(const char *errmsg)
{
	callbackTimedOut(callback, errmsg);
}

mail::imapEXPUNGE::~imapEXPUNGE()
{
}

void mail::imapEXPUNGE::installed(mail::imap &imapAccount)
{
	imapAccount.imapcmd("EXPUNGE", "EXPUNGE\r\n");
}

const char *mail::imapEXPUNGE::getName()
{
	return "EXPUNGE";
}

bool mail::imapEXPUNGE::untaggedMessage(mail::imap &imapAccount, string name)
{
	return false;
}

bool mail::imapEXPUNGE::taggedMessage(mail::imap &imapAccount, string name,
				      string message,
				      bool okfail, string errmsg)
{
	if (name == "EXPUNGE")
	{
		if (okfail)
			callback.success(errmsg);
		else
			callback.fail(errmsg);
		imapAccount.uninstallHandler(this);
		return true;
	}
	return false;
}

void mail::imap::updateNotify(bool enableDisable, callback &callbackArg)
{
	if (!ready(callbackArg))
		return;

	wantIdleMode=enableDisable;
	updateNotify(&callbackArg);
}

void mail::imap::updateNotify(callback *callbackArg)
{
	if (!smap && (!hasCapability("IDLE") || noIdle))
	{
		if (callbackArg)
			callbackArg->success("Ok");
		return;
	}

	installForegroundTask(smap ?
			      (imapHandler *)new smapIdleHandler(wantIdleMode,
								 callbackArg):
			      new imapIdleHandler(wantIdleMode, callbackArg));
}

void mail::imap::updateFolderIndexInfo(mail::callback &callback)
{
	if (!ready(callback))
		return;

	installForegroundTask(smap ?
			      (imapHandler *)new smapNoopExpunge("EXPUNGE",
								 callback,
								 *this)
			      : new mail::imapEXPUNGE(callback));
}

//////////////////////////////////////////////////////////////////////////
//
// UID EXPUNGE

////////////////////////////////////////////////////////////////////////////
//
// The IMAP EXPUNGE command.

LIBMAIL_START

class imapUIDEXPUNGE : public mail::callback::message {

	set<string> uids;

	ptr<mail::imap> acct;
	mail::callback &callback;

	bool uidSent;

public:

	imapUIDEXPUNGE(mail::imap &imapAccount,
		       mail::callback &callbackArg,
		       const vector<size_t> &msgs);
	~imapUIDEXPUNGE();

	void mkVector(mail::imap &, vector<size_t> &);

private:
	void success(string msg);
	void fail(string msg);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

};

LIBMAIL_END

mail::imapUIDEXPUNGE::imapUIDEXPUNGE(mail::imap &imapAccount,
				     mail::callback &callbackArg,
				     const vector<size_t> &msgs)
	: acct(&imapAccount), callback(callbackArg), uidSent(false)
{
	vector<size_t>::const_iterator b, e;

	for (b=msgs.begin(), e=msgs.end(); b != e; b++)
	{
		if (imapAccount.currentFolder &&
		    *b < imapAccount.currentFolder->index.size())
			uids.insert(imapAccount.currentFolder
				    ->index[*b].uid);
	}
}

mail::imapUIDEXPUNGE::~imapUIDEXPUNGE()
{
}

void mail::imapUIDEXPUNGE::mkVector(mail::imap &imapAccount,
				    vector<size_t> &vec)
{
	if (imapAccount.currentFolder)
	{
		size_t i;

		for (i=0; i<imapAccount.currentFolder->exists; i++)
		{
			if (uids.count(imapAccount.currentFolder
				       ->index[i].uid) > 0)
				vec.push_back(i);
		}
	}
}

void mail::imapUIDEXPUNGE::success(string dummy)
{
	if (acct.isDestroyed() || uidSent)
	{
		callback.success(dummy);
		delete this;
		return;
	}

	vector<size_t> vec;

	mkVector(*acct, vec);

	if (vec.size() == 0)
	{
		mail::callback * volatile c= &callback;

		delete this;

		c->fail(dummy);
		return;
	}

	uidSent=true;
	acct->messagecmd(vec, "", "UID EXPUNGE", "EXPUNGE", *this);
}

void mail::imapUIDEXPUNGE::reportProgress(size_t bytesCompleted,
					  size_t bytesEstimatedTotal,

					  size_t messagesCompleted,
					  size_t messagesEstimatedTotal)
{
}

void mail::imapUIDEXPUNGE::fail(string dummy)
{
	mail::callback * volatile c= &callback;

	delete this;

	c->fail(dummy);
}

void mail::imap::removeMessages(const std::vector<size_t> &messages,
				callback &cb)
{
	if (!ready(cb))
		return;

	if (smap)
	{
		installForegroundTask(new smapNoopExpunge(messages, cb,
							  *this));
		return;
	}

	if (!hasCapability("UIDPLUS"))
	{
		mail::generic::genericRemoveMessages(this, messages, cb);
		return;
	}

	imapUIDEXPUNGE *exp=new imapUIDEXPUNGE(*this, cb, messages);

	try {
		vector<size_t> msgs;

		exp->mkVector(*this, msgs);

		if (msgs.size() == 0)
		{
			delete exp;
			exp=NULL;

			cb.success("OK");
			return;
		}

		messagecmd(msgs, " +FLAGS.SILENT (\\Deleted)",
			   "UID STORE", "STORE", *exp);
	} catch (...) {
		if (exp)
			delete exp;

		cb.fail("An exception occured in removeMessages");
		return;
	}
}

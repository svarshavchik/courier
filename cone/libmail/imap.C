/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "imap.H"
#include "driver.H"
#include "smap.H"
#include "smapfetch.H"
#include "smapfetchattr.H"
#include "imapfolder.H"
#include "imaplogin.H"
#include "logininfo.H"
#include "imapfetchhandler.H"
#include "base64.H"
#include "structure.H"
#include "misc.H"
#include "genericdecode.H"

#include <unicode/unicode.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

//#define DEBUG 1

using namespace std;

/////////////////////////////////////////////////////////////////////////////

LIBMAIL_START

static bool open_imap(mail::account *&accountRet,
		      mail::account::openInfo &oi,
		      mail::callback &callback,
		      mail::callback::disconnect &disconnectCallback)
{
	mail::loginInfo imapLoginInfo;

	if (!mail::loginUrlDecode(oi.url, imapLoginInfo))
		return false;

	if (imapLoginInfo.method != "imap" &&
	    imapLoginInfo.method != "imaps")
		return false;

	accountRet=new mail::imap(oi.url, oi.pwd,
				  oi.certificates,
				  oi.loginCallbackObj,
				  callback,
				  disconnectCallback);
	return true;
}


static bool imap_remote(string url, bool &flag)
{
	mail::loginInfo nntpLoginInfo;

	if (!mail::loginUrlDecode(url, nntpLoginInfo))
		return false;

	if (nntpLoginInfo.method != "imap" &&
	    nntpLoginInfo.method != "imaps")
		return false;

	flag=true;
	return true;
}

driver imap_driver= { &open_imap, &imap_remote };

LIBMAIL_END

/////////////////////////////////////////////////////////////////////////////

mail::imapHandler::imapHandler( int timeoutValArg)
	: myimap(NULL), installedFlag(false), isBackgroundTask(false),
	  handlerTimeoutVal(timeoutValArg)
{
	time(&timeoutAt);
}

// Default message handler timeout function invokes the application
// callback.

void mail::imapHandler::callbackTimedOut(mail::callback &callback,
					 const char *errmsg)
{
	callback.fail(errmsg ? errmsg:"Server timed out.");
}

mail::imapHandler::~imapHandler()
{
	if (installedFlag)  // If I'm in the active handler list, take me out
		myimap->remove(this);
}

void mail::imapHandler::anotherHandlerInstalled(mail::imap &imapAccount)
{
}

bool mail::imapHandler::getTimeout(mail::imap &imapAccount, int &ioTimeout)
{
	ioTimeout=getTimeout(imapAccount) * 1000;

	return (ioTimeout == 0);
}

int mail::imapHandler::getTimeout(mail::imap &imapAccount)
{
	time_t t;

	time(&t);

	if (handlerTimeoutVal == 0)
	{
		handlerTimeoutVal=imapAccount.timeoutSetting;
		timeoutAt=t + handlerTimeoutVal;
	}

	return timeoutAt > t ? timeoutAt - t:0;
}

void mail::imapHandler::setTimeout(int t)
{
	handlerTimeoutVal=t;
	setTimeout();
}

void mail::imapHandler::setTimeout()
{
	if (handlerTimeoutVal == 0 && myimap)
		handlerTimeoutVal=myimap->timeoutSetting;

	time(&timeoutAt);
	timeoutAt += handlerTimeoutVal;
}

/////////////////////////////////////////////////////////////////////////
//
// mail::imap constructor
//


mail::imap::imap(string url,
		 string passwd,
		 std::vector<std::string> &certificates,
		 mail::loginCallback *callback_func,
		 mail::callback &callback,
		 mail::callback::disconnect &disconnectCallback)
	: mail::fd(disconnectCallback, certificates),
	  orderlyShutdown(false), sameServerFolderPtr(NULL),
	  smap(false),
	  smapProtocolHandlerFunc(&smapHandler::singleLineProcess),
	  smapBinaryCount(0)
{
	cmdcounter=0;
	currentFolder=NULL;
	currentFetch=NULL;
	wantIdleMode=false;

	mail::loginInfo loginInfo;

	loginInfo.loginCallbackFunc=callback_func;
	loginInfo.callbackPtr= &callback;

	if (!loginUrlDecode(url, loginInfo))
	{
		loginInfo.callbackPtr->fail("Invalid IMAP URL.");
		return;
	}


	timeoutSetting=getTimeoutSetting(loginInfo, "timeout", 60,
					 30, 600);
	noopSetting=getTimeoutSetting(loginInfo, "noop", 600,
				      5, 1800);

	noIdle=loginInfo.options.count("noidle") != 0;
	loginInfo.use_ssl= loginInfo.method == "imaps";

	if (passwd.size() > 0)
		loginInfo.pwd=passwd;


	string errmsg=socketConnect(loginInfo, "imap", "imaps");

	if (errmsg.size() > 0)
	{
		loginInfo.callbackPtr->fail(errmsg);
		return;
	}

	// Expect a server greeting as the first order of business.

	installBackgroundTask(new mail::imapGreetingHandler(loginInfo));
}

mail::imap::~imap()
{
	removeAllHandlers(false, NULL);

	disconnect();
}

// Verbotten characters

#if 0
void mail::imap::mkverbotten(std::vector<unicode_char> &verbotten)
{
	// Some chars should be modutf7-
	// encoded, just to be on the safe
	// side.

	verbotten.push_back('/');
	verbotten.push_back('\\');
	verbotten.push_back('%');
	verbotten.push_back('*');
	verbotten.push_back(':');
	verbotten.push_back('~');
	verbotten.push_back(0);
}
#endif

//
// Prefix an increasing counter to an imap tag, and send the entire command
//

void mail::imap::imapcmd(string cmd, string arg)
{
	if (cmd.size() == 0)
	{
		socketWrite(arg);
		return;
	}

	if (smap)
	{
		socketWrite(cmd + " " + arg);
		return;
	}

	ostringstream o;

	cmdcounter=(cmdcounter + 1) % 1000000;
	o << setw(6) << setfill('0') << cmdcounter
	  << cmd << " " << arg;

	socketWrite(o.str());
}

///////////////////////////////////////////////////////////////////////////
//
// Install a new handler to process replies from the IMAP server
//
// If there's already an active foreground task handler, push the handler
// onto the task queue.

void mail::imap::installForegroundTask(mail::imapHandler *handler)
{
	imapHandler *foregroundTask;

	handler->setBackgroundTask(false);

	if ((foregroundTask=hasForegroundTask()) != NULL)
	{
		if (!task_queue.empty())
			foregroundTask=NULL;

		task_queue.push(handler);

		if (foregroundTask)
			foregroundTask->anotherHandlerInstalled(*this);
		return;
	}

	insertHandler(handler);
}

mail::imapHandler *mail::imap::hasForegroundTask()
{
	handlerMapType::iterator b=handlers.begin(), e=handlers.end();
	imapHandler *h;

	while (b != e)
		if (!(h=(*b++).second)->getBackgroundTask())
			return h;
	return NULL;
}

// Unconditionally install a new message handler.
// (any existing message handler with the same name is removed).

void mail::imap::insertHandler(mail::imapHandler *handler)
{
	if (!handler)
		LIBMAIL_THROW("mail::imap:: out of memory.");

	const char *name=handler->getName();
				
	if (handlers.count(name) > 0)
	{
		mail::imapHandler *oldHandler=
			&*handlers.find(name)->second;

		uninstallHandler(oldHandler);
	}

	handlers.insert(make_pair(name, handler));
	handler->installedFlag=true;
	handler->myimap=this;
	handler->setTimeout();
	handler->installed( *this ); // Tell the handler the show's on.
}

// Install a new message handler, and mark it as a background handler,
// so that foreground message handlers can still be installed.
// (any existing message handler with the same name is removed).

void mail::imap::installBackgroundTask(mail::imapHandler *handler)
{
	handler->setBackgroundTask(true);
	insertHandler(handler);
	current_handler=handler;
}

// Unconditionally remove a handler.  The handler MUST be currently installed
// as active (it cannot be on the task queue

void mail::imap::uninstallHandler(mail::imapHandler *handler)
{
	if (!handler->installedFlag)
	{
		delete handler;
		return;
	}

	remove(handler);
	delete handler;
}

// Remove an active handler.

void mail::imap::remove(mail::imapHandler *handler)
{
	const char *name=handler->getName();

	if (current_handler != NULL &&
	    strcmp(current_handler->getName(), handler->getName()) == 0)
		current_handler=NULL; // Removed the current handler.

	handlers.erase(name);
	handler->installedFlag=false;

	// If we just popped off a foreground task, and there are other
	// foreground tasks waiting, install the next foreground task.

	if (!handler->getBackgroundTask())
	{
		if (!task_queue.empty())
		{
			mail::imapHandler *nextTask=task_queue.front();
			task_queue.pop();

			insertHandler(nextTask);
		}
		else if (wantIdleMode)
		{
			updateNotify(NULL);
		}
	}
}

void mail::imap::fatalError(string errmsg)
{
	disconnect_callback.servererror(errmsg.c_str());
}

bool mail::imap::ready(mail::callback &callback)
{
	if (socketConnected())
		return true;

	callback.fail("Account connection closed.");
	return false;
}

void mail::imap::resumed()
{
	handlerMapType::iterator bb=handlers.begin(),
		ee=handlers.end();

	while (bb != ee)
	{
		if (strcasecmp(bb->first, mail::imapFOLDER::name) &&
		    strcasecmp(bb->first, mail::smapFOLDER::name))
			bb->second->setTimeout();
		++bb;
	}
}

///////////////////////////////////////////////////////////////////////////
//
// Main event dispatcher.  Read any response received from the IMAP server,
// and process it.  Write any pending output to the imap server
//
// r, w: initialized to indicate any I/O status that must occur before
// process() should be called again.  r, w should be used as arguments to
// select(2) when process() returns.

void mail::imap::handler(vector<pollfd> &fds, int &ioTimeout)
{
	bool timedoutFlag=false;
	int fd_save=getfd();

	handlerMapType::iterator bb=handlers.begin(),
		ee=handlers.end();

	writtenFlag=false;

	MONITOR(mail::imap);

	while (bb != ee)
	{
		int t;

		if (bb->second->getTimeout(*this, t))
			timedoutFlag=true;

		if (t < ioTimeout)
			ioTimeout=t;
		bb++;
	}

	if (timedoutFlag)
	{
		timeoutdisconnect();
		return;
	}

	if (mail::fd::process(fds, ioTimeout) < 0)
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
		return;
	}

	if (writtenFlag)
		ioTimeout=0;

	return;
}

//
// Figure out where a response from the server should go.

int mail::imap::socketRead(const string &readbuffer)
{
	MONITOR(mail::imap);

	string buffer_cpy=readbuffer;

	// Try whatever handler accepted previous folder output.

	if (current_handler != NULL)
	{
#if DEBUG
		cerr << "Handler: " << current_handler->getName()
		     << endl;
#endif
		int n;

		n=current_handler->process(*this, buffer_cpy);


#if DEBUG
		cerr << "  Processed " << n << " bytes." << endl;
#endif
		if (DESTROYED())
			return 0;

		if (n > 0)
			return n;  // The handler took the input
	}

	// If the last handler didn't want it, step through the active
	// handlers, until someone picks it up.

	int processed=0;

	handlerMapType::iterator bb=handlers.begin(),
		ee=handlers.end();

	while (bb != ee)
	{
#if DEBUG
		cerr << "Trying " << bb->second->getName() << " ... "
		     << endl;

		if (strcmp(bb->second->getName(), "SELECT") == 0)
		{
			printf("OK\n");
		}
#endif
		current_handler=bb->second;

		processed=bb->second->process(*this, buffer_cpy);

		if (DESTROYED())
			return 0;

		if (processed > 0)
		{
#if DEBUG
			cerr << "   ... processed " << processed
			     << " of " << readbuffer.size()
			     << " bytes." << endl;
#endif
			break;
		}
		current_handler=NULL;
		bb++;
	}

	if (processed > 0)
		return processed;

	size_t pos=buffer_cpy.find('\n');

	if (pos == std::string::npos)
	{
		if (buffer_cpy.size() > 16000)
			return buffer_cpy.size() - 16000;
		// SANITY CHECK - DON'T LET HOSTILE SERVER DOS us

		return 0;  // Read more
	}

	// Ok, there are some server responses we know how to do ourselves.

	buffer_cpy.erase(pos);

	size_t processedCnt=pos+1;


	while ((pos=buffer_cpy.find('\r')) != std::string::npos)
		buffer_cpy.erase(buffer_cpy.begin() + pos,
				 buffer_cpy.begin() + pos + 1);

	string::iterator mb=buffer_cpy.begin(), me=buffer_cpy.end();

	if ( mb != me && *mb == '*' )
	{
		++mb;

		string::iterator save_mb=mb;

		string msg=get_word(&mb, &me);

		upper(msg);

		if (msg == "CAPABILITY")
		{
			clearCapabilities();
			string status;

			while ((status=get_word(&mb, &me)).length() > 0)
				setCapability(status);
			return processedCnt;
		}

		if (msg == "OK")
		{
			size_t p=buffer_cpy.find('[');
			size_t q=buffer_cpy.find(']');
			string brk, brkw;
			string::iterator bb, be;

			if (p != std::string::npos && q != std::string::npos &&
			    q > p && ((brk=string(buffer_cpy.begin()+p,
						  buffer_cpy.begin()+q)),
				      bb=brk.begin(),
				      be=brk.end(),
				      brkw=get_word(&bb, &be), upper(brkw),
				      brkw) != "ALERT")
			{
				if (brkw == "PERMANENTFLAGS")
					setPermanentFlags(bb, be);

				return processedCnt;
			}
		}

		if (msg == "NO" || msg == "OK")
		{
			if (serverMsgs.size() < 15)
			{
				if (msg == "NO")
					mb=save_mb;

				serverMsgs.push_back(std::string(mb, me));
			}

			return processedCnt;
		}

		if (msg != "BYE")
			mb=save_mb;

		while (mb != me && unicode_isspace((unsigned char)*mb))
			++mb;

		buffer_cpy.erase(buffer_cpy.begin(), mb);
	}

	fatalError(buffer_cpy);

	if (DESTROYED())
		return 0;

	return processedCnt;
}

void mail::imap::setPermanentFlags(string::iterator flagb,
				   string::iterator flage)
{
	string w;

	permanentFlags.clear();

	while (flagb != flage && unicode_isspace((unsigned char)*flagb))
		++flagb;

	if (flagb == flage)
		return;

	if (*flagb != '(')
	{
		w=get_word(&flagb, &flage);
		upper(w);
		if (w != "NIL")
			permanentFlags.insert(w);
		return;
	}
	++flagb;

	while ((w=get_word(&flagb, &flage)).size() > 0)
	{
		upper(w);

		permanentFlags.insert(w);
	}
}

void mail::imap::timeoutdisconnect()
{
	const char *errmsg="Server connection closed due to a timeout.";

	removeAllHandlers(true, errmsg);
	disconnect(errmsg);
}

void mail::imap::disconnect(const char *errmsg)
{
	string errmsg_cpy=errmsg ? errmsg:"";

	if (getfd() >= 0)
	{
		string errmsg2=socketDisconnect();

		if (errmsg2.size() > 0)
			errmsg_cpy=errmsg2;

		removeAllHandlers(true, errmsg_cpy.size() == 0 ?
				  "Connection closed by remote host."
				  :errmsg_cpy.c_str());


		if (orderlyShutdown)
			errmsg_cpy="";
		else if (errmsg_cpy.size() == 0)
			errmsg_cpy="Connection closed by remote host.";

		disconnect_callback.disconnected(errmsg_cpy.c_str());
	}
	else
	{
		removeAllHandlers(true, errmsg_cpy.size() == 0 ?
				  "Connection closed by remote host.":errmsg);
	}
}

void mail::imap::setCapability(string status)
{
	upper(status);
	capabilities.insert(status);
}

bool mail::imap::hasCapability(string status)
{
	upper(status);

	if (status == LIBMAIL_SINGLEFOLDER)
		return false;

	if (status == "ACL") // Special logic
	{
		if (capabilities.count("ACL") > 0)
			return true;

		set<string>::iterator p;

		for (p=capabilities.begin(); p != capabilities.end(); ++p)
		{
			string s= *p;
			upper(s);
			if (strncmp(s.c_str(), "ACL2=", 5) == 0)
				return true;
		}
		return false;
	}

	return (capabilities.count(status) > 0);
}

string mail::imap::getCapability(string name)
{
	upper(name);

	if (name == "ACL") // Special logic
	{
		set<string>::iterator p;

		for (p=capabilities.begin(); p != capabilities.end(); ++p)
		{
			string s= *p;
			upper(s);
			if (strncmp(s.c_str(), "ACL2=", 5) == 0)
				return s;
		}
		if (capabilities.count("ACL") > 0)
			return name;
		return "";
	}

	if (name == LIBMAIL_SERVERTYPE)
	{
		return servertype.size() == 0 ? "imap":servertype;
	}

	if (name == LIBMAIL_SERVERDESCR)
	{
		return serverdescr.size() == 0 ? "IMAP server":serverdescr;
	}

	return (capabilities.count(name) > 0 ? "1":"");
}

// Utility functions...

string mail::imap::get_word(string::iterator *b, string::iterator *e)
{
	string s="";

	while (*b != *e && unicode_isspace((unsigned char)**b))
		++*b;

	while (*b != *e && !unicode_isspace((unsigned char)**b) &&
	       strchr(")]", **b) == NULL)
	{
		s.append(&**b, 1);
		++*b;
	}
	return s;
}

// IMAP tags have are prefixed with a numerical counter.  Throw it away

string mail::imap::get_cmdreply(string::iterator *b, string::iterator *e)
{
	while (*b != *e && isdigit((int)(unsigned char)**b))
		++*b;
	return (get_word(b, e));
}


string mail::imap::quoteSimple(string s)
{
	string::iterator b, e;

#if 0
	b=s.begin();
	e=s.end();

	while (b != e)
	{
		if (*b >= 127 || *b < ' ')
		{
			string buffer;

			{
				ostringstream o;

				o << "{" << s.size() << "}\r\n";
				buffer=o.str();
			}

			return buffer + s;
		}
		b++;
	}
#endif

	string t="\"";

	b=s.begin();
	e=s.end();

	while (b != e)
	{
		if (*b == '"' || *b == '\\')
			t.append("\\");
		t.append(&*b, 1);
		b++;
	}
	t.append("\"");
	return t;
}

string mail::imap::quoteSMAP(string s)
{
	string::iterator b, e;

	string t="\"";

	b=s.begin();
	e=s.end();

	while (b != e)
	{
		if (*b == '"')
			t.append("\"");
		t.append(&*b, 1);
		b++;
	}
	t.append("\"");
	return t;
}

//
// Remove all handlers.  Possible reasons: destructor, connection timeout,
// orderly shutdown.
//

void mail::imap::removeAllHandlers(bool timedOutFlag, // true if conn timeout
				   const char *errmsg)
{
	while (!handlers.empty())
	{
		mail::imapHandler *handler=handlers.begin()->second;

		if (timedOutFlag)
			handler->timedOut(errmsg);

		handlers.erase(handlers.begin()->first);
		handler->installedFlag=false;
		delete handler;
	}

	while (!task_queue.empty())
	{
		mail::imapHandler *p=task_queue.front();

		task_queue.pop();

		if (timedOutFlag)
			p->timedOut(errmsg);
		delete p;
	}
}

mail::imap::msgSet::msgSet()
{
}

mail::imap::msgSet::~msgSet()
{
}

// Convert an array of message numbers to imap-style msg set

mail::imap::msgSetRange::msgSetRange(mail::imap *pArg,
				     const vector<size_t> &messages)
	: p(pArg)
{
	msglist=messages;

	sort(msglist.begin(), msglist.end());
	nextMsg=msglist.begin();
}

mail::imap::msgSetRange::~msgSetRange()
{
}

size_t mail::imap::msgSetRange::uidNum(mail::imap *i, size_t j)
{
	if (i->currentFolder == NULL || j >= i->currentFolder->index.size())
		return 0;

	// mail::imapFolder constructs the abstract libmail.a uid as
	// UIDVALIDITY/UID, so find the UID part of it.

	string uid=i->currentFolder->index[j].uid;

	size_t n=uid.find('/');

	if (n == string::npos)
		return 0;

	uid=uid.substr(n+1);

	n=0;

	istringstream(uid.c_str()) >> n;

	return n;
}

bool mail::imap::msgSetRange::getNextRange(size_t &first, size_t &last)
{
	do
	{
		if (nextMsg == msglist.end())
			return false;
	} while ((first=last= uidNum(p, *nextMsg++)) == 0);


	while (nextMsg != msglist.end() && last + 1 == uidNum(p, *nextMsg))
		last = uidNum(p, *nextMsg++);

	return true;
}

void mail::imap::messagecmd(const vector<size_t> &messages, string parameters,
			    string operation,
			    string name,
			    mail::callback::message &callback)
{
	msgSetRange setRange(this, messages);

	messagecmd(setRange, parameters, operation, name, callback);
}

void mail::imap::messagecmd(msgSet &msgRange, string parameters,
			    string operation,
			    string name,
			    mail::callback::message &callback)
{
	if (!ready(callback))
		return;

	// Prevent excessively huge IMAP commands that result from large
	// message sets.  The message set is capped at about 100 characters,
	// and multiple commands are generated, if this is not enough.
	//
	// mail::imapFetchHandler is told how many commands went out, so
	// it knows to count the number of server replies.

	mail::imapFetchHandler *cmd=new mail::imapFetchHandler(callback, name);

	size_t first, last;

	string s="";

	try {
		if (!cmd)
			LIBMAIL_THROW(strerror(errno));

		while (msgRange.getNextRange(first, last))
		{
			cmd->messageCntTotal += last + 1 - first;

			if (s.length() > 0)
			{
				if (s.length() > 100)
				{
					cmd->commands
						.push( make_pair(name,
								 operation
								 + " " + s
								 + parameters
								 + "\r\n"));
					++cmd->counter;
					s="";
				}
				else
					s += ",";
			}

			string buffer;

			if (first != last)
			{
				ostringstream o;

				o << first << ":" << last;
				buffer=o.str();
			}
			else
			{
				ostringstream o;

				o << first;
				buffer=o.str();
			}

			s += buffer;
		}

		if (s.size() == 0)
		{
			delete cmd;
			cmd=NULL;
			callback.success("No messages to process.");
			return;
		}

		cmd->commands.push( make_pair(name,
					      operation + " " + s + parameters
					      + "\r\n"));
		++cmd->counter;

		installForegroundTask(cmd);
	} catch (...) {

		if (cmd)
			delete cmd;

		callback.fail("An exception occured while attempting to start a command.");
	}
}




////////////////////////////////////////////////////////////////////////
//

void mail::imap::readMessageAttributes(const vector<size_t> &messages,
				       MessageAttributes attributes,
				       mail::callback::message &callback)
{
	if (smap)
	{
		installForegroundTask(new smapFETCHATTR(messages,
							attributes,
							callback,
							*this));
		return;
	}

	string attrList="";

	if (attributes & ARRIVALDATE)
		attrList += " INTERNALDATE";

	if (attributes & MESSAGESIZE)
		attrList += " RFC822.SIZE";

	if (attributes & ENVELOPE)
		attrList += " ENVELOPE BODY.PEEK[HEADER.FIELDS (References)]";

	if (attributes & MIMESTRUCTURE)
		attrList += " BODYSTRUCTURE";

	if (attrList.size() == 0)
	{
		callback.success("No attributes were requested.");
		return;
	}

	attrList[0]='(';

	messagecmd(messages, " " + attrList + ")",
		   "UID FETCH", "FETCHENV",
		   callback);
}

void mail::imap::readMessageContent(const vector<size_t> &messages,
				    bool peek,
				    enum mail::readMode readType,
				    mail::callback::message &callback)
{
	if (smap)
	{
		installForegroundTask(new smapFETCH(messages,
						    peek,
						    "",
						    readType,
						    "",
						    callback,
						    *this));
		return;
	}

	doReadMessageContent(messages, peek, NULL, readType, callback);
}

//////////////////////////////////////////////////////////////////////////
//
// There is no single RFC 2060 command that returns the headers of a
// MIME section, a blank line, then contents.  Therefore, we punt: first,
// we request a .MIME, then when this command returns, we request the body.
// This is implemented using the following helper object.
//

LIBMAIL_START

class imapComboFetchCallback
	: public mail::callback::message {

	mail::callback::message &originalCallback;
	bool origPeek;
	imap &origImap;
	size_t origMessageNum;

	string mime_id;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	imapComboFetchCallback(mail::callback::message &callback,
			       bool peek,
			       size_t messageNum,
			       const mimestruct &msginfo,
			       imap &imapAccount);
	~imapComboFetchCallback();
	void messageTextCallback(size_t n, string text);
	void fail(string message);
	void success(string message);
};

LIBMAIL_END

mail::imapComboFetchCallback::imapComboFetchCallback(mail::callback::message
						     &callback,
						     bool peek,
						     size_t messageNum,
						     const mimestruct &msginfo,
						     imap &imapAccount)
	: originalCallback(callback), origPeek(peek), origImap(imapAccount),
	  origMessageNum(messageNum),
	  mime_id(msginfo.mime_id)
{
}

mail::imapComboFetchCallback::~imapComboFetchCallback()
{
}

void mail::imapComboFetchCallback::messageTextCallback(size_t n, string text)
{
	originalCallback.messageTextCallback(n, text);
}

void mail::imapComboFetchCallback::fail(string message)
{
	originalCallback.fail(message);
	delete this;
}

void mail::imapComboFetchCallback::success(string message)
{
	vector<size_t> messageVector;

	messageVector.push_back(origMessageNum);

	string cmd="BODY";

	if (origPeek)
		cmd="BODY.PEEK";

	origImap.messagecmd(messageVector,
			    " (" + cmd + "[" + mime_id + "])",
			    "UID FETCH",
			    "CONTENTS", originalCallback);
	delete this;
}

void mail::imapComboFetchCallback::reportProgress(size_t bytesCompleted,
						  size_t bytesEstimatedTotal,

						  size_t messagesCompleted,
						  size_t messagesEstimatedTotal
						  )
{
	originalCallback.reportProgress(bytesCompleted,
					bytesEstimatedTotal,
					messagesCompleted,
					messagesEstimatedTotal);
}

void mail::imap::readMessageContent(size_t messageNum,
				    bool peek,
				    const mail::mimestruct &msginfo,
				    enum mail::readMode readType,
				    mail::callback::message &callback)
{
	vector<size_t> messageVector;

	messageVector.push_back(messageNum);

	if (smap)
	{
		installForegroundTask(new smapFETCH(messageVector,
						    peek,
						    msginfo.mime_id,
						    readType,
						    "",
						    callback,
						    *this));
		return;
	}

	if (readType == readBoth)
	{
		const mail::mimestruct *parent=msginfo.getParent();

		if (parent != NULL && !parent->messagerfc822())
		{
			// There is no RFC2060 command that returns what we
			// want here: .MIME, then the body, so we punt

			mail::imapComboFetchCallback *fakeCallback=new
				mail::imapComboFetchCallback(callback,
							     peek,
							     messageNum,
							     msginfo,
							     *this);

			if (!fakeCallback)
				LIBMAIL_THROW("Out of memory.");

			try {
				doReadMessageContent(messageVector,
						     peek,
						     &msginfo,
						     readHeaders,
						     *fakeCallback);
			} catch (...) {
				delete fakeCallback;
				LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
			}
			return;
		}
	}
	doReadMessageContent(messageVector, peek, &msginfo,
			     readType,
			     callback);
}

//
// readMessageContentDecoded is implemented by creating a proxy
// mail::generic::Decode object, that handles MIME decoding by itself,
// then forwards the decoded data to the original callback.

void mail::imap::readMessageContentDecoded(size_t messageNum,
					   bool peek,
					   const mail::mimestruct &msginfo,
					   mail::callback::message
					   &callback)
{
	if (smap)
	{
		vector<size_t> messageVector;

		messageVector.push_back(messageNum);

		installForegroundTask(new smapFETCH(messageVector,
						    peek,
						    msginfo.mime_id,
						    readContents,
						    ".DECODED",
						    callback,
						    *this));
		return;
	}

	mail::generic::Decode *d=
		new mail::generic::Decode(callback,
					  msginfo.content_transfer_encoding);

	if (!d)
		LIBMAIL_THROW("Out of memory.");

	try {
		readMessageContent(messageNum, peek, msginfo,
				   readContents, *d);
	} catch (...)
	{
		delete d;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

//
// Helper class for folding headers read from the IMAP server.
//

LIBMAIL_START

class imapFolderHeadersCallback : public mail::callback::message {

	mail::callback::message &originalCallback;

	bool doFolding;

	bool prevWasNewline;
	bool prevFoldingSpace;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	imapFolderHeadersCallback(bool, mail::callback::message &);
	~imapFolderHeadersCallback();

	void messageTextCallback(size_t n, string text);
	void success(string message);
	void fail(string message);
};

LIBMAIL_END

mail::imapFolderHeadersCallback
::imapFolderHeadersCallback(bool doFoldingArg,
			    mail::callback::message &origArg)
	: originalCallback(origArg), doFolding(doFoldingArg),
	  prevWasNewline(false), prevFoldingSpace(false)
{
}

mail::imapFolderHeadersCallback::~imapFolderHeadersCallback()
{
}

void mail::imapFolderHeadersCallback
::reportProgress(size_t bytesCompleted,
		 size_t bytesEstimatedTotal,

		 size_t messagesCompleted,
		 size_t messagesEstimatedTotal)
{
	originalCallback.reportProgress(bytesCompleted, bytesEstimatedTotal,
					messagesCompleted,
					messagesEstimatedTotal);
}

// Intercept the data in the callback, and use it to fold header lines.

void mail::imapFolderHeadersCallback
::messageTextCallback(size_t n, string text)
{
	string::iterator b=text.begin(), e=text.end(), c=b;

	string cpy="";

	while (b != e)
	{
		if (prevWasNewline) // Prev char was \n, which was eaten.
		{
			// This is the first character after a newline.

			prevWasNewline=false;
			if (*b == '\n') // Blank line, ignore
			{
				cpy += "\n"; // The eaten newline
				b++;
				c=b;
				continue;
			}

			if ( doFolding && unicode_isspace((unsigned char)*b))
			{
				// Bingo.

				prevFoldingSpace=true;
				cpy += " "; // Eaten newline replaced by spc
				b++;
				continue;
			}

			cpy += "\n"; // The eaten newline.
			c=b;
			b++;
			continue;
		}

		if (prevFoldingSpace)
		{
			if ( *b != '\n' && unicode_isspace((unsigned char)*b))
			{
				b++;
				continue; // Eating all whitespace
			}

			prevFoldingSpace=false; // Resume header.
			c=b;
		}

		if (*b == '\n')
		{
			cpy.insert(cpy.end(), c, b);
			prevWasNewline=true;
			b++;
			continue;
		}
		b++;
	}

	if (!prevWasNewline && !prevFoldingSpace)
		cpy.insert(cpy.end(), c, b);
		
	originalCallback.messageTextCallback(n, cpy);
}

void mail::imapFolderHeadersCallback::success(string message)
{
	originalCallback.success(message);
	delete this;
}

void mail::imapFolderHeadersCallback::fail(string message)
{
	originalCallback.fail(message);
	delete this;
}

//////
void mail::imap::doReadMessageContent(const vector<size_t> &messages,
				      bool peek,
				      const mail::mimestruct *msginfo,
				      mail::readMode readType,
				      mail::callback::message &callback)
{
	if (!ready(callback))
		return;

	bool needHeaderDecode=false;
	bool foldHeaders=false;

	string mime_id;
	const mail::mimestruct *parent=msginfo ? msginfo->getParent():NULL;

	// RFC 2060 mess.

	if (readType == readBoth)
	{
		if (parent == NULL)
			mime_id="";
		else
			mime_id=parent->mime_id;
	}
	else if (readType == readHeadersFolded ||
		 readType == readHeaders)
	{
		needHeaderDecode=true;

		if (readType == readHeadersFolded)
			foldHeaders=true;

		if (parent == NULL)
			mime_id="HEADER";
		else if (parent->messagerfc822())
			mime_id=parent->mime_id + ".HEADER";
		else
			mime_id=msginfo->mime_id + ".MIME";
	}
	else
	{
		// Retrieve content

		if (parent == NULL)
			mime_id="TEXT";
		else if (parent->messagerfc822())
			mime_id=parent->mime_id + ".TEXT";
		else
			mime_id=msginfo->mime_id;
	}

	string cmd="BODY";

	if (peek)
		cmd="BODY.PEEK";

	mail::imapFolderHeadersCallback *decodeCallback=NULL;

	if (needHeaderDecode)
	{
		if ((decodeCallback=new mail::imapFolderHeadersCallback
		     (foldHeaders, callback)) == NULL)
		{
			callback.fail("Out of memory.");
			return;
		}
	}

	try {
		mail::callback::message *cb= &callback;

		if (decodeCallback)
			cb=decodeCallback;

		messagecmd(messages,
			   " (" + cmd + "[" + mime_id + "])", "UID FETCH",
			   "CONTENTS", *cb);

	} catch (...) {
		if (decodeCallback)
			delete decodeCallback;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

/////////////////////////////////////////////////////////////////////////
//
// Helper class for most commands.  Determines whether the read line of
// input contains an untagged message (which results in untaggedMessage()
// getting invoked), or a tagged message (which results in taggedMessage()
// getting invoked.

mail::imapCommandHandler::imapCommandHandler(int timeout)
	: mail::imapHandler(timeout)
{
}

mail::imapCommandHandler::~imapCommandHandler()
{
}

int mail::imapCommandHandler::process(mail::imap &imapAccount, string &buffer)
{
	string::iterator b=buffer.begin();
	string::iterator e=buffer.end();

	while (b != e)
	{
		if (unicode_isspace((unsigned char)*b))
		{
			b++;
			continue;
		}
		if (*b != '*')
			break;

		b++;

		string w=imapAccount.get_word(&b, &e);

		if (b == e)
			break;

		size_t cnt= b - buffer.begin();

		mail::upper(w);

		if (untaggedMessage(imapAccount, w))
			return (cnt);
		return (0);
	}

	size_t p=buffer.find('\n');

	if (p == std::string::npos)
		return (0);

	string buffer_cpy=buffer;

	buffer_cpy.erase(p);

	size_t stripcr;

	while ((stripcr=buffer_cpy.find('\r')) != std::string::npos)
		buffer_cpy.erase(buffer_cpy.begin() + stripcr,
				 buffer_cpy.begin() + stripcr + 1);

	if (buffer_cpy.size() > 0 &&
	    buffer_cpy[0] == (imapAccount.smap ? '>':'+'))
		return continuationRequest(imapAccount, buffer_cpy) ? p+1:0;

	b=buffer_cpy.begin();
	e=buffer_cpy.end();

	string w=imapAccount.get_cmdreply(&b, &e);

	upper(w);

	if (imapAccount.smap)
	{
		while (b != e && unicode_isspace((unsigned char)*b))
			b++;

		buffer_cpy.erase(0, b - buffer_cpy.begin());

		return taggedMessage(imapAccount,
				     w,
				     buffer_cpy,
				     w == "+OK",
				     buffer_cpy) ? p+1:0;
	}

	buffer_cpy.erase(0, b - buffer_cpy.begin());

	string errmsg=buffer_cpy;

	b=errmsg.begin();
	e=errmsg.end();

	string okfailWord=imapAccount.get_word(&b, &e);

	upper(okfailWord);

	while (b != e && unicode_isspace((unsigned char)*b))
		b++;

	errmsg.erase(0, b-errmsg.begin());

	b=errmsg.begin();
	e=errmsg.end();

	if (b != e && *b == '[')  // Drop OK [APPENDUID ...], and friends
	{
		while (b != e && *b != ']')
			b++;

		if (b != e)
			b++;

		while (b != e && unicode_isspace((unsigned char)*b))
			b++;
		errmsg.erase(0, b-errmsg.begin());
	}

	ptr<mail::imap> acctPtr= &imapAccount;

	int n=taggedMessage(imapAccount, w, buffer_cpy,
			    okfailWord == "OK", errmsg) ? p+1:0;

	// Report any accumulated server messages in the response
	
	if (!acctPtr.isDestroyed() && acctPtr->serverMsgs.size() > 0)
	{
		vector<string>::iterator b, e;

		b=acctPtr->serverMsgs.begin();
		e=acctPtr->serverMsgs.end();

		errmsg="";

		while (b != e)
		{
			if (errmsg.size() > 0)
				errmsg += "\n";
			errmsg += *b++;
		}
		acctPtr->serverMsgs.clear();
		acctPtr->fatalError(errmsg);
	}
	return n;
}

// Subclasses must explicitly declare their intention to handle continuation
// requests.

bool mail::imapCommandHandler::continuationRequest(mail::imap &imapAccount,
						   string request)
{
	return false;
}

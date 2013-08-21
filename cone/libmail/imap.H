/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_imap_H
#define libmail_imap_H

#include "libmail_config.h"
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "mail.H"
#include	<sys/types.h>
#include	<time.h>
#include	<map>
#include	<set>
#include	<queue>

#include "imapfolders.H"
#include "unicode/unicode.h"
#include "maildir/maildirkeywords.h"
#include "fd.H"

///////////////////////////////////////////////////////////////////////////
//
// An IMAP implementation.  Probably the most sophisticated libmail driver,
// primarily because it directly implements every request, instead of
// fobbing off the task to libgeneric.
//

#define IMAP_UWIMAP	"uwimap"
#define IMAP_COURIER	"courier"
#define	IMAP_CYRUS	"cyrus"   // TODO

LIBMAIL_START

class imapSYNCHRONIZE;

class smapHandler;
class smapAddMessage;

class imap : public fd {

private:

	bool orderlyShutdown; // true - orderly shutdown in progress

public:
	time_t timeoutSetting;
	time_t noopSetting;
	bool noIdle;		// Manual override - do not use IMAP IDLE

	const imapFolder *sameServerFolderPtr;
	// helper fld to detect folders linked
	// to this server

	bool smap;

	int (smapHandler::*smapProtocolHandlerFunc)(imap &, std::string &);
	unsigned long smapBinaryCount;

private:

	int socketRead(const std::string &readbuffer);

	struct ltstr {
		bool operator()(const char *a, const char *b) const
		{
			return (strcmp(a,b) < 0);
		}
	};

public:
	bool ready(mail::callback &callback);

	void serverError(const char *errmsg)
	{
		disconnect_callback.servererror(errmsg);
	}

	// A number of "handlers" are active at any time, ready to process
	// server responses.  Each handler is identified by a unique name,
	// and all handlers are saved in the following map.

	typedef std::map<const char*, class imapHandler *, ltstr>
		handlerMapType;

	handlerMapType handlers;

	friend class imapFOLDER_COUNT;
	friend class imapSELECT_FLAGS;
	friend class imapSELECT_OK;
	friend class imapSELECT;
private:

	std::set<std::string> capabilities; // Server capabilities
	std::set<std::string> folderFlags;  // Current folder's available flags

	std::set<std::string> permanentFlags;
	void setPermanentFlags(std::string::iterator flagb,
			       std::string::iterator flage);

	class imapHandler *current_handler;
	// Handler the last processed server output.


public:
	void fatalError(std::string errmsg); // server connection failed
private:
	size_t cmdcounter; // Counter to generate unique IMAP cmd tags.

	// An IMAP cmdtag consists of a unique counter portion, followed by
	// handler's name, so when we receive a server response we know which
	// handler is responsible.

public:

	// INTERNAL FUNCTIONS

	mail::keywords::Hashtable keywordHashtable; // Keyword support

	bool keywordAllowed(std::string);

	class imapFOLDERinfo *currentFolder; // Currently open folder
	imapSYNCHRONIZE *currentSYNCHRONIZE; // Current synchronization

	class imapFetchHandler *currentFetch;
	// Current fetch reply in progress.

	bool wantIdleMode; // True if we want to be in IDLE mode, now

	void updateNotify(callback *callbackArg);

	std::queue<imapHandler *> task_queue;
	// Waiting handlers that will be added to the active handler map
	// as soon as all the existing handlers are gone.

	void installBackgroundTask(imapHandler *handler);
	// Install a new handler unconditionally.  Typically, the handler
	// understands server messages not particular to any command
	// (* EXPUNGE, et al...)

	void installForegroundTask(imapHandler *handler);
	// Install a pending task, typically a command.  The handler
	// is placed on the task_queue if any other foreground handler is
	// already present.  When the foreground handler is done, the first
	// task is popped off the task_queue, to replace it.

	void uninstallHandler(imapHandler *handler);
	// The handler's probably done its job.

	void removeAllHandlers(bool timedOutFlag, const char *errmsg);
	// Possibly a fatal error.
	// timedOutFlag - handlers are removed due to server timeout
	// (all handler's timedOut() method is invoked).  timedOutFlag
	// is false is set when removeAllHandlers() is invoked from the
	// destructor, where we want to clean up only, and we better not
	// trigger any more activity.

	void remove(imapHandler *handler);
	// Remove a specific handler from the handler map.

private:
	void insertHandler(imapHandler *handler);
	// Add a handler to the handler map

public:
	imapHandler *hasForegroundTask();
	// Indicate whether any foreground tasks are running

	void imapcmd(std::string cmd, std::string arg);
	// Write a command to the server.

	void disconnect()
	{
		disconnect(NULL);
	}

private:
	void timeoutdisconnect();
public:
	void disconnect(const char *errmsg);

	void setCapability(std::string capability);
	void clearCapabilities() { capabilities.clear(); }
	// Utility functions

	static std::string get_word(std::string::iterator *b, std::string::iterator *e);
	static std::string get_cmdreply(std::string::iterator *b, std::string::iterator *e);

	static std::string quoteSimple(std::string s);
	static std::string quoteSMAP(std::string s);

	static inline bool ok(std::string w)
	{
		return (w == "OK");
	}

	void readSubFolders(std::string path,
			    mail::callback::folderList &callback1,
			    mail::callback &callback2);

	mail::addMessage *addMessage(std::string path,
				     mail::callback &callback);

	void folderStatus(std::string path,
			  mail::callback::folderInfo &callback1,
			  mail::callback &callback2);

	void openFolder(std::string path, mail::snapshot *restoreSnapshot,
			mail::callback &openCallback,
			mail::callback::folder &folderCallback);

	//////////// PUBLIC INTERFACE FUNCTIONS //////////////

	// The constructors begins the login process.

	imap(std::string url, std::string passwd,
	     std::vector<std::string> &certificates,
	     mail::loginCallback *loginCallbackFunc,
	     mail::callback &callback,
	     mail::callback::disconnect &disconnectCallback);

	imap(const imap &);  // UNDEFINED
	imap &operator=(const imap &); // UNDEFINED
	~imap();

	void resumed();

	void handler(std::vector<pollfd> &fds, int &timeout);
	// Event loop processing

	bool hasCapability(std::string capability);
	std::string getCapability(std::string capability);

	std::string servertype;
	std::string serverdescr;

	void logout(mail::callback &callback);
	void checkNewMail(mail::callback &callback);

	std::string messageFlagStr(const mail::messageInfo &indexInfo,
			      mail::messageInfo *oldIndexInfo=NULL,
			      bool *flagsUpdated=NULL);

	std::vector <imapFolder> namespaces;

	std::vector <std::string> serverMsgs; // * NO, * OK ALERT...

private:

	mail::folder *folderFromString(std::string);
	void readTopLevelFolders(mail::callback::folderList &callback1,
				 mail::callback &callback2);
	void findFolder(std::string folder,
			class mail::callback::folderList &callback1,
			class mail::callback &callback2);
	std::string translatePath(std::string path);

	folder *getSendFolder(const smtpInfo &info,
			      const mail::folder *folder,
			      std::string &errmsg);

	size_t getFolderIndexSize();
	class mail::messageInfo getFolderIndexInfo(size_t);
	void getFolderKeywordInfo(size_t, std::set<std::string> &);

public:
	// NOT public interface, for friends only:

	static void addSmapFolderCmd(mail::smapAddMessage *, std::string);

	void messagecmd(const std::vector<size_t> &messages, std::string parameters,
			std::string operation,
			std::string name,
			mail::callback::message &callback);
	// Message data retrieval function.
	//    messages - which messages to retrieve
	//    parameters - which attributes to retrieve.
	//    operation - the IMAP command name
	//
	// The typical command that the server received would be
	// "name operation [msgset] parameters" (name is the imap tag).

	class msgSet;

	void messagecmd(msgSet &messages, std::string parameters,
			std::string operation,
			std::string name,
			mail::callback::message &callback);

	// Like the above, except that 'messages' is an abstract superclass
	// that's defined as follows:

	class msgSet {
	public:
		msgSet();
		virtual ~msgSet();

		virtual bool getNextRange(size_t &first, size_t &last)=0;
		//
		// Each time getNextRange is invoked, it should initialize
		// first/last to the starting/ending affected message number,
		// and returns true; or false if no more messages

	};

	// Construct a msgSet from UIDs of indicated messages.
	//
	// UIDs are used in order to handle situations where something
	// got expunged in the middle of multiple fetches (yes, servers
	// shouldn't really do this, but we can handle it).

	class msgSetRange : public msgSet {

		std::vector<size_t> msglist;

		std::vector<size_t>::iterator nextMsg;

		imap *p;

		static size_t uidNum(imap *, size_t);

	public:
		msgSetRange(imap *pArg, const std::vector<size_t> &messages);
		~msgSetRange();

		bool getNextRange(size_t &first, size_t &last);
	};


private:

	// mail::account implementations:

	void readMessageAttributes(const std::vector<size_t> &messages,
				   MessageAttributes attributes,
				   class mail::callback::message
				   &callback);

	void readMessageContent(const std::vector<size_t> &messages,
				bool peek,
				enum mail::readMode readType,
				mail::callback::message &callback);

	void readMessageContent(size_t messageNum,
				bool peek,
				const mimestruct &msginfo,
				enum mail::readMode readType,
				mail::callback::message &callback);

	void readMessageContentDecoded(size_t messageNum,
				       bool peek,
				       const mimestruct &msginfo,
				       mail::callback::message &callback);

	void doReadMessageContent(const std::vector<size_t> &messages,
				  bool peek,
				  const class mimestruct *msginfo,
				  mail::readMode getType,
				  mail::callback::message &callback);

	void saveFolderIndexInfo(size_t messageNum,
				 const mail::messageInfo &indexInfo,
				 mail::callback &callback);

	void updateFolderIndexFlags(const std::vector<size_t> &messages,
				    bool doFlip,
				    bool enableDisable,
				    const mail::messageInfo &flags,
				    mail::callback &callback);

	void updateFolderIndexFlags(const std::vector<size_t> &messages,
				    bool enableDisable,
				    const mail::messageInfo &flags,
				    mail::callback &callback);

	void updateKeywords(const std::vector<size_t> &messages,
			    const std::set<std::string> &keywords,
			    bool setOrChange,
			    // false: set, true: see changeTo
			    bool changeTo,
			    callback &cb);
	void updateImapKeywords(const std::vector<size_t> &messages,
				const std::set<std::string> &keywords,
				bool changeTo,
				callback &cb);
	class updateImapKeywordHelper;
public:
	friend class updateImapKeywordHelper;
private:

	void updateNotify(bool enableDisable, callback &callbackArg);

	void updateFolderIndexInfo(mail::callback &callback);
	void removeMessages(const std::vector<size_t> &messages,
			    callback &cb);

	void copyMessagesTo(const std::vector<size_t> &messages,
			    mail::folder *copyTo,
			    mail::callback &callback);

	void moveMessagesTo(const std::vector<size_t> &messages,
			    mail::folder *copyTo,
			    mail::callback &callback);

	void searchMessages(const class mail::searchParams &searchInfo,
			    class mail::searchCallback &callback);

};

//////////////////////////////////////////////////////////////////////////
//
// A handler for a server response.  Each time something is read from the
// server, all available handlers' process() method is invoked.  A handler
// should return a non-zero count of consumed bytes from the readbuffer.
// A handler should return zero if readbuffer does not contain a response
// it understands.  If imap finds a handler that consumes some read
// data, it is removed from the readbuffer, and imap tries again
// (in the event the server sent multiple responses).
//
// imap reports a fatal error if no handler claims newline-terminated
// data.
//
// A handler must wait until imap tells it that it can proceed to do
// whatever it needs to do.  imap invokes the installed() method when
// the handler has the floor to itself.
//

class imapHandler {

protected:
	imap *myimap;
public:
	bool installedFlag;	// true if this handler is active.

	bool isBackgroundTask;	// true if this is a background handler

	int handlerTimeoutVal;		// When this handler times out.

protected:
	time_t timeoutAt;		// The actual system timestamp.

 public:
	friend class imap;

	imapHandler(int timeoutValArg=0);
	virtual ~imapHandler();

	virtual void installed(imap &)=0;
	virtual void anotherHandlerInstalled(mail::imap &);

	virtual bool getTimeout(imap &, int &);
	virtual int getTimeout(imap &);
	virtual void setTimeout();
	virtual void setTimeout(int timeoutVal);
	virtual int process(imap &imapAccount, std::string &buffer)=0;
	virtual const char *getName()=0;

	virtual void setBackgroundTask(bool flag) { isBackgroundTask=flag; }
	virtual bool getBackgroundTask() const { return isBackgroundTask; }

	virtual void timedOut(const char *errmsg)=0;

protected:
	static void callbackTimedOut(mail::callback &callback,
				     const char *errmsg);
};

//////////////////////////////////////////////////////////////////////////
//
// A helper class that separates untagged and tagged replies.  The class
// provides an implementation for process().  Subclasses should implement
// untaggedMessage() and/or taggedMessage().
//

class imapCommandHandler : public imapHandler {

public:
	imapCommandHandler(int timeout=0);
	~imapCommandHandler();

protected:
	int process(imap &imapAccount, std::string &buffer);

private:
	// Callback: process an untagged message
	//
	// name: the name of the untagged reply (the first word after '*',
	// uppercase).  If return true, '* name' is removed from the input
	// buffer, and the subclass is expected to have installed its own
	// handler, now.

	virtual bool untaggedMessage(imap &imapAccount, std::string name)=0;

	// Callback: process a tagged message
	//
	// name: the tag
	//
	// message: the rest of the message
	//
	// okfail: true if the first word of message is status
	//
	// errmsg: everything following the first word of the message
	//
	// A true return removes the line from the input buffer

	virtual bool taggedMessage(imap &imapAccount, std::string name,
				   std::string message,
				   bool okfail, std::string errmsg)=0;

	//
	// Continuation request received.
	//
	// A true return indicates that the continuation request has been
	// processed.
	//

	virtual bool continuationRequest(imap &imapAccount,
					 std::string request);
};

LIBMAIL_END

#endif

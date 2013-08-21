/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mail_H
#define libmail_mail_H

#include "libmail_config.h"
#include <time.h>
#include <string>

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/types.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/poll.h>

#include <vector>
#include <list>
#include <set>

#include "namespace.H"
#include "objectmonitor.H"
#include "misc.H"

#if LIBMAIL_THROW_DEBUG
#define LIBMAIL_THROW(x) do { fprintf(stderr, "Exception at %s(%d)\n", __FILE__, __LINE__); throw x;} while (0)
#else
#define LIBMAIL_THROW(x) throw x

#define LIBMAIL_THROW_EMPTY

#endif

LIBMAIL_START

class envelope;
class xenvelope;
class mimestruct;
class addMessage;
class searchParams;
class searchCallback;
class smtpInfo;
class snapshot;
class loginInfo;

// Encapsulate poll() structures

typedef struct ::pollfd pollfd;

// i.e. pollfd.fd, pollfd.events,

static const int pollin=POLLIN;
static const int pollpri=POLLPRI;
static const int pollout=POLLOUT;
static const int pollerr=POLLERR;
static const int pollhup=POLLHUP;

static const int pollread= POLLIN | POLLHUP | POLLERR;

static const int pollwrite= POLLOUT | POLLERR;


//
// libmail.a uses a mail::callback object to notify the application when
// the requested operation succeeds.  libmail.a's API is completely
// asynchronous.  Most operations cannot be completed immediately.  Instead,
// the application passes a mail::callback object (or one of its subclasses)
// to a function/method call that makes a particular request.  The function
// or a method returns immediately, and the mail::callback's success() or
// fail() method will be called whenever the requested operation completes.
// All processing occurs within mail::account::process().
//
// It's possible that the requested operation can be completed immediately,
// without blocking for I/O.  In which case the function/method call invokes
// the success() or the fail() method prior to returning.

class callback {
public:
	callback() {}
	virtual ~callback() {}
	virtual void success(std::string message)=0;
	virtual void fail(std::string message)=0;

	virtual void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal)=0;

	class disconnect;
	class folder;
	class folderInfo;
	class folderList;
	class message;
};

class folder;
class messageInfo;
class loginCallback;

#define MAILCHECKINTERVAL 10

///////////////////////////////////////////////////////////////////////////
//
// A mail::account object represents a single mail account.  A mail account is
// defined as a collection of related mail folders.  A mail folder may
// contain other mail subfolders, or messages (or both), depending on the
// mail account.
//
// A mail::account object is not instantiated directly, but is instantiated by the
// mail::account::open() function, which instantiated the appropriate subclass
// based on the mail account's URL.  mail::account::open() returns a new mail::account
// object.  As a rule, the mail::account object is not yet fully initialized; the
// application must wait until the success() callback method is invoked.
//
// Mail folders are arranged into a tree-like hierarchy.  The list of folders
// at the topmost hierarchy level is obtained. by the readTopLevelFolders()
// method.

#define LIBMAIL_SINGLEFOLDER "*LIBMAIL_SINGLEFOLDER"

#define LIBMAIL_CHEAPSTATUS "*LIBMAIL_CHEAPSTATUS"

#define LIBMAIL_SERVERTYPE "*LIBMAIL_SERVERTYPE"

#define LIBMAIL_SERVERDESCR "*LIBMAIL_SERVERDESCR"

#define LIBMAIL_ACL	"ACL"

class account : public obj {

	// All instantiated mail::account objects:

	static std::list<account *> mailaccount_list;

	std::list<account *>::iterator my_mailaccount_p;

	virtual void resumed()=0;

public:
	static void resume();
private:
	// Handle any pending I/O for this mail::account object.

	virtual void handler(std::vector<pollfd> &fds, int &timeout)=0;

protected:

	// Disconnect callback object that's passed to the constructor.

	callback::disconnect &disconnect_callback;

public:

	account(callback::disconnect &disconnect_callback);
	//
	// disconnect_callback - a callback object.
	//     disconnect_callback.disconnected() gets called from within
	//     handler() when the connection to the server is terminated.
	//     disconnect_callback.servererror() gets called whenever the
	//     server reports a fatal error which does not result in the
	//     server connection shutting down.

	virtual ~account();

	class openInfo {
	public:
		//
		// Information for creating a new mail::account object:

		std::string url;
		std::string pwd;

		std::vector<std::string> certificates; // Raw SSL certificates

		std::string extraString; // newsrc filename, etc...

		mail::loginCallback *loginCallbackObj;

		openInfo();
		~openInfo();
	};


	static account *open(openInfo openInfoArg,
			     callback &callback,
			     callback::disconnect &disconnectCallback);
	//
	// Create a new mail::account object.
	//
	// url - mail account URL.
	// pwd - mail account password.  May be an empty string for mail
	//      accounts that do not require passwords (local mail storage).
	// callback - The open() function generally returns immediately.
	//            The app must wait until callback.success() is invoked
	//            indicating that libmail.a succesfully opened account.
	//            callback.fail() is invoked if the login failed (at which
	//            point the mail::account object is automatically destroyed).
	//            A fatal error within the open function itself,
	//            (such as an invalid URL or out of memory) results in
	//            an invocation of callback.fail(), followed by open()
	//            return a NULL pointer.
	//
	// disconnect_callback -
	//            disconnect_callback.disconnected() gets called from
	//            within handler() when the connection to the server is
	//            terminated. disconnect_callback.servererror() gets
	//            called whenever the server reports a fatal error which
	//            does not result in the server connection shutting down.
	//
	//            The disconnect_callback object may not be destroyed until
	//            either callback.fail() calls, or the logout() function
	//            completes (the fail() or the success() method of the
	//            object passed to mail::account::logout() is invoked).


	static bool isRemoteUrl(std::string url);
	// Return TRUE if url represents a remote server account
	// (IMAP, POP3, NNTP, or SMTP)

	static void process(std::vector<pollfd> &fds, int &timeout);

	//
	// The process function is where ALL libmail.a processing takes
	// place.  All other methods in all mail::account objects merely
	// initiate a request to perform the given task.  The process()
	// function must be called periodically in order to process pending
	// I/O requests.
	//
	// The fds and timeout arguments are intended to be passed directly
	// to the poll(2) function call.  Before calling process() initialize
	// fds to an empty vector.  'timeout' should also be initialized
	// to a fairly large timeout value, which should generally be the
	// timeout before any further application activity takes place.
	//
	// After process() terminates, it should not be invoked again until
	// poll(&fds[0], fds.size(), timeout) indicates that I/O requested
	// by the fd array is available, or until the 'tv' timeout expires.
	//
	// Note that the application should check if fds.size() == 0, which
	// indicates that libmail.a does not expect I/O on any file descriptor,
	// instead the application needs to wait for a simple timer to expire.
	// 
	// If mail::account::process expects further processing to take place
	// earlier than what's indicated by 'timeout', mail::account::process()
	// will reduce the indicated timeout (the timeout will never be made
	// longer, only shorter).

	static int poll(std::vector<pollfd> &fds, int timeout);

	virtual void logout(callback &callback)=0;
	//
	// Close the mail account.  The application should wait until
	// either callback.success() or callback.fail() is called (not much
	// of a difference, really, in both cases the mail::account object gets
	// automatically destroyed)

	virtual void checkNewMail(callback &callback)=0;
	//
	// Explicitly check for any new mail if a folder is currently open.
	// A call to mail::account::checkNewMail() invokes mail::folderCallback's
	// methods to reflect any changes to the currently open folder.
	// mail::folderCallback::newMessages() will be invoked if new messages
	// were added to the folder. mail::folderCallback::messagesRemoved()
	// will be invoked if messages were removed from a folder (perhaps
	// by another application doing updateFolderIndexInfo, and
	// mail::folderCallback::messageChanged() may be invoked to reflect
	// changes to existing messages.

	virtual bool hasCapability(std::string capability)=0;
	virtual std::string getCapability(std::string capability)=0;
	//
	// Indicate whether this mail::account object supports the indicated
	// capability.

	virtual class folder *folderFromString(std::string)=0;
	//
	// The opposite of mail::folder::toString().  Creates a new
	// mail::folder object.  The mail::folder object is linked to this
	// mail::account object, and may only be used as argument to this mail::account
	// object's methods.
	// Note that the returned object does not imply that the actual
	// folder exists, only that if it does exist then the new
	// mail::folder object may be used to refer to it.

	virtual void readTopLevelFolders(callback::folderList
					 &callback1,
					 callback &callback2)=0;
	// Enumerate the topmost mail folders available in this mail account.
	// callback1.success() is invoked to enumerate the folders at the
	// top level of the folder hierarchy.

	virtual void findFolder(std::string path,
				callback::folderList &callback1,
				callback &callback2)=0;
	// Recreate a folder object.
	//
	// path - the folder's known path -- can be obtained by invoking
	// mail::folder::getPath().
	//
	// name - the name of this folder, in the application charset.

	virtual std::string translatePath(std::string path)=0;
	//
	// Take a "human readable" path, and return its server representation.
	// For example: IMAP accounts translate path from the local charset
	// to modified-UTF7.
	//
	// Returns an empty string if the original path was invalid
	// (setting errno accordingly).  A non-empty return does not guarantee
	// an existing folder, merely that the path is valid.



	virtual folder *getSendFolder(const smtpInfo &info,
				      const mail::folder *folder,
				      std::string &errmsg);
	//
	// Create a folder object to be used for sending outgoing
	// mail.
	//
	// info - parameters for sending mail
	//
	// folder - if not NULL, file a CC of the message into this folder
	//
	// errmsg - if this mail account login cannot be used for sending
	// mail, getSendFolder() returns a null ptr, and sets errmsg
	// accordingly.


	enum MessageAttributes {
		ARRIVALDATE=1,
		MESSAGESIZE=2,
		ENVELOPE=4,
		MIMESTRUCTURE=8};

	virtual void readMessageAttributes(const std::vector<size_t> &messages,
					   MessageAttributes attributes,
					   callback::message
					   &callback)=0;
	//
	// Return metadata about messages in the currently open folder.
	//
	// messages - a list of messages whose metadata should be returned.
	//
	// attributes - which attributes to return.  A logical OR of the
	//     enumerated constant.  Each constant directly translates to a
	//     method in the mail::callback::message object.  If multiple
	//     messages are specified the order of messages for whom the
	//     the attributes are returned is arbitrary.  If multiple attrs
	//     are specified the order in which the attributes are returned is
	//     arbitrary.
	//     With multiple messages and attributes, any order is possible.
	//     It's possible that all attributes for the same message are
	//     returned before the attributes for the next message are
	//     returned; or the attribute for all messages are returned before
	//     returning the next attribute for all messages, etc...
	//
	//              ARRIVALDATE - when the message was added to the
	//              folder (messageArrivalDateCallback).
	//
	//              MESSAGESIZE - approximate message size (may not be
	//              exact, may be just a rough estimate --
	//              messageSizeCallback).
	//
	//              ENVELOPE - message envelope/thread summary
	//              (messageEnvelopeCallback, maybe
	//               messageReferencesCallback).
	//
	//              MIMESTRUCTURE - message's MIME structure
	//              (messageStructureCallback).
	//

	virtual void readMessageContent(const std::vector<size_t> &messages,
					bool peek,
					enum mail::readMode readType,
					callback::message
					&callback)=0;

	//
	// Return the entire header and/or body content of one or more
	// messages.
	//
	// messages - a list of messages whose contents are returned.
	// peek     - if true, do not reset the unread flag, if possible.
	// justHeader - return the contents of the header portion of the
	//              message.
	// justContents - return the contents of the body portion of each
	//              message.
	// mail::callback::message - the messageTextCallback method of
	// this object is invoked for each listed message.  The relative
	// order of messages is arbitrary, but the requested contents of
	// each message are returned in their entirety before returning the
	// contents of the next message.
	//
	// Either justHeader or justContents (or both) must be set.  If
	// only justHeader is set (justContents is not set), header lines are
	// automatically folded.

	virtual void readMessageContent(size_t messageNum,
					bool peek,
					const class mimestruct &msginfo,
					enum mail::readMode readType,
					callback::message
					&callback)=0;
	//
	// Return the contents of a MIME section of a message.
	// messageNum - the message whose contents are returned.
	// peek - do not reset the unread flag, if possible
	// msgInfo - the MIME section whose contents to be returned.
	// justHeader - return the contents of the header portion of the
	//              message.
	// justContents - return the contents of the body portion of each
	//              message.
	//
	// Either justHeader or justContents (or both) must be set.  If
	// only justHeader is set (justContents is not set), header lines are
	// automatically folded.

	virtual void readMessageContentDecoded(size_t messageNum,
					       bool peek,
					       const class mimestruct
					       &msginfo,
					       callback::message
					       &callback)=0;
	//
	// Return the decoded contents of a MIME section.  Like
	// readMessageContents, except that base64/quoted-printable content
	// is automatically decoded, and callback.messageTextCallback()
	// gets the raw, decoded, contents.
	//
	// messageNum - the message whose contents are returned.
	// peek - do not reset the unread flag, if possible
	// msgInfo - the MIME section whose contents to be returned.

	virtual size_t getFolderIndexSize()=0;
	//
	// Return the number of messages in the currently opened folders.
	// The message number arguments to the previous functions should range
	// should range between 0 and one less than the return value from
	// getFolderIndexSize()
	//

	virtual class messageInfo getFolderIndexInfo(size_t)=0;
	//
	// Return the mail::messageInfo structure for an indicated message.
	//

	virtual void saveFolderIndexInfo(size_t messageNum,
					 const messageInfo &msgInfo,
					 callback &callback)=0;
	//
	// Reflect updated message flags.
	//
	// messageNum - update flags of this message
	// msgInfo - new message flags.


	virtual void updateFolderIndexFlags(const std::vector<size_t> &messages,
					    bool doFlip,
					    bool enableDisable,
					    const messageInfo &flags,
					    callback &callback)=0;
	//
	// Update flags of multiple messages.
	//
	// messages - list of messages whose flags are to be updated.
	//
	// doFlip - flip the state of the indicated flags
	//
	// enableDisable - whether the indicated flags should be set, or
	//          cleared (ignored if doFlip is set)
	//
	// flags - which flags should be changed.  The flags structure is
	// NOT saved for each indicated message, instead the action specified
	// by doFlip/enableDisable is applied to each flag set in the flags
	// object.

	virtual void updateFolderIndexInfo(callback &)=0;
	//
	// Remove deleted messages.


	virtual void removeMessages(const std::vector<size_t> &messages,
				    callback &cb)=0;
	// Remove specified messages



	// Update keywords
	virtual void updateKeywords(const std::vector<size_t> &messages,
				    const std::set<std::string> &keywords,
				    bool setOrChange,
				    // false: set, true: see changeTo
				    bool changeTo,
				    callback &cb);

	// Variations of the above.  Implemented in the base class.

	void updateKeywords(const std::vector<size_t> &messages,
			    const std::vector<std::string> &keywords,
			    bool setOrChange,
			    // false: set, true: see changeTo
			    bool changeTo,
			    callback &cb);

	void updateKeywords(const std::vector<size_t> &messages,
			    const std::list<std::string> &keywords,
			    bool setOrChange,
			    // false: set, true: see changeTo
			    bool changeTo,
			    callback &cb);

	virtual void getFolderKeywordInfo(size_t, std::set<std::string> &);

	//
	// Application requests to be notified about folder changes
	// immediately, if possible (IMAP IDLE)
	//

	virtual void updateNotify(bool enableDisable, callback &callbackArg);

	//
	// Update the folder according to the flags on each message.
	// Generally, each message with the deleted flag is set gets removed
	// from the folder.  Additionally, updateFolderIndexInfo() usually
	// results in the applicatio notified regarding any changes to the
	// folder's contents, via mail::folderCallback's methods.

	virtual void copyMessagesTo(const std::vector<size_t> &messages,
				    folder *copyTo,
				    callback &callback)=0;
	//
	// Copy messages to another folder
	//
	// messages - which message to copy
	// copyTo - a folder from the same, or another mail account.


	virtual void moveMessagesTo(const std::vector<size_t> &messages,
				    mail::folder *copyTo,
				    mail::callback &callback);
	//
	// Like copyMessagesTo(), except that the messages are moved, not
	// copied (invoking mail::folderCallback::messagesRemoved()
	// appropriate.
	// Subclasses may override.  The generic implementation uses
	// copyMessagesTo(), followed by removeMessages


	virtual void searchMessages(const searchParams &searchInfo,
				    searchCallback &callback)=0;
	//
	// Search messages in this folder.
	//
	// searchInfo - specified the search criteria
	//
	// callback - callback object that receives the list of messages
	// that fit the search criteria
};

// gcc food

inline account::MessageAttributes operator| (account::MessageAttributes a,
					     account::MessageAttributes b)
{
	return (account::MessageAttributes)((int)a | (int)b);
}

inline account::MessageAttributes operator& (account::MessageAttributes a,
					     account::MessageAttributes b)
{
	return (account::MessageAttributes)((int)a & (int)b);
}

inline account::MessageAttributes &operator&= (account::MessageAttributes &a,
					       account::MessageAttributes b)
{
	return (a=(account::MessageAttributes)((int)a & (int)b));
}

inline account::MessageAttributes operator~ (account::MessageAttributes a)
{
	return (account::MessageAttributes)~(int)a;
}

//
// A folder object represents a folder in a mail account.  Each mail::folder
// object is linked to the mail::account object that created it.

class folder : public obj {

	ptr<mail::account> myServer;

protected:
	bool isDestroyed(callback &callback) const;
	bool isDestroyed() const;

	folder(const folder &); // UNDEFINED
	folder &operator=(const folder &o)
	{
		myServer=o.myServer;
		return *this;
	}

public:

	// Applications should not use the constructor directly, but instead
	// should use other methods in the mail::account and mail::folder objects
	// to create instances of mail::folder.  Applications can destroy
	// mail::folder objects at any time where the object is not an
	// argument to an uncompleted request.

	folder(mail::account *);
	virtual ~folder();

	virtual void sameServerAsHelperFunc() const=0;
	//
	// For internal use.
	//

	virtual std::string getName() const=0;
	//
	// Return the name of the folder, in the application character set.

	virtual std::string getPath() const=0;
	//
	// Return the folder's path.  Applications should consider paths to
	// be complete opaque strings, for libmail.a's internal use only.
	// The only officially blessed usage for paths is as arguments for
	// mail::account::findFolder, and mail::folder::isParentOf().

	virtual bool hasMessages() const=0;
	//
	// Return an indication whether this folder can contain message.

	virtual bool hasSubFolders() const=0;
	//
	// Return an indication whether this folder can contain subfolders.

	virtual std::string isNewsgroup() const;
	//
	// Returns a non-empty string if this is a newsgroup, with the
	// string being the newsgroup name.

	virtual bool isParentOf(std::string path) const=0;
	// True if this folder would be considered a parent folder in the
	// folder hierarchy.
	//
	// path - the path of another folder.

	virtual void hasMessages(bool)=0;
	virtual void hasSubFolders(bool)=0;
	//
	// For internal use only

	virtual void readFolderInfo( callback::folderInfo
				     &callback1,
				     callback &callback2) const=0;
	// Return folder metadata, such as the size of this folder


	virtual void getParentFolder(callback::folderList &callback1,
				     callback &callback2) const=0;

	// Return parent folder

	virtual void readSubFolders( callback::folderList &callback1,
				     callback &callback2)
		const=0;
	// Enumerate any subfolders in this folder

	virtual mail::addMessage
	*addMessage(callback &callback) const=0;
	//
	// Add a new message to this folder.  Returns a mail::addMessage
	// object.
	//
	// callback - the usual callback object.
	//
	// If addMessage() returns NULL, callback.fail() will be invoked to
	// indicate the reason why.  Otherwise, the new message's metadata
	// should be saved in the mail::addMessage's fields.
	// The message's contents must be passed to saveMessageContents()
	// (multiple times, if necessary), followed by
	// saveMessageContents::go().  If it becomes necessary to abort
	// the process, saveMessageContents::fail() will hold the presses,
	// invoke callback.fail(), and destroy the mail::addMessage object.


	virtual void createSubFolder(std::string name, bool isDirectory,
				     callback::folderList
				     &callback1,
				     callback &callback2) const=0;

	// Create a new subfolder.
	//
	// name - the name of the new subfolder.
	// isDirectory - if true, the subfolder is expected to contain
	// subfolders.  To create a folder that should contain both subfolders
	// and messages do this twice, first with isDirectory set to false,
	// then with isDirectory set to true.  Note that not every mail
	// account supports dual-purpose folders.
	// callback1 - if succesfully, callback1.success() gets invoked with
	// a folder list containing the new folder directory entry.

	virtual void create(bool isDirectory,
			    callback &callback) const=0;
	//
	// An alternative path to create a new folder is to use
	// mail::account::folderFromString() to create a folder object for a
	// folder that doesn't really exist, then use this method to create
	// the new folder.

	virtual void destroy(callback &callback, bool destroyDir)
		const=0;
	//
	// The opposite of create() - deletes this folder.  destroyDir should
	// be set to indicate if this folder contains subfolders or messages.
	// For dual-purpose folders, only the folder component represented
	// by destroyDir() gets removed.

	virtual void renameFolder(const folder *newParent, std::string newName,
				  callback::folderList &callback1,
				  callback &callback2) const=0;
	// Rename this folder.
	//
	// newParent - new parent folder, or NULL if folder should be renamed
	// to the top of the hierarcy tree
	// newName - new folder name, in application charset
	//


	virtual folder *clone() const=0;
	//
	// Create another mail::folder object that refers to the same
	// folder.

	virtual std::string toString() const=0;
	//
	// Convert this folder's identity to a single string.  Applications
	// should consider the string to be a completely opaque string; with
	// its only documented purpose is that passing the string to
	// mail::account::folderFromString() will create a mail::folder object that
	// refers to the same folder that this object refers to.

	virtual void open(callback &openCallback,
			  snapshot *restoreSnapshot,
			  callback::folder &folderCallback) const=0;
	//
	// Open the messages in this folder.  Any existing open folder is
	// automatically closed.  If the open fails, whether an existing folder
	// remains open is undefined.
	//
	// openCallback - the standard callback method whose methods will be
	// invoked to indicate whether the operation succeeded or not.
	//
	// If not NULL, a pointer to a snapshot object that holds a previously
	// saved folder index snapshot (see callback::folder::saveSnapshot).
	//
	//
	// folderCallback - the object whose methods will be invoked to notify
	// of any changes to the folder's state while it is opened.  This
	// object must not be destroyed, and must continue to exist until
	// either another folder's open request succeeds, or the mail account
	// is closed or disconnected.


	// ---------  Access control list support ---------- //

	virtual void getMyRights(callback &getCallback, std::string &rights)
		const;
	virtual void getRights(callback &getCallback,
			       std::list<std::pair<std::string, std::string> >
			       &rights)
		const;

	virtual void setRights(callback &setCallback,
			       std::string &errorIdentifier,
			       std::vector<std::string> &errorRights,
			       std::string identifier,
			       std::string rights)
		const;
	virtual void delRights(callback &setCallback,
			       std::string &errorIdentifier,
			       std::vector<std::string> &errorRights,
			       std::string identifier)
		const;


	// Helper function for sorting folders.

	class sort {

		bool foldersFirst;
	public:
		sort(bool foldersFirstArg);
		~sort();

		bool operator()(const folder *a, const folder *b);
	};
};

//
// A mail::callback::disconnect() object is provided for each request to
// open a mail account.
// When the connection to the mail account's server terminates, the object's
// disconnected() method is invoked.  If the connection was abnormally
// terminated, the errmsg string will be empty.
// The servererror() method may be invoked at any time to report a critical
// server error that was not severe enough to force the connection to be
// terminated.

class callback::disconnect {
public:
	disconnect() {}
	virtual ~disconnect() {}
	virtual void disconnected(const char *errmsg)=0;
	virtual void servererror(const char *errmsg)=0;
};

//
// Callbacks for readMessageContent():
//

class callback::message : public callback {
public:
	message();
	virtual ~message();

	virtual void messageEnvelopeCallback(size_t messageNumber,
					     const envelope
					     &envelopeArg);

	virtual void messageReferencesCallback(size_t messageNumber,
					       const std::vector<std::string>
					       &references);

	virtual void messageArrivalDateCallback(size_t messageNumber,
						time_t datetime);

	virtual void messageSizeCallback(size_t messageNumber,
					 unsigned long size);

	virtual void messageStructureCallback(size_t messageNumber,
					      const mimestruct
					      &messageStructure);
	virtual void messageTextCallback(size_t n, std::string text);

};

//
// A callback for readTopLevelFolders/readSubFolders:
// This callback object is also used for creating a new folder.  A
// mail::callback::folderList object is provided when creating a new folder.
// The success() method is invoked with a vector containing exactly one
// mail::folder object, representing the newly-created folder.
//
// NOTE: The mail::folder objects provided to the success() method are valid
// only until the success method() terminates, and are IMMEDIATELY destroyed
// afterwards.  If the callback function needs to preserve a mail::folder
// object (perhaps for the application's main code, later, after it checks
// that this request completed succesfully and needs to do something else to
// the folder) it should use mail::folder::clone() method to create its own
// private copy of the mail::folder object.

class callback::folderList {
public:
	folderList();
	virtual ~folderList();
	virtual void success(const std::vector<const mail::folder *>
			     &folders)=0;
};

/////////////////////////////////////////////////////////////////////
//
// Information about a folder

class callback::folderInfo {
public:
	size_t messageCount;
	size_t unreadCount;

	bool fastInfo;
	// Initialize to TRUE to only do this if it can be done fast.

	folderInfo();
	virtual ~folderInfo();

	virtual void success();
};

/////////////////////////////////////////////////////////////////////
//
//  A reference to the following object is saved when a folder is opened
//
//  The object should be subclassed, and virtual methods implemented.
//
//  libmail.a uses the object to notify the application about any changes
//  to the contents of the folder.  The application should expect any of
//  the following methods to be invoked at any time.  However, the methods
//  are usually invoked by mail::account::saveFolderIndexInfo(),
//  mail::account::updateFolderIndexFlags(), mail::account::updateFolderIndexInfo(),
//  and mail::account::checkNewMail().  Additionally, if the 'peek' argument to
//  mail::account::readMessageContents(), and mail::account::readMessageContentsDecoded() is
//  false, mail::folderCallback::messageChanged() will be invoked to indicate
//  that the message's unread flag has been reset.
//
//  However, the application must be prepared to deal with any method being
//  invoked at any time, in the event that the mail account allows the same
//  folder to be opened by multiple applications, and another applications
//  makes changes to the folder.

class callback::folder {
public:
	folder();
	virtual ~folder();

	virtual void newMessages()=0;
	//
	// New messages have been added to the folder.  The application
	// should use mail::account::getFolderIndexSize() to obtain the current
	// folder size, and compare it with the previously known folder
	// size in order to determine newly-added messages.

	virtual void messagesRemoved(std::vector< std::pair<size_t, size_t> >
				     &)=0;
	//
	// Messages were removed from the folder.  messagesRemoved() receives
	// a reference to an array of first-last pairs.  Each first-last
	// pair specifies a range of messages removed from the folder index.
	// <3, 5> indicates that messages 3, 4, and 5 were removed,
	// <3, 3> indicates that message #3 was removed.
	//
	// The array is sorted in numerically increasing order.

	virtual void messageChanged(size_t n)=0;
	//
	// Message metadata has changed.  Use mail::account::getFolderIndexInfo()
	// to obtain the updated set of message flags.

	virtual void saveSnapshot(std::string snapshotId);
	// Optionally cache the current folder index (getFolderIndexInfo()),
	// as snapshot "snapshotId"

};

//
// libmail.a collects the following metadata about each message, when
// a folder is opened.
// The application is notified via mail::folderCallback::messageChanged()
// about any changes to an existing message's metadata.
// NOTE: this includes changes initiated by the application itself.
// For example, mail::folderCallback::messageChanged() will usually be
// invoked before the mail::account::saveFolderIndexInfo() request completes
// Any changes to the following metadata are
// The folder index.

class messageInfo {
public:

	std::string uid;
	//
	// A unique ID is assigned to each message in a folder.  Applications
	// must consider the uid value to be a completely opaque string.
	// The only assumption that applications may make is that no two
	// messages will ever have the same uid in the same folder.
	// Not all mail accounts support the following flags.  Where not,
	// libmail.a will simulate the semantics of the flag while the
	// mail account is opened.

	bool	draft,		// This is a draft message
		replied,	// This message has been replied to
		marked,	       // This message is marked for further processing
		deleted,  // Marked for deletion (updateFolderIndexInfo()
			  // should end up removing this message

		unread,	// The contents of this message have not been read.
		recent; // This is the first time the folder has been opened
			// with this message in the folder.


#define LIBMAIL_MSGFLAGS \
	do { \
	DOFLAG(   FLAG, draft, "\\DRAFT"); \
	DOFLAG(   FLAG, replied, "\\ANSWERED"); \
	DOFLAG(   FLAG, marked, "\\FLAGGED"); \
	DOFLAG(   FLAG, deleted, "\\DELETED"); \
	DOFLAG(NOTFLAG, unread, "\\SEEN"); \
	} while (0)

#define LIBMAIL_SMAPFLAGS \
	do {\
		DOFLAG(   FLAG, draft, "DRAFT");\
		DOFLAG(   FLAG, replied, "REPLIED");\
		DOFLAG(   FLAG, deleted, "DELETED");\
		DOFLAG(   FLAG, marked, "MARKED");\
		DOFLAG(NOTFLAG, unread, "SEEN");\
	} while (0)

	messageInfo();

	~messageInfo();

	operator std::string() const;
	messageInfo(std::string);
};

LIBMAIL_END

#endif

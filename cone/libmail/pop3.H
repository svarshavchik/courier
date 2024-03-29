/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_pop3_H
#define libmail_pop3_H

#include "libmail_config.h"
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include	"mail.H"
#include	<sys/types.h>

#include	"maildir/maildirkeywords.h"
#include	"logininfo.H"
#include	"fd.H"
#include	"generic.H"
#include	"snapshot.H"
#include	<stdio.h>
#include	<time.h>
#include	<string>
#include	<map>
#include	<list>
#include	<vector>

///////////////////////////////////////////////////////////////////////////
//
// An POP3 implementation
//
// The POP3 INBOX folder

LIBMAIL_START

class pop3;
class addMessage;

class pop3Folder : public folder {

	void sameServerAsHelperFunc() const override;

	pop3 *server;

public:
	pop3Folder(pop3 *serverArg);
	~pop3Folder();

	std::string getName() const override;
	std::string getPath() const override;

	bool hasMessages() const override;
	bool hasSubFolders() const override;
	bool isParentOf(std::string path) const override;

	void hasMessages(bool) override;
	void hasSubFolders(bool) override;

	void getParentFolder(callback::folderList &callback1,
			     callback &callback2) const override;

	void readFolderInfo( class callback::folderInfo
			     &callback1,
			     callback &callback2) const override;

	void readSubFolders( callback::folderList &callback1,
			     callback &callback2) const override;

	mail::addMessage *addMessage(callback &callback) const override;

	void createSubFolder(std::string name, bool isDirectory,
			     class callback::folderList &callback1,
			     callback &callback2) const override;

	void create(bool isDirectory,
		    callback &callback) const override;

	void destroy(callback &callback, bool destroyDir) const override;

	void renameFolder(const mail::folder *newParent, std::string newName,
			  mail::callback::folderList &callback1,
			  callback &callback) const override;

	folder *clone() const override;
	std::string toString() const override;

	void open(class callback &openCallback,
		  snapshot *restoreSnapshot,
		  class callback::folder &folderCallback) const override;
};


//////////////////////////////////////////////////////////////////////////////

class pop3 : public fd, public generic, public snapshot::restore {

private:
	bool calledDisconnected;
	// True when the disconnect callback is invoked

	bool orderlyShutdown;

	time_t timeoutSetting;
	time_t noopSetting;

	// Superclass of POP3 tasks.  Methods create subclasses of Task
	// objects, and push them to the tasks queue.  Replies from the
	// POP3 server are routed to the foremost Task on the queue.

	class Task {

	protected:
		callback *callbackPtr;	// App callback.
		pop3 * volatile myserver;
		// Marked volatile due to destructor monkey business

		time_t defaultTimeout;

	public:
		virtual void done();
		// Task completed, start the next task on the queue,
		// and delete this Task object.

		Task(callback *callbackArg,
		     pop3 &myserverArg);
		virtual ~Task();

		virtual int getTimeout();
		// How long before this task times out.

		virtual void serverResponse(const char *message)=0;
		// Process a line of text from the server

		virtual void disconnected(const char *reason);
		// Server has disconnected.
		// The default implementation takes this task off the queue,
		// calls the next Task's disconnect method, then deletes
		// itself.

		virtual void installedTask();
		// This task is now at the front of the queue

		void resetTimeout();
		// Reset timeout interval

		virtual bool isLogoutTask();
		// This is a logout task.  If it's not the logout task,
		// its completion time is recorded.  If nothing happens,
		// a new NOOP task is started to keep the server from timing
		// out.

		virtual bool willReconnect();
		// This logout task is a part of expunge processing, so
		// do not do disconnect processing.
	};

#if 0
	class AutologoutCallback : public callback {
	public:
		AutologoutCallback();
		~AutologoutCallback();

		void success(std::string message);
		void fail(std::string message) override;
	};
#endif

	void resumed() override;
	void handler(std::vector<pollfd> &fds, int &timeout) override;

	std::map<std::string, std::string> capabilities;

	loginInfo pop3LoginInfo;

	loginInfo savedLoginInfo;

	int socketRead(const std::string &readbuffer) override;

	void disconnect(const char *reason) override;
	const char *application_protocol() const override;

	std::list<Task *> tasks;

	void installTask(Task *p);

	time_t lastTaskCompleted;

	class LoginTask;
	class LogoutTask;
	class LoggedInTask;

	class CheckNewMailTask;
	class ReadMessageTask;
	class ForceCheckNewMailTask;
	class CacheMessageTask;
	class UpdateTask;
	class NoopTask;

	pop3Folder inbox;

	// A map from message UID to the current message number in the POP3
	// server.
	std::map<std::string, int> uidlMap;

	std::map<int, unsigned long> listMap;
	// Size of each msg, in bytes.  Should be an array, though, but that's
	// ok.


	callback::folder *folderCallback;
	// The folder callback.
	// POP3 implements snapshots for the purpose of preserving message
	// status flags.  There is no concept of message status flags in POP3,
	// so by default, unless snapshots are used, each message is marked
	// as a new message, when the POP3 folder is opened.  If snapshots are
	// used, the message status flags will be restored from the snapshot.
	// The snapshot ID is always fixed, "POP3", since POP3 keys off msg
	// UIDs entirely, so a snapshot restore simply restores whatever
	// snapshot UIDs are available, then does a checknewmail.
	//
	// A snapshot is also saved each time message status flags are updated.

	// The virtual folder.

	class pop3MessageInfo : public messageInfo {
	public:
		pop3MessageInfo();
		~pop3MessageInfo();

		mail::keywords::Message keywords;
	};

	mail::keywords::Hashtable keywordHashtable;

	std::vector<pop3MessageInfo> currentFolderIndex;
	// Virtual folder index

public:
	friend class Task;
	friend class LoginTask;
	friend class LogoutTask;

	friend class CheckNewMailTask;
	friend class ReadMessageTask;
	friend class ForceCheckNewMailTask;
	friend class CacheMessageTask;
	friend class UpdateTask;
	friend class NoopTask;

	pop3(std::string url, std::string passwd,
	     std::vector<std::string> &certificates,
	     mail::loginCallback *loginCallbackFunc,
	     callback &callback,
	     callback::disconnect &disconnectCallback);

	pop3(const pop3 &); // UNDEFINED
	pop3 &operator=(const pop3 &); // UNDEFINED

	~pop3();

	void logout(callback &callback) override;

	void checkNewMail(callback &callback) override;
	bool hasCapability(std::string capability) override;
	std::string getCapability(std::string capability) override;

	folder *folderFromString(std::string) override;

	void readTopLevelFolders(callback::folderList &callback1,
				 callback &callback2) override;
	void findFolder(std::string folder,
			class callback::folderList &callback1,
			class callback &callback2) override;
	std::string translatePath(std::string path) override;

	void readMessageAttributes(const std::vector<size_t> &messages,
				   MessageAttributes attributes,
				   callback::message &callback) override;

	void readMessageContent(const std::vector<size_t> &messages,
				bool peek,
				enum mail::readMode readType,
				callback::message &callback) override;

	void readMessageContent(size_t messageNum,
				bool peek,
				const mimestruct &msginfo,
				enum mail::readMode readType,
				callback::message &callback) override;

	void readMessageContentDecoded(size_t messageNum,
				       bool peek,
				       const mimestruct &msginfo,
				       callback::message &callback) override;

	size_t getFolderIndexSize() override;
	messageInfo getFolderIndexInfo(size_t) override;

	void saveFolderIndexInfo(size_t,
				 const messageInfo &,
				 callback &) override;
	void getFolderKeywordInfo(size_t, std::set<std::string> &) override;
	void updateKeywords(const std::vector<size_t> &messages,
			    const std::set<std::string> &keywords,
			    bool setOrChange,
			    // false: set, true: see changeTo
			    bool changeTo,
			    callback &cb) override;
private:
	bool genericProcessKeyword(size_t messageNumber,
				   updateKeywordHelper &helper) override;
public:
	void updateFolderIndexFlags(const std::vector<size_t> &messages,
				    bool doFlip,
				    bool enableDisable,
				    const messageInfo &flags,
				    callback &callback) override;

	void updateFolderIndexInfo(callback &) override;
	void removeMessages(const std::vector<size_t> &messages,
			    callback &cb) override;

	void copyMessagesTo(const std::vector<size_t> &messages,
			    folder *copyTo,
			    callback &callback) override;

	void searchMessages(const searchParams &searchInfo,
			    searchCallback &callback) override;

	// Used by pop3Folder:

	void readFolderInfo( callback::folderInfo &callback1,
			     callback &callback2);

	void open(callback &openCallback,
		  callback::folder &folderCallback,
		  mail::snapshot *restoreSnapshot);


	bool reconcileFolderIndex();

private:

	void genericMarkRead(size_t messageNumber) override;

	void genericMessageRead(std::string uid,
				size_t messageNumber,
				bool peek,
				mail::readMode readType,
				callback::message &callback) override;

	void genericMessageSize(std::string uid,
				size_t messageNumber,
				callback::message &callback) override;

	void genericGetMessageFd(std::string uid,
				 size_t messageNumber,
				 bool peek,
				 int &fdRet,
				 callback &callback) override;

	void genericGetMessageStruct(std::string uid,
				     size_t messageNumber,
				     struct rfc2045 *&structRet,
				     callback &callback) override;

	bool genericCachedUid(std::string uid) override;

	// One message is cached to a temp file, and parsed.

	std::string cachedUid;
	int genericTmpFd;
	struct rfc2045 *genericTmpRfcp;

	//
	// Inherited from mail::snapshot::restore:
	//

	void restoreIndex(size_t msgNum,
			  const mail::messageInfo &info) override;
	void restoreKeywords(size_t msgNum,
			     const std::set<std::string> &) override;
	void abortRestore() override;

	bool restoreAborted;


protected:
	// Subclassed by mail::pop3maildrop::pop3acct():

	virtual bool ispop3maildrop();
	virtual void pop3maildropreset();

	virtual mail::addMessage *newDownloadMsg();
	virtual std::string commitDownloadedMsgs();

};

LIBMAIL_END

#endif

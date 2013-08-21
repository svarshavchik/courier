/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mbox_H
#define libmail_mbox_H

#include "libmail_config.h"
#include "mail.H"

#include "unicode/unicode.h"
#include "maildir/maildirkeywords.h"

#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <queue>
#include <map>
#include <set>
#include <vector>

#include "mboxmagictag.H"
#include "generic.H"

#include "namespace.H"

struct ll_mail;

LIBMAIL_START

class file;

/////////////////////////////////////////////////////////////////////////
//
// Legacy mbox driver.
//
//
class mbox : public mail::account, public generic {

	void resumed();
	void handler(std::vector<pollfd> &fds, int &timeout);

	bool calledDisconnected; // True if the disconnect callback was invoked
	bool magicInbox;	// True if this account uses INBOX
	std::string inboxSpoolPath; // Path to the system spool inbox
	std::string inboxMailboxPath; // $HOME/mboxAccount

	std::string rootPath;   // Root path for folders.

	class folder : public mail::folder {

		std::string path;
		std::string name;
		mbox &mboxAccount;

		bool saveHasMessages;
		bool saveHasFolders;

		class closeCallback : public mail::callback {

			void reportProgress(size_t bytesCompleted,
					    size_t bytesEstimatedTotal,
			    
					    size_t messagesCompleted,
					    size_t messagesEstimatedTotal) {}
			//TODO

		public:
			ptr<mbox> origmbox;
			mail::callback &origCallback;
			mail::callback::folder &origFolderCallback;
			std::string origPath;

			closeCallback(mbox *origmboxArg,
				      mail::callback &origCallbackArg,
				      mail::callback::folder
				      &origFolderCallbackarg,
				      std::string origPathArg);
			~closeCallback();

			void success(std::string);
			void fail(std::string);
		};

		class add;

	public:
		friend class mbox;

		folder(std::string pathArg,
		       mbox &mboxArg);
		~folder();

		folder(const mbox::folder &);

		static std::string defaultName(std::string path);

		void sameServerAsHelperFunc() const;

		std::string getName() const;
		std::string getPath() const;

		bool hasMessages() const;
		bool hasSubFolders() const;

		bool isParentOf(std::string path) const;

		void hasMessages(bool);
		void hasSubFolders(bool);


		void getParentFolder(callback::folderList &callback1,
				     callback &callback2) const;

		void readFolderInfo( mail::callback::folderInfo
				     &callback1,
				     mail::callback &callback2) const;

		void readSubFolders( mail::callback::folderList &callback1,
				     mail::callback &callback2) const;

		mail::addMessage *addMessage(mail::callback &callback) const;

		void createSubFolder(std::string name, bool isDirectory,
				     mail::callback::folderList &callback1,
				     mail::callback &callback2) const;

		void create(bool isDirectory,
			    mail::callback &callback) const;

		void destroy(mail::callback &callback, bool destroyDir)
			const;

		void renameFolder(const mail::folder *newParent,
				  std::string newName,
				  mail::callback::folderList &callback1,
				  callback &callback) const;

		mail::folder *clone() const;
		std::string toString() const;

		void open(mail::callback &openCallback,
			  snapshot *restoreSnapshot,
			  mail::callback::folder &folderCallback) const;
	};

	// Special folders

	folder inboxFolder, hierarchyFolder;

	// Stuff gets done by creating a task object, and pushing it into
	// the tasks queue (see below).  The first task in the queue gets
	// its doit() method called.  doit() receives the timeout parameter
	// from mbox::handler(), which it can adjust, if necessary.
	//
	// The doit() method should invoke done() when its done, otherwise
	// it can simply exit, and be re-invoked the next time the handler
	// is invoked.
	//
	// The task's disconnected() method gets invoked if the mbox
	// object is destroyed in mid-stream, or if there's a fatal timeout
	// trying to lock the mboxAccount file.  disconnected() should NOT invoke
	// done(), because that's going to happen anyway as soon as
	// disconnected() wraps up.  disconnected() is present so that
	// the task can invoke any application callbacks the task is
	// responsible for.

	class task {

	protected:
		mbox &mboxAccount;

	public:
		task(mbox &mboxAccount);
		virtual ~task();

		virtual void doit(int &timeout)=0;
		virtual void disconnected()=0;

		virtual void done();
	};

	// TimedTask subclasses from task, to provide a task with a timeout.
	//
	// Subclasses should implement bool doit().  bool doit() should return
	// true if the task succeeded, or failed.  If the task is deferred,
	// the subclass's doit() method should return false.
	//
	// The subclass should call either fail() or success(), which invokes
	// the callback's method, then invokes done().
	//
	// The timedOut() method invokes the callback's fail method, if this
	// tasks times out.  The subclass can override this method to provide
	// some value-added functionality.
	//

	class TimedTask : public mbox::task {

		time_t timeout;
		time_t nextTry;

	protected:
		mail::callback &callback;
	public:
		TimedTask(mbox &mboxAccount,
			  mail::callback &callbackArg, int timeoutArg=60);
		~TimedTask();

		void doit(int &timeout);

		void fail(std::string errmsg);
		virtual bool doit()=0;
		virtual void timedOut();
		virtual void disconnected();
	};

	// This is a wrapper for liblock's locking functions

	class lock {
		struct ll_mail *ll;

		int fd;

		bool readOnlyLock;

		lock();

	public:
		lock(std::string filename); // File to create an mboxAccount lock for.
		~lock();

		bool operator()(bool readOnly=false);
		// Attempt to lock this mboxAccount file, via liblock.
		// Set readOnly to true if a read-only lock is permissible.

		int getfd() const { return fd; }
		// Success - return the file descriptor of locked file.

		bool readOnly() const { return readOnlyLock; }
		// Indicator whether the file was locked in read-only mode.


		lock *copy();
		// Copy this lock structure to another object, used for
		// optimizing consecutive parallel locks.

	};

	class StatusTask;
	class sighandler;
	class OpenTask;
	class CheckNewMailTask;

	class LockTask;
	class GenericReadTask;
	class GenericGetMessageTask;
	class ExpungeTask;

	class MultiLock;
	class MultiLockGenericMessageRead;
	class MultiLockGenericAttributes;
	class MultiLockRelease;
	class RenameTask;

	std::queue<task *> tasks;

	void installTask(task *);

	// The currently open folder.

	std::string currentFolder;
	bool currentFolderReadOnly;	// Folder opened in readonly mode.

	off_t folderSavedSize;
	time_t folderSavedTimestamp;
	// Detect if someone else modified the folder file.

	std::string multiLockFilename;
	lock *multiLockLock;

	bool folderDirty;
	// Set to true when message metadata has been modified, but the
	// folder file has not been updated to reflect these changes.

	bool newMessages;
	// Set when index is updated from folderMessageIndex, and new messages
	// were found.

	////////////////////////////////////////////////////////
	//
	// The actual parsed contents of the folder:

	struct mboxMessageIndex {

		off_t startingPos;	// Starting offset
		time_t internalDate;	// Synthesized message arrival date
		mboxMagicTag tag;	// The synthesized UID.
	};

	mail::keywords::Hashtable keywordHashtable;
	std::vector<mboxMessageIndex> folderMessageIndex; // The folder

	std::map<std::string, size_t> uidmap;	// Look up index from uid.

	// Generic interface caches the temporary file with the most recently
	// opened message:

	std::string cachedMessageUid;	// The cached uid
	struct rfc2045 *cachedMessageRfcp;
	FILE *cachedMessageFp;

	void resetFolder();

	mail::callback::folder *currentFolderCallback;

	// The application sees the following index:

	std::vector<mail::messageInfo> index;

	// Update the mboxAccount file.  This is where everything happens.

	bool scan(file &scanFile,
		  file *saveFile,
		  bool reopening,
		  std::set<std::string> *deleteMsgs,
		  bool rewrite,
		  mail::callback *progress);

	void checkNewMail();

public:
	friend class folder;
	friend class task;
	friend class TimedTask;
	friend class StatusTask;
	friend class OpenTask;
	friend class CheckNewMailTask;
	friend class RenameTask;

	friend class LockTask;
	friend class GenericReadTask;
	friend class GenericGetMessageTask;
	friend class ExpungeTask;
	friend class MultiLock;
	friend class MultiLockGenericMessageRead;
	friend class MultiLockGenericAttributes;
	friend class MultiLockRelease;

	mbox(bool magicInboxArg,
		   std::string folderRootArg, mail::callback &callback,
		   mail::callback::disconnect &disconnect_callback);
	~mbox();

	void logout(mail::callback &callback);
	void checkNewMail(mail::callback &callback);

	bool hasCapability(std::string capability);
	std::string getCapability(std::string capability);

	mail::folder *folderFromString(std::string);

	void readTopLevelFolders(mail::callback::folderList &callback1,
				 mail::callback &callback2);

	void findFolder(std::string folder,
			mail::callback::folderList &callback1,
			mail::callback &callback2);
	std::string translatePath(std::string path);

	static std::string translatePathCommon(std::string path,
					       const char *verbotten,
					       const char *sep);

	void readMessageAttributes(const std::vector<size_t> &messages,
				   MessageAttributes attributes,
				   mail::callback::message &callback);

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

	size_t getFolderIndexSize();
	mail::messageInfo getFolderIndexInfo(size_t);

	void saveFolderIndexInfo(size_t,
					 const mail::messageInfo &,
					 mail::callback &);

	void updateFolderIndexFlags(const std::vector<size_t> &messages,
				    bool doFlip,
				    bool enableDisable,
				    const mail::messageInfo &flags,
				    mail::callback &callback);

	void updateFolderIndexInfo(mail::callback &);


	void updateKeywords(const std::vector<size_t> &messages,
			    const std::set<std::string> &keywords,
			    bool setOrChange,
			    // false: set, true: see changeTo
			    bool changeTo,
			    callback &cb);
	void getFolderKeywordInfo(size_t, std::set<std::string> &);

	bool genericProcessKeyword(size_t messageNumber,
				   generic::updateKeywordHelper &helper);

	void removeMessages(const std::vector<size_t> &messages,
			    callback &cb);

	void copyMessagesTo(const std::vector<size_t> &messages,
			    mail::folder *copyTo,
			    mail::callback &callback);

	void searchMessages(const class mail::searchParams &searchInfo,
			    class mail::searchCallback &callback);

	bool verifyUid(std::string uid, size_t &messageNumber,
		       mail::callback &callback);


	void genericMessageRead(std::string uid,
				size_t messageNumber,
				bool peek,
				mail::readMode,
				mail::callback::message &callback);

	void genericMessageSize(std::string uid,
				size_t messageNumber,
				mail::callback::message &callback);

	void genericGetMessageFd(std::string uid,
				 size_t messageNumber,
				 bool peek,
				 int &fdRet,
				 mail::callback &callback);

	void genericGetMessageStruct(std::string uid,
				     size_t messageNumber,
				     struct rfc2045 *&structRet,
				     mail::callback &callback);

	bool genericCachedUid(std::string uid);

	void genericGetMessageFdStruct(std::string uid,
				       size_t messageNumber,
				       bool peek,
				       int &fdRet,
				       struct rfc2045 *&structret,
				       mail::callback &callback);
	void genericMarkRead(size_t messageNumber);
};

LIBMAIL_END

#endif

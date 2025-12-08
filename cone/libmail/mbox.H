/*
** Copyright 2002, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mbox_H
#define libmail_mbox_H

#include "libmail_config.h"
#include "mail.H"

#include <courier-unicode.h>
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

	void resumed() override;
	void handler(std::vector<pollfd> &fds, int &timeout) override;

	// True if the disconnect callback was invoked
	bool calledDisconnected{false};
	bool magicInbox{false};	// True if this account uses INBOX
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
					    size_t messagesEstimatedTotal)
			override {}
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

			void success(std::string) override;
			void fail(std::string message) override;
		};

		class add;

	public:
		friend class mbox;

		folder(std::string pathArg,
		       mbox &mboxArg);
		~folder();

		folder(const mbox::folder &);

		static std::string defaultName(std::string path);

		void sameServerAsHelperFunc() const override;

		std::string getName() const override;
		std::string getPath() const override;

		bool hasMessages() const override;
		bool hasSubFolders() const override;

		bool isParentOf(std::string path) const override;

		void hasMessages(bool) override;
		void hasSubFolders(bool) override;


		void getParentFolder(callback::folderList &callback1,
				     callback &callback2) const override;

		void readFolderInfo( mail::callback::folderInfo
				     &callback1,
				     mail::callback &callback2) const override;

		void readSubFolders( mail::callback::folderList &callback1,
				     mail::callback &callback2) const override;

		mail::addMessage *addMessage(mail::callback &callback) const override;

		void createSubFolder(std::string name, bool isDirectory,
				     mail::callback::folderList &callback1,
				     mail::callback &callback2) const override;

		void create(bool isDirectory,
			    mail::callback &callback) const override;

		void destroy(mail::callback &callback, bool destroyDir)
			const override;

		void renameFolder(const mail::folder *newParent,
				  std::string newName,
				  mail::callback::folderList &callback1,
				  callback &callback) const override;

		mail::folder *clone() const override;
		std::string toString() const override;

		void open(mail::callback &openCallback,
			  snapshot *restoreSnapshot,
			  mail::callback::folder &folderCallback)
			const override;
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

		void doit(int &timeout) override;

		void fail(std::string errmsg);
		virtual bool doit()=0;
		virtual void timedOut();
		virtual void disconnected() override;
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
	bool currentFolderReadOnly{false}; // Folder opened in readonly mode.

	off_t folderSavedSize{0};
	time_t folderSavedTimestamp{0};
	// Detect if someone else modified the folder file.

	std::string multiLockFilename;
	lock *multiLockLock{nullptr};

	bool folderDirty{false};
	// Set to true when message metadata has been modified, but the
	// folder file has not been updated to reflect these changes.

	bool newMessages{false};
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
	std::shared_ptr<rfc2045::entity> cachedMessageRfcp;
	std::shared_ptr<rfc822::fdstreambuf> cachedMessageFp;

	void resetFolder();

	mail::callback::folder *currentFolderCallback{nullptr};

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

	void logout(mail::callback &callback) override;
	void checkNewMail(mail::callback &callback) override;

	bool hasCapability(std::string capability) override;
	std::string getCapability(std::string capability) override;

	mail::folder *folderFromString(std::string) override;

	void readTopLevelFolders(mail::callback::folderList &callback1,
				 mail::callback &callback2) override;

	void findFolder(std::string folder,
			mail::callback::folderList &callback1,
			mail::callback &callback2) override;
	std::string translatePath(std::string path) override;

	static std::string translatePathCommon(std::string path,
					       const char *sep);

	void readMessageAttributes(const std::vector<size_t> &messages,
				   MessageAttributes attributes,
				   mail::callback::message &callback) override;

	void readMessageContent(const std::vector<size_t> &messages,
				bool peek,
				enum mail::readMode readType,
				mail::callback::message &callback) override;

	void readMessageContent(size_t messageNum,
				bool peek,
				const mimestruct &msginfo,
				enum mail::readMode readType,
				mail::callback::message &callback) override;

	void readMessageContentDecoded(size_t messageNum,
				       bool peek,
				       const mimestruct &msginfo,
				       mail::callback::message &callback)
		override;

	size_t getFolderIndexSize() override;
	mail::messageInfo getFolderIndexInfo(size_t) override;

	void saveFolderIndexInfo(size_t,
				 const mail::messageInfo &,
				 mail::callback &) override;

	void updateFolderIndexFlags(const std::vector<size_t> &messages,
				    bool doFlip,
				    bool enableDisable,
				    const mail::messageInfo &flags,
				    mail::callback &callback) override;

	void updateFolderIndexInfo(mail::callback &) override;


	void updateKeywords(const std::vector<size_t> &messages,
			    const std::set<std::string> &keywords,
			    bool setOrChange,
			    // false: set, true: see changeTo
			    bool changeTo,
			    callback &cb) override;
	void getFolderKeywordInfo(size_t, std::set<std::string> &) override;

	bool genericProcessKeyword(size_t messageNumber,
				   generic::updateKeywordHelper &helper) override;

	void removeMessages(const std::vector<size_t> &messages,
			    callback &cb) override;

	void copyMessagesTo(const std::vector<size_t> &messages,
			    mail::folder *copyTo,
			    mail::callback &callback) override;

	void searchMessages(const class mail::searchParams &searchInfo,
			    class mail::searchCallback &callback) override;

	bool verifyUid(std::string uid, size_t &messageNumber,
		       mail::callback &callback);


	void genericMessageRead(std::string uid,
				size_t messageNumber,
				bool peek,
				mail::readMode,
				mail::callback::message &callback) override;

	void genericMessageSize(std::string uid,
				size_t messageNumber,
				mail::callback::message &callback) override;

	void genericGetMessageFd(std::string uid,
				 size_t messageNumber,
				 bool peek,
				 std::shared_ptr<rfc822::fdstreambuf> &fdRet,
				 mail::callback &callback) override;

	void genericGetMessageStruct(
		std::string uid,
		size_t messageNumber,
		std::shared_ptr<rfc2045::entity> &structRet,
		mail::callback &callback) override;

	bool genericCachedUid(std::string uid) override;

	void genericGetMessageFdStruct(
		std::string uid,
		size_t messageNumber,
		bool peek,
		std::shared_ptr<rfc822::fdstreambuf> &fdRet,
		std::shared_ptr<rfc2045::entity> &structRet,
		mail::callback &callback) override;
	void genericMarkRead(size_t messageNumber) override;
};

LIBMAIL_END

#endif

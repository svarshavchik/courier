/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntp_H
#define libmail_nntp_H

#include "libmail_config.h"
#include	"mail.H"
#include	<sys/types.h>

#include	"maildir/maildirkeywords.h"

#include	"logininfo.H"
#include	"fd.H"
#include	"generic.H"
#include	"search.H"
#include	<stdio.h>
#include	<time.h>
#include	<string>
#include	<map>
#include	<list>
#include	<vector>

///////////////////////////////////////////////////////////////////////////
//
// An NNTP implementation
//

LIBMAIL_START

//////////////////////////////////////////////////////////////////////////////

class nntp : public fd, public generic {

public:
	class newsrc;
	typedef unsigned long msgnum_t;

private:

	time_t timeoutSetting;
	time_t autologoutSetting;

	loginInfo nntpLoginInfo;

	loginInfo savedLoginInfo;

	time_t inactivityTimeout;

	callback::folder *folderCallback;

	void resumed();
	void handler(std::vector<pollfd> &fds, int &timeout);
	int socketRead(const std::string &readbuffer);

	void disconnect(const char *reason);

	// Superclass of NNTP tasks.  Methods create subclasses of Task
	// objects, and push them to the tasks queue.  Replies from the
	// NNTP server are routed to the foremost Task on the queue.

	class Task {

	protected:
		callback *callbackPtr;	// App callback.
		nntp * volatile myserver;
		// Marked volatile due to destructor monkey business

	public:
		time_t defaultTimeout;

		virtual void done();
		// Task completed, start the next task on the queue,
		// and delete this Task object.

		void resetTimeout();

		virtual void success(std::string);
		virtual void fail(std::string);
		// success/fail for use by subclasses.  Invoke the callback
		// method, then done.

		Task(callback *callbackArg,
		     nntp &myserverArg);
		virtual ~Task();

		virtual int getTimeout();
		// How long before this task times out.

		virtual void emptyQueue();
		// After removing this task, the task queue is now empty

		virtual void serverResponse(const char *message)=0;
		// Process a line of text from the server

		virtual void disconnected(const char *reason);
		// Server has disconnected.
		// The default implementation takes this task off the queue,
		// calls the next Task's disconnect method, then deletes
		// itself.

		virtual void installedTask();
		// This task is now at the front of the queue
	};

	std::list<Task *> tasks;

	std::vector<std::string> newgroups;
	bool hasNewgroups;

	std::map<std::string, newsrc> cachedNewsrc;
	bool didCacheNewsrc;

	void cacheNewsrc();
	bool updateCachedNewsrc();
	void discardCachedNewsrc();
	bool updateOpenedNewsrc(newsrc &);

	void createNewsrc(newsrc &);

	class cachedNewsrcSort;

	class folder;
	class add;
	class LoggedInTask;
	class LoginTask;
	class LogoutTask;
	class ListActiveTask;
	class GroupTask;
	class GroupInfoTask;
	class GroupOpenTask;
	class FetchTask;
	class FetchTaskBase;
	class CacheMessageTask;
	class CacheTask;
	class XoverTask;
	class PostTask;
	class CheckNewTask;

	class XpatTask;
	class XpatTaskCallback;

	std::string newsrcFilename;	// .newsrc file

	std::string openedGroup; // Group we have logically opened

	// Here's the index of an opened group.  'cause Usenet groups can
	// be fairly large, we try to be extra skimpy on memory usage.

	class nntpMessageInfo {
	public:
		nntpMessageInfo();
		~nntpMessageInfo();
		msgnum_t msgNum;

		mail::keywords::Message keywords;

		unsigned char msgFlag;
#define IDX_DELETED 1
#define IDX_FLAGGED 2

#define IDX_SEARCH  128  // Flag used to mark msgs found by search
	};

private:
	static bool equalMsgNums(nntpMessageInfo a,
				 nntpMessageInfo b);

public:
	mail::keywords::Hashtable keywordHashtable;
	std::vector<nntpMessageInfo> index;

	msgnum_t loWatermark, hiWatermark; // Saved from last GROUP

	std::string serverGroup; // Group actually open on the server

	callback::disconnect *disconnectCallback;

	void installTask(Task *);
public:

	friend class Task;
	friend class LoggedInTask;
	friend class folder;
	friend class LogoutTask;
	friend class ListActiveTask;
	friend class GroupTask;
	friend class GroupInfoTask;
	friend class GroupOpenTask;
	friend class FetchTask;
	friend class FetchTaskBase;
	friend class CacheMessageTask;
	friend class CacheTask;
	friend class XoverTask;
	friend class add;
	friend class PostTask;
	friend class CheckNewTask;
	friend class XpatTask;
	friend class XpatTaskCallback;

	nntp(std::string url, std::string passwd,
	     std::vector<std::string> &certificates,
	     std::string newsrcFilename,
	     mail::loginCallback *loginCallbackFunc,
	     callback &callback,
	     callback::disconnect &disconnectCallbackArg);

	nntp(const nntp &); // UNDEFINED
	nntp &operator=(const nntp &); // UNDEFINED

	~nntp();

	void logout(callback &callback);

	void checkNewMail(callback &callback);
	bool hasCapability(std::string capability);
	std::string getCapability(std::string capability);

	mail::folder *folderFromString(std::string);

	void readTopLevelFolders(callback::folderList &callback1,
				 callback &callback2);
	void findFolder(std::string folder,
			class callback::folderList &callback1,
			class callback &callback2);
	std::string translatePath(std::string path);

	mail::folder *getSendFolder(const smtpInfo &info,
				    const mail::folder *folder,
				    std::string &errmsg);

	void readMessageAttributes(const std::vector<size_t> &messages,
				   MessageAttributes attributes,
				   callback::message &callback);

	void readMessageContent(const std::vector<size_t> &messages,
				bool peek,
				enum mail::readMode readType,
				callback::message &callback);

	void readMessageContent(size_t messageNum,
				bool peek,
				const mimestruct &msginfo,
				enum mail::readMode readType,
				callback::message &callback);

	void readMessageContentDecoded(size_t messageNum,
				       bool peek,
				       const mimestruct &msginfo,
				       callback::message &callback);

	size_t getFolderIndexSize();
	messageInfo getFolderIndexInfo(size_t);

	void saveFolderIndexInfo(size_t,
				 const messageInfo &,
				 callback &);

	void updateFolderIndexFlags(const std::vector<size_t> &messages,
				    bool doFlip,
				    bool enableDisable,
				    const messageInfo &flags,
				    callback &callback);

	void updateFolderIndexInfo(callback &);

	void getFolderKeywordInfo(size_t, std::set<std::string> &);

	void updateKeywords(const std::vector<size_t> &messages,
			    const std::set<std::string> &keywords,
			    bool setOrChange,
			    // false: set, true: see changeTo
			    bool changeTo,
			    callback &cb);
private:
	bool genericProcessKeyword(size_t msgNum,
				   updateKeywordHelper &helper);
public:
	void removeMessages(const std::vector<size_t> &messages,
			    callback &cb);

	void copyMessagesTo(const std::vector<size_t> &messages,
			    mail::folder *copyTo,
			    callback &callback);

	void searchMessages(const searchParams &searchInfo,
			    searchCallback &callback);

	void saveSnapshot();
private:
	void searchMessagesXpat(std::string hdr, std::string srch,
				bool searchNot,
				searchParams::Scope searchScope,
				size_t rangeLo, size_t rangeHi,
				searchCallback &callback);

	bool fixGenericMessageNumber(std::string uid, size_t &messageNumber);

	void genericMessageRead(std::string uid,
				size_t messageNumber,
				bool peek,
				mail::readMode readTypeArg,
				callback::message &callback);

	void genericMessageSize(std::string uid,
				size_t messageNumber,
				callback::message &callback);

	void genericGetMessageFd(std::string uid,
				 size_t messageNumber,
				 bool peek,
				 int &fdRet,
				 callback &callback);
	void genericMarkRead(size_t messageNumber);

	void genericGetMessageStruct(std::string uid,
				     size_t messageNumber,
				     struct rfc2045 *&structRet,
				     callback &callback);

	bool genericCachedUid(std::string uid);

	// One message is cached to a temp file, and parsed.

	std::string cachedUid;
	FILE *genericTmpFp;
	struct rfc2045 *genericTmpRfcp;

	void cleartmp();
};

LIBMAIL_END

#endif

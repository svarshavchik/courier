/*
** Copyright 2003-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_tmpaccount_H
#define libmail_tmpaccount_H

#include "libmail_config.h"
#include "mail.H"
#include "generic.H"
#include "addmessage.H"

#include "rfc2045/rfc2045.h"
#include <stdio.h>

LIBMAIL_START

//
// The tmp: driver - a driver that uses a temporary file that holds exactly
// one message.
// The driver provides a single folder, INBOX.  Adding a message to the
// folder automatically expunges the previous message.
//

class tmpaccount : public account, public generic {

	void resumed();
	void handler(std::vector<pollfd> &fds, int &timeout);

	callback::disconnect *disconnect_callback;
	callback::folder *folder_callback;
	FILE *f;
	struct rfc2045 *rfc2045p;
	messageInfo fInfo;

	void disconnect(const char *);

	class folder : public mail::folder {

		tmpaccount *account;

		static void na(callback &);
	public:
		folder(tmpaccount *);
		~folder();

		bool isParentOf(std::string path) const;

		void hasMessages(bool);
		void hasSubFolders(bool);
		void readFolderInfo( callback::folderInfo
				     &callback1,
				     callback &callback2) const;
		void getParentFolder(callback::folderList &callback1,
				     callback &callback2) const;
		void readSubFolders( callback::folderList &callback1,
				     callback &callback2) const;

		mail::addMessage *addMessage(callback &callback) const;

		void createSubFolder(std::string name, bool isDirectory,
				     callback::folderList
				     &callback1,
				     callback &callback2) const;
		void create(bool isDirectory,
			    callback &callback) const;

		void destroy(callback &callback, bool destroyDir) const;

		void renameFolder(const mail::folder *newParent,
				  std::string newName,
				  callback::folderList &callback1,
				  callback &callback2) const;

		folder *clone() const;

		std::string toString() const;

		void open(callback &openCallback,
			  snapshot *restoreSnapshot,
			  callback::folder &folderCallback) const;

		void sameServerAsHelperFunc() const;

		std::string getName() const;
		std::string getPath() const;
		bool hasMessages() const;
		bool hasSubFolders() const;
	};

	class add : public addMessage {

		FILE *newFile;
		struct rfc2045 *rfc2045p;

		tmpaccount *account;
		mail::callback &cb;
	public:
		add(tmpaccount *accountArg, mail::callback &cbArg);
		~add();
		virtual void saveMessageContents(std::string);
		virtual void go();
		virtual void fail(std::string errmsg);
	};

public:

	friend class mail::tmpaccount::folder;
	friend class mail::tmpaccount::add;

	tmpaccount(callback::disconnect &disconnect_callbackArg);
	virtual ~tmpaccount();

	void logout(callback &callback);
	void checkNewMail(callback &callback);

	bool hasCapability(std::string capability);
	std::string getCapability(std::string capability);
	mail::folder *folderFromString(std::string);
	void readTopLevelFolders(callback::folderList &callback1,
				 callback &callback2);
	void findFolder(std::string path,
			callback::folderList &callback1,
			callback &callback2);
	std::string translatePath(std::string path);


	void readMessageAttributes(const std::vector<size_t> &messages,
				   MessageAttributes attributes,
				   callback::message &callback);
	void readMessageContent(const std::vector<size_t> &messages,
				bool peek,
				enum mail::readMode readType,
				callback::message &callback);

	void readMessageContent(size_t messageNum,
				bool peek,
				const class mimestruct &msginfo,
				enum mail::readMode readType,
				callback::message &callback);

	void readMessageContentDecoded(size_t messageNum,
				       bool peek,
				       const class mimestruct
				       &msginfo,
				       callback::message &callback);
	size_t getFolderIndexSize();

	messageInfo getFolderIndexInfo(size_t);
	void saveFolderIndexInfo(size_t messageNum,
				 const messageInfo &msgInfo,
				 callback &callback);
	void updateFolderIndexFlags(const std::vector<size_t> &messages,
				    bool doFlip,
				    bool enableDisable,
				    const messageInfo &flags,
				    callback &callback);

	void updateFolderIndexInfo(callback &);
	void removeMessages(const std::vector<size_t> &messages,
			    callback &cb);
	void copyMessagesTo(const std::vector<size_t> &messages,
			    mail::folder *copyTo,
			    callback &callback);
	void searchMessages(const searchParams &searchInfo,
			    searchCallback &callback);




	void genericMessageRead(std::string uid,
				size_t messageNumber,
				bool peek,
				mail::readMode readType,
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

	void genericGetMessageFdStruct(std::string uid,
				       size_t messageNumber,
				       bool peek,
				       int &fdRet,
				       struct rfc2045 *&structret,

				       mail::callback &callback);

	bool genericCachedUid(std::string uid);

	void genericMarkRead(size_t messageNumber);
};

LIBMAIL_END

#endif

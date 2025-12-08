/*
** Copyright 2003-2004, S. Varshavchik.
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

	void resumed() override;
	void handler(std::vector<pollfd> &fds, int &timeout) override;

	callback::disconnect *disconnect_callback;
	callback::folder *folder_callback;

	std::shared_ptr<rfc822::fdstreambuf> f;
	std::shared_ptr<rfc2045::entity> rfc2045p;
	messageInfo fInfo;

	void disconnect(const char *);

	class folder : public mail::folder {

		tmpaccount *account;

		static void na(callback &);
	public:
		folder(tmpaccount *);
		~folder();

		bool isParentOf(std::string path) const override;

		void hasMessages(bool) override;
		void hasSubFolders(bool) override;
		void readFolderInfo( callback::folderInfo
				     &callback1,
				     callback &callback2) const override;
		void getParentFolder(callback::folderList &callback1,
				     callback &callback2) const override;
		void readSubFolders( callback::folderList &callback1,
				     callback &callback2) const override;

		mail::addMessage *addMessage(callback &callback) const override;

		void createSubFolder(std::string name, bool isDirectory,
				     callback::folderList
				     &callback1,
				     callback &callback2) const override;
		void create(bool isDirectory,
			    callback &callback) const override;

		void destroy(callback &callback, bool destroyDir) const override;

		void renameFolder(const mail::folder *newParent,
				  std::string newName,
				  callback::folderList &callback1,
				  callback &callback2) const override;

		folder *clone() const override;

		std::string toString() const override;

		void open(callback &openCallback,
			  snapshot *restoreSnapshot,
			  callback::folder &folderCallback) const override;

		void sameServerAsHelperFunc() const override;

		std::string getName() const override;
		std::string getPath() const override;
		bool hasMessages() const override;
		bool hasSubFolders() const override;
	};

	class add : public addMessage {

		std::shared_ptr<rfc822::fdstreambuf> newFile;
		rfc2045::entity_parser<false> rfc2045p;

		tmpaccount *account;
		mail::callback &cb;
	public:
		add(tmpaccount *accountArg, mail::callback &cbArg);
		~add();
		virtual void saveMessageContents(std::string) override;
		virtual void go() override;
		virtual void fail(std::string errmsg) override;
	};

public:

	friend class mail::tmpaccount::folder;
	friend class mail::tmpaccount::add;

	tmpaccount(callback::disconnect &disconnect_callbackArg);
	virtual ~tmpaccount();

	void logout(callback &callback) override;
	void checkNewMail(callback &callback) override;

	bool hasCapability(std::string capability) override;
	std::string getCapability(std::string capability) override;
	mail::folder *folderFromString(std::string) override;
	void readTopLevelFolders(callback::folderList &callback1,
				 callback &callback2) override;
	void findFolder(std::string path,
			callback::folderList &callback1,
			callback &callback2) override;
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
				const class mimestruct &msginfo,
				enum mail::readMode readType,
				callback::message &callback) override;

	void readMessageContentDecoded(size_t messageNum,
				       bool peek,
				       const class mimestruct
				       &msginfo,
				       callback::message &callback) override;
	size_t getFolderIndexSize() override;

	messageInfo getFolderIndexInfo(size_t) override;
	void saveFolderIndexInfo(size_t messageNum,
				 const messageInfo &msgInfo,
				 callback &callback) override;
	void updateFolderIndexFlags(const std::vector<size_t> &messages,
				    bool doFlip,
				    bool enableDisable,
				    const messageInfo &flags,
				    callback &callback) override;

	void updateFolderIndexInfo(callback &) override;
	void removeMessages(const std::vector<size_t> &messages,
			    callback &cb) override;
	void copyMessagesTo(const std::vector<size_t> &messages,
			    mail::folder *copyTo,
			    callback &callback) override;
	void searchMessages(const searchParams &searchInfo,
			    searchCallback &callback) override;




	void genericMessageRead(std::string uid,
				size_t messageNumber,
				bool peek,
				mail::readMode readType,
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

	void genericGetMessageFdStruct(
		std::string uid,
		size_t messageNumber,
		bool peek,
		std::shared_ptr<rfc822::fdstreambuf> &fdRet,
		std::shared_ptr<rfc2045::entity> &structRet,
		mail::callback &callback) override;

	bool genericCachedUid(std::string uid) override;

	void genericMarkRead(size_t messageNumber) override;
};

LIBMAIL_END

#endif

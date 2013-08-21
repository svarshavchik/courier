/*
** Copyright 2002-2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_sync_H
#define libmail_sync_H

#include "libmail_config.h"
#include <string>
#include <vector>
#include <list>
#include <set>
#include <iostream>

////////////////////////////////////////////////////////////////////////
//
// A synchronous libmail.a interface.
//
// USAGE:
//
//      bool ok=mail->login(string url, string password);
//
//      mail->logout();
//
//      mail::ACCOUNT::FolderList folderList;
//
//      bool ok=mail->getTopLevelFolders(folderList);
//
//      size_t folderCnt=folderList.size();
//
//      const mail::folder *folder=folderList[n];
//
//      bool ok=mail->getSubFolders(folder, folderList);
//
//      bool ok=mail->getParentFolder(folder, folderList);
//
//      string folderStr=folder->toString();
//
//      string folderPath=folder->getPath();
//
//      mail::folder *f=mail->getFolderFromString(folderStr);
//
//      mail::folder *p=mail->getFolderFromPath(folderPath);
//
//	std::string path=mail->translatePath(std::string pathName);
//
//	bool ok=mail->createFolder(const mail::folder *, bool createDirectory);
//
//      mail::folder *f=mail->createFolder(const mail::folder *parent,
//                                      string Name, bool createDirectory);
//
//	mail::folder *f=mail->renameFolder(mail::folder *oldFolder,
//					mail::folder *newParent,
//					string newName);
//                                 
//      bool ok=mail->deleteFolder(f, bool deleteDirectory);
//
//      mail::ACCOUNT::FolderInfo folderInfo;
//
//      bool ok=readFolderInfo(f, folderInfo);
//
//      bool ok=mail->openFolder(f, snapshot);
//
//      size_t nMsgs=mail->getFolderIndexSize();
//
//	mail::messageInfo getFolderIndexInfo(size_t msgNum);
//
//	void getFolderKeywordInfo(size_t msgNum, set<string> &keywords);
//
//	mail::messageInfo msgInfo=mail->readFolderInfo(size_t msgNum);
//
//      vector<mail::xenvelope> envelopeList;
//      vector<mail::mimestruct> structureList;
//      vector<size_t> msgNumVector;
//
//      bool ok=getMessageEnvelope(msgNumVector, envelopeList);
//
//      bool ok=getMessageStructure(msgNumVector, structureList);
//
//	mail::xenvelope env=getMessageEnvelope(size_t messageNum);
//	mail::mimestruct str=getMessageStructure(size_t messageNum);
//
//      bool ok=mail->saveFolderIndexInfo(msgNum, msgInfo);
//
//	bool updateKeywords(const std::vector<size_t> &messages,
//			    const std::set<std::string> &keywords,
//			    bool setOrChange,
//			    // false: set, true: see changeTo
//			    bool changeTo);
//	bool updateKeywords(const std::vector<size_t> &messages,
//			    const std::vector<std::string> &keywords,
//			    bool setOrChange,
//			    // false: set, true: see changeTo
//			    bool changeTo);
//	bool updateKeywords(const std::vector<size_t> &messages,
//			    const std::list<std::string> &keywords,
//			    bool setOrChange,
//			    // false: set, true: see changeTo
//			    bool changeTo);
//      bool ok=mail->updateFolderIndexInfo();
//
//      bool ok=mail->checkNewMail();
//
//      bool flag=mail->folderModified();
//
//	bool ok=mail->removeMessages(msgNumVector);
//
//	bool ok=mail->updateFolderIndexFlags(msgNumVector, bool doFlip,
//					     bool enableDisable,
//					     mail::messageInfo &flags);
//
//      bool ok=mail->copyMessagesTo(msgNumVector, mail::folder *copyTo);

//      bool ok=mail->moveMessagesTo(msgNumVector, mail::folder *copyTo);
//
//      bool ok=mail->searchMessages(const mail::searchParams &searchInfo,
//                             msgNumVector);
//
//      class myStore : public mail::ACCOUNT::Store {
//
//            ....
//
//      }
//
//      myStore myStoreCallback;
//
//      bool ok=mail->getMessageContent(msgNumVector, bool peek,
//                             bool getHeaders,
//                             bool getContent, myStoreCallback);
//
//      bool ok=mail->getMessageContent(msgNum, bool peek,
//                             mail::mimestruct &part,
//                             bool getHeaders,
//                             bool getContent, myStoreCallback);
//
//      bool ok=mail->getMessageContentDecoded(msgNum, bool peek,
//                                    mail::mimestruct &part,
//                                    myStoreCallback);
//
//      class getMyMessageContents : public mail::addMessagePull {
//
//       ....
//
//      }
//
//      getMyMessageContents contents;
//
//      bool ok=mail->addMessage(f, contents);
//
//      mail::smtpInfo sendMessageInfo;
//
//      bool sent=mail->send(sendMessageInfo, folder, contents);
//
//      string errmsg=mail->errmsg;
//
//      bool disconnected=mail->disconnected();

LIBMAIL_START

class account;

class xenvelope;
class mimestruct;
class addMessagePull;

class ACCOUNT {

public:
	class callback;
	class disconnectCallback;
	class readFoldersCallback;
	class readMessageCallback;
	class SearchCallback;

	class Progress {
		bool reportingProgress;
		ACCOUNT *myAccount;
	public:
		friend class ACCOUNT;
		friend class ACCOUNT::callback;

		Progress(ACCOUNT *);
		virtual void operator()(size_t bytesCompleted,
					size_t bytesEstimatedTotal,

					size_t messagesCompleted,
					size_t messagesEstimatedTotal)=0;
		virtual ~Progress();
	};

	friend class Progress;

private:
	account *ptr;

	Progress *currentProgressReporter;

	class disconnectCallback *imap_disconnect;

	bool eventloop(callback &);

	class FolderCallback : public mail::callback::folder {
	public:
		ACCOUNT *account;

		FolderCallback();
		~FolderCallback();
	private:
		void newMessages();
		void messagesRemoved(std::vector< std::pair<size_t,
				     size_t> > &);
		void messageChanged(size_t n);
	} folderCallback;

	bool folderModifiedFlag;

public:

	class FolderList {
		std::vector<class mail::folder *> folders;

	public:
		FolderList();
		~FolderList();

		FolderList(const FolderList &);
		FolderList &operator=(const FolderList &);

		void append(const mail::folder *);

		typedef std::vector<class mail::folder *>::iterator iterator;

		iterator begin()
		{
			return iterator(folders.begin());
		}

		iterator end()
		{
			return iterator(folders.end());
		}

		size_t size() const
		{
			return folders.size();
		}

		const mail::folder *operator[](size_t n) const
		{
			return n < 0 || n >= folders.size()
				? NULL:folders[n];
		}
	};

	class Store {
	public:
		Store();
		virtual ~Store();

		virtual void store(size_t, std::string)=0;
	};

	class FolderInfo {
	public:
		FolderInfo();
		~FolderInfo();

		size_t messageCount;
		size_t unreadCount;
		bool fastInfo;
	};

	std::string errmsg;

	ACCOUNT();
	virtual ~ACCOUNT();

	std::string getErrmsg() { return errmsg; }

	bool login( mail::account::openInfo openInfoArg);

	void logout();

	bool getTopLevelFolders(FolderList &list);
	bool getSubFolders(const mail::folder *folder, FolderList &list);
	bool getParentFolder(const mail::folder *folder, FolderList &list);
	mail::folder *getFolderFromString(std::string);
	mail::folder *getFolderFromPath(std::string);
	std::string translatePath(std::string);

	bool createFolder(const mail::folder *, bool);

	mail::folder *createFolder(const mail::folder *, std::string, bool);

	mail::folder *renameFolder(const mail::folder *,
				   const mail::folder *, std::string);

	bool deleteFolder(const mail::folder *, bool);

	bool openFolder(const mail::folder *folder,
			mail::snapshot *restoreSnapshot);
	bool readFolderInfo(const mail::folder *folder, FolderInfo &info);

	size_t getFolderIndexSize();
	class mail::messageInfo getFolderIndexInfo(size_t);

	void getFolderKeywordInfo(size_t msgNum,
				  std::set<std::string> &keywords);

	bool saveFolderIndexInfo(size_t messageNum,
				 const mail::messageInfo &newInfo);

	bool updateKeywords(const std::vector<size_t> &messages,
			    const std::set<std::string> &keywords,
			    bool setOrChange,
			    // false: set, true: see changeTo
			    bool changeTo);
	bool updateKeywords(const std::vector<size_t> &messages,
			    const std::vector<std::string> &keywords,
			    bool setOrChange,
			    // false: set, true: see changeTo
			    bool changeTo);
	bool updateKeywords(const std::vector<size_t> &messages,
			    const std::list<std::string> &keywords,
			    bool setOrChange,
			    // false: set, true: see changeTo
			    bool changeTo);


	bool updateFolderIndexInfo();

	bool removeMessages(const std::vector<size_t> &messages);

	bool updateFolderIndexFlags(const std::vector<size_t> &messages,
				    bool doFlip,
				    bool enableDisable,
				    const mail::messageInfo &flags);

	bool copyMessagesTo(const std::vector<size_t> &messages,
			    mail::folder *copyTo);

	bool moveMessagesTo(const std::vector<size_t> &messages,
			    mail::folder *copyTo);


	bool checkNewMail();

	bool getMessageEnvelope(const std::vector<size_t> &messages,
				std::vector<xenvelope> &envelopes);
	bool getMessageStructure(const std::vector<size_t> &messages,
				 std::vector<mimestruct> &structures);
	xenvelope getMessageEnvelope(size_t messageNum);
	mimestruct getMessageStructure(size_t messageNum);

	bool searchMessages(const searchParams &searchInfo,
			    std::vector<size_t> &messageList);

	bool getMessageContent(const std::vector<size_t> &messages,
			       bool peek,
			       enum mail::readMode getType,
			       Store &contentCallback);
	bool getMessageContent(size_t messageNum,
			       bool peek,
			       const mimestruct &messagePart,
			       enum mail::readMode getType,
			       Store &contentCallback);
	bool getMessageContentDecoded(size_t messageNum,
				      bool peek,
				      const mimestruct &messagePart,
				      Store &callback);

	bool addMessage(const mail::folder *folder,
			addMessagePull &message);

	bool send(const class mail::smtpInfo &messageInfo,
		  const mail::folder *saveFolder,
		  addMessagePull &message);

	bool getMyRights(const mail::folder *folder, std::string &rights);
	bool getRights(const mail::folder *folder,
		       std::list<std::pair<std::string, std::string> >
		       &rights);

	bool setRights(const mail::folder *folder,
		       std::string &errorIdentifier,
		       std::vector<std::string> &errorRights,
		       std::string identifier,
		       std::string rights);
	bool delRights(const mail::folder *folder,
		       std::string &errorIdentifier,
		       std::vector<std::string> &errorRights,
		       std::string identifier);

	bool folderModified();
	bool disconnected();
	void disconnected(std::string errmsg);
};
LIBMAIL_END

#endif

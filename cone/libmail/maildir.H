/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_maildir_H
#define libmail_maildir_H

#include "libmail_config.h"
#include "mail.H"
#include "generic.H"
#include "unicode/unicode.h"
#include "maildir/maildirwatch.h"
#include "maildir/maildirkeywords.h"

#include <string>
#include <vector>

////////////////////////////////////////////////////////////////////////
//
// A maildirAccount.  A notable observation is that the message filenames are
// used directly as uids.

LIBMAIL_START

class maildir : public mail::account, public generic {

	bool calledDisconnected; // Disconnected callback has been called.

	void resumed();
	void handler(std::vector<pollfd> &fds, int &timeout);

public:
	std::string path;	// Path to the maildirAccount
private:
	class folder;
public:
	class addmessage;
private:
	folder *sameServerFolderPtr;


protected:

	mail::callback::folder *folderCallback;
	std::string folderPath; // Path to the currently opened folder

	bool ispop3maildrop; // This is a subclass of pop3maildrop

private:

	// Caching most recently opened message:
	std::string cacheUID;
	struct rfc2045 *cacheRfcp;
	int cachefd;


	mail::keywords::Hashtable keywordHashtable; // Keyword support

	// The current folder's index.

	class maildirMessageInfo : public mail::messageInfo {
	public:
		maildirMessageInfo();
		~maildirMessageInfo();

		std::string lastKnownFilename; // Message was opened as
		bool changed; // Detects when filename has changed.

		mail::keywords::Message keywords;
		// Keywords set for a message.
	};

	std::vector<maildirMessageInfo> index;

	class readKeywordHelper;
	friend class readKeywordHelper;

	// Get the full path to a message file

	std::string getfilename(size_t index);

	// Rescan the maildirAccount

	bool scan(std::string folderStr,
		    std::vector<maildirMessageInfo> &index,
		    bool scanNew=false);

	class indexSort;

	void open(std::string pathStr, mail::callback &callback,
		  mail::callback::folder &folderCallback);

	static bool updateFlags(const char *fname, mail::messageInfo &info);

	bool updateFlags(size_t msgNum);

	struct maildirwatch *watchFolder;
	struct maildirwatch *lockFolder;
	struct maildirwatch_contents watchFolderContents;
	bool watchStarting;

public:
	static bool maildirmake(std::string subdir, bool isFolder);
	static bool maildirdestroy(std::string dir);

	friend class folder;
	friend class addmessage;
	friend class indexSort;

	maildir(mail::callback::disconnect &disconnect_callback,
		mail::callback &callback,
		std::string pathArg);
	~maildir();

	void logout(mail::callback &callback);
	void checkNewMail(mail::callback &callback);

protected:
	maildir(mail::callback::disconnect &disconnect_callback);

	bool init(mail::callback &callback, std::string pathArg);

	virtual void checkNewMail(mail::callback *callback);
public:

	bool hasCapability(std::string capability);
	std::string getCapability(std::string capability);

	mail::folder *folderFromString(std::string);
	void readTopLevelFolders(mail::callback::folderList &callback1,
				 mail::callback &callback2);

	void findFolder(std::string folder,
			mail::callback::folderList &callback1,
			mail::callback &callback2);
	std::string translatePath(std::string path);
	void readMessageAttributes(const std::vector<size_t> &messages,
				   MessageAttributes attributes,
				   mail::callback::message &callback);

	void readMessageContent(const std::vector<size_t> &messages,
				bool peek,
				enum mail::readMode readType,
				mail::callback::message &callback);

	void readMessageContent(size_t messageNum,
				bool peek,
				const class mimestruct &msginfo,
				enum mail::readMode readType,
				mail::callback::message &callback);

	void readMessageContentDecoded(size_t messageNum,
				       bool peek,
				       const mimestruct &msginfo,
				       mail::callback::message &callback);

	size_t getFolderIndexSize();
	mail::messageInfo getFolderIndexInfo(size_t);

	void saveFolderIndexInfo(size_t,
				 const mail::messageInfo &, mail::callback &);

	static std::string getMaildirFlags(const mail::messageInfo &);

	void updateFolderIndexFlags(const std::vector<size_t> &messages,
				    bool doFlip,
				    bool enableDisable,
				    const mail::messageInfo &flags,
				    mail::callback &callback);

	void updateFolderIndexInfo(mail::callback &);
	void removeMessages(const std::vector<size_t> &messages,
			    callback &cb);
private:
	void updateFolderIndexInfo(mail::callback *, bool);
public:

	void getFolderKeywordInfo(size_t, std::set<std::string> &);
	void updateKeywords(const std::vector<size_t> &messages,
			    const std::set<std::string> &keywords,
			    bool setOrChange,
			    bool changeTo,
			    callback &cb);
private:
	bool updateKeywords(std::string dir,
			    const std::vector<size_t> &messages,
			    const std::set<std::string> &keywords,
			    bool setOrChange,
			    bool changeTo,
			    struct libmail_kwGeneric *oldFlags,
			    bool *keepGoing);
public:


	void copyMessagesTo(const std::vector<size_t> &messages,
			    mail::folder *copyTo,
			    mail::callback &callback);

	void moveMessagesTo(const std::vector<size_t> &messages,
			    mail::folder *copyTo,
			    mail::callback &callback);

	void searchMessages(const class mail::searchParams &searchInfo,
			    class mail::searchCallback &callback);

	void updateNotify(bool enableDisable, callback &callbackArg);

private:

	void updateNotify(bool enableDisable);

public:
	/* ----- */

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

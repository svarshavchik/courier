/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smtpfolder_H
#define libmail_smtpfolder_H

#include "libmail_config.h"
#include "mail.H"
#include "smtp.H"
#include "smtpinfo.H"
#include "addmessage.H"

#include <stdio.h>

// smtp::getSendFolder() creates a fake folder object.  Copying
// messages to the fake folder sends them via SMTP.

struct rfc2045;

LIBMAIL_START

class smtpFolder : public mail::folder {

	ptr<smtp> smtpServer;

	smtpFolder(const smtpFolder &); //UNDEFINED
	smtpFolder &operator=(const smtpFolder &); //UNDEFINED

	smtpInfo smtpSendInfo;

public:
	smtpFolder(smtp *, const smtpInfo &smtpInfo);
	~smtpFolder();

	void sameServerAsHelperFunc() const;
	std::string getName() const;
	std::string getPath() const;
	bool hasMessages() const;
	bool hasSubFolders() const;

	bool isParentOf(std::string) const;

	void hasMessages(bool);
	void hasSubFolders(bool);

	void getParentFolder(callback::folderList &callback1,
			     callback &callback2) const;

	void readFolderInfo( mail::callback::folderInfo &callback1,
			     mail::callback &callback2) const;

	void readSubFolders( mail::callback::folderList &callback1,
			     mail::callback &callback2) const;

	mail::addMessage *addMessage(mail::callback &callback) const;

	void create(bool isDirectory, mail::callback &callback) const;

	void createSubFolder(std::string name, bool isDirectory,
			     mail::callback::folderList &callback1,
			     mail::callback &callback2) const;

	void destroy(mail::callback &callback, bool destroyDir) const;

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

/////////////////////////////////////////////////////////////////////////
//
// addMessage object created by smtpFolder::addMessage()

class smtpAddMessage : public addMessage {

	ptr<smtp> myServer;
	smtpInfo myInfo;
	mail::callback &myCallback;

	FILE *temporaryFile;
	struct rfc2045 *rfcp;
	bool flag8bit;

public:
	smtpAddMessage(smtp *smtpServer,
			     smtpInfo messageInfo,
			     mail::callback &callback);

	~smtpAddMessage();

	void saveMessageContents(std::string);
	void go();
	void fail(std::string errmsg);
};

LIBMAIL_END

#endif

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
#include "rfc822/rfc822.h"
#include "rfc2045/rfc2045.h"
#include "rfc2045/encode.h"
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

	void readFolderInfo( mail::callback::folderInfo &callback1,
			     mail::callback &callback2) const override;

	void readSubFolders( mail::callback::folderList &callback1,
			     mail::callback &callback2) const override;

	mail::addMessage *addMessage(mail::callback &callback) const override;

	void create(bool isDirectory, mail::callback &callback) const override;

	void createSubFolder(std::string name, bool isDirectory,
			     mail::callback::folderList &callback1,
			     mail::callback &callback2) const override;

	void destroy(mail::callback &callback, bool destroyDir) const override;

	void renameFolder(const mail::folder *newParent,
			  std::string newName,
			  mail::callback::folderList &callback1,
			  callback &callback) const override;

	mail::folder *clone() const override;

	std::string toString() const override;

	void open(mail::callback &openCallback,
		  snapshot *restoreSnapshot,
		  mail::callback::folder &folderCallback) const override;
};

/////////////////////////////////////////////////////////////////////////
//
// addMessage object created by smtpFolder::addMessage()

class smtpAddMessage : public addMessage {

	mail::ptr<smtp> myServer;
	smtpInfo myInfo;
	mail::callback &myCallback;

	rfc822::fdstreambuf temporaryFile;
	rfc2045::entity_parser<false> parser;
	bool has8bitmime{false};
	bool flag8bit{false};
public:
	smtpAddMessage(smtp *smtpServer,
			     smtpInfo messageInfo,
			     mail::callback &callback);

	~smtpAddMessage();

	void saveMessageContents(std::string) override;
	void go() override;
	void fail(std::string errmsg) override;
};

LIBMAIL_END

#endif

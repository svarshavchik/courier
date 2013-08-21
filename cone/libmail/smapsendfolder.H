/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapsendfolder_H
#define libmail_smapsendfolder_H

#include "libmail_config.h"
#include	"mail.H"
#include	"imap.H"
#include	"smtpinfo.H"

LIBMAIL_START

class smapSendFolder : public folder {

	mail::imap *imapAccount;

	smtpInfo sendInfo;
	std::string sentPath;

public:
	smapSendFolder(mail::imap *, const smtpInfo &, std::string);
	~smapSendFolder();

	void sameServerAsHelperFunc() const;

	std::string getName() const;

	std::string getPath() const;
	bool hasMessages() const;
	bool hasSubFolders() const;
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

	void renameFolder(const folder *newParent, std::string newName,
			  callback::folderList &callback1,
			  callback &callback2) const;
	folder *clone() const;
	std::string toString() const;
	void open(callback &openCallback,
		  snapshot *restoreSnapshot,
		  callback::folder &folderCallback) const;
};

LIBMAIL_END

#endif

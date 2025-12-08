/*
** Copyright 2003, S. Varshavchik.
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

	void sameServerAsHelperFunc() const override;

	std::string getName() const override;

	std::string getPath() const override;
	bool hasMessages() const override;
	bool hasSubFolders() const override;
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

	void renameFolder(const folder *newParent, std::string newName,
			  callback::folderList &callback1,
			  callback &callback2) const override;
	folder *clone() const override;
	std::string toString() const override;
	void open(callback &openCallback,
		  snapshot *restoreSnapshot,
		  callback::folder &folderCallback) const override;
};

LIBMAIL_END

#endif

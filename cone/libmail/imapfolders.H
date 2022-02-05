/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_imapfolders_H
#define libmail_imapfolders_H

#include "libmail_config.h"
#include "mail.H"

LIBMAIL_START


class imap;

///////////////////////////////////////////////////////////////////////////
//
//  The IMAP driver's mail::folder subclass.

class imapFolder : public mail::folder {

	imap &imapAccount;

public:
	std::string path, hiersep, name;
private:
	bool hasmessages, hassubfolders;
	int type;

	void sameServerAsHelperFunc() const override;

public:
	imapFolder(imap &imapAccount,
		   std::string pathArg, std::string hierArg, std::string nameArg,
		   int typeArg);
	imapFolder(const imapFolder &);

	imapFolder &operator=(const imapFolder &);

	~imapFolder();

	void hasMessages(bool) override;
	void hasSubFolders(bool) override;

	std::string getName() const override;
	std::string getPath() const override;
	bool isParentOf(std::string path) const override;

	std::string getHierSep() { return hiersep.c_str(); }
	void setHierSep(std::string h) { hiersep=h; }

	void setName(const char *c) { name=c; }
	int getType() const { return (type); }

	bool hasMessages() const override;
	bool hasSubFolders() const override;

	void readSubFolders(mail::callback::folderList &callback1,
			    mail::callback &callback2) const override;

	void getParentFolder(callback::folderList &callback1,
			     callback &callback2) const override;

	mail::addMessage *addMessage(mail::callback &callback) const override;

	void readFolderInfo(mail::callback::folderInfo
			    &callback1,
			    mail::callback &callback2) const override;

	void createSubFolder(std::string name, bool isDirectory,
			     mail::callback::folderList &callback1,
			     mail::callback &callback2) const override;

	void create(bool isDirectory,  mail::callback &callback) const override;

	void destroy(mail::callback &callback, bool destroyDir) const override;

	void renameFolder(const folder *newParent, std::string newName,
			  mail::callback::folderList &callback1,
			  callback &callback) const override;

	mail::folder *clone() const override;
	std::string toString() const override;

	void open(mail::callback &openCallback,
		  snapshot *restoreSnapshot,
		  mail::callback::folder &folderCallback) const override;

	void getMyRights(callback &getCallback, std::string &rights) const override;
	void getRights(callback &getCallback,
		       std::list<std::pair<std::string, std::string> >
		       &rights) const override;

	void setRights(callback &setCallback,
		       std::string &errorIdentifier,
		       std::vector<std::string> &errorRights,
		       std::string identifier,
		       std::string rights) const override;
	void delRights(callback &setCallback,
		       std::string &errorIdentifier,
		       std::vector<std::string> &errorRights,
		       std::string identifier) const override;
};

LIBMAIL_END

#endif

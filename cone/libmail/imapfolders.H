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

	void sameServerAsHelperFunc() const;

public:
	imapFolder(imap &imapAccount,
		   std::string pathArg, std::string hierArg, std::string nameArg,
		   int typeArg);
	imapFolder(const imapFolder &);

	imapFolder &operator=(const imapFolder &);

	~imapFolder();

	void hasMessages(bool);
	void hasSubFolders(bool);

	std::string getName() const;
	std::string getPath() const;
	bool isParentOf(std::string) const;

	std::string getHierSep() { return hiersep.c_str(); }
	void setHierSep(std::string h) { hiersep=h; }

	void setName(const char *c) { name=c; }
	int getType() const { return (type); }

	bool hasMessages() const;
	bool hasSubFolders() const;

	void readSubFolders(mail::callback::folderList &callback1,
			    mail::callback &callback2) const;

	void getParentFolder(callback::folderList &callback1,
			     callback &callback2) const;

	mail::addMessage *addMessage(mail::callback &callback) const;

	void readFolderInfo(mail::callback::folderInfo
			    &callback1,
			    mail::callback &callback2) const;

	void createSubFolder(std::string name, bool isDirectory,
			     mail::callback::folderList &callback1,
			     mail::callback &callback2) const;

	void create(bool isDirectory,  mail::callback &callback) const;

	void destroy(mail::callback &callback, bool destroyDir) const;

	void renameFolder(const folder *newParent, std::string newName,
			  mail::callback::folderList &callback1,
			  callback &callback) const;

	mail::folder *clone() const;
	std::string toString() const;

	void open(mail::callback &openCallback,
		  snapshot *restoreSnapshot,
		  mail::callback::folder &folderCallback) const;

	void getMyRights(callback &getCallback, std::string &rights) const;
	void getRights(callback &getCallback,
		       std::list<std::pair<std::string, std::string> >
		       &rights) const;

	void setRights(callback &setCallback,
		       std::string &errorIdentifier,
		       std::vector<std::string> &errorRights,
		       std::string identifier,
		       std::string rights) const;
	void delRights(callback &setCallback,
		       std::string &errorIdentifier,
		       std::vector<std::string> &errorRights,
		       std::string identifier) const;
};

LIBMAIL_END

#endif

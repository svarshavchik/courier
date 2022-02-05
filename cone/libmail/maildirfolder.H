/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_maildirfolder_H
#define libmail_maildirfolder_H

#include "maildir.H"

#include <string>
#include <set>
#include <list>

LIBMAIL_START

////////////////////////////////////////////////////////////////////////////
//
// Courier-compatible maildir folder.

class maildir::folder : public mail::folder {

	maildir *maildirAccount;

	std::string path;
	std::string name;

	bool hasMessagesFlag;
	bool hasSubfoldersFlag;

	// Compile subdirectory listings

	class listinfo {
	public:
		listinfo();
		~listinfo();

		std::string path;

		std::set<std::string> list, subdirs;
	};

	static void maildir_list_callback(const char *, void *);

public:
	friend class maildir;

	folder(maildir *maildirArg,
	       std::string pathArg);
	~folder();

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

private:
	void buildFolderList(std::list<mail::folder *> &folderList,
			     std::set<std::string> *folders,
			     std::set<std::string> *dirs) const;

	bool doCreate(bool isDirectory) const;

public:
	mail::addMessage *addMessage(mail::callback &callback) const override;

	void createSubFolder(std::string name, bool isDirectory,
			     mail::callback::folderList &callback1,
			     mail::callback &callback2) const override;

	void create(bool isDirectory,
		    mail::callback &callback) const override;

	void destroy(mail::callback &callback, bool destroyDir) const override;

	void renameFolder(const mail::folder *newParent, std::string newName,
			  mail::callback::folderList &callback1,
			  mail::callback &callback) const override;

	mail::folder *clone() const override;
	std::string toString() const override;

	void open(mail::callback &openCallback,
		  snapshot *restoreSnapshot,
		  mail::callback::folder &folderCallback) const override;
};

LIBMAIL_END

#endif

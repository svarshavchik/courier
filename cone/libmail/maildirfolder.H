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

	void sameServerAsHelperFunc() const;

	std::string getName() const;
	std::string getPath() const;

	bool hasMessages() const;
	bool hasSubFolders() const;
	bool isParentOf(std::string path) const;

	void hasMessages(bool);
	void hasSubFolders(bool);

	void getParentFolder(callback::folderList &callback1,
			     callback &callback2) const;

	void readFolderInfo( mail::callback::folderInfo &callback1,
			     mail::callback &callback2) const;

	void readSubFolders( mail::callback::folderList &callback1,
			     mail::callback &callback2) const;

private:
	void buildFolderList(std::list<mail::folder *> &folderList,
			     std::set<std::string> *folders,
			     std::set<std::string> *dirs) const;

	bool doCreate(bool isDirectory) const;

public:
	mail::addMessage *addMessage(mail::callback &callback) const;

	void createSubFolder(std::string name, bool isDirectory,
			     mail::callback::folderList &callback1,
			     mail::callback &callback2) const;

	void create(bool isDirectory,
		    mail::callback &callback) const;

	void destroy(mail::callback &callback, bool destroyDir) const;

	void renameFolder(const mail::folder *newParent, std::string newName,
			  mail::callback::folderList &callback1,
			  mail::callback &callback) const;

	mail::folder *clone() const;
	std::string toString() const;

	void open(mail::callback &openCallback,
		  snapshot *restoreSnapshot,
		  mail::callback::folder &folderCallback) const;
};

LIBMAIL_END

#endif



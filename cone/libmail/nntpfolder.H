/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntpfolder_H
#define libmail_nntpfolder_H

#include "libmail_config.h"

#include "nntp.H"

LIBMAIL_START

// An NNTP folder object

class mail::nntp::folder : public mail::folder {

	nntp *myserver;

	std::string path;
	std::string name;

	bool hasMessagesFlag;
	bool hasSubFoldersFlag;

	// Private class used to build hierarchy listings

	std::string newsgroupName() const;

public:
	class hierEntry {
	public:
		bool foundFolder;
		bool foundSubFolders;

		hierEntry() : foundFolder(false), foundSubFolders(false)
		{
		}

		~hierEntry()
		{
		}
	};

	folder(nntp *myserverArg, std::string pathArg,
	       bool hasMessagesArg, bool hasSubFoldersArg);

	folder(const mail::nntp::folder &);
	mail::nntp::folder &operator=(const mail::nntp::folder &);

	~folder();

	void sameServerAsHelperFunc() const;
	std::string getName() const;
	std::string getPath() const;
	bool hasMessages() const;
	bool hasSubFolders() const;
	std::string isNewsgroup() const;
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
	void renameFolder(const mail::folder *newParent, std::string newName,
			  callback::folderList &callback1,
			  callback &callback2) const;
	mail::folder *clone() const;
	std::string toString() const;
	void open(callback &openCallback,
		  snapshot *restoreSnapshot,
		  callback::folder &folderCallback) const;
};

// Special folder paths

#define FOLDER_SUBSCRIBED "/subscribed" // Subscribed newsrc folders

#define FOLDER_NEWSRC "/newsrc"		// All newsgroups in newsrc

#define FOLDER_CHECKNEW "/newgroups"	// New newsgroups

#define FOLDER_REFRESH "/refresh"	// Refresh newsgroup list

LIBMAIL_END
#endif

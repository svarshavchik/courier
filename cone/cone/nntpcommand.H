/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef nntpcommand_H
#define nntpcommand_H

#include "config.h"

#include "libmail/mail.H"

#include <string>

////////////////////////////////////////////////////////////////////
//
// A custom subclass of folder that's used for running an external
// command to post netnews messages.  All mail::folder methods are
// generic stubs, but addMessage returns a class that happily takes
// a message, and forks and execs a subprocess with that message on
// standar dinput.

class nntpCommandFolder : public mail::folder {


public:
	nntpCommandFolder();
	~nntpCommandFolder();

	static std::string nntpCommand;

	class add;

private:

	static void na(mail::callback &cb);

	// mail::folder stubs

	void sameServerAsHelperFunc() const;
	std::string getName() const;
	std::string getPath() const;
	bool hasMessages() const;
	bool hasSubFolders() const;
	std::string isNewsgroup() const;
	bool isParentOf(std::string path) const;
	void hasMessages(bool);
	void hasSubFolders(bool);
	void readFolderInfo( mail::callback::folderInfo &callback1,
			     mail::callback &callback2) const;
	void getParentFolder(mail::callback::folderList &callback1,
			     mail::callback &callback2) const;
	void readSubFolders( mail::callback::folderList &callback1,
			     mail::callback &callback2) const;
	mail::addMessage *addMessage(mail::callback &callback) const;
	void createSubFolder(std::string name, bool isDirectory,
			     mail::callback::folderList &callback1,
			     mail::callback &callback2) const;
	void create(bool isDirectory,
		    mail::callback &callback) const;
	void destroy(mail::callback &callback, bool destroyDir) const;
	void renameFolder(const mail::folder *newParent, std::string newName,
			  mail::callback::folderList &callback1,
			  mail::callback &callback2) const;
	folder *clone() const;
	std::string toString() const;
	void open(mail::callback &openCallback,
		  mail::snapshot *restoreSnapshot,
		  mail::callback::folder &folderCallback) const;
};

#endif

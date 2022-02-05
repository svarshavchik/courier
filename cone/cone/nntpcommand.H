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

	void sameServerAsHelperFunc() const override;
	std::string getName() const override;
	std::string getPath() const override;
	bool hasMessages() const override;
	bool hasSubFolders() const override;
	std::string isNewsgroup() const override;
	bool isParentOf(std::string path) const override;
	void hasMessages(bool) override;
	void hasSubFolders(bool) override;
	void readFolderInfo( mail::callback::folderInfo &callback1,
			     mail::callback &callback2) const override;
	void getParentFolder(mail::callback::folderList &callback1,
			     mail::callback &callback2) const override;
	void readSubFolders( mail::callback::folderList &callback1,
			     mail::callback &callback2) const override;
	mail::addMessage *addMessage(mail::callback &callback) const override;
	void createSubFolder(std::string name, bool isDirectory,
			     mail::callback::folderList &callback1,
			     mail::callback &callback2) const override;
	void create(bool isDirectory,
		    mail::callback &callback) const override;
	void destroy(mail::callback &callback, bool destroyDir) const override;
	void renameFolder(const mail::folder *newParent, std::string newName,
			  mail::callback::folderList &callback1,
			  mail::callback &callback2) const override;
	folder *clone() const override;
	std::string toString() const override;
	void open(mail::callback &openCallback,
		  mail::snapshot *restoreSnapshot,
		  mail::callback::folder &folderCallback) const override;
};

#endif

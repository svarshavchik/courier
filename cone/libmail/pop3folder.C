/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"

#include "mail.H"
#include "pop3.H"

using namespace std;

mail::pop3Folder::pop3Folder(mail::pop3 *serverArg)
	: mail::folder(serverArg), server(serverArg)
{
}

mail::pop3Folder::~pop3Folder()
{
}

void mail::pop3Folder::sameServerAsHelperFunc() const
{
}

string mail::pop3Folder::getName() const
{
	return "INBOX";
}

string mail::pop3Folder::getPath() const
{
	return getName();
}

bool mail::pop3Folder::hasMessages() const
{
	return true;
}

bool mail::pop3Folder::hasSubFolders() const
{
	return false;
}

bool mail::pop3Folder::isParentOf(string path) const
{
	return false;
}

void mail::pop3Folder::hasMessages(bool flag)
{
}

void mail::pop3Folder::hasSubFolders(bool flag)
{
}

void mail::pop3Folder::readFolderInfo( mail::callback::folderInfo &callback1,
				       mail::callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	server->readFolderInfo(callback1, callback2);
}

void mail::pop3Folder::readSubFolders( mail::callback::folderList &callback1,
				       mail::callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	vector<const mail::folder *> dummy;

	callback1.success(dummy);
	callback2.success("No folders exist on a POP3 server.");
}

mail::addMessage *mail::pop3Folder::addMessage(mail::callback &callback) const
{
	callback.fail("Messages cannot be copied to this POP3 server account.");
	return NULL;
}

void mail::pop3Folder::createSubFolder(string name, bool isDirectory,
				       mail::callback::folderList &callback1,
				       mail::callback &callback2) const
{
	callback2.fail("Folders cannot be created in this POP3 server account.");
}

void mail::pop3Folder::create(bool isDirectory, mail::callback &callback) const
{
	callback.fail("Folders cannot be created in this POP3 server account.");
}

void mail::pop3Folder::destroy(mail::callback &callback, bool destroyDir) const
{
	callback.fail("You cannot delete this POP3 folder.  It's a fake,"
		      " and how did you get this error message anyway?");
}

void mail::pop3Folder::renameFolder(const mail::folder *newParent,
				    std::string newName,
				    mail::callback::folderList &callback1,
				    callback &callback) const
{
	callback.fail("POP3 folders cannot be renamed.");
}

mail::folder *mail::pop3Folder::clone() const
{
	mail::pop3Folder *p=new mail::pop3Folder(server);

	if (!p)
		return NULL;

	return p;
}

string mail::pop3Folder::toString() const
{
	return getPath();
}

void mail::pop3Folder::open(mail::callback &openCallback,
			    mail::snapshot *restoreSnapshot,
			    mail::callback::folder &folderCallback) const
{
	if (isDestroyed(openCallback))
		return;
	server->open(openCallback, folderCallback, restoreSnapshot);
}

string mail::pop3::translatePath(string path)
{
	return "INBOX";	// NOOP
}


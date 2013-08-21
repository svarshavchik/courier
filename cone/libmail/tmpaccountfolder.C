/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "tmpaccount.H"
#include <errno.h>
#include <vector>
#include <cstring>

using namespace std;

void mail::tmpaccount::folder::na(callback &cb)
{
	cb.fail("Not implemented.");
}

mail::tmpaccount::folder::folder(tmpaccount *accountArg)
	: mail::folder(accountArg), account(accountArg)
{
}

mail::tmpaccount::folder::~folder()
{
}

bool mail::tmpaccount::folder::isParentOf(std::string path) const
{
	return false;
}

void mail::tmpaccount::folder::hasMessages(bool dummy)
{
}

void mail::tmpaccount::folder::hasSubFolders(bool dummy)
{
}

void mail::tmpaccount::folder::readFolderInfo( callback::folderInfo
					       &callback1,
					       callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	callback1.messageCount=0;
	callback1.unreadCount=0;

	if (account->f)
	{
		++callback1.messageCount;
		if (account->fInfo.unread)
			++callback1.unreadCount;
	}
	callback1.success();
	callback2.success("Ok");
}
	
void mail::tmpaccount::folder::getParentFolder(callback::folderList &callback1,
					       callback &callback2) const
{
	na(callback2);
}

void mail::tmpaccount::folder::readSubFolders( callback::folderList &callback1,
					       callback &callback2) const
{
	vector<const mail::folder *> dummy;

	callback1.success(dummy);
	callback2.success("Ok");
}

mail::addMessage *mail::tmpaccount::folder::addMessage(callback &callback)
	const
{
	if (isDestroyed(callback))
		return NULL;

	add *a=new add(account, callback);

	if (!a)
		callback.fail(strerror(errno));

	return a;
}

void mail::tmpaccount::folder::createSubFolder(std::string name,
					       bool isDirectory,
					       callback::folderList
					       &callback1,
					       callback &callback2) const
{
	na(callback2);
}

void mail::tmpaccount::folder::create(bool isDirectory,
				      callback &callback) const
{
	na(callback);
}

void mail::tmpaccount::folder::destroy(callback &callback,
				       bool destroyDir) const
{
	na(callback);
}

void mail::tmpaccount::folder::renameFolder(const mail::folder *newParent,
					    string newName,
					    callback::folderList &callback1,
					    callback &callback2) const
{
	na(callback2);
}

mail::tmpaccount::folder *mail::tmpaccount::folder::clone() const
{
	return new folder(account);
}

string mail::tmpaccount::folder::toString() const
{
	return "INBOX";
}

void mail::tmpaccount::folder::open(callback &openCallback,
				    snapshot *restoreSnapshot,
				    callback::folder &folderCallback) const
{
	if (isDestroyed(openCallback))
		return;

	if (account->f)
		fclose(account->f);
	account->f=NULL;

	account->folder_callback= &folderCallback;
	openCallback.success("Ok.");
}

void mail::tmpaccount::folder::sameServerAsHelperFunc() const
{
}

string mail::tmpaccount::folder::getName() const
{
	return "INBOX";
}

string mail::tmpaccount::folder::getPath() const
{
	return "INBOX";
}

bool mail::tmpaccount::folder::hasMessages() const
{
	return true;
}

bool mail::tmpaccount::folder::hasSubFolders() const
{
	return false;
}

/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "smapsendfolder.H"
#include "smapaddmessage.H"
#include <errno.h>
#include <cstring>

using namespace std;

mail::smapSendFolder::smapSendFolder(mail::imap *imap,
				     const smtpInfo &sendInfoArg,
				     std::string sentPathArg)
	: folder(imap), imapAccount(imap), sendInfo(sendInfoArg),
	  sentPath(sentPathArg)
{
}

mail::smapSendFolder::~smapSendFolder()
{
}

void mail::smapSendFolder::sameServerAsHelperFunc() const
{
}

string mail::smapSendFolder::getName() const
{
	return "";
}

string mail::smapSendFolder::getPath() const
{
	return "";
}

bool mail::smapSendFolder::hasMessages() const
{
	return true;
}

bool mail::smapSendFolder::hasSubFolders() const
{
	return false;
}

bool mail::smapSendFolder::isParentOf(string path) const
{
	return false;
}

void mail::smapSendFolder::hasMessages(bool arg)
{
}

void mail::smapSendFolder::hasSubFolders(bool arg)
{
}

void mail::smapSendFolder::readFolderInfo( callback::folderInfo
					   &callback1,
					   callback &callback2) const
{
	callback1.success();
	callback2.success("OK");
}

void mail::smapSendFolder::getParentFolder(callback::folderList &callback1,
					   callback &callback2) const
{
	callback2.fail("Not implemented");
}

void mail::smapSendFolder::readSubFolders( callback::folderList &callback1,
					   callback &callback2) const
{
	callback2.fail("Not implemented");
}


mail::addMessage *mail::smapSendFolder::addMessage(callback &callback) const
{
	if (isDestroyed(callback))
		return NULL;

	mail::smapAddMessage *add=new smapAddMessage(*imapAccount, callback);

	if (!add)
	{
		callback.fail(strerror(errno));
		return NULL;
	}

	try {
		if (sentPath.find('\n') != std::string::npos ||
		    sendInfo.sender.find('\n') != std::string::npos)
		{
			mail::smapAddMessage *p=add;

			add=NULL;

			p->fail("Invalid sender address.");
			return NULL;
		}

		if (sentPath.size() > 0)
			mail::imap::addSmapFolderCmd(add, sentPath);

		add->cmds.push_back("ADD " +
				    mail::imap::quoteSMAP("MAILFROM="
							  + sendInfo.sender)
				    + "\n");
		vector<string>::const_iterator b=sendInfo.recipients.begin(),
			e=sendInfo.recipients.end();

		while (b != e)
		{
			if ( (*b).find('\n') != std::string::npos)
			{
				mail::smapAddMessage *p=add;

				add=NULL;

				p->fail("Invalid recipient address.");
				return NULL;
			}

			add->cmds.push_back("ADD " +
					    mail::imap::quoteSMAP("RCPTTO="
								  + *b)
					    + "\n");
			b++;
		}

		map<string, string>::const_iterator optPtr;

		for (optPtr=sendInfo.options.begin();
		     optPtr != sendInfo.options.end(); optPtr++)
		{

			if (optPtr->second.find('\n') !=
			    std::string::npos)
			{
				mail::smapAddMessage *p=add;

				add=NULL;
				p->fail("Invalid option.");
				return NULL;
			}
		}

		optPtr=sendInfo.options.find("DSN");

		if (optPtr != sendInfo.options.end())
		{
			add->cmds.push_back("ADD " +
					    mail::imap::quoteSMAP("NOTIFY="
								  + optPtr
								  ->second)
					    + "\n");
		}
	} catch (...) {
		if (add)
		{
			delete add;
			add=NULL;
		}
	}
	return add;
}

void mail::smapSendFolder::createSubFolder(string name, bool isDirectory,
					   callback::folderList
					   &callback1,
					   callback &callback2) const
{
	callback2.fail("Not implemented");
}


void mail::smapSendFolder::create(bool isDirectory,
				  callback &callback) const
{
	callback.fail("Not implemented");
}

void mail::smapSendFolder::destroy(callback &callback, bool destroyDir) const
{
	callback.fail("Not implemented");
}

void mail::smapSendFolder::renameFolder(const folder *newParent, string newName,
					callback::folderList &callback1,
					callback &callback2) const
{
	callback2.fail("Not implemented");
}

mail::folder *mail::smapSendFolder::clone() const
{
	if (isDestroyed())
	{
		errno=ENOENT;
		return NULL;
	}

	return new smapSendFolder(imapAccount, sendInfo, sentPath);
}

string mail::smapSendFolder::toString() const
{
	return "";
}

void mail::smapSendFolder::open(callback &openCallback,
				snapshot *restoreSnapshot,
				callback::folder &folderCallback) const
{
	openCallback.fail("Not implemented");
}

/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "smtpfolder.H"
#include <errno.h>
#include "rfc2045/rfc2045.h"
#include <cstring>

using namespace std;

mail::smtpFolder::smtpFolder(mail::smtp *mySmtp,
				   const mail::smtpInfo &myInfo)
	: mail::folder(mySmtp), smtpServer(mySmtp),
	  smtpSendInfo(myInfo)
{
}

mail::smtpFolder::~smtpFolder()
{
}

void mail::smtpFolder::sameServerAsHelperFunc() const
{
}

string mail::smtpFolder::getName() const
{
	return "SMTP";
}

string mail::smtpFolder::getPath() const
{
	return "SMTP";
}

bool mail::smtpFolder::isParentOf(string path) const
{
	return false;
}

bool mail::smtpFolder::hasMessages() const
{
	return true;
}

bool mail::smtpFolder::hasSubFolders() const
{
	return false;
}

void mail::smtpFolder::hasMessages(bool dummy)
{
}

void mail::smtpFolder::hasSubFolders(bool dummy)
{
}

void mail::smtpFolder::getParentFolder(callback::folderList &callback1,
				       callback &callback2) const
{
	callback2.fail("Not implemented");
}

void mail::smtpFolder::readFolderInfo( mail::callback::folderInfo &callback1,
				       mail::callback &callback2) const
{
	callback1.success();
	callback2.success("OK");
}

void mail::smtpFolder::readSubFolders( mail::callback::folderList &callback1,
				       mail::callback &callback2) const
{
	vector<const mail::folder *> dummy;

	callback1.success(dummy);
	callback2.success("OK");
}

mail::addMessage *mail::smtpFolder::addMessage(mail::callback &callback) const
{
	if (smtpServer.isDestroyed())
	{
		callback.fail("Server connection closed.");
		return NULL;
	}

	mail::smtpAddMessage *msgp=new mail::smtpAddMessage(smtpServer,
							    smtpSendInfo,
							    callback);
	if (!msgp)
	{
		callback.fail(strerror(errno));
		return NULL;
	}

	return msgp;
}

void mail::smtpFolder::createSubFolder(string name, bool isDirectory,
				       mail::callback::folderList &callback1,
				       mail::callback &callback2) const
{
	callback2.fail("Cannot create subfolder.");
}

void mail::smtpFolder::create(bool isDirectory, mail::callback &callback) const
{
	callback.success("OK");
}

void mail::smtpFolder::destroy(mail::callback &callback, bool destroyDir) const
{
	callback.success("OK");
}

void mail::smtpFolder::renameFolder(const mail::folder *newParent,
				    std::string newName,
				    mail::callback::folderList &callback1,
				    callback &callback) const
{
	callback.fail("Not implemented.");
}

mail::folder *mail::smtpFolder::clone() const
{
	mail::smtpFolder *c=new mail::smtpFolder(smtpServer, smtpSendInfo);
	
	if (!c)
		return NULL;

	c->smtpSendInfo=smtpSendInfo;

	return c;
}

string mail::smtpFolder::toString() const
{
	return "SMTP";
}

void mail::smtpFolder::open(mail::callback &openCallback,
			    mail::snapshot *restoreSnapshot,
			    mail::callback::folder &folderCallback) const
{
	openCallback.success("OK");
}

//////////////////////////////////////////////////////////////////////////
//
// mail::smtpAddMessage collects the message into a temporary file.
// If the smtpAccount server does not support 8bit mail, 8bit messages are rewritten
// as quoted printable. mail::smtp::send() receives the end result.

mail::smtpAddMessage::smtpAddMessage(mail::smtp *smtpServer,
				     mail::smtpInfo messageInfo,
				     mail::callback &callback)
	: mail::addMessage(smtpServer),
	  myServer(smtpServer),
	  myInfo(messageInfo),
	  myCallback(callback),
	  temporaryFile(NULL),
	  rfcp(NULL),
	  flag8bit(false)
{
	if (!smtpServer->hasCapability("8BITMIME"))
	{
		if ((rfcp=rfc2045_alloc_ac()) == NULL)
			LIBMAIL_THROW("Out of memory.");

		// Might need rewriting

	}

	if ((temporaryFile=tmpfile()) == NULL)
	{
		if (rfcp)
			rfc2045_free(rfcp);
		rfcp=NULL;
		LIBMAIL_THROW("Out of memory.");
	}
}

mail::smtpAddMessage::~smtpAddMessage()
{
	if (temporaryFile)
		fclose(temporaryFile);
	if (rfcp)
		rfc2045_free(rfcp);
}

void mail::smtpAddMessage::saveMessageContents(string s)
{
	string::iterator b=s.begin(), e=s.end();

	while (b != e)
		if (*b++ & 0x80)
			flag8bit=true;
	if (fwrite(&*s.begin(), s.size(), 1, temporaryFile) != 1)
		; // Ignore gcc warning.
	if (rfcp)
		rfc2045_parse(rfcp, &*s.begin(), s.size());
}

void mail::smtpAddMessage::fail(string errmsg)
{
	myCallback.fail(errmsg);
	delete this;
}

void mail::smtpAddMessage::go()
{
	try {
		if (fflush(temporaryFile) || ferror(temporaryFile))
		{
			fail(strerror(errno));
			return;
		}

		if (myServer.isDestroyed())
			return;

		if (rfcp)
		{
			rfc2045_parse_partial(rfcp);
			if (rfc2045_ac_check(rfcp, RFC2045_RW_7BIT))
			{
				FILE *tmpfile2=tmpfile();
				if (!tmpfile2)
				{
					fail(strerror(errno));
					return;
				}

				int fd_copy=dup(fileno(tmpfile2));

				if (fd_copy < 0)
				{
					fclose(tmpfile2);
					fail(strerror(errno));
					return;
				}

				struct rfc2045src *src=
					rfc2045src_init_fd
					(fileno(temporaryFile));

				if (src == NULL ||
				    rfc2045_rewrite(rfcp,
						    src,
						    fd_copy,
						    "mail::account") < 0)
				{
					if (src)
						rfc2045src_deinit(src);
					close(fd_copy);
					fclose(tmpfile2);
					fail(strerror(errno));
					return;
				}
				close(fd_copy);
				fclose(temporaryFile);
				temporaryFile=tmpfile2;
				flag8bit=false;
				rfc2045src_deinit(src);
			}
		}
		myServer->send(temporaryFile, myInfo, &myCallback,
			       flag8bit);
		temporaryFile=NULL;
		delete this;
	} catch (...) {
		fail("An exception occurred while sending message.");
		delete this;
	}
}



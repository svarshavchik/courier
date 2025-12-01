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
	: mail::addMessage{smtpServer},
	  myServer{smtpServer},
	  myInfo{messageInfo},
	  myCallback{callback},
	  temporaryFile{rfc822::fdstreambuf::tmpfile()}
{
	has8bitmime=smtpServer->hasCapability("8BITMIME");
}

mail::smtpAddMessage::~smtpAddMessage()=default;

void mail::smtpAddMessage::saveMessageContents(string s)
{
	for (auto &c:s)
		if (c & 0x80)
			flag8bit=true;

	auto ptr=s.data();
	auto n=s.size();

	while (n)
	{
		auto d=temporaryFile.sputn(ptr, n);

		if (d <= 0)
			break;

		ptr += d;
		n -= d;
	}

	parser.parse(s.begin(), s.end());
}

void mail::smtpAddMessage::fail(string errmsg)
{
	myCallback.fail(errmsg);
	delete this;
}

void mail::smtpAddMessage::go()
{
	try {
		if (temporaryFile.pubsync() < 0 || temporaryFile.error())
		{
			fail(strerror(errno));
			return;
		}

		if (myServer.isDestroyed())
			return;

		auto entity=parser.parsed_entity();

		if (!has8bitmime)
		{
			if (entity.autoconvert_check(
				    rfc2045::convert::sevenbit)
			)
			{
				rfc822::fdstreambuf tmpfile2{
					rfc822::fdstreambuf::tmpfile()
				};

				if (tmpfile2.error())
				{
					fail(strerror(errno));
					return;
				}

				rfc2045::entity::autoconvert_meta meta;

				meta.appid="mail::account";

				rfc2045::entity::line_iter<false>::autoconvert(
					entity,
					[&]
					(const char *ptr, size_t n)
					{
						while (n)
						{
							auto d=tmpfile2.sputn(
								ptr, n
							);

							if (d <= 0)
							{
								tmpfile2={};
								return;
							}
							ptr += d;
							n -= d;
						}
					},
					temporaryFile,
					meta
				);

				temporaryFile=std::move(tmpfile2);

				if (temporaryFile.error())
				{
					fail(strerror(errno));
				}
			}
		}

		myServer->send(temporaryFile, myInfo, &myCallback,
			       flag8bit);
		delete this;
	} catch (...) {
		fail("An exception occurred while sending message.");
		delete this;
	}
}

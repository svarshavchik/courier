/*
** Copyright 2003-2008, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#include "tmpaccount.H"
#include "driver.H"
#include "file.H"
#include "copymessage.H"
#include "search.H"
#include <errno.h>
#include <sys/stat.h>
#include <cstring>

using namespace std;

LIBMAIL_START

static bool open_tmp(mail::account *&accountRet,
		     mail::account::openInfo &oi,
		     mail::callback &callback,
		     mail::callback::disconnect &disconnectCallback)
{
	if (oi.url == "tmp:")
	{
		accountRet=new mail::tmpaccount(disconnectCallback);

		if (accountRet)
			callback.success("Ok.");
		else
			callback.fail(strerror(errno));
		return true;
	}
	return false;
}

static bool remote_tmp(string url, bool &flag)
{
	if (url == "tmp:")
	{
		flag=false;
		return true;
	}

	return false;
}

driver tmp_driver= { &open_tmp, &remote_tmp };

LIBMAIL_END

mail::tmpaccount::tmpaccount(callback::disconnect &disconnect_callbackArg)
	: account(disconnect_callbackArg),
	  disconnect_callback(&disconnect_callbackArg),
	  folder_callback(NULL), f(NULL), rfc2045p(NULL)
{
}

void mail::tmpaccount::disconnect(const char *reason)
{
	callback::disconnect *d=disconnect_callback;

	disconnect_callback=NULL;

	if (d)
		d->disconnected(reason);
}

mail::tmpaccount::~tmpaccount()
{
	disconnect("Disconnected.");
}


void mail::tmpaccount::resumed()
{
}

void mail::tmpaccount::handler(vector<pollfd> &fds, int &ioTimeout)
{
}

void mail::tmpaccount::logout(mail::callback &callback)
{
	disconnect("");
	callback.success("Ok.");
}

void mail::tmpaccount::checkNewMail(callback &callback)
{
	callback.success("Ok.");
}

bool mail::tmpaccount::hasCapability(std::string capability)
{
	if (capability == LIBMAIL_SINGLEFOLDER)
		return true;
	return false;
}

string mail::tmpaccount::getCapability(std::string capability)
{
	if (capability == LIBMAIL_SERVERTYPE)
		return "tmp";

	if (capability == LIBMAIL_SERVERDESCR)
		return "Temporary account";

	return "";
}

mail::folder *mail::tmpaccount::folderFromString(std::string path)
{
	return new folder(this);
}

void mail::tmpaccount::readTopLevelFolders(callback::folderList &callback1,
					   callback &callback2)
{
	folder dummyFolder(this);

	vector<const mail::folder *> dummy;

	dummy.push_back(&dummyFolder);

	callback1.success(dummy);
	callback2.success("Ok.");
}

void mail::tmpaccount::findFolder(std::string path,
				  callback::folderList &callback1,
				  callback &callback2)
{
	folder dummyFolder(this);

	vector<const mail::folder *> dummy;

	dummy.push_back(&dummyFolder);

	callback1.success(dummy);
	callback2.success("Ok.");
}

std::string mail::tmpaccount::translatePath(std::string path)
{
	return path;
}

void mail::tmpaccount::readMessageAttributes(const std::vector<size_t>
					     &messages,
					     MessageAttributes attributes,
					     callback::message &callback)
{
	genericAttributes(this, this, messages, attributes, callback);
}

void mail::tmpaccount::readMessageContent(const std::vector<size_t> &messages,
					  bool peek,
					  enum mail::readMode readType,
					  callback::message &callback)
{
	genericReadMessageContent(this, this, messages, peek,
				  readType, callback);
}

void mail::tmpaccount::readMessageContent(size_t messageNum,
					  bool peek,
					  const class mimestruct &msginfo,
					  enum mail::readMode readType,
					  callback::message &callback)
{
	genericReadMessageContent(this, this, messageNum, peek,
				  msginfo, readType, callback);
}

void mail::tmpaccount::readMessageContentDecoded(size_t messageNum,
						 bool peek,
						 const class mimestruct
						 &msginfo,
						 callback::message &callback)
{
	genericReadMessageContentDecoded(this, this,
					 messageNum, peek, msginfo, callback);
}

size_t mail::tmpaccount::getFolderIndexSize()
{
	return f ? 1:0;
}

mail::messageInfo mail::tmpaccount::getFolderIndexInfo(size_t n)
{
	if (n == 0 && f)
		return fInfo;

	return mail::messageInfo();
}

void mail::tmpaccount::saveFolderIndexInfo(size_t messageNum,
					   const messageInfo &msgInfo,
					   callback &callback)
{
	if (messageNum == 0 && f)
	{
		messageInfo n=msgInfo;

		n.uid=fInfo.uid;
		fInfo=n;

		if (folder_callback)
			folder_callback->messageChanged(0);
	}

	callback.success("Ok.");
}

void mail::tmpaccount::updateFolderIndexFlags(const std::vector<size_t>
					      &messages,
					      bool doFlip,
					      bool enableDisable,
					      const messageInfo &flags,
					      callback &callback)
{
	vector<size_t>::const_iterator b, e;

	b=messages.begin();
	e=messages.end();

	bool dirty=false;

	while (b != e)
	{
		size_t i= *b++;

		if (i == 0 && f)
		{
#define DOFLAG(dummy, field, dummy2) \
			if (flags.field) \
			{ \
				fInfo.field=\
					doFlip ? !fInfo.field\
					       : enableDisable; \
			}

			LIBMAIL_MSGFLAGS;
#undef DOFLAG
			dirty=true;
		}
	}

	if (dirty && folder_callback)
		folder_callback->messageChanged(0);

	callback.success("Ok.");
}

void mail::tmpaccount::updateFolderIndexInfo(callback &cb)
{
	if (f && fInfo.deleted)
	{
		f.reset();

		vector< pair<size_t, size_t> > dummy;

		dummy.push_back( make_pair( (size_t)0, (size_t)0));
		if (folder_callback)
			folder_callback->messagesRemoved(dummy);
	}
	cb.success("Ok.");
}

void mail::tmpaccount::removeMessages(const std::vector<size_t> &messages,
				      callback &cb)
{
	genericRemoveMessages(this, messages, cb);
}

void mail::tmpaccount::copyMessagesTo(const std::vector<size_t> &messages,
				      mail::folder *copyTo,
				      callback &callback)
{
	mail::copyMessages::copy(this, messages, copyTo, callback);
}

void mail::tmpaccount::searchMessages(const searchParams &searchInfo,
				      searchCallback &callback)
{
	mail::searchMessages::search(callback, searchInfo, this);
}

void mail::tmpaccount::genericMessageRead(std::string uid,
					  size_t messageNumber,
					  bool peek,
					  mail::readMode readType,
					  mail::callback::message &callback)
{
	if (f && fInfo.uid == uid)
	{
		if (f->pubseekoff(0, std::ios_base::end) < 0 ||
		    f->pubseekpos(0) != 0)
		{
			callback.fail(strerror(errno));
			return;
		}

		mail::file rf(f->fileno(), "r");

		if (!rf)
		{
			callback.fail(strerror(errno));
			return;
		}

		rf.genericMessageRead(this, 0, readType, callback);

		if (ferror(rf))
		{
			callback.fail(strerror(errno));
			return;
		}
	}

	if (!peek)
		genericMarkRead(0);

	callback.success("Ok.");
}

void mail::tmpaccount::genericMessageSize(std::string uid,
					  size_t messageNumber,
					  mail::callback::message &callback)
{
	if (f && fInfo.uid == uid)
	{
		struct stat stat_buf;

		if (fstat(f->fileno(), &stat_buf) == 0)
		{
			callback.messageSizeCallback(0, stat_buf.st_size);
			callback.success("Ok.");
			return;
		}

		callback.fail(strerror(errno));
	}
	else
		callback.success("Ok.");
}

void mail::tmpaccount::genericGetMessageFd(
	std::string uid,
	size_t messageNumber,
	bool peek,
	std::shared_ptr<rfc822::fdstreambuf> &fdRet,
	mail::callback &callback)
{
	std::shared_ptr<rfc2045::entity> dummy;

	genericGetMessageFdStruct(uid, messageNumber, peek, fdRet, dummy,
				  callback);
}

void mail::tmpaccount::genericGetMessageStruct(
	std::string uid,
	size_t messageNumber,
	std::shared_ptr<rfc2045::entity> &structRet,
	mail::callback &callback)
{
	std::shared_ptr<rfc822::fdstreambuf> dummy;

	genericGetMessageFdStruct(uid, messageNumber, true, dummy, structRet,
				  callback);
}

bool mail::tmpaccount::genericCachedUid(std::string uid)
{
	return f && uid == fInfo.uid;
}

void mail::tmpaccount::genericGetMessageFdStruct(
	std::string uid,
	size_t messageNumber,
	bool peek,
	std::shared_ptr<rfc822::fdstreambuf> &fdRet,
	std::shared_ptr<rfc2045::entity> &structRet,
	mail::callback &callback)
{
	if (f && uid == fInfo.uid)
	{
		if (f->pubseekoff(0, std::ios_base::end) < 0 ||
		    f->pubseekpos(0) != 0)
		{
			callback.fail(strerror(errno));
			return;
		}
		fdRet=f;
		structRet=rfc2045p;
		if (!peek)
			genericMarkRead(0);

		callback.success("Ok.");
		return;
	}

	callback.fail(strerror(ENOENT));
}

void mail::tmpaccount::genericMarkRead(size_t messageNumber)
{

	if (f && fInfo.unread)
	{
		fInfo.unread=false;
		if (folder_callback)
			folder_callback->messageChanged(0);
	}
}

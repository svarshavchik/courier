/*
** Copyright 2002-2006, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"

#include "mboxgetmessage.H"
#include "file.H"
#include "rfc2045/rfc2045.h"

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

mail::mbox::GenericGetMessageTask::GenericGetMessageTask(
	mail::mbox &mboxAccount,
	mail::callback &callbackArg,
	string uidArg,
	size_t messageNumberArg,
	bool peekArg,
	std::shared_ptr<rfc822::fdstreambuf> *fdretArg,
	std::shared_ptr<rfc2045::entity> *structretArg)
	: LockTask{mboxAccount, callbackArg},
	  uid{uidArg},
	  messageNumber{messageNumberArg},
	  peek{peekArg},
	  fdret{fdretArg},
	  structret{structretArg}
{
}

mail::mbox::GenericGetMessageTask::~GenericGetMessageTask()=default;


bool mail::mbox::GenericGetMessageTask::locked(mail::file &file)
{
	if (!mboxAccount.verifyUid(uid, messageNumber, callback))
	{
		done();
		return true;
	}

	mail::ptr<mail::mbox> me= &mboxAccount;

	size_t realMsgNum=mboxAccount.uidmap.find(uid)->second;

	off_t startingPos=mboxAccount.folderMessageIndex[realMsgNum]
		.startingPos;

	if (fseek(file, startingPos, SEEK_SET) < 0)
	{
		fail(strerror(errno));
		return true;
	}
	tempFp=rfc822::fdstreambuf::tmpfile();

	if (tempFp.error())
	{
		fail(strerror(errno));
		return true;
	}

	off_t endingPos=0;

	if (realMsgNum + 1 >= mboxAccount.folderMessageIndex.size())
	{
		struct stat stat_buf;

		if (fstat(fileno(static_cast<FILE *>(file)), &stat_buf) < 0)
		{
			fail(strerror(errno));
			return true;
		}
		endingPos=stat_buf.st_size;
	}
	else
		endingPos=mboxAccount.folderMessageIndex[realMsgNum+1]
			.startingPos;

	if (endingPos >= startingPos)
		endingPos -= startingPos;
	else
		endingPos=0;

	off_t nextUpdatePos=startingPos;

	bool firstLine=true;

	rfc2045::entity_parser<false> parser;

	while (!feof(file) && !me.isDestroyed())
	{
		off_t pos=file.tell();

		// Progress update every 10K
		if (pos >= nextUpdatePos)
		{
			nextUpdatePos=pos + 10000;

			callback.reportProgress(pos - startingPos, endingPos,
						0, 1);
		}

		string line=file.getline();

		if (line.size() == 0 && feof(file))
			break;

		if (strncmp(line.c_str(), "From ", 5) == 0)
		{
			if (firstLine)
				continue;

			break;
		}

		if (firstLine)
		{
			firstLine=false;

			if (mail::mboxMagicTag(line,
					       mboxAccount.keywordHashtable)
			    .good())
				continue; // Ignore magic tag line.
		}

		const char *p=line.c_str();

		if (*p == '>')
		{
			while (*p == '>')
				p++;

			if (strncmp(p, "From ", 5) == 0)
				line=line.substr(1);
		}

		line += "\n";

		auto n=line.size();
		p=line.c_str();

		while (n)
		{
			auto d=tempFp.sputn(p, n);

			if (d <= 0)
			{
				fail(strerror(errno));
				return true;
			}

			p += d;
			n -= d;
		}

		parser.parse(line.begin(), line.end());
	}

	auto tempStruct=parser.parsed_entity();

	if (tempFp.error())
	{
		fail(strerror(errno));
		return true;
	}

	fcntl(tempFp.fileno(), F_SETFD, FD_CLOEXEC);

	mboxAccount.cachedMessageUid=uid;
	mboxAccount.cachedMessageFp=std::make_shared<rfc822::fdstreambuf>(
		std::move(tempFp)
	);
	mboxAccount.cachedMessageRfcp=std::make_shared<rfc2045::entity>(
		std::move(tempStruct)
	);

	if (fdret)
		*fdret=mboxAccount.cachedMessageFp;
	if (structret)
		*structret=mboxAccount.cachedMessageRfcp;

	if (mboxAccount.currentFolderCallback && !peek &&
	    mboxAccount.index[messageNumber].unread)
	{
		mboxAccount.index[messageNumber].unread=false;
		mboxAccount.folderDirty=true;
		mboxAccount.currentFolderCallback->messageChanged(messageNumber);
	}

	callback.success("OK");
	done();
	return true;
}

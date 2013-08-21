/*
** Copyright 2002-2006, Double Precision Inc.
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

mail::mbox::GenericGetMessageTask::GenericGetMessageTask(mail::mbox &mboxAccount,
							 mail::callback
							 &callbackArg,
							 string uidArg,
							 size_t
							 messageNumberArg,
							 bool peekArg,
							 int *fdretArg,
							 struct rfc2045
							 **structretArg)
	: LockTask(mboxAccount, callbackArg),
	  uid(uidArg),
	  messageNumber(messageNumberArg),
	  peek(peekArg),
	  fdret(fdretArg),
	  structret(structretArg),

	  tempFp(NULL),
	  tempStruct(NULL)
{
}

mail::mbox::GenericGetMessageTask::~GenericGetMessageTask()
{
	// tempFp/tempStruct should be null, if all's well.  If not, clean up
	// after outselves.

	if (tempFp)
		fclose(tempFp);

	if (tempStruct)
		rfc2045_free(tempStruct);
}

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

	if (fseek(file, startingPos, SEEK_SET) < 0 ||
	    (tempFp=tmpfile()) == NULL ||
	    (tempStruct=rfc2045_alloc()) == NULL)
	{
		fail(strerror(errno));
		return true;
	}

	off_t endingPos=0;

	if (realMsgNum + 1 >= mboxAccount.folderMessageIndex.size())
	{
		struct stat stat_buf;

		if (fstat(fileno(static_cast<FILE *>(file)), &stat_buf) < 0)
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

		if (fwrite(&line[0], line.size(), 1, tempFp) != 1)
			; // Ignore gcc warning

		rfc2045_parse(tempStruct, &line[0], line.size());
	}

	if (fflush(tempFp) < 0 || ferror(tempFp) < 0)
	{
		fail(strerror(errno));
		return true;
	}

	mboxAccount.cachedMessageUid="";
	if (mboxAccount.cachedMessageFp)
		fclose(mboxAccount.cachedMessageFp);
	if (mboxAccount.cachedMessageRfcp)
		rfc2045_free(mboxAccount.cachedMessageRfcp);

	mboxAccount.cachedMessageFp=NULL;
	mboxAccount.cachedMessageRfcp=NULL;

	mboxAccount.cachedMessageUid=uid;
	mboxAccount.cachedMessageFp=tempFp;
	mboxAccount.cachedMessageRfcp=tempStruct;

	fcntl(fileno(tempFp), F_SETFD, FD_CLOEXEC);

	tempFp=NULL;
	tempStruct=NULL;

	if (fdret)
		*fdret=fileno(mboxAccount.cachedMessageFp);
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


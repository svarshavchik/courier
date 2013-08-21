/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "mboxread.H"
#include "mboxmagictag.H"
#include "file.H"

#include <ctype.h>
#include <errno.h>

using namespace std;

mail::mbox::GenericReadTask::GenericReadTask(mail::mbox &mboxAccount,
					     mail::callback::message
					     &callbackArg,
					     string uidArg,
					     size_t messageNumberArg,
					     bool peekArg,
					     mail::readMode
					     readTypeArg)
	: LockTask(mboxAccount, callbackArg),
	  callback(callbackArg),
	  uid(uidArg),
	  messageNumber(messageNumberArg),
	  peek(peekArg),
	  readType(readTypeArg)
{
}

mail::mbox::GenericReadTask::~GenericReadTask()
{
}

bool mail::mbox::GenericReadTask::locked(mail::file &file)
{
	if (!mboxAccount.verifyUid(uid, messageNumber, callback))
	{
		done();
		return true;
	}

	mail::ptr<mail::mbox> me= &mboxAccount;

	off_t p=mboxAccount.folderMessageIndex[mboxAccount.uidmap.find(uid)->second]
		.startingPos;

	if (fseek(file, p, SEEK_SET) < 0)
	{
		fail(strerror(errno));
		return true;
	}

	bool firstLine=true;
	bool inHeaders=true;

	string header="";

	while (!feof(file) && !me.isDestroyed())
	{
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
				continue; // Ignore the magic tag line
		}

		if (inHeaders)
		{
			if (line.size() == 0)
				inHeaders=false;

			if (readType == mail::readHeadersFolded)
			{
				const char *p=line.c_str();

				if (line.size() > 0 &&
				    unicode_isspace((unsigned char)*p))
				{
					while (unicode_isspace((unsigned char)*p))
						p++;

					header += " ";
					header += p;
					continue;
				}

				if (header.size() > 0)
				{
					header += "\n";
					callback.messageTextCallback(messageNumber,
								     header);
				}
				header=line;
				continue;
			}

			line += "\n";
			callback.messageTextCallback(messageNumber, line);
			continue;
		}

		if (readType != mail::readBoth &&
		    readType != mail::readContents)
		{
			header="";
			break;
		}

		line += "\n";

		const char *p=line.c_str();

		if (*p == '>')
		{
			while (*p == '>')
				p++;

			if (strncmp(p, "From ", 5) == 0)
				line=line.substr(1);
			// Dequote a >From_ line.
		}

		callback.messageTextCallback(messageNumber, line);
	}

	if (header.size() > 0) // Only headers, something got left over.
	{
		header += "\n";
		callback.messageTextCallback(messageNumber, header);
	}

	if (!me.isDestroyed() && !peek &&
	       mboxAccount.index[messageNumber].unread)
	{
		mboxAccount.index[messageNumber].unread=false;
		mboxAccount.folderDirty=true;
		if (mboxAccount.currentFolderCallback)
			mboxAccount.currentFolderCallback
				->messageChanged(messageNumber);
	}


	callback.success("Folder locked.");
	done();
	return true;
}

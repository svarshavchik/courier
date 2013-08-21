/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include <cstring>

#include "mbox.H"
#include "mboxexpunge.H"
#include "expungelist.H"
#include "file.H"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

mail::mbox::ExpungeTask::ExpungeTask(mail::mbox &mboxAccount,
				     mail::callback &callback,
				     bool purgeDeletedArg,
				     const vector<size_t> *listPtr)
	: LockTask(mboxAccount, callback),
	  purgeDeleted(purgeDeletedArg)
{
	if (purgeDeleted)
	{
		if (!listPtr)
		{
			// Default - delete msgs marked for deletion.

			vector<mail::messageInfo>::iterator b, e;

			b=mboxAccount.index.begin();
			e=mboxAccount.index.end();

			while (b != e)
			{
				if (b->deleted)
					deleteList.insert(b->uid);
				b++;
			}
		}
		else // Explicitly delete these msgs only
		{
			vector<size_t>::const_iterator b, e;
			b=listPtr->begin();
			e=listPtr->end();

			while (b != e)
			{
				size_t n= *b++;

				if (n < mboxAccount.index.size())
					deleteList.insert(mboxAccount.index[n]
							  .uid);
			}
		}
	}
}

mail::mbox::ExpungeTask::~ExpungeTask()
{
}

//
// Override LockTask::locked().  Create a scratch file, then call the
// superclass, which invokes locked(mail::file), which runs mboxAccount.scan() to
// do the actual deed.
//
// After the mailbox is purged, the folder index is compared to the real
// folder index, and removed messages are removed from the index, the any
// new messages are added.
//

bool mail::mbox::ExpungeTask::locked(mail::mbox::lock &mlock, string path)
{
	struct stat stat_buf;

	// Rewrite the file to the following scratch file:

	string scratchFile=path + "~";

	if (mboxAccount.currentFolderReadOnly)
	{
		callback.success("Folder opened in read-only mode.");
		done();
		return true;
	}

	mail::ptr<mail::mbox> monitor(&mboxAccount);


	int f= ::open(scratchFile.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0600);

	if (f < 0)
	{
		fail(strerror(errno));
		return true;
	}

	if (fstat(mlock.getfd(), &stat_buf) < 0 ||
	    chmod(scratchFile.c_str(), stat_buf.st_mode & 0777) < 0)
	{
		close(f);
		unlink(scratchFile.c_str());
		fail(strerror(errno));
		return true;
	}

	bool rc;

	bool flag;

	try {
		mail::file wf(f, "w");

		close(f);
		f= -1;

		saveFile= &wf;
		ok= &flag;
		flag=false;

		rc= LockTask::locked(mlock, path);
	} catch (...) {
		if (f >= 0)
			close(f);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	if (rc)
	{
		if (flag)
		{
			rename(scratchFile.c_str(), path.c_str());

			if (purgeDeleted && !monitor.isDestroyed())
			{
				size_t i=mboxAccount.index.size();

				mail::expungeList removedList;

				while (mboxAccount.index.size() > 0 && i > 0)
				{
					if (mboxAccount.uidmap.count(mboxAccount.index[--i]
								     .uid) > 0)
						continue;

					mboxAccount.index
						.erase(mboxAccount.index
						       .begin()+i);

					removedList << i;
				}

				removedList >>
					mboxAccount.currentFolderCallback;
			}

			// Now, check for new mail.

			if (!monitor.isDestroyed())
				mboxAccount.checkNewMail();

			if (!monitor.isDestroyed())
			{
				mboxAccount.folderDirty=false;
				if (mboxAccount.newMessages)
				{
					mboxAccount.newMessages=false;
					if (mboxAccount.currentFolderCallback)
						mboxAccount.currentFolderCallback
							->newMessages();
				}
			}

			callback.success("Folder expunged.");
			done();
		}
		else
			unlink(scratchFile.c_str());
	}
	return rc;
}

bool mail::mbox::ExpungeTask::locked(mail::file &file)
{

	if (!mboxAccount.scan(file, saveFile, true,
			      purgeDeleted ? &deleteList:NULL,
			      true,
			      &callback))
	{
		fail(strerror(errno));
		return true;
	}

	*ok=true;

	return true;
}

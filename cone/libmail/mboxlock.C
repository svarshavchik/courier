/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "mboxlock.H"
#include "mboxopen.H"
#include "file.H"
#include <cstring>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

//
// Stub callback used for the OpenTask that updates the internal folder
// index.  When OpenTask completes this object goes back on the queue.
//


mail::mbox::LockTask::reopenCallback::reopenCallback(mail::mbox::LockTask *t)
	: task(t)
{
}

mail::mbox::LockTask::reopenCallback::~reopenCallback()
{
}

void mail::mbox::LockTask::reopenCallback::success(string msg)
{
	try {
		task->mboxAccount.installTask(task);
	} catch (...) {
		delete task;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::mbox::LockTask::reopenCallback::fail(string msg)
{
	task->fail(msg);
}

////////////////////////////////////////////////////////////////////////

mail::mbox::LockTask::LockTask(mail::mbox &mboxAccount,
		   mail::callback &callbackArg, string filenameArg)
	: TimedTask(mboxAccount, callbackArg), reopen(this),
	  filename(filenameArg)
{
}

mail::mbox::LockTask::~LockTask()
{
}

bool mail::mbox::LockTask::doit()
{
	for (;;)
	{
		string pathStr=filename;
		bool ro=false;

		if (pathStr.size() == 0)
		{
			pathStr=mboxAccount.currentFolder;
			ro=mboxAccount.currentFolderReadOnly;
		}

		if (pathStr.size() == 0)
		{
			callback.success("Folder locked");
			done();
			return true;
		}

		struct stat stat_buf;

		if (pathStr == "INBOX")
			pathStr=mboxAccount.inboxMailboxPath;

		if (mboxAccount.multiLockLock &&
		    mboxAccount.multiLockFilename == pathStr)
		{
			return locked( *mboxAccount.multiLockLock, pathStr);
		}

		mail::mbox::lock mailbox_lock( pathStr );

		if (!mailbox_lock(ro))
		{
			// TODO: fail if EEXIST

			if (errno != EAGAIN && errno != EEXIST)
			{
				fail(pathStr + ": " + strerror(errno));
				return true;
			}
			return false;
		}

		if (filename.size() > 0)
			// Locked another folder, no need
			// to check.
		{
			try {
				return locked(mailbox_lock, pathStr);
			} catch (...) {
				fail("Exception caught while accessing folder.");
			}
			return true;
		}

		if (fstat(mailbox_lock.getfd(), &stat_buf) < 0)
		{
			fail(pathStr + ": " + strerror(errno));
			return true;
		}

		if (stat_buf.st_size != mboxAccount.folderSavedSize ||
		    stat_buf.st_mtime != mboxAccount.folderSavedTimestamp)
			break;

		try {
			return locked(mailbox_lock, pathStr);
		} catch (...) {
			fail("Exception caught while accessing folder.");
		}
		return true;
	}

	// Uh-oh, something happened to the folder.

	OpenTask *t=new OpenTask(mboxAccount, mboxAccount.currentFolder, reopen, NULL);

	if (!t)
	{
		fail(mboxAccount.currentFolder + ": " + strerror(errno));
		return true;
	}

	try {
		mboxAccount.tasks.pop();
	} catch (...) {
		delete t;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	try {
		mboxAccount.installTask(t);
	} catch (...) {
		fail("Exception caught while accessing folder.");
	}

	return true;
}

bool mail::mbox::LockTask::locked(mail::mbox::lock &mlock, string path)
{
	mail::file mboxFp(mlock.getfd(),
			  mlock.readOnly() ? "r":"r+");

	if (!mboxFp)
	{
		fail(strerror(errno));
		return true;
	}

	return (locked(mboxFp));
}

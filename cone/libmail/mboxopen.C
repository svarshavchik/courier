/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include <cstring>

#include "mbox.H"
#include "mboxopen.H"
#include "mboxsighandler.H"
#include "mboxmagictag.H"
#include "file.H"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>

using namespace std;

mail::mbox::OpenTask::OpenTask(mail::mbox &mboxAccount,
			       string folderArg, mail::callback &callbackArg,
			       mail::callback::folder *openCallbackArg)
	: TimedTask(mboxAccount, callbackArg),
	  folder(folderArg),
	  openCallback(openCallbackArg)
{
}

mail::mbox::OpenTask::~OpenTask()
{
}

bool mail::mbox::OpenTask::doit()
{
	bool isInbox= folder == "INBOX";

	if (folder == "")
	{
		fail(strerror(ENOENT));
		return true;
	}

	mail::mbox::lock mailbox_lock(isInbox ? mboxAccount.inboxMailboxPath:folder);

	mail::mbox::lock inbox_lock(mboxAccount.inboxSpoolPath); // Just in case

	struct stat stat_buf;

	if (isInbox)
	{
		// We must have a r/w lock on the mail spool file.

		if (!inbox_lock(false))
		{
			if (errno == EAGAIN)
				return false; // Deferral, try again later.

			// ENOENT is ok, the spool file does not exist.

			if (errno != ENOENT)
			{
				fail(strerror(errno));
				return true;
			}
			isInbox=false;

			// spool file not created yet, new account probably.
		}

		// Make sure $HOME/mboxAccount exists.
		int createMbox=::open(mboxAccount.inboxMailboxPath.c_str(),
				    O_RDWR|O_CREAT, 0600);

		if (createMbox < 0)
		{
			fail(strerror(errno));
			return true;
		}
		close(createMbox);
	}

	bool hasExistingFolder= openCallback == NULL;

	// Opening an existing folder, in read-only mode.  A read-only lock
	// will be fine.

	bool ro=false;

	if (hasExistingFolder && mboxAccount.currentFolderReadOnly)
	{
		if (!mailbox_lock(true))
		{
			if (errno != EAGAIN && errno != EEXIST)
			{
				fail(strerror(errno));
				return true;
			}
			return false;
		}
	}

	// Try for a read/write lock

	else if (!mailbox_lock(false))
	{
		// If not permission denied, we're in trouble.

		if (errno != EPERM && errno != EACCES)
		{
			if (errno != EAGAIN && errno != EEXIST)
			{
				fail(strerror(errno));
				return true;
			}

			// EAGAIN means try again

			return false;

		}

		if (isInbox) // Must have r/w to inbox
		{
			if (access(mboxAccount.inboxSpoolPath.c_str(), W_OK))
			{
				fail("Invalid permissions on system mailbox.");
				return true;
			}
			return false;
		}

		// Try a read-only lock, then.

		if (!mailbox_lock(true))
		{
			if (errno != EAGAIN)
			{
				fail(strerror(errno));
				return true;
			}
			return false;
		}
		ro=true;
	}

	// Lock-n-load

	int mboxfd=mailbox_lock.getfd();

	mail::file mboxFp(mboxfd, isInbox ? "r+":"r");

	if (!mboxAccount.scan(mboxFp, NULL, openCallback == NULL,
			      NULL,
			      false, &callback))
	{
		fail(errno == EINVAL
		     ? "File does not appear to be a mail folder."
		     : strerror(errno));
		return true;
	}

	if (isInbox)
	{
		// Copy new mail.

		mail::mbox::sighandler updating(mboxfd); // Critical section

		mail::file spoolFp(inbox_lock.getfd(), "r");

		if (!mboxAccount.scan(spoolFp, &mboxFp, openCallback == NULL,
				      NULL,
				      false, &callback))
		{
			updating.rollback();

			fail(errno == EINVAL
			     ? "File does not appear to be a mail folder."
			     : strerror(errno));
			return true;
		}

		updating.block();
		if (ftruncate(inbox_lock.getfd(), 0) < 0)
			; // Ignore gcc warning

		struct utimbuf ut;

		if (fstat(mboxfd, &stat_buf) < 0)
		{
			fail(strerror(errno));
			return true;
		}

		ut.modtime=--stat_buf.st_mtime;
		ut.actime= ut.modtime;

		if (utime(mboxAccount.inboxMailboxPath.c_str(), &ut) < 0)
		{
			fail(strerror(errno));
			return true;
		}
	}
	else if (fstat(mboxfd, &stat_buf) < 0)
	{
		fail(strerror(errno));
		return true;
	}

	// Save folder particulars, to detect someone else's changes.

	mboxAccount.folderSavedTimestamp= stat_buf.st_mtime;
	mboxAccount.folderSavedSize=stat_buf.st_size;

	const char *okmsg="OK";

	if (openCallback) // We're doing mail::folder::open()
	{
		mboxAccount.currentFolderCallback=openCallback;
		mboxAccount.currentFolderReadOnly=ro;

		mboxAccount.index.clear();

		vector<mboxMessageIndex>::iterator b, e;

		b=mboxAccount.folderMessageIndex.begin();
		e=mboxAccount.folderMessageIndex.end();

		while (b != e)
			mboxAccount.index.push_back( (*b++).tag.getMessageInfo());

		mboxAccount.currentFolder=folder;

		if (ro)
			okmsg="WARNING: Folder opened in read-only mode.";
	}

	opened(okmsg, callback);
	return true;
}

void mail::mbox::OpenTask::opened(const char *okmsg, mail::callback &callback)
{
	done();
	callback.success("Folder opened");
}

//
// Slight variation on the above - explicit new mail check.
//


mail::mbox::CheckNewMailTask::CheckNewMailTask(mail::mbox &mboxAccount,
					       string folderArg,
					       mail::callback &callbackArg,
					       mail::callback::folder
					       *openCallbackArg)
	: OpenTask(mboxAccount, folderArg, callbackArg, openCallbackArg)
{
}

mail::mbox::CheckNewMailTask::~CheckNewMailTask()
{
}

void mail::mbox::CheckNewMailTask::opened(const char *okmsg,
					  mail::callback &callback)
{
	mboxAccount.checkNewMail();

	if (mboxAccount.newMessages)
	{
		mboxAccount.newMessages=false;
		if (mboxAccount.currentFolderCallback)
			mboxAccount.currentFolderCallback->newMessages();
	}
	OpenTask::opened(okmsg, callback);
}



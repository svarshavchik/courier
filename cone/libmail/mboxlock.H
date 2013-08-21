/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mboxlock_H
#define libmail_mboxlock_H

#include "libmail_config.h"
#include "mbox.H"

////////////////////////////////////////////////////////////////////////
//
// Convenient superclass that locks the mailbox file in a consistent
// state.  First, a mailbox lock is obtained.  After the mailbox is locked,
// its timestamp/size is checked against the cached timestamp/size.
// If a difference is found this task is taken off the queue, and an OpenTask
// is added to the mboxAccount queue in order to update our mailbox index.  OpenTask
// receives a callback object that places this task back onto the mboxAccount queue.

LIBMAIL_START

class file;

class mbox::LockTask : public mbox::TimedTask {

	class reopenCallback : public mail::callback {
		LockTask *task;

		void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal)
		{} // TODO

	public:
		reopenCallback(LockTask *task);
		~reopenCallback();
		void success(std::string);
		void fail(std::string);
	};

	reopenCallback reopen;

	std::string filename;
	// Filename to lock.  Empty string: lock current folder

public:
	LockTask(mbox &mboxAccount,		// Account
		 mail::callback &callbackArg,
		 std::string filenameArg="");
	// Empty string - current folder, non-empty -- some other file

	~LockTask();

	virtual bool locked(mbox::lock &mlock, std::string path);
	// Mailbox file is now open.  Create a file, then invoke
	// the following:

	virtual bool locked(file &)=0;
	bool doit();
};

LIBMAIL_END

#endif

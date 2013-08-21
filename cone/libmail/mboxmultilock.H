/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mboxmultilock_H
#define libmail_mboxmultilock_H

#include "libmail_config.h"
#include "mboxlock.H"

//
// A request for multiple messages would normally result in a separate lock
// attempt for each message.  Horribly inefficient.
//
// What we do is open a lock first, and save it in the mail::mbox object.
// Before we actually try to lock something check if this lock is already
// obtained.  If so, just reuse the lock
//

LIBMAIL_START

class mbox::MultiLock : public mbox::LockTask {

	bool locked(file &fileArg); // No-op

public:
	MultiLock(mbox &mboxAccount,
		  mail::callback &callbackArg);
	~MultiLock();

	bool locked(mbox::lock &mlock, std::string path);
};

class mbox::MultiLockGenericAttributes : public callback {

	ptr<mbox> mboxAccount;
	std::vector<size_t> messages;
	MessageAttributes attributes;
	mail::callback::message &callback;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	MultiLockGenericAttributes(mbox &mboxAccountArg,
				   const std::vector<size_t> &messagesArg,
				   MessageAttributes attributesArg,
				   mail::callback::message &callbackArg);
	~MultiLockGenericAttributes();

	void success(std::string message);
	void fail(std::string message);
};

class mbox::MultiLockGenericMessageRead : public callback {

	ptr<mbox> mboxAccount;
	const std::vector<size_t> messages;
	bool peek;

	enum mail::readMode readType;
	bool justHeader;
	bool justContents;
	callback::message &callback;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	MultiLockGenericMessageRead(mbox &mboxAccountArg,
				    const std::vector<size_t> &messagesArg,
				    bool peekArg,
				    enum mail::readMode readTypeArg,
				    callback::message &callbackArg);
	~MultiLockGenericMessageRead();
	void success(std::string message);
	void fail(std::string message);
};

//
// After we're done with a multilocked request, we can free the lock
//

class mbox::MultiLockRelease : public callback::message {

	callback::message * volatile origCallback;
	ptr<mbox> mboxAccount;
public:
	MultiLockRelease(mbox &mboxAccountArg,
			 callback::message &origCallbackArg);
	~MultiLockRelease();

	void success(std::string message);
	void fail(std::string message);

	void messageEnvelopeCallback(size_t messageNumber,
				     const envelope &envelope);

	void messageArrivalDateCallback(size_t messageNumber,
					time_t datetime);

	void messageSizeCallback(size_t messageNumber,
				 unsigned long size);

	void messageStructureCallback(size_t messageNumber,
				      const mimestruct &messageStructure);

	void messageTextCallback(size_t n, std::string text);
	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

};

LIBMAIL_END

#endif

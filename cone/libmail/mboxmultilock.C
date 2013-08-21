/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "mboxmultilock.H"

using namespace std;

mail::mbox::MultiLock::MultiLock(mail::mbox &mboxAccount,
				 mail::callback &callbackArg)
	:LockTask(mboxAccount, callbackArg)
{
}

mail::mbox::MultiLock::~MultiLock()
{
}

bool mail::mbox::MultiLock::locked(mail::mbox::lock &mlock,
				   string path)
{
	if (mboxAccount.multiLockLock &&
	    mboxAccount.multiLockFilename == path) // This is me
		;
	else
	{
		if (mboxAccount.multiLockLock)
		{
			delete mboxAccount.multiLockLock;
			mboxAccount.multiLockLock=NULL;
		}

		mboxAccount.multiLockFilename=path;
		mboxAccount.multiLockLock=mlock.copy();
	}

	callback.success("Folder locked");
	done();
	return true;
}

bool mail::mbox::MultiLock::locked(file &fileArg) // No-op
{
	return true;
}

/////////////////////////////////////////////////////////////////////////////
//
// After obtaining a MultiLock, finally do genericMessageAttributes/Read

mail::mbox::MultiLockGenericAttributes
::MultiLockGenericAttributes(mbox &mboxAccountArg,
			      const std::vector<size_t> &messagesArg,
			      MessageAttributes attributesArg,
			      mail::callback::message &callbackArg)
	: mboxAccount(&mboxAccountArg),
	  messages(messagesArg),
	  attributes(attributesArg),
	  callback(callbackArg)
{
}

mail::mbox::MultiLockGenericAttributes::~MultiLockGenericAttributes()
{
}

void mail::mbox::MultiLockGenericAttributes::success(std::string message)
{
	try {
		if (mboxAccount.isDestroyed())
		{
			fail("Operation aborted.");
			return;
		}

		mboxAccount->genericAttributes(&*mboxAccount,
					       &*mboxAccount,
					       messages,
					       attributes,
					       callback);
		delete this;
	} catch (...) {
		fail("An exception occured in mbox::MultiLockGenericAttributes");
	}
}

void mail::mbox::MultiLockGenericAttributes::fail(string message)
{
	try {
		callback.fail(message);
		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::mbox::MultiLockGenericAttributes
::reportProgress(size_t bytesCompleted,
		 size_t bytesEstimatedTotal,

		 size_t messagesCompleted,
		 size_t messagesEstimatedTotal)
{
	callback.reportProgress(bytesCompleted, bytesEstimatedTotal,
				messagesCompleted, messagesEstimatedTotal);
}

//

mail::mbox::MultiLockGenericMessageRead
::MultiLockGenericMessageRead(mbox &mboxAccountArg,
			      const vector<size_t> &messagesArg,
			      bool peekArg,
			      enum mail::readMode readTypeArg,
			      mail::callback::message &callbackArg)
	: mboxAccount(&mboxAccountArg),
	  messages(messagesArg),
	  peek(peekArg),
	  readType(readTypeArg),
	  callback(callbackArg)
{
}

mail::mbox::MultiLockGenericMessageRead::~MultiLockGenericMessageRead()
{
}

void mail::mbox::MultiLockGenericMessageRead::success(std::string message)
{
	try {
		if (mboxAccount.isDestroyed())
		{
			fail("Operation aborted.");
			return;
		}

		mboxAccount->genericReadMessageContent(&*mboxAccount,
						       &*mboxAccount,
						       messages,
						       peek,
						       readType,
						       callback);
		delete this;
	} catch (...) {
		fail("An exception occured in mbox::MultiLockGenericAttributes");
	}
}

void mail::mbox::MultiLockGenericMessageRead::fail(std::string message)
{
	try {
		callback.fail(message);
		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::mbox::MultiLockGenericMessageRead
::reportProgress(size_t bytesCompleted,
		 size_t bytesEstimatedTotal,

		 size_t messagesCompleted,
		 size_t messagesEstimatedTotal)
{
	callback.reportProgress(bytesCompleted, bytesEstimatedTotal,
				messagesCompleted, messagesEstimatedTotal);
}

/////////////////////////////////////////////////////////////////////////////
//
// Cleanup.

mail::mbox::MultiLockRelease::MultiLockRelease(mbox &mboxAccountArg,
					       callback::message
					       &origCallbackArg)
	: origCallback(&origCallbackArg),
	  mboxAccount(&mboxAccountArg)
{
}

mail::mbox::MultiLockRelease::~MultiLockRelease()
{
	if (origCallback)
	{
		callback *p=origCallback;

		origCallback=NULL;

		try {
			p->fail("Request aborted.");
			delete p;
		} catch (...) {
			delete p;
		}
	}
}

void mail::mbox::MultiLockRelease::success(std::string message)
{
	if (!mboxAccount.isDestroyed() &&
	    mboxAccount->multiLockLock)
	{
		delete mboxAccount->multiLockLock;
		mboxAccount->multiLockLock=NULL;
	}

	callback *p=origCallback;

	origCallback=NULL;

	try {
		p->success(message);
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
	delete this;
}

void mail::mbox::MultiLockRelease::fail(std::string message)
{
	if (!mboxAccount.isDestroyed() &&
	    mboxAccount->multiLockLock)
	{
		delete mboxAccount->multiLockLock;
		mboxAccount->multiLockLock=NULL;
	}

	callback *p=origCallback;

	origCallback=NULL;

	try {
		if (p)
			p->fail(message);
		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::mbox::MultiLockRelease::messageEnvelopeCallback(size_t
							   messageNumber,
							   const envelope
							   &envelopeArg)
{
	if (origCallback)
		origCallback->messageEnvelopeCallback(messageNumber,
						      envelopeArg);
}

void mail::mbox::MultiLockRelease::messageArrivalDateCallback(size_t
							      messageNumber,
							      time_t datetime)
{
	if (origCallback)
		origCallback->messageArrivalDateCallback(messageNumber,
							 datetime);
}

void mail::mbox::MultiLockRelease::messageSizeCallback(size_t messageNumber,
						       unsigned long size)
{
	if (origCallback)
		origCallback->messageSizeCallback(messageNumber,
						  size);
}

void mail::mbox::MultiLockRelease::messageStructureCallback(size_t
							    messageNumber,
							    const mimestruct &
							    messageStructure)
{
	if (origCallback)
		origCallback->messageStructureCallback(messageNumber,
						       messageStructure);
}

void mail::mbox::MultiLockRelease::messageTextCallback(size_t n,
						       string text)
{
	if (origCallback)
		origCallback->messageTextCallback(n, text);
}

void mail::mbox::MultiLockRelease::reportProgress(size_t bytesCompleted,
						  size_t bytesEstimatedTotal,
						  size_t messagesCompleted,
						  size_t messagesEstimatedTot)
{
	if (origCallback)
		origCallback->reportProgress(bytesCompleted,
					     bytesEstimatedTotal,
					     messagesCompleted,
					     messagesEstimatedTot);
}

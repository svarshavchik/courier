/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "addressbookopen.H"
#include "envelope.H"

using namespace std;

mail::addressbook::Open::Open(mail::addressbook *addressBookArg,
			      mail::callback &callbackArg)
	: addressBook(addressBookArg),
	  callback(callbackArg)
{
}

mail::addressbook::Open::~Open()
{
}

void mail::addressbook::Open::success(string msg)
{
	(this->*successFunc)(msg);
}

void mail::addressbook::Open::fail(string msg)
{
	try {
		callback.fail(msg);
		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

// 1. Select the folder

void mail::addressbook::Open::go()
{
	successFunc= &mail::addressbook::Open::opened;

	addressBook->folder->open(*this, NULL, *addressBook);
}

// 2. Read envelopes of all messages in the folder

void mail::addressbook::Open::opened(string successMsg)
{
	addressBook->index.clear();
	addressBook->index.insert(addressBook->index.end(),
				  addressBook->server->getFolderIndexSize(),
				  Index());

	vector<size_t> msgs;

	size_t i;

	for (i=0; i<addressBook->index.size(); i++)
		msgs.push_back(i);

	successFunc= &mail::addressbook::Open::readIndex;

	addressBook->server->readMessageAttributes(msgs,
						   addressBook->server
						   -> ENVELOPE,
						   *this);
}

void mail::addressbook::Open::readIndex(string msg)
{
	try {
		callback.success(msg);
		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::addressbook::Open
::messageEnvelopeCallback(size_t messageNumber,
			  const class mail::envelope &envelope)
{
	addressBook->setIndex(messageNumber, envelope.subject);
}

void mail::addressbook::Open::reportProgress(size_t bytesCompleted,
					     size_t bytesEstimatedTotal,

					     size_t messagesCompleted,
					     size_t messagesEstimatedTotal)
{
	callback.reportProgress(bytesCompleted, bytesEstimatedTotal,
				messagesCompleted, messagesEstimatedTotal);
}

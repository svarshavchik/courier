/*
** Copyright 2002, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#ifndef libmail_addressbookopen_H
#define libmail_addressbookopen_H

#include "addressbook.H"
#include "structure.H"

//
// Open the address book, as follows:
//
// 1. Select the folder.
//
// 2. Read envelopes of all messages in the folder

class mail::addressbook::Open : public mail::callback::message {

	mail::ptr<mail::addressbook> addressBook;
	mail::callback &callback;

	void success(std::string successMsg) override;
	void (mail::addressbook::Open::*successFunc)(std::string);

	void fail(std::string failMsg) override;

	void opened(std::string successMsg);
	void readIndex(std::string successMsg);

	void messageEnvelopeCallback(size_t messageNumber,
				     const class mail::envelope &envelope) override;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal) override;

public:
	Open(mail::addressbook *addressBookArg,
	     mail::callback &callbackArg);
	~Open();

	void go();
};

#endif

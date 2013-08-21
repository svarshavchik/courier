/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_addressbookadd_H
#define libmail_addressbookadd_H

#include "addressbook.H"
#include "structure.H"
#include "addmessage.H"

//
// Add new address book entry.

class mail::addressbook::Add : public mail::callback::message {

	mail::ptr<mail::addressbook> addressBook;

	size_t totCnt;
	size_t currentNum;

	std::list<mail::addressbook::Entry> newEntries;
	std::string oldUid;
	mail::callback &callback;

	mail::addMessage *addMessage;
	size_t dummyRet;

	std::string nickname;
	std::vector<size_t> multipart_params;

	void success(std::string successMsg);
	void (mail::addressbook::Add::*successFunc)(std::string);
	void addedIntro(std::string successMsg);
	void addedBeef(std::string successMsg);
	void addedAll(std::string successMsg);
	void added(std::string successMsg);
	void checked(std::string successMsg);
	void reindexed(std::string successMsg);

	void fail(std::string failMsg);

	void messageEnvelopeCallback(size_t messageNumber,
				     const class mail::envelope &envelope);
	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);
public:
	Add(mail::addressbook *addressBookArg,
	    mail::addressbook::Entry entryArg,
	    std::string oldUidArg,
	    mail::callback &callbackArg);

	Add(mail::addressbook *addressBookArg,
	    std::list<mail::addressbook::Entry> &entries,
	    mail::callback &callbackArg);
	~Add();

	void go();
};

#endif

/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "addressbookadd.H"
#include "rfc2047encode.H"
#include "attachments.H"
#include "headers.H"
#include "envelope.H"
#include "rfcaddr.H"
#include "misc.H"
#include "unicode/unicode.h"
#include <ctype.h>
#include <vector>
#include <errno.h>
#include <cstring>

using namespace std;

mail::addressbook::Add::Add(mail::addressbook *addressBookArg,
			    mail::addressbook::Entry entryArg,
			    string oldUidArg,
			    mail::callback &callbackArg)
	: addressBook(addressBookArg),
	  oldUid(oldUidArg),
	  callback(callbackArg),
	  addMessage(NULL)
{
	totCnt=1;
	currentNum=0;
	newEntries.push_back(entryArg);
}

mail::addressbook::Add::Add(mail::addressbook *addressBookArg,
			    std::list<mail::addressbook::Entry> &entries,
			    mail::callback &callbackArg)
	: addressBook(addressBookArg),
	  callback(callbackArg),
	  addMessage(NULL)
{
	newEntries.insert(newEntries.end(),
			  entries.begin(),
			  entries.end());
	totCnt=newEntries.size();
	currentNum=0;
}

void mail::addressbook::Add::go()
{
	callback.reportProgress(0, 0, currentNum, totCnt);
	if (newEntries.empty())
	{
		successFunc= &mail::addressbook::Add::checked;
		addressBook->server->checkNewMail( *this );
		return;
	}

	multipart_params.clear();

	mail::addressbook::Entry &newEntry=newEntries.front();

	nickname=toutf8(newEntry.nickname);

	string::iterator b=nickname.begin(), e=nickname.end();

	while (b != e)
	{
		char c= *b++;

		if ( (int)(unsigned char)c < ' ' || c == '[' || c == ']')
		{
			fail("Invalid address book nickname.");
			return;
		}
	}

	if (newEntry.addresses.size() == 0)
	{
		fail("Invalid address.");
		return;
	}

	mail::addMessage *addp=
		addressBook->folder->addMessage(*this);

	if (!addp)
		return;

	time(&addp->messageDate);

	addMessage=addp;

	successFunc= &mail::addressbook::Add::addedIntro;

	multipart_params.push_back(0);

	mail::Header::list headers;

	headers << mail::Header::mime("Content-Type", "text/plain")
		("charset", "utf-8");


	mail::Attachment intro(headers,
			       "This message is used to store Libmail's"
			       " address book.  Please do not modify\n"
			       "this folder, and message!\n",
			       "utf-8");

	addMessage->assembleContent(multipart_params.end()[-1], intro,
				    *this);
}

void mail::addressbook::Add::addedIntro(string successMsg)
{
	mail::Header::list headers;
	mail::addressbook::Entry &newEntry=newEntries.front();

	headers << mail::Header::plain("Content-Type",
				       "text/x-libmail-addressbook");

				
	mail::Attachment addresses(headers, "VERSION: 2\n" +
				   mail::address::toString("Address: ",
							   newEntry.addresses)
				   + "\n", "utf-8", "8bit");

	successFunc= &mail::addressbook::Add::addedBeef;

	multipart_params.push_back(0);

	addMessage->assembleContent(multipart_params.end()[-1], addresses,
				    *this);
}

void mail::addressbook::Add::addedBeef(string successMsg)
{
	// Assemble the multipart message.

	mail::Header::list headers;

	vector<mail::emailAddress> from_addresses;

	from_addresses.push_back(mail::address("Libmail Address Book",
					       "libmail@localhost"));

	headers << mail::Header::addresslist("From", from_addresses);
	headers << mail::Header::encoded("Subject",
					 "[" + nickname + "]",
					 "utf-8");

	successFunc= &mail::addressbook::Add::addedAll;
	addMessage->assembleMultipart(dummyRet, headers, multipart_params,
				      "multipart/mixed", *this);
}

void mail::addressbook::Add::addedAll(string successMsg)
{
	newEntries.pop_front();
	successFunc= &mail::addressbook::Add::added;
	if (!addMessage->assemble())
	{
		addMessage->fail(strerror(errno));
		return;
	}
	addMessage->go();
}


mail::addressbook::Add::~Add()
{
}

void mail::addressbook::Add::success(string successMsg)
{
	(this->*successFunc)(successMsg);
}

//
// After adding a new entry, make sure it gets added to the index.
//

void mail::addressbook::Add::added(string successMsg)
{
	++currentNum;
	go();
}

//
// Now, update our address book index.

void mail::addressbook::Add::checked(string successMsg)
{
	size_t n=addressBook->index.size();

	size_t n2=addressBook->server->getFolderIndexSize();

	vector<size_t> msgNums;

	while (n < n2)
		msgNums.push_back(n++);

	if (msgNums.size() == 0)
	{
		reindexed(successMsg); // Unlikely
		return;
	}

	addressBook->index.insert(addressBook->index.end(),
				  msgNums.size(), Index());

	successFunc= &mail::addressbook::Add::reindexed;

	addressBook->server->readMessageAttributes(msgNums,
						   addressBook->server
						   -> ENVELOPE,
						   *this);
}

void mail::addressbook::Add::messageEnvelopeCallback(size_t messageNumber,
						     const mail::envelope
						     &envelope)
{
	addressBook->setIndex(messageNumber, envelope.subject);
}

void mail::addressbook::Add::reportProgress(size_t bytesCompleted,
					    size_t bytesEstimatedTotal,

					    size_t messagesCompleted,
					    size_t messagesEstimatedTotal)
{
	callback.reportProgress(bytesCompleted, bytesEstimatedTotal,
				messagesCompleted, messagesEstimatedTotal);
}

void mail::addressbook::Add::reindexed(string successMsg)
{
	try {
		// If this is meant to replace another entry, delete it then.

		if (oldUid.size() > 0)
		{
			addressBook->del(oldUid, callback);
		}
		else
			callback.success(successMsg);
		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::addressbook::Add::fail(string failMsg)
{
	try {
		callback.fail(failMsg);
		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}


/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_addressbookget_H
#define libmail_addressbookget_H

#include "addressbook.H"
#include "structure.H"
#include "rfcaddr.H"

#include <map>

//
// Extract an address book entry, as follows:
//
// 1. Read the message's MIME structure.
// 2. Verify the presence of text/x-libmail-addressbook content.
// 3. Read text/x-libmail-addressbook content.

template<class T>
class mail::addressbook::GetAddressList : public mail::callback::message {

	mail::ptr<mail::addressbook> addressBook;
	size_t msgNum;
	std::vector<T> &addrListRet;
	mail::callback &callback;
	mail::mimestruct mimeStructure;

	void success(std::string successMsg);
	void (mail::addressbook::GetAddressList<T>::*successFunc)(std::string);

	void readstructure(std::string successMsg);
	void readContents(std::string successMsg);

	void fail(std::string failMsg);

	void messageStructureCallback(size_t messageNumber,
				      const mail::mimestruct
				      &messageStructure);
	void messageTextCallback(size_t n, std::string text);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

	std::string linebuffer;
	// Reading x-libmail-addressbook content; partial line read.

	void addressBookLine(std::string text);

	std::map<std::string, std::string> addressBookLineMap;
	std::string lastAddressBookLine;
	// Name of the current x-libmail-addressbook header line being read.

public:
	GetAddressList(mail::addressbook *addressBookArg,
		       size_t msgNumArg,
		       std::vector<T> &addrListRetArg,
		       mail::callback &callbackArg);
	~GetAddressList();

	void go();
};

#endif

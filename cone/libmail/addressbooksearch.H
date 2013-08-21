/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_addressbooksearch_H
#define libmail_addressbooksearch_H

#include "libmail_config.h"
#include "addressbook.H"

#include "rfcaddr.H"

//
// Search the address book.  The main mail::addressbook object already compiled
// a list of address book entries, our job is to just to read them and
// combine them.
//

template<class T>
class mail::addressbook::Search : private mail::callback {

	mail::ptr<mail::addressbook> addressBook;
	std::vector<T> &addrList;
	mail::callback &callback;

	void success(std::string);
	void fail(std::string);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	Search(mail::addressbook *addressBookArg,
	       std::vector<T> &addrListArg,
	       mail::callback &callbackArg);
	~Search();

	std::list< std::string > uidList;

	void go();

};

#endif

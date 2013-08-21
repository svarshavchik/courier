/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "addressbooksearch.H"

using namespace std;

template<class T>
mail::addressbook::Search<T>::Search(mail::addressbook *addressBookArg,
				  vector<T> &addrListArg,
				  mail::callback &callbackArg)
	: addressBook(addressBookArg),
	  addrList(addrListArg),
	  callback(callbackArg)
{
}

template<class T>
mail::addressbook::Search<T>::~Search()
{
}

template<class T>
void mail::addressbook::Search<T>::success(std::string msg)
{
	go();
}

template<class T>
void mail::addressbook::Search<T>::fail(std::string msg)
{
	try {
		callback.fail(msg);
		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

template<class T>
void mail::addressbook::Search<T>::reportProgress(size_t bytesCompleted,
					       size_t bytesEstimatedTotal,

					       size_t messagesCompleted,
					       size_t messagesEstimatedTotal)
{
	callback.reportProgress(bytesCompleted, bytesEstimatedTotal,
				messagesCompleted, messagesEstimatedTotal);
}

template<class T>
void mail::addressbook::Search<T>::go()
{
	list<string>::iterator b=uidList.begin(), e=uidList.end();

	if (b == e) // Finished the list.
	{
		try {
			callback.success("OK");
			delete this;
		} catch (...) {
			delete this;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		return;
	}

	// Just get the next address book entry.

	string uid= *b;

	uidList.erase(b);

	addressBook->getEntry( uid, addrList, *this );
}

template class mail::addressbook::Search<mail::address>;
template class mail::addressbook::Search<mail::emailAddress>;

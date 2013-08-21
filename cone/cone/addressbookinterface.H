/*
** Copyright 2006, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef addressbookinterface_H
#define addressbookinterface_H

#include "addressbook.H"
#include <list>
#include <vector>

//
// Generic address book lookup interface.
//
// This interface defines address book primitives.
//
// Constructor: receives the name of the address book, for descriptive purposes
//
// close: close the address book.  Orderly shutdown before object gets
//        destroyed
// closed: returns true if the address book is closed due to an error
//
// done: invoked after searching is finished, for a while.
//
// add: add address book entries
//
// import: bulk add
//
// del: delete an entry
//
// rename: rename an entry
//
// searchNickname: search the address book
//
// getIndex: return the index of all entries
//
// getEntry: return a specific entry.

class AddressBook::Interface {

public:
	Interface();
	virtual ~Interface();

	virtual void close()=0;
	virtual bool closed()=0;
	virtual void done()=0;
	virtual bool add(mail::addressbook::Entry &newEntry,
			 std::string oldUid)=0;
	virtual bool import(std::list<mail::addressbook::Entry> &newList)=0;
	virtual bool del(std::string uid)=0;
	virtual bool rename(std::string uid,
			    std::string newnickname)=0;

	virtual bool searchNickname(std::string nickname,
				    std::vector<mail::emailAddress>
				    &addrListArg)=0;
	virtual void getIndex(std::list< std::pair<std::string, std::string> >
			      &listArg)=0;

	virtual bool getEntry(std::string uid,
			      std::vector<mail::emailAddress> &addrList)=0;


	class Mail;
	class LDAP;
};

#endif

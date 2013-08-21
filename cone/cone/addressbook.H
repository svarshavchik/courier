/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef addressbook_H
#define addressbook_H

#include "config.h"
#include <list>

#include "libmail/mail.H"
#include "libmail/addressbook.H"

class myServerLoginCallback;

//
// Encapsulate LibMAIL address books

class AddressBook {

	std::string name;
	std::string url;
	std::string folderStr;

	class Interface;

	Interface *myInterface;

	bool open();

	AddressBook(AddressBook &); // NOT IMPLEMENTED
	AddressBook &operator=(const AddressBook &); // NOT IMPLEMENTED

	class disconnect;

public:
	void close(); // Log off the server connection.

	// Close all addressbooks, or just the remote ones.

	static void closeAll();
	static void closeAllRemote();

	void init(std::string nameArg,
		  std::string urlArg,
		  std::string folderStrArg); // Open an address book.

	static AddressBook *defaultAddressBook; // ~/.cone/addressbook

	std::pair<std::string,
		  std::string> currentRecord;
	// For internal use by addressbook.C. pair<nickname, uid>

	bool iwantFocus; // Also for internal use

	static std::string importName, importAddr; // More internal usage

	static std::list<AddressBook *> addressBookList;
	// All address books

	static void init();

	AddressBook();
	~AddressBook();

	std::string getName() const { return name; }
	std::string getURL() const { return url; }
	std::string getFolder() const { return folderStr; }

	bool add(mail::addressbook::Entry &newEntry);
	bool import(std::list<mail::addressbook::Entry> &list);
	bool del(std::string uid);
	bool rename(std::string uid,
		    std::string newnickname);

	bool searchNickname(std::string nickname,
			    std::vector<mail::emailAddress> &addrListArg);

	void getIndex( std::list< std::pair<std::string, std::string> >
		       &listArg); // Get index of address book entries

	bool getEntry(std::vector<mail::emailAddress> &addrList);
	bool getEntry(std::string uid,
		      std::vector<mail::emailAddress> &addrList);

	static bool searchAll(std::vector<mail::emailAddress> &origList,
			      std::vector<mail::emailAddress> &retList);

private:
	static bool searchAll2(std::vector<mail::emailAddress> &origList,
			      std::vector<mail::emailAddress> &retList);
public:

	// Search addresses.  Those found in the address book are replaced
	// with the addresses pulled frmo the address book.

	static void take(std::vector<mail::address> &addrListArg);
	//
	// Open the 'take addresses screen' with these addresses

	static void updateAccount(std::string oldUrl, std::string newUrl);
	//
	// When an account's URL changes, update the URL for any addressbook
	// in this account.

private:
	void updateAccountChk(std::string oldUrl, std::string newUrl);

	static std::vector<mail::address> savedList;

	static void takeScreen(void *);
} ;

#endif

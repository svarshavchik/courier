/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_addressbook_H
#define libmail_addressbook_H

#include "libmail_config.h"

#include "mail.H"
#include "namespace.H"
#include "objectmonitor.H"

#include <string>
#include <list>
#include <vector>

LIBMAIL_START

class address;
class emailAddress;

/////////////////////////////////////////////////////////////////////////////
//
// Turn any folder into an address book.
//
// Create a mail::account object and a mail::folder object, then use this
// object to create an address book-type interface to them.
//
// This is implemented by saving each address book entry as a message in
// the folder.  The message contains of a minimum set of mail headers.
// The Subject: header contains the address book nickname/handle.
// The address book entry is saved in a text/x-libmail-addressbok MIME section.
// There's also a brief text/plain section that informs the human reader to
// stay out of this folder and message.
//
// At this time, text/x-libmail-addressbok MIME section itself consists of a
// single Address: header (folded in the similar fashion as real mail headers),
// containing an RFC 2822-formatted address list, that uses the UTF-8
// character set.

class addressbook : public mail::obj,
		    private mail::callback::folder {

	//
	// Handle folder update callbacks.
	//

	void newMessages();
	void messagesRemoved(std::vector< std::pair<size_t, size_t> > &);
	void messageChanged(size_t n);

	mail::account *server;
	mail::folder *folder;
	//
	// The address book is here.
	//
	// server may NOT be used for any other purposes, by the application.
	//
	// server and folder may NOT be destroyed until this object is
	// destroyed first.

	//
	// The address book index.
	//

	class Index {
	public:
		Index();
		~Index();

		std::string nickname;	// Nickname, in app's charset
		std::string uid;	// Nickname uid.
	};

	std::vector<Index> index;

	class Add;
	class Del;
	class GetIndex;
	class Open;

	template<class T> class GetAddressList;
	template<class T> class Search;

	//
	// Message #messageNumber has the following subject.
	// The subject line stores the nickname, decode it.
	//
	// The subject line uses the UTF-8 character set, and contains the
	// nickname inside brackets.  setIndex() parses it out.
	//

	void setIndex(size_t messageNumber, std::string subject);

public:
	friend class Add;
	friend class Del;
	friend class GetIndex;
	friend class Open;

	friend class Search<mail::address>;
	friend class Search<mail::emailAddress>;

	friend class GetAddressList<mail::address>;
	friend class GetAddressList<mail::emailAddress>;

	class Entry {
	public:
		Entry();
		~Entry();

		std::string nickname;	// Local charset

		std::vector<mail::emailAddress> addresses;
	};

	addressbook(mail::account *serverArg,
		    mail::folder *folderArg);
	~addressbook();

	// Open the address book.  Read the folder's contents, and initialize
	// the address book index.
	void open(mail::callback &callback);

	// Add a new address book entry.
	void addEntry(Entry &newEntry, std::string olduid,
		      mail::callback &callback);

	void importEntries(std::list<Entry> &newEntries,
			   mail::callback &callback);

	// Delete an address book entry.
	void del(std::string uid, mail::callback &callback);

	// Initialize listArg with the contents of the address book.
	// listArg is a pair of nickname/uid.
	void getIndex( std::list< std::pair<std::string, std::string> >
		       &listArg, mail::callback &callback);

	// Read address book entry #uid.  Addresses are added to addrListArg.
	template<class T>
	void getEntry( std::string uid,
		       std::vector<T> &addrListArg,
		       mail::callback &callback);

	// Search the address book for the nickname.  Initialize addrListArg
	// if found.
	template<class T>
	void searchNickname(std::string nickname,
			    std::vector<T> &addrListArg,
			    mail::callback &callback);
};

LIBMAIL_END

#endif


/*
** Copyright 2006, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef addressbookinterfacemail_H
#define addressbookinterfacemail_H

#include "addressbookinterface.H"

//
// An address book interface implementation that uses mail::addressbook
//

class AddressBook::Interface::Mail : public AddressBook::Interface,
				       private mail::callback::disconnect {

	std::string name;

	mail::account *server;
	mail::folder *folder;
	mail::addressbook *addressBook;

	void disconnected(const char *errmsg) override;
	void servererror(const char *errmsg) override;

	bool serverLogin(std::string uid, std::string pwd,
			 myServerLoginCallback *callbackPtr);

public:
	Mail(std::string name);
	virtual ~Mail();

	bool open(std::string url, std::string folderStr);
	void close() override;
	bool closed() override;
	void done() override;
	bool add(mail::addressbook::Entry &newEntry,
		 std::string oldUid) override;
	bool import(std::list<mail::addressbook::Entry> &newList) override;
	bool del(std::string uid) override;
	bool rename(std::string uid,
		    std::string newnickname) override;

	bool searchNickname(std::string nickname,
			    std::vector<mail::emailAddress> &addrListArg) override;
	void getIndex(std::list< std::pair<std::string, std::string> >
		      &listArg) override;

	bool getEntry(std::string uid,
		      std::vector<mail::emailAddress> &addrList) override;


};
#endif

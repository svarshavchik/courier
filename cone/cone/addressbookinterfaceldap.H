/*
** Copyright 2006, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef addressbookinterfaceldap_H
#define addressbookinterfaceldap_H

#include "addressbookinterface.H"

//
// An address book interface implementation that talks to an LDAP server.
//

#include "ldapaddressbook/config.h"

#if HAVE_OPENLDAP
#include "ldapaddressbook/libldapsearch.h"

class AddressBook::Interface::LDAP : public AddressBook::Interface {

	std::string name;

	struct ldapsearch *search;

	static int callback_func(const char *utf8_name,
				 const char *address,
				 void *callback_arg);

	class callback_info;

public:
	LDAP(std::string name);
	virtual ~LDAP();

	bool openUrl(std::string url, std::string suffix);
	void close();
	bool closed();
	void done();
	bool add(mail::addressbook::Entry &newEntry,
		 std::string oldUid);
	bool import(std::list<mail::addressbook::Entry> &newList);
	bool del(std::string uid);
	bool rename(std::string uid,
		    std::string newnickname);

	bool searchNickname(std::string nickname,
			    std::vector<mail::emailAddress> &addrListArg);
	void getIndex(std::list< std::pair<std::string, std::string> >
		      &listArg);

	bool getEntry(std::string uid,
		      std::vector<mail::emailAddress> &addrList);
};
#endif

#endif

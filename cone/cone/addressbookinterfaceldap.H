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

#endif

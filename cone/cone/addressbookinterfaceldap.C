/*
** Copyright 2006, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "libmail/rfcaddr.H"
#include "libmail/rfc2047encode.H"
#include "libmail/misc.H"
#include "libmail/logininfo.H"

#include "addressbookinterfaceldap.H"

#include "curses/cursesstatusbar.H"
#include "gettext.H"
#include <errno.h>
#include <stdlib.h>

extern CursesStatusBar *statusBar;

#define MAXMATCHES 10

#if HAVE_OPENLDAP

AddressBook::Interface::LDAP::LDAP(std::string nameArg)
	: name(nameArg), search(NULL)
{
}

AddressBook::Interface::LDAP::~LDAP()
{
	if (search)
		l_search_free(search);
}

bool AddressBook::Interface::LDAP::openUrl(std::string url, std::string suffix)
{
	if (search)
		return true;

	mail::loginInfo loginInfo;

	if (!mail::loginUrlDecode(url, loginInfo))
		return false;

	search=l_search_alloc(loginInfo.server.c_str(), LDAP_PORT,
			      loginInfo.uid.c_str(),
			      loginInfo.pwd.c_str(), suffix.c_str());

	if (!search)
	{
		statusBar->status(loginInfo.server + ": " + strerror(errno),
				  statusBar->SERVERERROR);
		statusBar->beepError();
		return false;
	}

	return true;
}

void AddressBook::Interface::LDAP::close()
{
	if (search)
		l_search_free(search);
	search=NULL;
}

void AddressBook::Interface::LDAP::done()
{
	close();
}

bool AddressBook::Interface::LDAP::closed()
{
	return search == NULL;
}

bool AddressBook::Interface::LDAP::add(mail::addressbook::Entry &newEntry,
				       std::string oldUid)
{
	return true; // Not implemented
}

bool AddressBook::Interface::LDAP::import(std::list<mail::addressbook::Entry>
					  &newList)
{
	return true; // Not implemented
}

bool AddressBook::Interface::LDAP::del(std::string uid)
{
	return true; // Not implemented
}

bool AddressBook::Interface::LDAP::rename(std::string uid,
					  std::string newnickname)
{
	return true; // Not implemented
}

class AddressBook::Interface::LDAP::callback_info {
public:
	std::vector<mail::emailAddress> *addrList;
	size_t cnt;
};


int AddressBook::Interface::LDAP::callback_func(const char *utf8_name,
						const char *address,
						void *callback_arg)
{
	try {
		callback_info *info=
			(callback_info *)callback_arg;

		mail::address n(mail::rfc2047::encodeAddrName(utf8_name, "UTF-8"),
			address);

		info->addrList->push_back(n);

		if (++info->cnt >= MAXMATCHES)
			return 1;

		return 0;
	} catch (...)
	{
		statusBar->status(_("Unexpected exception occured during address book lookup"),
				  statusBar->SERVERERROR);
		statusBar->beepError();
		return -1;
	}
}

bool AddressBook::Interface::LDAP::searchNickname(std::string nickname,
						  std::vector<
						  mail::emailAddress>
						  &addrListArg)
{
	addrListArg.clear();

	if (!search)
		return true; // Closed, no-op.

	std::string k=Gettext::toutf8(nickname);

	callback_info info;

	info.addrList= &addrListArg;
	info.cnt=0;

	int rc=l_search_do(search, k.c_str(), callback_func, &info);

	if (rc == 1)
	{
		statusBar->clearstatus();

		statusBar->status(Gettext(_("Discarded additional matches on %1%"))
				  << nickname);
		statusBar->beepError();
		rc=0;
	}

	if (rc != 0)
		return false;

	return true;
}

void AddressBook::Interface::LDAP::getIndex(std::list< std::pair<std::string,
					    std::string> > &listArg)
{
	return; // Not implemented
}


bool AddressBook::Interface::LDAP::getEntry(std::string uid,
					    std::vector<mail::emailAddress>
					    &addrList)
{
	return true; // Not implemented
}

#endif

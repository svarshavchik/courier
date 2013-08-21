/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef passwordlist_H
#define passwordlist_H

#include "config.h"

#include <string>
#include <map>
#include <set>

//
// Memorize passwords that works.  When logging in to a URL with a known
// password, use it.
//

class PasswordList {

	std::map<std::string, std::string> list;

	std::string masterPassword;

public:
	PasswordList();
	~PasswordList();

	bool check(std::string url, std::string &pwd);
	// Known password exists?

	bool get(std::string url, std::string &pwd);
	// check(), without prompting for the master password.

	void save(std::string url, std::string pwd);
	// Known password.

	void save();

	void remove(std::string url); // URL does not exist.

	bool hasMasterPassword();
	bool hasMasterPasswordFile();
	void setMasterPassword(std::string);

	void savePasswords(std::set<std::string> &urls);

	void loadPasswords(const char * const *,
			   const char * const *); // See tlspasswordcache.h


	static PasswordList passwordList;

	std::string static masterPasswordFile();

};

#endif

/*
** Copyright 2003-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_misc_H
#define libmail_misc_H

#include "libmail_config.h"
#include "namespace.H"

#include <string>

LIBMAIL_START

class loginInfo;

std::string toutf8(std::string);
std::string fromutf8(std::string);

std::string hostname();
	// Return this machine's name

enum readMode {readHeadersFolded, readContents, readBoth, readHeaders};

void upper(std::string &w);
	//
	// Convert the string to uppercase characters using the ISO-8859-1
	// character set

std::string homedir();
	//
	// The home directory of the userid running this application.

std::string loginUrlEncode(std::string method,
			   std::string server, std::string uid,
			   std::string password);
	// Create a new mail account URL.
	// method - the support method (imap, pop3Account, etc...)
	// server - the name of the mail account's server
	// uid - mail account ID
	// pwd - mail account password

bool loginUrlDecode(std::string url, class loginInfo &info);
	// Decode a mail account URL
	//
	// url - the URL.
	//
	// info - login information


LIBMAIL_END

#endif

/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smtpinfo_h
#define libmail_smtpinfo_h

#include <string>
#include <vector>
#include <map>

#include "namespace.H"

//
// This structure holds options for sending mail.
//

LIBMAIL_START

class smtpInfo {
public:
	std::string sender;
	// SMTP envelope sender

	std::vector<std::string> recipients;
	// SMTP envelope recipients

	std::map<std::string, std::string> options;
	// Options.  Currently defined options:
	//    notls
	//    cram
	//    DSN=never,success,fail,delay
	//    RET=hdrs,full
	//    NOPIPELINING
	//    VERP (Courier extension)
	//    SECURITY (extension)
	//    POST (recipient list is empty, we think this is an nntp server)
};

LIBMAIL_END

#endif

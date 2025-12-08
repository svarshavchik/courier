/*
** Copyright 2004, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapacl_H
#define libmail_smapacl_H

#include "libmail_config.h"
#include	"mail.H"
#include	"imap.H"
#include	"smap.H"

#include	<vector>

LIBMAIL_START

class smapACL : public smapHandler {

	std::string folderName;
	std::string &rights;
	const char *getName() override;

public:
	smapACL(std::string folderNameArg,
		std::string &rightsArg,
		mail::callback &getCallbackArg);
	~smapACL();
	void installed(imap &) override;

	bool processLine(imap &imapAccount,
			 std::vector<const char *> &words) override;
};

class smapGETACL : public smapHandler {

	std::string folderName;
	std::list<std::pair<std::string, std::string> > &rights;
	const char *getName() override;

public:
	smapGETACL(std::string folderNameArg,
		   std::list<std::pair<std::string, std::string> > &rightsArg,
		   mail::callback &getCallbackArg);
	~smapGETACL();
	void installed(imap &) override;

	bool processLine(imap &imapAccount,
			 std::vector<const char *> &words) override;
};

class smapSETACL : public smapHandler {

	std::string folderName;
	std::string identifier;
	std::string rights;
	bool delIdentifier;
	std::string &errorIdentifier;
	std::vector<std::string> &errorRights;

	const char *getName() override;

public:
	smapSETACL(std::string folderName,
		   std::string identifierArg,
		   std::string rightsArg,
		   bool delIdentifierArg,
		   std::string &errorIdentifier,
		   std::vector<std::string> &errorRights,
		   mail::callback &callbackArg);
	~smapSETACL();
	void installed(imap &) override;

	bool processLine(imap &imapAccount,
			 std::vector<const char *> &words) override;
};

LIBMAIL_END

#endif

/*
** Copyright 2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_imapacl_H
#define libmail_imapacl_H

#include "libmail_config.h"
#include "mail.H"
#include "imap.H"

LIBMAIL_START

class imapGetMyRights : public imapCommandHandler {

	std::string folderName;
	std::string &rights;
	mail::callback &callback;

public:
	imapGetMyRights(std::string folderName, std::string &rightsArg,
			mail::callback &callbackArg);
	~imapGetMyRights();

	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *errmsg);

	bool untaggedMessage(imap &imapAccount, std::string msgname);

	bool taggedMessage(imap &imapAccount, std::string msgname,
			   std::string message,
			   bool okfail, std::string errmsg);

	class parseMyRights;
	class listMyRights;
};

class imapGetRights : public imapCommandHandler {

	std::string folderName;
	std::list<std::pair<std::string, std::string> > &rights;

	mail::callback &callback;

public:
	imapGetRights(std::string folderName,
		      std::list<std::pair<std::string,std::string> >&rightsArg,
		      mail::callback &callbackArg);
	~imapGetRights();

	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *errmsg);

	bool untaggedMessage(imap &imapAccount, std::string msgname);

	bool taggedMessage(imap &imapAccount, std::string msgname,
			   std::string message,
			   bool okfail, std::string errmsg);

public:
	class parseGetRights;
	class listGetRights;
};

class imapSetRights : public imapCommandHandler {

	std::string folderName;
	std::string identifier;
	std::string rights;
	bool delIdentifier;
	std::string &errorIdentifier;
	std::vector<std::string> &errorRights;

	mail::callback &callback;

	std::string dummy;

	std::list< std::pair< std::string, std::string> > dummy_rights;

public:
	imapSetRights(std::string folderName,
		      std::string identifierArg,
		      std::string rightsArg,
		      bool delIdentifierArg,
		      std::string &errorIdentifier,
		      std::vector<std::string> &errorRights,
		      mail::callback &callbackArg);
	~imapSetRights();

	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *errmsg);

	bool untaggedMessage(imap &imapAccount, std::string msgname);

	bool taggedMessage(imap &imapAccount, std::string msgname,
			   std::string message,
			   bool okfail, std::string errmsg);

	class parseRightsInfo;
	class parseAclFailed;
};

LIBMAIL_END

#endif

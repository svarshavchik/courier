/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smaplist_H
#define libmail_smaplist_H

#include "libmail_config.h"
#include	"mail.H"
#include	"imap.H"
#include	"smap.H"

#include	<vector>

LIBMAIL_START

class smapLIST : public smapHandler {

protected:
	std::string path;
	mail::callback::folderList &listCallback;

	const char *getName();

protected:

	std::vector<mail::folder *> subfolders;

public:
	smapLIST(std::string pathArg,
		 mail::callback::folderList &listCallbackArg,
		 mail::callback &callbackArg);
	~smapLIST();
	void installed(imap &);

	bool processLine(imap &imapAccount,
			 std::vector<const char *> &words);
	bool ok(std::string);
};

class smapLISToneFolder : public smapLIST {

	std::string nameComponent;

public:
	smapLISToneFolder(std::string pathArg,
			  mail::callback::folderList &listCallbackArg,
			  mail::callback &callbackArg);
	~smapLISToneFolder();

	bool processLine(imap &imapAccount,
			 std::vector<const char *> &words);
	bool ok(std::string);
};

LIBMAIL_END

#endif

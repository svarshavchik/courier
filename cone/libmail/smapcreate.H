/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapcreate_H
#define libmail_smapcreate_H

#include "libmail_config.h"
#include	"mail.H"
#include	"imap.H"
#include	"smap.H"

#include	<vector>

LIBMAIL_START

class smapCREATE : public smapHandler {

	std::string oldPath;
	std::string path;
	bool createDirectory;
	bool renameFolder;
	mail::callback::folderList *listCallback;

	const char *getName();


public:
	smapCREATE(std::string pathArg,
		   std::string name,
		   bool createDirectoryArg,
		   mail::callback::folderList &listCallbackArg,
		   mail::callback &callbackArg);

	smapCREATE(std::string pathArg,
		   bool createDirectoryArg,
		   mail::callback &callbackArg);

	smapCREATE(std::string oldPathArg,
		   std::string pathArg,
		   std::string name,
		   mail::callback::folderList &listCallbackArg,
		   mail::callback &callbackArg);

	~smapCREATE();
	void installed(imap &);

	bool ok(std::string);
};

LIBMAIL_END

#endif

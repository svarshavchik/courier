/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapdelete_H
#define libmail_smapdelete_H

#include "libmail_config.h"
#include	"mail.H"
#include	"imap.H"
#include	"smap.H"

#include	<vector>

LIBMAIL_START

class smapDELETE : public smapHandler {

	std::string path;
	bool deleteDirectory;

	const char *getName();

public:
	smapDELETE(std::string pathArg,
		   bool deleteDirectoryArg,
		   mail::callback &callbackArg);

	~smapDELETE();
	void installed(imap &);
};

LIBMAIL_END

#endif

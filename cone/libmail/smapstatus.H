/*
** Copyright 2003, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapstatus_H
#define libmail_smapstatus_H

#include "libmail_config.h"
#include	"mail.H"
#include	"imap.H"
#include	"smap.H"

#include	<vector>

LIBMAIL_START

class smapSTATUS : public smapHandler {

	std::string path;
	mail::callback::folderInfo &infoCallback;

	const char *getName() override;

public:
	smapSTATUS(std::string pathArg,
		   mail::callback::folderInfo &infoCallbackArg,
		   mail::callback &callbackArg);
	~smapSTATUS();
	void installed(imap &) override;

	bool processLine(imap &imapAccount,
			 std::vector<const char *> &words) override;
};

LIBMAIL_END

#endif

/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_maildiradd_H
#define libmail_maildiradd_H

#include "libmail_config.h"
#include "maildir.H"
#include "addmessage.H"

#include <stdio.h>

LIBMAIL_START

///////////////////////////////////////////////////////////////////////////
//
//  Maildir add message implementation

class maildir::addmessage : public addMessage {

	std::string tmpname;
	std::string newname;

	std::string folderPath;
	int initialized;
	std::string errmsg;

	void initialize();

	FILE *tmpfile;

	mail::callback *callback;

public:
	addmessage(maildir *maildirArg, std::string folderPath,
		   mail::callback &callbackArg);
	~addmessage();

	void saveMessageContents(std::string);
	void go();
	void fail(std::string errmsg);
};

LIBMAIL_END

#endif

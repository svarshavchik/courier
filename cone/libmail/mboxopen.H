/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mboxopen_H
#define libmail_mboxopen_H

#include "libmail_config.h"
#include "mbox.H"

LIBMAIL_START

class file;

/////////////////////////////////////////////////////////////////////////
//
// A couple of tasks that open the mboxAccount folder.


class mbox::OpenTask : public mbox::TimedTask {

	std::string folder;
	mail::callback::folder *openCallback;

	bool scan(file &scanFile,
		  file *saveFile,
		  bool skipDeleted);

protected:
	virtual void opened(const char *okmsg, mail::callback &callback);

public:
	OpenTask(mbox &mboxAccount,
		 std::string folderArg, mail::callback &callbackArg,
		 mail::callback::folder *openCallbackArg
		 // Not NULL - this is mail::account::Open(), and here's the new
		 // folder callback.
		 // NULL - opening the folder for some other reason
		 );

	~OpenTask();

	bool doit();
};

//
// Subclass OpenTask to implement a new mail check.

class mbox::CheckNewMailTask : public mbox::OpenTask {

	void opened(const char *okmsg, mail::callback &callback);

public:
	CheckNewMailTask(mbox &mboxAccount,
		 std::string folderArg, mail::callback &callbackArg,
		 mail::callback::folder *openCallbackArg);
	~CheckNewMailTask();
};

LIBMAIL_END

#endif

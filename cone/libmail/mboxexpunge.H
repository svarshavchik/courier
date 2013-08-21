/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mboxexpunge_H

#include "mbox.H"
#include "mboxlock.H"

#include <set>
#include <string>

LIBMAIL_START

class file;

////////////////////////////////////////////////////////////////////////////
//
// Expunge messages now.

class mbox::ExpungeTask : public mbox::LockTask {

	file *saveFile;
	bool *ok;

	bool purgeDeleted;

	std::set<std::string> deleteList;

public:
	ExpungeTask(mbox &mboxAccount, mail::callback &callback,
		    bool purgeDeletedArg,
		    const std::vector<size_t> *deleteList);
	~ExpungeTask();

	bool locked(mbox::lock &mlock, std::string path);
	bool locked(file &file);
};

LIBMAIL_END

#endif

/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mboxread_H
#define libmail_mboxread_H

#include "libmail_config.h"
#include "mbox.H"
#include "mboxlock.H"

LIBMAIL_START

class file;

///////////////////////////////////////////////////////////////////////////
//
// Read a message, for generic::genericMessageRead()

class mbox::GenericReadTask : public mbox::LockTask {

	mail::callback::message &callback;
	std::string uid;
	size_t messageNumber;
	bool peek;
	mail::readMode readType;

public:
	GenericReadTask(mbox &mboxAccount,
			mail::callback::message &callbackArg,
			std::string uidArg,
			size_t messageNumberArg,
			bool peekArg,
			mail::readMode readTypeArg);
	~GenericReadTask();

	bool locked(file &file);
};

LIBMAIL_END

#endif

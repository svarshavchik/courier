/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mboxgetmessage_H
#define libmail_mboxgetmessage_H

#include "libmail_config.h"
#include "mbox.H"
#include "mboxlock.H"

struct rfc2045;

LIBMAIL_START

class file;

////////////////////////////////////////////////////////////////////////////
//
// Implement generic's genericGetMessage[Fd][Struct] functions

class mbox::GenericGetMessageTask : public mbox::LockTask {

	std::string uid;
	size_t messageNumber;
	bool peek;
	int *fdret;
	struct rfc2045 **structret;

	FILE *tempFp;
	struct rfc2045 *tempStruct;

public:
	GenericGetMessageTask(mbox &mboxAccount,
			      mail::callback &callbackArg,
			      std::string uidArg,
			      size_t messageNumberArg,
			      bool peekArg,
			      int *fdretArg,
			      struct rfc2045 **structretArg);
	~GenericGetMessageTask();

	bool locked(file &file);
};

LIBMAIL_END

#endif

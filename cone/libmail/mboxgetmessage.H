/*
** Copyright 2002, S. Varshavchik.
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
	std::shared_ptr<rfc822::fdstreambuf> *fdret;
	std::shared_ptr<rfc2045::entity> *structret;

	rfc822::fdstreambuf tempFp;

public:
	GenericGetMessageTask(mbox &mboxAccount,
			      mail::callback &callbackArg,
			      std::string uidArg,
			      size_t messageNumberArg,
			      bool peekArg,
			      std::shared_ptr<rfc822::fdstreambuf> *fdretArg,
			      std::shared_ptr<rfc2045::entity> *structretArg);
	~GenericGetMessageTask();

	bool locked(file &file) override;
};

LIBMAIL_END

#endif

/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapfetch_H
#define libmail_smapfetch_H

#include "libmail_config.h"
#include	"mail.H"
#include	"smap.H"

#include "smapmsgrange.H"

LIBMAIL_START

class smapFETCH : public smapHandler {

	const char *getName();

	smapUidSet uidSet;
	smapMsgRange msgRange;

	std::string fetchCmd;
	size_t fetchingMessageNum;

	mail::callback::message &msgcallback;

	size_t expectedCount;
	size_t countProcessed;

	unsigned long estimatedSize;
	unsigned long sizeDone;

public:
	smapFETCH(const std::vector<size_t> &messages,
		  bool peekArg,
		  std::string mime_idArg,
		  mail::readMode getType,
		  const char *decodedArg,
		  mail::callback::message &msgcallbackArg,
		  mail::imap &imapAccount);
	~smapFETCH();

	void installed(imap &);
	bool ok(std::string);

	bool go();

private:
	void beginProcessData(imap &imapAccount,
			      std::vector<const char *> &words,
			      unsigned long estimatedSize);
	void processData(imap &imapAccount,
			 std::string data);
	void endData(imap &imapAccount);

};

LIBMAIL_END

#endif

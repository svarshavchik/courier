/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapfetchattr_H
#define libmail_smapfetchattr_H

#include "libmail_config.h"
#include	"mail.H"
#include	"smap.H"
#include	"envelope.H"
#include	"structure.H"

#include "smapmsgrange.H"

LIBMAIL_START

class smapFETCHATTR : public smapHandler {

	const char *getName();

	smapUidSet uidSet;
	smapMsgRange fetchList;
	mail::account::MessageAttributes attributes;
	mail::callback::message &msgcallback;

	size_t nextPtr;

public:
	smapFETCHATTR(const std::vector<size_t> &messages,
		      mail::account::MessageAttributes attributesArg,
		      mail::callback::message &callbackArg,
		      mail::imap &imapAccount);
	~smapFETCHATTR();

	void installed(imap &);
	bool ok(std::string);

	bool go();

private:
	void fetchedMessageSize(size_t msgNum,
				unsigned long bytes);

	void fetchedInternalDate(size_t msgNum,
				 time_t internalDate);

	void beginProcessData(imap &imapAccount,
			      std::vector<const char *> &words,
			      unsigned long estimatedSize);
	void processData(imap &imapAccount,
			 std::string data);
	void endData(imap &imapAccount);

	std::string fetchingHeader;
	size_t fetchingMessageNum;
	bool doFetchingEnvelope;
	bool seenMimeVersion;

	mail::mimestruct *doFetchingStructure;
	size_t fetchingStructCount;

	mail::envelope fetchingEnvelope;
	mail::mimestruct fetchingStructure;

	void processFetchedHeader(std::string);

	void checkMimeVersion();

	static mail::mimestruct *findMimeId(mail::mimestruct *p,
					    std::string mimeid,
					    size_t recursionLevel);

	static void parseMimeHeader(std::string,
				    std::string &,
				    mail::mimestruct::parameterList &);

};

LIBMAIL_END

#endif

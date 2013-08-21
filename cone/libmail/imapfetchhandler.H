/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_imapfetchhandler_H
#define libmail_imapfetchhandler_H

#include	"config.h"
#include	"imap.H"
#include	"imaphandler.H"

LIBMAIL_START

///////////////////////////////////////////////////////////////////////////
//
//  A helper class for processing IMAP FETCH.

class imapFetchHandler : public imapCommandHandler {

	mail::callback::message &callback;

	std::string name;

	imap *imapAccount;

	std::string failmsg;

public:
	int counter;

	std::queue<std::pair<std::string, std::string> > commands;

	imapFetchHandler(mail::callback::message &callbackArg,
			       std::string nameArg);

	~imapFetchHandler();

	bool untaggedMessage(imap &imapAccount, std::string name);

	bool taggedMessage(imap &imapAccount, std::string name,
			   std::string message,
			   bool okfail, std::string errmsg);

	void installed(imap &imapArg);

	void messageTextCallback(size_t messageNum, std::string text);
	void messageEnvelopeCallback(size_t messageNumber,
				     const envelope &envelope);
	void messageReferencesCallback(size_t messageNumber,
				       const std::vector<std::string>
				       &references);
	void messageStructureCallback(size_t messageNumber,
				      const mimestruct &messageStructure);

	void messageArrivalDateCallback(size_t messageNumber,
					time_t datetime);

	void messageSizeCallback(size_t messageNumber,
				 unsigned long messagesize);

	void timedOut(const char *);

	size_t messageTextEstimatedSize;
	size_t messageTextCompleted;
	size_t messageCntDone;
	size_t messageCntTotal;
private:
	const char *getName();
};

LIBMAIL_END

#endif

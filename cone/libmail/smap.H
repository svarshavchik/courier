/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smap_H
#define libmail_smap_H

#include "libmail_config.h"
#include	"mail.H"
#include	"imap.H"

#include	<vector>

LIBMAIL_START

class smapHandler : public imapHandler {

protected:
	mail::callback *defaultCB;

	void timedOut(const char *errmsg);

	bool doDestroy;

public:
	smapHandler(int timeoutValArg=0);
	virtual ~smapHandler();

	int process(imap &imapAccount, std::string &buffer);

	int singleLineProcess(imap &imapAccount, std::string &buffer);
	int multiLineProcessDotStuffed(imap &, std::string &);
	int multiLineProcessBinary(imap &, std::string &);

	virtual bool processLine(imap &imapAccount,
				 std::vector<const char *> &words);

	virtual void beginProcessData(imap &imapAccount,
				      std::vector<const char *> &words,
				      unsigned long estimatedSize);
	virtual void processData(imap &imapAccount,
				 std::string data);
	virtual void endData(imap &imapAccount);

	virtual bool ok(std::string);
	virtual bool fail(std::string);

	virtual void existsOrExpungeSeen();
	// Some subclasses want to know if an EXISTS or EXPUNGE was received

	virtual void fetchedMessageSize(size_t msgNum,
					unsigned long bytes);

	virtual void fetchedInternalDate(size_t msgNum,
					 time_t internalDate);
	virtual void fetchedIndexInfo();

	virtual void messagesRemoved(std::vector< std::pair<size_t, size_t> >
				     &removedList);
	virtual void messageChanged(size_t msgNum);

	static std::string words2path(std::vector<const char *> &);

	static void path2words(std::string, std::vector<std::string> &);

	static void commaSplit(std::string, std::vector<std::string> &);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal)
	{
		if (defaultCB)
			defaultCB->reportProgress(bytesCompleted,
						  bytesEstimatedTotal,
						  messagesCompleted,
						  messagesEstimatedTotal);
	}

};

LIBMAIL_END

#endif

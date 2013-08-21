/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntpfetch_H
#define libmail_nntpfetch_H

#include "libmail_config.h"

#include "nntp.H"
#include "nntpgroup.H"

#include <string>

LIBMAIL_START

//
// Fetch message contents.
//

class mail::nntp::FetchTaskBase : public mail::nntp::GroupTask {

protected:
	size_t msgNum;
	std::string uid;
	mail::readMode getType;

	unsigned long byteCount;

	void (mail::nntp::FetchTaskBase::*response_func)(const char *);

public:
	FetchTaskBase(mail::callback *callbackArg, nntp &myserverArg,
		      std::string groupNameArg,
		      size_t msgNumArg,
		      std::string uidArg,
		      mail::readMode getTypeArg);
	~FetchTaskBase();

	void selectedGroup(msgnum_t estimatedCount,
			   msgnum_t loArticleCount,
			   msgnum_t hiArticleCount);
	void processGroup(const char *);

private:
	void processStatusResponse(const char *);

	void processFetchResponse(const char *);

	bool foldedNewline;
	void processFetchFoldedResponse(const char *);

	virtual void fetchedText(std::string)=0;
};

class mail::nntp::FetchTask : public mail::nntp::FetchTaskBase {

	callback::message &textCallback;
public:
	FetchTask(callback::message *textCallbackArg, nntp &myserverArg,
		  std::string groupNameArg,
		  size_t msgNumArg,
		  std::string uidArg,
		  mail::readMode getType);

	~FetchTask();
	void fetchedText(std::string);
};


LIBMAIL_END

#endif

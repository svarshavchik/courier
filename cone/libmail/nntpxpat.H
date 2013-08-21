/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntpxpat_H
#define libmail_nntpxpat_H

#include "libmail_config.h"

#include "nntp.H"
#include "nntpgroup.H"
#include "search.H"

#include <string>

LIBMAIL_START

//
// nntp::Task wants a mail::callback, but we have a mail::search::callback
//

class nntp::XpatTaskCallback : public mail::callback {
public:
	searchCallback *realCallback;

	XpatTaskCallback(searchCallback *);
	~XpatTaskCallback();

	void success(std::string message);
	void fail(std::string message);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);
};

class mail::nntp::XpatTask : public mail::nntp::GroupTask {

	XpatTaskCallback *cb;

	void (mail::nntp::XpatTask::*response_func)(const char *);

	std::string hdr;
	std::string srch;
	bool searchNot;

	searchParams::Scope searchScope;
	size_t rangeLo, rangeHi;

public:
	XpatTask(XpatTaskCallback *, nntp &, std::string,
		 std::string, std::string, bool, searchParams::Scope,
		 size_t, size_t);
	~XpatTask();

	void selectedGroup(msgnum_t estimatedCount,
			   msgnum_t loArticleCount,
			   msgnum_t hiArticleCount);
	void processGroup(const char *);


private:
	void processStatusResponse(const char *);

	void processXpatResponse(const char *);

	void done(const char *);

	std::vector<mail::nntp::nntpMessageInfo>::iterator lastIdxMsgNum;
};

LIBMAIL_END

#endif

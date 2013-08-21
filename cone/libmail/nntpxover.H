/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntpxover_H
#define libmail_nntpxover_H

#include "libmail_config.h"
#include <fstream>

#include "nntp.H"
#include "nntpgroup.H"

#include <set>
#include <string>

LIBMAIL_START

//
// XOVER-based implementation of readMessageAttributes() for
// MESSAGESIZE, ENVELOPE, and DATE
//


class mail::nntp::XoverTask : public mail::nntp::GroupTask {

	void (mail::nntp::XoverTask::*response_func)(const char *);

	std::vector< std::pair<size_t, std::string> >  msgUids;
	std::vector< std::pair<size_t, std::string> >::iterator nextUid,
		prevUid;

	msgnum_t firstMsgNum, lastMsgNum;

	MessageAttributes attributes;

	callback::message &xoverCallback;

public:

	XoverTask(callback::message *callbackArg, nntp &myserverArg,
		  std::string groupNameArg,
		  const std::vector<size_t> &messages,
		  MessageAttributes attributes);

	~XoverTask();

	void selectedGroup(msgnum_t estimatedCount,
			   msgnum_t loArticleCount,
			   msgnum_t hiArticleCount);
	void processGroup(const char *);

private:
	void doNextXoverRange();
	void processXoverStatus(const char *);
	void processXover(const char *);
};

LIBMAIL_END

#endif

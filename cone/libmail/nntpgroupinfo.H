/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntpgroupinfo_H
#define libmail_nntpgroupinfo_H

#include "libmail_config.h"

#include "nntp.H"
#include "nntpgroup.H"

#include <string>

LIBMAIL_START

//
// Obtain number of messages in the group by using a GROUP
//

class mail::nntp::GroupInfoTask : public mail::nntp::GroupTask {
	callback::folderInfo &infoCallback;

public:

	GroupInfoTask(callback *callbackArg, nntp &myserverArg,
		      std::string groupNameArg,
		      callback::folderInfo &infoCallbackArg);
	~GroupInfoTask();

	void selectedGroup(msgnum_t estimatedCount,
			   msgnum_t loArticleCount,
			   msgnum_t hiArticleCount);
	void processGroup(const char *);
};

LIBMAIL_END

#endif

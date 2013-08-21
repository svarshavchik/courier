/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntpgroupopen_H
#define libmail_nntpgroupopen_H

#include "libmail_config.h"

#include "nntp.H"
#include "nntpgroup.H"
#include "nntpnewsrc.H"
#include "snapshot.H"

#include <string>

LIBMAIL_START

//
// Officially open a newsgruop
//

class mail::nntp::GroupOpenTask : public mail::nntp::GroupTask {

	callback::folder *folderCallback;
	std::string groupName;
	snapshot *restoreSnapshot;

	void (mail::nntp::GroupOpenTask::*response_func)(const char *);

	newsrc myNewsrc;
	std::vector< std::pair<msgnum_t, msgnum_t> >::iterator msglistI;

	class snapshotRestoreHelper : public mail::snapshot::restore {
		mail::nntp &orignntp;
	public:
		bool abortedFlag;

		std::vector<nntpMessageInfo> index;

		snapshotRestoreHelper(mail::nntp &nntpArg);
		~snapshotRestoreHelper();
		void restoreIndex(size_t msgNum,
				  const mail::messageInfo &);
		void restoreKeywords(size_t msgNum,
				     const std::set<std::string> &);
		void abortRestore();
	};

public:

	GroupOpenTask(callback *callbackArg, nntp &myserverArg,
		      std::string groupNameArg,
		      snapshot *restoreSnapshotArg,
		      callback::folder *folderCallbackArg);
	~GroupOpenTask();

	void selectedGroup(msgnum_t estimatedCount,
			   msgnum_t loArticleCount,
			   msgnum_t hiArticleCount);
	void processGroup(const char *);

private:
	void processLISTGROUP(const char *);
	void processMessageNumber(const char *);
	void processMessageNumber(msgnum_t n, unsigned char flags,
				  mail::keywords::Message &keywords);
	bool opened();
};

LIBMAIL_END

#endif

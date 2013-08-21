/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "nntp.H"
#include "nntpgroupopen.H"
#include "nntpchecknew.H"
#include "nntpnewsrc.H"
#include <sstream>
#include <fstream>
#include <errno.h>
#include <cstring>

using namespace std;

/////////////////////////////////////////////////////////////////////////////
//
// Helper class for restoring application-saved snapshots

mail::nntp::GroupOpenTask::snapshotRestoreHelper
::snapshotRestoreHelper(mail::nntp &nntpArg)
	: orignntp(nntpArg), abortedFlag(false)
{
}

mail::nntp::GroupOpenTask::snapshotRestoreHelper::~snapshotRestoreHelper()
{
}

void mail::nntp::GroupOpenTask::snapshotRestoreHelper
::restoreIndex(size_t msgNum,
	       const mail::messageInfo &mi)
{
	if (msgNum >= index.size())
		return;

	istringstream i(mi.uid);
	msgnum_t n=0;

	i >> n;

	if (i.fail() || n == 0)
		return;

	index[msgNum].msgNum=n;

	unsigned char flags=0;

	if (mi.deleted)
		flags |= IDX_DELETED;

	if (mi.marked)
		flags |= IDX_FLAGGED;

	index[msgNum].msgFlag=flags;
}

void mail::nntp::GroupOpenTask::snapshotRestoreHelper
::restoreKeywords(size_t msgNum,
		  const std::set<std::string> &kwSet)
{
	if (msgNum >= index.size())
		return;

	index[msgNum].keywords.setFlags(orignntp.keywordHashtable, kwSet);
}

void mail::nntp::GroupOpenTask::snapshotRestoreHelper::abortRestore()
{
	abortedFlag=true;
}

/////////////////////////////////////////////////////////////////////////////

mail::nntp::GroupOpenTask::GroupOpenTask(callback *callbackArg,
					 nntp &myserverArg,
					 std::string groupNameArg,
					 snapshot *restoreSnapshotArg,
					 callback::folder *folderCallbackArg)
	: GroupTask(callbackArg, myserverArg, groupNameArg),
	  folderCallback(folderCallbackArg),
	  groupName(groupNameArg),
	  restoreSnapshot(restoreSnapshotArg)
{
}

mail::nntp::GroupOpenTask::~GroupOpenTask()
{
}

void mail::nntp::GroupOpenTask::selectedGroup(msgnum_t estimatedCount,
					      msgnum_t loArticleCount,
					      msgnum_t hiArticleCount)
{
	myserver->openedGroup="";
	myserver->cleartmp();
	myserver->index.clear();
	myserver->index.reserve(estimatedCount);
	myserver->loWatermark=loArticleCount;
	myserver->hiWatermark=hiArticleCount;

	map<std::string, newsrc>::iterator p;

	if (myserver->didCacheNewsrc &&
	    (p=myserver->cachedNewsrc.find(groupName)) !=
	    myserver->cachedNewsrc.end())
	{
		myNewsrc=p->second;
	}
	else
	{
		myNewsrc.newsgroupname=groupName;
		myNewsrc.subscribed=false;

		ifstream i(myserver->newsrcFilename.c_str());

		string line;

		while (i.is_open() && !getline(i, line).fail())
		{
			newsrc parseLine(line);

			if (parseLine.newsgroupname == groupName)
			{
				myNewsrc=parseLine;
				break;
			}
		}
	}

	if (!myNewsrc.subscribed) // Subscribed now, clear deleted list.
	{
		myNewsrc.subscribed=true;
		myNewsrc.msglist.clear();
	}

	if (myNewsrc.msglist.size() > 0 &&
	    myNewsrc.msglist.end()[-1].second > hiArticleCount)
	{
		myNewsrc.msglist.clear(); // Group renumbered on the swerver
	}

	msglistI=myNewsrc.msglist.begin();

	string cmd="LISTGROUP\r\n";

	if (restoreSnapshot)
	{
		size_t cnt;
		string dummy;
		msgnum_t cLo=0, cHi=0;

		restoreSnapshot->getSnapshotInfo(dummy, cnt);

		if (dummy.size() > 0)
		{
			istringstream iss(dummy);

			char dummyChar;

			iss >> cLo >> dummyChar >> cHi;

			if (iss.fail() || cLo == 0 || dummyChar != '-' ||
			    cHi < cLo ||
			    cHi > myserver->hiWatermark
			    // Server probably renumbered
			    )
			{
				dummy="";
			}
		}

		if (dummy.size() > 0)
		{
			snapshotRestoreHelper myHelper(*myserver);

			myHelper.index.resize(cnt);

			restoreSnapshot->restoreSnapshot(myHelper);

			if (!myHelper.abortedFlag)
			{
				// Sanity check

				vector<nntpMessageInfo>::iterator
					b=myHelper.index.begin(),
					e=myHelper.index.end();

				bool first=true;

				while (b != e)
				{
					if (b->msgNum == 0 ||
					    (!first &&
					     b[-1].msgNum >= b->msgNum))
					{
						myHelper.abortedFlag=true;
						break;
					}
					b++;
					first=false;
				}
			}
			if (!myHelper.abortedFlag)
			{
				size_t i;
				size_t n=myHelper.index.size();

				for (i=0; i<n; i++)
				{
					if (myHelper.index[i].msgNum <
					    myserver->loWatermark)
						continue;

					processMessageNumber(myHelper
							     .index[i].msgNum,
							     myHelper
							     .index[i].msgFlag,
							     myHelper
							     .index[i].keywords);
				}

				ostringstream o;

				o << "XHDR Lines " << cHi << "-\r\n";
				cmd=o.str();
			}
		}
	}

	response_func= &mail::nntp::GroupOpenTask::processLISTGROUP;
	myserver->socketWrite(cmd);
}

void mail::nntp::GroupOpenTask::processGroup(const char *msg)
{
	(this->*response_func)(msg);
}

void mail::nntp::GroupOpenTask::processLISTGROUP(const char *msg)
{
	if (msg[0] == '4')
	{
		response_func=
			&mail::nntp::GroupOpenTask::processMessageNumber;

		processMessageNumber(".");
		return;
	}

	if (msg[0] != '2')
	{
		fail(msg);
		return;
	}

	response_func= &mail::nntp::GroupOpenTask::processMessageNumber;
}


bool mail::nntp::GroupOpenTask::opened()
{
	myserver->folderCallback=folderCallback;
	myserver->openedGroup=groupName;
	myserver->createNewsrc(myNewsrc);

	if (!myserver->updateOpenedNewsrc(myNewsrc))
	{
		fail(strerror(errno));
		return false;
	}
	return true;
}

void mail::nntp::GroupOpenTask::processMessageNumber(const char *msg)
{
	if (strcmp(msg, ".") == 0)
	{
		if (!opened())
			return;
		myserver->saveSnapshot();

		ostringstream o;

		o << groupName << ": " << myserver->index.size()
		  << " messages.";

		success(o.str());
		return;
	}

	string s(msg);
	istringstream i(s);

	msgnum_t n;

	i >> n;

	if (!i.bad() && !i.fail())
	{
		mail::keywords::Message kw;

		processMessageNumber(n, 0, kw);
	}
}

void mail::nntp::GroupOpenTask::processMessageNumber(msgnum_t n,
						     unsigned char flags,
						     mail::keywords::Message
						     &kw)
{
	while (msglistI != myNewsrc.msglist.end() &&
	       msglistI->second < n)
		msglistI++;

	if (msglistI != myNewsrc.msglist.end() &&
	    msglistI->first <= n)
		return; // This message is excluded

	if (myserver->index.size() > 0 &&
	    n <= myserver->index.end()[-1].msgNum)
		return; // Bad server mojo

	nntpMessageInfo newInfo;

	newInfo.msgNum=n;
	newInfo.msgFlag=flags;
	newInfo.keywords=kw;
	myserver->index.push_back(newInfo);
}

/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "nntpgroupinfo.H"
#include "nntpnewsrc.H"

using namespace std;

mail::nntp::GroupInfoTask::GroupInfoTask(callback *callbackArg,
					 nntp &myserverArg,
					 std::string groupNameArg,
					 callback::folderInfo &infoCallbackArg)
	: GroupTask(callbackArg, myserverArg, groupNameArg),
	  infoCallback(infoCallbackArg)
{
}

mail::nntp::GroupInfoTask::~GroupInfoTask()
{
}

void mail::nntp::GroupInfoTask::selectedGroup(msgnum_t estimatedCount,
					      msgnum_t loArticleCount,
					      msgnum_t hiArticleCount)
{
	if (groupName == myserver->openedGroup)
	{
		infoCallback.messageCount=myserver->index.size();
		success("Ok");
		return;
	}

	myserver->cacheNewsrc();

	// Subtract articles marked as read, from the estimated count

	map<string, newsrc>::iterator p
		=myserver->cachedNewsrc.find(myserver->serverGroup);

	if (p != myserver->cachedNewsrc.end())
	{
		estimatedCount=hiArticleCount - loArticleCount;

		vector< pair<msgnum_t, msgnum_t> >::iterator
			c=p->second.msglist.begin(),
			e=p->second.msglist.end();

		while (c != e)
		{
			size_t a=c->first;
			size_t b=c->second+1;

			if (b > loArticleCount && a < hiArticleCount)
			{
				if (a < loArticleCount)
					a=loArticleCount;

				if (b > hiArticleCount)
					b=hiArticleCount;

				b -= a;

				if (b < estimatedCount)
					estimatedCount -= b;
				else
					estimatedCount=0;
			}
			c++;
		}
	}

	infoCallback.messageCount=estimatedCount;

	success("Ok");
}

void mail::nntp::GroupInfoTask::processGroup(const char *msg)
{
}

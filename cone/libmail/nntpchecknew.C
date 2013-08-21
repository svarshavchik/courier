/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "nntpchecknew.H"
#include "nntpnewsrc.H"
#include <sstream>
#include <algorithm>

using namespace std;

mail::nntp::CheckNewTask::CheckNewTask(callback *callbackArg,
				       nntp &myserverArg,
				       std::string groupNameArg)
	: LoggedInTask(callbackArg, myserverArg),
	  groupName(groupNameArg)
{
}

mail::nntp::CheckNewTask::~CheckNewTask()
{
}

void mail::nntp::CheckNewTask::loggedIn()
{
	myserver->serverGroup="";
	myserver->socketWrite("GROUP " + groupName + "\r\n");
	response_func= &mail::nntp::CheckNewTask::processGroupStatus;
}

void mail::nntp::CheckNewTask::processLine(const char *message)
{
	(this->*response_func)(message);
}

void mail::nntp::CheckNewTask::processGroupStatus(const char *msg)
{
	string buf=msg;
	istringstream i(buf);

	int status;
	msgnum_t est, lo, hi;

	i >> status >> est >> lo >> hi;

	if (i.bad() || i.fail() || (status / 100) != 2)
	{
		fail(msg);
		return;
	}

#if 0
	// DEBUG

	if (myserver->idxMsgNums.size() > 0)
		lo=myserver->idxMsgNums[0]+1;

	// DEBUG
#endif

	myserver->serverGroup=groupName;

	loWatermark= lo;
	hiWatermark= ++hi;

	if (lo == hi || myserver->hiWatermark >= hiWatermark)
	{
		ptr<mail::nntp> s=myserver;

		checkPurged();

		if (!s.isDestroyed())
		{
			s->saveSnapshot();
			success("Ok");
		}

		return;
	}

	ostringstream o;

	firstNewMsg=myserver->hiWatermark;

	if (firstNewMsg >= lo + (hi-lo)/2)
	{
		// Cheaper to use XHDR

		o << "XHDR Lines " << firstNewMsg << "-\r\n";
	}
	else
	{
		o << "LISTGROUP\r\n";
	}

	response_func=&mail::nntp::CheckNewTask::processXhdrStatus;

	myserver->socketWrite(o.str());
}

void mail::nntp::CheckNewTask::processXhdrStatus(const char *msg)
{
	if (msg[0] == '4')
	{
		response_func=
			&mail::nntp::CheckNewTask::processXhdrList;
	
		processXhdrList(".");
		return;
	}

	if (msg[0] != '2')
	{
		fail(msg);
		return;
	}

	response_func= &mail::nntp::CheckNewTask::processXhdrList;
}

bool mail::nntp::equalMsgNums(nntpMessageInfo a,
			      nntpMessageInfo b)
{
	return a.msgNum == b.msgNum;
}

void mail::nntp::CheckNewTask::processXhdrList(const char *msg)
{
	if (strcmp(msg, ".") == 0)
	{
		sort(newMsgList.begin(), newMsgList.end());

		vector<msgnum_t>::iterator p=
			unique(newMsgList.begin(),
			       newMsgList.end());

		ptr<mail::nntp> s=myserver;

		checkPurged();

		if (s.isDestroyed())
			return;

		myserver->hiWatermark=hiWatermark;
		newMsgList.erase(p, newMsgList.end());


		vector<msgnum_t>::iterator b=newMsgList.begin(),
			e=newMsgList.end();

		while (b != e)
		{
			mail::nntp::nntpMessageInfo mi;

			mi.msgNum= *b;
			++b;
			myserver->index.push_back(mi);
		}

		if (newMsgList.size() > 0 && myserver->folderCallback)
			myserver->folderCallback->newMessages();

		if (!s.isDestroyed())
		{
			s->saveSnapshot();
			success("Ok");
		}
		return;
	}

	string s(msg);
	istringstream i(s);

	msgnum_t n;

	i >> n;

	if (!i.bad() && !i.fail() && n >= firstNewMsg)
	{
		newMsgList.push_back(n);
	}
}

void mail::nntp::CheckNewTask::checkPurged()
{
	size_t n=myserver->index.size();
	size_t i;

	for (i=0; i<n; i++)
		if (myserver->index[i].msgNum >= loWatermark)
			break;

	myserver->loWatermark=loWatermark;
	if (i > 0 && myserver->folderCallback)
	{
		myserver->index.erase(myserver->index.begin(),
				      myserver->index.begin()+i);

		vector< pair<size_t, size_t> > del;

		del.push_back(make_pair(0, i-1));

		newsrc myNewsrc;

		myNewsrc.newsgroupname=groupName;

		myserver->createNewsrc(myNewsrc);
		myserver->updateOpenedNewsrc(myNewsrc);

		myserver->folderCallback->messagesRemoved(del);
	}
}

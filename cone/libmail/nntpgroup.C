/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "nntpgroup.H"
#include <sstream>

using namespace std;

//
// Make sure a particular group is selected, before proceeding
//

mail::nntp::GroupTask::GroupTask(callback *callbackArg, nntp &myserverArg,
				 std::string groupNameArg)
	: LoggedInTask(callbackArg, myserverArg),
	  groupName(groupNameArg)
{
}

mail::nntp::GroupTask::~GroupTask()
{
}

void mail::nntp::GroupTask::loggedIn()
{
	if (myserver->serverGroup == groupName &&
	    myserver->openedGroup == groupName)
		// Must check openedGroup
	{
		selectedCurrentGroup();
		return;
	}

	response_func= &mail::nntp::GroupTask::processGroupStatus;
	myserver->serverGroup="";
	myserver->socketWrite("GROUP " + groupName + "\r\n");
}

void mail::nntp::GroupTask::processLine(const char *message)
{
	(this->*response_func)(message);
}

void mail::nntp::GroupTask::selectedCurrentGroup()
{
	response_func= &mail::nntp::GroupTask::processOtherStatus;
	if (myserver->index.size() == 0)
		selectedGroup(0, 0, 0);
	else
		selectedGroup(myserver->index[0].msgNum,
			      myserver->index.end()[-1].msgNum,
			      myserver->index.size());
}

void mail::nntp::GroupTask::processGroupStatus(const char *msg)
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

	++hi;

	if (est > hi-lo)
		est=hi-lo; // Fix an impossibility


	response_func= &mail::nntp::GroupTask::processOtherStatus;
	myserver->serverGroup=groupName;

	if (myserver->openedGroup == groupName)
		selectedCurrentGroup();
	else
		selectedGroup(est, lo, hi);
}

void mail::nntp::GroupTask::processOtherStatus(const char *msg)
{
	processGroup(msg);
}

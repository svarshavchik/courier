/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "nntplistactive.H"
#include "nntpfolder.H"
#include "nntpnewsrc.H"
#include <sstream>
#include <algorithm>
#include <errno.h>
#include <cstring>

using namespace std;

mail::nntp::ListActiveTask::ListActiveTask(callback *callbackArg,
					   nntp &myserverArg)
	: LoggedInTask(callbackArg, myserverArg), count(0),
	  folderListCallback(NULL), cmd(LIST)
{
}

mail::nntp::ListActiveTask::ListActiveTask(callback *callbackArg,
					   nntp &myserverArg,
					   string dateArg,
					   callback::folderList
					   *folderListCallbackArg)
	: LoggedInTask(callbackArg, myserverArg), serverDate(dateArg),
	  count(0), folderListCallback(folderListCallbackArg),
	  cmd(NEWGROUPS)
{
}

mail::nntp::ListActiveTask::~ListActiveTask()
{
}

void mail::nntp::ListActiveTask::loggedIn()
{
	response_func= &mail::nntp::ListActiveTask::processDateStatus;
	myserver->socketWrite("DATE\r\n");
}

void mail::nntp::ListActiveTask::processLine(const char *message)
{
	(this->*response_func)(message);
}

void mail::nntp::ListActiveTask::processDateStatus(const char *msg)
{
	string prevDate=serverDate;

	if (msg[0] == '1')
	{
		while (*msg && !unicode_isspace((unsigned char)*msg))
			msg++;

		while (*msg && unicode_isspace((unsigned char)*msg))
			msg++;

		serverDate=msg;
	}

	response_func= &mail::nntp::ListActiveTask::processListStatus;

	switch (cmd) {
	case NEWGROUPS:
		myserver->socketWrite("NEWGROUPS " +
				      prevDate.substr(0,8) + " " +
				      prevDate.substr(8) + " GMT\r\n");
		break;
	default:
		myserver->socketWrite("LIST\r\n");
	}
}

void mail::nntp::ListActiveTask::processListStatus(const char *msg)
{
	if (msg[0] != '2')
	{
		fail(msg);
		return;
	}
	response_func= &mail::nntp::ListActiveTask::processSubscription;
}

void mail::nntp::ListActiveTask::processSubscription(const char *msg)
{
	if (strcmp(msg, "."))
	{
		const char *p=msg;

		while (*p && !unicode_isspace((unsigned char)*p))
			p++;

		string newsgName=string(msg, p);
		msgnum_t lo, hi;

		string msgRange=p;
		istringstream i(msgRange);

		i >> lo >> hi;

		if (!i.fail())
		{
			newsgroupList.insert(newsgName);

			if ((++count % 100) == 0 && callbackPtr)
			{
				callbackPtr->reportProgress(0, 0, count, 0);
			}
		}
		return;
	}

	myserver->updateCachedNewsrc();
	myserver->discardCachedNewsrc();

	string newNewsrcFilename=myserver->newsrcFilename + ".tmp";

	size_t newGroupCount=0;
	size_t delGroupCount=0;

	{
		ifstream i(myserver->newsrcFilename.c_str());

		ofstream o(newNewsrcFilename.c_str());

		if (o.is_open())
		{
			if (serverDate.size() > 0)
				o << "#DATE: " << serverDate << endl;

			string line;

			// Copy old newsrc to new newsrc
			// Removed groups are skipped.  Found groups are removed
			// from newsgroupList, ending up with new newsgroups

			while (i.is_open() && !getline(i, line).fail())
			{
				newsrc parseLine(line);

				if (parseLine.newsgroupname.size() == 0)
					continue;

				set<string>::iterator n=newsgroupList
					.find(parseLine.newsgroupname);

				if (n == newsgroupList.end())
				{
					if (cmd != NEWGROUPS)
					{
						++delGroupCount;
						continue;
					}
				}
				else
					newsgroupList.erase(n);

				o << (string)parseLine << endl;
			}

			vector<string> sortedNewsgroupList;

			sortedNewsgroupList.insert(sortedNewsgroupList.end(),
						   newsgroupList.begin(),
						   newsgroupList.end());
			newsgroupList.clear();

			sort(sortedNewsgroupList.begin(),
			     sortedNewsgroupList.end());

			vector<string>::iterator b=sortedNewsgroupList.begin(),
				e=sortedNewsgroupList.end();

			while (b != e)
			{
				o << *b << "!" << endl;
				++newGroupCount;
				b++;
			}

			i.close();
			o << flush;
			o.close();

			if (cmd == NEWGROUPS)
			{
				myserver->newgroups=sortedNewsgroupList;
				myserver->hasNewgroups=true;
			}
		}

		if (o.fail() || o.bad() ||
		    rename(newNewsrcFilename.c_str(),
			   myserver->newsrcFilename.c_str()) < 0)
		{
			string msg=strerror(errno);
			fail(msg);
			return;
		}
	}

	if (cmd == NEWGROUPS && callbackPtr && myserver->hasNewgroups)
	{
		callback *c=callbackPtr;

		callbackPtr=NULL;

		mail::nntp::folder fakeFolder(myserver,
					      FOLDER_CHECKNEW,
					      false, true);

		fakeFolder.readSubFolders(*folderListCallback, *c);
	}

	ostringstream o;

	o << count << " groups, " << newGroupCount << " new, "
	  << delGroupCount << " removed.";

	success(o.str());
}

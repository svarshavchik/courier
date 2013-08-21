/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "nntpxover.H"
#include "generic.H"
#include "envelope.H"

#include <algorithm>
#include <sstream>

using namespace std;

mail::nntp::XoverTask::XoverTask(callback::message *callbackArg,
				 nntp &myserverArg,
				 string groupNameArg,
				 const vector<size_t> &messages,
				 MessageAttributes attributesArg)
	: GroupTask(callbackArg, myserverArg, groupNameArg),
	  attributes(attributesArg),
	  xoverCallback( *callbackArg )
{
	vector<size_t> cpy=messages;

	sort(cpy.begin(), cpy.end());

	vector<size_t>::iterator b=cpy.begin(), e=cpy.end();

	size_t n=myserverArg.getFolderIndexSize();

	while (b != e)
	{
		vector<size_t>::iterator c=b;

		++c;

		if (c == e || *c != *b)
		{
			if (*b >= n)
				break;

			ostringstream o;

			o << myserverArg.index[*b].msgNum;

			msgUids.push_back(make_pair(*b, o.str()));
		}
		b=c;
	}
}

mail::nntp::XoverTask::~XoverTask()
{
}

void mail::nntp::XoverTask::selectedGroup(msgnum_t estimatedCount,
					  msgnum_t loArticleCount,
					  msgnum_t hiArticleCount)
{
	nextUid=msgUids.begin();
	doNextXoverRange();
}

void mail::nntp::XoverTask::doNextXoverRange()
{
	// Issue an XOVER for the next range of messages.

	while (nextUid != msgUids.end())
	{
		// Skip msgs that no longer exist

		if (!myserver->fixGenericMessageNumber( nextUid->second,
							nextUid->first))
		{
			nextUid++;
			continue;
		}

		// Grab a consecutive range of message #s

		prevUid=nextUid;

		do
		{
			++nextUid;
		} while (nextUid != msgUids.end() &&
			 myserver->fixGenericMessageNumber( nextUid->second,
							    nextUid->first) &&
			 
			 myserver->index[nextUid[-1].first].msgNum + 1 ==
			 myserver->index[nextUid->first].msgNum);

		response_func=&mail::nntp::XoverTask::processXoverStatus;

		{
			istringstream i(prevUid->second);

			i >> firstMsgNum;

			if (i.bad() || i.fail())
				continue;
		}

		{
			istringstream i(nextUid[-1].second);

			i >> lastMsgNum;

			if (i.bad() || i.fail())
				continue;
		}

		ostringstream o;

		o << "XOVER " << prevUid->second << "-"
		  << nextUid[-1].second << "\r\n";

		myserver->socketWrite(o.str());
		return;
	}

	// Ok, we've done our part.  Let the generics do theirs.

	vector<size_t> msgNums;

	msgNums.reserve(msgUids.size());

	vector< pair<size_t, string> >::iterator b=msgUids.begin(),
		e=msgUids.end();

	while (b != e)
	{
		if (myserver->fixGenericMessageNumber(b->second, b->first))
			msgNums.push_back(b->first);
		b++;
	}

	attributes &= ~(MESSAGESIZE | ENVELOPE | ARRIVALDATE);

	if (attributes && callbackPtr)
	{
		myserver->genericAttributes(myserver, myserver, msgNums,
					    attributes, xoverCallback);
		callbackPtr=NULL;
	}

	success("Ok");
}

void mail::nntp::XoverTask::processGroup(const char *msg)
{
	(this->*response_func)(msg);
}

void mail::nntp::XoverTask::processXoverStatus(const char *msg)
{
	if (*msg != '2')
	{
		fail(msg);
		return;
	}
	response_func=&mail::nntp::XoverTask::processXover;
}

void mail::nntp::XoverTask::processXover(const char *msg)
{
	if (strcmp(msg, ".") == 0)
	{
		doNextXoverRange();
		return;
	}

	string xover=msg;

	string fields[8];

	string::iterator b=xover.begin(), e=xover.end();

	size_t i;

	for (i=0; i<8; i++)
	{
		string::iterator c=b;

		while (c != e)
		{
			if (*c == '\t')
				break;
			c++;
		}

		fields[i]=string(b,c);

		if (c != e)
			c++;
		b=c;
	}

	msgnum_t n=0;
	msgnum_t byte_count=0;
	msgnum_t line_count=0;

	{
		istringstream i(fields[0]);

		i >> n;

		if (i.bad() || i.fail())
			return;
	}

	{
		istringstream i(fields[6]);

		i >> byte_count;

		if (i.bad() || i.fail())
			byte_count=0;
	}

	{
		istringstream i(fields[7]);

		i >> line_count;

		if (i.bad() || i.fail())
			line_count=0;
	}

	if (n < firstMsgNum || n > lastMsgNum || !callbackPtr)
		return;

	mail::envelope env;

	mail::generic::genericBuildEnvelope("Subject", fields[1], env);
	mail::generic::genericBuildEnvelope("From", fields[2], env);
	mail::generic::genericBuildEnvelope("Date", fields[3], env);
	mail::generic::genericBuildEnvelope("Message-ID", fields[4], env);
	mail::generic::genericBuildEnvelope("References", fields[5], env);
	// Not used, for now...

	size_t msgNum= n - firstMsgNum + prevUid->first;

	if (attributes & MESSAGESIZE)
		xoverCallback.messageSizeCallback(msgNum, byte_count);

	if (attributes & ARRIVALDATE)
		xoverCallback.messageArrivalDateCallback(msgNum,
							 env.date);

	if (attributes & ENVELOPE)
		xoverCallback.messageEnvelopeCallback(msgNum, env);
}

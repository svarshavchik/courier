/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "nntpfetch.H"
#include <sstream>

using namespace std;

mail::nntp::FetchTaskBase::FetchTaskBase(mail::callback *callbackArg,
					 nntp &myserverArg,
					 std::string groupNameArg,
					 size_t msgNumArg,
					 std::string uidArg,
					 mail::readMode getTypeArg)
	: GroupTask(callbackArg, myserverArg, groupNameArg),
	  msgNum(msgNumArg),
	  uid(uidArg),
	  getType(getTypeArg)
{
}

mail::nntp::FetchTaskBase::~FetchTaskBase()
{
}

void mail::nntp::FetchTaskBase::selectedGroup(msgnum_t estimatedCount,
					  msgnum_t loArticleCount,
					  msgnum_t hiArticleCount)
{
	response_func= &mail::nntp::FetchTaskBase::processStatusResponse;

	if (!myserver->fixGenericMessageNumber(uid, msgNum))
	{
		fail("Invalid message number.");
		return;
	}

	byteCount=0;
	ostringstream o;

	switch (getType) {
	case mail::readContents:
		o << "BODY ";
		break;
	case mail::readBoth:
		o << "ARTICLE ";
		break;
	default:
		o << "HEAD ";
	}

	o << myserver->index[msgNum].msgNum << "\r\n";

	myserver->socketWrite(o.str());
}

void mail::nntp::FetchTaskBase::processGroup(const char *cmd)
{
	(this->*response_func)(cmd);
}

void mail::nntp::FetchTaskBase::processStatusResponse(const char *msg)
{
	if (msg[0] != '2')
	{
		ostringstream o;

		o << "The following error occured while reading this article:\n"
			"\n"
			"    " << msg << "\n\n";

		fetchedText(o.str());
		success("Ok");
		return;
	}

	response_func= getType == mail::readHeadersFolded
		? &mail::nntp::FetchTaskBase::processFetchFoldedResponse:
		&mail::nntp::FetchTaskBase::processFetchResponse;
	foldedNewline=false;
}

void mail::nntp::FetchTaskBase::processFetchResponse(const char *cmd)
{
	if (strcmp(cmd, "."))
	{
		if (*cmd == '.')
			++cmd;

		string s(cmd);

		s += "\n";

		byteCount += s.size();
		if (callbackPtr)
			callbackPtr->reportProgress(byteCount, 0, 0, 1);
		fetchedText(s);
		return;
	}

	if (callbackPtr)
		callbackPtr->reportProgress(byteCount, byteCount, 0, 1);
	success("Ok\n");
}

void mail::nntp::FetchTaskBase::processFetchFoldedResponse(const char *cmd)
{
	if (strcmp(cmd, "."))
	{
		if (*cmd == '.')
			++cmd;

		if (*cmd && unicode_isspace((unsigned char)*cmd) &&
		    foldedNewline)
		{
			while (*cmd && unicode_isspace((unsigned char)*cmd))
				cmd++;

			string s(" ");
			s += cmd;

			byteCount += s.size();

			if (callbackPtr)
				callbackPtr->
					reportProgress(byteCount, 0, 0, 1);
			fetchedText(s);
		}
		else
		{
			string s;

			if (foldedNewline)
				s += "\n";
			foldedNewline=true;
			s += cmd;

			byteCount += s.size();
			if (callbackPtr)
				callbackPtr->
					reportProgress(byteCount, 0, 0, 1);
			fetchedText(s);
		}
		return;
	}
	++byteCount;

	if (callbackPtr)
		callbackPtr->reportProgress(byteCount, byteCount, 0, 1);

	fetchedText("\n");
	success("Ok");
}

//
// TaskBase is recyled by CacheTask, which also fetches a message's contents,
// but does not have a callback::message, because it goes into a temporary
// file.  That's why we separate out callback::message-specific processing
// into a subclass.
//

mail::nntp::FetchTask::FetchTask(callback::message *textCallbackArg,
				 nntp &myserverArg,
				 std::string groupNameArg,
				 size_t msgNumArg,
				 std::string uidArg,
				 mail::readMode getType)
	: FetchTaskBase(textCallbackArg, myserverArg,
			groupNameArg,
			msgNumArg,
			uidArg,
			getType),
	  textCallback(*textCallbackArg)
{
}

mail::nntp::FetchTask::~FetchTask()
{
}

void mail::nntp::FetchTask::fetchedText(std::string s)
{
	textCallback.messageTextCallback(msgNum, s);
}

/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "smapfetch.H"

using namespace std;

const char *mail::smapFETCH::getName()
{
	return "FETCHMESSAGE";
}

mail::smapFETCH::smapFETCH(const std::vector<size_t> &messages,
			   bool peek,
			   std::string mime_id,
			   mail::readMode getType,
			   const char *decoded,
			   mail::callback::message &msgcallbackArg,
			   mail::imap &imapAccount)
	: uidSet(imapAccount, messages),
	  msgcallback(msgcallbackArg),
	  expectedCount(messages.size()),
	  countProcessed(0)
{
	defaultCB= &msgcallbackArg;

	string element;

	switch (getType) {
	case mail::readHeadersFolded:
		element="HEADERS";
		break;
	case mail::readContents:
		element="BODY";
		break;
	case mail::readBoth:
		element="ALL";
		break;
	case mail::readHeaders:
		element="RAWHEADERS";
		break;
	}

	fetchingMessageNum=0;

	if (decoded && *decoded)
		element += decoded;

	if (mime_id.size() > 0)
		mime_id="[" + mime_id + "]";

	fetchCmd= mail::imap::quoteSMAP((peek ? "CONTENTS.PEEK=":"CONTENTS=")
					+ element + mime_id);
}

mail::smapFETCH::~smapFETCH()
{
}

void mail::smapFETCH::installed(imap &imapAccount)
{
	msgRange.init(imapAccount, uidSet);
	uidSet.clear();
	if (go())
		return;

	ok("OK");
	imapAccount.uninstallHandler(this);
}

bool mail::smapFETCH::ok(std::string msg)
{
	if (go())
	{
		doDestroy=false;
		return true;
	}

	return smapHandler::ok(msg);
}

bool mail::smapFETCH::go()
{
	ostringstream msgList;

	msgList << "FETCH";

	if (!myimap->currentFolder ||
	    myimap->currentFolder->closeInProgress ||
	    !(msgRange >> msgList))
		return false;

	msgList << " " << fetchCmd << "\n";
	myimap->imapcmd("", msgList.str());
	return true;
}

void mail::smapFETCH::beginProcessData(imap &imapAccount,
				       std::vector<const char *> &words,
				       unsigned long estimatedSizeArg)
{
	estimatedSize=estimatedSizeArg;
	sizeDone=0;

	if (words.size() >= 2 && strcasecmp(words[0], "FETCH") == 0)
	{
		string n=words[1];
		istringstream i(n);

		fetchingMessageNum=0;

		i >> fetchingMessageNum;
	}
}

void mail::smapFETCH::processData(imap &imapAccount,
				  std::string data)
{
	if (fetchingMessageNum > 0)
	{
		if ((sizeDone += data.size()) > estimatedSize)
			estimatedSize=sizeDone;

		msgcallback.reportProgress(sizeDone, estimatedSize,
					   countProcessed, expectedCount);
		msgcallback.messageTextCallback(fetchingMessageNum-1,
						data);
	}
}

void mail::smapFETCH::endData(imap &imapAccount)
{
	if (++countProcessed > expectedCount)
		expectedCount=countProcessed;

	msgcallback.reportProgress(sizeDone, sizeDone, countProcessed,
				   expectedCount);
}

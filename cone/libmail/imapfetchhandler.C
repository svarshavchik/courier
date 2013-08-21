/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "imapfetchhandler.H"

using namespace std;

mail::imapFetchHandler
::imapFetchHandler(mail::callback::message &callbackArg,
			 string nameArg)
	: callback(callbackArg),
	  name(nameArg),
	  imapAccount(NULL),
	  counter(0),
	  messageTextEstimatedSize(0),
	  messageTextCompleted(0),
	  messageCntDone(0),
	  messageCntTotal(0)
{
}

mail::imapFetchHandler::~imapFetchHandler()
{
	if (imapAccount)
		imapAccount->currentFetch=NULL;
}

bool mail::imapFetchHandler::untaggedMessage(mail::imap &imapAccount, string name)
{
	return false;
}

bool mail::imapFetchHandler::taggedMessage(mail::imap &imapAccount, string name,
					   string message,
					   bool okfail, string errmsg)
{
	if (name == getName())
	{
		if (!okfail)
		{
			if (failmsg.size() == 0)
				failmsg=errmsg;
		}

		// Long message sets are broken up.  We need to count the
		// tagged replies received, and only report smashing success
		// (or flaming failure) when the last one comes in.

		--counter;

		if (counter == 0)
		{
			if (failmsg.size() > 0)
				callback.fail(failmsg);
			else
				callback.success(errmsg);
			imapAccount.uninstallHandler(this);
		}
		return true;
	}
	return false;
}

void mail::imapFetchHandler::installed(mail::imap &imapArg)
{
	imapAccount= &imapArg;

	if (imapAccount->currentFetch)
		imapAccount->currentFetch->imapAccount=NULL; // We're the top dog now.

	imapAccount->currentFetch=this;

	// Just spit out all the commands, all together.

	while (!commands.empty())
	{
		pair<string, string> cmd=commands.front();

		commands.pop();
		imapAccount->imapcmd(cmd.first, cmd.second);
	}
}

// As stuff comes back from the server, make sure to bump the timeout
// to let mail::imap know we're still alive.

void mail::imapFetchHandler::messageTextCallback(size_t messageNum,
						 string text)
{
	setTimeout();
	callback.reportProgress(messageTextCompleted,
				messageTextEstimatedSize,
				messageCntDone, messageCntTotal);
	callback.messageTextCallback(messageNum, text);
}

void mail::imapFetchHandler::messageEnvelopeCallback(size_t messageNumber,
						     const mail::envelope
						     &envelope)
{
	setTimeout();
	callback.messageEnvelopeCallback(messageNumber, envelope);
}

void mail::imapFetchHandler::messageReferencesCallback(size_t messageNumber,
						       const vector<string>
						       &references)
{
	setTimeout();
	callback.messageReferencesCallback(messageNumber, references);
}


void mail::imapFetchHandler::messageStructureCallback(size_t messageNumber,
						      const mail::mimestruct &
						      messageStructure)
{
	setTimeout();
	callback.messageStructureCallback(messageNumber, messageStructure);
}


void mail::imapFetchHandler::messageArrivalDateCallback(size_t messageNumber,
							time_t datetime)
{
	setTimeout();
	callback.messageArrivalDateCallback(messageNumber, datetime);
}

void mail::imapFetchHandler::messageSizeCallback(size_t messageNumber,
						 unsigned long messagesize)
{
	setTimeout();
	callback.messageSizeCallback(messageNumber, messagesize);
}

void mail::imapFetchHandler::timedOut(const char *errmsg)
{
	callbackTimedOut(callback, errmsg);
}

const char *mail::imapFetchHandler::getName()
{
	return name.c_str();
}

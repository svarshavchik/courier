/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"

#include "mail.H"
#include "copymessage.H"
#include "addmessage.H"

#include <vector>
#include <queue>
#include <sstream>
#include <iostream>

using namespace std;

void mail::copyMessage::copy(mail::account *copyFrom, size_t messageNum,
			     mail::folder *copyTo,
			     mail::callback &callback,
			     size_t completedCount)
{
	mail::copyMessage *cm=NULL;

	try {
		cm=new mail::copyMessage(copyFrom, messageNum,
					 copyTo, callback);

		if (!cm)
		{
			callback.fail("Out of memory.");
			return;
		}

		cm->completedCount=completedCount;
		cm->go();

	} catch (...) {
		if (cm)
			delete cm;

		callback.fail("An exception occurred while copying messages.  The copy is aborted.");
		return;
	}
}

mail::copyMessage::copyMessage(mail::account *fromArg,
				     size_t messageNumArg,
				     mail::folder *toArg,
				     mail::callback &callbackArg)
	: callback(callbackArg),
	  from(fromArg),
	  to(toArg),
	  addMessagePtr(NULL),
	  messageNum(messageNumArg),
	  arrivalDate(0),
	  fetchCompletedCnt(0),
	  fetchEstimatedCnt(0),
	  addCompleted(0),
	  addEstimated(0),
	  completedCount(0)
{
	fetch_callback.me=this;
	add_callback.me=this;

	messageInfo= fromArg->getFolderIndexInfo(messageNumArg);
}

mail::copyMessage::FetchCallback::FetchCallback()
{
}

mail::copyMessage::FetchCallback::~FetchCallback()
{
}

void mail::copyMessage::FetchCallback::messageArrivalDateCallback(size_t dummy,
								  time_t dt)
{
	me->arrivalDate=dt;
}

void mail::copyMessage::FetchCallback::messageTextCallback(size_t dummy,
							   string text)
{
	if (me->addMessagePtr)
	{
		me->addMessagePtr->saveMessageContents(text);
	}
}

void mail::copyMessage::reportProgress()
{
	callback.reportProgress((fetchCompletedCnt + addCompleted)/2,
				(fetchEstimatedCnt + addEstimated)/2,
				completedCount, completedCount+1);
}

void mail::copyMessage
::FetchCallback::reportProgress(size_t bytesCompleted,
				size_t bytesEstimatedTotal,

				size_t messagesCompleted,
				size_t messagesEstimatedTotal)
{
	me->fetchCompletedCnt=bytesCompleted;
	me->fetchEstimatedCnt=bytesEstimatedTotal;
	me->reportProgress();
}


void mail::copyMessage::FetchCallback::success(string dummy)
{
	(me->*successCallback)();
}

void mail::copyMessage::FetchCallback::fail(string errmsg)
{
	ptr<copyMessage> cm(me);

	if (me->addMessagePtr != NULL) // Report it this way
	{
		addMessage *p=me->addMessagePtr;

		me->addMessagePtr=NULL;

		p->fail(errmsg);
	}
	else
		me->callback.fail(errmsg);


	if (!cm.isDestroyed())
	{
		copyMessage *p=cm;
		delete p;
	}
	return;
}

mail::copyMessage::AddCallback::AddCallback()
{
}

mail::copyMessage::AddCallback::~AddCallback()
{
}

void mail::copyMessage::AddCallback::success(string msg)
{
	me->addMessagePtr=NULL;
	me->success(me->callback, msg);
}

void mail::copyMessage::success(mail::callback &callback, string msg)
{
	delete this;
	callback.success(msg);
}

void mail::copyMessage::AddCallback::fail(string msg)
{
	me->addMessagePtr=NULL;
	me->callback.fail(msg);
	delete me;
}

void mail::copyMessage
::AddCallback::reportProgress(size_t bytesCompleted,
			      size_t bytesEstimatedTotal,

			      size_t messagesCompleted,
			      size_t messagesEstimatedTotal)
{
	me->addCompleted=bytesCompleted;
	me->addEstimated=bytesEstimatedTotal;
	me->reportProgress();
}

mail::copyMessage::~copyMessage()
{
	if (addMessagePtr)
	{
		mail::addMessage *p=addMessagePtr;

		addMessagePtr=NULL;
		p->fail("Message copy operation aborted.");
	}
}

void mail::copyMessage::go()
{
	try {
		fetch_callback.successCallback= &mail::copyMessage::fetchText;

		vector<size_t> messageNumVector;

		messageNumVector.push_back(messageNum);

		from->readMessageAttributes(messageNumVector,
					    mail::account::ARRIVALDATE,
					    fetch_callback);
	} catch (...) {
		callback.fail("An exception occurred while copying messages.  The copy is aborted.");
		delete this;
	}
}

void mail::copyMessage::fetchText()
{
	if (to.isDestroyed() || from.isDestroyed())
	{
		callback.fail("Server connection closed during copy.");
		delete this;
		return;
	}

	if (from->getFolderIndexSize() <= messageNum ||
	    from->getFolderIndexInfo(messageNum).uid != messageInfo.uid)
	{
		callback.fail("Original folder's contents modified during copy");
		delete this;
	}

	fetch_callback.successCallback= &mail::copyMessage::fetchCompleted;

	mail::addMessage *ptr;

	try {
		ptr=to->addMessage(add_callback);

		if (ptr == NULL)
			return;

		ptr->messageInfo=messageInfo;
		ptr->messageDate=arrivalDate;

	} catch (...) {
		callback.fail("An exception occurred while copying messages.  The copy is aborted.");
		delete this;
		return;
	}

	addMessagePtr=ptr;

	try {
		vector<size_t> messageNumVector;

		messageNumVector.push_back(messageNum);

		from->readMessageContent(messageNumVector, true,
					 mail::readBoth, fetch_callback);
	} catch (...) {

		addMessagePtr->
			fail("An exception occurred while copying messages.  The copy is aborted.");
		addMessagePtr=NULL;
		delete this;
	}
}

void mail::copyMessage::fetchCompleted()
{
	try {
		RunLater();
	} catch (...) {

		addMessagePtr->
			fail("An exception occurred while copying messages.  The copy is aborted.");
		addMessagePtr=NULL;
		delete this;
	}
}

void mail::copyMessage::RunningLater()
{
	try {
		addMessagePtr->go();
	} catch (...) {

		addMessagePtr->
			fail("An exception occurred while copying messages.  The copy is aborted.");
		addMessagePtr=NULL;
		delete this;
	}
}

//////////////////////////////////////////////////////////////////////////////

mail::copyMessages::Callback::Callback()
{
}

mail::copyMessages::Callback::~Callback()
{
}

void mail::copyMessages::Callback::success(string message)
{
	++me->copiedCount;
	--me->inProgress;
	reportProgress(0, 0, 0, 0);
	me->doSomething(message);
}

void mail::copyMessages::Callback::fail(string message)
{
	--me->inProgress;
	if (me->failMessage.size() == 0)
		me->failMessage=message;
	me->doSomething(message);
}

void mail::copyMessages
::Callback::reportProgress(size_t bytesCompleted,
			   size_t bytesEstimatedTotal,

			   size_t messagesCompleted,
			   size_t messagesEstimatedTotal)
{
	if (messagesCompleted >= me->progressUpdateCount)
	{
		me->progressUpdateCount=messagesCompleted;
		me->callback.reportProgress(bytesCompleted,
					    bytesEstimatedTotal,
					    me->copiedCount,
					    me->totalCount);
	}
}

mail::copyMessages::copyMessages(mail::account *copyFromArg,
				 const vector<size_t> &messagesArg,
				 mail::folder *copyToArg,
				 mail::callback &callbackArg)
	: callback(callbackArg), copyFrom(copyFromArg),
	  copyTo(copyToArg), inProgress(0), copiedCount(0),
	  totalCount(messagesArg.size()),
	  progressUpdateCount(0),
	  failMessage("")
{
	copy_callback.me=this;

	vector<size_t>::const_iterator b=messagesArg.begin(),
		e=messagesArg.end();

	while (b != e)
	{
		todo.push( make_pair( *b, copyFromArg
				      ->getFolderIndexInfo(*b).uid));
		b++;
	}
}

mail::copyMessages::~copyMessages()
{
}

void mail::copyMessages::RunningLater()
{
	doSomething("No messages copied.");
}

void mail::copyMessages::doSomething(string message)
{
	while (inProgress < 2 && !todo.empty() && failMessage.size() == 0)
	{
		try {
			size_t messageNum=todo.front().first;
			string uid=todo.front().second;

			todo.pop();

			if (copyFrom.isDestroyed() || copyTo.isDestroyed())
			{
				failMessage="Server connection closed during copy.";
				break;
			}

			if (copyFrom->getFolderIndexSize() <= messageNum ||
			    copyFrom->getFolderIndexInfo(messageNum).uid != uid)
			{
				failMessage="Original folders contents have changed in a middle of the copy operation.";
				break;
			}

			mail::copyMessage::copy(copyFrom, messageNum,
						copyTo, copy_callback);
			++inProgress;
		} catch (...) {
			failMessage="An exception occurred while copying messages.  The copy is aborted.";
		}
	}

	if (inProgress == 0 && (failMessage.size() > 0 || todo.empty()))
	{
		if (failMessage.size() > 0)
		{
			if (copiedCount > 0)
				try {
					string buffer;

					{
						ostringstream o;

						o << copiedCount;
						buffer=o.str();
					}

					failMessage += " (";
					failMessage += buffer;
					failMessage += " message(s) copied)";
				} catch (...) {
				}

			callback.fail(failMessage);
			delete this;
		}
		else
			success(callback, message);
	}
}

void mail::copyMessages::success(mail::callback &callback, string message)
{
	delete this;
	callback.success(message);
}

void mail::copyMessages::copy(mail::account *copyFrom, const vector<size_t> &messages,
			      mail::folder *copyTo,
			      mail::callback &callback)
{
	mail::copyMessages *cm=NULL;

	try {
		cm=new mail::copyMessages(copyFrom,
					  messages,
					  copyTo,
					  callback);

		if (!cm)
		{
			callback.fail("Out of memory.");
			return;
		}

		cm->RunLater();
	} catch (...) {
		if (cm)
			delete cm;
		callback.fail("An exception occurred while copying messages.  The copy is aborted.");
		return;
	}
}

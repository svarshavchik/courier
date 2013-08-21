/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_copymessage_H
#define libmail_copymessage_H

#include "libmail_config.h"
#include "mail.H"
#include "objectmonitor.H"
#include "runlater.H"

#include <vector>
#include <queue>

////////////////////////////////////////////////////////////////////////////
//
// This class copies a single message between two servers
//

LIBMAIL_START

class addMessage;

class copyMessage : public runLater {

	mail::callback &callback;

	ptr<mail::account> from;
	ptr<mail::folder> to;

	addMessage *addMessagePtr;

	size_t messageNum;
	mail::messageInfo messageInfo;
	time_t arrivalDate;

	copyMessage(mail::account *fromArg,
			  size_t messageNumArg,
			  mail::folder *toArg,
			  mail::callback &callbackArg);
	virtual ~copyMessage();

	void go();
	void fetchText();

	class FetchCallback : public mail::callback::message {
	public:
		copyMessage *me;

		void (copyMessage::*successCallback)();

		FetchCallback();
		~FetchCallback();

		void messageArrivalDateCallback(size_t messageNumber,
						time_t datetime);

		void messageTextCallback(size_t n, std::string text);

		void success(std::string);
		void fail(std::string);

		void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal);

	} fetch_callback;

	class AddCallback : public mail::callback {
	public:
		copyMessage *me;

		AddCallback();
		~AddCallback();

		void success(std::string);
		void fail(std::string);

		void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal);

	} add_callback;

	size_t fetchCompletedCnt;
	size_t fetchEstimatedCnt;

	size_t addCompleted;
	size_t addEstimated;

	void reportProgress();

	void fetchCompleted();
	void RunningLater();

	void success(mail::callback &callback, std::string msg);

public:
	size_t completedCount;

	friend class FetchCallback;
	friend class AddCallback;

	static void copy(mail::account *copyFrom, size_t messageNum,
			 mail::folder *copyTo,
			 mail::callback &callback,
			 size_t completedCount=0);
};

///////////////////////////////////////////////////////////////////////////
//
// A helper class for copying a range of messages.  This is done by
// instantiating multiple mail::copyMessage object.  We'll try to keep
// two instances going at a time, hopefully at all times one instance
// reads from the source server, and the second instance is uploading
// to the desintation server

class copyMessages : public runLater {

	mail::callback &callback;

	// To make sure that the source folder doesn't change, we'll
	// keep track of each message's UID.

	std::queue< std::pair<size_t, std::string> > todo;

	ptr<mail::account> copyFrom;
	ptr<mail::folder> copyTo;

	size_t inProgress; // # of copies in progress, at most 2.

	size_t copiedCount;
	size_t totalCount;

	size_t progressUpdateCount;

	std::string failMessage;

	// Helper

	class Callback : public mail::callback {
	public:
		copyMessages *me;

		Callback();
		~Callback();

		void success(std::string message);
		void fail(std::string message);

		void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal);


	} copy_callback;

	copyMessages(mail::account *copyFrom,
			   const std::vector<size_t> &messages,
			   mail::folder *copyTo,
			   mail::callback &callback);
	~copyMessages();

	void RunningLater();

	void doSomething(std::string message);

	void success(mail::callback &callback, std::string message);

public:

	static void copy(mail::account *copyFrom,
			 const std::vector<size_t> &messages,
			 mail::folder *copyTo,
			 mail::callback &callback);
};

LIBMAIL_END

#endif

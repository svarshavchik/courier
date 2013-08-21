/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_search_H
#define libmail_search_H

#include "libmail_config.h"

#if TIME_WITH_SYS_TIME
#include        <sys/time.h>
#include        <time.h>
#else
#if HAVE_SYS_TIME_H
#include        <sys/time.h>
#else
#include        <time.h>
#endif
#endif

#include "mail.H"
#include "structure.H"
#include "runlater.H"

#include "maildir/maildirsearch.h"


#include <vector>
#include <queue>

#include <time.h>

/////////////////////////////////////////////////////////////////////////
//
// Structure that describes the search criteria

#include <string>

LIBMAIL_START

class searchParams {

public:
	searchParams();
	~searchParams();

	bool searchNot; // If set, the following criteria is negated

	enum Criteria {

		// Messages with the following flags set:

		replied,
		deleted,
		draft,
		unread,

		// Header match

		from,
		to,
		cc,
		bcc,
		subject,

		header,
		body,
		text,

		// Internal date:

		before,
		on,    
		since,

		// Sent date,

		sentbefore,
		senton,
		sentsince,

		larger,
		smaller
	} criteria;

	std::string param1; // Header name, or date, or byte size
	std::string param2; // Header or content value

	std::string charset; // Charset for param2

	// All strings use UTF-8


	enum Scope {
		search_all, // Search all messages

		search_marked,	// Only those msgs already marked

		search_unmarked, // Only those msgs not already marked

		search_range // Only messages in the following range:
	} scope;

	size_t rangeLo, rangeHi; // Used for search_range only

	// Serialize/deserialize:

	searchParams(std::string);
	operator std::string() const;

	// INTERNAL USE:

	static std::string decode(std::string, std::string &);
	static std::string encode(std::string);
};

class searchCallback {
public:
	searchCallback();
	virtual ~searchCallback();

	virtual void success(const std::vector<size_t> &found)=0;
	virtual void fail(std::string)=0;

	virtual void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal)=0;

};

class searchOneMessage {

	searchCallback &callback;

//
// For searches, mail::callback::success is called with a non-empty string
// if the message matches, an empty string if it does not.
//

	searchParams searchInfo;
	time_t cmpDate;

	mail::ptr<mail::account> ptr;
	size_t messageNum;
	size_t alreadyCompleted;
	std::string uid;

	std::string searchCharset;

	class Callback : public mail::callback::message {
	public:

		searchOneMessage *me;

		void (searchOneMessage::*nextFunc)();

		Callback();
		~Callback();
		void success(std::string message);
		void fail(std::string message);

		void messageEnvelopeCallback(size_t messageNumber,
					     const class envelope
					     &envelope);

		void messageArrivalDateCallback(size_t messageNumber,
						time_t datetime);

		void messageSizeCallback(size_t messageNumber,
					 unsigned long size);

		void messageStructureCallback(size_t messageNumber,
					      const class mimestruct
					      &messageStructure);
		void messageTextCallback(size_t n, std::string text);

		void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal);
	};

	Callback my_callback;

	bool searchFlag;

	class Searcher : public mail::Search, public mail::iconvert {

		int converted(const char *str, size_t n);

	public:
		using mail::Search::operator bool;
		using mail::Search::operator !;
		using mail::iconvert::operator();
	};

	Searcher searchEngine;

	mimestruct structureBuffer;
	std::queue<mimestruct *> mimeSearch;

	std::string headerSearchBuffer;

	void doDateCmp(time_t);

public:

	searchOneMessage(searchCallback &callbackArg,
			 searchParams &searchInfoArg,
			 mail::account *ptrArg,
			 size_t messageNumArg,
			 size_t alreadyCompletedArg=0);

	~searchOneMessage();

	void go();

	void search(const envelope &envelope);
	void search(time_t internaldate);
	void search(unsigned long messageSize);
	void search(const mimestruct &structureInfo);
	void search(std::string text);

	void searchEnvelope(const envelope &envelope);
	void searchFwdEnvelope(mimestruct &structureInfo);

private:
	bool sanityCheck();
	void checkSearch();
	void checkNextHeader();
	void checkFwdEnvelope();
	void success(searchCallback &callback, size_t messageNum,
		     bool result);
};

class searchMessages : public runLater {

	searchCallback &callback;

	size_t nextMsgNum;

	std::string uid;

	std::vector<std::string> successArray;

	class Callback : public searchCallback {
	public:
		searchMessages *me;

		Callback();
		~Callback();

		void fail(std::string message);
		void success(const std::vector<size_t> &found);

		void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal);
	} search_callback;

	ptr<mail::account> server;

	searchMessages(searchCallback &callbackArg,
			     const searchParams &searchInfoArg,
			     mail::account *ptrArg);

	~searchMessages();

	searchParams searchInfo, searchInfoCpy;

	void nextSearch();
	void RunningLater();
public:

	friend class Callback;

	static void search(searchCallback &callbackArg,
			   const searchParams &searchInfoArg,
			   mail::account *ptrArg);
};

LIBMAIL_END

#endif

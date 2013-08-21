/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntppost_H
#define libmail_nntppost_H

#include "libmail_config.h"

#include "nntp.H"
#include "nntplogin.H"

#include <stdio.h>

LIBMAIL_START

//
// POST a mesasge
//

class mail::nntp::PostTask : public mail::nntp::LoggedInTask {

	void (mail::nntp::PostTask::*response_func)(const char *);

public:
	FILE *msg;
private:
	unsigned long tot_count, byte_count;

	class pump : public WriteBuffer {

		bool newLine;
	public:
		PostTask *me;

		pump(PostTask *);
		~pump();
		bool fillWriteBuffer();
	};

	pump *myPumper;
public:
	friend class pump;

	PostTask(callback *callbackArg, nntp &myserverArg,
		     FILE *msgArg);
	~PostTask();

	virtual void loggedIn();
	virtual void processLine(const char *message);

private:
	void doPostStatus(const char *);
	void doPost(const char *);
};

LIBMAIL_END
#endif

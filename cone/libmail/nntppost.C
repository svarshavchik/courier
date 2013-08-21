/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "nntppost.H"
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <cstring>

using namespace std;

mail::nntp::PostTask::PostTask(callback *callbackArg, nntp &myserverArg,
			       FILE *msgArg)
	: LoggedInTask(callbackArg, myserverArg), msg(msgArg),
	  myPumper(NULL)
{
}

mail::nntp::PostTask::~PostTask()
{
	if (myPumper)
	{
		myPumper->writebuffer += "\r\n.\r\n";
		myPumper->me=NULL;
	}
	if (msg)
		fclose(msg);
}

void mail::nntp::PostTask::loggedIn()
{
	struct stat stat_buf;

	if (fseek(msg, 0L, SEEK_SET) < 0 || fstat(fileno(msg), &stat_buf) < 0)
	{
		fail(strerror(errno));
		return;
	}

	tot_count=stat_buf.st_size;
	byte_count=0;
	response_func= &mail::nntp::PostTask::doPostStatus;
	myserver->socketWrite("POST\r\n");
}

void mail::nntp::PostTask::processLine(const char *message)
{
	(this->*response_func)(message);
}

mail::nntp::PostTask::pump::pump(PostTask *p) : newLine(true), me(p)
{
}

mail::nntp::PostTask::pump::~pump()
{
	if (me)
		me->myPumper=NULL;
}

bool mail::nntp::PostTask::pump::fillWriteBuffer()
{
	if (!me)
		return false;

	char buffer[BUFSIZ];

	int n=fread(buffer, 1, sizeof(buffer), me->msg);

	if (n <= 0)
	{
		if (!newLine)
			writebuffer += "\r\n";
		writebuffer += ".\r\n";
		me->myPumper=NULL;
		me=NULL;
		return true;
	}

	// Copy the read chunk into the writebuffer, convert NLs to CRNLs,
	// and dot-stuffing.

	char *b=buffer, *e=b + n, *c=b;

	while (b != e)
	{
		if (newLine && *b == '.') // Leading dot, double it.
		{
			writebuffer.insert(writebuffer.end(), c, b+1);
			c=b; // Net effect is the dot doubled.
		}

		if ((newLine= *b == '\n') != 0)
		{
			if (c != b)
				writebuffer.insert(writebuffer.end(), c, b);
			writebuffer += "\r";
			c=b;
		}
		b++;
	}

	if (c != b)
		writebuffer.insert(writebuffer.end(), c, b);

	me->byte_count += n;

	if (me->tot_count < me->byte_count)
		me->tot_count=me->byte_count;

	me->callbackPtr->reportProgress(me->byte_count, me->tot_count, 0, 1);
	return true;
}

void mail::nntp::PostTask::doPostStatus(const char *resp)
{
	if (resp[0] != '3')
	{
		fail(resp);
		return;
	}

	response_func= &mail::nntp::PostTask::doPost;
	pump *p=new pump(this);

	if (p)
		try {
			myserver->socketWrite(p);
			myPumper=p;
			return;
		} catch (...) {
			delete p;
		}

	myserver->socketWrite(".\r\n");
}

void mail::nntp::PostTask::doPost(const char *msg)
{
	if (myPumper)
	{
		myPumper->writebuffer += "\r\n.\r\n";
		myPumper->me=NULL;
	}
	myPumper=NULL;

	switch (msg[0]) {
	case '2':
	case '1':
		success(msg);
		break;
	default:
		fail(msg);
	}
}

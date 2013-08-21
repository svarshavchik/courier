/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "nntpxpat.H"
#include <sstream>

using namespace std;

mail::nntp::XpatTaskCallback::XpatTaskCallback(mail::searchCallback *cb)
	: realCallback(cb)
{
}

mail::nntp::XpatTaskCallback::~XpatTaskCallback()
{
}

void mail::nntp::XpatTaskCallback::success(std::string message)
{
	// Should NOT happen

	fail(message);
}

void mail::nntp::XpatTaskCallback::fail(std::string message)
{
	mail::searchCallback *cb=realCallback;

	realCallback=NULL;

	delete this;

	cb->fail(message);
}

void mail::nntp::XpatTaskCallback::reportProgress(size_t bytesCompleted,
						  size_t bytesEstimatedTotal,

						  size_t messagesCompleted,
						  size_t messagesEstimatedTotal
						  )
{
	realCallback->reportProgress(bytesCompleted, bytesEstimatedTotal,
				     messagesCompleted,
				     messagesEstimatedTotal);
}


mail::nntp::XpatTask::XpatTask(XpatTaskCallback *cbArg,
			       nntp &serverArg, string groupName,
			       string hdrArg,
			       string srchArg, bool searchNotArg,
			       searchParams::Scope searchScopeArg,
			       size_t rangeLoArg, size_t rangeHiArg)
	: GroupTask(cbArg, serverArg, groupName),
	  cb(cbArg),
	  hdr(hdrArg),
	  srch(srchArg),
	  searchNot(searchNotArg),
	  searchScope(searchScopeArg),
	  rangeLo(rangeLoArg),
	  rangeHi(rangeHiArg)
{
}

mail::nntp::XpatTask::~XpatTask()
{
	if (cb)
		delete cb;
}

void mail::nntp::XpatTask::selectedGroup(msgnum_t estimatedCount,
			   msgnum_t loArticleCount,
			   msgnum_t hiArticleCount)
{
	response_func= &mail::nntp::XpatTask::processStatusResponse;

	vector<mail::nntp::nntpMessageInfo>::iterator b, e;

	b=myserver->index.begin();
	e=myserver->index.end();

	while (b != e)
	{
		b->msgFlag &= ~IDX_SEARCH;

		++b;
	}

	lastIdxMsgNum=myserver->index.begin();
	ostringstream o;

	o << "XPAT " << hdr << " " <<
		(myserver->index.size() > 0 ?
		 myserver->index[0].msgNum:1) << "- *" << srch << "*\r\n";

	myserver->socketWrite(o.str());

}

void mail::nntp::XpatTask::processGroup(const char *line)
{
	(this->*response_func)(line);
}

void mail::nntp::XpatTask::processStatusResponse(const char *line)
{
	if (line[0] != '2')
	{
		done(line);
		return;
	}
	response_func= &mail::nntp::XpatTask::processXpatResponse;
}

void mail::nntp::XpatTask::processXpatResponse(const char *l)
{
	if (*l == '.')
		done("OK");

	msgnum_t n;

	istringstream i(l);

	i >> n;

	if (i.fail() || myserver->index.size() == 0)
		return;

	if (lastIdxMsgNum == myserver->index.end())
		--lastIdxMsgNum;

	for (;;)
	{
		if (lastIdxMsgNum->msgNum > n)
		{
			if (lastIdxMsgNum == myserver->index.begin())
				break;
			--lastIdxMsgNum;
			if (lastIdxMsgNum->msgNum < n)
				break;
			continue;
		}
		else if (lastIdxMsgNum->msgNum < n)
		{
			if (++lastIdxMsgNum == myserver->index.end())
				break;
			if (lastIdxMsgNum->msgNum > n)
				break;
		}
		else
		{
			myserver->index[lastIdxMsgNum
					- myserver->index.begin()].msgFlag
				|= IDX_SEARCH;
			break;
		}
	}
}

void mail::nntp::XpatTask::done(const char *l)
{
	unsigned char Xor=0;
	unsigned char Ror=0;

	if (searchNot)
		Xor=IDX_SEARCH;

	switch (searchScope) {
	case searchParams::search_all:
		Ror=IDX_SEARCH;
		break;

	case searchParams::search_unmarked:
		Xor |= IDX_FLAGGED;
		break;

	case searchParams::search_marked:
		break;

	case searchParams::search_range:
		Ror=IDX_FLAGGED;
		break;
	}

	vector<size_t> searchResults;

	vector<nntpMessageInfo>::iterator bb,b, e;

	bb=b=myserver->index.begin();
	e=myserver->index.end();

	if (searchScope == searchParams::search_range)
	{
		if (rangeHi > myserver->index.size())
			rangeHi=myserver->index.size();

		if (rangeLo > rangeHi)
			rangeLo=rangeHi;

		e= b + rangeHi;
		b += rangeLo;
	}

	while (b != e)
	{
		unsigned char f= (b->msgFlag ^ Xor) | Ror;

		if ((f & IDX_FLAGGED) && (f & IDX_SEARCH))
			searchResults.push_back(b - bb);

		++b;

	}

	searchCallback *cbPat=cb->realCallback;

	delete cb;
	cb=NULL;
	callbackPtr=NULL;

	try {
		Task::done();
	} catch (...) {
		cbPat->success(searchResults);
		throw;
	}
	cbPat->success(searchResults);
}

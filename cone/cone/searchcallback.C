/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "searchcallback.H"
#include "myservercallback.H"
#include "init.H"
#include "gettext.H"

using namespace std;

SearchCallbackHandler::SearchCallbackHandler(myServer::Callback &callbackArg)
	: callback(callbackArg)
{
}

SearchCallbackHandler::~SearchCallbackHandler()
{
}

void SearchCallbackHandler::reportProgress(size_t bytesCompleted,
					   size_t bytesEstimatedTotal,

					   size_t messagesCompleted,
					   size_t messagesEstimatedTotal)
{
	myServer::reportProgress(bytesCompleted,
				 bytesEstimatedTotal,
				 messagesCompleted,
				 messagesEstimatedTotal);
}

void SearchCallbackHandler::success(const vector<size_t> &found)
{
	messageNumbers=found;
	callback.success(Gettext(_("Found %1% messages.")) << found.size());
}

void SearchCallbackHandler::fail(string errmsg)
{
	statusBar->clearstatus();
	callback.fail(errmsg);
}

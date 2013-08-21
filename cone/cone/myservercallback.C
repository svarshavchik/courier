/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "myservercallback.H"
#include "messagesize.H"
#include "gettext.H"

#include <sstream>

using namespace std;

extern CursesStatusBar *statusBar;

myServer::Callback::Callback(CursesStatusBar::statusLevel levelArg)
	: level(levelArg), succeeded(false), failed(false), interrupted(false),
	  noreport(false)
{
}

myServer::Callback::~Callback()
{
}

CursesStatusBar::statusLevel myServer::Callback::getLevel() const
{
	return level;
}

void myServer::Callback::success(string message)
{
	msg=message; succeeded=true;
}

void myServer::Callback::fail(string message)
{
	msg=message; failed=true;
}

void myServer::Callback::reportProgress(size_t bytesCompleted,
					size_t bytesEstimatedTotal,

					size_t messagesCompleted,
					size_t messagesEstimatedTotal)
{
	myServer::reportProgress(bytesCompleted,
				 bytesEstimatedTotal,
				 messagesCompleted,
				 messagesEstimatedTotal);
}

////////////////////////////////////////////////////////////////////
//
// Report the progress of the command.  The trick is to stall repeated
// callbacks, and only update the status bar periodically.

void myServer::reportProgress(size_t bytesCompleted,
			      size_t bytesEstimatedTotal,

			      size_t messagesCompleted,
			      size_t messagesEstimatedTotal)
{
	// statusBar will only want progress once a second, however if
	// the report indicates that the progress has finished, update
	// anyway.

	if (!statusBar->progressWanted()
	    && (bytesCompleted != bytesEstimatedTotal
		|| bytesCompleted == 0))
		return;

	string completedStr=MessageSize(bytesCompleted);
	string estimatedStr=MessageSize(bytesEstimatedTotal);

	if (bytesEstimatedTotal < bytesCompleted)
		estimatedStr=completedStr; // Sanity check.

	string byteProgress=
		bytesEstimatedTotal > 0 // 0 estimate means dunno.

		? (string)(Gettext(_("%1% of %2%"))
			   << completedStr << estimatedStr)
		: completedStr;

	ostringstream msgDone;
	ostringstream msgTotal;

	msgDone << messagesCompleted;
	msgTotal << messagesEstimatedTotal;

	string cntDone=msgDone.str();
	string cntTotal=msgTotal.str();

	if (messagesEstimatedTotal < messagesCompleted)
		cntTotal=cntDone; // Sanity check.

	string msgProgress= messagesEstimatedTotal > 0
		? (string)(Gettext(_("%1% of %2%")) << cntDone << cntTotal)
		: cntDone;

	if (bytesCompleted == 0 && bytesEstimatedTotal == 0)
		statusBar->progress(Gettext(_(" (%1% done)"))
				    << msgProgress); // No byte cnt, just phase
	else
		statusBar->progress(Gettext(messagesEstimatedTotal > 1 ?
					    _(" (%1%; %2% done)"):
					    _(" (%1% done)"))
				    << byteProgress << msgProgress);
}

myServer::CreateFolderCallback::CreateFolderCallback() : folderFound(NULL)
{
}

myServer::CreateFolderCallback::~CreateFolderCallback()
{
	if (folderFound)
		delete folderFound;
}

void myServer::CreateFolderCallback
::success(const vector<const mail::folder *> &folders)
{
	if (folders.size() == 1)
		folderFound=folders[0]->clone();
}

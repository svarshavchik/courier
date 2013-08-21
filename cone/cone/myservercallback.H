/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef myservercallback_h
#define myservercallback_h

#include "config.h"
#include "myserver.H"
#include "libmail/mail.H"
#include "curses/cursesstatusbar.H"

#include <string>
#include <vector>

/////////////////////////////////////////////////////////////////////////
//
// Generic callback used by myServer::eventloop()

class myServer::Callback : public mail::callback {
	CursesStatusBar::statusLevel level;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	bool succeeded;	// Libmail succeeded
	bool failed;	// Libmail failed
	bool interrupted;	// Libmail wants a callback (at login time)
	bool noreport;	// Shut up.
	std::string msg;	// Err msg from libmail.

	Callback(CursesStatusBar::statusLevel
		 levelArg=CursesStatusBar::SERVERERROR);
	~Callback();

	CursesStatusBar::statusLevel getLevel() const;
	void success(std::string message);
	void fail(std::string message);
};


///////////
//
// Helper class when creating a new folder.  The new folder object is
// captured

class myServer::CreateFolderCallback : public mail::callback::folderList {

public:
	mail::folder *folderFound;

	CreateFolderCallback();
	~CreateFolderCallback();

	void success(const std::vector<const mail::folder *> &folders);
};

#endif

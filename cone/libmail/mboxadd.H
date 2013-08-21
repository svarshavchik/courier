/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mboxfolderadd_H
#define libmail_mboxfolderadd_H

#include "libmail_config.h"
#include "mbox.H"
#include "mboxlock.H"
#include "addmessage.H"

#include <stdio.h>

LIBMAIL_START

///////////////////////////////////////////////////////////////////////////
//
// Add a message to a folder.

class file;

class mbox::folder::add : public addMessage,
		  public mail::callback {

	std::string path;
	mail::callback &callback;
	FILE *fp;

	ptr<mbox> mboxAccount;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,
			    
			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

	//
	// Lock whatever folder's being added to.
	//

	class LockCurrentFolder : public mbox::LockTask {

		add &me;

	public:
		LockCurrentFolder(add &meArg, std::string pathArg="");
		~LockCurrentFolder();

		bool locked(file &lockedFile);
	};

public:
	add(mbox &mboxArg, std::string pathArg,
		   mail::callback &callbackArg);
	~add();

	void saveMessageContents(std::string);
	void go();
	void fail(std::string);

	void success(std::string);

	void copyTo(file &file);
};

LIBMAIL_END

#endif


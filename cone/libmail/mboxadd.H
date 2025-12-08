/*
** Copyright 2002, S. Varshavchik.
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

	mail::ptr<mbox> mboxAccount;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal) override;

	//
	// Lock whatever folder's being added to.
	//

	class LockCurrentFolder : public mbox::LockTask {

		add &me;

	public:
		LockCurrentFolder(add &meArg, std::string pathArg="");
		~LockCurrentFolder();

		bool locked(file &lockedFile) override;
	};

public:
	add(mbox &mboxArg, std::string pathArg,
		   mail::callback &callbackArg);
	~add();

	void saveMessageContents(std::string) override;
	void go() override;
	void fail(std::string message) override;

	void success(std::string) override;

	void copyTo(file &file);
};

LIBMAIL_END

#endif

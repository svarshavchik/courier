/*
** Copyright 2002-2006, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mboxsighandler_H
#define libmail_mboxsighandler_H

#include "libmail_config.h"
#include "mbox.H"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

LIBMAIL_START

///////////////////////////////////////////////////////////////////////
//
// Trap process signals when updating an mboxAccount file.  A received signal
// restores an mboxAccount file to its original size.

class mbox::sighandler {

	int fd;
	off_t origSize;

	static void handler(int);

	static mbox::sighandler *curHandler;

public:
	sighandler(int fd);
	off_t getOrigSize() const { return (origSize); }
	void block();
	void rollback() const
	{
		if (ftruncate(fd, origSize) < 0)
			; // Ignore gcc warning
	}

	~sighandler();
};

LIBMAIL_END

#endif

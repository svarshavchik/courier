/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_file_H
#define libmail_file_H

#include "libmail_config.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <vector>
#include <string>

#include "namespace.H"
#include "mail.H"

///////////////////////////////////////////////////////////////////////////
// A wrapper for a stdio FILE object, that mail::mbox finds very useful
//
// The constructor accepts a file descriptor, and a file access mode.
// The original file descriptor is not touched, the object dupes it, and
// attaches the dupe to a FILE object.
//
// The FILE object is cleaned up by the destructor.

#include "mail.H"

LIBMAIL_START

class file {
	int fd;
	FILE *fp;
	off_t pos;

	std::vector<char> buffer;
public:
	file(int fdArg, const char *mode);
	~file();
	operator FILE *() const { return fp; }

	bool operator !() const { return fp == NULL; }

	std::string getline(); // Read a line of text

	void seeked();	// The caller seeked in the file, update pos.

	off_t tell() const { return pos; }


	void genericMessageRead(mail::account *account,
				size_t messageNumber,
				mail::readMode readType,
				mail::callback::message &callback);
};

LIBMAIL_END

#endif


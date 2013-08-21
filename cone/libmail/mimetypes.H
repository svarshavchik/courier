/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_mimetypes_H
#define libmail_mimetypes_H

///////////////////////////////////////////////////////////////////////////
//
// Convenient class for searching the mime.types file
//
// A mime.types file contains lines formatted as follows:
//
// type/subtype   ext1 ext2 ext3
//
// Filenames ending in .ext1, .ext2, or .ext3 are assigned to type/subtype
// MIME type

#include <string>
#include <vector>

#include "namespace.H"

LIBMAIL_START

class mimetypes {
public:
	mimetypes(std::string searchKey);
	// Either an extension or type/subtype

	~mimetypes();

	std::string type;
	std::vector<std::string> extensions;

	bool found() { return type.size() > 0; }
};

LIBMAIL_END

#endif

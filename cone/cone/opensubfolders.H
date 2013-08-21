/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef opensubfolders_H
#define opensubfolders_H

#include "config.h"

#include "libmail/mail.H"

#include <vector>

///////////////////////////////////////////////////////////
//
// Helper class for opening a hierarchy.  The list of open subfolders,
// by LibMAIL is captured.
//

class OpenSubFoldersCallback :
	public mail::callback::folderList {

public:
	std::vector<mail::folder *> folders;

	OpenSubFoldersCallback();
	~OpenSubFoldersCallback();
	void success(const std::vector<const mail::folder *> &folders);
};

#endif

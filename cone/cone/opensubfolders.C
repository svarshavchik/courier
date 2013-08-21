/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "opensubfolders.H"
#include "gettext.H"

using namespace std;

OpenSubFoldersCallback::OpenSubFoldersCallback()
{
}

OpenSubFoldersCallback::~OpenSubFoldersCallback()
{
	vector<mail::folder *>::iterator b=folders.begin(), e=folders.end();

	while (b != e)
		delete *b++;
}

// Make a copy of opened folders.

void OpenSubFoldersCallback::success(const vector<const mail::folder *>
				     &folders)
{
	vector<const mail::folder *>::const_iterator
		b=folders.begin(), e=folders.end();

	while (b != e)
	{
		mail::folder *f= (*b++)->clone();

		if (!f)
			outofmemory();

		try {
			this->folders.push_back(f);
		} catch (...)
		{
			delete f;
		}
	}
}

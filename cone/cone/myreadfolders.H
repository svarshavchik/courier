/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef myreadfolders_H
#define myreadfolders_H

#include "config.h"
#include "libmail/mail.H"

#include <vector>
#include <string>

////////////////////////////////////////////////////////////////////
//
// A helper subclass of mail::callback::folderList that makes a copy
// of the folder list for internal, private, use.
//
// The folder list is also sorted in locale-collating order

class myReadFolders : public mail::callback::folderList {

	std::vector <std::string> folder_list;	// We only need to keep the folder's
					// toString() format.

	bool foldersFirst;

public:
	myReadFolders(bool foldersFirst=true);
	// Sort folders first

	~myReadFolders();

	myReadFolders(const myReadFolders &);
	myReadFolders &operator=(const myReadFolders &);

	void success(const std::vector<const mail::folder *> &folders);
	void success(const std::vector<const mail::folder *> &folders, bool doSort);
	void add(std::string s) { folder_list.push_back(s); }

	size_t size() const { return folder_list.size(); }

	std::string operator[](size_t n) const
	{
		return folder_list[n];
	}

	typedef std::vector <std::string>::iterator iterator;

	iterator begin()
	{
		return folder_list.begin();
	}

	iterator end()
	{
		return folder_list.end();
	}
};
#endif

/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "myreadfolders.H"
#include "gettext.H"
#include <algorithm>
#include <string.h>

using namespace std;

myReadFolders::myReadFolders(const myReadFolders &o)
{
	(*this)=o;
}

myReadFolders &myReadFolders::operator=(const myReadFolders &o)
{
	folder_list=o.folder_list;
	return *this;
}


myReadFolders::myReadFolders(bool foldersFirstArg)
	: foldersFirst(foldersFirstArg)
{
}

myReadFolders::~myReadFolders()
{
}

void myReadFolders::success(const vector<const mail::folder *> &folders)
{
	success(folders, true);
}

void myReadFolders::success(const vector<const mail::folder *> &folders,
			    bool doSort)
{
	vector<const mail::folder *> cpy=folders;

	if (doSort)
		sort(cpy.begin(), cpy.end(),
		     mail::folder::sort(foldersFirst));

	vector<const mail::folder *>::const_iterator
		b=cpy.begin(),
		e=cpy.end();

	folder_list.clear();
	while (b != e)
	{
		const mail::folder *fptr= *b++;

		folder_list.push_back(fptr->toString());
	}
}

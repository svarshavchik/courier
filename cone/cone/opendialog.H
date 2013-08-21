/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef opendialog_H
#define opendialog_H

#include "config.h"

#include "curses/cursesfilereq.H"

#include <string>

//
// A CursesFileReq subclass for opening an existing file.
//

class OpenDialog : public CursesFileReq {

	std::vector<std::string> filenames;

	bool closing;
	bool noMultiplesFlag;
public:
	OpenDialog();
	~OpenDialog();

	void noMultiples(bool flag=true)
	{
		noMultiplesFlag=true;
	}

	void selected(std::vector<std::string> &);
	void abort();

	std::vector<std::string> &getFilenameList();
};

#endif

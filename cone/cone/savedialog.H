/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef savedialog_H
#define savedialog_H

#include "config.h"

#include "curses/cursesfilereq.H"

#include <string>

//
// A CursesFileReq subclass for creating a new file.

class SaveDialog : public CursesFileReq {

	std::string filename;

	bool closing;
public:
	SaveDialog(std::string defaultName="");
	~SaveDialog();

	void selected(std::vector<std::string> &);
	void abort();

	operator std::string();
};

#endif

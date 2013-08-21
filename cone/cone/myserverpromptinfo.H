/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef myserverpromptinfo_h
#define myserverpromptinfo_h

#include "config.h"
#include "myserver.H"
#include "curses/mycurses.H"
#include "gettext.H"

#include <string>
#include <vector>

///////////////////////////////////////////////////////////////////////////
//
// myServer::prompt() receives this object, describing what kind of prompt
// to put on the status line.

class myServer::promptInfo {
public:
	std::string prompt;
	std::string value;
	bool isPassword; // true - password prompt
	bool isYesNo;    // true - yes/no prompt

	// Additional options:
	std::vector< std::pair<std::string, std::string> > optionHelp;
	std::vector< unicode_char > optionList;

	Curses *currentFocus; // Internal use - saves previous focus.

	bool abortflag; // At exit, prompt was aborted.

	promptInfo(std::string promptArg);
	~promptInfo();

	// Convenience functions:

	promptInfo &initialValue(std::string v);
	promptInfo &password();
	promptInfo &yesno();

	promptInfo &option(Gettext::Key &theKey,
			   std::string keyname,
			   std::string keydescr);

	operator std::string() { return value; }

	unicode_char firstChar();

	bool aborted() const { return abortflag; }
};

#endif

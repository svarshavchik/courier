/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef searchprompt_H
#define searchprompt_H

#include "config.h"
#include "libmail/search.H"

#include <string>

////////////////////////////////////////////////////////////////////////////
//
// searchPrompt::prompt() interactively prompts for a search criteria, and
// initializes a searchInfo object.  Everything in searchInfo is prompted
// for, except for scope.
//
// An extra option, "select All" is also prompted for.  At exit, selectAll
// is set to true if this option was chosen instead of a search criteria.
//
// searchPrompt::prompt() returns false if the search prompting was aborted
// or true if a search criteria was prompted for.


class searchPrompt {
public:

	static bool prompt(mail::searchParams &searchInfo,
			   bool &selectAll);

private:
	static bool searchPromptType(mail::searchParams &searchInfo,
				     std::string &prompt1,
				     std::string &prompt2,
				     bool &selectAll);

	static bool searchPromptStatus(mail::searchParams &searchInfo);

	static bool searchPromptText(mail::searchParams &searchInfo,
				     std::string &prompt1,
				     std::string &prompt2);

	static bool searchPromptDate(mail::searchParams &searchInfo);

	static bool searchPromptDate(mail::searchParams &searchInfo,
				     std::string promptStr);

	static bool searchPromptSize(mail::searchParams &searchInfo);

	static bool searchPromptSize(mail::searchParams &searchInfo,
				     std::string promptStr);

	static void dayAdd(int &d, int &m, int &y, int howMuch);
	static void monAdd(int &d, int &m, int &y, int howMuch);
	static void yearAdd(int &d, int &m, int &y, int howMuch);
	static int ldom(int &mm, int &yy);
};


#endif

/*
** Copyright 2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesflowedline_H
#define cursesflowedline_H

#include <string>

//
// A line of text, a string, encoded in UTF-8, and a flag whether this line
// flows (or wraps) into the next one.
//

class CursesFlowedLine {

public:
	std::string text;
	bool flowed;

	CursesFlowedLine(const std::string &textArg="", bool flowedArg=false)
		: text(textArg), flowed(flowedArg)
	{
	}

	CursesFlowedLine(const std::vector<unicode_char> &textArg,
			 bool flowedArg);
};

#endif

/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "libmail/misc.H"
#include "tags.H"
#include "gettext.H"
#include <sstream>
#include <cstring>

using namespace std;

Tags::Tags()
{
	names.push_back(_("Normal"));
	names.push_back(_("Important"));
	names.push_back(_("Work"));
	names.push_back(_("Personal"));
	names.push_back(_("To Do"));
	names.push_back(_("Later"));
	names.push_back(_("Custom1"));
	names.push_back(_("Custom2"));
	names.push_back(_("Custom3"));
	names.push_back(_("Custom4"));
}

Tags::~Tags()
{
}

string Tags::getTagName(size_t n) const
{
	ostringstream o;

	o << "$Label" << n;
	return o.str();
}

bool Tags::isTagName(std::string s, size_t &n) const
{
	mail::upper(s);

	if (strncmp(s.c_str(), "$LABEL", 6) == 0)
	{
		n=0;

		istringstream i(s.substr(6));

		i >> n;

		if (!i.fail() && !i.bad() &&
		    n < Tags::tags.names.size())
			return true;
	}
	return false;
}

Tags Tags::tags;


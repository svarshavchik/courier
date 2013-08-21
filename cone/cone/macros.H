/*
** Copyright 2004-2010, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef macros_H
#define macros_H

#include "config.h"

#include <string>
#include <map>
#include <vector>
#include "unicode/unicode.h"

/*
** Memorized shortcuts and external filters
*/

class Macros {

public:
	/*
	** Name of a shortcut
	*/

	class name {
	public:
		unsigned f; // If > 0, function key #+1, else n:
		std::vector<unicode_char> n;

		name(std::vector<unicode_char> &s) : f(0), n(s) {}
		name(int fkey) : f(fkey+1) {}
		~name() {}

		bool operator==(const name &o) const
		{
			return f == o.f && n == o.n;
		}

		bool operator<(const name &o) const
		{
			return f < o.f || (f == o.f && n < o.n);
		}
	};

	// Shortcuts

	std::map<name, std::string> macroList;

	// External filters

	// Key is filter name. Value is a std::pair of filter name, filter
	// command, both use UTF-8.

	std::map<int, std::pair<std::string, std::string> > filterList;

	Macros();
	~Macros();

	static Macros *getRuntimeMacros();
};

#endif

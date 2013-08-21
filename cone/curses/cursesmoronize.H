/*
** Copyright 2003-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesmoronize_H
#define cursesmoronize_H

#include "mycurses.H"

///////////////////////////////////////////////////////////////////////////
//
// "Smart characters" processing, a.k.a. moronization
// Example:  Convert typed "1/4" to the ISO-8859-1 character for 1/4.
//

class CursesMoronize {

public:


	static size_t moronize(const unicode_char *buf,
			       std::vector<unicode_char> &nreplaced);
	//
	// 'buf' should be the characters just preceding the current cursor
	// position, IN REVERSE ORDER.  So, if the cursor is now:
	//
	//           ...1/4_       _ marks the cursor position
	//
	// ... then 'buf' should be '4/1...'.  buf can be as long as the
	// caller wants.  We search for a suitable replacement using
	// strncmp
	//
	// If found a replacement, returns non-zero chars to replace, and
	// set nreplaced to the replacement character

	class Entry {
	public:
		const unicode_char *keycode;
		size_t keycodeLen;
		const unicode_char *replacements;
	};

	static bool enabled;
	static Entry moronizationList[];

	static const int max_keycode_len=5;

};

#endif

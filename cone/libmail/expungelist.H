/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_expungelist_H
#define libmail_expungelist_H

#include "mail.H"
#include "namespace.H"

#include <list>
#include <vector>

LIBMAIL_START

//
// Helper class: prepare a list of removed message ranges for
// mail::callback::folder::messagesRemoved.  Iterating over a list of
// removed messages, IN REVERSE ORDER, this class builds a list of start-end
// ranges, which are then converted to an array.

class expungeList {
	std::list< std::pair<size_t, size_t> > list;

public:
	typedef std::list< std::pair<size_t, size_t> >::iterator iterator;

	expungeList();
	~expungeList();

	void operator<<(size_t); // Iterate in REVERSE ORDER

	void operator>>(mail::callback::folder *cb);

	iterator begin() { return list.begin(); }
	iterator end() { return list.end(); }
};

LIBMAIL_END

#endif

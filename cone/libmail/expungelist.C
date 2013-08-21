/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "expungelist.H"

mail::expungeList::expungeList()
{
}

mail::expungeList::~expungeList()
{
}

void mail::expungeList::operator<<(size_t n) // Iterate in REVERSE ORDER
{
	if (!list.empty() && n + 1 == list.begin()->first)
		--list.begin()->first;
	else
		list.insert(list.begin(), std::make_pair(n, n));
}

void mail::expungeList::operator>>(mail::callback::folder *cb)
{
	std::vector< std::pair<size_t, size_t> > v;

	v.reserve(list.size());

	while (!list.empty())
	{
		v.push_back(*list.begin());
		list.pop_front();
	}

	if (cb && v.size() > 0)
		cb->messagesRemoved(v);
}

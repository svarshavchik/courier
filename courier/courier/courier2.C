/*
** Copyright 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"comtrack.h"

#include	<string>
#include	<map>
#include	<vector>
#include	<algorithm>
#include	<iostream>

static int get_tracked_addresses_cb(time_t timestamp,
				    int status,
				    const char *address,
				    void *voidarg)
{
	std::map<std::string, int> *p=
		(std::map<std::string, int> *)voidarg;
	std::map<std::string, int>::iterator i;

	if (status == TRACK_ADDRACCEPTED)
	{
		i=p->find(address);

		if (i != p->end())
			p->erase(i);
	}
	else
	{
		(*p)[address]=status;
	}
	return (0);
}

static void get_tracked_addresses(std::vector<std::string> &addresses)
{
	std::map<std::string, int> addrmap;

	track_read_email(get_tracked_addresses_cb, &addrmap);
	std::map<std::string, int>::iterator b, e;

	addresses.reserve(addrmap.size());

	b=addrmap.begin();
	e=addrmap.end();

	while (b != e)
	{
		addresses.push_back(b->first);
		++b;
	}
}

extern "C" {
	void courier_clear_all()
	{
		std::vector<std::string> addresses;

		get_tracked_addresses(addresses);

		std::vector<std::string>::iterator
			b=addresses.begin(),
			e=addresses.end();

		while (b != e)
		{
			track_save_email(b->c_str(), TRACK_ADDRACCEPTED);
			++b;
		}
	}

	void courier_show_all()
	{
		std::vector<std::string> addresses;

		get_tracked_addresses(addresses);

		std::sort(addresses.begin(), addresses.end());

		std::vector<std::string>::iterator
			b=addresses.begin(),
			e=addresses.end();

		while (b != e)
		{
			std::cout << *b++ << std::endl;
		}
	}
}

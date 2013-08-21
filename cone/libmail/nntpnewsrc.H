/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_nntpnewsrc_H
#define libmail_nntpnewsrc_H

#include "libmail_config.h"
#include "nntp.H"
#include <string>
#include <vector>

LIBMAIL_START

/////////////////////////////////////////////////////////////////////////////
//
// The objects that represents lines in a newsrc file

class mail::nntp::newsrc {

public:
	std::string newsgroupname;
	bool subscribed;

	std::vector< std::pair<msgnum_t, msgnum_t> > msglist;
	// A list of read messages, in strict ascending order.

	newsrc();
	~newsrc();

	newsrc(std::string); // Construct from a line of text in newsrc

	operator std::string() const;


	void read(msgnum_t);
	void unread(msgnum_t);
};

LIBMAIL_END

#endif

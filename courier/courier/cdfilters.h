/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	cdfilters_h
#define	cdfilters_h

/*
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif

#include	<string>

int run_filter(
	const char *,		// Filename  containing the message
	unsigned,		// How many message IDs (can be >1 if
				// message was split due to too many
				// recipients
	int,			// whitelisted flag, used to select
				// filters
	std::string (*)(unsigned, void *), // Function to return message id
				//  #n (n is 0..count-1)
	void *);		// Second argument to function.

#endif

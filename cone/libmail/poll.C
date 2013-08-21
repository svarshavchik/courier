/*
** Copyright 2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "mail.H"
#include "soxwrap/soxwrap.h"
#include <sys/poll.h>

int mail::account::poll(std::vector<mail::pollfd> &fds, int timeout)
{
	int npfd=fds.size();

	struct ::pollfd *pfd=npfd == 0 ? NULL: &fds[0];

	return ::sox_poll(pfd, npfd, timeout);
}

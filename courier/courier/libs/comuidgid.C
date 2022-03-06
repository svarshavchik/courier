/*
** Copyright 2022 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "config.h"
#include "courier.h"
#include "comuidgid.h"
#include "numlib/numlib.h"

uid_t courier_mailuid()
{
	static const uid_t u=libmail_getuid(MAILUSER, 0);

	return u;
}

gid_t courier_mailgid()
{
	static const gid_t g=libmail_getgid(MAILGROUP);

	return g;
}

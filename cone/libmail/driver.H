/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_driver_H
#define libmail_driver_H

#include "libmail_config.h"

#include "namespace.H"
#include "mail.H"

LIBMAIL_START

class account;

class driver {

public:
	bool (*open_func)(mail::account *&retobj,
			  mail::account::openInfo &oi,
			  mail::callback &callback,
			  mail::callback::disconnect &disconnectCallback);

	bool (*isRemote_func)(std::string url, bool &flag);

	static driver **get_driver_list();
};

LIBMAIL_END

#endif

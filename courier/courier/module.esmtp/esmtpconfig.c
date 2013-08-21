/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"esmtpconfig.h"
#include	"comreadtime.h"

time_t config_time_esmtptimeout()
{
	return (config_readtime("esmtptimeout", 60 * 10));
}

time_t config_time_esmtpkeepalive()
{
	return (config_readtime("esmtptimeoutkeepalive", 60));
}

time_t config_time_esmtpkeepaliveping()
{
	return (config_readtime("esmtptimeoutkeepaliveping", 0));
}

time_t config_time_esmtphelo()
{
	return (config_readtime("esmtptimeouthelo", 5 * 60));
}

time_t config_time_esmtpquit()
{
	return (config_readtime("esmtptimeoutquit", 10));
}

time_t config_time_esmtpconnect()
{
	return (config_readtime("esmtptimeoutconnect", 60));
}

time_t config_time_esmtpdata()
{
	return (config_readtime("esmtptimeoutdata", 60 * 5));
}

time_t config_time_esmtpdelay()
{
	return (config_readtime("esmtpdelay", 60 * 5));
}

/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"comreadtime.h"

time_t config_readtime(const char *control, time_t deftime)
{
char	*n=config_localfilename(control);
char	*buf=config_read1l(n);
const char *p;

time_t	t, t2;

	free(n);
	if (!buf)
		return (deftime);

	p=buf;
	t=0;
	while (*p)
	{
		if (isspace((int)(char)*p))
		{
			p++;
			continue;
		}

		t2=0;
		for (; *p; p++)
		{
			if (!isdigit((int)(char)*p))	break;
			t2 = t2 * 10 + (*p-'0');
		}
		while (*p && isspace((int)(char)*p))	p++;

		switch (*p)	{
		case 'm':
		case 'M':
			t += t2 * 60;
			++p;
			continue;
		case 'h':
		case 'H':
			t += t2 * 60*60;
			++p;
			continue;
		case 'd':
		case 'D':
			t += t2 * 60*60*24;
			++p;
			continue;
		case 'w':
		case 'W':
			t += t2 * 60*60*24*7;
			++p;
			continue;
		case '\0':
			t += t2;
			break;
		case 's':
		case 'S':
			t += t2;
			++p;
			continue;
		}
		break;
	}
	free(buf);
	return (t);
}

time_t config_time_queuetime()
{
	return (config_readtime("queuetime", 7L * 24 * 60 * 60 ));
}

time_t config_time_faxqueuetime()
{
	return (config_readtime("faxqueuetime", 8 * 60 * 60 ));
}

time_t config_time_warntime()
{
	return (config_readtime("warntime", 4L * 60 * 60));
}

time_t config_time_respawnlo()
{
	return (config_readtime("respawnlo", 60 * 60));
}

time_t config_time_respawnhi()
{
	return (config_readtime("respawnhi", 7L * 24 * 60 * 60 ));
}

time_t config_time_retryalpha()
{
	return (config_readtime("retryalpha", 60 * 5));
}

time_t config_time_retrygamma()
{
	return (config_readtime("retrygamma", 60 * 15));
}

time_t config_time_submitdelay()
{
	return (config_readtime("submitdelay", 0));
}

time_t config_time_queuefill()
{
	return (config_readtime("queuefill", 5 * 60));
}

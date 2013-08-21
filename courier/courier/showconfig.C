/*
** Copyright 1998 - 2008 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"comcargs.h"
#include	"comreadtime.h"
#include	"comconfig.h"
#include	"maxlongsize.h"
#include	"module.esmtp/esmtpconfig.h"
#include	<string.h>
#include	<stdio.h>
#include	<iostream>

static const char *queuetime(), *queuefill(), *faxqueuetime(),
	*localtimeout(),
	*warntime(), *mysizelimit(), *queuelo(),
	*myesmtptimeout(), *myesmtptimeoutkeepalive(),
	*myesmtptimeoutkeepaliveping(), *myesmtptimeouthelo(),
	*myesmtptimeoutconnect(), *myesmtptimeoutdata(),
	*myesmtptimeoutquit(), *mysubmitdelay(), *mybackuprelay(),
	*queuehi(), *retryalpha(), *retrybeta(), *retrygamma(),
	*retrymaxdelta(), *mydsnlimit(), *respawnlo(), *respawnhi();

static const char *myacceptmailfor();

#define	JUMP(a,b) static const char *a() { return b(); }

JUMP(mydsnfrom, config_dsnfrom)
JUMP(mybatchsize, config_batchsize)
JUMP(mydefaultdomain, config_defaultdomain)
JUMP(myesmtpgreeting, config_esmtpgreeting)
JUMP(myesmtphelo, config_esmtphelo)
JUMP(mymsgidhost, config_msgidhost)
JUMP(myme, config_me)

static struct {
	const char *name, *descr;
	const char *(*func)();
	} configs[]={
		{"backuprelay", "relay for undeliverable messages", mybackuprelay},
		{"batchsize", "split messages if there are more recipients", mybatchsize},
		{"defaultdomain", "domain to append to addresses by default", mydefaultdomain},
		{"dsnfrom", "return address on delivery status notifications", mydsnfrom},
		{"dsnlimit", "maximum message size returned in DSN in is entirety", mydsnlimit},
		{"esmtpacceptmailfor", "accept mail for these domains via SMTP", myacceptmailfor},
		{"esmtpgreeting", "my ESMTP identification banner", myesmtpgreeting},
		{"esmtphelo", "my parameter to the HELO/EHLO SMTP verbs", myesmtphelo},
		{"esmtptimeout", "Timeout for most outbound ESMTP commands", myesmtptimeout},
		{"esmtptimeoutconnect", "Timeout for ESMTP connection attempt.", myesmtptimeoutconnect},
		{"esmtptimeoutdata", "Timeout for ESMTP data transfer.", myesmtptimeoutdata},
		{"esmtptimeouthelo", "Timeout for the response to the initial EHLO/HELO command.", myesmtptimeouthelo},
		{"esmtptimeoutkeepalive", "How long to keep outbound ESMTP connections idle, before closing", myesmtptimeoutkeepalive},
		{"esmtptimeoutkeepaliveping", "How often outbound ESMTP connections are pinged", myesmtptimeoutkeepaliveping},
		{"esmtptimeoutquit", "Timeout for the response to the QUIT command.", myesmtptimeoutquit},
		{"faxqueuetime", "how long fax messages stay in the queue", faxqueuetime},
		{"localtimeout", "watchdog timeout for local mail deliveries",
		 localtimeout},

		{"msgidhost", "hostname used in generating Message-ID: headers", mymsgidhost},
		{"me","my hostname", myme},
		{"queuelo", "Message queue cache low watermark", queuelo},
		{"queuehi", "Message queue cache high watermark", queuehi},
		{"queuefill", "Message queue refill interval", queuefill},
		{"queuetime", "how long messages stay in the queue", queuetime},
		{"respawnlo", "courierd automatic restart low watermark", respawnlo},
		{"respawnhi", "courierd automatic restart high watermark",
				respawnhi},

		{"retryalpha", "'alpha' retry interval", retryalpha},
		{"retrybeta", "'beta' retry count", retrybeta},
		{"retrygamma", "'gamma' retry interval", retrygamma},
		{"retrymaxdelta", "'maxdelta' retry exponent", retrymaxdelta},
		{"sizelimit", "maximum size of a message", mysizelimit},
		{"submitdelay", "delay before initial delivery attempt", mysubmitdelay},
		{"warntime", "how long messages stay in the queue before a warning message is sent\n"
			"     (a delayed delivery status notification)", warntime},
		{ 0, 0, 0}
		};

static const char *convtime(time_t t)
{
static char buf[80];
char	buf2[20];
const char	*sep="";
time_t	weeks, days, hours, mins, secs;

	secs= t % 60; t= t/60;
	mins= t % 60; t= t/60;
	hours= t % 24; t= t/24;
	days= t % 7; weeks= t/7;

	buf[0]=0;
	sprintf(buf2, "%u week%s", (unsigned)weeks, weeks == 1 ? "":"s");
	if (weeks)
	{
		strcpy(buf, buf2);
		sep=", ";
	}

	sprintf(buf2, "%u day%s", (unsigned)days, days == 1 ? "":"s");
	if (days)
	{
		strcat(buf, sep);
		strcat(buf, buf2);
		sep=", ";
	}

	sprintf(buf2, "%u hour%s", (unsigned)hours, hours == 1 ? "":"s");
	if (hours)
	{
		strcat(buf, sep);
		strcat(buf, buf2);
		sep=", ";
	}

	sprintf(buf2, "%u min%s", (unsigned)mins, mins == 1 ? "":"s");
	if (mins)
	{
		strcat(buf, sep);
		strcat(buf, buf2);
		sep=", ";
	}
	sprintf(buf2, "%u sec%s", (unsigned)secs, secs == 1 ? "":"s");
	if (secs)
	{
		strcat(buf, sep);
		strcat(buf, buf2);
	}
	return (buf);
}

static const char *queuefill()
{
	return (convtime(config_time_queuefill()));
}

static const char *queuetime()
{
	return (convtime(config_time_queuetime()));
}

static const char *faxqueuetime()
{
	return (convtime(config_time_faxqueuetime()));
}

static const char *localtimeout()
{
	return (convtime(config_readtime("localtimeout", 15 * 60)));
}

static const char *warntime()
{
	return (convtime(config_time_warntime()));
}

static const char *mybackuprelay()
{
char	*cfname=config_localfilename("backuprelay");
char	*conf=config_read1l(cfname);


	free(cfname);
	return (conf ? conf:"(none)");
}

static const char *showsize(unsigned long n)
{
static char buf[MAXLONGSIZE+20];

	if (n == 0)
		return ("unlimited");
	if (n < 1024 * 1024)
		sprintf(buf, "%lu Kb", (n + 512) / 1024);
	else
		sprintf(buf, "%1.1f Mb", (double)n / (1024 * 1024));
	return (buf);
}

static const char *mysizelimit()
{
	return (showsize(config_sizelimit()));
}

static const char *mydsnlimit()
{
	return (showsize(config_dsnlimit()));
}

static const char *retryalpha()
{
	return (convtime(config_time_retryalpha()));
}

static const char *respawnlo()
{
	return (convtime(config_time_respawnlo()));
}

static const char *respawnhi()
{
	return (convtime(config_time_respawnhi()));
}

static const char *retrybeta()
{
int	n=config_retrybeta();
static char	buf[MAXLONGSIZE];

	sprintf(buf, "%d", n);
	return (buf);
}

static const char *retrygamma()
{
	return (convtime(config_time_retrygamma()));
}

static const char *retrymaxdelta()
{
int	n=config_retrymaxdelta();
static char	buf[MAXLONGSIZE];

	sprintf(buf, "%d", n);
	return (buf);
}

static const char *myesmtptimeout()
{
	return (convtime(config_time_esmtptimeout()));
}

static const char *myesmtptimeoutkeepalive()
{
	return (convtime(config_time_esmtpkeepalive()));
}

static const char *myesmtptimeoutkeepaliveping()
{
	return (convtime(config_time_esmtpkeepaliveping()));
}

static const char *myesmtptimeouthelo()
{
	return (convtime(config_time_esmtphelo()));
}

static const char *myesmtptimeoutquit()
{
	return (convtime(config_time_esmtpquit()));
}

static const char *myesmtptimeoutconnect()
{
	return (convtime(config_time_esmtpconnect()));
}

static const char *myesmtptimeoutdata()
{
	return (convtime(config_time_esmtpdata()));
}

static const char *mysubmitdelay()
{
	return (convtime(config_time_submitdelay()));
}

static const char *queuelo()
{
char	*cfname=config_localfilename("queuelo");
char	*conf=config_read1l(cfname);
static char	buf[MAXLONGSIZE];

	free(cfname);
	if (conf)
	{
		sprintf(buf, "%u", atoi(conf));
		free(conf);
		return (buf);
	}
	return ("default");
}

static const char *queuehi()
{
char	*cfname=config_localfilename("queuehi");
char	*conf=config_read1l(cfname);
static char	buf[MAXLONGSIZE];

	free(cfname);
	if (conf)
	{
		sprintf(buf, "%u", atoi(conf));
		free(conf);
		return (buf);
	}
	return ("default");
}

static void showconfig()
{
int	i;

	for (i=0; configs[i].name; i++)
		std::cout << configs[i].name << ": " <<
			(*configs[i].func)() << std::endl <<
			"   - " << configs[i].descr << std::endl;
}

static const char *courierdir_arg=0;

static struct courier_args arginfo[]={
	{"dir", &courierdir_arg},
	{0}
	} ;

static const char *myacceptmailfor()
{
char *filename=config_localfilename("esmtpacceptmailfor");
char *buf;
char *p, *q;
size_t l;

	buf=readfile(filename, 0);

	free(filename);

	if (!buf)	return (config_me());

	removecomments(buf);
	l=strlen(buf)+8;
	for (p=buf; *p; p++)
		if (*p == '\n')	l += 6;
	p=(char *)courier_malloc(l);
	*p=0;
	for (q=buf; (q=strtok(q, "\n")); q=0)
		strcat(strcat(p, "\n     "), q);
	free(buf);
	return (p);
}

int cppmain(int argc, char **argv)
{
	(void)cargs(argc, argv, arginfo);

	if (courierdir_arg)
		set_courierdir(courierdir_arg);
	clog_open_stderr("showconfig");
	showconfig();
	return (0);
}

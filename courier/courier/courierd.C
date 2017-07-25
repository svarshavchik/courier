/*
** Copyright 1998 - 2013 Double Precision, Inc.
** See COPYING for distribution information.
*/
#include	"cdpendelinfo.h"
#include	"cddlvrhost.h"
#include	"cddelinfo.h"
#include	"cddrvinfo.h"
#include	"cdrcptinfo.h"
#include	"cdmsgq.h"

#include	"courier.h"
#include	"localstatedir.h"
#include	"rw.h"
#include	"comreadtime.h"
#include	"comctlfile.h"
#include	"comconfig.h"
#include	"comparseqid.h"
#include	"comqueuename.h"
#include	"comtrack.h"
#include	"courierd.h"
#include	"mydirent.h"
#include	"numlib/numlib.h"
#include	<sys/types.h>
#if HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#if HAVE_SYS_FILE_H
#include	<sys/file.h>
#endif
#if TIME_WITH_SYS_TIME
#include	<sys/time.h>
#include	<time.h>
#else
#if HAVE_SYS_TIME_H
#include	<sys/time.h>
#else
#include	<time.h>
#endif
#endif
#include	<stdio.h>
#include	<string.h>
#include	<errno.h>
#include	<signal.h>

extern time_t courierastart, courierbstart;
extern const char *triggername;

time_t flushtime, delaytime;

static int triggerw;	// Dummy fd open to trigger for writting, so we
			// block on reading.

static int triggerr;

static time_t start_time;
int shutdown_flag;

static time_t respawnlo, respawnhi;

time_t retryalpha;
int	retrybeta;
time_t retrygamma;
int	retrymaxdelta;

time_t queuefill;
time_t nextqueuefill;

//
// Courier retries up to RETRYBETA times, RETRYALPHA minutes apart.
// Then, Courier waits RETRYGAMMA*1 minutes, and makes RETRYBETA more
// delivery attempts, RETRYALPHA minutes apart.
// Then, Courier waits RETRYGAMMA*2 minutes, and makes RETRYBETA more
// delivery attempts, RETRYALPHA minutes apart.
// Then, Courier waits RETRYGAMMA*4 minutes, and makes RETRYBETA more
// delivery attempts, RETRYALPHA minutes apart.
//
// Finally, Courier will repeatedly wait RETRYGAMMA*(2^RETRYMAXDELTA) minutes,
// then make RETRYBETA delivery attempts, RETRYALPHA minutes apart.
//

// At startup, purge msgs and msgq

static void purgedir(const char *dirname)
{
	std::string	buf;
	DIR	*p;
	struct dirent *de;

	clog_msg_start_info();
	clog_msg_str("Purging ");
	clog_msg_str(dirname);
	clog_msg_send();

	p=opendir(dirname);
	if (!p)	return;
	while ((de=readdir(p)) != NULL)
	{
		if (de->d_name[0] == '.')	continue;
		buf=dirname;
		buf += '/';
		buf += de->d_name;
		rmdir(buf.c_str());		// Just for the hell of it
	}
	closedir(p);
}

// Status file format:
//
//    ssss - couriera start time
//    [space]
//    ssss - courierb start time
//    [space]
//    ssss - update time
//    [space]
//    nnnn - number of delivery attempts made
//    [space]
//    nnnn - number of messages delivered
//    [space]
//    nnnn - K delivered
//    [space]
//    nnnn - number of messages in the memory queue
//    [space]
//    nnnn - number of delivery attempts in progress

unsigned stat_nattempts;
unsigned stat_ndelivered;
unsigned long stat_nkdelivered;
unsigned stat_nkkdelivered;

//
// Check if we're low on disk space.  To avoid calling statfs every time,
// we cache the results for a couple hundred loops.
//

static int diskspacelow()
{
static int counter=0;
static int islow=0;

	if (counter == 0)
	{
	unsigned long checkblocks, nblock, checkinodes, ninodes;
	unsigned blocksize;

		// Time to recheck disk free space.  By default, remember
		// the ersults for 100 (or so) loops.
		//

		counter=100;
		if (freespace(MSGQDIR "/.", &checkblocks, &nblock, &checkinodes, &ninodes, &blocksize))
			return (islow);

		if (checkinodes && ninodes < (unsigned long)(100+msgq::queue.size()*2))
			islow=1;
		else
		{
		unsigned long n=100;
		size_t i;

		//
		// Add up how many recipients we have in memory.
		// The number of 512 byte disk blocks we require is 100 +
		// number of recipients.
		//

			for (i=0; i<msgq::queuehashfirst.size(); i++)
			{
			struct msgq *p;

				for (p=msgq::queuehashfirst[i]; p;
					p=p->nexthash)
				{
					size_t j;

					for (j=0; j<p->rcptinfo_list.size();
						j++)
						n += p->rcptinfo_list[j].
							addresses.size();
				}
			}

			n = n * 512 / blocksize;

			if (n  > nblock)
				islow=1;
			else
			{
			//
			// Figure out when to check again.
			// We'll wait a turn for every 512 byte block we're
			// above the minimum, subject to a 10 turn minimum
			// or 500 turn maximum.
			//
				n -= nblock;
				if (n > 500)	n=500;
				if (n < 10)	n=10;
				counter=n;
			}
		}
	}

	--counter;
	return (islow);
}

int courierbmain()
{
fd_set	fds, fdc;
struct	mybuf trigger_buf;

	clog_open_syslog("courierd");
	rw_init_verbose(1);
	if (rw_init_courier(0))	return (1);
	drvinfo::init();

	libmail_changeuidgid(MAILUID, MAILGID);
	signal(SIGPIPE, SIG_IGN);

	respawnlo=config_time_respawnlo();
	respawnhi=config_time_respawnhi();

	retryalpha=config_time_retryalpha();
	retrybeta=config_retrybeta();
	retrygamma=config_time_retrygamma();
	retrymaxdelta=config_retrymaxdelta();
	flushtime=0;
	delaytime=config_time_submitdelay();

	queuefill=config_time_queuefill();
	nextqueuefill=0;

	purgedir(MSGQDIR);
	purgedir(MSGSDIR);
	rmdir(MSGQDIR);
	mkdir(MSGQDIR, 0755);
	trackpurge(TRACKDIR);

	if ((triggerw=open(triggername, O_WRONLY, 0)) < 0 ||
		(triggerr=open(triggername, O_RDONLY, 0)) < 0)
		clog_msg_errno();

	time(&start_time);

	courierbstart=start_time;

	msgq::init(queuehi);

	shutdown_flag=0;
	FD_ZERO(&fds);

unsigned nd;
int	maxfd=triggerr;

	FD_SET(maxfd, &fds);
	mybuf_init(&trigger_buf, triggerr);

	for (nd=0; nd<drvinfo::nmodules; nd++)
	{
	int	fd=drvinfo::modules[nd].module_from.fd;

		if (fd > maxfd)	maxfd=fd;
		FD_SET(fd, &fds);
	}

	++maxfd;

time_t	current_time, next_time, shutdown_time;

	current_time=start_time;

	shutdown_time=0;

	msgq::queuescan();	// Initial queue scan

int	doscantmp=1;

////////////////////////////////////////////////////////////////////////////
//
// Main loop:
//
// A) Scan tmp for new messages.
//
// B) Schedule messages for delivery, when their time comes.
//
// C) Update statistics.
//
// D) Stop scheduling messages when restart signal received.
//
// E) Restart if there's no activity for 5 mins after we've been running
//    for respawnlo amount of times.
//
// F) When we've been running for respawnhi amount of time, stop scheduling
//    messages, restart when all deliveries have been completed.
//
// G) Receive delivery completion messages from modules.
//
////////////////////////////////////////////////////////////////////////////
	for (;;)
	{
	msgq	*mp;

		if (shutdown_flag)
			doscantmp=0;

		if (doscantmp)
			doscantmp=msgq::tmpscan();

		if (shutdown_flag && msgq::inprogress == 0)
			break;

		if (current_time >= start_time + respawnhi && !shutdown_flag)
		{
			clog_msg_start_info();
			clog_msg_str("SHUTDOWN: respawnhi limit reached.");
			clog_msg_send();
			shutdown_flag=1;
			continue;
		}

		if (diskspacelow() && msgq::queuehead &&
			msgq::queuehead->nextsenddel <= current_time)
		{
			clog_msg_start_err();
			clog_msg_str("Low on disk space.");
			clog_msg_send();
			for (mp=msgq::queuehead; mp &&
				mp->nextsenddel <= current_time + 300;
				mp=mp->next)
				mp->nextsenddel=current_time+300;
		}

		if (!shutdown_flag)
		{
			while ((mp=msgq::queuehead) != 0 &&
				mp->nextsenddel <= current_time)
			{
				mp->removewq();
				mp->start_message();
			}
		}

		if (nextqueuefill && nextqueuefill <= current_time)
		{
			nextqueuefill=0;
			if (!shutdown_flag)
			{
				msgq::queuescan();
			}

		}

		next_time=0;
		if ( msgq::queuehead && msgq::queuehead->nextsenddel
			> current_time)
			next_time=msgq::queuehead->nextsenddel;
		if (!shutdown_flag && msgq::inprogress == 0 &&
			shutdown_time == 0)
		{
			shutdown_time=current_time + 5 * 60;
			if (shutdown_time < start_time + respawnlo)
				shutdown_time=start_time + respawnlo;
		}

		if (shutdown_time && (next_time == 0 ||
				shutdown_time < next_time))
			next_time=shutdown_time;

		if (nextqueuefill)
		{
			/* Or, wake up next time we need to refill the queue */

			if (next_time == 0 || nextqueuefill < next_time)
			{
				next_time=nextqueuefill;
			}
		}

		fdc=fds;

	struct	timeval	tv;

		if (next_time)
		{
			tv.tv_sec= next_time > current_time ?
				next_time - current_time:0;
			tv.tv_usec=0;
		}

#if 1
		{
		char buf[40];

		clog_msg_start_info();
		clog_msg_str("Waiting.  shutdown time=");
		strcpy(buf, shutdown_time ? ctime(&shutdown_time):"none\n");
		*strchr(buf, '\n')=0;
		clog_msg_str(buf);

		clog_msg_str(", wakeup time=");
		strcpy(buf, next_time ? ctime(&next_time):"none\n");
		*strchr(buf, '\n')=0;
		clog_msg_str(buf);
		clog_msg_str(", queuedelivering=");
		clog_msg_int(msgq::queuedelivering);
		clog_msg_str(", inprogress=");
		clog_msg_int(msgq::inprogress);
		clog_msg_send();
		}
#endif

		while (select(maxfd, &fdc, (fd_set *)0, (fd_set *)0,
				(next_time && doscantmp == 0 ?
				 &tv:(struct timeval *)0)) < 0)
		{
			if (errno != EINTR)	clog_msg_errno();
		}
		time(&current_time);
		if (shutdown_time)
		{
			if (current_time >= shutdown_time &&
				msgq::inprogress == 0)
			{
				clog_msg_start_info();
				clog_msg_str("SHUTDOWN: respawnlo limit reached, system inactive.");
				clog_msg_send();
				shutdown_flag=1;
			}
			shutdown_time=0;
		}

		/* Handle commands sent via the trigger pipe */

		if (FD_ISSET(triggerr, &fdc))
		{
		unsigned	l;
		char	cmdbuf[256];
		int	c;

			do
			{
				l=0;
				do
				{
					c=mybuf_get(&trigger_buf);
					if (l < sizeof(cmdbuf)-1)
						cmdbuf[l++]=c;
				} while (c != '\n');
				cmdbuf[l]=0;

				if (strcmp(cmdbuf, TRIGGER_NEWMSG) == 0)
					doscantmp=1;
				else if (strcmp(cmdbuf, TRIGGER_RESTART) == 0)
				{
					shutdown_flag=1;
					clog_msg_start_info();
					clog_msg_str("SHUTDOWN: Restarting...");
					clog_msg_send();
				}
				else if (strcmp(cmdbuf, TRIGGER_STOP) == 0)
				{
					clog_msg_start_info();
					clog_msg_str("SHUTDOWN: Stopping...");
					clog_msg_send();
					_exit(99);
				}
				else if (strcmp(cmdbuf, TRIGGER_FLUSH) == 0)
				{
					flushtime=current_time;
					for (mp=msgq::queuehead; mp;
							mp=mp->next)
						mp->nextsenddel=current_time;
				}
				else if (strncmp(cmdbuf, TRIGGER_FLUSHMSG,
					sizeof(TRIGGER_FLUSHMSG)-1) == 0)
				{
				char *p;
				ino_t	n;

					if ((p=strchr(cmdbuf, '\n')) != 0)
						*p=0;

					p=cmdbuf+sizeof(TRIGGER_FLUSHMSG)-1;
					while (*p == ' ')	++p;
					if (comparseqid(p, &n) == 0 &&
						!shutdown_flag)
						msgq::flushmsg(n, p);
				}
			} while (mybuf_more(&trigger_buf));
		}

		for (nd=0; nd<drvinfo::nmodules; nd++)
		{
		struct mybuf *p= &drvinfo::modules[nd].module_from;
		size_t	delnum;
		int	c;

			if (!FD_ISSET(p->fd, &fdc))	continue;
			do
			{
				delnum=0;
				while (errno=0, ((c=mybuf_get(p)) != '\n'))
				{
					if (c < 0)
					{
						if (errno == EINTR)
							continue;

						clog_msg_start_err();
						clog_msg_str("MODULE ");
						clog_msg_str(
							drvinfo::modules[nd]
								.module->name);
						clog_msg_str(" ABORTED.");
						clog_msg_send();
						_exit(1);
					}
					if (c >= '0' && c <= '9')
						delnum=delnum * 10 + (c-'0');
				}
				msgq::completed(drvinfo::modules[nd], delnum);
			} while (mybuf_more(p));
			if (!shutdown_flag)	shutdown_time=0;
		}
	}

	close(triggerr);
	close(triggerw);

	drvinfo::cleanup();
	return (0);
}

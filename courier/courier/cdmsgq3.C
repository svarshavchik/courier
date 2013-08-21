/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"cdpendelinfo.h"
#include	"cddlvrhost.h"
#include	"cddelinfo.h"
#include	"cddrvinfo.h"
#include	"cdrcptinfo.h"
#include	"cdmsgq.h"

#include	"courier.h"
#include	"courierd.h"
#include	"rw.h"
#include	"comqueuename.h"
#include	"comctlfile.h"
#include	"comstrinode.h"
#include	"comstrtimestamp.h"
#include	"comstrtotime.h"

#include	<sys/types.h>
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<sys/uio.h>
#include	<time.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif

extern time_t	retryalpha;
extern int	retrybeta;
extern time_t	retrygamma;
extern int	retrymaxdelta;
extern int	shutdown_flag;

extern unsigned stat_nattempts;
extern unsigned stat_ndelivered;
extern unsigned long stat_nkdelivered;
extern unsigned stat_nkkdelivered;

void msgq::logmsgid(msgq *q)
{
	clog_msg_str(q->msgid.c_str());
}

void msgq::startdelivery(drvinfo *drvp, delinfo *delp)
{
rcptinfo *ri=delp->rcptlist;
int i;

	++inprogress;
	++stat_nattempts;

	/* If the message is due for cancellation, cancel it here */

	if (strcmp(drvp->module->name, drvinfo::module_dsn->module->name) &&
		ri->msg->cancelled)
	{
	struct	ctlfile	ctf;

		if (ctlfile_openi(ri->msg->msgnum, &ctf, 0) == 0)
		{
			const char *msg;

			for (i=0; i<(int)ri->addresses.size(); i++)
			{
			int	j;

				msg=0;
				for (j=0; ctf.lines[j]; j++)
				{
					if (ctf.lines[j][0]
						!= COMCTLFILE_CANCEL_MSG)
						continue;

					if (msg)
						ctlfile_append_connectioninfo(
							&ctf,
							ri->addressesidx[i],
						COMCTLFILE_DELINFO_REPLY, msg);
			
					msg=ctf.lines[j]+1;
				}

				if (!msg)	msg="Cancelled by system.";
				ctlfile_append_reply(&ctf,
					ri->addressesidx[i], msg,
					COMCTLFILE_DELFAIL, 0);
			}
			ctlfile_close(&ctf);
		}

		completed(*drvp, delp->delid);
		return;
	}

	for (i=0; i<(int)ri->addresses.size(); i++)
	{
		clog_msg_start_info();
		clog_msg_str("started,id=");
		logmsgid(delp->rcptlist->msg);
		clog_msg_str(",from=<");
		clog_msg_str(delp->rcptlist->envsender.c_str());
		clog_msg_str(">,module=");
		clog_msg_str(delp->rcptlist->delmodule->module->name);
		clog_msg_str(",host=");
		clog_msg_str(delp->dlvrphost->hostname.c_str());
		clog_msg_str(",addr=<");
		clog_msg_str(ri->addresses[i].c_str());
		clog_msg_str(">");
		clog_msg_send();
	}
#if 0
clog_msg_start_info();
clog_msg_str("Host ");
clog_msg_str(delp->dlvrphost->hostname);
clog_msg_str(", # pending deliveries=");
clog_msg_uint(delp->dlvrphost->dlvrcount);
clog_msg_send();
#endif

	fprintf(drvp->module_to, "%s\t%s\t",
		strinode(delp->rcptlist->msg->msgnum),
		delp->rcptlist->msg->msgid.c_str());
	fprintf(drvp->module_to, "%s\t",
		ri->envsender.c_str());
	fprintf(drvp->module_to, "%u\t%s",
		delp->delid, delp->dlvrphost->hostname.c_str());
	for (i=0; i<(int)ri->addresses.size(); i++)
	{
		fprintf(drvp->module_to, "\t%u\t%s",
			(unsigned)ri->addressesidx[i],
			ri->addresses[i].c_str());
	}
	putc('\n', drvp->module_to);
	fflush(drvp->module_to);
}

//////////////////////////////////////////////////////////////////////////
//
// We've been told that the following delivery attempt has been completed
//
//////////////////////////////////////////////////////////////////////////

void msgq::completed(drvinfo &drvp, size_t delnum)
{
delinfo *di;

	--inprogress;
	if (delnum >= drvp.delinfo_list.size() ||
		drvp.delinfo_list[delnum].dlvrphost == 0 ||
		drvp.delinfo_list[delnum].rcptlist == 0)
	{
		clog_msg_start_err();
		clog_msg_str("INVALID DELIVERY ID");
		clog_msg_send();
		exit(1);
		return;
	}

	// First thing to do is to grab the relevant info, then recycle the
	// delinfo structure

	di=&drvp.delinfo_list[delnum];

dlvrhost *hostp=di->dlvrphost;
rcptinfo *rcptlist=di->rcptlist;

	di->dlvrphost=0;
	di->rcptlist=0;
	di->freenext=drvp.delpfreefirst;
	drvp.delpfreefirst=di;

	--hostp->dlvrcount;	// ... to this host

#if 0
clog_msg_start_info();
clog_msg_str("Host ");
clog_msg_str(hostp->hostname);
clog_msg_str(", # pending deliveries=");
clog_msg_uint(hostp->dlvrcount);
clog_msg_send();
#endif

	// Move the structure for the host of this delivery attempt up to
	// the start of the MRU list.

	if (hostp->prev)	hostp->prev->next=hostp->next;
	else drvp.hdlvrpfirst=hostp->next;
	if (hostp->next)	hostp->next->prev=hostp->prev;
	else drvp.hdlvrplast=hostp->prev;

	hostp->prev=0;
	if ((hostp->next=drvp.hdlvrpfirst) != 0)
		hostp->next->prev=hostp;
	else
		drvp.hdlvrplast=hostp;
	drvp.hdlvrpfirst=hostp;

	// Now, in order to avoid starvation, move the LAST recently used
	// host up to the plate.

	hostp=drvp.hdlvrplast;

	if (hostp->prev)	hostp->prev->next=hostp->next;
	else drvp.hdlvrpfirst=hostp->next;
	/* if (hostp->next)	hostp->next->prev=hostp->prev;
	else */ drvp.hdlvrplast=hostp->prev;

	hostp->prev=0;
	if ((hostp->next=drvp.hdlvrpfirst) != 0)
		hostp->next->prev=hostp;
	else
		drvp.hdlvrplast=hostp;
	drvp.hdlvrpfirst=hostp;







	// See if there are any hosts where recent delivery attempts
	// were made which can now accept additional delivery attempts
	// and have pending deliveries waiting.

	for (hostp=drvp.hdlvrpfirst; hostp && !shutdown_flag; hostp=hostp->next)
	{
		if (hostp->dlvrcount >= drvp.maxhost)	continue;
		if (hostp->pending_list == 0)	continue;

	delinfo *newdi=drvp.delpfreefirst;

		if (newdi == 0)
			break;	// Cancel the show - no free delivery slots

		// Pop off the first pending delivery into this delivery
		// slot.

		drvp.delpfreefirst=newdi->freenext;
		newdi->dlvrphost=hostp;
		newdi->rcptlist=hostp->pending_list->receipient_list.front();
		hostp->pending_list->receipient_list.pop_front();

		if (hostp->pending_list->receipient_list.empty())
		{
			// No more pending deliveries to this host, drop
			// the pending list.
			drvp.pendelinfo_list.erase(
				hostp->pending_list->pos);
			hostp->pending_list=0;
		}

		newdi->rcptlist->pending=0;	// This delivery's in progress
		++hostp->dlvrcount;
		startdelivery(&drvp, newdi);	// And away we go.
	}

	// Allright, if we have pending deliveries NOT to any recently
	// used host, but free delivery slots, recycle a host structure if
	// one is available.

	std::list<pendelinfo>::iterator pos;

	for (pos=drvp.pendelinfo_list.begin();
	     pos != drvp.pendelinfo_list.end() && !shutdown_flag; )
	{
		pendelinfo &pi= *pos++;

		if (pi.hostp)	continue;	// We would've gotten it above

	delinfo *newdi=drvp.delpfreefirst;

		if (newdi == 0)
			break;	// Stop - no available delivery slots

		// Appropriate a hostp structure, and move it to the top
		// of the MRU list.

		if (drvp.hdlvrpfree)
		{
			// We have an unused one

			hostp=drvp.hdlvrpfree;
			drvp.hdlvrpfree=hostp->next;

			hostp->hostname=pi.hostname;
			hostp->dlvrcount=0;
		}
		else
		{
			for (hostp=drvp.hdlvrplast; hostp; hostp=hostp->prev)
				if (hostp->dlvrcount == 0 &&
					hostp->pending_list == 0)
					break;
			if (!hostp)	break;

			hostp->hostname=pi.hostname;
			if (hostp->next)	hostp->next->prev=hostp->prev;
			else	drvp.hdlvrplast=hostp->prev;

			if (hostp->prev)	hostp->prev->next=hostp->next;
			else	drvp.hdlvrpfirst=hostp->next;
		}


		if ((hostp->next=drvp.hdlvrpfirst) != 0)
			hostp->next->prev=hostp;
		else
			drvp.hdlvrplast=hostp;
		hostp->prev=0;
		drvp.hdlvrpfirst=hostp;

		// The first thing we need to do is to officially marry
		// the hostp structure with its pending_list.

		hostp->pending_list=&pi;
		pi.hostp=hostp;

		// Backpedal and finish up initializing the delinfo structure

		drvp.delpfreefirst=newdi->freenext;
		newdi->dlvrphost=hostp;
		newdi->rcptlist=pi.receipient_list.front();
		pi.receipient_list.pop_front();
			// Pop off the next scheduled delivery to this host

		if (pi.receipient_list.empty())
		{
			// Last one - popoff the delinfo structure

			drvp.pendelinfo_list.erase( pi.pos );
			hostp->pending_list=0;
		}

		newdi->rcptlist->pending=0;
		++hostp->dlvrcount;
		startdelivery(&drvp, newdi);
	}

	// If this was the last batch of recipients of this message,
	// see what we need to do with the message now.

	if ( --rcptlist->msg->rcptcount == 0)
		done(rcptlist->msg, 1);
}

void msgq::done(msgq *p, int truecompletion)
{
struct ctlfile	ctlinfo;
unsigned failcnt, successcnt, completedcnt, i;
time_t	current_time;
struct	iovec iov[3];
static char completed[1]={COMCTLFILE_DELCOMPLETE};
const char *t;
int	j;
int	isdsncompletion;

	clog_msg_start_info();
	clog_msg_str("completed,id=");
	logmsgid(p);
	clog_msg_send();

	isdsncompletion=p->rcptinfo_list.size() == 1 &&
		strcmp(p->rcptinfo_list[0].delmodule->module->name, DSN) == 0;

	errno=0;
	if (ctlfile_openit(p->msgnum, p->nextdel, &ctlinfo, 0))
	{
		if (errno)
			clog_msg_prerrno();
		p->removeq();
		if (queuedelivering < queuelo)	queuescan();
		return;
	}
	time(&current_time);

	if (truecompletion && !isdsncompletion)
	{
		iov[0].iov_base=(caddr_t)&completed;
		iov[0].iov_len=sizeof(completed);

		t=strtimestamp(current_time);
		iov[1].iov_base=(caddr_t)t;
		iov[1].iov_len=strlen(t);
		iov[2].iov_base=(caddr_t)"\n";
		iov[2].iov_len=1;
		ctlfile_appendv(&ctlinfo, iov, 3);
	}

	failcnt=0;
	successcnt=0;

	/*
	** Although we can add up COMCTLFILE_DELSUCCESS and COMCTLFILE_DELFAIL
	** records, this strategy is vulnerable to various types of corruption,
	** so let's handle this the right way.
	*/

	{
		std::vector<char> completed_array;

		completed_array.resize( ctlinfo.nreceipients );
		for (i=0; i < ctlinfo.nreceipients; i++)
			completed_array[i]=0;

		for (i=0; ctlinfo.lines[i]; i++)
		{
		unsigned n;

			switch (ctlinfo.lines[i][0])	{
			case COMCTLFILE_DELSUCCESS:
			case COMCTLFILE_DELFAIL:
				n=atoi(ctlinfo.lines[i]+1);
				if (n < (unsigned)ctlinfo.nreceipients)
					completed_array[n]=ctlinfo.lines[i][0];
			}
		}
		for (i=0; i < ctlinfo.nreceipients; i++)
			switch (completed_array[i])	{
			case COMCTLFILE_DELSUCCESS:
				++successcnt;
				break;
			case COMCTLFILE_DELFAIL:
				++failcnt;
				break;
			}
	}

	completedcnt=1;	// Count this one too

	for (i=0; ctlinfo.lines[i]; i++)
		if (ctlinfo.lines[i][0] == COMCTLFILE_DELCOMPLETE)
			++completedcnt;

	std::string	buf;

	buf=qmsgqname(p->msgnum, p->nextdel);

	j=ctlfile_searchfirst(&ctlinfo, COMCTLFILE_EXPIRES);

int	reschedule=0;
int	cancelled=0;

	if (isdsncompletion)
	{
		/* This message just came back from the DSN module */

		if (p->rcptinfo_list[0].delhost == "deferred")
		{
			p->removeq();
			reschedule=1;
		}
		else
		{
			p->removeq();
			if (access(qmsgsdatname(p->msgnum), 0) == 0)
				reschedule=1;
		}
	}
	else if (successcnt + failcnt == ctlinfo.nreceipients || j < 0
		|| strtotime(ctlinfo.lines[j]+1) <= current_time)
	{
	const char *sender;

		if ((sender=p->needs_dsn(&ctlinfo)) != 0)
		{
//
// Schedule the message to be delivered to the DSN module, which will
// remove the message from the queue, when done.
//
			p->rcptinfo_list.resize(1);
			p->rcptcount=1;
			p->rcptinfo_list[0].
				init_1rcpt(p, drvinfo::module_dsn, "", "",
					sender);
//
// Can't set !!!nextdel!!!, because that's used to locate the hard link
// in msgq.
//
			p->start_message();
		}
		else
		{
			p->removeq();
			qmsgunlink(p->msgnum, current_time);
			unlink(buf.c_str());

			++stat_ndelivered;
			stat_nkdelivered += p->nksize / 1024;
			stat_nkkdelivered += p->nksize % 1024;
			if (stat_nkkdelivered >= 1024)
			{
				stat_nkkdelivered -= 1024;
				++stat_nkdelivered;
			}

			stat_nkdelivered += p->nksize / 1024;
		}
	}
	else if ( (j=ctlfile_searchfirst(&ctlinfo, COMCTLFILE_WARNINGSENT)) < 0
		&& (j=ctlfile_searchfirst(&ctlinfo, COMCTLFILE_WARNING)) >= 0
		&& strtotime(ctlinfo.lines[j]+1) <= current_time)
	{
	const char *sender;

		if (backup_relay_driver)
		{
		static const char mark[]={COMCTLFILE_WARNINGSENT, '\n', 0};

			ctlfile_append(&ctlinfo, mark);
			p->removeq();
			reschedule=1;
			cancelled=1;	/* Hack - forces an immediate resend */
		}
		else if ((sender=p->needs_warndsn(&ctlinfo)) != 0)
		{
//
// Schedule the message to be delivered to the DSN module, which will
// generate the warning message.
//
			p->rcptinfo_list.resize(1);
			p->rcptcount=1;
			p->rcptinfo_list[0].
				init_1rcpt(p, drvinfo::module_dsn, "deferred",
					"", sender);
//
// Can't set !!!nextdel!!!, because that's used to locate the hard link
// in msgq.
//
			p->start_message();
		}
		else
		{
			p->removeq();
			reschedule=1;
		}
	}
	else
	{
		if (ctlinfo.cancelled)	cancelled=1;
		p->removeq();
		reschedule=1;
		/* Message was cancelled while being delivered */
	}

	if (reschedule)
	{
		/* If message was cancelled, schedule it immediately, else
		** bump it up by a retry interval.  NOTE - first immediate
		** retry is marked, so we don't get stuck in an infinite loop
		** if there's a soft error in the delivery module.
		*/

		if (cancelled && ctlfile_searchfirst(&ctlinfo,
					COMCTLFILE_CANCELLED_ACK) < 0)
		{
		static const char mark[]={COMCTLFILE_CANCELLED_ACK, '\n', 0};

			ctlfile_append(&ctlinfo, mark);
		}
		else if (completedcnt % retrybeta)
			current_time += retryalpha;
		else
		{
			completedcnt /= retrybeta;
			if ((int)completedcnt > retrymaxdelta)
				completedcnt=retrymaxdelta;

			current_time += retrygamma * (1L << completedcnt);
		}
	
		std::string buf2;

		ctlfile_nextattempt(&ctlinfo, current_time);
		buf2=qmsgqname(p->msgnum, current_time);

		// There is a possibility that buf=buf2: courierd gets a flush
		// signal, tries to deliver this message, and it is deferred
		// again, and it happens to be for the same time that it
		// was deferred to again (keep in mind that the delivery
		// mechanism does NOT always go to longer and longer deferrals).

		if ( buf != buf2 && rename(buf.c_str(), buf2.c_str()))
		{
			mkdir (qmsgqdir(current_time), 0755);
			if (rename(buf.c_str(), buf2.c_str()))
				clog_msg_errno();
		}

		if (!shutdown_flag)
		{
			size_t i=buf2.rfind('/');

			queuescan3(buf2.substr(0, i), buf2.substr(i+1), 0);
		}
	}

	ctlfile_close(&ctlinfo);
	if (queuedelivering < queuelo && !shutdown_flag)	queuescan();
}

//
// Determine if after a message has been delivered, we need to generate
// a DSN.
//

const char *msgq::needs_dsn(struct ctlfile *ctlinfo)
{
const char *p;
unsigned j;

	for (j=0; j<ctlinfo->nreceipients; j++)
		if ((p=dsn_sender(ctlinfo, j, 0)) != 0)	return (p);
	return (0);
}

//
// Determine if the message needs a delayed DSN.
//

const char *msgq::needs_warndsn(struct ctlfile *ctlinfo)
{
const char *p;
unsigned	j;

	for (j=0; j<ctlinfo->nreceipients; j++)
		if ((p=dsn_sender(ctlinfo, j, 1)) != 0)	return (p);
	return (0);
}

/*****************************************************************************

  Attempt to flush a single message

*****************************************************************************/

void msgq::flushmsg(ino_t n, const char *msgid)
{
msgq	*mp;
struct	ctlfile ctf;

	for (mp=msgq::queuehead; mp; mp=mp->next)
		if (mp->msgnum == n)
			break;

	if (mp)
	{
		if (mp->msgid == msgid)
		{
			if (ctlfile_openi(n, &ctf, 0) == 0)
			{
				if (ctf.cancelled)
					mp->cancelled=1;
				ctlfile_close(&ctf);
			}
			mp->removewq();
			mp->start_message();
		}
		return;
	}

	if ((mp=msgq::findq(n)) != 0)
	{
		/* In the buffer, being delivered, I suppose */

		if (mp->msgid == msgid)
		{
			if (ctlfile_openi(n, &ctf, 0) == 0)
			{
				if (ctf.cancelled)
					mp->cancelled=1;
				ctlfile_close(&ctf);
			}
		}
		return;
	}

	if (ctlfile_openi(n, &ctf, 1) == 0)
	{
	int	l=ctlfile_searchfirst(&ctf, COMCTLFILE_MSGID);
	time_t	t=ctlfile_getnextattempt(&ctf);

		if (l >= 0 && strcmp(ctf.lines[l]+1, msgid) == 0 && t)
		{
			std::string s;
			int	i;

			s=qmsgqname(n, t);
			i=s.rfind('/');
			queuescan3(s.substr(0, i), s.substr(i+1),
				   (const char *)0);
		}
		ctlfile_close(&ctf);
	}
}

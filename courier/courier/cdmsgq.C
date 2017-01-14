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

#include	"mydirent.h"
#include	"maxlongsize.h"
#include	"comqueuename.h"
#include	"comctlfile.h"
#include	"comstrinode.h"
#include	"comstrtimestamp.h"
#include	"comstrtotime.h"
#include	"courier.h"
#include	"localstatedir.h"
#include	"rw.h"
#include	"rfc822/rfc822.h"
#include	<ctype.h>
#include	<errno.h>
#include	<stdlib.h>
#include	<string.h>
#if HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<time.h>
#include	<utime.h>
#include	"numlib/numlib.h"

#include	<vector>
#include	<list>
#include	<algorithm>
#include	<functional>

std::vector<msgq> msgq::queue;
std::vector<msgq *> msgq::queuehashfirst, msgq::queuehashlast;

msgq *msgq::queuehead, *msgq::queuetail, *msgq::queuefree;
unsigned msgq::queuedelivering, msgq::queuewaiting, msgq::inprogress;
drvinfo *msgq::backup_relay_driver;
std::string msgq::backup_relay;

extern time_t flushtime;
extern time_t delaytime;

extern time_t queuefill;
extern time_t nextqueuefill;

msgq::msgq() : cancelled(0), next(0), prev(0), nexthash(0) {}
msgq::~msgq()	{}

void msgq::init(unsigned qs)
{
	queue.resize(qs);
	queuehashfirst.resize(qs);
	queuehashlast.resize(qs);

	queuehead=0;
	queuetail=0;
	queuefree=0;
	queuedelivering=0;
	queuewaiting=0;
	inprogress=0;
	backup_relay_driver=0;

char *filename=config_localfilename("backuprelay");
char *buf=config_read1l(filename);

	free(filename);
	if (buf)
	{
		const char *h=strtok(buf, " \t\r\n");

		if (h)
		{
			backup_relay=h;
			h=strtok(0, " \t\r\n");
			if (!h)	h="esmtp";

		struct rw_transport *t=rw_search_transport(h);

			if (!t)
			{
				clog_msg_start_err();
				clog_msg_str("BACKUP RELAY DRIVER NOT FOUND: ");
				clog_msg_str(h);
				clog_msg_send();
				exit(1);
			}
			backup_relay_driver=(drvinfo *)t->udata;
		}
	}

unsigned	i;

	for (i=0; i<qs; i++)
	{
		queue[i].next=queuefree;
		queuefree=&queue[i];
		queuehashfirst[i]=0;
		queuehashlast[i]=0;
	}
}

msgq *msgq::findq(ino_t inum)
{
msgq	*qp=queuehashfirst[ inum % queuehashfirst.size() ];

	while (qp && qp->msgnum != inum)
		qp=qp->nexthash;
	return (qp);
}

static time_t	current_time;

/////////////////////////////////////////////////////////////////////////
//
// Scan TMPDIR for new messages to add to the queue.  When we find a
// message that's ready to be submitted, put it into msgs/msgq.
//
/////////////////////////////////////////////////////////////////////////

int msgq::tmpscan()
{
	int	found=0;

	time(&current_time);

	DIR *tmpdir=opendir(TMPDIR);

	if (!tmpdir)	clog_msg_errno();

	struct dirent *de;
	std::string	subdir;
	std::string	ctlfile, datfile, newctlfile, newdatfile;
	struct	stat stat_buf;
	std::string	qdir, qname;

	while ((de=readdir(tmpdir)) != 0)
	{
	const char *p;

		for (p=de->d_name; *p; p++)
			if (!isdigit((int)(unsigned char)*p))	break;
		if (*p)	continue;	// Subdirs must be named all digits

		subdir=TMPDIR "/";
		subdir += de->d_name;

		DIR *subde=opendir(subdir.c_str());

		while ((de=readdir(subde)) != 0 && found < 100)
		{
			if (strcmp(de->d_name, ".") == 0 ||
				strcmp(de->d_name, "..") == 0)	continue;

			ctlfile=subdir;
			ctlfile += '/';
			ctlfile += de->d_name;

			int	i=-1;

			if (stat(ctlfile.c_str(), &stat_buf))	continue;

			size_t j=ctlfile.size();

			while (j)
			{
				if (ctlfile[--j] == '/')
					break;

				if (ctlfile[j] == '.')
				{
					i=j;
					break;
				}
			}

		ino_t	inum=stat_buf.st_ino;

		// Cleanup timeout. Should always be sufficiently longer than
		// the submit timeout set in submit.C via alarm_timeout.

			if (stat_buf.st_mtime < current_time - 48 * 60 * 60)
			{
				if (de->d_name[0] == 'D')
				{
					datfile=subdir + "/C" +
						(de->d_name+1);

					if (stat(datfile.c_str(), &stat_buf)
					    == 0)
						continue;
						// Wait until the C file is
						// purged

				// Be carefull with Cnnnn.x files, because
				// Cnnnn.1 is used to hold submission of all
				// of them.  A race condition can leave
				// Cnnnn.1 in a submittable state, so if it
				// is removed, other copies of this message
				// can become submittable, during a very small
				// window.

				} else if (de->d_name[0] == 'C' && i >= 0)
				{

				// This race condition is handled simply
				// by not removing Cxxxxx.n if Cxxxxx.n+1
				// exists.

					char	nbuf[NUMBUFSIZE];

					datfile=ctlfile.substr(0, i);

					size_t	n=atoi( ctlfile.c_str()+i+1);

					++n;

					datfile += '.';
					datfile += libmail_str_size_t(n, nbuf);

					if (stat(datfile.c_str(), &stat_buf)
					    == 0)
						continue;
						// Wait until the C.1 file is
						// purged

					unlink(ctlfile.c_str());

					ctlfile=subdir + "/D" +
						(de->d_name+1);
					/* FALLTHRU */
				}
				unlink(ctlfile.c_str());
				continue;
			}

			if (de->d_name[0] != 'C')	continue;
			if (i >= 0)
			{
				datfile=ctlfile.substr(0, i);
				datfile += ".1";

				if (ctlfile == datfile ||
				    stat(datfile.c_str(), &stat_buf) == 0)
					continue;
			}


			datfile=subdir + "/D" + (de->d_name+1);

			newdatfile=qmsgsdatname(inum);
			newctlfile=qmsgsctlname(inum);

			struct	ctlfile ctf;

			if ((access(datfile.c_str(), 0) == 0 &&
			     access(newdatfile.c_str(), 0) == 0) ||
			    access(newctlfile.c_str(), 0) == 0 ||
			    ctlfile_openfn(ctlfile.c_str(), &ctf, 0, 0))
			{
				clog_msg_start_err();
				clog_msg_str("QUEUE FILE CORRUPTION: inode ");
				clog_msg_uint(inum);
				clog_msg_send();
				utime(ctlfile.c_str(), 0);
				// Keep it around
				continue;
			}

		time_t	nextattempt=current_time+delaytime;

			if ((i=ctlfile_searchfirst(&ctf,
				COMCTLFILE_SUBMITDELAY)) >= 0)
				nextattempt=current_time+
					atoi(ctf.lines[i]+1);

			qname=qmsgqname(inum, nextattempt);

			ctlfile_nextattempt(&ctf, nextattempt);

			if (link(ctlfile.c_str(), qname.c_str()))
			{
				mkdir(qmsgqdir(current_time),0755);
				if (link(ctlfile.c_str(), qname.c_str())
				    && errno != EEXIST)
					clog_msg_errno();
			}

			if (rename(datfile.c_str(), newdatfile.c_str()))
			{
				mkdir(qmsgsdir(inum), 0755);
					// We may need to create this dir
				rename(datfile.c_str(), newdatfile.c_str());
			}

			if (rename(ctlfile.c_str(), newctlfile.c_str()))
				clog_msg_errno();

			for (i=qname.size(); i; )
			{
				if (qname[--i] == '/')
					break;
			}

			qdir=qname.substr(0, i);
			qname=qname.substr(i+1);
			++found;
			ctlfile_close(&ctf);

			queuescan3(qdir, qname, de->d_name);
		}
		closedir(subde);

		if (stat(subdir.c_str(), &stat_buf) == 0 &&
			stat_buf.st_mtime < current_time - 2 * 60 * 60)
			rmdir(subdir.c_str());	// Just give it a try
	}
	closedir(tmpdir);
	return (found);
}

////////////////////////////////////////////////////////////////////////////
//
// Add more messages from MSGQDIR to msgq::queue
//
////////////////////////////////////////////////////////////////////////////

static ino_t strtoino(const char *p)
{
ino_t	n=0;

	while (isdigit((int)(unsigned char)*p))
	{
		n *= 10;
		n += *p++ - '0';
	}
	return (n);
}

void msgq::queuescan()
{
static	int queuescan_flag=0;

	if (queuescan_flag)	return;
		// Recursive invocation if message just pulled into queue
		// has been delivered to all of its recipients.

	nextqueuefill=0;

#if 0
clog_msg_start_info();
clog_msg_str("queue scan");
clog_msg_send();
#endif

	try
	{
		std::list<std::string> subdirlist;
		DIR *tmpdir=opendir(MSGQDIR);
		struct dirent *de;
		std::string	s;

		queuescan_flag=1;
		if (!tmpdir)	clog_msg_errno();

		time(&current_time);

		while ((de=readdir(tmpdir)) != 0)
		{
			const char *p;

			for (p=de->d_name; *p; p++)
				if (!isdigit((int)(unsigned char)*p))	break;

			if (*p)	continue;	// Subdirs must be named all digits

			p=de->d_name;
			time_t n=strtotime(p);

			std::list<std::string>::iterator sb, se;

			sb=subdirlist.begin();
			se=subdirlist.end();

			while (sb != se)
			{
				if (strtotime(sb->c_str()) > n)
					break;
				++sb;
			}

			subdirlist.insert(sb, de->d_name);
		}
		closedir(tmpdir);

		while (!subdirlist.empty())
		{
			s=MSGQDIR "/";
			s += subdirlist.front();
			subdirlist.pop_front();

			if (queuescan2(s) <= 0)	break;
				// Stop if we can't add any more msgs
		}
		queuescan_flag=0;
	}
	catch (...)
	{
		queuescan_flag=0;
		throw;
	}
}

static bool sort_by_qtime(std::string a, std::string b)
{
	const char *pa=strchr(a.c_str(), '.')+1;
	const char *pb=strchr(b.c_str(), '.')+1;
	time_t ta=strtotime(pa);
	time_t tb=strtotime(pb);

	return ta < tb;
}

///////////////////////////////////////////////////////////////////////////
//
// Read all control files in this subdirectory, sort them by scheduled
// delivery time, attempt to add them to the queue.
//
// Returns: -1 - error, 0 - no msgs were added, >0 - msgs were added
//
///////////////////////////////////////////////////////////////////////////

int msgq::queuescan2(std::string s)
{
	DIR *tmpdir=opendir(s.c_str());
	struct dirent *de;
	std::list<std::string>	filename_list;

	if (!tmpdir)	clog_msg_errno();

	while ((de=readdir(tmpdir)) != 0)
	{
	const char *p=de->d_name;

		if (*p++ != 'C')	continue;

	ino_t	inum=strtoino(p);

		// Perhaps this one's in the queue already

		if (findq(inum))	continue;	// Already in msgq

		while (isdigit(*p))	p++;
		if (*p++ != '.')	continue;

		while (isdigit(*p))
			++p;

		if (*p)	continue;

		filename_list.push_back(de->d_name);
	}
	closedir(tmpdir);

	if (filename_list.size() == 0)
		return (1);	// Pretend we added something, so keep going

	std::vector<std::string> filename_array;

	filename_array.reserve(filename_list.size());

	filename_array.insert(filename_array.end(),
			      filename_list.begin(), filename_list.end());

	int	rc;
	int	flag=0;

	std::sort(filename_array.begin(), filename_array.end(),
		  std::ptr_fun(sort_by_qtime));


	std::vector<std::string>::iterator b, e;

	b=filename_array.begin();
	e=filename_array.end();

	while (b != e)
	{
		rc=queuescan3(s, *b++, 0);

		if (rc < 0)
			return (-1);
		if (rc)	break;
		flag=1;
	}
	return (flag);
}

void msgq::removewq()
{
	--queuewaiting;
	if (prev)	prev->next=next;
	else	queuehead=next;
	if (next)	next->prev=prev;
	else	queuetail=prev;
}

//////////////////////////////////////////////////////////////////////////////
//
// Deallocate a msgq structure.  This can happen after all deliveries have
// been complete, or if we want to make room in a full queue for a message
// with an earlier scheduled delivery time.  Therefore, we examine
// the message's pending pointer, and deallocate them if they are set.
// Serious corruption will result if the message has any deliveries in
// progress.
//
//////////////////////////////////////////////////////////////////////////////

void msgq::removeq()
{
	size_t i;

	for (i=0; i<rcptinfo_list.size(); i++)
	{
		rcptinfo &ri=rcptinfo_list[i];

		pendelinfo *pi=ri.pending;

		if (!pi)	continue;

		pi->receipient_list.erase(ri.pendingpos);
		if (!pi->receipient_list.empty()) continue;
		if (pi->hostp)
			pi->hostp->pending_list=0;
		pi->drvp->pendelinfo_list.erase(pi->pos);
	}

	ino_t	hashbucket=msgnum % queuehashfirst.size();

	if (prevhash) prevhash->nexthash=nexthash;
	else queuehashfirst[hashbucket]=nexthash;
	if (nexthash) nexthash->prevhash=prevhash;
	else queuehashlast[hashbucket]=prevhash;
	next=queuefree;
	queuefree=this;
	--queuedelivering;
}

//////////////////////////////////////////////////////////////////////////////
//
// Attempt to add this control file to the queue.  We've already verified that
// this control file is not already in the queue.
//
//////////////////////////////////////////////////////////////////////////////

int msgq::queuescan3(std::string subdir, std::string name,
		     const char *isnewmsg)
{
	struct ctlfile	ctlinfo;
	ino_t	inum;
	time_t	deltime, delsendtime;
	const char *p=name.c_str();
	struct	stat	stat_buf;

	++p;
	inum=strtoino(p);
	while (isdigit(*p))	p++;
	++p;
	deltime=strtotime(p);
	name= subdir + '/' + name;
	if (ctlfile_openfn(name.c_str(), &ctlinfo, 0, 1))
	{
		if (errno)
		{
			clog_msg_start_err();
			clog_msg_str("Cannot read ");
			clog_msg_str(name.c_str());
			clog_msg_send();
			clog_msg_prerrno();
		}
		return (0);
	}
	delsendtime=deltime;
	if (flushtime && ctlinfo.mtime < flushtime && flushtime < deltime)
		delsendtime=flushtime;

	if (!queuefree)
	{
	msgq	*p;

//
// msgq array is full.  See if we can remove the last message from the
// pending queue.

		p=queuetail;
		if (p && p->nextsenddel > delsendtime)
		{

#if 0
clog_msg_start_info();
clog_msg_str("Removing ");
clog_msg_uint(p->msgnum);
clog_msg_str(" to make room for this message.");
clog_msg_send();
#endif

			p->removewq();
			p->removeq();
		}
	}

msgq	*newq=queuefree;

	if (!newq)
	{
		ctlfile_close(&ctlinfo);

		if (queuefill && nextqueuefill == 0)
		{
			nextqueuefill=current_time + queuefill;
		}
		return (1);
	}

	const char *cn=qmsgsctlname(inum);

	if ( stat(cn, &stat_buf) == -1 )
	{
		unlink(name.c_str());
		unlink(qmsgsdatname(inum));
		unlink(cn);
		ctlfile_close(&ctlinfo);
		return (0);
	}


#if 0
clog_msg_start_info();
clog_msg_str("Adding ");
clog_msg_uint(inum);
clog_msg_str(" to queue.");
clog_msg_send();
#endif

	queuefree=newq->next;
	++queuedelivering;

	ino_t	hashbucket=inum % queuehashfirst.size();

	if (queuehashfirst[hashbucket])
		queuehashfirst[hashbucket]->prevhash=newq;
	else
		queuehashlast[hashbucket]=newq;

	newq->nexthash=queuehashfirst[hashbucket];
	newq->prevhash=0;
	queuehashfirst[hashbucket]=newq;

	newq->nksize= (unsigned long)stat_buf.st_size;
	ctlinfo.starttime=stat_buf.st_mtime;

int	k=ctlfile_searchfirst(&ctlinfo, COMCTLFILE_MSGID);

	newq->msgid= k < 0 ? "": ctlinfo.lines[k]+1;

	newq->msgnum=inum;
	newq->nextdel=deltime;
	newq->nextsenddel=delsendtime;
	newq->rcptinfo_list.resize(0);
	newq->rcptcount=0;
	newq->cancelled=ctlinfo.cancelled;

	if (isnewmsg)
	{
		int auth;

		clog_msg_start_info();
		clog_msg_str("newmsg,id=");
		logmsgid(newq);

		auth=ctlfile_searchfirst(&ctlinfo, COMCTLFILE_AUTHNAME);

		if (auth >= 0)
		{
			clog_msg_str(", auth=");
			clog_msg_str(ctlinfo.lines[auth]+1);
		}

		int m=ctlfile_searchfirst(&ctlinfo, COMCTLFILE_FROMMTA);

		if (m >= 0)
		{
			clog_msg_str(": ");
			clog_msg_str(ctlinfo.lines[m]+1);
		}

		clog_msg_send();
	}

	unsigned	i, j;
	std::string host, addr;

	k=ctlfile_searchfirst(&ctlinfo, COMCTLFILE_SENDER);

	struct rfc822t *sendert=rw_rewrite_tokenize(k < 0 ? "":ctlinfo.lines[k]+1);
	std::string	errmsg;

	for (i=0; i<ctlinfo.nreceipients; i++)
	{
		for (j=0; ctlinfo.lines[j]; j++)
		{
			switch (ctlinfo.lines[j][0])	{
			case COMCTLFILE_DELSUCCESS:
			case COMCTLFILE_DELFAIL:
				if ((unsigned)atoi(ctlinfo.lines[j]+1) == i)
					break;
				// This one has been delivered
			default:
				continue;
			}
			break;
		}
		if (ctlinfo.lines[j])	continue;

	drvinfo *module=getdelinfo(sendert->tokens,
			ctlinfo.receipients[i], host, addr, errmsg);

		if (!module)
		{
			ctlfile_append_reply(&ctlinfo, i, errmsg.c_str(),
					     SMTPREPLY_TYPE(errmsg.c_str()),
					     0);
			continue;
		}

		/* Check if it's time to move the message to a backup relay */

		if (backup_relay_driver &&
			ctlfile_searchfirst(&ctlinfo, COMCTLFILE_WARNINGSENT)
			>= 0 &&
		    strcmp(module->module->name,
			   backup_relay_driver->module->name) == 0)
		{
			// module=backup_relay_driver;
			host=backup_relay;
		}

		/* Group all recipients for the same driver and host together */

		for (j=0; j<newq->rcptcount; j++)
			if (strcmp(module->module->name,
				   newq->rcptinfo_list[j].delmodule->
				   module->name) == 0 &&
			    newq->rcptinfo_list[j].delhost == host &&
			    newq->rcptinfo_list[j].addresses.size()
			    < module->maxrcpt )
				break;
		if (j == newq->rcptcount)
		{
#if 0
clog_msg_start_info();
clog_msg_str("id=");
logmsgid(newq);
clog_msg_str(",new rcpt list - module=");
clog_msg_str(module->module->name);
clog_msg_str(", host=");
clog_msg_str(host);
clog_msg_send();
#endif
			newq->rcptinfo_list.resize(++newq->rcptcount);

		struct	rw_info_rewrite rwir;
		struct	rw_info	rwi;

			rwir.buf=0;
			rwir.errmsg=0;
			rw_info_init(&rwi, sendert->tokens, rw_err_func);
			rwi.sender=0;
			rwi.mode=RW_OUTPUT|RW_ENVSENDER;
			rwi.udata= (void *)&rwir;
			rw_rewrite_module(module->module, &rwi,
				rw_rewrite_chksyn_print);

		char *address=((struct rw_info_rewrite *)rwi.udata)->buf;
		char *errmsg= ((struct rw_info_rewrite *)rwi.udata)->errmsg;

			if (!address)
			{
				ctlfile_append_reply(&ctlinfo, i, errmsg,
					SMTPREPLY_TYPE(errmsg), 0);
				newq->rcptinfo_list.resize(--newq->rcptcount);
				free(errmsg);
				continue;
			}

			if (errmsg)	free(errmsg);
			newq->rcptinfo_list[j].init(newq, module, host, address);
			free(address);
		}
#if 0
clog_msg_start_info();
clog_msg_str("id=");
logmsgid(newq);
clog_msg_str(",module=");
clog_msg_str(module->module->name);
clog_msg_str(", host=");
clog_msg_str(host);
clog_msg_str(", addr=<");
clog_msg_str(addr);
clog_msg_str(">");
clog_msg_send();
#endif

		newq->rcptinfo_list[j].addresses.push_back(addr);
		newq->rcptinfo_list[j].addressesidx.push_back(i);
	}
	rfc822t_free(sendert);
	ctlfile_close(&ctlinfo);

	if (newq->nextsenddel <= current_time ||

/*
** If there are no more recipients, we want to call done() via
** start_message.  HOWEVER, if DSN injection FAILED, we want to respect
** the rescheduled delivery attempt time.  We can detect that if isnewmsg == 0
*/
		(newq->rcptinfo_list.size() == 0 && isnewmsg))
	{
		newq->start_message();
		return (0);
	}

msgq	*qp, *qprev;

	for (qprev=queuetail, qp=0; qprev; qp=qprev, qprev=qp->prev)
		if (qprev->nextsenddel < newq->nextsenddel)
			break;

	newq->next=qp;
	newq->prev=qprev;

	if (qprev)	qprev->next=newq;
	else		queuehead=newq;

	if (qp)	qp->prev=newq;
	else	queuetail=newq;
	++queuewaiting;
	return (0);
}

void msgq::start_message()
{
//
// Schedule deliveries for this message.
//

#if 0
clog_msg_start_info();
clog_msg_str("Start message ");
logmsgid(this);
clog_msg_send();
#endif

	size_t n=rcptinfo_list.size();

	if (n == 0)
	{
		done(this, 0);
		return;
	}

	size_t i;

	for (i=0; i<n; i++)
	{
		rcptinfo &ri=rcptinfo_list[i];

	drvinfo *drvp=ri.delmodule;
	dlvrhost *hostp, *freehostp=0;

//
// Check if the same host has any deliveries in progress, or pending.
// Search the hdlvrpfree/hdlvrplast link list for this host.
//
		for (hostp=drvp->hdlvrplast; hostp; hostp=hostp->prev)
		{
			if (hostp->hostname == ri.delhost)
{
#if 0
clog_msg_start_info();
clog_msg_str("Found host ");
clog_msg_str(ri.delhost);
clog_msg_str(", # of deliveries=");
clog_msg_uint(hostp->dlvrcount);
clog_msg_send();
#endif
				break;
}

//
// Just in case we don't find it, remember the last host at the end of the
// MRU list which does not have any deliveries in progress.
//
			if (!freehostp &&
				drvp->hdlvrpfree == 0 && hostp->dlvrcount == 0)
				freehostp=hostp;
				// Recycle a host struct used longest ago
		}
		if (!hostp)
		{
//
// Did not find this host.
//
			if (drvp->hdlvrpfree)
			{
//
// There's an unused host structure, take it.
//
				hostp=drvp->hdlvrpfree;
				drvp->hdlvrpfree=hostp->next;
			}
			else if (freehostp)
			{
//
// Recycle a host structure for a host without any current deliveries,
// remove it from the MRU list.
//
				hostp=freehostp;
				if (hostp->next)
					hostp->next->prev=hostp->prev;
				else
					drvp->hdlvrplast=hostp->prev;
				if (hostp->prev)
					hostp->prev->next=hostp->next;
				else
					drvp->hdlvrpfirst=hostp->next;
			}

			if (hostp)
			{
//
// Ok, initialize this host, and stick it at the end of the MRU list.
//
				hostp->hostname=ri.delhost;
				hostp->dlvrcount=0;
				hostp->next=0;
				if ((hostp->prev=drvp->hdlvrplast) != 0)
					drvp->hdlvrplast->next=hostp;
				else
					drvp->hdlvrpfirst=hostp;
				drvp->hdlvrplast=hostp;
				if (hostp->pending_list)
					hostp->pending_list->hostp=0;
				hostp->pending_list=0;
			}
		}
//
// Determine if we can start a new delivery.  The conditions are:
// A) We found an unused host structure, and the number of deliveries in
// progress to this host is less than the maximum set for the module,
//
// AND
//
// B) The total number of current deliveries to this host is less than
// the maximum set for this module (there's an available delivery slot
// structure)
//
		if (hostp && hostp->dlvrcount < drvp->maxhost &&
			drvp->delpfreefirst)
		{
		delinfo *newdi=drvp->delpfreefirst;

			drvp->delpfreefirst=newdi->freenext;
			newdi->dlvrphost=hostp;
			newdi->rcptlist=&ri;

			ri.pending=0;
			hostp->dlvrcount++;
			startdelivery(drvp, newdi);
			continue;
		}
//
// We are unable to start the delivery at this time.  At the recipient list
// to the list of pending deliveries for this host.
//
// The first thing is to make sure that this host structure has the pending
// list structure allocated.
//

	pendelinfo *pi;

		if (hostp)
		{
			if ((pi=hostp->pending_list) == 0)
			{
				drvp->pendelinfo_list.push_back(pendelinfo());

				std::list<pendelinfo>::iterator pos=
					--drvp->pendelinfo_list.end();

				pi=& *pos;
				pi->pos=pos;
				pi->drvp=drvp;
				pi->hostname=ri.delhost;
				pi->hostp=hostp;
				hostp->pending_list=pi;
			}
		}
		else
		{
			std::list<pendelinfo>::iterator pos;

//
// We don't even have a host structure.
// That's ok, search the pendelinfo_list of this module anyway.
//
			pi=0;

			for (pos=drvp->pendelinfo_list.begin();
			     pos != drvp->pendelinfo_list.end();
			     ++pos)
			{
				pi=& *pos;
				if (pi->hostname == ri.delhost) break;
				pi=0;
			}
			if (!pi)
			{
				drvp->pendelinfo_list.push_back(pendelinfo());
				pos= --drvp->pendelinfo_list.end();

				pi=& *pos;
				pi->pos=pos;
				pi->drvp=drvp;
				pi->hostname=ri.delhost;
				pi->hostp=0;
			}
		}

		ri.pending=pi;
		pi->receipient_list.push_back(&ri);

		ri.pendingpos= --pi->receipient_list.end();
	}
}

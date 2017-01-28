/*
** Copyright 1998 - 2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	"submit.h"
#include	"bofh.h"
#include	"rw.h"
#include	"maxlongsize.h"
#include	"courier.h"
#include	"libexecdir.h"
#include	"comreadtime.h"
#include	"comctlfile.h"
#include	"cdfilters.h"
#include	"localstatedir.h"
#include	"sysconfdir.h"
#include	"numlib/numlib.h"
#include	"rfc822/rfc822hdr.h"
#include	"rfc2045/rfc2045.h"
#include	"rfc2045/rfc2045charset.h"
#include	<stdio.h>
#include	<string.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<algorithm>
#include	<iostream>
#include	<iomanip>
#if HAVE_SYS_TYPES_H
#include	<sys/types.h>
#endif
#include        <sys/uio.h>
#if HAVE_SYS_WAIT_H
#include	<sys/wait.h>
#endif
#include	<signal.h>
#include	<stdlib.h>

#define	SPILLLEVEL	100

extern std::string security;
extern time_t	submit_time;
extern const char *submitdelay;
extern const char *msgsource;
extern const char *authname;
extern int suppressbackscatter;
extern int checkfreespace(unsigned *);
SubmitFile *SubmitFile::current_submit_file=0;

static time_t	queuetime, faxqueuetime, warntime;
static unsigned batchsize=100;
static int spamtrap_flag=0;

extern int verpflag;

//
// Messages are owned by the submitting user, but MAILGID must still be
// able to read/write them!
//

#define	PERMISSION	0660

SubmitFile::SubmitFile() : rwrfcptr(0)
{
}

SubmitFile::~SubmitFile()
{
	interrupt();
	current_submit_file=0;
	if (rwrfcptr)	rfc2045_free(rwrfcptr);
}

// Start the ball rolling by specifying the envelope sender.

void SubmitFile::Sender(const char *f,
	const char *p, const char *e, char t)
{
char buf[2];

	frommta=f;
	num_control_files_created=0;	// No control files created yet
	rcptcount=0;
	sender=p;
	envid= (e ? e:"");

       	buf[0]=t;
       	buf[1]=0;
       	dsnformat=buf;

	addrlist_map.clear();
	addrlist_gdbm.Close();
	// Read various configuration parameters

	queuetime=config_time_queuetime();
	faxqueuetime=config_time_faxqueuetime();
	warntime=config_time_warntime();
	batchsize=atoi(config_batchsize());
}

// Close the current to-be control file.

void SubmitFile::closectl()
{
	const char *p=config_get_local_vhost();

	static const char *env_vars[]={
		"RELAYCLIENT",
		"TCPREMOTEIP",
		0,
	};

	if (p && *p)
		ctlfile << COMCTLFILE_VHOST << p << std::endl;

	if (security.size() > 0)
		ctlfile << COMCTLFILE_SECURITY << security
			<< std::endl;
	if (verpflag)
		ctlfile << COMCTLFILE_VERP << std::endl;

	if (warntime == 0)
		ctlfile << COMCTLFILE_WARNINGSENT << std::endl;

	if (authname)
		ctlfile << COMCTLFILE_AUTHNAME << authname << std::endl;

	for (size_t i=0; env_vars[i]; ++i)
	{
		const char *p=getenv(env_vars[i]);

		if (p)
			ctlfile << COMCTLFILE_ENVVAR << env_vars[i] << "="
				<< p << std::endl;
	}

	ctlfile << COMCTLFILE_MSGSOURCE << msgsource << std::endl
		<< COMCTLFILE_EXPIRES << submit_time + queuetime << std::endl
		<< COMCTLFILE_FAXEXPIRES << submit_time + faxqueuetime << std::endl
		<< COMCTLFILE_WARNING << submit_time + warntime << std::endl << std::flush;

	if (suppressbackscatter)
		ctlfile << COMCTLFILE_TRACK << std::endl;

	if (ctlfile.fail())	clog_msg_errno();
#if EXPLICITSYNC
	ctlfile.sync();
	fsync(ctlfile.fd());
#endif
	ctlfile.close();
	if (ctlfile.fail())	clog_msg_errno();
}

int SubmitFile::ChkRecipient(const char *key)
{
	receipient=key;

	// Ignore duplicate addresses.

	if (addrlist_gdbm.IsOpen())
	{
		if (addrlist_gdbm.Exists(receipient))
			return (-1);	/* Already exists */

		if (addrlist_gdbm.Store(receipient, "", "R"))
			clog_msg_errno();
	}
	else
	{
		if (addrlist_map.find(receipient) != addrlist_map.end())
			return (-1);	/* Already exists */
		addrlist_map.insert(receipient);

		if (addrlist_map.size() > SPILLLEVEL)
		{
//
// Store the GDBM file in the name that's reserved for the first hard link
// link to the message file (which is never used).
//
			std::string gdbmname=namefile("D",1);

			if (addrlist_gdbm.Open(gdbmname.c_str(), "N"))
			{
				clog_msg_start_err();
				clog_msg_str(gdbmname.c_str());
				clog_msg_str(": ");
				clog_msg_errno();
			}

			std::set<std::string>::iterator b, e;

			for (b=addrlist_map.begin(),
				     e=addrlist_map.end(); b != e; ++b)
			{
				if (addrlist_gdbm.Store(*b, std::string(""),
							"R"))
					clog_msg_errno();
			}
			addrlist_map.clear();
		}
	}
	return (0);
}

void SubmitFile::AddReceipient(const char *r,
	const char *orig, const char *dsn, int delivered)
{
	// If # of receipients in the current control file exceeds the
	// defined batch size, close the current control file.

	if (rcptcount >= batchsize)
	{
		closectl();
//
// The first time we create another control file, rename the first
// control file to Cnnnn.1, which is a flag not to process Cnnnn.2, .3, etc...
//
// Cnnnn.2, .3, ... will be processed only after Cnnnn.1 is renamed to Cnnnn
//
		if (num_control_files_created == 1)
		{
			std::string p=name1stctlfile();
			std::string q=namefile("C", 1);

			if (rename(p.c_str(), q.c_str()))
			{
				clog_msg_start_err();
				clog_msg_str(p.c_str());
				clog_msg_str(" -> ");
				clog_msg_str(q.c_str());
				clog_msg_str(": ");
				clog_msg_errno();
			}
		}
	}

	// Open a new control file, if necessary.

	if (ctlfile.fd() < 0)
		openctl();
	ctlfile << COMCTLFILE_RECEIPIENT << r << std::endl
		<< COMCTLFILE_ORECEIPIENT << (orig ? orig:"") << std::endl
		<< COMCTLFILE_DSN << (dsn ? dsn:"") << std::endl;

	if (delivered)
	{
		ctlfile << COMCTLFILE_DELINFO << rcptcount << ' '
			<< COMCTLFILE_DELINFO_REPLY
			<< " 250 Ok - delivered to alias." << std::endl
			<< COMCTLFILE_DELSUCCESS << rcptcount << ' '
				<< submit_time << " r" << std::endl;
	}
	ctlfile << std::flush;
	++rcptcount;
	if (bofh_chkspamtrap(r))
	{
		spamtrap_flag=1;
	}
}

//
//  Save original recipient list for recipient-specific filtering.
//

void SubmitFile::ReceipientFilter(struct rw_transport *rw,
		const char *host,
		const char *addr,
		unsigned rcptnum)
{

	if (rcptfilterlist_file.is_open())
	{
		rcptfilterlist_file
			<< num_control_files_created - 1 << std::endl
			<< rcptcount - 1 << std::endl
			<< rw->name << std::endl
			<< host << std::endl
			<< addr << std::endl
			<< rcptnum << std::endl;

		return;
	}

	rcptfilterlist.push_back(RcptFilterInfo());
	RcptFilterInfo &last_pos=rcptfilterlist.back();

	last_pos.num_control_file=num_control_files_created - 1;
	last_pos.num_receipient=rcptcount - 1;
	last_pos.driver=rw;
	last_pos.host=host;
	last_pos.address=addr;
	last_pos.rcptnum=rcptnum;

	if (rcptfilterlist.size() > SPILLLEVEL)
	{
		std::string filename=namefile("R", 0);

		rcptfilterlist_file.open(filename.c_str(),
					 std::ios::in | std::ios::out
					 | std::ios::trunc);
		if (!rcptfilterlist_file.is_open())
		{
			clog_msg_start_err();
			clog_msg_str(filename.c_str());
			clog_msg_str(": ");
			clog_msg_errno();
		}

		unlink(filename.c_str());	/* Immediately delete it */

		std::list<RcptFilterInfo>::iterator ab, ae;

		for (ab=rcptfilterlist.begin(),
			     ae=rcptfilterlist.end(); ab != ae; ++ab)
		{
			RcptFilterInfo &p= *ab;

			rcptfilterlist_file << p.num_control_file << std::endl
				<< p.num_receipient << std::endl
				<< p.driver->name << std::endl
				<< p.host << std::endl
				<< p.address << std::endl
				<< p.rcptnum << std::endl;
		}
		rcptfilterlist.clear();
	}
}

void SubmitFile::openctl()
{
	std::string filename;

	ctlfile.close();
	if (num_control_files_created == 0)	// First recipient
	{
		for (;;)
		{
		const char *timeptr, *pidptr, *hostnameptr;

			getnewtmpfilenameargs(&timeptr, &pidptr,
						&hostnameptr);

			filename=name1stctlfile();
			current_submit_file=this;

			int nfd=open(filename.c_str(),
				     O_WRONLY | O_TRUNC | O_CREAT | O_EXCL | O_APPEND,
				     PERMISSION);

			if (nfd >= 0)
			  ctlfile.fd(nfd);

			if (nfd >= 0 || errno != EEXIST)
				break;
			current_submit_file=0;
			sleep(3);
		}
		++num_control_files_created;

		if (ctlfile.fd() < 0)
		{
		//
		// One reason why we may not be able to create it
		// would be if the subdirectory, based on current time,
		// does not exist, so fork a tiny program to create it,
		// with the right permissions
		//

		pid_t p=fork();
		pid_t w;
		int wait_stat;

			if (p == -1)
			{
				clog_msg_start_err();
				clog_msg_str("fork: ");
				clog_msg_errno();
			}

			if (p == 0)
			{
				filename=std::string(filename.begin(),
						     std::find(filename.begin()
							       , filename.end()
							       , '/'));

				execl(LIBEXECDIR "/courier/submitmkdir",
				      "submitmkdir", filename.c_str(),
				      (char *)0);
				exit(0);
			}
			while ((w=wait(&wait_stat)) != p)
				if (w == -1 && errno == ECHILD)	break;

			int nfd=open(filename.c_str(),
				     O_WRONLY | O_TRUNC | O_CREAT | O_EXCL | O_APPEND,
				     PERMISSION);

			if (nfd >= 0)
			  ctlfile.fd(nfd);
		}
	}
	else
	{
		++num_control_files_created;
		filename=namefile("C", num_control_files_created);

		int nfd=open(filename.c_str(),
			     O_WRONLY | O_TRUNC | O_CREAT | O_EXCL | O_APPEND,
			     PERMISSION);

		if (nfd >= 0)
			ctlfile.fd(nfd);
	}
	if (ctlfile.fd() < 0)	clog_msg_errno();

struct	stat	stat_buf;
char	ino_buf[sizeof(ino_t)*2+1];

	if (fstat(ctlfile.fd(), &stat_buf) != 0)
		clog_msg_errno();

	rcptcount=0;
	if (num_control_files_created == 1)
	{
	char	time_buf[sizeof(time_t)*2+1];
	char	pid_buf[sizeof(time_t)*2+1];
	char	msgidbuf[sizeof(time_buf)+sizeof(pid_buf)];

		ctltimestamp=stat_buf.st_mtime;
		ctlpid=getpid();

		strcat(strcat(strcpy(msgidbuf,
			libmail_strh_time_t(ctltimestamp, time_buf)), "."),
			libmail_strh_pid_t(getpid(), pid_buf));

		basemsgid=msgidbuf;
		ctlinodenum=stat_buf.st_ino;
	}

	libmail_strh_ino_t( stat_buf.st_ino, ino_buf );

	ctlfile << COMCTLFILE_SENDER << sender << std::endl
		<< COMCTLFILE_FROMMTA << frommta << std::endl
		<< COMCTLFILE_ENVID << envid << std::endl
		<< COMCTLFILE_DSNFORMAT << dsnformat << std::endl
		<< COMCTLFILE_MSGID << ino_buf << '.' << basemsgid << std::endl;

	if (submitdelay)
		ctlfile << COMCTLFILE_SUBMITDELAY << submitdelay << std::endl;

	ctlfile << std::flush;
}

std::string SubmitFile::name1stctlfile()
{
	const char *timeptr, *pidptr, *hostnameptr;

	gettmpfilenameargs(&timeptr, &pidptr, &hostnameptr);

	std::string submit_filename_buf=timeptr;
	size_t l=submit_filename_buf.size();

	if (l > 4)
		submit_filename_buf=
			submit_filename_buf.substr(0, l-4);
	else
		submit_filename_buf="0";

	std::string n=submit_filename_buf + "/" + timeptr + "." + pidptr + "."
		+ hostnameptr;

	all_files.insert(n);

	return n;
}

std::string SubmitFile::QueueID()
{
	char result[NUMBUFSIZE+1];

	libmail_strh_ino_t(ctlinodenum, result);

	return std::string(result) + "." + basemsgid;
}

std::string SubmitFile::namefile(const char *pfix, unsigned n)
{
char	buf[MAXLONGSIZE], *p;
const char *timeptr, *pidptr, *hostnameptr;

	gettmpfilenameargs(&timeptr, &pidptr, &hostnameptr);

	std::string submit_filename_buf=timeptr;

	size_t l=submit_filename_buf.size();

	if (l > 4)
		submit_filename_buf=
			submit_filename_buf.substr(0, l-4);
	else
		submit_filename_buf="0";

	submit_filename_buf = submit_filename_buf + "/" + pfix;

	p=buf+MAXLONGSIZE-1;

ino_t	inum=ctlinodenum;

	*p=0;
	do
	{
		*--p= '0' + (inum % 10);
		inum=inum/10;
	} while (inum);

	submit_filename_buf += p;

	if (n > 0)
	{
		p=buf+MAXLONGSIZE-1;
		*p=0;
		do
		{
			*--p= '0' + (n % 10);
			n=n/10;
		} while (n);
		submit_filename_buf = submit_filename_buf + "." + p;
	}

	all_files.insert(submit_filename_buf);
	return (submit_filename_buf);
}


// Process about to terminate.  Remove all possible files we might've
// created.

void SubmitFile::interrupt()
{
	if (!current_submit_file)	return;

	unlink(current_submit_file->name1stctlfile().c_str());
	if (current_submit_file->ctlinodenum == 0)	return;

unsigned	n;

	for (n=0; n<= 1 || n <= current_submit_file->num_control_files_created;
		n++)
		unlink(current_submit_file->namefile("D", n).c_str());

	n=current_submit_file->num_control_files_created;
	while ( n > 0)
	{
		unlink(current_submit_file->namefile("C", n).c_str());
		--n;
	}
}

//
// And now, process the message
//

void SubmitFile::MessageStart()
{
	bytecount=0;
	sizelimit=config_sizelimit();
	addrlist_map.clear();
	addrlist_gdbm.Close();
	if (ctlfile.fd() < 0)
		openctl();
	unlink(namefile("D", 1).c_str());	// Might be the GDBM file

	int nfd=open(namefile("D",0).c_str(),
		     O_RDWR | O_CREAT | O_TRUNC, PERMISSION);
	if (nfd < 0)
	{
		clog_msg_start_err();
		clog_msg_str(namefile("D",0).c_str());
		clog_msg_str(": ");
		clog_msg_errno();
	}

	datfile.fd(nfd);

	rwrfcptr=rfc2045_alloc_ac();

	if (rwrfcptr == NULL)
		clog_msg_errno();
	diskfull=checkfreespace(&diskspacecheck);
}

void SubmitFile::Message(const char *p)
{
size_t	l=strlen(p);

	if (sizelimit && bytecount > sizelimit)	return;
	bytecount += l;
	if (sizelimit && bytecount > sizelimit)	return;

	if (diskfull)	return;
	datfile << p;
	if (l > diskspacecheck)
	{
		if (checkfreespace(&diskspacecheck))
		{
			diskfull=1;
			return;
		}
		diskspacecheck += l;
	}
	diskspacecheck -= l;

	if (datfile.fail())	clog_msg_errno();
	rfc2045_parse(rwrfcptr, p, strlen(p));
}

/* ------------ */

static int call_rfc2045_rewrite(struct rfc2045 *p, int fdin_arg, int fdout_arg,
				const char *appname)
{
	struct rfc2045src *src=rfc2045src_init_fd(fdin_arg);
	int rc;

	if (!src)
		return -1;

	rc=rfc2045_rewrite(p, src, fdout_arg, appname);
	rfc2045src_deinit(src);
	return rc;
}

int SubmitFile::MessageEnd(unsigned rcptnum, int iswhitelisted,
			   int filter_enabled)
{
int	is8bit=0, dorewrite=0, rwmode=0;
const	char *mime=getenv("MIME");
unsigned	n;
struct	stat	stat_buf;

	if (sizelimit && bytecount > sizelimit)
	{
		std::cout << "523 Message length (" <<
			bytecount << " bytes) exceeds administrative limit(" << sizelimit << ")."
			<< std::endl << std::flush;
		return (1);
	}

	if (diskfull)
	{
		std::cout << "431 Mail system full." << std::endl << std::flush;
		return (1);
	}

	if (spamtrap_flag)
	{
		std::cout << "550 Spam refused." << std::endl << std::flush;
		return (1);
	}

	if (rwrfcptr->rfcviolation & RFC2045_ERR2COMPLEX)
	{
                std::cout <<
                   "550 Message MIME complexity exceeds the policy maximum."
		     << std::endl << std::flush;
		return (1);
	}

	datfile << std::flush;
	if (datfile.fail())	clog_msg_errno();

	ctlfile << std::flush;
	if (ctlfile.fail())	clog_msg_errno();

	/* Run global filters for this message */

	std::string dfile=namefile("D", 0);

	if (!mime || strcmp(mime, "none"))
	{
		if (mime && strcmp(mime, "7bit") == 0)
		{
			rwmode=RFC2045_RW_7BIT;
			is8bit=0;
		}
		if (mime && strcmp(mime, "8bit") == 0)
			rwmode=RFC2045_RW_8BIT;
		if (rfc2045_ac_check(rwrfcptr, rwmode))
			dorewrite=1;
	}
	else
		(void)rfc2045_ac_check(rwrfcptr, 0);

	if (rwrfcptr->has8bitchars)
		is8bit=1;

	unlink(namefile("D", 1).c_str());	// Might be the GDBM file
					// if receipients read from headers.
	if (dorewrite)
	{
		int	fd1=dup(datfile.fd());
		int	fd2;

		if (fd1 < 0)	clog_msg_errno();
		datfile.close();
		if (datfile.fail())	clog_msg_errno();

		if ((fd2=open(namefile("D", 1).c_str(),
			O_RDWR|O_CREAT|O_TRUNC, PERMISSION)) < 0)
			clog_msg_errno();

		if (call_rfc2045_rewrite(rwrfcptr, fd1, fd2,
					 PACKAGE " " VERSION))
		{
			clog_msg_errno();
			std::cout << "431 Mail system full." << std::endl << std::flush;
			return (1);
		}
		close(fd1);

#if	EXPLICITSYNC
		fsync(fd2);
#endif
		fstat(fd2, &stat_buf);
		close(fd2);

		std::string p=namefile("D", 0);

		unlink(p.c_str());
		if (rename(namefile("D", 1).c_str(), p.c_str()) != 0)
			clog_msg_errno();
	}
	else
	{
		datfile.sync();
#if EXPLICITSYNC
		fsync(datfile.fd());
#endif
		fstat(datfile.fd(), &stat_buf);
		datfile.close();
		if (datfile.fail())	clog_msg_errno();
	}
	if (is8bit)
	{
		ctlfile << COMCTLFILE_8BIT << "\n" << std::flush;
		closectl();

		if (num_control_files_created > 1)
		{
			for (n=1; n < num_control_files_created; n++)
			{
				std::string p=namefile("C", n);

				int nfd=open(p.c_str(), O_WRONLY | O_APPEND);

				if (nfd < 0)	clog_msg_errno();
				ctlfile.fd(nfd);
				ctlfile << COMCTLFILE_8BIT << "\n" << std::flush;
				if (ctlfile.fail())	clog_msg_errno();
#if EXPLICITSYNC
				ctlfile.sync();
				fsync(ctlfile.fd());
#endif
				ctlfile.close();
				if (ctlfile.fail())	clog_msg_errno();
			}
		}
	}
	else
	{
		closectl();
	}

	SubmitFile *voidp=this;

	if (filter_enabled &&
	    run_filter(dfile.c_str(), num_control_files_created,
		       iswhitelisted,
		       &SubmitFile::get_msgid_for_filtering, &voidp))
		return (1);

	std::string cfile=namefile("C", 0);

	for (n=2; n <= num_control_files_created; n++)
	{
		if (link(dfile.c_str(), namefile("D", n).c_str()) != 0)
			clog_msg_errno();
	}

	std::string okmsg("250 Ok. ");

	okmsg += basemsgid;

	int	hasxerror=datafilter(dfile.c_str(), rcptnum, okmsg.c_str());

	current_submit_file=0;
	if (num_control_files_created == 1)
	{
		if (rename(name1stctlfile().c_str(), cfile.c_str()) != 0)
			clog_msg_errno();
	}
	else
	{
		if (rename(namefile("C", 1).c_str(), cfile.c_str()) != 0)
			clog_msg_errno();
	}

	if (!hasxerror)
	{
#if EXPLICITDIRSYNC
		size_t p=cfile.rfind('/');

		if (p != std::string::npos)
		{
			std::string dir=cfile.substr(0, p);

			int fd=open(dir.c_str(), O_RDONLY);

			if (fd >= 0)
			{
				fsync(fd);
				close(fd);
			}
		}
#endif

		std::cout << okmsg << std::endl << std::flush;
	}

	trigger(TRIGGER_NEWMSG);
	return (0);
}

std::string SubmitFile::get_msgid_for_filtering(unsigned n, void *p)
{
	SubmitFile *objptr= *(SubmitFile **)p;
	std::string ctlname(TMPDIR "/");

	if (objptr->num_control_files_created == 1)
		ctlname += objptr->name1stctlfile();
	else
		ctlname += objptr->namefile("C", n+1);

	return (ctlname);
}

static void print_xerror(const char *address, const char *errbuf, int isfinal)
{
unsigned	c;
const char *p;
int	errcode=0;

	do
	{
		for (c=0; errbuf[c] && errbuf[c] != '\n'; c++)
			;
		p=errbuf;
		errbuf += c;
		if (*errbuf)	++errbuf;

		if (!errcode)
		{
			if (c > 3 && p[0] >= '2' && p[0] <= '5' &&
				p[1] >= '0' && p[1] <= '9' &&
				p[2] >= '0' && p[2] <= '9')
				errcode= (p[0] - '0') * 100 +
					(p[1] - '0') * 10 + p[2] - '0';
			else
				errcode=450;
		}
		if (c > 4 &&  p[0] >= '2' && p[0] <= '5' &&
			p[1] >= '0' && p[1] <= '9' &&
			p[2] >= '0' && p[2] <= '9' &&
			(p[3] == ' ' || p[3] == '-'))
		{
			p += 4;
			c -= 4;
		}
		if (address)
		{
			std::cout << "558-" << errcode << "-"
				<< address << ":" << std::endl;
			address=0;
		}
		std::cout << (( *errbuf == 0 && isfinal) ? "558 ":"558-")
			<< errcode << (*errbuf == 0 ? " ":"-");
		std::cout.write(p, c);
		std::cout << std::endl;
	} while (*errbuf);
}

//
// SubmitFile::datafilter runs recipient-specific filters.  This function
// will generate a data extended error message if any recipient's filters
// reject the message.  Each rejection results in the control file being
// reopened and appended a delivery record for the failed recipients, so
// we don't actually try to deliver the message to this address (the addy
// is officially rejected).
//

int SubmitFile::datafilter(const char *datfilename, unsigned nrcpts,
				const char *okmsg)
{
int	fd=open(datfilename, O_RDONLY);
unsigned last_error=0;
int	flag=0;

	if (fd < 0)	clog_msg_errno();

	if (rcptfilterlist_file.is_open())
	{
		// List of receipients was large enough to be dumped into a
		// file.

		rcptfilterlist_file.seekg(0);
		if (rcptfilterlist_file.bad())
			clog_msg_errno();

		// Read recipient list from the dump file, and filter each
		// one.

		RcptFilterInfo r;
		struct rw_transport *rw;
		std::string buf;

		while (!std::getline(rcptfilterlist_file, buf).fail())
		{
			if (sscanf(buf.c_str(), "%u",
				   &r.num_control_file) != 1
			    || std::getline(rcptfilterlist_file, buf).fail()
			    || sscanf(buf.c_str(), "%u",
				      &r.num_receipient) != 1
			    || std::getline(rcptfilterlist_file, buf).fail())
				clog_msg_errno();

			for (rw=rw_transport_first; rw; rw=rw->next)
				if (rw->name == buf)
					break;
			if (!rw)	clog_msg_errno();
			r.driver=rw;
			if ( std::getline(rcptfilterlist_file, r.host).fail()
			     || std::getline(rcptfilterlist_file,
					     r.address).fail()
			     || std::getline(rcptfilterlist_file, buf).fail()
			     || sscanf(buf.c_str(), "%u",
				       &r.rcptnum) != 1)
				clog_msg_errno();

			do_datafilter(last_error, flag, fd,
				      r.driver,
				      r.host,
				      r.address,
				      r.rcptnum,

				      r.num_control_file,
				      r.num_receipient,

				      okmsg, nrcpts);
		}
		if ( rcptfilterlist_file.bad())
			clog_msg_errno();
		rcptfilterlist_file.close();
	}
	else
	{
		std::list<RcptFilterInfo>::iterator ab, ae;

		for (ab=rcptfilterlist.begin(),
			     ae=rcptfilterlist.end(); ab != ae; ++ab)
		{
			RcptFilterInfo &r= *ab;

			do_datafilter(last_error, flag, fd,
				      r.driver,
				      r.host,
				      r.address,
				      r.rcptnum,

				      r.num_control_file,
				      r.num_receipient,

				      okmsg, nrcpts);
		}
	}
	close(fd);
	while (last_error && last_error < nrcpts)
	{
		print_xerror(0, okmsg, ++last_error == nrcpts);
	}
	std::cout << std::flush;
	return (flag);
}

// This is where the dirty work of running a filter actually happens.
// We get here whether the list of recipients was small enough to fit
// into memory, or not.

void SubmitFile::do_datafilter( unsigned &last_error, int &flag, int fd,
				struct rw_transport *driver,
				std::string host,
				std::string address,
				unsigned rcptnum,

	unsigned num_control_file,
	unsigned num_receipient,

	const char *okmsg,
	unsigned nrcpts)
{
char	buf[2048];
int	ctf;
int	rc;

	if (driver->rw_ptr->filter_msg == 0)	return;

	buf[0]=0;

	if (lseek(fd, 0L, SEEK_SET) < 0)
		clog_msg_errno();

	// Call the driver's filter function.

	rc=driver->rw_ptr->filter_msg
		? (*driver->rw_ptr->filter_msg)(sending_module, fd,
						host.c_str(),
						address.c_str(),
						sender.c_str(),
						buf, sizeof(buf)):0;
	if (rc == 0)	return;

	if (buf[0] == 0)	// Error but no msg, make one up
		strcpy(buf, "Access denied.");

	ctf=open ( (num_control_files_created == 1 ?
		    name1stctlfile(): namefile( "C", num_control_file+1))
		   .c_str(), O_WRONLY|O_APPEND);
	if (ctf < 0)	clog_msg_errno();

	/*
	** Mark the recipient as delivered, so no more processing is
	** done.
	*/
	ctlfile_append_replyfd(ctf, num_receipient,
		buf, COMCTLFILE_DELSUCCESS, 0);
	close(ctf);

	/*
	** We are now required to return an EXDATA extended error.  If there
	** were any good recipients up until now, we need to return an OK
	** message for those recipients.
	*/

	while (last_error < rcptnum)
	{
		print_xerror(0, okmsg, 0);
		++last_error;
	}

	print_xerror( address.c_str(), buf, rcptnum + 1 == nrcpts);
	++last_error;
	flag=1;
}

static RETSIGTYPE sighandler(int signum)
{
	SubmitFile::interrupt();
	signal(SIGINT, SIG_DFL);
	kill(getpid(), SIGKILL);
#if	RETSIGTYPE != void
	return (0);
#endif
}

void SubmitFile::trapsignals()
{
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGHUP, sighandler);
	signal(SIGALRM, sighandler);
	if (atexit(SubmitFile::interrupt)) clog_msg_errno();
}

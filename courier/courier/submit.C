/*
** Copyright 1998 - 2013 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"localstatedir.h"
#include	"rw.h"
#include	"comcargs.h"
#include	"comtrack.h"
#include	"rfc822/rfc822.h"
#include	"rfc822/rfc2047.h"
#include	"rfc2045/rfc2045charset.h"
#include	"rfc1035/rfc1035.h"
#include	"rfc1035/rfc1035mxlist.h"
#include	"rfc1035/spf.h"
#include	"numlib/numlib.h"
#include	"dbobj.h"
#include	"afx/afx.h"
#include	"aliases.h"
#include	"submit.h"
#include	"maxlongsize.h"
#include	"bofh.h"
#include	<string.h>
#include	<cstdlib>
#include	<ctype.h>
#include	<errno.h>
#include	<utime.h>
#include	<iostream>
#if	HAVE_LOCALE_H
#include	<locale.h>
#endif
#if	HAVE_SYS_TYPES_H
#include	<sys/types.h>
#endif
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<fcntl.h>
#include	<time.h>
#include	<poll.h>

#include	<algorithm>
#include	<functional>

#include	<ctype.h>

time_t	submit_time;
static const char *expnname=0;
static const char *vrfyname=0;
static const char *receivedfrommta=0;
static const char *identinfo=0;
static const char *onlybcc=0;

const char *authname=0;

static const char *rfc3848_receivedwith=0;

const char *msgsource=0;
int suppressbackscatter=0;

const char *submitdelay=0;

static int nofilter;

int verpflag;
std::string security;
extern std::string mkmessageidheader();
const char *setvhost=0;

static struct courier_args arginfo[]={
	{"expn", &expnname},
	{"vrfy", &vrfyname},
	{"bcc", &onlybcc},
	{"src", &msgsource},
	{"delay", &submitdelay},
	{"auth", &authname},
	{"rfc3848", &rfc3848_receivedwith},
	{"vhost", &setvhost},
	{0}
	} ;

// Strip off trailing periods in domain names.

static void stripdots(std::string &s)
{
	std::string::iterator b=s.begin(), e=s.end();

	while (b < e && e[-1] == '.')
		--e;

	s.erase(e, s.end());
}

static const char you_are_whitelisted[]=
	"412 You are whitelisted by this recipient, please try again later.";

static const char you_are_not_whitelisted[]=
	"412 You are not whitelisted by this recipient, please try again later.";

// A pointer to expninfo is supplied as a misc argument to the
// rewrite_address function, which passes it along to the handler of the
// rewritten address.
//
// expninfo is used by the handler function (showexpn, showvrfy, or ...)
// in order to find the original AliasSearch function to search for
// any aliases of this address.

struct expninfo {
	AliasSearch *asptr;
	int	flag;
	struct rw_transport *module;
	} ;

/////////////////////////////////////////////////////////////////////////////
//
// rewrite_address() rewrites an arbitrary address using a specified module's
// rewriting function.
//
// inaddress - address to rewrite
// module    - module whose rewrite function is used
// mode      - rewriting mode
// sender    - what to put in rw_info.sender
//
// handler   - function that will be called.  The first argument to the
//             function will be the rewritten address, the second argument
//             is the same address, but as rfc822token link list.  The third
//             argument will be an error message.  If NULL, the address was
//             rewritten succesfully.  The last argument will be the misc
//             pointer.
// misc      - miscellaneous pointer to pass to the handler function.
//
/////////////////////////////////////////////////////////////////////////////

static void rewrite_address(const char *inaddress, struct rw_transport *module,
	int mode,
	struct rfc822token *sender,
	void (*handler)(const char *inaddress,
		struct rfc822token *inaddresst, const char *errmsg, void *misc),
	void *misc)
{
struct	rw_info_rewrite	rwir;
struct	rw_info rwi;
struct	rfc822t *rfcp;
char	*address, *p, *errmsg;

	rfcp=rw_rewrite_tokenize(inaddress);

	rw_info_init(&rwi, rfcp->tokens, rw_err_func);

	rwi.mode=mode;
	rwi.sender=sender;
	rwi.udata=(void *)&rwir;
	rwir.buf=0;
	rwir.errmsg=0;

	rw_rewrite_module(module, &rwi, rw_rewrite_chksyn_print);

	address=((struct rw_info_rewrite *)rwi.udata)->buf;
	if (address)
	{
	char	*hostdomain=0;

		if ((p=strrchr(address, '@')) &&
			config_islocal(p+1, &hostdomain) && hostdomain == 0)
			locallower(address);
		if (hostdomain)	free(hostdomain);
		domainlower(address);
	}
	errmsg=((struct rw_info_rewrite *)rwi.udata)->errmsg;

	(*handler)( address, rwi.ptr, errmsg, misc);
	if (address)	free(address);
	if (errmsg)	free(errmsg);
	rfc822t_free(rfcp);
}

int checkfreespace(unsigned *availbytes)
{
static unsigned nsubmitters;
static unsigned min_inodes=0;
unsigned long checkblocks, nfreeblocks, checkinodes, nfreeinodes;
unsigned blksize;
unsigned dummy;

	if (min_inodes == 0)	// Calculate thresholds
	{
	const char *p=getenv("MAXSUBMITTERS");

		nsubmitters= p ? (unsigned)atoi(p):0;

		if (nsubmitters < 10)	nsubmitters=10;
		if (nsubmitters > 1000)	nsubmitters=1000;

		min_inodes=nsubmitters*5+100;
	}

	if (!availbytes)	availbytes= &dummy;

	*availbytes=65536;

	if (freespace( ".", &checkblocks, &nfreeblocks, &checkinodes, &nfreeinodes, &blksize))
	{
		return (0);
	}

	if (checkinodes && nfreeinodes < min_inodes)	return (-1);

	if (nfreeblocks < blksize * nsubmitters/65536 + 100)
		*availbytes=4096;
	else if (nfreeblocks < blksize * nsubmitters/4096 + 100)
		return (-1);
	return (0);

}

//////////////////////////////////////////////////////////////////////
//
// ExpnAliasHandler - subclasses AliasHandler.  The AliasHandler
// class is used by the AliasSearch class to output addresses found
// in the aliases.dat file.  AliasHandler is subclassed so that
// one alias is always buffered.  When an alias address is received,
// the previous one is printed on std::cout, as an RFC822 multiline
// response.  The final call to ExpnAliasHandler::Alias() std::flushes out
// the final address and prints it as an RFC822 final multiline
// response.
//
//////////////////////////////////////////////////////////////////////

class ExpnAliasHandler : public AliasHandler {
public:
	ExpnAliasHandler();
	~ExpnAliasHandler();

	std::string prev_alias;
	void	Alias(const char *);
} ;

ExpnAliasHandler::ExpnAliasHandler()
{
}

ExpnAliasHandler::~ExpnAliasHandler()
{
}

void	ExpnAliasHandler::Alias(const char *p)
{
	if ( prev_alias.size() > 0)
		std::cout << "250" << ( p ? '-':' ') <<
			"<" << prev_alias << ">" << std::endl << std::flush;

	if (p)
		prev_alias=p;
}

////////////////////////////////////////////////////////////////////////////
//
// showexpn - expand aliases.  After rewriting an address, expand any aliases.
//
////////////////////////////////////////////////////////////////////////////

static void showexpn(const char *address, struct rfc822token *addresst,
	const char *errmsg, void *misc)
{
struct	expninfo *info = (struct expninfo *)misc;

	addresst=addresst;

	if (errmsg)
	{
		std::cout << errmsg << std::flush;
		info->flag=1;
		return;
	}

	ExpnAliasHandler expnh;
	int rc;

	rc=info->asptr->Search(address, expnh);

	if (rc < 0)
		std::cout << "400 <" << address <<
			"> - service temporarily unavailable."
		     << std::endl << std::flush;
	else if (rc)
		std::cout << "500 <" << address << "> not found." << std::endl << std::flush;
				// No aliases
	else
		expnh.Alias(0);
				// Print last alias
	info->flag=0;
}

/////////////////////////////////////////////////////////////////////////////
//
// VRFY
//
// A pointer to rw_info_vrfy is passed to rw_searchdel as a misc pointer.
// The found_transport function is called when an output module is found
// for the address.  found_transport() saves the delivery information in
// the rw_info_vrfy structure
//
/////////////////////////////////////////////////////////////////////////////

struct rw_info_vrfy {
	struct rw_info_rewrite rwr;
	struct	rw_transport *module;
	char	*host;
	char	*addr;
	} ;

static void found_transport(struct rw_info *rwi,
		struct rw_transport *t,
		const struct rfc822token *host,
		const struct rfc822token *addr)
{
struct rw_info_vrfy *p=(struct rw_info_vrfy *)rwi->udata;

	p->module=t;
	if (!(p->host=rfc822_gettok(host)) ||
		!(p->addr=rfc822_gettok(addr)))
		clog_msg_errno();
}

static void showvrfy(const char *address, struct rfc822token *addresst,
	const char *errmsg, void *misc)
{
struct	expninfo *info = (struct expninfo *)misc;

	addresst=addresst;

	if (!address)
	{
		std::cout << errmsg << std::flush;
		info->flag=1;
		return;
	}

	// If there's an alias defined for this one, presume that this is
	// a good address.

	if (info->asptr->Found(address))
	{
		std::cout << "250 <" << address << ">" << std::endl << std::flush;
		return;
	}

	// Attempt to find an output module for this address.

struct	rfc822t *rfcp=rw_rewrite_tokenize(address);
struct	rw_info_vrfy riv;

	riv.rwr.buf=0;
	riv.rwr.errmsg=0;
	riv.module=0;
	riv.host=0;
	riv.addr=0;

struct	rw_info	rwi;

	rw_info_init(&rwi, rfcp->tokens, rw_err_func);
	rwi.udata= (void *)&riv;
	rwi.mode=RW_VERIFY;
	rw_searchdel(&rwi, &found_transport);
	rfc822t_free(rfcp);

	if (!riv.module && !riv.rwr.errmsg)
		rw_err_func(511, "Undeliverable address.", &rwi);


	if (!riv.module)	// Not found.
	{
		std::cout << riv.rwr.errmsg << std::flush;
		((struct expninfo *)misc)->flag=1;
		free(riv.rwr.errmsg);
		return;
	}
	free(riv.host);
	free(riv.addr);
	std::cout << "250 <" << address << ">" << std::endl << std::flush;
}

class Input {

	char buffer[BUFSIZ];

	char *bufptr;
	size_t bufleft;

	SubmitFile *submitfile;

	int fillbuf();

public:

	Input();
	~Input();

	void setSubmitFile(SubmitFile &f)
	{
		submitfile= &f;
	}

	int getinputline(std::string &linebuf);
	int getinputchar();
	int unputinputchar(char c);
};

Input::Input() : bufptr(NULL), bufleft(0), submitfile(NULL)
{
}

Input::~Input()
{
}

int Input::fillbuf()
{
	struct pollfd pfd;
	int n;

	std::cout.flush();
	std::cerr.flush();

	pfd.fd=0;
	pfd.events=POLLIN | POLLHUP | POLLERR;

	while (1)
	{
		n=poll(&pfd, 1, submitfile ? 300*1000:-1);

		if (n > 0)
			break;

		if (n == 0)
		{
			if (submitfile)
			{
				std::set<std::string>::iterator
					b=submitfile->all_files.begin(),
					e=submitfile->all_files.end();

				while (b != e)
				{
					utime(b->c_str(), NULL);
					++b;
				}
			}
			continue;
		}
		if (errno != EINTR)
			return -1;
	}

	if (!(pfd.revents & POLLIN))
		return -1;

	if ((n=read(0, buffer, sizeof(buffer))) <= 0)
		return -1;

	bufptr=buffer;
	bufleft=n;
	return 0;
}

int Input::getinputline(std::string &linebuf)
{
	linebuf.clear();

	do
	{
		size_t i;

		for (i=0; i<bufleft; i++)
		{
			if (bufptr[i] == '\n')
			{
				linebuf.append(bufptr, i);
				++i;
				bufptr += i;
				bufleft -= i;
				return 0;
			}
		}

		linebuf.append(bufptr, bufleft);
		bufleft=0;
	} while (fillbuf() == 0);

	if (linebuf.size())
		return 0;
	return -1;
}

int Input::getinputchar()
{
	if (!bufleft && fillbuf())
		return -1;

	--bufleft;
	return (unsigned char)*bufptr++;
}

int Input::unputinputchar(char c)
{
	++bufleft;
	*--bufptr=c;
	return (unsigned char)c;
}

static void getrcpts(struct rw_info *rwi);

struct mailfrominfo {
	AliasSearch	*asptr;
	std::string	*envid;
	char	dsnformat;
	struct	rw_transport *module;
	int	flag;
	std::string	helohost;
	std::string	*receivedspfhelo;
	std::string	*receivedspfmailfrom;
	int	mailfrom_passed_spf;

	Input		*input;
	} ;

static void mailfromerr(int msgnum, const char *errtext, struct rw_info *rwi)
{
	rwi=rwi;

char	*errmsg=makeerrmsgtext(msgnum, errtext);

	std::cout << errmsg << std::flush;
	free(errmsg);
}

static void setmsgopts(struct mailfrominfo &mfi, std::string optstring)
{
	std::string::iterator q=optstring.begin();

	while (q != optstring.end())
	{
		char ch= *q++;

		std::string opts;

		if (q != optstring.end() && *q == '{')
		{
			std::string::iterator b= ++q;

			while (q != optstring.end())
			{
				if (*q == '}')
				{
					opts=std::string(b, q);
					++q;
					break;
				}
				++q;
			}
		}

		switch (ch) {
		case 'f':
		case 'F':
			if (mfi.dsnformat == 0)
				mfi.dsnformat='F';
			break;
		case 'h':
		case 'H':
			if (mfi.dsnformat == 0)
				mfi.dsnformat='H';
			break;
		case 'V':
			verpflag=1;
			break;
		case 's':
		case 'S':
			if (opts.size() && security.size() == 0)
				security=opts;
			break;
		}
	}
}

static int ipcmp(const char *a, const char *b)
{
	if (strncasecmp(a, "::ffff:",7) == 0)
		a += 7;
	if (strncasecmp(b, "::ffff:",7) == 0)
		b += 7;

	return strcasecmp(a, b);
}

static int checka(const char *hostname)
{
	RFC1035_ADDR *addrs;
	unsigned        naddrs, n;
	int rc;
	struct rfc1035_res res;
	const char *tcpremoteip=getenv("TCPREMOTEIP");

	rfc1035_init_resolv(&res);
	rc=rfc1035_a(&res, hostname, &addrs, &naddrs);
	rfc1035_destroy_resolv(&res);

	if (rc > 0)
		return 1;

	if (rc < 0)
		return -1;

	for (n=0; n<naddrs; n++)
	{
		char	buf[RFC1035_NTOABUFSIZE];

		rfc1035_ntoa(&addrs[n], buf);
		if (ipcmp(buf, tcpremoteip) == 0)
		{
			free(addrs);
			return 0;
		}
	}

	free(addrs);
	return (-1);
}

static const char *my_spf_lookup(const char *checkname,
				 const char *mailfrom,
				 const char *tcpremoteip,
				 const char *helodomain,
				 std::string *hdrOut,
				 char *errmsg_buf,
				 size_t errmsg_buf_size)
{
	static const char * const keys[]={
		"sender",
		"remoteip",
		"remotehost",
		"helo",
		"receiver"};
	const char *values[5];
	char *q;
	char result=rfc1035_spf_lookup((values[0]=mailfrom),
				       (values[1]=tcpremoteip),
				       (values[2]=getenv("TCPREMOTEHOST")),
				       (values[3]=helodomain),
				       (values[4]=config_esmtphelo()),
				       errmsg_buf,
				       errmsg_buf_size);

	const char *str;
	size_t i;

	switch (result) {
	case SPF_NONE:
		str="none";
		break;
	case SPF_NEUTRAL:
		str="neutral";
		break;
	case SPF_PASS:
		str="pass";
		break;
	case SPF_FAIL:
		str="fail";
		break;
	case SPF_SOFTFAIL:
		str="softfail";
		break;
	case SPF_ERROR:
		str="error";
		break;
	default:
		str="unknown";
	}

	(*hdrOut)="Received-SPF: ";

	for (q=errmsg_buf; *q; q++)
		if (*q < ' ' || *q >= 127)
			*q='?';

	(*hdrOut) += str;
	(*hdrOut) += " (";
	(*hdrOut) += errmsg_buf;
	(*hdrOut) += ")\n  SPF=";
	(*hdrOut) += checkname;
	(*hdrOut) += ";\n";

	for (i=0; i<sizeof(values)/sizeof(values[0]); i++)
	{
		char *v=strdup(values[i] ? values[i]:"");

		for (q=v; q && *q; q++)
			if (*q < ' ' || *q >= 127)
				*q='?';
			else if (*q == ';')
				*q=' ';
		(*hdrOut) += "  ";
		(*hdrOut) += keys[i];
		(*hdrOut) += "=";
		(*hdrOut) += v ? v:strerror(errno);
		(*hdrOut) += ";\n";
	}
	return str;
}

static inline char* getenv_allow(int found)
{
	char buf[32];
	sprintf(buf, "ALLOW_%d", found);
	return getenv(buf);
}

static std::string get_spf_errmsg(const char *bofh_var,
				  const char *result,
				  const char *errmsg_txt,
				  const char *what)
{
	std::string errmsg;

	int allowok = getenv_allow(0) &&
		bofh_checkspf(bofh_var, "off", "allowok");

	if (!allowok && !bofh_checkspf_status(bofh_var, "off", result))
	{
		const char *p=getenv("BOFHSPFNOVERBOSE");

		errmsg += bofh_checkspf("BOFHSPFHARDERROR",
					"fail,softfail", result)
			? "517":"417";
		errmsg += " SPF ";
		errmsg += result;
		errmsg += " ";
		errmsg += what;
		errmsg += ": ";

		if (p && atoi(p))
			errmsg += "Rejected by the Sender Policy Framework.";
		else errmsg += errmsg_txt;
	}
	return errmsg;
}

static int checkmx(const char *helohost,
		   const char *tcpremoteip,
		   int bofhcheckhelo)
{
	struct rfc1035_res res;
	int rc;
	/*
	** If HELO/EHLO does not match the IP address, query the MX
	** records.
	*/
	struct rfc1035_mxlist *mxlist, *mxp;
	int checked_a=0;

	const char *harderr="517";

	if (bofhcheckhelo == 2)
		harderr="417";

	rfc1035_init_resolv(&res);

	rc=rfc1035_mxlist_create_x(&res, helohost,
				   RFC1035_MX_AFALLBACK |
				   RFC1035_MX_IGNORESOFTERR,
				   &mxlist);

	rfc1035_destroy_resolv(&res);

	switch (rc)
	{
	case RFC1035_MX_OK:

		for (mxp=mxlist; mxp; mxp=mxp->next)
		{
			RFC1035_ADDR	addr;
			char	buf[RFC1035_NTOABUFSIZE];

			if (rfc1035_sockaddrip(&mxp->address,
					       sizeof(mxp->address),
					       &addr))
				continue;

			rfc1035_ntoa(&addr, buf);
			if (ipcmp(buf, tcpremoteip) == 0)
				break;

			if (mxp->priority == -1)
				checked_a=1;

		}
		if (mxp)
		{
			rfc1035_mxlist_free(mxlist);
			break;
		}
		rfc1035_mxlist_free(mxlist);

		if (!checked_a) /* No need to recheck A records */
		{
			int rc=checka(helohost);

			if (rc == 0)
			{
				break;
			}

			if (rc > 0) /* temp failure */
			{
				std::cout << "417 DNS lookup on HELO "
					  << helohost
					  << " failed.  Try again later."
					  << std::endl << std::flush;
				return 1;
			}
		}

		std::cout << harderr << " HELO " << helohost
			  << " does not match " << tcpremoteip << std::endl;
		return 1;

	case RFC1035_MX_HARDERR:
		std::cout << harderr << " HELO " << helohost
			  << " does not exist." << std::endl;
		return 1;
	case RFC1035_MX_BADDNS:
		std::cout << "517 RFC 1035 violation: recursive CNAME records for " << helohost << "." << std::endl;
		return 1;
	default:
		std::cout << "417 DNS lookup on HELO " << helohost <<
			" failed.  Try again later." << std::endl << std::flush;
		return 1;
	}
	return 0;
}

static int getsender(Input &input, struct rw_transport *module)
{
	struct	mailfrominfo frominfo;
	std::string	buf;
	std::string	sender;
	std::string	envid;
	AliasSearch	asearch;
	std::string receivedspfhelo;
	std::string receivedspfmailfrom;
	const char *p, *q;

	static char bofh1[]="BOFHSPFHELO=off",
		bofh2[]="BOFHSPFMAILFROM=off",
		bofh3[]="BOFHSPFFROM=off";

	frominfo.asptr= &asearch;
	frominfo.flag=1;
	frominfo.module=module;
	frominfo.dsnformat=0;
	frominfo.envid=NULL;
	frominfo.receivedspfhelo=NULL;
	frominfo.receivedspfmailfrom=NULL;
	frominfo.mailfrom_passed_spf=0;
	frominfo.input= &input;

	if (input.getinputline(buf) < 0)
		return (1);

	size_t tab_p=buf.find('\t');

	if (tab_p != buf.npos)
	{
		std::string opts=buf.substr(tab_p+1);

		buf=buf.substr(0, tab_p);

		tab_p=opts.find('\t');

		if (tab_p != opts.npos)
		{
			envid=opts.substr(tab_p+1);
			frominfo.envid= &envid;
			opts=opts.substr(0, tab_p);

			stripdots(envid);
		}

		setmsgopts(frominfo, opts);
	}

	stripdots(buf);

	if ((q=getenv("DSNRET")) != 0)
		setmsgopts(frominfo, q);

	const char *tcpremoteip=getenv("TCPREMOTEIP");
	int bofhcheckhelo=(q=getenv("BOFHCHECKHELO")) == NULL ? 0:atoi(q);

	if (strcmp(module->name, "esmtp") == 0 && receivedfrommta &&
	    strncmp(receivedfrommta, "dns; ", 5) == 0 && tcpremoteip)
	{
		const char *s=receivedfrommta+5;

		while (s && *s != ' ')
			++s;

		frominfo.helohost=std::string(receivedfrommta+5, s);

		q=getenv("BOFHSPFTRUSTME");

		if (q && atoi(q) && getenv("RELAYCLIENT"))
		{
			putenv(bofh1);
			putenv(bofh2);
			putenv(bofh3);
		}
	}
	else
	{
		bofhcheckhelo=0;
		putenv(bofh1);
		putenv(bofh2);
		putenv(bofh3);
	}

	if (tcpremoteip && frominfo.helohost.size())
	{
		const char *result;
		char errmsg[256];

		if (!bofh_checkspf("BOFHSPFHELO", "off", "off"))

		{
			errmsg[0]=0;
			if (ipcmp(tcpremoteip, frominfo.helohost.c_str()) == 0)
				result="pass";
			/* Consider HELO=ip.address a PASS */
			else
			{
				result=my_spf_lookup("HELO",
						     frominfo.helohost.c_str(),
						     tcpremoteip,
						     frominfo.helohost.c_str(),
						     &receivedspfhelo,
						     errmsg,
						     sizeof(errmsg));

				if (strcmp(result, "pass") == 0)
					bofhcheckhelo=0;

				std::string errmsg_buf=
					get_spf_errmsg("BOFHSPFHELO", result,
						       errmsg,
						       frominfo.helohost
						       .c_str());

				if (errmsg_buf.size() > 0)
				{
					std::cout << errmsg_buf << std::endl;
					return 1;
				}
				frominfo.receivedspfhelo=&receivedspfhelo;
			}
		}

		if (buf.size() &&
		    !bofh_checkspf("BOFHSPFMAILFROM", "off", "off"))

		{
			errmsg[0]=0;

			result=my_spf_lookup("MAILFROM",
					     buf.c_str(),
					     tcpremoteip,
					     frominfo.helohost.c_str(),
					     &receivedspfmailfrom,
					     errmsg,
					     sizeof(errmsg));

			std::string errmsg_buf=
				get_spf_errmsg("BOFHSPFMAILFROM", result,
					       errmsg, buf.c_str());

			if (errmsg_buf.size() > 0)
			{
				std::cout << errmsg_buf << std::endl;
				return 1;
			}
			frominfo.receivedspfmailfrom=&receivedspfmailfrom;
			if (strcmp(result, "pass") == 0 ||
			    strcmp(result, "error") == 0)
				frominfo.mailfrom_passed_spf=1;
		}

	}

	if (bofhcheckhelo &&
	    getenv("RELAYCLIENT") == NULL)
	{
		if (ipcmp(frominfo.helohost.c_str(), tcpremoteip))
		{
			if (checkmx(frominfo.helohost.c_str(), tcpremoteip,
				    bofhcheckhelo))
				return 1;
		}
	}

	sender=buf;

	if ((!frominfo.envid || frominfo.envid->size()== 0)
	    && (p=getenv("DSNENVID")) && *p)
	{
		for (envid=""; *p; p++)
		{
			if (*p < '!' || *p > '~' || *p == '+' ||
				*p == '=')
			{
			static char hexchar[17]="0123456789ABCDEF";

				envid += '+';
				envid += hexchar[ ((*p) >> 4) & 15];
				envid += hexchar[ (*p) & 15];
			}
			else	envid += *p;
		}
		frominfo.envid= &envid;
	}

	struct	rfc822t *rfcp=rw_rewrite_tokenize(sender.c_str());
	struct	rw_info	rwi;

	rw_info_init(&rwi, rfcp->tokens, mailfromerr);

	rwi.mode=RW_ENVSENDER|RW_SUBMIT;
	rwi.sender=NULL;
	rwi.udata=(void *)&frominfo;
	rwi.smodule=module->name;

	rw_rewrite_module(module, &rwi, getrcpts);
	rfc822t_free(rfcp);

	return (frominfo.flag);
}

static int checkfreemail(const char *, struct bofh_list *b);

static int checkdns(const char *sender,
		    struct mailfrominfo *mf)
{
	struct	rfc1035_mxlist *mxlist, *p;
	const char *q;
	int	docheckdomain=1;
	struct bofh_list *d;
	struct rfc1035_res res;
	int rc;

	if (bofh_chkbadfrom(sender))
	{
		std::cout << "517 Sender rejected: " << sender << std::endl << std::flush;
		return (-1);
	}

	if (strcmp(mf->module->name, "esmtp") == 0
	    && getenv("RELAYCLIENT") == 0
	    && (d=bofh_chkfreemail(sender)) != NULL)
	{
		q=getenv("TCPREMOTEIP");
		if (q && *q)
		{
			q=getenv("TCPREMOTEHOST");

			if (!q || checkfreemail(q, d))
			{
				std::cout << "517-Sender rejected: " << sender
				     << " can be accepted only from" << std::endl
				     << "517 " << d->name << "'s mail relays."
				     << std::endl << std::flush;
				free(d);
				return (-1);
			}
		}
	}

	q=getenv("BOFHCHECKDNS");
	if (!q || !*q || *q == '0')
		docheckdomain=0;

	{
	const char *p=getenv("BLOCK");

		if (p && *p)
			docheckdomain=0; /* Will be rejected later. */
	}

	if (!docheckdomain)	return (0);

	if ((sender=strrchr(sender, '@')) == 0)	return (0);
	++sender;

	rfc1035_init_resolv(&res);
        rc=rfc1035_mxlist_create_x(&res, sender,
				   RFC1035_MX_AFALLBACK |
				   RFC1035_MX_IGNORESOFTERR,
				   &mxlist);
	rfc1035_destroy_resolv(&res);

	switch (rc)	{
	case RFC1035_MX_OK:

		for (p=mxlist; p; p=p->next)
		{
		RFC1035_ADDR	addr;
		char	buf[RFC1035_NTOABUFSIZE];

			if (rfc1035_sockaddrip(&p->address,
					sizeof(p->address), &addr))
				continue;

			rfc1035_ntoa(&addr, buf);
			if (strcmp(buf, p->hostname) == 0)
				break;

			if (bofh_chkbadmx(buf))
			{
				std::cout << "517 Mail server at " << buf
				     << " is blocked." << std::endl;
				rfc1035_mxlist_free(mxlist);
				return (-1);
			}
		}
		rfc1035_mxlist_free(mxlist);
		if (p == 0)	return (0);
		std::cout << "517-MX records for " << sender << " violate section 3.3.9 of RFC 1035." << std::endl;
		break;
	case RFC1035_MX_HARDERR:
		if (!docheckdomain)	return (0);
		std::cout << "517-Domain does not exist: " << sender << "." << std::endl;
		break;
	case RFC1035_MX_BADDNS:
		if (!docheckdomain)	return (0);
		std::cout << "517-RFC 1035 violation: recursive CNAME records for " << sender << "." << std::endl;
		break;
	default:
		if (!docheckdomain)	return (0);
		std::cout << "417 DNS lookup failure: " << sender << ".  Try again later." << std::endl << std::flush;
		return (-1);
	}

	std::cout << "517 Invalid domain, see <URL:ftp://ftp.isi.edu/in-notes/rfc1035.txt>" << std::endl << std::flush;
	return (-1);
}

static int do_checkfreemail(const char *, const char *);

static int checkfreemail(const char *host, struct bofh_list *p)
{
	if (do_checkfreemail(host, p->name) == 0)
		return (0);

	for (p=p->aliases; p; p=p->next)
		if (do_checkfreemail(host, p->name) == 0)
			return (0);
	return (1);
}

static int do_checkfreemail(const char *host, const char *domain)
{
	int hl, dl;

	if (strcmp(host, "softdnserr") == 0)
		return (0);

	if (strcasecmp(host, domain) == 0)
		return (0);
	hl=strlen(host);
	dl=strlen(domain);
	if (dl < hl)
	{
		host  += (hl-dl);
		if (host[-1] == '.' && strcasecmp(host, domain) == 0)
			return (0);
	}
	return (1);
}

struct rcptinfo;

class RcptAliasHandler : public AliasHandler {
public:
	RcptAliasHandler();
	~RcptAliasHandler();
	void	Alias(const char *);
	SubmitFile *submitptr;
	struct rcptinfo *rcptinfoptr;
	std::string	listname;
	unsigned listcount;
	std::string	aliasbuf;

	std::string	abortmsg;
	int aborted;
	int ret_code;

	struct rfc822token *addresst, *sendert;
	struct rw_transport *sourcemodule;
} ;

struct	rcptinfo {
	struct rw_transport *module;
	struct rfc822token *sendert;
	std::string prw_receipient;	/* Pre-rewrite recipient */
	std::string *oreceipient;	/* DSN original receipient */
	std::string *dsn;		/* DSN flags */
	AliasSearch *asptr;
	int has_receipient;
	RcptAliasHandler rcptalias;
	SubmitFile submitfile;
	std::string	errmsgtext;
	int	errflag;
	unsigned rcptnum;
	int	whitelisted_only;	/* Spam filter - allow whitelisted
					** recipients only.
					*/
	int	nonwhitelisted_only;	/* Spam filter - allow nonwhitelisted
					** recipients only.
					*/
	} ;

RcptAliasHandler::RcptAliasHandler()
{
}

RcptAliasHandler::~RcptAliasHandler()
{
}

static int do_receipient_filter(struct rw_info *, struct rw_info_vrfy *,
				std::string &);

static const char *trackaddress(const char *p)
{
	time_t timestamp;
	int results;

	if (suppressbackscatter)
	{
		results=track_find_email(p, &timestamp);

		if (results == TRACK_ADDRDEFERRED ||
		    results == TRACK_ADDRFAILED)
		{
			if (timestamp >= time(NULL) - TRACK_NHOURS * 60 * 60)
			{
				return results == TRACK_ADDRDEFERRED ?
					"456 Address temporarily unavailable.":
					"556 Address unavailable.";
			}
		}
	}

	return NULL;
}

void RcptAliasHandler::Alias(const char *p)
{
	const char *trackerrmsg;

	if (aborted)	return;

	if ((trackerrmsg=trackaddress(p)) != NULL)
	{
		aborted=1;
		abortmsg=trackerrmsg;
		return;
	}

	if (listcount == 0)
	{
	struct rw_info	rwi;
	struct rw_info_vrfy riv;
	std::string	errmsg;

		riv.rwr.buf=0;
		riv.rwr.errmsg=0;
		riv.module=0;
		riv.host=0;
		riv.addr=0;

		rw_info_init(&rwi, addresst, rw_err_func);
		rwi.sender=sendert;
		rwi.mode=RW_SUBMIT;
		rwi.udata= (void *)&riv;
		rwi.smodule=sourcemodule->name;

		riv.module=rw_search_transport("local");
		riv.rwr.errmsg=0;

	const char *cfa=config_filteracct();

		// Ok, we want the "host" portion to be
		// alias!alias-address!uid!gid!cfa!!

	int	rc=0;

		if (cfa && *cfa)
		{
		struct	stat	stat_buf;
		static char emptystr[]="";

			if (stat(cfa, &stat_buf))
			{
				aborted=1;
				abortmsg="400 stat() failed on aliasfilteracct.";
				return;
			}

		char	uidbuf[NUMBUFSIZE], gidbuf[NUMBUFSIZE];

			libmail_str_uid_t(stat_buf.st_uid, uidbuf);
			libmail_str_gid_t(stat_buf.st_gid, gidbuf);

	char	*fa=(char *)courier_malloc(strlen(cfa)+strlen(uidbuf)+
					   strlen(gidbuf)+listname.size()+
				sizeof("alias!alias-!!!!!"));

			strcpy(fa, "alias!alias-");
			strcat(fa, listname.c_str());
			strcat(fa, "!");
			strcat(fa, uidbuf);
			strcat(fa, "!");
			strcat(fa, gidbuf);
			strcat(fa, "!");
			strcat(fa, cfa);
			strcat(fa, "!!");

			riv.host=fa;
			riv.addr=emptystr;	// Unused, for now

			rc=riv.module ?
				do_receipient_filter(&rwi, &riv, errmsg):0;
			free(fa);
		}

		ret_code=rc;

		if (rc)
		{
			if (rc != 99)
			{
				aborted=1;
				abortmsg=errmsg;
			}

			if (rcptinfoptr->whitelisted_only)
			{
				aborted=1;
				abortmsg=you_are_not_whitelisted;
			}
			else
				rcptinfoptr->nonwhitelisted_only=1;
		}
		else
		{
			if (rcptinfoptr->nonwhitelisted_only)
			{
				aborted=1;
				abortmsg=you_are_whitelisted;
			}
			else
				rcptinfoptr->whitelisted_only=1;
		}

		if (aborted)	return;

		aliasbuf=p;
		++listcount;
		return;
	}

	if (listcount == 1)
	{
		if (submitptr->ChkRecipient(aliasbuf.c_str()) == 0)
			submitptr->AddReceipient(aliasbuf.c_str(), "", "N", 0);
	}

	if (submitptr->ChkRecipient(p) == 0)
		submitptr->AddReceipient(p, "", "N", 0);
	++listcount;
}

static bool read_next_line(Input &input, std::string &str)
{
	char buf[5000];

	size_t i;

	for (i=0; i<sizeof(buf); i++)
	{
		int c=input.getinputchar();

		if (c == EOF)
		{
			if (i == 0)
				return false;
			break;
		}

		if (c == '\n')
		{
			input.unputinputchar(c);
			break;
		}
		buf[i]=(char)c;
	}

	str=std::string(buf, buf+i);
	return true;
}

static void getrcpt(struct rw_info *rwi);
static void rcpttoerr(int, const char *, struct rw_info *);

static inline char* getenv_subvar(char const *first, char const *second)
{
	char *buf=(char *)courier_malloc(strlen(first) + strlen(second) + 2);
	char *p;

	sprintf(buf, "%s_%s", first, second);
	p= getenv(buf);
	free(buf);
	return p;
}

static std::string a_r_from_env(const std::string& cme_name)
{
	std::string a_r;
	char *p;
	int found;

	for (found=0; (p=getenv_allow(found)) != 0; ++found)
	{
		char *zone = getenv_subvar(p, "ZONE");
		char *ip = getenv_subvar(p, "IP");
		char *txt = getenv_subvar(p, "TXT");

		if (zone && (ip || txt))
		{
			if (a_r.size() <= 0)
			{
				a_r = "Authentication-Results: ";
				a_r += cme_name;
			}
			a_r += ";\n    dnswl=pass dns.zone=";
			a_r += zone;

			if (ip)
			{
				a_r += "\n    policy.ip=";
				a_r += ip;
			}

			/*
			* Although a value can be token or quoted-string,
			* we (arbitrarily) restrict the text to a token.
			*/
			if (txt)
			{
				const char *p;

				a_r += "\n    policy.txt=\"";

				for (p=txt; *p; p++)
				{
					if (*p & 0x80 || *p < ' ' ||
					    *p == '"' || *p == '\\')
						break;
				}

				if (!*p)
				{
					a_r += txt;
				}
				else
				{
					char *q=rfc2047_encode_str(txt,
								   "UTF-8",
								   rfc2047_qp_allow_word);

					if (q)
					{
						a_r += q;
						free(q);
					}
					else
					{
						while (*txt)
						{
							if ((*txt & 0x80) == 0
							    && *txt != '"'
							    && *txt != '\\')
								a_r += *txt;
							++txt;
						}
					}
				}
				a_r += '"';
			}
		}
	}
	if (a_r.size())
		a_r += "\n";
	return a_r;
}

static void getrcpts(struct rw_info *rwi)
{
	struct mailfrominfo *mf=(struct mailfrominfo *)rwi->udata;

	Input &input= *mf->input;

	struct rfc822token *addresst=rwi->ptr;
	unsigned hasrcpts=0;

	if (rw_syntaxchk(addresst))
	{
		std::cout << "517 Syntax error." << std::endl << std::flush;
		return;
	}

char	*sender=rfc822_gettok(addresst);

	if (!sender)	clog_msg_errno();

	if (checkdns(sender, mf))	return;

	std::cout << "250 Ok." << std::endl << std::flush;

	mf->asptr->Open(mf->module);

	struct	rcptinfo my_rcptinfo;
	std::string	receipient;
	std::string	oreceipient;
	std::string	dsn;

	my_rcptinfo.module=mf->module;
	my_rcptinfo.sendert=addresst;
	my_rcptinfo.asptr=mf->asptr;
	my_rcptinfo.has_receipient=0;
	my_rcptinfo.rcptalias.submitptr= &my_rcptinfo.submitfile;
	my_rcptinfo.rcptalias.rcptinfoptr= &my_rcptinfo;

	my_rcptinfo.submitfile.SendingModule( mf->module->name );

	my_rcptinfo.submitfile.Sender(receivedfrommta, sender,
			(mf->envid && mf->envid->size() > 0 ?
			 mf->envid->c_str(): (const char *)0),
			mf->dsnformat);
	my_rcptinfo.oreceipient= &oreceipient;
	my_rcptinfo.dsn= &dsn;
	my_rcptinfo.rcptnum=0;
	my_rcptinfo.whitelisted_only=0;
	my_rcptinfo.nonwhitelisted_only=0;

	free(sender);


	if (strcmp(mf->module->name, "local") == 0 ||
	    strcmp(mf->module->name, "dsn") == 0)
		input.setSubmitFile(my_rcptinfo.submitfile);

	for (;; )
	{
		if (input.getinputline(receipient) < 0)
			return;

		if (receipient.size() == 0)
			break;

		if (hasrcpts >= max_bofh)
		{
			std::cout << (max_bofh_ishard
				 ? "531 Too many recipients."
				 : "431 Too many recipients.")
			     << std::endl << std::flush;
			continue;
		}

		oreceipient="";
		dsn="";

		size_t tab_p=receipient.find('\t');

		if (tab_p != receipient.npos)
		{
			dsn=receipient.substr(tab_p+1);
			receipient=receipient.substr(0, tab_p);

			tab_p=dsn.find('\t');

			if (tab_p != dsn.npos)
			{
				oreceipient=dsn.substr(tab_p+1);
				dsn=dsn.substr(0, tab_p);

				stripdots(oreceipient);
			}
		}

		stripdots(receipient);

		if (receipient.size() == 0 || receipient[0] == '@')
		{
			std::cout << "500 Invalid address" << std::endl
				  << std::flush;
			continue;
		}

		struct	rw_info rwi;
		struct	rfc822t *rfcp;

		rfcp=rw_rewrite_tokenize(receipient.c_str());
		rw_info_init(&rwi, rfcp->tokens, rcpttoerr);
		rwi.mode=RW_ENVRECIPIENT|RW_SUBMIT;
		rwi.sender=addresst;
		rwi.smodule=mf->module->name;
		rwi.udata=(void *)&my_rcptinfo;
		my_rcptinfo.errflag=0;
		my_rcptinfo.prw_receipient=receipient;

		rw_rewrite_module(mf->module, &rwi, getrcpt);

		// CANNOT reject if errflag=0 any more, due to filtering
		// syncronization.

		rfc822t_free(rfcp);
		++hasrcpts;

		if (my_rcptinfo.errflag > 0)
		{
			const char *errmsg=my_rcptinfo.errmsgtext.c_str();
			int	l=atoi(errmsg);

			if (l < 400 || l > 599)	l=511;
			mailfromerr(l, errmsg, 0);
		}
		else
		{
			std::cout << "250 Ok." << std::endl << std::flush;
			my_rcptinfo.has_receipient=1;
			++my_rcptinfo.rcptnum;
		}
	}

	if (hasrcpts)
	{
		if (!my_rcptinfo.has_receipient)	return;
	}

	// Read headers

	std::string spf_from_address="";
	std::string	header, headername, headernameorig;
	std::string	line;
	std::string	accumulated_errmsg;
	std::string	cme_name = config_me();
	int	has_msgid=0;
	int	has_date=0;
	int	has_tocc=0;
	size_t	headercnt=500;
	size_t	headerlimit=100000;
	std::string::iterator line_iter;
	int	noaddrrewrite=0;
	int	a_r_guarded=0;
	const	char *p;

	p=getenv("NOADDRREWRITE");

	if (p)
	{
		noaddrrewrite=atoi(p);
	}

	p=getenv("ALLOW_EXCLUSIVE");

	if (p)
		a_r_guarded=atoi(p);

	p=getenv("BOFHHEADERLIMIT");

	if (p)
	{
		int n=atoi(p);

		if (n < 1000)
		    n=1000;
		headerlimit=n;
	}

	my_rcptinfo.submitfile.MessageStart();

	line="Received: from ";

	for (p=strchr(receivedfrommta, ';'); *++p == ' '; )
		;

	line += p;

	if (identinfo && *identinfo)
	{
		line += "\n  (";
		line += identinfo;
		line += ')';
	}

	line += "\n  by ";
	line += cme_name;

	line += " with ";

	if (rfc3848_receivedwith && *rfc3848_receivedwith)
		line += rfc3848_receivedwith;
	else
		line += mf->module->name;
	line += "; ";

	line += rfc822_mkdate(submit_time);

	// Add unique id here.
	line += "\n  id ";
	line += my_rcptinfo.submitfile.QueueID();
	line += "\n";

	for (line_iter=line.begin(); line_iter != line.end(); ++line_iter)
		if ( (*line_iter & 0x80) != 0)
		{
			char *s=rfc2047_encode_str(line.c_str(),
						   RFC2045CHARSET,
						   rfc2047_qp_allow_any);

			if (!s)
			{
				perror("500 ");
				exit(1);
			}

			try
			{
				line=s;
			}
			catch (...)
			{
				perror("500 ");
				exit(1);
			}
			free(s);
			break;
		}

	if (mf->receivedspfhelo)
		my_rcptinfo.submitfile.Message(mf->receivedspfhelo->c_str());
	if (mf->receivedspfmailfrom)
		my_rcptinfo.submitfile.Message(mf->receivedspfmailfrom
					       ->c_str());
	my_rcptinfo.submitfile.Message(line.c_str());

	/* Add Authentication-Results header */
	line=a_r_from_env(cme_name);
	if (line.size() > 0)
		my_rcptinfo.submitfile.Message(line.c_str());

	unsigned	received_cnt=0;

	while (read_next_line(input, line))
	{
		struct	rfc822t *rfcp;
		struct	rfc822a	*rfca;
		int	i;
		size_t	l;
		size_t	headerl;

		size_t colon_pos;

		if ((i=input.getinputchar()) != EOF && i != '\n')
		{
			headercnt=0;
			continue;	// Swallow unwanted message
		}

		l=line.size();

		if (l == 0)
		{
			if (i == '\n')
				input.unputinputchar('\n');
			break;
		}

		if (l && line[l-1] == '\r')
			line=line.substr(0, l-1); // Strip trailing CR

		header=line;

		while ( ((i=input.getinputchar()) != EOF
			 ? input.unputinputchar(i):i) == ' '
			|| i == '\t')
		{
			if (!read_next_line(input, line))
			{
				headercnt=0;
				break;
			}

			if ((i=input.getinputchar()) != EOF && i != '\n')
			{
				headercnt=0;
				continue;	// Swallow unwanted message
			}
			l=line.size();
			if (l && line[l-1] == '\r')
				line=line.substr(0,l-1); // Strip trailing CR
			l=line.size() + header.size();
			if (l > headerlimit)
				headercnt=0;
			if (headercnt == 0)	continue;
			header += "\n";
			header += line;
			if (i == EOF)	break;
		}
		header += "\n";
		if (header.size() > headerlimit)
			headercnt=0;

		if (headercnt == 0 || --headercnt == 0)	continue;
		headerlimit -= (headerl=header.size());

		colon_pos=header.find(':');

		if (colon_pos == header.npos)
		{
			my_rcptinfo.submitfile.Message(header.c_str());
			continue;
		}

		headernameorig=headername=header.substr(0, colon_pos);
		header=header.substr(colon_pos+1);
		std::transform(headername.begin(),
			       headername.end(),
			       headername.begin(),
			       std::ptr_fun(::tolower));

		if (headername == "received")	++received_cnt;

		else if (noaddrrewrite > 1 && headername == "dkim-signature")
			noaddrrewrite = 1;

		//
		// If no receipient were listed, grab them from the header.
		//

		if (!hasrcpts && (headername == "bcc" ||
			(onlybcc == 0 && (
			headername == "to" ||
			headername == "cc"))))
		{
			rfcp=rw_rewrite_tokenize(header.c_str());
			rfca=rfc822a_alloc(rfcp);
			if (!rfca)	clog_msg_errno();

			for (i=0; i<rfca->naddrs; i++)
			{
				if (rfca->addrs[i].tokens == NULL)
					continue;

			struct	rw_info rwi;

				rw_info_init(&rwi, rfca->addrs[i].tokens,
								rcpttoerr);
				rwi.mode=RW_ENVRECIPIENT|RW_SUBMIT;
				rwi.smodule=mf->module->name;
				rwi.sender=addresst;
				rwi.udata=(void *)&my_rcptinfo;
				my_rcptinfo.errflag=0;
				(*my_rcptinfo.oreceipient) = "";
				(*my_rcptinfo.dsn) = "";

			char	*p=rfc822_gettok(rfca->addrs[i].tokens);
				my_rcptinfo.prw_receipient=p;
				free(p);

				rw_rewrite_module(mf->module, &rwi, getrcpt);

		// CANNOT reject if errflag=0 any more, due to filtering
		// syncronization.

				if (my_rcptinfo.errflag > 0)
				{
					accumulated_errmsg +=
						my_rcptinfo.errmsgtext;
				}
				else
				{
					my_rcptinfo.has_receipient=1;
					++my_rcptinfo.rcptnum;
				}
			}
			rfc822a_free(rfca);
			rfc822t_free(rfcp);
		}

		// Rewrite every RFC822 header we can find.

		if (!hasrcpts && headername == "bcc")
		{
			// ... except this one, if we're reading addresses
			// from the headers.

			headerlimit += headerl;
			++headercnt;
			continue;
		}

	int	istocc=0;

		if (headername == "to" || headername == "cc")
		{
			istocc=1;
			has_tocc=1;
		}

		if (istocc || headername == "from" ||
			headername == "reply-to" ||
			headername == "sender")
		{
			// The signature SHOULD be prepended to the message.

			if (noaddrrewrite > 1)
				noaddrrewrite = 0;
			if (!noaddrrewrite)
			{

				char	*errmsg;
				char	*new_header=
					rw_rewrite_header(mf->module,
							  header.c_str(),
							  RW_HEADER|RW_SUBMIT,
							  addresst, &errmsg);

				if (!new_header)
				{
					accumulated_errmsg += errmsg;
					free(errmsg);
				}
				else
				{
					const char *p;
					char	*q;

					header="";
					p=" ";
					for (q=new_header;
					     (q=strtok(q, "\n")) != 0; q=0)
					{
						header += p;
						header += q;
						p="\n    ";
					}
					free(new_header);
					header += '\n';
				}
			}
			if (headername == "from" &&
			    !bofh_checkspf("BOFHSPFFROM", "off", "off"))
			{
				struct rfc822t *t=
					rfc822t_alloc_new(header.c_str(),
							  NULL, NULL);

				if (!t)
					clog_msg_errno();

				struct rfc822a *a=rfc822a_alloc(t);
				if (!a)
					clog_msg_errno();

				if (a->naddrs > 0 && a->addrs[0].tokens)
				{
					char *s=rfc822_getaddr(a, 0);

					if (!s)
						clog_msg_errno();

					spf_from_address=s;
					free(s);
				}
				rfc822a_free(a);
				rfc822t_free(t);

				if (mf->mailfrom_passed_spf &&
				    bofh_checkspf("BOFHSPFFROM", "off",
						  "mailfromok"))
				{
					spf_from_address="";
				}

			}

		}
		else if (headername == "message-id")	has_msgid=1;
		else if (headername == "date")	has_date=1;

		// Quote Return-Path:'s at this point

 		else if (headername == "return-path")
 			headernameorig="Old-"+headernameorig;

 		// Rename Received-SPF only if own SPF checking is done

 		else if (headername == "received-spf" &&
		    (mf->helohost.size() > 0) &&
		    !(bofh_checkspf("BOFHSPFHELO", "off", "off") &&
		      bofh_checkspf("BOFHSPFMAILFROM", "off", "off") &&
		      bofh_checkspf("BOFHSPFFROM", "off", "off")))
  			headernameorig="Old-"+headernameorig;

		// Quote A-R if the auth server-id is guarded

		else if (a_r_guarded > 0 &&
			 headername == "authentication-results")
			headernameorig="Old-"+headernameorig;

		my_rcptinfo.submitfile.Message(headernameorig.c_str());
		my_rcptinfo.submitfile.Message(":");
		my_rcptinfo.submitfile.Message(header.c_str());
	}

	if (!my_rcptinfo.has_receipient)
	{
		accumulated_errmsg += "511 Headers specify no receipients.\n";
	}

	if (headercnt > 0)
	{
		if (spf_from_address.size() > 0)
		{
			std::string received_spf;
			char errmsg[256];

			const char *result=my_spf_lookup("FROM",
							 spf_from_address
							 .c_str(),
							 getenv("TCPREMOTEIP"),
							 mf->helohost.c_str(),
							 &received_spf,
							 errmsg,
							 sizeof(errmsg));

			std::string errmsg_buf=
				get_spf_errmsg("BOFHSPFFROM", result,
					       errmsg, spf_from_address
					       .c_str());

			if (errmsg_buf.size() > 0)
			{
				while (input.getinputchar() != EOF)
					;
				mf->flag=1;
				std::cout << errmsg_buf << std::endl;
				return;
			}
			my_rcptinfo.submitfile.Message(received_spf.c_str());
		}
	}
	if (accumulated_errmsg.size())
	{
		int	n=atoi(accumulated_errmsg.c_str());

		if ( (n / 100) != 4 && (n / 100) != 5)	n=511;

		while (input.getinputchar() != EOF)
			;

		char	*buf=makeerrmsgtext(n, accumulated_errmsg.c_str());

		std::cout << buf << std::flush;
		free(buf);
		mf->flag=1;
		return;
	}

	if (received_cnt > 100)
		headercnt=0;

	if (headercnt)
	{
		if (!has_msgid)
		{
		const char *p=getenv("NOADDMSGID");

			if (!p || *p == '0')
				my_rcptinfo.submitfile
					.Message(mkmessageidheader().c_str());
		}

		if (!has_date)
		{
		const char *p=getenv("NOADDDATE");

			if (!p || *p == '0')
			{
				header="Date: ";
				header += rfc822_mkdate(submit_time);
				header += '\n';
				my_rcptinfo.submitfile.Message(header.c_str());
			}
		}
		if (!has_tocc)
			my_rcptinfo.submitfile.Message(
				"To: undisclosed-recipients: ;\n");
	}

	while (read_next_line(input, line))
	{
		int	i, l;

		if ((i=input.getinputchar()) != EOF && i != '\n')
			headercnt=0;
		if (headercnt == 0)	continue;
		l=line.size();
		if (l && line[l-1] == '\r')
			line=line.substr(0, l-1);	// Strip trailing CR

		if (i != EOF || line.size())
		{
			my_rcptinfo.submitfile.Message(line.c_str());
			my_rcptinfo.submitfile.Message("\n");
		}
		if (i == EOF)	break;
	}

	if (received_cnt > 100)
	{
		std::cout << "546 Routing loop detected -- too many Received: headers." << std::endl << std::flush;
		mf->flag=1;
	}
	else if (headercnt == 0)
	{
		std::cout << "534 Message header size exceeds policy limit."
			  << std::endl << std::flush;
		mf->flag=1;
	}
	else
	{
		alarm(0);	/* Cancel alarm() -- we'll wrap up soon */

		/* Make sure bounces go through */

		mf->flag=my_rcptinfo.submitfile.MessageEnd(my_rcptinfo.rcptnum,
			my_rcptinfo.whitelisted_only, !nofilter);
	}
}

static void rcpttoerr(int msgnum, const char *errtext, struct rw_info *rwi)
{
struct rcptinfo *my_rcptinfo=(struct rcptinfo *)rwi->udata;

char	*errmsg=makeerrmsgtext(msgnum, errtext);

	if (errmsg)
	{
		my_rcptinfo->errflag=1;
		my_rcptinfo->errmsgtext=errmsg;
		free(errmsg);
	}
}

static std::string checkrcpt(struct rcptinfo *,
	struct rfc822token *,
	const char *);

static void getrcpt(struct rw_info *rwi)
{
struct rcptinfo *my_rcptinfo=(struct rcptinfo *)rwi->udata;
struct rfc822token *addresst=rwi->ptr;
char	*address=rfc822_gettok(addresst);

	if (!address)
		clog_msg_errno();

	std::string	errmsg;

	domainlower(address);
	errmsg=checkrcpt(my_rcptinfo, addresst, address);
	my_rcptinfo->errflag=0;

	if (errmsg.size())
	{
		my_rcptinfo->errmsgtext=errmsg;
		my_rcptinfo->errflag=1;
	}
	free(address);
}

static std::string checkrcpt(struct rcptinfo *my_rcptinfo,
			     struct rfc822token *addresst,
			     const char *address)
{
	struct rw_transport *source=my_rcptinfo->module;
	struct rfc822token *sendert=my_rcptinfo->sendert;
	const char *oaddress=my_rcptinfo->oreceipient->c_str();
	const char *dsn=my_rcptinfo->dsn->c_str();
	AliasSearch &aliasp=*my_rcptinfo->asptr;
	RcptAliasHandler &handlerp=my_rcptinfo->rcptalias;
	unsigned rcptnum=my_rcptinfo->rcptnum;
	const char *prw_receipient= my_rcptinfo->prw_receipient.c_str();

	std::string	errmsg;
	char	envdsn[5];
	int	dsnN, dsnS, dsnF, dsnD;
	char	*addressl;
	std::string oaddressbuf;

	char	deliv_envdsn[5];
	std::string deliv_addressl;
	std::string deliv_oaddress;

	if (!oaddress)	oaddress="";
	if (!dsn)	dsn="";

	strcpy(deliv_envdsn, "N");
	dsnN=0;
	dsnS=0;
	dsnF=0;
	dsnD=0;

	if (!*dsn && (dsn=getenv("DSNNOTIFY")) != 0)
		while (*dsn)
		{
			switch (*dsn)	{
			case 'n':
			case 'N':
				dsnN=1;
				break;
			case 's':
			case 'S':
				dsnS=1;
				break;
			case 'f':
			case 'F':
				dsnF=1;
				break;

			case 'd':
			case 'D':
				dsnD=1;
				break;
			}
			while (*dsn && *dsn != ',' && *dsn != ' ')
				++dsn;
			if (*dsn)	++dsn;
		}
	else if (dsn)
		for ( ; *dsn; dsn++)
		{
			switch (*dsn)	{
			case 'n':
			case 'N':
				dsnN=1;
				break;
			case 's':
			case 'S':
				dsnS=1;
				break;
			case 'f':
			case 'F':
				dsnF=1;
				break;

			case 'd':
			case 'D':
				dsnD=1;
				break;
			}
		}

	strcpy(envdsn, "");

	if (dsnN)
		strcpy(envdsn, "N");
	else
	{
		if (dsnS) strcat(envdsn, "S");
		if (dsnF) strcat(envdsn, "F");
		if (dsnD) strcat(envdsn, "D");
	}

	handlerp.listname=address;	// Just in case
	handlerp.listcount=0;
	handlerp.aborted=0;
	handlerp.ret_code=0;
	handlerp.addresst=addresst;
	handlerp.sendert=sendert;
	handlerp.sourcemodule=source;

	if (checkfreespace(0))
	{
		errmsg="431 Mail system full.";
		return (errmsg);
	}

	{
	const char *p=getenv("BLOCK");

		if (p && *p)
		{
			errmsg=p;
			return (errmsg);
		}
	}

	addressl=strcpy((char *)courier_malloc(strlen(address)+1), address);
	locallower(addressl);

	int search_rc=aliasp.Search(addressl, handlerp);

	if (search_rc < 0)
	{
		handlerp.abortmsg="400 Service temporarily unavailable.";
		return (handlerp.abortmsg);
	}

	if (search_rc ||
		handlerp.listcount == 1 ||
		handlerp.aborted)
	{
	struct rw_info	rwi;
	struct rw_info_vrfy riv;
	struct rfc822t *aliasaddresst=0;
	int	rwmode=RW_SUBMIT;
	const char *trackerrmsg;

		if (!handlerp.aborted &&
		    (trackerrmsg=trackaddress(address)) != NULL)
		{
			handlerp.aborted=1;
			handlerp.abortmsg=trackerrmsg;
		}

		if (handlerp.aborted)
		{
			return (handlerp.abortmsg);
		}

		/*
		** For aliases of exactly one address, we just replace
		** addresst with the tokenized alias.
		**
		** Note - if original address is not set, it is set to
		** the alias address.
		*/

		if (handlerp.listcount == 1)
		{
			rwmode |= RW_SUBMITALIAS;

			if (verpflag && strcmp(envdsn, "N"))

			/* This is effectively a rewrite of a VERPed
			** recipient, so the buck stops here.
			*/
			{
				strcpy(deliv_envdsn, envdsn);
				deliv_addressl=addressl;
				deliv_oaddress=oaddress;
					/* Generate a "delivered" notice if
					** a DSN was requested */

				strcpy(envdsn, "N");
					/* No errors for this one */
			}

			aliasaddresst=rw_rewrite_tokenize(handlerp.aliasbuf
							  .c_str());
			if (!aliasaddresst)	clog_msg_errno();

			if (*oaddress == 0)
			{
			char	*p=dsnencodeorigaddr(address);

				oaddressbuf=p;
				free(p);
				oaddress=oaddressbuf.c_str();
			}

			addresst=aliasaddresst->tokens;
			address=handlerp.aliasbuf.c_str();
		}

		/*
		**  If the module functions rewrote the recipient address, and
		**  the verp flag is set, we need to make sure to generate a
		**  DSN for final delivery, and suppress any more DSNs for
		**  this recipient, from now on, because the sender will not
		**  have any idea that the address has been aliased.
		*/

		else if (verpflag && strcmp(address, prw_receipient) &&
			strcmp(envdsn, "N"))
		{
			strcpy(deliv_envdsn, envdsn);
			deliv_addressl=addressl;
			deliv_oaddress=oaddress;
			strcpy(envdsn, "N");
		}
		free(addressl);

		riv.rwr.buf=0;
		riv.rwr.errmsg=0;
		riv.module=0;
		riv.host=0;
		riv.addr=0;

		rw_info_init(&rwi, addresst, rw_err_func);
		rwi.sender=sendert;
		rwi.mode=rwmode;
		rwi.udata= (void *)&riv;
		rwi.smodule=source->name;

		/* Just let the DSN source through */

		if (strcmp(source->name, DSN) == 0)
		{
			riv.module=source;
			riv.rwr.errmsg=0;
			riv.host=strcpy((char *)courier_malloc(1), "");
			riv.addr=strcpy((char *)courier_malloc(
				strlen(address)+1), address);
		}
		else
		{
			if (!nofilter)
				rwi.mode |= RW_FILTER;
			rw_searchdel(&rwi, &found_transport);
		}

		if (aliasaddresst)
			rfc822t_free(aliasaddresst);

		if (!riv.module && !riv.rwr.errmsg)
			rw_err_func(511, "Unknown user.", &rwi);
		if (!riv.module)
		{
			errmsg=riv.rwr.errmsg;
			free(riv.rwr.errmsg);
			return (errmsg);
		}

		/* If we're here because of a one-alias recipient, pretend
		** that it's a whitelisted recipient */

	int	rc=handlerp.listcount == 1 ? handlerp.ret_code:
			do_receipient_filter(&rwi, &riv, errmsg);

		if (rc)
		{
			if (rc != 99)	return (errmsg);

			/*
			** Return code 99 -- nonwhitelisted recipient subject
			** to spam filtering.
			*/
			if (my_rcptinfo->whitelisted_only)
			{
				errmsg=you_are_not_whitelisted;
				return (errmsg);
			}
			my_rcptinfo->nonwhitelisted_only=1;
		}
		else
		{
			/*
			** Return code 0 - whitelisted recipient.
			*/
			if (my_rcptinfo->nonwhitelisted_only)
			{
				errmsg=you_are_whitelisted;
				return (errmsg);
			}
			my_rcptinfo->whitelisted_only=1;
		}

		// Good address, but should we add it to the list of
		// recipients?  Figure out what should be the key to check
		// for a duplicate address.
		//
		// DSN: NEVER, use module<nl>host<nl>addr, should get rid
		//      of the most number of duplicate addresses
		//
		// Every case, use module<nl>host<nl>addr<nl>orecipient.

		std::string key;
		int	dupflag=0;

		key=riv.module->name;
		key += '\n';
		key += riv.host;
		key += '\n';
		key += riv.addr;
		if (strcmp(envdsn, "N") == 0)
		{
			if (handlerp.submitptr->ChkRecipient(key.c_str()))
				dupflag=1;
		}

		key += '\n';
		key += oaddress;

		if (handlerp.submitptr->ChkRecipient(key.c_str()))
			dupflag=1;

		if (!dupflag)
		{
			if (strcmp(deliv_envdsn, "N"))
				handlerp.submitptr->
					AddReceipient(deliv_addressl.c_str(),
						      deliv_oaddress.c_str(),
						      deliv_envdsn, 1);

			handlerp.submitptr->AddReceipient(address,
					oaddress, envdsn, 0);

			/*
			** If we're handling nonwhitelisted recipients,
			** record their control file location for the
			** spam filter, to follow later.
			*/

			if (my_rcptinfo->nonwhitelisted_only
				&& handlerp.listcount != 1) /* NOT ALIASES */
				handlerp.submitptr->ReceipientFilter(
					riv.module,
					riv.host,
					riv.addr,
					rcptnum);
		}

		free(riv.host);
		free(riv.addr);
	}
	else	// Make sure aliases are properly recorded for DSN generation
		// purposes.
	{
		handlerp.submitptr->AddReceipient(addressl, oaddress, envdsn,1);
		free(addressl);
	}
	return (errmsg);
}

static int do_receipient_filter(struct rw_info *rwi, struct rw_info_vrfy *riv,
				std::string &errmsg)
{
int	rc;
char	errmsgbuf[2048];
char *p;

	if (riv->module->rw_ptr->filter_msg == 0)
		return (0);		/* Handle as a whitelisted recipient */

	if (nofilter)
		return (0);		/* Caller module is whitelisted */

	p=rfc822_gettok(rwi->sender);
	if (!p)	clog_msg_errno();

	errmsgbuf[0]=0;
	rc=(*riv->module->rw_ptr->filter_msg)(
		rwi->smodule,
		-1,
		riv->host,
		riv->addr,
		p,
		errmsgbuf,
		sizeof(errmsgbuf));
	free(p);

	if (!(rc == 0 || rc == 99))
	{
		const char *p=errmsgbuf;
		if (!*p)
			p="571 Delivery not authorized, message refused.";
		errmsg=p;
	}
	return (rc);
}

static void check_suppressbackscatter()
{
	const char *bofh=getenv("BOFHSUPPRESSBACKSCATTER");
	char buf[256];
	char *p;

	buf[0]=0;
	strncat(buf, bofh ? bofh:"smtp", 250);

	for (p=buf; (p=strtok(p, ",")) != NULL; p=NULL)
		if (strcmp(p, msgsource) == 0)
		{
			suppressbackscatter=1;
			break;
		}
}

int cppmain(int argc, char **argv)
{
	int	argn;
	const	char *module;
	static char shenv[]="SHELL=/bin/sh";

	clog_open_syslog("submit");
	umask(007);
#if HAVE_SETLOCALE
	setlocale(LC_ALL, "C");
#endif
	putenv(shenv);
	// In 0.34 maildrop environment init was changed to import SHELL,
	// when built as part of Courier.  Therefore, we need to make sure
	// that SHELL is initialized here.

	if (getuid() == 0)	/* Running as root screws up perms */
		libmail_changeuidgid(MAILUID, MAILGID);

	argn=cargs(argc, argv, arginfo);
	if (argc - argn < 2)
		exit(1);

	if (setvhost && *setvhost)
		config_set_local_vhost(setvhost);
	else if (authname && *authname)
	{
		const char *domain=strrchr(authname, '@');

		if (domain && config_has_vhost(++domain))
			config_set_local_vhost(domain);
	}

	module=argv[argn];
	receivedfrommta=argv[argn+1];
	if (strchr(receivedfrommta, ';') == 0)	exit(1);

	if (msgsource == NULL)
		msgsource=module;

	if (argc - argn > 2)
		identinfo=argv[argn+2];

	if (chdir(courierdir()) || chdir(TMPDIR))
	{
		clog_msg_start_err();
		clog_msg_str(courierdir());
		clog_msg_str("/" TMPDIR ": ");
		clog_msg_errno();
	}

	if (rw_init_courier(0))	exit (1);

struct rw_transport *modulep=rw_search_transport(module);

	if (!modulep)
	{
		clog_msg_start_err();
		clog_msg_str(module);
		clog_msg_str(" - not a valid module.");
		clog_msg_send();
		exit(1);
	}

	if (expnname || vrfyname)
	{
	struct	expninfo expn_info;
	AliasSearch	asearch;

		{

			asearch.Open(modulep);
			expn_info.asptr=&asearch;
			expn_info.flag=0;

			rewrite_address(expnname ? expnname:vrfyname,
				modulep,
				(expnname ? RW_ENVRECIPIENT|RW_EXPN:
					RW_ENVRECIPIENT|RW_VERIFY), NULL,
				expnname ? showexpn:showvrfy, &expn_info);
		}
		exit(expn_info.flag);
	}

	time(&submit_time);

	SubmitFile::trapsignals();
	verpflag=0;

	/* Determine if mail filtering should be used */

	nofilter=1;
	{
	char	*fn=config_localfilename("enablefiltering");
	char	*list=config_read1l(fn);

		free(fn);
		if (list)
		{
		char	*p;

			for (p=list; (p=strtok(p, " \r\t\n")) != 0; p=0)
				if (strcmp(p, module) == 0)
				{
					nofilter=0;
					break;
				}
			free(list);
		}
	}

	if (strcmp(module, DSN) == 0 ||
	    strcmp(module, "local") == 0)
	{
		alarm(0); /* No timeout for local mail */
	}
	else
	{
		alarm(36 * 60 * 60);
		// Alarm timeout, should be sufficiently smaller than the
		// cleanup interval in msgq::tmpscan().
	}

	if (strcmp(module, DSN))
		bofh_init();

	check_suppressbackscatter();

	Input input;

	if (fcntl(0, F_SETFL, O_NONBLOCK) < 0)
	{
		clog_msg_str("fcntl: ");
		clog_msg_errno();
	}

	exit (getsender(input, modulep));
	return (0);
}

extern "C" void rfc2045_error(const char *errmsg)
{
	std::cout << "430 " << errmsg << std::endl << std::flush;
	exit(1);
}

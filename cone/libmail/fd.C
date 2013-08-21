/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include	"config.h"
#include	"mail.H"
#include	"fd.H"
#include	"fdtls.H"
#include	"logininfo.H"
#include	"soxwrap/soxwrap.h"

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>
#include	<netdb.h>
#include	<fcntl.h>
#include	<errno.h>

#include	<iostream>
#include	<sstream>
#include	<cstring>
#include	<cstdlib>

using namespace std;

///////////////////////////////////////////////////////////////////////////

string mail::fd::rootCertRequiredErrMsg;

mail::fd::fd(mail::callback::disconnect &disconnect_callback,
	     std::vector<std::string> &certificatesArg) :
	mail::account(disconnect_callback), socketfd(-1),
	ioDebugFlag(false), certificates(certificatesArg), writtenFlag(false),
	tls(NULL)
{
}


mail::fd::~fd()
{
	if (tls)
		delete tls;
	if (socketfd >= 0)
		sox_close(socketfd);

	while (!writequeue.empty())
	{
		WriteBuffer *p=writequeue.front();
		writequeue.pop();
		delete p;
	}
}

string *mail::fd::getWriteBuffer()
{
	while (!writequeue.empty())
	{
		WriteBuffer *p=writequeue.front();

		if (p->writebuffer.size() > 0)
			return (&p->writebuffer);

		if (p->fillWriteBuffer())
		{
			if (p->writebuffer.size() > 0)
				return (&p->writebuffer);
			break;	// Handler's waiting for a server response
		}

		writequeue.pop();
		delete p;
	}

	return NULL;
}

static void debug_io(const char *type, int socketfd,
		     const char *b, const char *e)
{
	ostringstream o;

	o << type << "(" << socketfd << "): ";

	while (b != e)
	{
		if (*b == '\r')
			o << "\\r";
		else if (*b == '\n')
			o << "\\n";
		else if (*b == '\t')
			o << "\\t";
		else
			o << (((*b) & 127) < 32 ? '.':*b);
		b++;
	}
	o << endl;

	string s=o.str();

	if (write(2, s.c_str(), s.size()) < 0)
		; // Ignore gcc warning 
}

#define DEBUG_IO(a,b,c) \
	do { if (ioDebugFlag) debug_io((a),(socketfd),(b),(c)); } while (0)

string mail::fd::socketDisconnect()
{
	string errmsg="";

	if (socketfd < 0)
		return errmsg;

	DEBUG_IO("Socket.CLOSE", NULL, NULL);

#if HAVE_LIBCOURIERTLS

	if (tls)
	{
		if (tls->errmsg.size() > 0)
			errmsg=tls->errmsg;

		delete tls;
		tls=NULL;
	}
#endif

	sox_close(socketfd);
	socketfd= -1;

	return errmsg;
}

string mail::fd::socketConnect(mail::loginInfo &loginInfo,
			       const char *plainservice,
			       const char *sslservice)
{

#if HAVE_LIBCOURIERTLS

#else
	if (loginInfo.use_ssl)
		return "SSL support not available.";

#endif

	struct addrinfo *res;

	string server=loginInfo.server, portnum;
	const char *port=loginInfo.use_ssl ? sslservice:plainservice;
	size_t n=server.find(':');

	if (n != std::string::npos)
	{
		port=NULL;
		portnum=server.substr(n+1);
		server=server.substr(0, n);
	}

	if (loginInfo.options.count("debugio") > 0)
		ioDebugFlag=true;

	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family=PF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;

	int errcode=getaddrinfo(server.c_str(),	port, &hints, &res);

	if (errcode)
		return gai_strerror(errcode);

	if (!port)
	{
		int nport=atoi(portnum.c_str());

		if (nport <= 0 || nport > 65535)
		{
			freeaddrinfo(res);
			return strerror(EINVAL);
		}

		switch (res->ai_addr->sa_family) {
		case AF_INET:
			((struct sockaddr_in *)res->ai_addr)
				->sin_port=htons(nport);
			break;
#ifdef AF_INET6
		case AF_INET6:
			((struct sockaddr_in6 *)res->ai_addr)
				->sin6_port=htons(nport);
			break;
#endif
		default:
			freeaddrinfo(res);
			return strerror(EAFNOSUPPORT);
		}
	}

	socketfd=socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	connecting=1;

	// Put socket into non-blocking mode.  We're 100% asynchronous.
	// Set close-on-exec bit, we don't want this fd to be seen by any
	// other process.

	if (socketfd < 0 || fcntl(socketfd, F_SETFL, O_NONBLOCK) < 0 ||
	    fcntl(socketfd, F_SETFD, FD_CLOEXEC) < 0)
	{
		freeaddrinfo(res);
		return strerror(errno);
	}

#if HAVE_LIBCOURIERTLS

	if (loginInfo.use_ssl)
	{
		string errmsg= starttls(loginInfo, false);

		if (errmsg.size() > 0)
		{
			freeaddrinfo(res);
			return errmsg;
		}
	}
#endif

	if (sox_connect(socketfd, res->ai_addr, res->ai_addrlen) < 0)
	{
		freeaddrinfo(res);
		if (errno != EINPROGRESS)
			return strerror(errno);

		// Async connection in progress.
	}
	else	// Managed to connect right away.
	{
		freeaddrinfo(res);
		connecting=3;	// Managed to skip some steps in process()
	}

	return "";
}

string mail::fd::socketAttach(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0 ||
	    fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
	{
		return strerror(errno);
	}

	socketfd=fd;
	connecting=3;
	return "";
}

bool mail::fd::socketBeginEncryption(mail::loginInfo &loginInfo)
{
#if HAVE_LIBCOURIERTLS

	string errmsg=starttls(loginInfo, true);

	if (errmsg.size() > 0)
	{
		loginInfo.callbackPtr->fail(errmsg);
		return false;
	}

	return establishtls();

#else


	loginInfo.callbackPtr->fail("SSL/TLS encryption not available.");
	return false;

#endif
}

time_t mail::fd::getTimeoutSetting(mail::loginInfo &loginInfo,
				   const char *name, time_t defaultValue,
				   time_t minValue, time_t maxValue)
{
	map<string, string>::iterator p=loginInfo.options.find(name);

	if (p != loginInfo.options.end())
	{
		istringstream i(p->second);

		i >> defaultValue;
	}

	if (defaultValue < minValue)
		defaultValue=minValue;

	if (defaultValue > maxValue)
		defaultValue=maxValue;
	return defaultValue;
}

//
// Allocate a new TLS structure.  Called before any TLS/SSL negotiation
// takes place.
//

string mail::fd::starttls(mail::loginInfo &loginInfo,
			  bool starttlsFlag)
{
#if HAVE_LIBCOURIERTLS

	if (tls)
		return "";

	if ((tls=new mail::fdTLS(starttlsFlag, certificates)) == NULL)
		return strerror(errno);

	tls->fd=socketfd;
	loginInfo.tlsInfo=tls;
	//tls->callback= loginInfo.callbackPtr;
	tls->tls_info.app_data=tls;
	tls->tls_info.getconfigvar= &mail::fdTLS::get_tls_config_var;
	tls->tls_info.tls_err_msg= &mail::fdTLS::get_tls_err_msg;
	tls->tls_info.getpemclientcert4ca=
		&mail::fdTLS::get_tls_client_certs;
	tls->tls_info.releasepemclientcert4ca=
		&mail::fdTLS::free_tls_client_certs;

	tls->domain="";

	if (loginInfo.options.count("novalidate-cert") == 0)
	{
		tls->domain=loginInfo.server;
		tls->tls_info.peer_verify_domain=tls->domain.c_str();

		const char *p=mail::fdTLS
			::get_tls_config_var("TLS_TRUSTCERTS", tls);

		if (!p || !*p)
		{
			if (rootCertRequiredErrMsg.size() > 0)
				return rootCertRequiredErrMsg;
		}
	}
#endif
	return "";
}

//
// Begin SSL/TLS negotiation.
//

bool mail::fd::establishtls()
{
#if HAVE_LIBCOURIERTLS

	tls->errmsg="Unable to establish a secure connection.";
	if ((tls->ctx=tls_create(0, &tls->tls_info)) == NULL ||
	    (tls->ssl=tls_connect(tls->ctx, socketfd)) == NULL)
	{
		string errmsg=tls->errmsg;
		// mail::callback *c=tls->callback;

		delete tls;
		tls=NULL;

		disconnect(errmsg.c_str());
		//if (c)
		// c->fail(errmsg);
		return false;
	}

	tls_transfer_init(&tls->tls_transfer);
#endif

	return true;
}

bool mail::fd::socketConnected()
{
	if (socketfd < 0)
		return false;

#if HAVE_LIBCOURIERTLS
	if (tls)
		tls->errmsg="";
#endif
	return true;
}

bool mail::fd::socketEndEncryption()
{
#if HAVE_LIBCOURIERTLS
	if (tls)
	{
		tls_closing(&tls->tls_transfer);
		return true;
	}
#endif

	return false;
}

//
// Handle any pending I/O to/from the server.
//

int mail::fd::process(std::vector<pollfd> &fds, int &timeout)
{
	mail::pollfd myPollFd;

	char buffer[BUFSIZ];
	int n;
	socklen_t s;

	bool mustReadSomething=false;

	myPollFd.fd=socketfd;
	myPollFd.events=0;
	myPollFd.revents=0;

	if (socketfd < 0) // Disconnected, flush anything that's written.
	{
		while (!writequeue.empty())
		{
			WriteBuffer *p=writequeue.front();
			writequeue.pop();
			delete p;
		}
		return (-1);
	}

	// First step in asynchronous connection logic: wait for the socket
	// to become writable.

	switch (connecting) {
	case 1:
		myPollFd.events=pollwrite;
		fds.push_back(myPollFd);
		++connecting;
		return (0);
	case 2:

		// Socket is writable, check the error code of the connection
		// request.

		s=sizeof(n);
		if (sox_getsockopt(socketfd, SOL_SOCKET, SO_ERROR, &n, &s) < 0
		    || (errno=EINVAL, s) < sizeof(n)
		    || (errno=n) != 0)
		{
			timeout=0;
			disconnect(strerror(errno));
			return (-1);
		}

		/* FALLTHROUGH */

	case 3:
		connecting=0;

		// Step 3 - begin SSL/TLS negotation, if requested.

#if HAVE_LIBCOURIERTLS

		if (tls)
			if (!establishtls())
			{
				timeout=0;
				return (-1);
			}
#endif
	}

	string *writePtr;

#if HAVE_LIBCOURIERTLS

	// When the connection has SSL/TLS enabled, everything gets handled
	// by libcouriertls.

	if (tls)
	{
		if (tls->tls_transfer.writeleft == 0 &&
		    (writePtr=getWriteBuffer()) != NULL)
		{
			// Transfer the next chunk of data to write to the
			// TLS buffer

			tls->writebuffer= *writePtr;
			tls->tls_transfer.writeleft=tls->writebuffer.size();
			tls->tls_transfer.writeptr=&tls->writebuffer[0];
			writtenBuffer(writePtr->size());
			// As far as the write queue is concerned, this
			// stuff was written.

		}

		// Repeatedly call tls_transfer() until it stops doing
		// something useful.

		for (;;)
		{
			// Set the read ptr to start of the read buffer,
			// remember the current write ptr, then call
			// tls_transfer()

			tls->tls_transfer.readptr=tls->readBuffer;
			tls->tls_transfer.readleft=sizeof(tls->readBuffer);

			const char *save_writeptr=tls->tls_transfer.writeptr;

			int rc;
			{
				fd_set r, w;

				FD_ZERO(&r);
				FD_ZERO(&w);

				rc=tls_transfer(&tls->tls_transfer, tls->ssl,
						socketfd, &r, &w);

				if (FD_ISSET(socketfd, &r))
					myPollFd.events |= pollread;

				if (FD_ISSET(socketfd, &w))
					myPollFd.events |= pollwrite;
			}

			if ((tls_isclosing(&tls->tls_transfer) ||
			     tls_isclosed(&tls->tls_transfer)) &&
			    !tls->tlsShutdownSent)
			{
				tls->tlsShutdownSent=true;
			}

			readbuffer.append(tls->readBuffer,
					  tls->tls_transfer.readptr);

			// We might've read something.

			if (rc < 0)
			{
#if 0
				cerr << "DEBUG: SSL connection terminate: buffer="
				     << readbuffer.size()
				     << ", tls_isclosed: " << tls_isclosed(&tls->tls_transfer)
				     << ", errmsg=" << tls->errmsg
				     << endl;
#endif

				if (readbuffer.size() > 0)
				{
					// There's still unfinished stuff in
					// readbuffer, so pretend we're happy,
					// for now, but set retry to 0 seconds,
					// so we go back in here to process
					// more read data.

					myPollFd.events |= pollwrite;
					// If we don't do that, we'll return
					// without requesting any r/w activity,
					// so the application will now stall.
					// Let's request write status, which
					// should come back immediately and
					// result in -1 return from
					// tls_transfer().

					mustReadSomething=true;

					break; // Finish processing read data
				}

				timeout=0;

				DEBUG_IO("EOF.SSL", NULL, NULL);
				if (tls)
					disconnect(tls_isclosed(&tls->
								tls_transfer)
						   ? NULL:strerror(errno));
				return (-1);
			}

			if (save_writeptr != tls->tls_transfer.writeptr)
			{
				DEBUG_IO("Write.SSL",save_writeptr,
					 tls->tls_transfer.writeptr);
			}

			if (tls->tls_transfer.readptr != tls->readBuffer)
			{
				DEBUG_IO("Read.SSL",
					 tls->readBuffer,
					 tls->tls_transfer.readptr);
			}

			if (save_writeptr != tls->tls_transfer.writeptr ||
			    tls->tls_transfer.readptr != tls->readBuffer)
			{
				timeout=0;
			}
			break;
		}
	}
	else
#endif

		// Connection not encrypted
				 
		for (;;)
		{
			n=sox_read(socketfd, buffer, sizeof(buffer));

			if (n < 0)
			{
				if (errno == EAGAIN)
				{
					myPollFd.events |= pollread;
					break;
				}
				DEBUG_IO("Read.ERROR", NULL, NULL);
				timeout=0;
				disconnect(strerror(errno));
				return (-1);
			}

			if (n == 0)
			{
				timeout=0;
				mustReadSomething=true;
				break;
			}

			if (n > 0)
				DEBUG_IO("Read", buffer, buffer+n);

			readbuffer=readbuffer.append(buffer, n);
			if (readbuffer.length() >= sizeof(buffer))
			{
				myPollFd.events |= pollread;
				break;
			}
		}



	while (
#if HAVE_LIBCOURIERTLS

	       !tls &&
#endif
	       (writePtr=getWriteBuffer()) != NULL)
	{
		n= ::sox_write(socketfd, &(*writePtr)[0], writePtr->size());

		if (n < 0)
		{
			DEBUG_IO("Write.ERROR", NULL, NULL);
			if (errno == EAGAIN)
			{
				myPollFd.events |= pollwrite;
				break;
			}
			timeout=0;
			disconnect(strerror(errno));
			return (-1);
		}

		if (n > 0)
		{
			const char *p=(*writePtr).c_str();

			DEBUG_IO("Write", p, p+n);
		}

		if (n == 0)
		{
			DEBUG_IO("Write.EOF", NULL, NULL);
			timeout=0;
			disconnect("Connection closed by remote host.");
			return (-1);
		}

		writtenBuffer(n);
	}

	//
	// Try to process any read data.
	//

	MONITOR(mail::account);

	while (readbuffer.length() > 0)
	{
#if 0
		static size_t skip=0;

		if (skip == 0)
		{
			cerr << "PROCESS: [" << readbuffer << "]" << endl;

			char linebuf[80];

			cin.getline(linebuf, sizeof(linebuf));
			if (strcmp(linebuf, "y") == 0)
			{
				timeout=0;
				disconnect("Forced disconnect");
				return (-1);
			}

			if (isdigit(linebuf[0]))
				istrstream(linebuf) >> skip;
		}
		else
			--skip;
#endif

#if 0
		{
			string cpy=readbuffer;

			if (cpy.length() > 32)
			{
				cpy.erase(32);
				cpy += "...";
			}

			string::iterator b=cpy.begin(), e=cpy.end();

			while (b != e)
			{
				if (iscntrl((int)(unsigned char)*b))
					*b='.';
				b++;
			}

			cerr << "Process: [" << cpy << "] ("
			     << readbuffer.length() << " bytes)" << endl;
		}
#endif

		int n=socketRead(readbuffer);

		if (DESTROYED())
		{
			timeout=0;
			return (0);
		}

		if (n <= 0)
		{
			timeout=0;

			if (mustReadSomething)
				break;

			if (myPollFd.events)
				fds.push_back(myPollFd);
			return n;
		}

		mustReadSomething=false;

		readbuffer.erase(0, n);

#if 0
		if (getWriteBuffer() != NULL && socketfd >= 0)
#endif
		{
			timeout=0; // Callback handler wrote something
		}
	}
#if HAVE_LIBCOURIERTLS
	if (!DESTROYED() && tls && tls_isclosing(&tls->tls_transfer) &&
	    !tls_isclosed(&tls->tls_transfer))
	{
		if (!tls->tlsShutdownSent)
			myPollFd.events |= pollwrite;
		else
			myPollFd.events |= pollread;
		readbuffer="";
		mustReadSomething=false;
	}
#endif

	if (mustReadSomething)
	{
		DEBUG_IO("Read.EOF", NULL, NULL);
		// Subclass hasn't processed any data, and it
		// will never see any more data, so give up.

		disconnect("Connection closed by remote host.");
		return -1;
	}

	if (myPollFd.events)
		fds.push_back(myPollFd);

	return 0;
}

// Write a string.  Queue it up, and wait for process() to do its job

void mail::fd::socketWrite(string s)
{
	writtenFlag=true;

	WriteBuffer *w=new WriteBuffer;

	if (!w)
		LIBMAIL_THROW("Out of memory.");

	try {
		w->writebuffer=s;
		writequeue.push(w);
	} catch (...) {
		delete w;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::fd::socketWrite(WriteBuffer *w)
{
	writtenFlag=true;
	writequeue.push(w);
}

/////////////////////////////////////////////////////////////////////////////
//
// Initialize the WriteBuffer superclass.

mail::fd::WriteBuffer::WriteBuffer() : writebuffer("")
{
}

mail::fd::WriteBuffer::~WriteBuffer()
{
}

bool mail::fd::WriteBuffer::fillWriteBuffer()
{
	return false;
}


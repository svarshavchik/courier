/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "misc.H"
#include "smtp.H"
#include "driver.H"
#include "search.H"
#include "imaphmac.H"
#include "base64.H"
#include "smtpfolder.H"
#include "tcpd/spipe.h"
#include "unicode/unicode.h"
#include "libcouriertls.h"

#include <errno.h>

#include <sstream>
#include <iomanip>

#include <sys/types.h>
#include <signal.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

using namespace std;

#define SMTPTIMEOUT 60

#define INACTIVITY_TIMEOUT 30

#define DIGIT(c) (strchr("0123456789", (c)))


/////////////////////////////////////////////////////////////////////////////

LIBMAIL_START

static bool open_smtp(mail::account *&accountRet,
		      mail::account::openInfo &oi,
		      mail::callback &callback,
		      mail::callback::disconnect &disconnectCallback)
{
	mail::loginInfo nntpLoginInfo;

	if (!mail::loginUrlDecode(oi.url, nntpLoginInfo))
		return false;

	if (nntpLoginInfo.method != "smtp" &&
	    nntpLoginInfo.method != "smtps" &&
	    nntpLoginInfo.method != "sendmail")
		return false;

	accountRet=new mail::smtp(oi.url, oi.pwd, oi.certificates, callback,
				  disconnectCallback,
				  oi.loginCallbackObj);
	return true;
}

static bool smtp_remote(string url, bool &flag)
{
	mail::loginInfo nntpLoginInfo;

	if (!mail::loginUrlDecode(url, nntpLoginInfo))
		return false;

	if (nntpLoginInfo.method != "smtp" &&
	    nntpLoginInfo.method != "smtps")
		return false;

	flag=true;
	return true;
}

driver smtp_driver= { &open_smtp, &smtp_remote };

LIBMAIL_END

///////////////////////////////////////////////////////////////////////////
//
// Helper class sends outbound message
//
///////////////////////////////////////////////////////////////////////////

mail::smtp::Blast::Blast(mail::smtp *smtpArg)
	: eofSent(false), mySmtp(smtpArg), lastChar('\n'),
	  bytesTotal(0), bytesDone(0)
{
}

mail::smtp::Blast::~Blast()
{
	if (mySmtp)
		mySmtp->myBlaster=NULL;
}

bool mail::smtp::Blast::fillWriteBuffer()
{
	if (!mySmtp)
	{
		if (!eofSent)
		{
			eofSent=true;
			writebuffer += "\r\n.\r\n";
			return true;
		}
		return false;
	}

	mySmtp->installHandler(mySmtp->responseHandler);
	// Reset the timeout every time the write buffer gets filled

	char buffer[BUFSIZ];

	size_t i;

	FILE *fp=mySmtp->sendQueue.front().message;

	for (i=0; i<sizeof(buffer); i++)
	{
		int ch=getc(fp);

		if (ch == EOF)
		{
			writebuffer += string(buffer, buffer+i);
			if (lastChar != '\n')
				writebuffer += "\r\n";
			writebuffer += ".\r\n";

			mail::callback *c=mySmtp->sendQueue.front().callback;

			if (c)
				c->reportProgress(bytesTotal, bytesTotal,
						  0, 1);

			mySmtp->myBlaster=NULL;
			mySmtp=NULL;
			eofSent=true;
			return true;
		}

		++bytesDone;

		if (lastChar == '\n' && ch == '.')
		{
			ungetc(ch, fp); // Leading .s are doubled.
			--bytesDone;
		}

		if (ch == '\n' && lastChar != '\r')
		{
			ungetc(ch, fp);
			--bytesDone;
			ch='\r';
		}

		buffer[i]=lastChar=ch;
	}

	mail::callback *c=mySmtp->sendQueue.front().callback;

	if (c)
		c->reportProgress(bytesDone, bytesTotal, 0, 1);

	writebuffer += string(buffer, buffer+sizeof(buffer));
	return true;
}

// Something arrived from the server.

int mail::smtp::socketRead(const string &readbuffer)
{
	size_t n=readbuffer.find('\n');

	if (n == std::string::npos)
		return 0;

	string l=readbuffer.substr(0, n++);

	size_t cr;

	while ((cr=l.find('\r')) != std::string::npos)
		l=l.substr(0, cr) + l.substr(cr+1);

	// Collect multiline SMTP replies.  Keep only the last 30 lines
	// received.

	smtpResponse.push_back(l);

	if (smtpResponse.size() > 30)
		smtpResponse.erase(smtpResponse.begin());

	string::iterator b=l.begin(), e=l.end();
	int numCode=0;

	while (b != e && isdigit((int)(unsigned char)*b))
		numCode=numCode * 10 + (*b++ - '0');

	if (b == l.begin()+3)
	{
		if (b == e || unicode_isspace((unsigned char)*b))
		{
			string s="";

			list<string>::iterator b, e;

			b=smtpResponse.begin();
			e=smtpResponse.end();

			while (b != e)
			{
				if (s.size() > 0) s += "\n";

				s += *b++;
			}

			smtpResponse.clear();

			if (responseHandler != 0)
				(this->*responseHandler)(numCode, s);
			else
				fatalError=true;
			// SMTP msg when no handler installed, something's
			// gone wrong.

		}
	}

	return n;
}

void mail::smtp::installHandler(void (mail::smtp::*handler)(int,
							    string))
{
	responseHandler=handler;
	responseTimeout=0;

	if (handler != 0)
	{
		time(&responseTimeout);
		responseTimeout += SMTPTIMEOUT;
	}
}


void mail::smtp::disconnect(const char *reason)
{
	while (!sendQueue.empty())
	{
		struct messageQueueInfo &info=sendQueue.front();

		info.callback->fail(reason);
		fclose(info.message);
		sendQueue.pop();
	}
	pingTimeout=0;

	if (getfd() >= 0)
	{
		string errmsg=socketDisconnect();

		if (errmsg.size() == 0)
			errmsg=reason;

		if (orderlyShutdown)
			success(errmsg);
		else if (smtpLoginInfo.callbackPtr)
			error(errmsg);
		else
			disconnect_callback.disconnected(errmsg.c_str());
	}
}

void mail::smtp::resumed()
{
	if (responseHandler != 0)
	{
		time(&responseTimeout);
		responseTimeout += SMTPTIMEOUT;
	}
}


void mail::smtp::handler(vector<pollfd> &fds, int &ioTimeout)
{
	if (fatalError)
		return;

	int fd_save=getfd();

	time_t now;

	time(&now);

	if (responseHandler != 0)
	{
		if (now >= responseTimeout)
		{
			fatalError=true;
			ioTimeout=0;
			(this->*responseHandler)(500, "500 Server timed out.");
			return;
		}

		if (ioTimeout >= (responseTimeout - now) * 1000)
		{
			ioTimeout=(responseTimeout - now)*1000;
		}
	}

	if (pingTimeout)
	{
		if (pingTimeout < now)
		{
			mail::smtpInfo dummy;

			send(NULL, dummy, NULL, false);
		}
		else
		{
			if (ioTimeout >= (pingTimeout - now)*1000)
			{
				ioTimeout=(pingTimeout - now)*1000;
			}
		}
	}


	mail::fd::process(fds, ioTimeout);

	if (getfd() < 0)
	{
		size_t i;

		for (i=fds.size(); i; )
		{
			--i;

			if (fds[i].fd == fd_save)
			{
				fds.erase(fds.begin()+i, fds.begin()+i+1);
				break;
			}
		}
	}
}

mail::smtp::smtp(string url, string passwd,
		 std::vector<std::string> &certificates,
		 mail::callback &callback,
		 mail::callback::disconnect &disconnectCallback,
		 loginCallback *loginCallbackFunc)
	: mail::fd(disconnectCallback, certificates), orderlyShutdown(false),
	  ready2send(false), fatalError(true), pingTimeout(0)
{
	responseHandler=NULL;
	smtpLoginInfo.callbackPtr= &callback;
	smtpLoginInfo.loginCallbackFunc=loginCallbackFunc;

	if (!loginUrlDecode(url, smtpLoginInfo))
	{
		smtpLoginInfo.callbackPtr->fail("Invalid SMTP URL.");
		return;
	}

	if (smtpLoginInfo.method == "sendmail") // Local sendmail
	{
		smtpLoginInfo.use_ssl=false;
		smtpLoginInfo.uid="";
		smtpLoginInfo.pwd="";

		string errmsg;

		int pipefd[2];

		if (libmail_streampipe(pipefd) < 0)
		{
			smtpLoginInfo.callbackPtr->fail(strerror(errno));
			return;
		}

		pid_t pid1=fork();

		if (pid1 < 0)
		{
			close(pipefd[0]);
			close(pipefd[1]);
			smtpLoginInfo.callbackPtr->fail(strerror(errno));
			return;
		}

		if (pid1 == 0)
		{
			close(0);
			close(1);
			errno=EIO;
			if (dup(pipefd[1]) != 0 || dup(pipefd[1]) != 1)
			{
				perror("500 dup");
				exit(1);
			}
			close(pipefd[0]);
			close(pipefd[1]);

			pid1=fork();
			if (pid1 < 0)
			{
				printf("500 fork() failed.\r\n");
				exit(1);
			}

			if (pid1)
				exit(0);
			execl(SENDMAIL, SENDMAIL, "-bs", (char *)NULL);

			printf("500 %s: %s\n", SENDMAIL, strerror(errno));
			exit(0);
		}

		close(pipefd[1]);
		int waitstat;
		pid_t pid2;

		while ((pid2=wait(&waitstat)) != pid1)
		{
			if (kill(pid1, 0))
				break;
		}


		try {
			errmsg=socketAttach(pipefd[0]);
		} catch (...) {
			close(pipefd[0]);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	}
	else
	{
		smtpLoginInfo.use_ssl= smtpLoginInfo.method == "smtps";

		if (passwd.size() > 0)
			smtpLoginInfo.pwd=passwd;

		string errmsg=socketConnect(smtpLoginInfo, "smtp", "smtps");

		if (errmsg.size() > 0)
		{
			smtpLoginInfo.callbackPtr->fail(errmsg);
			return;
		}
	}

	fatalError=false;
	hmac_method_index=0;
	installHandler(&mail::smtp::greetingResponse);
}

// Login failed

void mail::smtp::error(string errMsg)
{
	mail::callback *c=smtpLoginInfo.callbackPtr;

	smtpLoginInfo.callbackPtr=NULL;

	if (c)
		c->fail(errMsg);

}

// Login succeeded

void mail::smtp::success(string errMsg)
{
	mail::callback *c=smtpLoginInfo.callbackPtr;

	smtpLoginInfo.callbackPtr=NULL;

	if (c)
		c->success(errMsg);
}

// Send EHLO/HELO

void mail::smtp::howdy(const char *hi)
{
	char buffer[512];

	buffer[sizeof(buffer)-1]=0;

	if (gethostname(buffer, sizeof(buffer)-1) < 0)
		strcpy(buffer, "localhost");

	socketWrite(string(hi) + buffer + "\r\n");
}

void mail::smtp::greetingResponse(int n, string s)
{
	switch (n / 100) {
	case 1:
	case 2:
	case 3:
		break;
	default:
		error(s);
		return;
	}

	installHandler(&mail::smtp::ehloResponse);
	howdy("EHLO ");
}

void mail::smtp::ehloResponse(int n, string s)
{
	switch (n / 100) {
	case 1:
	case 2:
	case 3:
		break;
	default: // Maybe HELO will work
		installHandler(&mail::smtp::heloResponse);
		howdy("HELO ");
		return;
	}

	// Parse EHLO capability list

	bool firstLine=true;

	while (s.size() > 0)
	{
		string line;

		size_t p=s.find('\n');

		if (p == std::string::npos)
		{
			line=s;
			s="";
		}
		else
		{
			line=s.substr(0, p);
			s=s.substr(p+1);
		}

		if (firstLine)
		{
			firstLine=false;
			continue;
		}

		while ((p=s.find('\r')) != std::string::npos)
			s=s.substr(0, p) + s.substr(p+1);

		for (p=0; p<line.size(); p++)
			if (!DIGIT((int)(unsigned char)line[p]))
				break;

		if (p < line.size())
			p++;

		// Put capability to uppercase

		line=mail::iconvert::convert_tocase(line.substr(p),
						    "iso-8859-1",
						    unicode_uc);

		for (p=0; p<line.size(); p++)
			if (unicode_isspace((unsigned char)line[p]) ||
			    line[p] == '=')
				break;

		string capa=line.substr(0, p);

		// Save SASL authentication methods as capabilities
		// AUTH=method

		if (capa == "AUTH" || capa == "XSECURITY")
		{
			if (p < line.size())
				p++;

			line=line.substr(p);

			while (line.size() > 0)
			{
				for (p=0; p<line.size(); p++)
					if (unicode_isspace((unsigned char)
						    line[p]) ||
					    line[p] == ',')
						break;

				if (p > 0)
				{
					capabilities.insert(capa + "=" +
							    line.substr(0, p));
				}
				if (p < line.size())
					p++;
				line=line.substr(p);
			}
		}
		else
			capabilities.insert(capa);
	}
	starttls();
}

void mail::smtp::heloResponse(int n, string s)
{
	switch (n / 100) {
	case 1:
	case 2:
	case 3:
		starttls();
		return;
	}

	error(s);
}

void mail::smtp::starttls()
{
#if HAVE_LIBCOURIERTLS

	if (smtpLoginInfo.use_ssl // Already SSL-encrypted
	    || smtpLoginInfo.options.count("notls") > 0
	    || !hasCapability("STARTTLS")
	    || socketEncrypted())
	{
		begin_auth();
		return;
	}

	installHandler(&mail::smtp::starttlsResponse);

	socketWrite("STARTTLS\r\n");
#else
	begin_auth();
	return;
#endif
}

void mail::smtp::starttlsResponse(int n, string s)
{
	switch (n / 100) {
	case 1:
	case 2:
		if (!socketBeginEncryption(smtpLoginInfo))
		{
			smtpLoginInfo.callbackPtr=NULL;
			return;
		}
		capabilities.clear();
		greetingResponse(n, s);
		return;
	}

	error(s);
}

void mail::smtp::begin_auth()
{
	if (!hasCapability("AUTH=EXTERNAL"))
	{
		begin_auth_nonexternal();
		return;
	}
	installHandler(&mail::smtp::auth_external_response);
	socketWrite("AUTH EXTERNAL =\r\n");
}

void mail::smtp::auth_external_response(int n, string s)
{
	switch (n / 100) {
	case 1:
	case 2:
	case 3:
		authenticated();
		return;
	}
	begin_auth_nonexternal();
}

void mail::smtp::begin_auth_nonexternal()
{
	int i;

	if (smtpLoginInfo.uid.size() == 0)
	{
		authenticate_hmac();
		return;
	}

	for (i=0; mail::imaphmac::hmac_methods[i]; ++i)
	{
		if (hasCapability(string("AUTH=CRAM-") +
				  mail::imaphmac
				  ::hmac_methods[hmac_method_index]->getName()
				  ))
			break;
	}

	if (mail::imaphmac::hmac_methods[i] ||
	    hasCapability("AUTH=LOGIN"))
	{
		if (smtpLoginInfo.pwd.size() == 0)
		{
			currentCallback=smtpLoginInfo.loginCallbackFunc;

			if (currentCallback)
			{
				currentCallback->target=this;
				currentCallback->getPassword();
				return;
			}
		}
	}
	authenticate_hmac();
}

void mail::smtp::loginInfoCallback(std::string str)
{
	currentCallback=NULL;
	smtpLoginInfo.pwd=str;
	authenticate_hmac();
}

void mail::smtp::loginInfoCallbackCancel()
{
	currentCallback=NULL;
	error("Login cancelled");
}

void mail::smtp::authenticate_hmac()
{
	for (;;)
	{
		if (mail::imaphmac::hmac_methods[hmac_method_index] == NULL ||
		    smtpLoginInfo.uid.size() == 0)
		{
			authenticate_login(); // Exhausted SASL, forget it.
			return;
		}

		if (hasCapability(string("AUTH=CRAM-") +
				  mail::imaphmac
				  ::hmac_methods[hmac_method_index]->getName()
				  ))
			break;
		hmac_method_index++;
	}

	installHandler(&mail::smtp::hmacResponse);

	socketWrite(string("AUTH CRAM-") +
		    mail::imaphmac::hmac_methods[hmac_method_index]->getName()
		    + "\r\n");
}

void mail::smtp::hmacResponse(int n, string s)
{
	switch (n / 100) {
	case 1:
	case 2:
		authenticated();
		return;
	}

	if ((n / 100) != 3 || s.size() < 4)
	{
		++hmac_method_index;
		authenticate_hmac();
		return;
	}

	s=mail::decodebase64str(s.substr(4));
	s=(*mail::imaphmac::hmac_methods[hmac_method_index])(smtpLoginInfo
							     .pwd, s);

	string sHex;

	{
		ostringstream hexEncode;

		hexEncode << hex;

		string::iterator b=s.begin();
		string::iterator e=s.end();

		while (b != e)
			hexEncode << setw(2) << setfill('0')
				  << (int)(unsigned char)*b++;

		sHex=hexEncode.str();
	}

	s=mail::encodebase64str(smtpLoginInfo.uid + " " + sHex);
	socketWrite(s + "\r\n");
}

void mail::smtp::authenticate_login()
{
	if (!hasCapability("AUTH=LOGIN") || smtpLoginInfo.uid.size() == 0 ||
	    smtpLoginInfo.options.count("cram"))
	{
		authenticated();
		return;
	}

	installHandler(&mail::smtp::loginUseridResponse);

	socketWrite("AUTH LOGIN\r\n");
}

void mail::smtp::loginUseridResponse(int n, string msg)
{
	if ((n / 100) != 3)
	{
		error(msg);
		return;
	}

	installHandler(&mail::smtp::loginPasswdResponse);
	socketWrite(string(mail::encodebase64str(smtpLoginInfo.uid))
		    + "\r\n");
}

void mail::smtp::loginPasswdResponse(int n, string msg)
{
	if ((n / 100) != 3)
	{
		error(msg);
		return;
	}

	installHandler(&mail::smtp::loginResponse);
	socketWrite(string(mail::encodebase64str(smtpLoginInfo.pwd))
		    + "\r\n");
}

void mail::smtp::loginResponse(int n, string msg)
{
	if ((n / 100) != 2)
	{
		error(msg);
		return;
	}

	authenticated();
}

void mail::smtp::authenticated()
{
	ready2send=true;
	if (!sendQueue.empty())
		startNextMessage(); // Already something's queued up.
	else
		pingTimeout=time(NULL) + INACTIVITY_TIMEOUT;
	success("Connected to " + smtpLoginInfo.server);
}

mail::smtp::~smtp()
{
	disconnect("Connection forcibly aborted.");
}

void mail::smtp::logout(mail::callback &callback)
{
	if (fatalError)
	{
		callback.success("Server timed out.");
		return;
	}

	smtpLoginInfo.callbackPtr= &callback;
	orderlyShutdown=true;
	installHandler(&mail::smtp::quitResponse);

	socketWrite("QUIT\r\n");
}

void mail::smtp::quitResponse(int n, string s)
{
	if (!socketEndEncryption())
	{
		socketDisconnect();
		success(s);
	}
}

void mail::smtp::checkNewMail(mail::callback &callback) // STUB
{
	callback.success("OK");
}

bool mail::smtp::hasCapability(string capability)
{
	if (capability == LIBMAIL_SINGLEFOLDER)
		return false;

	return capabilities.count(capability) > 0;
}

string mail::smtp::getCapability(string name)
{
	upper(name);

	if (name == LIBMAIL_SERVERTYPE)
	{
		return "smtp";
	}

	if (name == LIBMAIL_SERVERDESCR)
	{
		return "ESMTP server";
	}

	if (name == LIBMAIL_SINGLEFOLDER)
		return "";

	return capabilities.count(name) > 0 ? "1":"";
}


// Not implemented

mail::folder *mail::smtp::folderFromString(string)
{
	errno=EINVAL;
	return NULL;
}

// Not implemented

void mail::smtp::readTopLevelFolders(mail::callback::folderList &callback1,
				     mail::callback &callback2)
{
	vector<const mail::folder *> dummy;

	callback1.success(dummy);
	callback2.success("OK");
}

// Not implemented

void mail::smtp::findFolder(string folder,
			    class mail::callback::folderList &callback1,
			    class mail::callback &callback2)
{
	callback2.fail("Folder not found.");
}

// This is why we're here:

mail::folder *mail::smtp::getSendFolder(const mail::smtpInfo &info,
					const mail::folder *folderArg,
					string &errmsg)
{
	if (folderArg)
	{
		return mail::account::getSendFolder(info, folderArg, errmsg);
	}

	if (info.recipients.size() == 0)
	{
		errno=ENOENT;
		errmsg="Empty recipient list.";
		return NULL;
	}

	mail::smtpFolder *folder=new mail::smtpFolder(this, info);

	if (!folder)
	{
		errmsg=strerror(errno);
		return NULL;
	}
	return folder;
}


void mail::smtp::readMessageAttributes(const vector<size_t> &messages,
				       MessageAttributes attributes,
				       mail::callback::message &callback)
{
	callback.fail("Invalid message number.");
}


void mail::smtp::readMessageContent(const vector<size_t> &messages,
				    bool peek,
				    enum mail::readMode readType,
				    mail::callback::message &callback)
{
	callback.fail("Invalid message number.");
}

void mail::smtp::readMessageContent(size_t messageNum,
				    bool peek,
				    const mail::mimestruct &msginfo,
				    enum mail::readMode readType,
				    mail::callback::message &callback)
{
	callback.fail("Invalid message number.");
}

void mail::smtp::readMessageContentDecoded(size_t messageNum,
					   bool peek,
					   const mail::mimestruct &msginfo,
					   mail::callback::message &callback)
{
	callback.fail("Invalid message number.");
}

size_t mail::smtp::getFolderIndexSize()
{
	return 0;
}

mail::messageInfo mail::smtp::getFolderIndexInfo(size_t messageNum)
{
	return mail::messageInfo();
}

void mail::smtp::saveFolderIndexInfo(size_t messageNumber,
				     const mail::messageInfo &info,
				     mail::callback &callback)
{
	callback.fail("Invalid message number.");
}

void mail::smtp::updateFolderIndexFlags(const vector<size_t> &messages,
					bool doFlip,
					bool enableDisable,
					const mail::messageInfo &flags,
					mail::callback &callback)
{
	callback.fail("Invalid message number.");
}

void mail::smtp::updateFolderIndexInfo(mail::callback &callback)
{
	callback.success("OK");
}

void mail::smtp::removeMessages(const std::vector<size_t> &messages,
				mail::callback &cb)
{
	updateFolderIndexInfo(cb);
}

void mail::smtp::copyMessagesTo(const vector<size_t> &messages,
				mail::folder *copyTo,
				mail::callback &callback)
{
	callback.fail("Invalid message number.");
}

void mail::smtp::searchMessages(const mail::searchParams &searchInfo,
				mail::searchCallback &callback)
{
	vector<size_t> empty;

	callback.success(empty);
}

//////////////////////////////////////////////////////////////////////////////
//
// Another message is ready to go out.  Called by mail::smtpFolder::go,
// and one other place.

void mail::smtp::send(FILE *tmpfile, mail::smtpInfo &info,
		      mail::callback *callback, bool flag8bit)
{
	pingTimeout=0;

	struct messageQueueInfo msgInfo;

	msgInfo.message=tmpfile;
	msgInfo.messageInfo=info;
	msgInfo.callback=callback;
	msgInfo.flag8bit=flag8bit;

	bool wasEmpty=sendQueue.empty();

	sendQueue.push(msgInfo);

	if (wasEmpty && ready2send)
		startNextMessage();
}

void mail::smtp::startNextMessage()
{
	if (fatalError)
	{
		messageProcessed(500, "500 A fatal error occurred while connected to remote server.");
		return;
	}

	socketWrite("RSET\r\n");
	installHandler(&mail::smtp::rsetResponse);
}

void mail::smtp::rsetResponse(int result, string message)
{
	struct messageQueueInfo &info=sendQueue.front();

	if (info.message == NULL) // Just a NO-OP ping.
	{
		sendQueue.pop();

		if (!sendQueue.empty())
			startNextMessage();
		else
			pingTimeout=time(NULL) + INACTIVITY_TIMEOUT;
		return;
	}

	switch (result / 100) {
	case 1:
	case 2:
	case 3:
		break;
	default:
		messageProcessed(result, message);
		return;
	}

	if (!validString(info.messageInfo.sender))
		return;

	{
		vector<string>::iterator b=info.messageInfo.recipients.begin(),
			e=info.messageInfo.recipients.end();

		while (b != e)
			if (!validString(*b++))
				return;
	}

	{
		map<string, string>::iterator
			b=info.messageInfo.options.begin(),
			e=info.messageInfo.options.end();

		while (b != e)
			if (!validString((*b++).second))
				return;
	}

	string mailfrom="MAIL FROM:<" + info.messageInfo.sender + ">";

	if (hasCapability("8BITMIME"))
		mailfrom += info.flag8bit ? " BODY=8BITMIME":" BODY=7BIT";

	if (hasCapability("SIZE"))
	{
		long pos;

		if (fseek(info.message, 0L, SEEK_END) < 0 ||
		    (pos=ftell(info.message)) < 0)
		{
			messageProcessed(500, strerror(errno));
			return;
		}

		pos= pos < 1024 * 1024 ? pos * 70 / 68: pos/68 * 70;

		string buffer;

		{
			ostringstream o;

			o << pos;
			buffer=o.str();
		}

		mailfrom += " SIZE=";
		mailfrom += buffer;
	}

	if (info.messageInfo.options.count("DSN") > 0 ||
	    info.messageInfo.options.count("RET") > 0)
	{
		if (!hasCapability("DSN"))
		{
			messageProcessed(500, "500 This mail server does not support delivery status notifications.");
			return;
		}
	}

	if (hasCapability("DSN"))
	{
		if (info.messageInfo.options.count("RET") > 0)
			mailfrom += " RET=" + info.messageInfo.options
				.find("RET")->second;
	}

	if (hasCapability("XVERP=COURIER"))
	{
		if (info.messageInfo.options.count("VERP") > 0)
			mailfrom += " VERP";
	}

	if (info.messageInfo.options.count("SECURITY") > 0)
	{
		string sec=info.messageInfo.options.find("SECURITY")->second;

		if (info.messageInfo.options.count("XSECURITY=" + sec)  == 0)
		{
			messageProcessed(500, "500 requested security not available.");
			return;
		}

		mailfrom += " SECURITY=" + sec;
	}

	pipelineCmd=mailfrom;

	mailfrom += "\r\n";

	pipelineCount=0;
	installHandler(&mail::smtp::mailfromResponse);
	socketWrite(mailfrom);
}

// RCPT TO handler in pipeline mode.

void mail::smtp::pipelinedResponse(int result, string message)
{
	switch (result / 100) {
	case 1:
	case 2:
	case 3:
		break;
	default:
		if (pipelineError.size() == 0)
			pipelineError=sendQueue.front().messageInfo
				.recipients[rcptCount] + ": " + message;
		break;
	}

	++rcptCount;
	if (--pipelineCount || fatalError)
		return; // More on the way

	if (pipelineError.size() > 0)
		messageProcessed(500, pipelineError);
	else
	{
		installHandler(&mail::smtp::dataResponse);
		socketWrite("DATA\r\n");
	}
}

// MAIL FROM response. RCPT TO response in non-pipelined mode.

void mail::smtp::mailfromResponse(int result, string message)
{
	switch (result / 100) {
	case 1:
	case 2:
	case 3:
		break;
	default:
		messageProcessed(result, pipelineCmd + ": " + message);
		return;
	}

	struct messageQueueInfo &info=sendQueue.front();

	rcptCount=0;
	if (pipelineCount == 0 && hasCapability("PIPELINING") &&
	    smtpLoginInfo.options.count("nopipelining") == 0)
	{
		string rcptto="";

		vector<string>::iterator b=info.messageInfo.recipients.begin(),
			e=info.messageInfo.recipients.end();

		pipelineError="";

		while (b != e)
		{
			rcptto += "RCPT TO:<" + *b++ + ">";

			if (info.messageInfo.options.count("DSN") > 0)
				rcptto += " NOTIFY="
					+ info.messageInfo.options.find("DSN")
					->second;
			rcptto += "\r\n";
			++pipelineCount;
		}

		installHandler(&mail::smtp::pipelinedResponse);
		socketWrite(rcptto);
		return;
	}

	if (pipelineCount < info.messageInfo.recipients.size())
	{
		pipelineCmd=info.messageInfo.recipients[pipelineCount++];
		string rcptto= "RCPT TO:<" + pipelineCmd + ">";

		if (info.messageInfo.options.count("DSN") > 0)
			rcptto += " NOTIFY="
				+ info.messageInfo.options.find("DSN")
				->second;

		rcptto += "\r\n";
		socketWrite(rcptto);
		return;
	}

	installHandler(&mail::smtp::dataResponse);
	socketWrite("DATA\r\n");
}

// 1st DATA response

void mail::smtp::dataResponse(int result, string message)
{
	switch (result / 100) {
	case 1:
	case 2:
	case 3:
		break;
	default:
		messageProcessed(result, message);
		break;
	}

	long fpos;

	if ( fseek(sendQueue.front().message, 0L, SEEK_END) < 0 ||
	     (fpos=ftell(sendQueue.front().message)) < 0 ||
	     fseek(sendQueue.front().message, 0L, SEEK_SET) < 0 ||
	     (myBlaster=new Blast(this)) == NULL)

	{
		fatalError=true;
		messageProcessed(500, strerror(errno));
		return;
	}

	myBlaster->bytesTotal=fpos;
	installHandler(&mail::smtp::data2Response);
	socketWrite(myBlaster);
}

void mail::smtp::data2Response(int result, string message)
{
	if (myBlaster)
		myBlaster->mySmtp=NULL;
	installHandler(NULL); // Nothing more.
	messageProcessed(result, message);
}

bool mail::smtp::validString(string s)
{
	string::iterator b=s.begin(), e=s.end();

	while (b != e)
	{
		if ((int)(unsigned char)*b <= ' ' || *b == '>' || *b == '<')
		{
			messageProcessed(500, "500 Invalid character in message sender, recipient, or options.");
			return false;
		}
		b++;
	}
	return true;
}

void mail::smtp::messageProcessed(int result, string message)
{
	struct messageQueueInfo &info=sendQueue.front();

	mail::callback *callback=info.callback;
	fclose(info.message);

	try {
		sendQueue.pop();
		if (!sendQueue.empty())
			startNextMessage();
		else
			pingTimeout=time(NULL) + INACTIVITY_TIMEOUT;

		switch (result / 100) {
		case 1:
		case 2:
		case 3:
			callback->success(message);
			break;
		default:
			callback->fail(message);
			break;
		}

	} catch (...) {
		callback->fail("An exception has occurred while processing message.");
	}
}

string mail::smtp::translatePath(string path)
{
	return "INBOX"; // NOOP
}

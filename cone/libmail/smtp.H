/*
** Copyright 2002-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smtp_H
#define libmail_smtp_H

#include "libmail_config.h"
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include	"mail.H"
#include	<sys/types.h>

#include	"smtpinfo.H"
#include	"logininfo.H"
#include	"fd.H"
#include	<stdio.h>
#include	<time.h>
#include	<list>
#include	<string>
#include	<set>
#include	<queue>
#include	<vector>

LIBMAIL_START

///////////////////////////////////////////////////////////////////////////
//
// An SMTP implementation
//

class smtp : public fd, public loginInfo::callbackTarget {

private:
	void resumed() override;
	void handler(std::vector<pollfd> &fds, int &timeout) override;

	std::set<std::string> capabilities;

	bool orderlyShutdown;

	loginInfo smtpLoginInfo;

	int socketRead(const std::string &readbuffer) override;

	void disconnect(const char *reason) override;
	const char *application_protocol() const override;

	std::list<std::string> smtpResponse;
	// SMTP server response being accumulated.

	void (smtp::*responseHandler)(int numCode,
					    std::string multilineResponse);
	// Handler called when an SMTP response is received in entirety

	void installHandler(void (smtp::*)(int numCode,
						 std::string multilineResponse));
	// Install next handler.

	time_t responseTimeout;
	// SMTP timeout

	void error(std::string);	/* Login failed */
	void success(std::string);	/* Login succeeded */

	void howdy(const char *); /* send ehlo/helo */

	void greetingResponse(int, std::string);
	void ehloResponse(int, std::string);
	void heloResponse(int, std::string);
	void auth_external_response(int, std::string);
	void quitResponse(int, std::string);

	int hmac_method_index;
	/* Next HMAC authentication hash function to try */

	void authenticate_hmac();
	void hmacResponse(int, std::string);

	void starttls();
	void starttlsResponse(int, std::string);

	void authenticate_login();

	void begin_auth();
	void begin_auth_nonexternal();

	void loginInfoCallback(std::string str) override;
	void loginInfoCallbackCancel() override;


	void loginUseridResponse(int, std::string);
	void loginPasswdResponse(int, std::string);
	void loginResponse(int, std::string);

	void authenticated();

	bool ready2send;	/* Fully logged in */

	struct messageQueueInfo {
		FILE *message;
		mail::smtpInfo messageInfo;
		mail::callback *callback;
		bool flag8bit;
	};

	std::queue<messageQueueInfo> sendQueue;
	/* Queue of messages waiting to go out */

	bool fatalError;	// Fatal connection error occured
	time_t pingTimeout;	// When idling, ping occasionally

	// Custom WriteBuffer that spits out the whole message

	class Blast : public fd::WriteBuffer {

		bool eofSent;
	public:
		smtp *mySmtp;

		char lastChar;

		size_t bytesTotal;
		size_t bytesDone;

		Blast(smtp *smtpArg);
		~Blast();

		bool fillWriteBuffer() override;
	};

	Blast *myBlaster;

public:
	friend class Blast;

	smtp(std::string url, std::string passwd,
	     std::vector<std::string> &certificates,
	     mail::callback &callback,
	     mail::callback::disconnect &disconnectCallback,
	     loginCallback *loginCallbackFunc);

	smtp(const smtp &); // UNDEFINED
	smtp &operator=(const smtp &); // UNDEFINED

	~smtp();

	void logout(mail::callback &callback) override;
	void checkNewMail(mail::callback &callback) override;
	bool hasCapability(std::string capability) override;
	std::string getCapability(std::string capability) override;

	mail::folder *folderFromString(std::string) override;

	void readTopLevelFolders(mail::callback::folderList &callback1,
				 mail::callback &callback2) override;

	void findFolder(std::string folder,
			class mail::callback::folderList &callback1,
			class mail::callback &callback2) override;
	std::string translatePath(std::string path) override;

	mail::folder *getSendFolder(const mail::smtpInfo &info,
				    const mail::folder *folder,
				    std::string &errmsg) override;


	void readMessageAttributes(const std::vector<size_t> &messages,
				   MessageAttributes attributes,
				   mail::callback::message &callback) override;

	void readMessageContent(const std::vector<size_t> &messages,
				bool peek,
				enum mail::readMode readType,
				mail::callback::message &callback) override;

	void readMessageContent(size_t messageNum,
				bool peek,
				const mimestruct &msginfo,
				enum mail::readMode readType,
				mail::callback::message &callback) override;

	void readMessageContentDecoded(size_t messageNum,
				       bool peek,
				       const mimestruct &msginfo,
				       mail::callback::message &callback) override;

	size_t getFolderIndexSize() override;
	mail::messageInfo getFolderIndexInfo(size_t) override;

	void saveFolderIndexInfo(size_t,
				 const mail::messageInfo &,
				 mail::callback &) override;

	void updateFolderIndexFlags(const std::vector<size_t> &messages,
				    bool doFlip,
				    bool enableDisable,
				    const mail::messageInfo &flags,
				    mail::callback &callback) override;

	void updateFolderIndexInfo(mail::callback &) override;
	void removeMessages(const std::vector<size_t> &messages,
			    callback &cb) override;

	void copyMessagesTo(const std::vector<size_t> &messages,
			    mail::folder *copyTo,
			    mail::callback &callback) override;

	void searchMessages(const searchParams &searchInfo,
			    searchCallback &callback) override;


	void send(FILE *tmpfile, mail::smtpInfo &info,
		  mail::callback *callback,
		  bool flag8bit);

private:
	void startNextMessage();
	void messageProcessed(int, std::string);

	void rsetResponse(int, std::string);
	bool validString(std::string s);

	std::string pipelineError;
	unsigned pipelineCount; // Count pipelined replies received
	unsigned rcptCount; // RCPT TO count.
	std::string pipelineCmd; // Command that got rejected.

	void pipelinedResponse(int, std::string);
	void mailfromResponse(int, std::string);

	void dataResponse(int, std::string);
	void data2Response(int, std::string);
};

LIBMAIL_END

#endif

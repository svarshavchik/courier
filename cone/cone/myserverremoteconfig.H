/*
** Copyright 2003-2007, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef myserverremoteconfig_H
#define myserverremoteconfig_H

#include "config.h"
#include "myserver.H"
#include "libmail/mail.H"
#include "libmail/structure.H"
#include <fstream>
#include <stdio.h>

////////////////////////////////////////////////////////////////////
//
// Remote configuration

class myServer::remoteConfig : public mail::callback::disconnect,
		public mail::callback::folder {

	mail::account *account;
	mail::folder *folder;

	std::ifstream lastFilenameSaved, lastMacroFilenameSaved;
public:
	remoteConfig();
	~remoteConfig();

	mail::account *login();

private:
	mail::account *login2();
public:
	void logout();

	std::string saveconfig(std::string filename,
			       std::string macrofilename);
	std::string loadconfig(std::string filename,
			       std::string macrofilename);

	// Inherited from callback::disconnect

	void disconnected(const char *errmsg);
	void servererror(const char *errmsg);

	// Inherited from callback::folder

	void messagesRemoved(std::vector< std::pair<size_t, size_t> > &);
	void messageChanged(size_t n);
	void newMessages();

private:
	std::string errmsg;

	bool saveconfig2(std::string filename,
			 std::string subjmarker,
			 std::ifstream &savedCpy);
	bool loadconfig2(std::string filename,
			 std::string subjmarker,
			 bool optional,
			 std::ifstream &savedCpy);

	bool configFileUnchanged(std::ifstream &savedCpy,
				 FILE *newCpy);
	// Loading a message from a remote folder.

	class loadCallback : public mail::callback::message {
	public:
		myServer::Callback &origCallback;

		mail::mimestruct mimeinfo;

		std::ofstream o;

		loadCallback(myServer::Callback &callback);
		~loadCallback();

		void success(std::string message);
		void fail(std::string message);

		void messageStructureCallback(size_t messageNumber,
					      const mail::mimestruct
					      &messageStructure);

		void messageTextCallback(size_t n, std::string text);
		void reportProgress(size_t bytesCompleted,
				    size_t bytesEstimatedTotal,

				    size_t messagesCompleted,
				    size_t messagesEstimatedTotal);
	};
};

#endif

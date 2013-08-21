/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef myserver_H
#define myserver_H

#include "config.h"
#include "hierarchy.H"

#include "libmail/mail.H"
#include "curses/timer.H"
#include "myreadfolders.H"
#include "certificates.H"

#include <string>
#include <vector>
#include <queue>
#include <map>

LIBMAIL_START

class address;

LIBMAIL_END

class myServerLoginCallback;

/////////////////////////////////////////////////////////////////////////
//
// A server/account.
//
// It's subclassed from mail::callback:disconnect, so that it receives
// server errors/disconnect notices.
//

class myServer : public mail::callback::disconnect, public Timer {

public:
	static std::vector<myServer *> server_list; // All servers

	static myServer *getServerByUrl(std::string url);
	// Search for a server with the given URL.

	static Hierarchy hierarchy; // The folder hierarchy, displayed

	class Task;
	class Callback;
	class CreateFolderCallback;
	class promptInfo;

	std::queue <Task *> tasks; // Background tasks for this server.

	myReadFolders topLevelFolders; // Default folders.

	std::string serverName;
	std::string serverDescr;
	std::string url;
	std::string password; // Saved by autologin
	std::string certificate;

	std::string newsrc;	// For nntp: accounts

	time_t mailCheckInterval; // How often to check for mail.

	mail::account *server; // NULL if not logged in.

	class myFolder *currentFolder; // NULL if no folder is open.

	Hierarchy::Server *hierarchyEntry; // Entry in the Hierarchy for me

private:
	// The per-server configuration data is kept in a map hashed by
	// setting name, with the contents being the setting value

	std::map<std::string, std::string> server_configuration;

	//
	// The per-folder configuration data is kept in a map hashed by
	// folder's path.  The map's contents is another map hashed by the
	// configuration setting name, with the contents being the
	// configuration setting value.
	//
	std::map<std::string, std::map<std::string, std::string>
	> folder_configuration;

public:
	bool updateServerConfiguration(std::string name, std::string value);
	std::string getServerConfiguration(std::string name);

	bool updateFolderConfiguration(const mail::folder *folder,
				       std::string name,
				       std::string value);

	bool updateFolderConfiguration(std::string path,
				       std::string name,
				       std::string value);

	std::string getFolderConfiguration(const mail::folder *folder,
				      std::string name);
	std::string getFolderConfiguration(std::string p, std::string name);

	void saveFolderConfiguration();
	// Allow multiple updates, without saving each time

	static std::vector<std::string> myAddresses;
	// Config - list of my addresses

	static bool isMyAddress(const class mail::address &);

	static std::vector<std::string> myListAddresses;
	// Config - mailing list addys

	static std::string customHeaders;
	// Comma-separated list of custom headers.  Hidden headers prefixed
	// by '*'

	static std::string smtpServerURL; // Config - SMTP server URL
	static std::string smtpServerCertificate;
	static std::string smtpServerPassword;

	static bool useIMAPforSending;
	// Config - use main MAIL acct to send mail


	static std::string remoteConfigURL;
	static std::string remoteConfigPassword;
	static std::string remoteConfigFolder;
	class remoteConfig;
	static remoteConfig *remoteConfigAccount;
	// Remote configuration file

	static Certificates *certs; // SSL client certs

	void find_cert_by_id(std::vector<std::string> &);
	static void find_cert_by_id(std::vector<std::string> &certArg,
				    std::string certificate);
	static void find_cert_by_url(std::string url,
				     std::vector<std::string> &);

	// Demoronization settings

	static enum DemoronizationType {
		DEMORON_OFF,
		DEMORON_MIN,
		DEMORON_MAX
	} demoronizationType;

	static enum postAndMailType {
		POSTANDMAIL_ASK,
		POSTANDMAIL_YES,
		POSTANDMAIL_NO
	} postAndMail;

	static void setDemoronizationType(DemoronizationType);

	// Detect removed folders automatically.  When a new folder
	// subhierarchy is opened, check for any saved folder configurations
	// which are inferior to the newly-opened folder hierarchy, but
	// are not found in the newly opened hierarchy.

	void openedHierarchy(Hierarchy::Entry *);
	void openedHierarchy(Hierarchy::Entry *,
			     std::vector<mail::folder *> &);

	void disconnect();

	void disconnected(const char *errmsg);
	void servererror(const char *errmsg);

	myServer(std::string name, std::string url);
	~myServer();

	static unsigned cmdcount; // Nesting level of events.

	// Main application event loop.
	static void eventloop(void);
	static bool eventloop(Callback &); // loop until event completes

	// Next screen to open.
	static void (*nextScreen)(void *);
	static void *nextScreenArg;

	static promptInfo prompt(promptInfo info);
	// Create a status line prompt, wait for a response.

	static char *configDir;
	// -c parameter.

	static std::string getConfigDir();
	static std::string getHomeDir();
	static std::string getConfigFilename();

	static void saveconfig(bool saveRemote=true,
			       bool installRemote=false);
	static void getBackupConfigFiles(std::vector<std::string> &);

	static void savepasswords(std::string);
	static bool loadpasswords(std::string);

	// saveconfig() code has some dependencies (libxml) that just add
	// too much baggage.  Compartmentalize them in a dummy class, to
	// avoid loading the xml include files.

	class config;
	friend class config;

	static bool loadconfig();

	void saveFolderIndex(myFolder *);

	std::string getCachedFilename(myFolder *, const char *);

	static bool loadFolderIndex(std::string,
				    std::map<std::string, size_t> &,
				    class myFolder *);
	static void initializeHierarchy();
	void addHierarchy(bool assignRows);
	void showHierarchy();
	void updateHierarchy();
private:
	void addTopLevelFolders();
	void addTopLevelFolder(std::string);
	void disconnectTasks();

public:
	void alarm(); // Inherited from Timer

	void addTopLevelFolderRedraw(std::string path);

	bool login(std::string passwordStr);
	bool login(myServerLoginCallback &);
	bool login(std::string password, myServerLoginCallback *);
	void openFolder(const mail::folder *, bool autoOpenDraft=false);

	static void logout();
	void serverLogout();

	static void reportProgress(size_t bytesCompleted,
				   size_t bytesEstimatedTotal,

				   size_t messagesCompleted,
				   size_t messagesEstimatedTotal);

	void checkNewMail();
	void finishCheckingNewMail();

private:
	static bool eventloop(Callback *);

};

#endif

/*
** Copyright 2003-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "config.h"
#include "curses/cursesstatusbar.H"
#include "curses/cursesmoronize.H"
#include "curseseditmessage.H"
#include "myserver.H"
#include "myserverremoteconfig.H"
#include "myservercallback.H"
#include "myserverpromptinfo.H"
#include "myfolder.H"
#include "myreferences.H"
#include "specialfolder.H"
#include "addressbook.H"
#include "gettext.H"
#include "nntpcommand.H"
#include "init.H"
#include "macros.H"
#include "htmlentity.h"
#include "libmail/rfc2047encode.H"
#include "libmail/rfc2047decode.H"
#include "libmail/mail.H"
#include "libmail/maildir.H"
#include "libmail/misc.H"
#include "passwordlist.H"
#include "globalkeys.H"
#include "gpg.H"
#include "tags.H"
#include "colors.H"
#include "certificates.H"
#include "tcpd/tlspasswordcache.h"
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <sstream>
#include <rfc822/rfc822.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <set>
#include <map>
#include <iomanip>

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

// Save/load server configuration

extern CursesStatusBar *statusBar;

#define INDEXFORMAT "2"

Demoronize *currentDemoronizer;

std::vector<std::string> myServer::myAddresses, myServer::myListAddresses;
std::string myServer::smtpServerURL;   // Config - SMTP server URL
std::string myServer::smtpServerCertificate; // Config - SMTP server certificate
std::string myServer::smtpServerPassword;	// Config (not saved) - SMTP password

bool myServer::useIMAPforSending; // Config


std::string myServer::remoteConfigURL;
std::string myServer::remoteConfigPassword;
std::string myServer::remoteConfigFolder;
myServer::remoteConfig *myServer::remoteConfigAccount=NULL;

myServer::DemoronizationType myServer::demoronizationType
=myServer::DEMORON_MIN;

myServer::postAndMailType myServer::postAndMail=myServer::POSTANDMAIL_ASK;

std::string myServer::getConfigFilename()
{
	return myServer::getConfigDir() + "/conerc";
}

static std::string getCacheIndexFilename()
{
	return myServer::getConfigDir() + "/cacherc";
}

static std::string getMacroFilename()
{
	return myServer::getConfigDir() + "/macros";
}

std::string PasswordList::masterPasswordFile()
{
	return myServer::getConfigDir() + "/passwords";
}

char *myServer::configDir=NULL;
extern std::string curdir();

std::string myServer::getConfigDir()
{
	if (configDir)
	{
		std::string c=configDir;

		if (*configDir != '/')
			c= curdir() + "/" + c;

		return c;
	}

	return getHomeDir() + "/.cone";
}

static void saveError(std::string filename)
{
	statusBar->status(Gettext(_("%1%: %2%"))
			  .arg(filename).arg(strerror(errno)));
	statusBar->beepError();
}

bool myServer::isMyAddress(const mail::address &cmpAddr)
{
	size_t j;

	for (j=0; j<myAddresses.size(); j++)
		if (cmpAddr == mail::address("", myAddresses[j]))
			return true;
	return false;
}

void myServer::setDemoronizationType(DemoronizationType t)
{
	Demoronize::demoron_t type=
		t == myServer::DEMORON_MIN ? Demoronize::minimum:
		t == myServer::DEMORON_MAX ? Demoronize::maximum:
		Demoronize::none;

	Demoronize *demoron=
		new Demoronize(unicode_default_chset(), type);

	if (currentDemoronizer)
		delete currentDemoronizer;

	currentDemoronizer=demoron;

	myServer::demoronizationType=t;
}

/////////////////////////////////////////////////////////////////////////
//
// Server configuration can be saved in a file, or into a remote folder.
//
// Firstly, create a configuration XML file, then see where it should go.
//
// If remoteDocPtr is not NULL, it gets all the remote config stuff
// (and doc gets just the local stuff).
//
// cachedoc gets the index of locally saved cache files.

class myServer::config {

public:

	xmlDocPtr doc; // Main config file
	xmlDocPtr cachedoc; // Index of cache files

	xmlDocPtr remoteDocPtr; // Remotely-saved configuration

	xmlDocPtr macros; // Macros

	std::set<std::string> urls; // URLs we've saved.

	config();
	~config();

	void save();

	// Conveniently encapsulate load function too

	static bool loadconfig(xmlDocPtr doc,
			       std::set<std::string> &cache_filenames,
			       std::set<std::string> &aux_filenames,
			       std::map<std::string, xmlNodePtr> &cacheMap,
			       std::map<std::string, xmlNodePtr> &remoteMap,
			       bool skipServerConfig,
			       std::set<std::string> &accountNamesLoaded);

	static void loadserver(xmlNodePtr root,
			       std::set<std::string> &cache_filenames,
			       std::set<std::string> &aux_filenames,
			       std::map<std::string, xmlNodePtr> &cacheMap,
			       std::set<std::string> &accountNamesLoaded);

	static void loadmacros(Macros *, xmlDocPtr);
};

myServer::config::config() : doc(NULL), cachedoc(NULL),
			     remoteDocPtr(NULL),
			     macros(NULL)
{
}

myServer::config::~config()
{
	if (doc)
		xmlFreeDoc(doc);
	if (cachedoc)
		xmlFreeDoc(cachedoc);
	if (remoteDocPtr)
		xmlFreeDoc(remoteDocPtr);
	if (macros)
		xmlFreeDoc(macros);
}

/////////////////////////////////////////////////////////////////////////
//
// When an error occured saving/loading remote configuration, show an error
// message, then wait for a keypress to clear the error message.

class ClearKeyHandler : public CursesKeyHandler {
public:
	ClearKeyHandler();
	~ClearKeyHandler();
	bool processKey(const Curses::Key &key);
};

ClearKeyHandler::ClearKeyHandler()
	: CursesKeyHandler(PRI_STATUSOVERRIDEHANDLER)
{
}

ClearKeyHandler::~ClearKeyHandler()
{
}

bool ClearKeyHandler::processKey(const Curses::Key &key)
{
	Curses::keepgoing=false;
	statusBar->clearstatus();
	return true;
}

//
// Read a list of available backup files.
//

void myServer::getBackupConfigFiles(std::vector<std::string> &f)
{
	std::string backupConfigFilename=getConfigFilename() + ".backup";

	int i;

	for (i=0; i<4; i++)
	{
		std::ostringstream o;

		o << backupConfigFilename;

		if (i > 0)
			o << "." << i;

		f.push_back(o.str());
	}
}

static int save_password_func(const char *p, size_t n, void *vp)
{
	std::ofstream *o=(std::ofstream *)vp;

	o->write(p, n);

	return 0;
}

void myServer::savepasswords(std::string password)
{
	config saveConfig;

	saveConfig.save();

	std::vector<std::string> url_strings;
	std::vector<std::string> password_strings;

	std::set<std::string>::iterator b=saveConfig.urls.begin(),
		e=saveConfig.urls.end();

	while (b != e)
	{
		std::string pwd;

		if (PasswordList::passwordList.get( *b, pwd) && pwd.size())
		{
			url_strings.push_back( *b );
			password_strings.push_back( pwd );
		}
		++b;
	}

	std::map<std::string, Certificates::cert>::iterator
		cb=certs->certs.begin(), ce=certs->certs.end();

	while (cb != ce)
	{
		url_strings.push_back("ssl:" + cb->first);

		password_strings.push_back(Gettext::toutf8(cb->second.name)
					   + "\n" +
					   cb->second.cert);
		++cb;
	}

	std::vector<const char *> urlp, passp;

	std::vector<std::string>::iterator sb=url_strings.begin(), se=url_strings.end();

	urlp.reserve(url_strings.size());
	passp.reserve(password_strings.size());

	while (sb != se)
	{
		urlp.push_back(sb->c_str());
		++sb;
	}

	sb=password_strings.begin();
	se=password_strings.end();
	while (sb != se)
	{
		passp.push_back(sb->c_str());
		++sb;
	}

	urlp.push_back(NULL);
	passp.push_back(NULL);

	std::string passFilename=PasswordList::masterPasswordFile();
	std::string tmpPassFilename=passFilename + ".tmp";


	std::ofstream o(tmpPassFilename.c_str());

	if (tlspassword_save(&urlp[0], &passp[0], password.c_str(),
			     save_password_func, &o)
	    || (o << std::flush).bad() || o.fail())
	{
		o.close();
		statusBar->clearstatus();
		statusBar->status(strerror(errno));
		statusBar->beepError();
		return;
	}
	o.close();
	rename(tmpPassFilename.c_str(), passFilename.c_str());
}

static int load_func(char *buf, size_t n, void *vp)
{
	std::ifstream *i=(std::ifstream *)vp;

	i->read(buf, n);

	if (i->bad())
		return -1;

	return i->gcount();
}

static void install_passwords(const char * const *uids,
			      const char * const *pws,
			      void *dummy)
{
	PasswordList::passwordList.loadPasswords(uids, pws);
}

bool myServer::loadpasswords(std::string password)
{
	std::string f=PasswordList::masterPasswordFile();

	std::ifstream i(f.c_str());

	return tlspassword_load(load_func, &i, password.c_str(),
				install_passwords, NULL) == 0;
}

void myServer::saveconfig(bool saveRemote, // true: both local and remote save
			  bool installRemote
			  // installRemote: true when importing a remote
			  // config.  saveconfig() saved the entire current
			  // configuration, but also saves remote config
			  // info.  After saveconfig() is done we restart
			  // and pick up the remote config info
			  )
{
	std::string configFilename=getConfigFilename();
	std::string tmpConfigFilename=configFilename + ".tmp";

	std::string cacheFilename=getCacheIndexFilename();
	std::string tmpCacheFilename=cacheFilename + ".tmp";

	std::string backupConfigFilename=getConfigFilename() + ".backup";
	struct stat stat_buf;

	std::string macroFilename=getMacroFilename();
	std::string tmpMacroFilename=macroFilename + ".tmp";

	// Once a day make a backup copy of the configuration file.

	if (stat(backupConfigFilename.c_str(), &stat_buf) < 0 ||
	    stat_buf.st_mtime + 60 * 24 * 24 < time(NULL))
	{

		config saveConfig;

		saveConfig.save();

		unlink(tmpConfigFilename.c_str());

		if (!xmlSaveFile(tmpConfigFilename.c_str(), saveConfig.doc))
		{
			saveError(tmpConfigFilename);
			return;
		}

		std::string lastBackup=backupConfigFilename + ".3";

		int i;

		for (i=2; i >= 0; --i)
		{
			std::ostringstream o;

			o << backupConfigFilename;

			if (i > 0)
				o << "." << i;

			std::string s=o.str();

			rename(s.c_str(), lastBackup.c_str());
			lastBackup=s;
		}
		rename (tmpConfigFilename.c_str(),
			backupConfigFilename.c_str());
	}

	unlink(tmpConfigFilename.c_str());
	unlink(tmpCacheFilename.c_str());

	//
	// This is really an if (), but a while gives a convenient
	// mechanism to retry upon an error.
	//

	while (remoteConfigAccount && !installRemote)
	{
		config savedConfig;

		savedConfig.remoteDocPtr=xmlNewDoc((xmlChar *)"1.0");

		if (!savedConfig.remoteDocPtr)
			outofmemory();

		xmlNodePtr n=xmlNewDocNode(savedConfig.remoteDocPtr, NULL,
					   (xmlChar *)"CONE", NULL);

		if (!n)
			outofmemory();

		savedConfig.remoteDocPtr->children=n;

		savedConfig.save();

		std::string remoteMacroName="";

		if (savedConfig.macros)
		{
			remoteMacroName=tmpMacroFilename;

			if (!xmlSaveFile(tmpMacroFilename.c_str(),
					 savedConfig.macros))
			{
				saveError(macroFilename);
				return;
			}
		}

		std::string errmsg="";

		if (saveRemote) // Wanna save all the remote accts.
		{
			unlink(tmpConfigFilename.c_str());

			if (!xmlSaveFile(tmpConfigFilename.c_str(),
					 savedConfig.remoteDocPtr))
			{
				if (remoteMacroName.size() > 0)
					unlink(remoteMacroName.c_str());
				saveError(tmpConfigFilename);
				return;
			}
			
			errmsg=remoteConfigAccount
				->saveconfig(tmpConfigFilename,
					     remoteMacroName);
		}

		if (remoteMacroName.size() > 0)
			unlink(remoteMacroName.c_str());

		if (errmsg.size() == 0)
		{
			unlink(tmpConfigFilename.c_str());

			if (!xmlSaveFile(tmpConfigFilename.c_str(),
					 savedConfig.doc)
			    || !xmlSaveFile(tmpCacheFilename.c_str(),
					    savedConfig.cachedoc))
			{
				saveError(tmpConfigFilename);
				return;
			}
			unlink(macroFilename.c_str()); // Saved in remote acct
			break; // Save the local accts.
		}


		{
			ClearKeyHandler kh;

			statusBar->clearstatus();
			statusBar->status(Gettext(_("The following error occured "
						    "when trying to save the "
						    "configuration on a remote "
						    "folder:\n\n%1%\n\n"
						    "You may choose to try again, "
						    "or turn off remote configuration altogether."))
					  << errmsg);

			bool wasKeepGoing=Curses::keepgoing;

			myServer::eventloop();
			Curses::keepgoing=wasKeepGoing;
		}

		myServer::promptInfo promptInfo=
			myServer::prompt(myServer::promptInfo(_("Try again (otherwise cancel)? (Y/N) "))
					 .yesno());

		if (promptInfo.abortflag ||
		    (std::string)promptInfo == "Y")
			continue;

		remoteConfigAccount->logout();
		delete remoteConfigAccount;
		remoteConfigAccount=NULL;

		myServer::remoteConfigURL="";
		myServer::remoteConfigPassword="";
		myServer::remoteConfigFolder="";
	}

	// The other shoe: no remote configuration.

	if (!remoteConfigAccount || installRemote)
	{
		config savedConfig;

		savedConfig.save();

		if (!xmlSaveFile(tmpConfigFilename.c_str(), savedConfig.doc)
		    || !xmlSaveFile(tmpCacheFilename.c_str(),
				    savedConfig.cachedoc))
		{
			saveError(tmpConfigFilename);
			return;
		}

		if (!installRemote)
		{
		// Save macros only if we're completely local

			if (savedConfig.macros)
			{
				if (!xmlSaveFile(tmpMacroFilename.c_str(),
						 savedConfig.macros) ||
				    rename(tmpMacroFilename.c_str(),
					   macroFilename.c_str()) < 0)
				{
					saveError(macroFilename);
					return;
				}
			}
			else
				unlink(macroFilename.c_str());
		}
	}

	if (rename(tmpConfigFilename.c_str(),
		   configFilename.c_str()) < 0
	    || rename(tmpCacheFilename.c_str(),
		      cacheFilename.c_str()) < 0)
		saveError(tmpConfigFilename);
}

void myServer::config::save()
{
	urls.clear();

	// Make sure that all GPG passphrases are saved:
	{
		GPG::Key_iterator b=GPG::gpg.secret_keys.begin(),
			e=GPG::gpg.secret_keys.end();

		while (b != e)
			urls.insert("gpg:" + (*b++).fingerprint);
		urls.insert("decrypt:");
	}

	if (myServer::smtpServerURL.size() > 0)
		urls.insert(myServer::smtpServerURL);

	std::vector<myServer *>::iterator
		serverListB=server_list.begin(),
		serverListE=server_list.end();

	doc=xmlNewDoc((xmlChar *)"1.0");
	cachedoc=xmlNewDoc((xmlChar *)"1.0");

	std::string myCharset=unicode_default_chset();

#define REMOTE (remoteDocPtr ? remoteDocPtr:doc)

	if (!doc || !cachedoc)
		LIBMAIL_THROW((strerror(errno)));

	xmlNodePtr d=xmlNewDocNode(doc, NULL,
				   (xmlChar *)"CONE", NULL);
	if (!d)
		outofmemory();

	doc->children=d;

	xmlNodePtr cd=xmlNewDocNode(cachedoc, NULL,
				    (xmlChar *)"CONECACHE", NULL);

	if (!cd)
		outofmemory();

	cachedoc->children=cd;

	xmlNodePtr f;

	if (remoteConfigAccount)
	{
		urls.insert(myServer::remoteConfigURL);
		std::string sUrl=mail::rfc2047::encode(myServer::remoteConfigURL,
						  myCharset);

		std::string sFolder=mail::rfc2047::encode(myServer::
						     remoteConfigFolder,
						     myCharset);
		if (!xmlSetProp(d, (xmlChar *)"REMOTECONFIG",
				(xmlChar *)sUrl.c_str()) ||
		    !xmlSetProp(d, (xmlChar *)"REMOTEFOLDER",
				(xmlChar *)sFolder.c_str()))
			outofmemory();

	}

	while (serverListB != serverListE)
	{
		myServer *p= *serverListB++;

		urls.insert(p->url);

		std::string sName=mail::rfc2047::encode(p->serverName,
						   myCharset);
		std::string sUrl=mail::rfc2047::encode(p->url, myCharset);

		std::string sNewsrc=mail::rfc2047::encode(p->newsrc,
						     myCharset);
		std::string sCert=mail::rfc2047::encode(p->certificate,
						   myCharset);

		xmlNodePtr serverNode=xmlNewNode (NULL,
						  (xmlChar *)
						  "SERVER");

		xmlNodePtr serverCacheNode=xmlNewNode(NULL,
						      (xmlChar *)
						      "SERVER");


		// Until we change our mind, save this acct's info in
		// the main config file.
		xmlDocPtr saveServerDoc=doc;


		if (remoteDocPtr && mail::account::isRemoteUrl(p->url))
		{
			// Insert a stub in its place

			xmlNodePtr remoteServerNode=xmlNewNode(NULL,
							       (xmlChar *)
							       "REMOTESERVER");

			if (!remoteServerNode ||
			    !xmlSetProp(remoteServerNode,
					(xmlChar *)"NAME",
					(xmlChar *)sName.c_str()) ||

			    (sCert.size() > 0 &&
			     !xmlSetProp(remoteServerNode,
					 (xmlChar *)"CERTIFICATE",
					 (xmlChar *)sCert.c_str())) ||
			    !xmlAddChild(doc->children, remoteServerNode))
			{
				if (remoteServerNode)
					xmlFreeNode(remoteServerNode);
				outofmemory();
			}

			// Nope, save it in the remote file.

			saveServerDoc=remoteDocPtr;
		}

		if (!serverNode || !serverCacheNode ||
		    !xmlSetProp(serverNode,
				(xmlChar *)"NAME",
				(xmlChar *)sName.c_str()) ||
		    !xmlSetProp(serverNode,
				(xmlChar *)"URL",
				(xmlChar *)sUrl.c_str()) ||
		    (sCert.size() > 0 &&
		     !xmlSetProp(serverNode,
				 (xmlChar *)"CERTIFICATE",
				 (xmlChar *)sCert.c_str())) ||
		    !xmlSetProp(serverCacheNode,
				(xmlChar *)"NAME",
				(xmlChar *)sName.c_str()) ||
		    (sNewsrc.size() > 0 &&
		     !xmlSetProp(serverNode,
				 (xmlChar *)
				 (*sUrl.c_str() == 'p' ?
				  "MAILDIR":"NEWSRC"),
				 (xmlChar *)sNewsrc.c_str())) ||
		    !xmlAddChild(saveServerDoc->children, serverNode) ||
		    !xmlAddChild(cd, serverCacheNode))
		{
			if (serverNode)
				xmlFreeNode(serverNode);
			if (serverCacheNode)
				xmlFreeNode(serverCacheNode);

			outofmemory();
		}

		myReadFolders::iterator fb=p->topLevelFolders.begin(),
			fe=p->topLevelFolders.end();

		while (fb != fe)
		{
			std::string path=mail::rfc2047::encode(*fb++,
							  myCharset);

			xmlNodePtr f=xmlNewNode(NULL,
						(xmlChar *)"NAMESPACE"
						);
			if (!f ||
			    !xmlSetProp(f, (xmlChar *)"PATH",
					(xmlChar *)path.c_str()) ||
			    !xmlAddChild(serverNode, f))
			{
				if (f)
					xmlFreeNode(f);
				outofmemory();
			}
		}

		std::map<std::string, std::string>::iterator
			scb=p->server_configuration.begin(),
			sce=p->server_configuration.end();

		while (scb != sce)
		{
			std::string name=mail::rfc2047::encode(scb->first,
							  myCharset);
			std::string value=mail::rfc2047::encode(scb->second,
							   myCharset);

			xmlNodePtr f=xmlNewNode(NULL,
						(xmlChar *)"SETTING");

			if (!f || !xmlSetProp(f, (xmlChar *)"NAME",
					      (xmlChar *)name.c_str())
			    || !xmlSetProp(f, (xmlChar *)"VALUE",
					   (xmlChar *)value.c_str())
			    || !xmlAddChild(serverNode, f))
			{
				if (f)
					xmlFreeNode(f);
				outofmemory();
			}

			scb++;
		}

		std::map<std::string, std::map<std::string, std::string> >::iterator
			ffb=p->folder_configuration.begin(),
			ffe=p->folder_configuration.end();

		while (ffb != ffe)
		{
			std::string path=mail::rfc2047::encode(ffb->first,
							  myCharset);

			std::map<std::string, std::string>
				::iterator cb=ffb->second.begin(),
				ce=ffb->second.end();

			ffb++;

			xmlNodePtr f=xmlNewNode(NULL,
						(xmlChar *)"FOLDER");

			if (!f || !xmlSetProp(f, (xmlChar *)"PATH",
					      (xmlChar *)path.c_str())
			    || !xmlAddChild(serverNode, f))
			{
				if (f)
					xmlFreeNode(f);
				outofmemory();
			}

			xmlNodePtr cachePtr=NULL;

			while (cb != ce)
			{
				std::string name=
					mail::rfc2047::encode(cb->first,
							      myCharset);

				std::string val=
					mail::rfc2047::encode(cb->second,
							      myCharset);

				if (name == "INDEX" ||
				    name == "SNAPSHOT")
				{
					// Save cache filenames in the local
					// cache index file.

					if (!cachePtr)
					{
						cachePtr=
							xmlNewNode(NULL,
								   (xmlChar *)"FOLDER");
						if (!cachePtr ||
						    !xmlSetProp(cachePtr,
								(xmlChar *)"PATH",
								(xmlChar *)path.c_str())
						    || !xmlAddChild(serverCacheNode, cachePtr))
						{
							if (cachePtr)
								xmlFreeNode(cachePtr);
							outofmemory();
						}
					}

					if (!xmlSetProp(cachePtr,
							(xmlChar *)
							name.c_str(),
							(xmlChar *)
							val.c_str()))
						outofmemory();

				}
				else if (name == "FILTER" && val.size() > 0)
				{
					if (!xmlNewTextChild(f, NULL,
							     (xmlChar *)
							     "FILTER",
							     (xmlChar *)
							     val.c_str()))
						outofmemory();
				}
				else if (!xmlSetProp(f,
						     (xmlChar *)
						     name.c_str(),
						     (xmlChar *)
						     val.c_str()
						     ))
					outofmemory();
				cb++;
			}
		}
	}


	std::vector<std::string>::iterator sb, se;

	sb=myAddresses.begin();
	se=myAddresses.end();

	while (sb != se)
	{
		std::string address= mail::rfc2047::encode(*sb++, myCharset);

		if (!xmlNewTextChild(REMOTE->children, NULL,
				     (xmlChar *)"ADDRESS",
				     (xmlChar *)address.c_str()))
			outofmemory();
	}

	sb=myListAddresses.begin();
	se=myListAddresses.end();

	while (sb != se)
	{
		std::string address= mail::rfc2047::encode(*sb++, myCharset);

		if (!xmlNewTextChild(REMOTE->children, NULL,
				     (xmlChar *)"LISTADDRESS",
				     (xmlChar *)address.c_str()))
			outofmemory();
	}

	std::map<std::string, SpecialFolder>::iterator b, e;
	b=SpecialFolder::folders.begin();
	e=SpecialFolder::folders.end();

	while (b != e)
	{
		std::string id=b->first;
		SpecialFolder &sf=b->second;

		std::vector<SpecialFolder::Info>::iterator sb, se;

		sb=sf.folderList.begin();
		se=sf.folderList.end();

		while (sb != se)
		{
			urls.insert(sb->serverUrl);

			xmlNodePtr f=xmlNewNode(NULL,(xmlChar *)
						"SPECIALFOLDER");

			std::string idString=
				mail::rfc2047::encode(id, myCharset);

			std::string serverString=
				mail::rfc2047::encode(sb->serverUrl,
						      myCharset);

			std::string pathString=
				mail::rfc2047::encode(sb->serverPath,
						      myCharset);

			std::string nameString=
				mail::rfc2047::encode(sb->nameUTF8,
						      "utf-8");
			if (!f || !xmlSetProp(f, (xmlChar *)"ID",
					      (xmlChar *)
					      idString.c_str())
			    || !xmlSetProp(f, (xmlChar *)"SERVER",
					   (xmlChar *)serverString
					   .c_str())
			    || !xmlSetProp(f, (xmlChar *)"PATH",
					   (xmlChar *)pathString
					   .c_str())
			    || !xmlSetProp(f, (xmlChar *)"NAME",
					   (xmlChar *)nameString
					   .c_str())
			    || !xmlSetProp(f, (xmlChar *)"ARCHIVED",
					   (xmlChar *)
					   sb->lastArchivedMonth
					   .c_str())
			    || !xmlAddChild((mail::account
					     ::isRemoteUrl(sb->serverUrl)
					     ? REMOTE:doc)->children, f))
			{
				if (f)
					xmlFreeNode(f);
				outofmemory();
			}

			sb++;
		}
		b++;
	}

	urls.insert(myServer::smtpServerURL);

	std::string smtpUrl=mail::rfc2047::encode(myServer::smtpServerURL,
					     myCharset);

	f=xmlNewNode(NULL, (xmlChar *)"SMTP");

	if (!f || !xmlSetProp(f, (xmlChar *)"URL",
			      (xmlChar *)smtpUrl.c_str())
	    || (myServer::useIMAPforSending &&
		!xmlSetProp(f, (xmlChar *)"USEIMAP",
			    (xmlChar *)"TRUE"))

	    || (myServer::smtpServerCertificate.size() > 0 &&
		!xmlSetProp(f,
			    (xmlChar *)"CERTIFICATE",
			    (xmlChar *)myServer::smtpServerCertificate
			    .c_str()))
	    || !xmlAddChild(REMOTE->children, f))
	{
		if (f)
			xmlFreeNode(f);
		outofmemory();
	}

	{
		std::string c=mail::rfc2047
			::encode(nntpCommandFolder::nntpCommand,
				 myCharset);

		f=xmlNewNode(NULL, (xmlChar *)"NNTP");

		if (!f || !xmlSetProp(f, (xmlChar *)"COMMAND",
				      (xmlChar *)c.c_str())
		    || !xmlAddChild(REMOTE->children, f))
		{
			if (f)
				xmlFreeNode(f);
			outofmemory();
		}
	}


	f=xmlNewNode(NULL, (xmlChar *)"DICTIONARY");
		
	if (!f || !xmlSetProp(f, (xmlChar *)"NAME",
			      (xmlChar *)
			      spellCheckerBase->dictionaryName.c_str())
	    || !xmlAddChild(REMOTE->children, f))
	{
		if (f)
			xmlFreeNode(f);
		outofmemory();
	}

	{
		std::ostringstream o;

		o << CursesEditMessage::autosaveInterval;

		std::string s=o.str();

		std::ostringstream o1, o2;

		o1 << Watch::defaultWatchDays;
		o2 << Watch::defaultWatchDepth;


		std::string o1s=o1.str();
		std::string o2s=o2.str();

		std::string gpgopt1=mail::rfc2047
			::encode(GPG::gpg.extraEncryptSignOptions, myCharset);
		std::string gpgopt2=mail::rfc2047
			::encode(GPG::gpg.extraDecryptVerifyOptions,
				 myCharset);

		f=xmlNewNode(NULL, (xmlChar *)"OPTIONS");
		if (!f || !xmlSetProp(f, (xmlChar *)"DEMORONIZATION",
				      (xmlChar *)(myServer::demoronizationType
						  == myServer::DEMORON_MIN
						  ? "MIN":
						  myServer::demoronizationType
						  == myServer::DEMORON_MAX
						  ? "MAX":"OFF"))
		    || !xmlSetProp(f, (xmlChar *)"POSTANDMAIL",
				   (xmlChar *)
				   (myServer::postAndMail ==
				    myServer::POSTANDMAIL_YES ? "Y":
				    myServer::postAndMail ==
				    myServer::POSTANDMAIL_NO ? "N":"ASK"))
		    || !xmlSetProp(f, (xmlChar *)"SUSPEND",
				   (xmlChar *)Curses::suspendCommand.c_str())
		    || !xmlSetProp(f, (xmlChar *)"EDITOR",
				   (xmlChar *)CursesEditMessage::
				   externalEditor.c_str())
		    || (GlobalKeys::quitNoPrompt &&
			!xmlSetProp(f, (xmlChar *)"QUITNOPROMPT",
				    (xmlChar *)"YES"))
		    || (CursesMoronize::enabled &&
			!xmlSetProp(f, (xmlChar *)"SMARTSHORTCUTS",
				    (xmlChar *)"YES"))
		    || !xmlSetProp(f, (xmlChar *)"CUSTOMHEADERS",
				   (xmlChar *)myServer::customHeaders.c_str())
		    || !xmlSetProp(f, (xmlChar *)"AUTOSAVE",
				   (xmlChar *)s.c_str())
		    || !xmlSetProp(f, (xmlChar *)"GPGENCRYPTSIGNOPTIONS",
				   (xmlChar *)gpgopt1.c_str())
		    || !xmlSetProp(f, (xmlChar *)"GPGDECRYPTVERIFYOPTIONS",
				   (xmlChar *)gpgopt2.c_str())
		    || !xmlSetProp(f, (xmlChar *)"WATCHDAYS",
				   (xmlChar *)o1s.c_str())
		    || !xmlSetProp(f, (xmlChar *)"WATCHDEPTH",
				   (xmlChar *)o2s.c_str())
		    || !xmlAddChild(REMOTE->children, f))

		{
			if (f)
				xmlFreeNode(f);
			outofmemory();
		}

		f=xmlNewNode(NULL, (xmlChar *)"COLORS");

		if (!f || !xmlAddChild(REMOTE->children, f))
		{
			if (f)
				xmlFreeNode(f);
			outofmemory();
		}

		{
			struct CustomColorGroup *g;
			struct CustomColor **c;

			for (g=getColorGroups(); g->colors; ++g)
				for (c=g->colors; *c; ++c)
				{
					std::ostringstream o;

					o << (*c)->fcolor;

					std::string s=o.str();

					if (!xmlSetProp(f,(xmlChar *)(*c)
							->shortname,
							(xmlChar *)s.c_str()))
						outofmemory();
				}
		}

		{
			size_t i;

			for (i=1; i<Tags::tags.names.size(); i++)
			{
				f=xmlNewNode(NULL, (xmlChar *)"TAG");

				std::string n=mail::rfc2047::encode(Tags::tags.names
							       [i], myCharset);

				if (!f || !xmlSetProp(f, (xmlChar *)"NAME",
						      (xmlChar *)n.c_str())
				    || !xmlAddChild(REMOTE->children, f))
				{
					if (f)
						xmlFreeNode(f);
					outofmemory();
				}
			}
		}
	}

	std::list<AddressBook *>::iterator
		ab=AddressBook::addressBookList.begin(),
		ae=AddressBook::addressBookList.end();

	while (ab != ae)
	{
		AddressBook *p= *ab++;

		urls.insert(p->getURL());

		xmlNodePtr f=xmlNewNode(NULL,
					(xmlChar *)"ADDRESSBOOK");

		std::string nameString=mail::rfc2047::encode(p->getName(),
							myCharset);
		std::string urlString=mail::rfc2047::encode(p->getURL(),
						       myCharset);
		std::string folderString=mail::rfc2047::encode(p->getFolder(),
							  myCharset);

		if (!f || !xmlSetProp(f, (xmlChar *)"NAME",
				      (xmlChar *)nameString.c_str())
		    || !xmlSetProp(f, (xmlChar *)"URL",
				   (xmlChar *)urlString.c_str())
		    || !xmlSetProp(f, (xmlChar *)"FOLDER",
				   (xmlChar *)folderString.c_str())
		    || !xmlAddChild(REMOTE->children, f))
		{
			if (f)
				xmlFreeNode(f);
			outofmemory();
		}
	}

	Macros *mp=Macros::getRuntimeMacros();
	xmlNodePtr macroRootNode=NULL;

	if (!mp->macroList.empty() || !mp->filterList.empty())
	{
		macros=xmlNewDoc((xmlChar *)"1.0");
		macroRootNode=xmlNewDocNode(macros, NULL,
					    (xmlChar *)"CONE", NULL);
		if (!macroRootNode)
			outofmemory();
		macros->children=macroRootNode;
	}

	if (mp)
	{
		for (std::map<Macros::name, std::string>::iterator
			     mb=mp->macroList.begin(),
			     me=mp->macroList.end(); mb != me; ++mb)
		{
			xmlNodePtr n=xmlNewNode(NULL, (xmlChar *)"MACRO");
			if (!n)
				outofmemory();

			if (!xmlAddChild(macroRootNode, n))
			{
				xmlFreeNode(n);
				outofmemory();
			}

			if (mb->first.f)
			{
				std::ostringstream o;

				o << (mb->first.f-1);

				std::string s=o.str();

				

				if (!xmlSetProp(n,
						(xmlChar *)"FK",
						(xmlChar *)

						(xmlChar *)
						((std::string)mail::rfc2047
						 ::encode
						 (s, myCharset)).c_str()))
					outofmemory();
			}
			else
			{
				std::string s(mail::iconvert::convert
					      (mb->first.n, "utf-8"));

				s=mail::rfc2047::encode(s, "utf-8");

				if (!xmlSetProp(n,
						(xmlChar *)"NAME",
						(xmlChar *)s.c_str()))
					outofmemory();
			}
			if (!xmlNewTextChild(n, NULL,
					     (xmlChar *)"TEXT",
					     (xmlChar *)mb->second.c_str()))
				outofmemory();
		}

		for (std::map<int, std::pair<std::string, std::string>
			      >::iterator
			fb=mp->filterList.begin(), fe=mp->filterList.end();
		     fb != fe; ++fb)
		{
			xmlNodePtr n=xmlNewNode(NULL, (xmlChar *)"FILTER");
			if (!n)
				outofmemory();

			if (!xmlAddChild(macroRootNode, n))
			{
				xmlFreeNode(n);
				outofmemory();
			}

			{
				std::ostringstream o;

				o << fb->first;

				std::string s=o.str();

				if (!xmlSetProp(n, (xmlChar *)"FK",
						(xmlChar *)s.c_str())
				    ||
				    !xmlSetProp(n, (xmlChar *)"NAME",
						(xmlChar *)fb->second.first
						.c_str()))
					outofmemory();
			}

			if (!xmlNewTextChild(n, NULL,
					     (xmlChar *)"COMMAND",
					     (xmlChar *)fb->second.second
					     .c_str()))
				outofmemory();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// Get property, convert it to app's charset.

static std::string getProp(xmlNodePtr node, const char *prop)
{
	std::string s="";

	char *val=(char *)xmlGetProp(node, (const xmlChar *)prop);

	if (val)
		try {
			s=val;
			free(val);
		} catch (...) {
			free(val);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

	return mail::rfc2047::decoder().
		decode(s, unicode_default_chset());
}

/////////////////////////////////////////////////////////////////////////////
//
// Get property, convert it to UTF-8

static std::string getPropUTF8(xmlNodePtr node, const char *prop)
{
	std::string s="";

	char *val=(char *)xmlGetProp(node, (const xmlChar *)prop);

	if (val)
		try {
			s=val;
			free(val);
		} catch (...) {
			free(val);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

	return mail::rfc2047::decoder().decode(s, "utf-8");
}


// 8-bit octet-based properties are also 2047-encoded, but the character set
// info is irrelevant.  Get the raw encoded content.

static std::string getPropNC(xmlNodePtr node, const char *prop)
{
	std::string s="";

	char *val=(char *)xmlGetProp(node, (const xmlChar *)prop);

	if (val)
		try {
			s=val;
			free(val);
		} catch (...) {
			free(val);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

	return mail::rfc2047::decoder::decodeSimple(s);
}

static std::string getTextNode(xmlNodePtr node)
{
	std::string s="";

	char *val=(char *)xmlNodeGetContent(node);

	if (val)
		try {
			s=val;
			free(val);
		} catch (...) {
			free(val);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

	return mail::rfc2047::decoder()
		.decode(s, unicode_default_chset());
}
		
bool myServer::loadconfig()
{
	std::string configFile=getConfigFilename();

	xmlDocPtr doc;

	myServer::smtpServerURL="";
	myServer::smtpServerCertificate="";
	myServer::useIMAPforSending=false;
	nntpCommandFolder::nntpCommand="";

	myServer::remoteConfigURL="";
	myServer::remoteConfigPassword="";
	myServer::remoteConfigFolder="";
	Curses::suspendCommand="";
	myServer::customHeaders="";
	CursesEditMessage::externalEditor="";
	GlobalKeys::quitNoPrompt=false;
	CursesMoronize::enabled=false;

	initColorGroups();
	if (myServer::remoteConfigAccount)
	{
		myServer::remoteConfigAccount->logout();
		delete myServer::remoteConfigAccount;
		myServer::remoteConfigAccount=NULL;
	}

	if (access(configFile.c_str(), R_OK))
		doc=NULL; // Suppress warning in xmlParseFile
	else
		doc=xmlParseFile(configFile.c_str());

	if (!doc) // No config file, first time start cone.
	{
		struct passwd *pw=getpwuid(getuid());

		myAddresses.push_back(std::string(pw ? pw->pw_name:"nobody") + "@"
				      + mail::hostname());
		return false;
	}

	std::string cacheFilename=getCacheIndexFilename();
	std::string macroFilename=getMacroFilename();

	xmlDocPtr cachedoc;
	xmlDocPtr remoteDoc=NULL;

	if (access(cacheFilename.c_str(), R_OK))
		cachedoc=NULL;
	else
		cachedoc=xmlParseFile(cacheFilename.c_str());

	Macros *mp=Macros::getRuntimeMacros();

	std::set<std::string> cache_filenames;
	std::set<std::string> aux_filenames;
	std::map<std::string, xmlNodePtr> cacheMap;

	std::map<std::string, xmlNodePtr> remoteMap;

	std::set<std::string> accountNamesLoaded;

	xmlNodePtr root=xmlDocGetRootElement(doc);

	if (root && strcasecmp( (const char *)root->name, "CONE") == 0)
	{
		myServer::remoteConfigURL=getPropNC(root, "REMOTECONFIG");
		myServer::remoteConfigFolder=getPropNC(root, "REMOTEFOLDER");

		// The following is really an if() stmt.  It's a while()
		// because it's a convenient mechanism to retry upon an
		// error.

		while (myServer::remoteConfigURL.size() > 0)
		{
			if (myServer::remoteConfigAccount) // Retry after err
			{
				myServer::remoteConfigAccount->logout();
				delete myServer::remoteConfigAccount;
				myServer::remoteConfigAccount=NULL;
			}

			myServer::remoteConfigAccount =
				new myServer::remoteConfig();

			if (!myServer::remoteConfigAccount)
				outofmemory();

			std::string tmpConfigFile=configFile + ".tmp";
			std::string tmpMacroFile=macroFilename + ".tmp";

			unlink(tmpConfigFile.c_str());
			unlink(tmpMacroFile.c_str());

			std::string errmsg=remoteConfigAccount
				->loadconfig(tmpConfigFile, tmpMacroFile);

			if (errmsg.size() > 0)
			{
				ClearKeyHandler kh;

				statusBar->clearstatus();
				statusBar->status(Gettext(_("The following error occured "
							    "when trying to read the "
							    "configuration from a remote "
							    "folder:\n\n%1%\n\n"
							    "You may choose to try again, "
							    "or turn off remote configuration "
							    "altogether (and lose all remotely-saved configuration)."))
						  << errmsg);

				bool wasKeepGoing=Curses::keepgoing;

				myServer::eventloop();
				Curses::keepgoing=wasKeepGoing;
			}

			if (errmsg.size() > 0)
			{
				myServer::promptInfo promptInfo=
					myServer::prompt(myServer::promptInfo(_("Try again (otherwise cancel)? (Y/N) "))
							 .yesno());

				if (promptInfo.abortflag ||
				    (std::string)promptInfo == "Y")
					continue;

				myServer::remoteConfigAccount->logout();
				delete myServer::remoteConfigAccount;
				myServer::remoteConfigAccount=NULL;
				myServer::remoteConfigURL="";
				myServer::remoteConfigPassword="";
				myServer::remoteConfigFolder="";
				continue;
			}

			remoteDoc=xmlParseFile(tmpConfigFile.c_str());

			if (remoteDoc)
			{
				// Create a map of all remotely-saved
				// server configurations.

				// As we parse the main configuration file,
				// when we get to a remote config stub,
				// use the map to look it the remotely-saved
				// server config.

				xmlNodePtr root=
					xmlDocGetRootElement(remoteDoc);

				if (root && strcasecmp( (const char *)
							root->name,
							"CONE") == 0)
				{
					for (root=root->children;
					     root; root=root->next)
					{
						if (strcasecmp( (const char *)
								root->name,
								"SERVER"))
							continue;

						std::string name=getProp(root,
								    "NAME");
						remoteMap
							.insert(make_pair(name,
									  root)
								);
					}
				}
				else
				{
					xmlFreeDoc(remoteDoc); // Vat?
					remoteDoc=NULL;
				}
			}

			if (access(tmpMacroFile.c_str(), R_OK) == 0)
			{
				xmlDocPtr macroDoc=
					xmlParseFile(tmpMacroFile.c_str());
				if (macroDoc)
				{
					config::loadmacros(mp, macroDoc);
					xmlFreeDoc(macroDoc);
				}
			}
			break;
		}

		if (cachedoc)
		{
			xmlNodePtr cacheroot=xmlDocGetRootElement(cachedoc);

			if (cacheroot &&
			    strcasecmp( (const char *)cacheroot->name,
					"CONECACHE") == 0)
			{
				for (cacheroot=cacheroot->children;
				     cacheroot;
				     cacheroot=cacheroot->next)
				{
					if (strcasecmp( (const char *)
							cacheroot->name,
							"SERVER"))
						continue;

					std::string name=getProp(cacheroot, "NAME");

					if (name.size() == 0)
						continue;

					cacheMap.insert(make_pair(name,
								  cacheroot)
							);
				}
			}
		}
	}

	if (mp && myServer::remoteConfigURL.size() == 0)
	{
		if (access(macroFilename.c_str(), R_OK))
		{
			mp->macroList.clear();
			mp->filterList.clear();
		}
		else
		{
			xmlDocPtr macroDoc=xmlParseFile(macroFilename.c_str());

			if (macroDoc)
			{
				config::loadmacros(mp, macroDoc);
				xmlFreeDoc(macroDoc);
			}
		}
	}


	bool flag=config::loadconfig(doc, cache_filenames,
				     aux_filenames, cacheMap,
				     remoteMap, false,
				     accountNamesLoaded);

	if (flag && remoteDoc)
	{
		// Any unprocessed configs are manually inserted

		std::map<std::string, xmlNodePtr>::iterator p;

		if (remoteMap.size() > 0)
		{
			ClearKeyHandler kh;

			statusBar->clearstatus();
			statusBar->status(Gettext(_("Found remotely-saved configuration for %1% additional mail accounts.\n\n"
						    "Will try to restore these account(s) configuration."))
					  << remoteMap.size());

			bool wasKeepGoing=Curses::keepgoing;

			myServer::eventloop();
			Curses::keepgoing=wasKeepGoing;
		}

		for (p=remoteMap.begin(); p != remoteMap.end(); ++p)
			config::loadserver(p->second, cache_filenames,
					   aux_filenames, cacheMap,
					   accountNamesLoaded);

		remoteMap.clear();

		// Read other remotely-saved configuration (addresses, etc...)

		flag=config::loadconfig(remoteDoc, cache_filenames,
					aux_filenames, cacheMap,
					remoteMap, true,
					accountNamesLoaded);

		PasswordList::passwordList.save(myServer::remoteConfigURL,
						myServer::remoteConfigPassword);
	}

	if (doc)
		xmlFreeDoc(doc);
	if (cachedoc)
		xmlFreeDoc(cachedoc);
	if (remoteDoc)
		xmlFreeDoc(remoteDoc);

	// Garbage cleanup

	std::string d=getConfigDir();

	DIR *dirp=opendir(d.c_str());
	struct dirent *de;

	while (dirp && (de=readdir(dirp)) != NULL)
	{
		const char *suffix=strrchr(de->d_name, '.');

		if (!suffix)
			continue;

		if (strcmp(suffix, ".tmp") == 0
		    // We've locked the application, we're the only
		    // user, and we just started up, so blow away any
		    // leftover crap.
		    || (strcmp(suffix, ".cache") == 0 &&
			cache_filenames.count(std::string(de->d_name)) == 0)
		    || (strcmp(suffix, ".newsrc") == 0 &&
			aux_filenames.count(std::string(de->d_name))==0))
		{
			std::string f=d + "/" + de->d_name;
			unlink(f.c_str());
		}

		if (strcmp(suffix, ".maildir") == 0 &&
		    aux_filenames.count(std::string(de->d_name)) == 0)
		{
			std::string f=d + "/" + de->d_name;

			mail::maildir::maildirdestroy(f);
		}
	}

	if (dirp)
		closedir(dirp);

	return flag;
}

// Process a config file.  Maybe its local, mebbe its remote, don't matter.

bool myServer::config::loadconfig(xmlDocPtr doc,
				  std::set<std::string> &cache_filenames,
				  std::set<std::string> &aux_filenames,
				  std::map<std::string, xmlNodePtr> &cacheMap,
				  std::map<std::string, xmlNodePtr> &remoteMap,
				  bool skipServerConfig,
				  std::set<std::string> &accountNamesLoaded)
{
	xmlNodePtr root=xmlDocGetRootElement(doc);
	size_t tagNum=1;

	if (root && strcasecmp( (const char *)root->name, "CONE") == 0)
	{
		for (root=root->children; root; root=root->next)
		{
			if (strcasecmp( (const char *)root->name, "ADDRESS")
			    == 0)
			{
				myAddresses.push_back(getTextNode(root));
				continue;
			}

			if (strcasecmp( (const char *)root->name,
					"LISTADDRESS") == 0)
			{
				myListAddresses.push_back(getTextNode(root));
				continue;
			}

			if (strcasecmp( (const char *)root->name,
					"ADDRESSBOOK") == 0)
			{
				std::string name=getProp(root, "NAME");
				std::string url=getPropNC(root, "URL");
				std::string path=getPropNC(root, "FOLDER");

				AddressBook *abook=new AddressBook();

				if (!abook)
					outofmemory();

				try {
					abook->init(name, url, path);

					AddressBook::addressBookList
						.insert(AddressBook::
							addressBookList.end(),
							abook);
				} catch (...) {
					delete abook;
					LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
				}
				continue;
			}

			if (strcasecmp( (const char *)root->name,
					"SPECIALFOLDER") == 0)
			{
				std::string id=getProp(root, "ID");
				std::string server=getPropNC(root, "SERVER");
				std::string path=getPropNC(root, "PATH");
				std::string name=getPropNC(root, "NAME");
				std::string archivedMonth=getPropNC(root,
							       "ARCHIVED");

				if (SpecialFolder::folders.count(id) > 0)
				{
					SpecialFolder &sf=
						SpecialFolder::folders
						.find(id)->second;

					SpecialFolder::Info newInfo;

					newInfo.serverUrl=server;
					newInfo.serverPath=path;
					newInfo.lastArchivedMonth=
						archivedMonth;
					if (name.size() == 0)
						name=sf.defaultNameUTF8;

					newInfo.nameUTF8=name;

					sf.folderList.push_back(newInfo);
				}
				continue;
			}

			if (strcasecmp( (const char *)root->name, "NNTP") == 0)
			{
				nntpCommandFolder::nntpCommand=
					getPropNC(root, "COMMAND");
				continue;
			}

			if (strcasecmp( (const char *)root->name, "SMTP") == 0)
			{
				myServer::smtpServerURL=getProp(root, "URL");

				myServer::smtpServerCertificate=
					getProp(root, "CERTIFICATE");

				std::string useImap=getProp(root, "USEIMAP");

				myServer::useIMAPforSending=
					strcasecmp(useImap.c_str(), "TRUE")
					== 0;
				continue;
			}

			if (strcasecmp( (const char *)root->name,
					"DICTIONARY") == 0)
			{
				std::string setting=getProp(root, "NAME");

				if (setting.size() > 0)
					spellCheckerBase->
						setDictionary(setting);
				continue;
			}

			if (strcasecmp( (const char *)root->name,
					"COLORS") == 0)
			{
				struct CustomColorGroup *g;
				struct CustomColor **c;

				for (g=getColorGroups(); g->colors; ++g)
					for (c=g->colors; *c; ++c)
					{
						std::string s=getProp(root,
								 (*c)->
								 shortname);

						if (s.size() == 0)
							continue;

						std::istringstream i(s);

						i >> (*c)->fcolor;
					}
			}

			if (strcasecmp( (const char *)root->name,
					"OPTIONS") == 0)
			{
				std::string demoron=getProp(root,
						       "DEMORONIZATION");

				myServer::setDemoronizationType(demoron
								== "MIN"
								? myServer
								::DEMORON_MIN:
								demoron
								== "MAX"
								? myServer
								::DEMORON_MAX

								: myServer
								::DEMORON_OFF);

				std::string postandmail=getProp(root,
							   "POSTANDMAIL");

				myServer::postAndMail=
					postandmail == "Y" ?
					myServer::POSTANDMAIL_YES:
					postandmail == "N" ?
					myServer::POSTANDMAIL_NO:
					myServer::POSTANDMAIL_ASK;

				Curses::suspendCommand=
					getProp(root, "SUSPEND");
				CursesEditMessage::externalEditor=
					getProp(root, "EDITOR");

				if (getProp(root, "QUITNOPROMPT").size() > 0)
					GlobalKeys::quitNoPrompt=true;
				if (getProp(root, "SMARTSHORTCUTS").size()>0)
					CursesMoronize::enabled=true;

				myServer::customHeaders=
					getProp(root, "CUSTOMHEADERS");

				GPG::gpg.extraEncryptSignOptions=
					getProp(root, "GPGENCRYPTSIGNOPTIONS");

				GPG::gpg.extraDecryptVerifyOptions=
					getProp(root, "GPGDECRYPTVERIFYOPTIONS"
						);

				std::string t=getProp(root, "AUTOSAVE");

				std::istringstream i(t);

				i >> CursesEditMessage::autosaveInterval;

				if (CursesEditMessage::autosaveInterval < 60)
					CursesEditMessage::autosaveInterval=60;

				if (CursesEditMessage::autosaveInterval >60*60)
					CursesEditMessage::autosaveInterval
						=60 * 60;


				t=getProp(root, "WATCHDAYS");

				if (t.size() > 0)
				{
					std::istringstream ii(t);

					ii >> Watch::defaultWatchDays;
					if (Watch::defaultWatchDays <= 0)
						Watch::defaultWatchDays=1;
					if (Watch::defaultWatchDays > 99)
						Watch::defaultWatchDays=99;
				}

				t=getProp(root, "WATCHDEPTH");

				if (t.size() > 0)
				{
					std::istringstream ii(t);

					ii >> Watch::defaultWatchDepth;
					if (Watch::defaultWatchDepth <= 0)
						Watch::defaultWatchDepth=1;
					if (Watch::defaultWatchDepth > 99)
						Watch::defaultWatchDepth=99;
				}

				continue;
			}

			if (strcasecmp( (const char *)root->name,
					"TAG") == 0)
			{
				std::string n=getProp(root, "NAME");

				if (tagNum < Tags::tags.names.size())
					Tags::tags.names[tagNum++]=n;
			}

			if (strcasecmp( (const char *)root->name,
					"REMOTESERVER") == 0)
			{
				// Remotely-saved server config, see if we
				// have it.

				std::string name=getProp(root, "NAME");

				std::map<std::string, xmlNodePtr>::iterator
					p=remoteMap.find(name);

				if (p != remoteMap.end())
				{
					loadserver(p->second,
						   cache_filenames,
						   aux_filenames,
						   cacheMap,
						   accountNamesLoaded);

					remoteMap.erase(p);
				}
				else
				{
					ClearKeyHandler kh;

					statusBar->clearstatus();
					statusBar->status(Gettext(_("Unable to find remotely-saved configuration for %1%.\n\n"
								    "All configuration for this account has been lost."))
							  << name);

					bool wasKeepGoing=Curses::keepgoing;

					myServer::eventloop();
					Curses::keepgoing=wasKeepGoing;
				}
				continue;
			}

			if (strcasecmp( (const char *)root->name, "SERVER"))
				continue;


			if (skipServerConfig)
				continue;
			// Re-parsing the remote config file, only picking up
			// non-server configs.

			loadserver(root, cache_filenames, aux_filenames,
				   cacheMap, accountNamesLoaded);

		}
		return true;
	}
	return false;
}

//
// Process server configuration.

void myServer::config::loadserver(xmlNodePtr root,
				  std::set<std::string> &cache_filenames,
				  std::set<std::string> &aux_filenames,
				  std::map<std::string, xmlNodePtr> &cacheMap,
				  std::set<std::string> &accountNamesLoaded)
{
	std::string name=getProp(root, "NAME");
	std::string url=getPropNC(root, "URL");
	std::string certificate=getPropNC(root, "CERTIFICATE");

	if (name.length() == 0 || url.length() == 0)
		return;

	if (accountNamesLoaded.count(name) > 0)
	{
		size_t cnt=0;

		std::string newName;

		do
		{
			std::ostringstream o;

			o << name << std::setw(3) << std::setfill('0') << ++cnt;

			newName=o.str();
		} while (accountNamesLoaded.count(newName) > 0);


		ClearKeyHandler kh;

		statusBar->clearstatus();
		statusBar->status(Gettext(_("There are duplicate configuration for account named %1%.\n\n"
					    "The duplicate account will be renamed, "
					    "all cached information for the duplicate configuration will be discarded."))
				  << name);

		bool wasKeepGoing=Curses::keepgoing;

		myServer::eventloop();
		Curses::keepgoing=wasKeepGoing;

		name=newName;
		cacheMap.erase(name);
	}

	accountNamesLoaded.insert(name);


	myServer *s=new myServer(name, url);

	s->certificate=certificate;

	std::map<std::string, xmlNodePtr>::iterator cacheP=cacheMap.find(name);

	std::map<std::string, xmlNodePtr> folderCacheMap;

	if (cacheP != cacheMap.end())
	{
		xmlNodePtr folderCache= cacheP->second;

		for (folderCache=folderCache->children;
		     folderCache; folderCache=folderCache->next)
		{
			if (strcasecmp( (const char *)folderCache->name,
					"FOLDER"))
				continue;

			std::string name=getPropNC(folderCache, "PATH");

			if (name.size() == 0)
				continue;

			folderCacheMap.insert(make_pair(name, folderCache));
		}
	}

	std::string newsrc=getPropNC(root, "NEWSRC");
	std::string auxdescr=_("NetNews configuration file");

	if (newsrc.size() == 0)
	{
		newsrc=getPropNC(root, "MAILDIR");
		auxdescr=_("mail folder directory");
	}

	if (newsrc.size() > 0)
	{
		if (aux_filenames.count(newsrc) > 0)
		{
			size_t cnt=0;

			std::string newName;

			do
			{
				std::ostringstream o;

				o << std::setw(3) << std::setfill('0') << ++cnt;

				newName=o.str() + newsrc;
			} while (accountNamesLoaded.count(newName) > 0);

			newsrc=newName;

			ClearKeyHandler kh;

			statusBar->clearstatus();
			statusBar->status(Gettext(_("The specified %2% for %1% is a duplicate.\n\n"
						    "The duplicate %2% will be removed.  All saved "
						    "information will be lost."))
					  << name << auxdescr);

			bool wasKeepGoing=Curses::keepgoing;

			myServer::eventloop();
			Curses::keepgoing=wasKeepGoing;

		}

		s->newsrc=newsrc;
		aux_filenames.insert(newsrc);
	}

	xmlNodePtr n;

	for (n=root->children; n; n=n->next)
	{
		if (strcasecmp( (const char *)n->name, "NAMESPACE") == 0)
		{
			std::string path=getPropNC(n, "PATH");
			s->topLevelFolders.add(path);
		}

		if (strcasecmp( (const char *)n->name, "SETTING") == 0)
		{
			std::string name=getPropNC(n, "NAME");
			std::string value=getPropNC(n, "VALUE");

			s->server_configuration.insert(make_pair(name, value));
			continue;
		}
		if (strcasecmp( (const char *)n->name, "FOLDER"))
			continue;

		xmlAttrPtr attr=n->properties;

		std::map<std::string, std::string> folderprops;

		while (attr)
		{
			const char *a=(const char *)attr->name;

			std::string aDecode=
				mail::rfc2047::decoder()
				.decode(a, unicode_default_chset());

			std::string val;

			if (strcmp(a, "PATH") == 0 ||
			    strcmp(a, "INDEX") == 0 ||
			    strcmp(a, "SNAPSHOT") == 0)
				val=getPropNC(n, a);
			else
				val=getProp(n, a);

			folderprops.insert(make_pair(aDecode, val));

			attr=attr->next;
		}

		xmlNodePtr tn;

		for (tn=n->children; tn; tn=tn->next)
			if (strcasecmp((const char *)tn->name, "FILTER") == 0)
			{
				std::string s=getTextNode(tn);

				if (s.size() == 0)
					continue;

				folderprops.insert(make_pair(std::string( (const
								      char *)
								     tn->name),
							     s));
			}

		std::map<std::string, std::string>
			::iterator p=folderprops.find("PATH");

		if (p == folderprops.end())
			continue;

		std::string path=p->second;

		folderprops.erase(p);

		std::map<std::string, xmlNodePtr>::iterator cp=folderCacheMap.find(path);


		if (cp != folderCacheMap.end())
		{
			std::string n="INDEX";

			std::string s=getPropNC(cp->second, n.c_str());

			if (s.size() > 0)
				folderprops.insert(make_pair(n, s));

			n="SNAPSHOT";
			s=getPropNC(cp->second, n.c_str());
			if (s.size() > 0)
				folderprops.insert(make_pair(n, s));
		}

		p=folderprops.find("INDEX");
		if (p != folderprops.end())
		{
			struct stat stat_buf;

			if (cache_filenames.count(p->second) > 0
			    // DUPE - erase
			    || stat((getConfigDir() + "/" + p->second).c_str(),
				    &stat_buf) < 0) // Not found
			{
				folderprops.erase(p);
			}
			else
				cache_filenames.insert(p->second);
		}

		p=folderprops.find("SNAPSHOT");
		if (p != folderprops.end())
		{
			struct stat stat_buf;

			if (cache_filenames.count(p->second) > 0
			    // DUPE - erase
			    || stat((getConfigDir() + "/" + p->second).c_str(),
				    &stat_buf) < 0) // Not found
				folderprops.erase(p);
			else
				cache_filenames.insert(p->second);
		}

		s->folder_configuration
			.insert(make_pair(path, folderprops));

	}
}

void myServer::config::loadmacros(Macros *mp, xmlDocPtr docPtr)
{
	xmlNodePtr root=xmlDocGetRootElement(docPtr);

	if (!root || strcasecmp((const char *)root->name, "CONE"))
		return;

	for (root=root->children; root; root=root->next)
	{
		if (strcasecmp( (const char *)root->name, "MACRO") == 0)
		{
			std::string macroName=getProp(root, "NAME");
			std::string fkeyNum=getProp(root, "FK");

			Macros::name mn(0);

			if (fkeyNum.size() > 0)
			{
				int fkn=0;

				std::istringstream i(fkeyNum);

				i >> fkn;

				mn=Macros::name(fkn);
			}
			else
			{
				if (macroName.size() == 0)
					continue;

				std::vector<unicode_char> v;

				if (!mail::iconvert::convert(macroName,
							     "utf-8",
							     v))
					continue;

				mn=Macros::name(v);
			}

			xmlNodePtr c;

			for (c=root->children; c; c=c->next)
			{
				if (strcasecmp((const char *)c->name, "TEXT")
				    == 0)
				{
					std::string text_str;

					char *cp=(char *)xmlNodeGetContent(c);

					if (!cp)
						continue;

					try {
						text_str=cp;
						free(cp);
					} catch (...) {
						free(cp);
						LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
					}

					mp->macroList
						.insert(make_pair(mn,
								  text_str));
					break;
				}
			}
		}

		if (strcasecmp( (const char *)root->name, "FILTER") == 0)
		{
			std::string filterName=getProp(root, "NAME");
			std::string fkeyNum=getProp(root, "FK");
			std::string text_str;

			int fkn=0;

			{
				std::istringstream i(fkeyNum);

				i >> fkn;
			}

			xmlNodePtr c;

			for (c=root->children; c; c=c->next)
			{
				if (strcasecmp((const char *)c->name, "COMMAND")
				    == 0)
				{
					char *cp=(char *)xmlNodeGetContent(c);

					if (!cp)
						continue;

					try {
						text_str=cp;
						free(cp);
					} catch (...) {
						free(cp);
						LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
					}
					break;
				}
			}

			mp->filterList[fkn]=make_pair(filterName, text_str);
		}
	}
}
//
// Return this folder's INDEX or a SNAPSHOT filename
//

std::string myServer::getCachedFilename(myFolder *mf, const char *name)
{
	struct stat stat_buf;

	std::string filename=getFolderConfiguration(mf->getFolder(), name);
	std::string n="";

	if (filename.size() > 0)
	{
		n=getConfigDir() + "/" + filename;

		if (stat(n.c_str(), &stat_buf) < 0)
			filename="";
	}

	if (filename.size() == 0)
	{
		unsigned counter;
		pid_t p=getpid();
		time_t t;

		counter=0;
		time(&t);

		do
		{
			{
				std::ostringstream o;

				o << t << "." << p << "_" << counter++
				  << "." << mail::hostname()
				  << ".cache";
				filename=o.str();
			}

			n=getConfigDir() + "/" + filename;

		} while (stat(n.c_str(), &stat_buf) == 0);

		updateFolderConfiguration(mf->getFolder(), name, filename);
		saveconfig(false, false);
	}

	return n;
}

//
// Save folder's index
//

void myServer::saveFolderIndex(myFolder *mf)
{
	std::string n=getCachedFilename(mf, "INDEX");
	std::string t=n + ".tmp";

	xmlDocPtr doc=xmlNewDoc((xmlChar *)"1.0");

	std::string myCharset=unicode_default_chset();

	try {
		xmlNodePtr d=xmlNewDocNode(doc, NULL,
					   (xmlChar *)"INDEX", NULL);
		if (!d)
			outofmemory();

		doc->children=d;

		if (!xmlSetProp(d, (xmlChar *)"FORMAT",
				(xmlChar *)INDEXFORMAT))
			outofmemory();

		myFolder::iterator b=mf->begin(),
			e=mf->begin() + mf->sorted_index.size();

		while (b != e)
		{
			myFolder::Index &i = *b++;

			xmlNodePtr messageNode=xmlNewNode (NULL,
							   (xmlChar *)
							   "MESSAGE");

			if (!messageNode || !xmlAddChild(d, messageNode))
			{
				if (messageNode)
					xmlFreeNode(messageNode);
				outofmemory();
			}

			char arrivalDateStr[200];
			char messageDateStr[200];

			rfc822_mkdate_buf(i.arrivalDate, arrivalDateStr);
			rfc822_mkdate_buf(i.messageDate, messageDateStr);

			std::string sizebuf;

			{
				std::ostringstream n;

				n << i.messageSize;
				sizebuf=n.str();
			}

			std::string uid=mail::rfc2047::encode(i.uid, myCharset);

#if 0
			fprintf(stderr, "SUBJECT: %s, NAME: %s\n",
				i.subject_utf8.c_str(),
				i.name_utf8.c_str());
			fflush(stderr);
#endif
			std::string subject=mail::rfc2047::encode(i.subject_utf8,
								  "utf-8");
			std::string name=mail::rfc2047::encode(i.name_utf8,
							       "utf-8");

			std::string messageid=mail::rfc2047::encode((std::string)
								    i.messageid,
								    "utf-8");

			std::string references;


			{
				std::vector<mail::address> vec;
				std::vector<messageId>::iterator
					rb=i.references.begin(),
					re=i.references.end();

				vec.reserve(i.references.size());
				while (rb != re)
				{
					vec.push_back(mail::address("",
								    (std::string)
								    *rb));
					++rb;
				}

				references=mail::rfc2047
					::encode(mail::address
						 ::toString("", vec),
						 myCharset);
			}
			if (!xmlSetProp(messageNode,
					(xmlChar *)"UID",
					(xmlChar *)uid.c_str()) ||
			    (i.arrivalDate &&
			     !xmlSetProp(messageNode,
					 (xmlChar *)"ARRIVAL",
					 (xmlChar *)arrivalDateStr)) ||
			    (i.messageDate &&
			     !xmlSetProp(messageNode,
					 (xmlChar *)"SENT",
					 (xmlChar *)messageDateStr)) ||

			    !xmlSetProp(messageNode,
					(xmlChar *)"SIZE",
					(xmlChar *)sizebuf.c_str()) ||

			    !xmlSetProp(messageNode,
					(xmlChar *)"MESSAGEID",
					(xmlChar *)messageid.c_str()) ||

			    !xmlSetProp(messageNode,
					(xmlChar *)"REFERENCES",
					(xmlChar *)references.c_str()) ||

			    !xmlNewTextChild(messageNode, NULL,
					     (xmlChar *)"SUBJECT",
					     (xmlChar *)subject.c_str()
					     ) ||

			    !xmlNewTextChild(messageNode, NULL,
					     (xmlChar *)"NAME",
					     (xmlChar *)name.c_str()
					     ))
				outofmemory();

#if 0
			fprintf(stderr, "done\n"); fflush(stderr);
#endif
		}

		std::map<messageId, WatchInfo>::iterator
			wb=mf->watchList.watchList.begin(),
			we=mf->watchList.watchList.end();

		while (wb != we)
		{
			std::string messageid=wb->first;
			time_t expires=wb->second.expires;
			size_t depth=wb->second.depth;

			messageid=mail::rfc2047::encode(messageid, myCharset);

			std::ostringstream o1;

			o1 << expires;

			std::ostringstream o2;

			o2 << depth;

			xmlNodePtr watchNode=xmlNewNode(NULL,
							(xmlChar *)
							"WATCH");

			if (!watchNode || !xmlAddChild(d, watchNode))
			{
				if (watchNode)
					xmlFreeNode(watchNode);
				outofmemory();
			}


			if (!xmlSetProp(watchNode,
					(xmlChar *)"MESSAGEID",
					(xmlChar *)messageid.c_str()) ||

			    !xmlSetProp(watchNode,
					(xmlChar *)"EXPIRES",
					(xmlChar *)o1.str().c_str()) ||
			    !xmlSetProp(watchNode,
					(xmlChar *)"DEPTH",
					(xmlChar *)o2.str().c_str()))
				outofmemory();

			++wb;
		}

		if (!xmlSaveFile(t.c_str(), doc) ||
		    rename(t.c_str(), n.c_str()))
			saveError(t);

		xmlFreeDoc(doc);
	} catch (...) {
		xmlFreeDoc(doc);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

// Load folder's index.

bool myServer::loadFolderIndex(std::string filename,
			       std::map<std::string, size_t> &uid_list,
			       myFolder *folder)
{
	xmlDocPtr doc;

	if (access(filename.c_str(), R_OK) < 0 ||
	    (doc=xmlParseFile(filename.c_str())) == NULL)
		return true;

	bool flag=false;

	time_t now=time(NULL);

	try {
		xmlNodePtr rootNode=xmlDocGetRootElement(doc);

		if (rootNode && strcasecmp( (const char *)rootNode->name,
					    "INDEX") == 0 &&
		    getPropNC(rootNode, "FORMAT") == INDEXFORMAT)
		{
			// Load all the WATCH info first.

			xmlNodePtr root;

			for (root=rootNode->children; root; root=root->next)
			{
				if (strcasecmp( (const char *)root->name,
						"WATCH"))
					continue;

				std::string messageid=
					getPropNC(root, "MESSAGEID");

				std::string expires_s=
					getPropNC(root, "EXPIRES");

				std::string depth_s=
					getPropNC(root, "DEPTH");


				time_t expires=now;

				{
					std::istringstream i(expires_s);

					i >> expires;
				}

				size_t depth=1;
				{
					std::istringstream i(depth_s);

					i >> depth;
				}

				if (expires <= now)
					continue;


				folder->watchList(messageId(folder->
							    msgIds,
							    messageid),
						  expires, depth);
			}

			// Now the index info.

			for (root=rootNode->children; root; root=root->next)
			{
				if (strcasecmp( (const char *)root->name,
						"MESSAGE"))
					continue;

				std::string uid=getPropNC(root, "UID");

				if (uid_list.count(uid) == 0)
				{
					flag=true;
					continue;
				}

				std::string arrival=getProp(root, "ARRIVAL");

				std::string sent=getProp(root, "SENT");

				std::string size=getProp(root, "SIZE");

				std::string messageid=getPropUTF8(root,
								  "MESSAGEID");

				std::string references=getPropNC(root,
							    "REFERENCES");

				std::string subject="";
				std::string name="";

				std::map<std::string, size_t>
					::iterator ptr=uid_list.find(uid);

				myFolder::Index &i=folder->
					serverIndex[ptr->second];

				uid_list.erase(ptr);


				xmlNodePtr child;

				for (child=root->children; child;
				     child=child->next)
				{
					if (strcasecmp((const char *)
						       child->name,
						       "SUBJECT") == 0)
					{
						xmlChar *cp=
							xmlNodeGetContent
							(child);

						if (!cp)
							continue;

						try {
							subject=(const char *)
								cp;

							free(cp);
						} catch (...) {
							free(cp);
							LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
						}
					}

					if (strcasecmp((const char *)
						       child->name,
						       "NAME") == 0)
					{
						xmlChar *cp=
							xmlNodeGetContent
							(child);

						if (!cp)
							continue;

						try {
							name=(const char *)cp;
							free(cp);
						} catch (...) {
							free(cp);
							LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
						}
					}
				}

				i.arrivalDate=rfc822_parsedt(arrival.c_str());
				i.messageDate=rfc822_parsedt(sent.c_str());
				i.messageid=messageId(folder->msgIds,
						      messageid);

				std::vector<mail::address> refs;
				size_t dummy;

				mail::address::fromString(references, refs,
							  dummy);

				i.references.clear();
				i.references.reserve(refs.size());
				std::vector<mail::address>::iterator
					rb=refs.begin(),
					re=refs.end();

				while (rb != re)
				{
					i.references
						.push_back(messageId(folder->
								     msgIds,
								     rb->
								     getAddr())
							   );
					++rb;
				}

				i.messageSize=0;
				{
					std::istringstream s(size.c_str());

					s >> i.messageSize;
				}
				i.subject_utf8=mail::rfc2047::decoder()
					.decode(subject, "utf-8");
				i.name_utf8=mail::rfc2047::decoder()
					.decode(name, "utf-8");

				i.toupper();
				i.checkwatch(*folder);
			}
		}
		xmlFreeDoc(doc);
	} catch (...) {
		xmlFreeDoc(doc);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	return flag;
}

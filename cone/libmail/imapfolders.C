/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "addmessage.H"
#include "imap.H"
#include "misc.H"
#include "imapacl.H"
#include "imapfolder.H"
#include "imapfolders.H"
#include "imaplisthandler.H"

#include "smapacl.H"
#include "smapcreate.H"
#include "smapdelete.H"
#include "smapaddmessage.H"
#include "smapsendfolder.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sstream>
#include "unicode/unicode.h"
#include "rfc822/rfc822.h"
#include "maildir/maildiraclt.h"

using namespace std;

//
// Top level folders -- this stuff is taken from the IMAP NAMESPACE messages
// at login.

void mail::imap::readTopLevelFolders(mail::callback::folderList &callback1,
				     mail::callback &callback2)
{
	vector<const mail::folder *> folders;
	vector<mail::imapFolder>::iterator b, e;

	b=namespaces.begin();
	e=namespaces.end();

	while (b != e)
	{
		folders.push_back(&*b);
		b++;
	}

	callback1.success(folders);
	callback2.success("OK");
}

mail::folder *mail::imap::folderFromString(string s)
{
	string words[4];

	words[0]="";
	words[1]="";
	words[2]="";
	words[3]="";
	int i=0;

	string::iterator b=s.begin(), e=s.end();

	while (b != e)
	{
		if (*b == ':')
		{
			b++;
			++i;
			continue;
		}

		if (*b == '\\')
		{
			b++;
			if (b == e)
				break;
		}

		if (i < 4)
			words[i].append(&*b, 1);
		b++;
	}

	mail::imapFolder *f=new mail::imapFolder(*this,
						 words[0],
						 words[1],
						 words[2],
						 -1);

	if (f == NULL)
		return NULL;

	f->hasMessages(words[3].find('S') != std::string::npos);
	f->hasSubFolders(words[3].find('C') != std::string::npos);

	return f;
}

mail::imapFolder::imapFolder(mail::imap &myImap, string pathArg,
				   string hierArg, string nameArg,
				   int typeArg)
	: mail::folder(&myImap),
	  imapAccount(myImap), path(pathArg), hiersep(hierArg), name(nameArg),
	  hasmessages(true),
	  hassubfolders(false), type(typeArg)
{
}

mail::imapFolder::imapFolder(const mail::imapFolder &c)
	: mail::folder(&c.imapAccount), imapAccount(c.imapAccount)
{
	(*this)=c;
}

mail::imapFolder &mail::imapFolder::operator=(const mail::imapFolder &c)
{
	mail::folder::operator=(c);
	path=c.path;
	hiersep=c.hiersep;
	name=c.name;

	hasmessages=c.hasmessages;
	hassubfolders=c.hassubfolders;
	type=c.type;
	return *this;
}

mail::imapFolder::~imapFolder()
{
}

void mail::imapFolder::hasMessages(bool flag)
{
	hasmessages=flag;
}

void mail::imapFolder::hasSubFolders(bool flag)
{
	hassubfolders=flag;
}

string mail::imapFolder::getName() const
{
	return name;
}

string mail::imapFolder::getPath() const
{
	return path;
}

bool mail::imapFolder::isParentOf(string cpath) const
{
	if (isDestroyed())
		return false;

	string pfix=path + hiersep;

	if (imapAccount.smap)
		pfix += "/"; // SMAP implies this

	return (cpath.size() > pfix.size() &&
		pfix == cpath.substr(0, pfix.size()));
}

bool mail::imapFolder::hasMessages() const
{
	return hasmessages;
}

bool mail::imapFolder::hasSubFolders() const
{
	return hassubfolders;
}

void mail::imapFolder::getParentFolder(callback::folderList &callback1,
				       callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	string sep=hiersep;

	if (imapAccount.smap)
		sep="/"; // SMAP implied

	if (sep.size() > 0)
	{
		size_t n=path.rfind(sep[0]);

		if (n != std::string::npos)
		{
			mail::account &a=imapAccount;

			a.findFolder(path.substr(0, n),
				     callback1, callback2);
			return;
		}
	}

	mail::imapFolder topFolder( *this );

	topFolder.path="";
	topFolder.hiersep="";
	topFolder.hasmessages=false;
	topFolder.hassubfolders=true;
	topFolder.name="";

	vector<const mail::folder *> array;

	array.push_back(&topFolder);

	callback1.success(array);
	callback2.success("OK");
}

void mail::imapFolder::readSubFolders(mail::callback::folderList &callback1,
				      mail::callback &callback2)
	const
{
	if (isDestroyed(callback2))
		return;

	imapAccount.readSubFolders(path + hiersep, callback1, callback2);
}

mail::addMessage *mail::imapFolder::addMessage(mail::callback &callback) const
{
	if (isDestroyed(callback))
		return NULL;

	return imapAccount.addMessage(path, callback);
}

void mail::imapFolder::readFolderInfo(mail::callback::folderInfo &callback1,
				      mail::callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	if (imapAccount.currentFolder &&
	    imapAccount.currentFolder->path == path)
	{
		imapFOLDERinfo *f=imapAccount.currentFolder;

		vector<imapFOLDERinfo::indexInfo>::iterator b, e;

		b=f->index.begin();
		e=f->index.end();

		callback1.messageCount=f->index.size();
		callback1.unreadCount=0;

		while (b != e)
		{
			if (b->unread)
				++callback1.unreadCount;
			b++;
		}
		callback1.success();
		callback2.success("OK");
		return;
	}

	if (!imapAccount.smap && callback1.fastInfo &&
	    !imapAccount.hasCapability(LIBMAIL_CHEAPSTATUS))
	{
		callback2.success("OK");
		return;
	}

	imapAccount.folderStatus(path, callback1, callback2);
}

//////////////////////////////////////////////////////////////////////////////
//
// CREATE a subfolder of an existing folder.  First, issue a LIST to
// determine the eventual hierarchy separator character, followed by a CREATE.
//
// imapCREATE is also commandeered to rename a folder, since the processing
// is rather similar.

LIBMAIL_START

class imapCREATE : public imapCommandHandler {

	string renamePath; // If not empty, this is a rename
	string parentPath;
	string name;
	string encodedname;
	bool hasMessages;
	bool hasSubfolders;
	mail::callback::folderList &callback1;
	mail::callback &callback2;

	bool listHierSent;
	vector <imapFolder> hiersep; // Results of LIST for the hier sep

public:
	imapCREATE(string parentPathArg, // path + hiersep
		   string nameArg,
		   bool hasMessagesArg,
		   bool hasSubfoldersArg,
		   mail::callback::folderList &callback1Arg,
		   mail::callback &callback2Arg);

	imapCREATE(string renamePathArg,
		   string parentPathArg, // path + hiersep
		   string nameArg,
		   bool hasMessagesArg,
		   bool hasSubfoldersArg,
		   mail::callback::folderList &callback1Arg,
		   mail::callback &callback2Arg);

	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *errmsg);

	bool untaggedMessage(imap &imapAccount, string msgname);

	bool taggedMessage(imap &imapAccount, string msgname,
			   string message,
			   bool okfail, string errmsg);
};

LIBMAIL_END

mail::imapCREATE::imapCREATE(string parentPathArg,
			     string nameArg,
			     bool hasMessagesArg,
			     bool hasSubfoldersArg,
			     mail::callback::folderList &callback1Arg,
			     mail::callback &callback2Arg)
	: renamePath(""),
	  parentPath(parentPathArg),
	  name(nameArg),
	  hasMessages(hasMessagesArg),
	  hasSubfolders(hasSubfoldersArg),
	  callback1(callback1Arg),
	  callback2(callback2Arg),
	  listHierSent(false)
{
}

mail::imapCREATE::imapCREATE(string renamePathArg,
			     string parentPathArg,
			     string nameArg,
		   bool hasMessagesArg,
		   bool hasSubfoldersArg,
			     mail::callback::folderList &callback1Arg,
			     mail::callback &callback2Arg)
	: renamePath(renamePathArg),
	  parentPath(parentPathArg),
	  name(nameArg),
	  hasMessages(hasMessagesArg),
	  hasSubfolders(hasSubfoldersArg),
	  callback1(callback1Arg),
	  callback2(callback2Arg),
	  listHierSent(false)
{
}

void mail::imapCREATE::installed(mail::imap &imapAccount)
{
	// First things first, get the hier separator.

	imapAccount.imapcmd("CREATE", "LIST "
		     + imapAccount.quoteSimple(parentPath + "tree")
		     + " \"\"\r\n");
}

const char *mail::imapCREATE::getName()
{
	return "CREATE";
}

void mail::imapCREATE::timedOut(const char *errmsg)
{
	callbackTimedOut(callback2, errmsg);
}

bool mail::imapCREATE::untaggedMessage(mail::imap &imapAccount, string msgname)
{
	if (msgname == "LIST") // Untagged LIST replies
	{
		hiersep.clear();
		imapAccount.installBackgroundTask( new mail::imapLIST(hiersep,
								      0, true));
		return true;
	}
	return false;
}

bool mail::imapCREATE::taggedMessage(mail::imap &imapAccount, string msgname,
				     string message,
				     bool okfail, string errmsg)
{
	if (msgname != "CREATE")
		return false;

	if (!okfail)
	{
		callback2.fail(errmsg);
		imapAccount.uninstallHandler(this);
		return true;
	}

	if (!listHierSent)
	{
		if (hiersep.size() == 0)
		{
			callback2.fail("IMAP server failed to process the LIST command.");
			imapAccount.uninstallHandler(this);
			return true;
		}


		listHierSent=true;
		encodedname=name;

		// The folder name should be utf7-encoded

		{
			char *p=libmail_u_convert_toutf8(encodedname.c_str(),
							 unicode_default_chset(),
							 NULL);

			if (!p && strchr(p, *hiersep[0].getHierSep().c_str())
			    != NULL)
			{
				if (p)
					free(p);
				callback2.fail("Folder name contains an invalid character.");
				imapAccount.uninstallHandler(this);
				return true;
			}
			free(p);


			p=libmail_u_convert_tobuf(encodedname.c_str(),
						  unicode_default_chset(),
						  unicode_x_imap_modutf7,
						  NULL);

			if (p)
			{
				try {
					encodedname=p;
					free(p);
				} catch (...) {
					free(p);
					LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
				}
			}
		}


		imapAccount.imapcmd("CREATE",
				    (renamePath.size() > 0
				     ? string("RENAME ") +
				     imapAccount.quoteSimple(renamePath) + " ":
				     string("CREATE ")) +
				    imapAccount
				    .quoteSimple(parentPath +
						 encodedname +
						 (!hasMessages ?
						  hiersep[0].getHierSep()
						  :""))
				    + "\r\n");
		return true;

	}

	mail::imapFolder new_folder(imapAccount, parentPath + encodedname,
				    hiersep[0].getHierSep(),
				    name, -1);

	new_folder.hasMessages(hasMessages);
	new_folder.hasSubFolders(hasSubfolders);

	vector<const mail::folder *> dummy_list;

	dummy_list.push_back(&new_folder);

	callback1.success(dummy_list);
	callback2.success(errmsg);
	imapAccount.uninstallHandler(this);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// Creating a previously known folder, from scratch.  This is much easier.

LIBMAIL_START

class imapRECREATE : public imapCommandHandler {

	string path;
	mail::callback &callback;

public:
	imapRECREATE(string pathArg, mail::callback &callbackArg);
	~imapRECREATE();

	void installed(imap &imapAccount);

	const char *getName();
	void timedOut(const char *errmsg);

	bool untaggedMessage(imap &imapAccount, string msgname);
	
	bool taggedMessage(imap &imapAccount, string msgname,
			   string message,
			   bool okfail, string errmsg);
};

LIBMAIL_END

mail::imapRECREATE::imapRECREATE(string pathArg,
				       mail::callback &callbackArg)
	: path(pathArg), callback(callbackArg)
{
}

mail::imapRECREATE::~imapRECREATE()
{
}

void mail::imapRECREATE::installed(mail::imap &imapAccount)
{
	imapAccount.imapcmd("CREATE", "CREATE "
		     + imapAccount.quoteSimple(path) + "\r\n");
}

const char *mail::imapRECREATE::getName()
{
	return "CREATE";
}

void mail::imapRECREATE::timedOut(const char *errmsg)
{
	callbackTimedOut(callback, errmsg);
}

bool mail::imapRECREATE::untaggedMessage(mail::imap &imapAccount, string msgname)
{
	return false;
}
	
bool mail::imapRECREATE::taggedMessage(mail::imap &imapAccount, string msgname,
				       string message,
				       bool okfail, string errmsg)
{
	if (msgname == "CREATE")
	{
		if (okfail)
			callback.success(errmsg);
		else
			callback.fail(errmsg);
		imapAccount.uninstallHandler(this);
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////
//
// DELETE a folder.

LIBMAIL_START

class imapDELETE : public imapCommandHandler {
	mail::callback &callback;
	string path;

public:
	imapDELETE(mail::callback &callbackArg,
			 string pathArg);

	void installed(imap &imapAccount);

private:
	const char *getName();
	void timedOut(const char *errmsg);
	bool untaggedMessage(imap &imapAccount, string name);

	bool taggedMessage(imap &imapAccount, string name,
			   string message,
			   bool okfail, string errmsg);
};

LIBMAIL_END

mail::imapDELETE::imapDELETE(mail::callback &callbackArg,
				   string pathArg)
	: callback(callbackArg), path(pathArg)
{
}

void mail::imapDELETE::installed(mail::imap &imapAccount)
{
	imapAccount.imapcmd("DELETE", "DELETE " + imapAccount.quoteSimple(path)
		     + "\r\n");
}

const char *mail::imapDELETE::getName()
{
	return "DELETE";
}

void mail::imapDELETE::timedOut(const char *errmsg)
{
	callbackTimedOut(callback, errmsg);
}

bool mail::imapDELETE::untaggedMessage(mail::imap &imapAccount, string name)
{
	return false;
}

bool mail::imapDELETE::taggedMessage(mail::imap &imapAccount, string name,
				     string message,
				     bool okfail, string errmsg)
{
	if (name == "DELETE")
	{
		if (okfail)
			callback.success(errmsg);
		else
			callback.fail(errmsg);
		imapAccount.uninstallHandler(this);
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////

void mail::imapFolder::createSubFolder(string name, bool isDirectory,
				       mail::callback::folderList &callback1,
				       mail::callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	if (!imapAccount.ready(callback2))
		return;

	if (imapAccount.smap)
	{
		mail::smapCREATE *h=
			new mail::smapCREATE(path, name, isDirectory,
					     callback1, callback2);

		if (!h)
		{
			callback2.fail(strerror(errno));
			return;
		}

		try {
			imapAccount.installForegroundTask(h);
		} catch (...) {
			delete h;
			throw;
		}

		return;
	}

	mail::imapCREATE *h=new mail::imapCREATE(path + hiersep,
						 name,
						 !isDirectory,
						 isDirectory,
						 callback1,
						 callback2);
	if (!h)
	{
		callback2.fail(strerror(errno));
		return;
	}

	try {
		imapAccount.installForegroundTask(h);
	} catch (...) {
		delete h;
		throw;
	}
}

void mail::imapFolder::renameFolder(const folder *newParent,
				    std::string newName,
				    mail::callback::folderList &callback1,
				    callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	if (!imapAccount.ready(callback2))
		return;

	imapAccount.sameServerFolderPtr=NULL;

	string newPath="";

	if (newParent)
	{
		newParent->sameServerAsHelperFunc();

		if (imapAccount.sameServerFolderPtr == NULL)
		{
			callback2.fail("mail::imapFolder::renameFolder -"
				       " invalid destination folder object.");
			return;
		}

		newPath=imapAccount.sameServerFolderPtr->path +
			imapAccount.sameServerFolderPtr->hiersep;
	}

	imapHandler *h=imapAccount.smap ?
		(imapHandler *)
		new mail::smapCREATE(path, newPath, newName,
				     callback1, callback2) :
		new mail::imapCREATE(path, newPath, newName,
				     hasmessages,
				     hassubfolders,
				     callback1,
				     callback2);

	if (!h)
	{
		callback2.fail(strerror(errno));
		return;
	}

	try {
		imapAccount.installForegroundTask(h);
	} catch (...) {
		delete h;
		throw;
	}
}

void mail::imapFolder::create(bool isDirectory, mail::callback &callback2)
	const
{
	if (isDestroyed(callback2))
		return;
	if (!imapAccount.ready(callback2))
		return;

	if (imapAccount.smap)
	{
		mail::smapCREATE *h=
			new mail::smapCREATE(path, isDirectory, callback2);

		if (!h)
		{
			callback2.fail(strerror(errno));
			return;
		}

		try {
			imapAccount.installForegroundTask(h);
		} catch (...) {
			delete h;
			throw;
		}
		return;
	}

	mail::imapRECREATE *h=new mail::imapRECREATE(path + (isDirectory ?
							     hiersep:""),
						     callback2);
	if (!h)
	{
		callback2.fail(strerror(errno));
		return;
	}
	try {
		imapAccount.installForegroundTask(h);
	} catch (...) {
		delete h;
		throw;
	}
}

void mail::imapFolder::destroy(mail::callback &callback, bool subdirs) const
{
	if (isDestroyed(callback))
		return;

	if (!imapAccount.ready(callback))
		return;

	imapHandler *h=imapAccount.smap ? (imapHandler *)
		new mail::smapDELETE(path, subdirs, callback)
		: new mail::imapDELETE(callback,
				       path + (subdirs ? hiersep:""));

	if (!h)
	{
		callback.fail(strerror(errno));
		return;
	}
	try {
		imapAccount.installForegroundTask(h);
	} catch (...) {
		delete h;
		throw;
	}
}

void mail::imapFolder::open(mail::callback &openCallback,
			    mail::snapshot *restoreSnapshot,
			    mail::callback::folder &folderCallback) const
{
	if (isDestroyed(openCallback))
		return;
	imapAccount.openFolder(path, restoreSnapshot,
			       openCallback, folderCallback);
}


mail::folder *mail::imapFolder::clone() const
{
	mail::imapFolder *i=new mail::imapFolder(*this);

	if (i) return i;
	return NULL;
}

string mail::imapFolder::toString() const
{
	string s="";

	int i;

	for (i=0; i<3; i++)
	{
		if (i > 0)
			s.append(":",1);

		string ss;

		string::iterator b, e;

		switch (i) {
		case 0:
			ss=path;
			break;
		case 1:
			ss=hiersep;
			break;
		default:
			ss=name;
			break;
		}

		b=ss.begin();
		e=ss.end();

		while (b != e)
		{
			if (*b == '\\' || *b == ':')
				s.append("\\", 1);
			s.append(&*b, 1);
			b++;
		}
	}

	s += ":";
	if (hasMessages())
		s += "S";
	if (hasSubFolders())
		s += "C";

	return s;
}

void mail::imapFolder::getMyRights(callback &getCallback, string &rights)
	const
{
	if (isDestroyed(getCallback))
		return;

	if (!imapAccount.ready(getCallback))
		return;

	if (!imapAccount.hasCapability("ACL"))
	{
		mail::folder::getMyRights(getCallback, rights);
		return;
	}

	string s=getPath();

	if (s.find('\r') != std::string::npos ||
	    s.find('\n') != std::string::npos) // Sanity check
	{
		getCallback.fail(strerror(EINVAL));
		return;
	}


	if (imapAccount.smap)
	{
		if (imapAccount.getCapability("ACL") == "ACL")
		{
			// SMAP uses ACL2

			mail::folder::getMyRights(getCallback, rights);
			return;
		}

		smapACL *sa=new smapACL(s, rights, getCallback);

		if (!sa)
		{
			getCallback.fail(strerror(errno));
			return;
		}

		try
		{
			imapAccount.installForegroundTask(sa);
		} catch (...)
		{
			delete sa;
			throw;
		}
		return;
	}

	imapGetMyRights *gm=new imapGetMyRights(s, rights,
						getCallback);

	if (!gm)
	{
		getCallback.fail(strerror(errno));
		return;
	}

	try
	{
		imapAccount.installForegroundTask(gm);
	} catch (...) {
		delete gm;
		throw;
	}
}

void mail::imapFolder::getRights(callback &getCallback,
				 list<pair<string, string> > &rights) const
{
	if (isDestroyed(getCallback))
		return;

	if (!imapAccount.ready(getCallback))
		return;

	if (!imapAccount.hasCapability("ACL"))
	{
		mail::folder::getRights(getCallback, rights);
		return;
	}

	string s=getPath();

	if (s.find('\r') != std::string::npos ||
	    s.find('\n') != std::string::npos) // Sanity check
	{
		getCallback.fail(strerror(EINVAL));
		return;
	}

	if (imapAccount.smap)
	{
		if (imapAccount.getCapability("ACL") == "ACL")
		{
			// SMAP uses ACL2

			mail::folder::getRights(getCallback, rights);
			return;
		}

		smapGETACL *sa=new smapGETACL(s, rights, getCallback);

		if (!sa)
		{
			getCallback.fail(strerror(errno));
			return;
		}

		try
		{
			imapAccount.installForegroundTask(sa);
		} catch (...)
		{
			delete sa;
			throw;
		}
		return;
	}

	imapGetRights *gr=new imapGetRights(s, rights,
					    getCallback);

	if (!gr)
	{
		getCallback.fail(strerror(errno));
		return;
	}

	try
	{
		imapAccount.installForegroundTask(gr);
	} catch (...) {
		delete gr;
		throw;
	}
}

void mail::imapFolder::setRights(callback &setCallback,
				 string &errorIdentifier,
				 vector<string> &errorRights,
				 string identifier,
				 string rights) const
{
	errorIdentifier="";
	errorRights.clear();
	if (isDestroyed(setCallback))
		return;

	if (!imapAccount.ready(setCallback))
		return;

	if (!imapAccount.hasCapability("ACL"))
	{
		mail::folder::setRights(setCallback,
					errorIdentifier,
					errorRights,
					identifier, rights);
		return;
	}

	string s=getPath();

	if (s.find('\r') != std::string::npos ||
	    s.find('\n') != std::string::npos ||
	    identifier.find('\r') != std::string::npos ||
	    identifier.find('\n') != std::string::npos ||
	    rights.find('\r') != std::string::npos ||
	    rights.find('\n') != std::string::npos) // Sanity check
	{
		setCallback.fail(strerror(EINVAL));
		return;
	}

	if (imapAccount.smap)
	{
		if (imapAccount.getCapability("ACL") == "ACL")
		{
			// SMAP uses ACL2

			mail::folder::setRights(setCallback,
						errorIdentifier,
						errorRights,
						identifier, rights);
			return;
		}

		smapSETACL *sa=new smapSETACL(s, identifier, rights,
					      false,
					      errorIdentifier,
					      errorRights,
					      setCallback);

		if (!sa)
		{
			setCallback.fail(strerror(errno));
			return;
		}

		try
		{
			imapAccount.installForegroundTask(sa);
		} catch (...)
		{
			delete sa;
			throw;
		}
		return;
	}




	/* Fix ACL/ACL2 brokenness */

	if (rights.find(ACL_DELETEMSGS[0]) != std::string::npos &&
	    rights.find(ACL_DELETEFOLDER[0]) != std::string::npos &&
	    rights.find(ACL_EXPUNGE[0]) != std::string::npos)
	{
		size_t p=rights.find(ACL_DELETEMSGS[0]);

		rights.erase(rights.begin()+p, rights.begin()+p+1);
		p=rights.find(ACL_DELETEFOLDER[0]);
		rights.erase(rights.begin()+p, rights.begin()+p+1);
		p=rights.find(ACL_EXPUNGE[0]);
		rights.erase(rights.begin()+p, rights.begin()+p+1);

		rights += ACL_DELETE_SPECIAL;
	}

	imapSetRights *sr=new imapSetRights(s, identifier, rights,
					    false,
					    errorIdentifier,
					    errorRights,
					    setCallback);

	if (!sr)
	{
		setCallback.fail(strerror(errno));
		return;
	}

	try
	{
		imapAccount.installForegroundTask(sr);
	} catch (...) {
		delete sr;
		throw;
	}
}

void mail::imapFolder::delRights(callback &setCallback,
				 string &errorIdentifier,
				 vector<std::string> &errorRights,
				 string identifier) const
{
	errorIdentifier="";
	errorRights.clear();
	if (isDestroyed(setCallback))
		return;

	if (!imapAccount.ready(setCallback))
		return;

	if (!imapAccount.hasCapability("ACL"))
	{
		mail::folder::delRights(setCallback,
					errorIdentifier,
					errorRights,
					identifier);
		return;
	}

	string s=getPath();

	if (s.find('\r') != std::string::npos ||
	    s.find('\n') != std::string::npos) // Sanity check
	{
		setCallback.fail(strerror(EINVAL));
		return;
	}

	if (imapAccount.smap)
	{
		if (imapAccount.getCapability("ACL") == "ACL")
		{
			// SMAP uses ACL2

			mail::folder::delRights(setCallback,
						errorIdentifier,
						errorRights,
						identifier);
			return;
		}

		smapSETACL *sa=new smapSETACL(s, identifier, "",
					      true,
					      errorIdentifier,
					      errorRights,
					      setCallback);

		if (!sa)
		{
			setCallback.fail(strerror(errno));
			return;
		}

		try
		{
			imapAccount.installForegroundTask(sa);
		} catch (...)
		{
			delete sa;
			throw;
		}
		return;
	}
	imapSetRights *sr=new imapSetRights(s, identifier, "",
					    true,
					    errorIdentifier,
					    errorRights,
					    setCallback);

	if (!sr)
	{
		setCallback.fail(strerror(errno));
		return;
	}

	try
	{
		imapAccount.installForegroundTask(sr);
	} catch (...) {
		delete sr;
		throw;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// Adding a new message.

LIBMAIL_START

class imapAPPEND : public imapCommandHandler,
			 public addMessage {

	imap &imapAccount;
	mail::callback &callback;
	string path;

	bool continuationReceived;

	// Helper class that pushes the temp file's contents to the server.

	class Pusher : public fd::WriteBuffer {

	public:

		imapAPPEND *myappend;

		Pusher();
		~Pusher();

		bool fillWriteBuffer();
	};

	Pusher *myPusher;
public:
	friend class Pusher;

	FILE *tmpfileptr;
	string appendcmd;
	long bytes;
	long bytesDone;
	long bytesTotal;

	imapAPPEND(imap &imapArg,
			 mail::callback &callbackArg,
			 string pathArg);
	~imapAPPEND();

	void installed(imap &imapAccount);


	void saveMessageContents(string txt);
	void go();
	void fail(string errmsg);

private:
	const char *getName();
	void timedOut(const char *timedout);
	bool untaggedMessage(imap &imapAccount, string msgname);
	bool taggedMessage(imap &imapAccount, string msgname,
			   string message,
			   bool okfail, string errmsg);
	bool continuationRequest(imap &imapAccount, string msg);

	bool fillWriteBuffer();


};

LIBMAIL_END

mail::imapAPPEND::imapAPPEND(mail::imap &imapArg,
				   mail::callback &callbackArg,
				   string pathArg)
	: mail::addMessage(&imapArg),
	  imapAccount(imapArg), callback(callbackArg), path(pathArg),
	  continuationReceived(false), myPusher(NULL), tmpfileptr(NULL)
{
}

mail::imapAPPEND::~imapAPPEND()
{
	if (tmpfileptr)
		fclose(tmpfileptr);
	if (myPusher)
		myPusher->myappend=NULL;
}

const char *mail::imapAPPEND::getName()
{
	return "APPEND";
}

void mail::imapAPPEND::timedOut(const char *timedout)
{
	callbackTimedOut(callback, timedout);
}

// The application pushes the message's contents here.  Save the contents
// into a temporary file, using CRNL line termination.

void mail::imapAPPEND::saveMessageContents(string s)
{
	string::iterator b=s.begin(), e=s.end();

	while (b != e)
	{
		char c= *b++;

		if (c == '\n')
			putc('\r', tmpfileptr);
		putc(c, tmpfileptr);
	}
}


void mail::imapAPPEND::installed(mail::imap &imapAccount)
{
	myPusher=new Pusher();

	if (!myPusher)
		LIBMAIL_THROW("Out of memory.");

	try {
		imapAccount.imapcmd("APPEND", appendcmd);
		imapAccount.socketWrite(myPusher);
		myPusher->myappend=this;
	} catch (...) {
		delete myPusher;
		myPusher=NULL;
	}
}

bool mail::imapAPPEND::untaggedMessage(mail::imap &imapAccount, string msgname)
{
	return false;
}

bool mail::imapAPPEND::continuationRequest(mail::imap &imapAccount, string msg)
{
	continuationReceived=true; // Tell pusher the show's on.
	return true;
}

bool mail::imapAPPEND::taggedMessage(mail::imap &imapAccount, string name,
				     string message,
				     bool okfail, string errmsg)
{
	if (name == "APPEND")
	{
		if (okfail)
			callback.success(errmsg);
		else
			callback.fail(errmsg);
		imapAccount.uninstallHandler(this);
		return true;
	}
	return false;
}

bool mail::imapAPPEND::fillWriteBuffer()
{
	char buffer[BUFSIZ];

	if (!continuationReceived)
		return true;

	if (bytes <= 0)
	{
		callback.reportProgress(bytesTotal, bytesTotal, 0, 1);
		return false;
	}

	int n=fread(buffer, 1, bytes > (long)sizeof(buffer)
		    ? sizeof(buffer):bytes, tmpfileptr);

	if (n <= 0)
	{
		n=bytes > (long)sizeof(buffer) ? sizeof(buffer):bytes;
		memset(buffer, '\n', n);
	}

	bytes -= n;
	bytesDone += n;
	setTimeout();
	callback.reportProgress(bytesDone, bytesTotal, 0, 1);
	myPusher->writebuffer.insert(myPusher->writebuffer.end(),
				     buffer, buffer+n);
	return true;
}

mail::imapAPPEND::Pusher::Pusher() : myappend(NULL)
{
}

mail::imapAPPEND::Pusher::~Pusher()
{
	if (myappend)
		myappend->myPusher=NULL;
}

bool mail::imapAPPEND::Pusher::fillWriteBuffer()
{
	if (!myappend)
		return false;

	return myappend->fillWriteBuffer();
}

void mail::imap::addSmapFolderCmd(mail::smapAddMessage *add,
				  std::string path)
{
	vector<string> words;
	smapHandler::path2words(path, words);

	vector<string>::iterator b, e;

	string pstr="ADD FOLDER ";

	b=words.begin();
	e=words.end();

	while (b != e)
	{
		pstr += " ";
		pstr += quoteSMAP( *b );
		b++;
	}

	pstr += " \"\"\n";

	add->cmds.push_back(pstr);
}

mail::folder *mail::imap::getSendFolder(const smtpInfo &info,
				  const mail::folder *folder,
				  std::string &errmsg)
{
	if (!smap)
		return mail::account::getSendFolder(info, folder, errmsg);

	if (folder)
	{
		sameServerFolderPtr=NULL;
		folder->sameServerAsHelperFunc();
		if (!sameServerFolderPtr)
			return mail::account::getSendFolder(info, folder,
							    errmsg);
	}

	if (info.recipients.size() == 0)
	{
		errno=ENOENT;
		errmsg="Empty recipient list.";
		return NULL;
	}

	mail::folder *f=
		new smapSendFolder(this, info, folder ? folder->getPath():"");

	if (!f)
		errmsg=strerror(errno);
	return f;
}

mail::addMessage *mail::imap::addMessage(string path,
					 mail::callback &callback)
{
	if (smap)
	{
		mail::smapAddMessage *add=new
			smapAddMessage(*this, callback);

		if (!add)
		{
			callback.fail(strerror(errno));
			return NULL;
		}

		try {
			addSmapFolderCmd(add, path);
		} catch (...) {
			delete add;
			add=NULL;
		}
		return add;
	}

	mail::imapAPPEND *append=new mail::imapAPPEND(*this, callback, path);

	if (!append)
	{
		callback.fail(strerror(errno));
		return NULL;
	}

	if ((append->tmpfileptr=tmpfile()) == NULL)
	{
		callback.fail(strerror(errno));
		delete append;
		return NULL;
	}

	time(&append->messageDate);

	return append;
}

void mail::imapAPPEND::fail(string errmsg)
{
	callback.fail(errmsg);
	delete this;
}

void mail::imapAPPEND::go()
{
	if (!checkServer())
		return;

	if (!imapAccount.ready(callback))
	{
		delete this;
		return;
	}

	try {

		string flags=imapAccount.messageFlagStr(messageInfo);

		fprintf(tmpfileptr, "\r\n");

		// Part of the APPEND cmd, actually

		bytesDone=0;

		if (fflush(tmpfileptr) < 0 || ferror(tmpfileptr)
		    || (bytes=ftell(tmpfileptr)) == -1
		    || fseek(tmpfileptr, 0L, SEEK_SET) < 0)
		{
			callback.fail(strerror(errno));
			delete this;
			return;
		}

		bytesTotal=bytes;

		string cnt_s;

		{
			ostringstream o;

			o << bytes-2;
			cnt_s=o.str();
		}

		char buffer[100];

		rfc822_mkdate_buf(messageDate, buffer);

		char *p=strchr(buffer, ',');

		if (p && *++p && *++p)
		{
			char *q=strchr(p, ' ');

			if (q)
				*q='-';

			q=strchr(p, ' ');

			if (q)
				*q='-';
		}
		else
			p=buffer;

		appendcmd="APPEND " + imapAccount.quoteSimple(path)
			+ " (" + flags + ") "
			+ imapAccount.quoteSimple(p)
			+ " {" + cnt_s + "}\r\n";

		imapAccount.installForegroundTask(this);
	} catch (...) {
		callback.fail(strerror(errno));
		delete this;
	}
}

#if 0
bool mail::imapFolder:sameServerAs(const mail::folder *f) const
{
	if (isDestroyed())
		return false;

	imapAccount.sameServerHelperFlag=false;

	f->sameServerAsHelperFunc();

	return imapAccount.sameServerHelperFlag;
}
#endif

void mail::imapFolder::sameServerAsHelperFunc() const
{
	if (isDestroyed())
		return;

	imapAccount.sameServerFolderPtr=this;
}

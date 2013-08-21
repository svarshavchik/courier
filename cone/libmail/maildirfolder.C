/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "maildir/config.h"
#include "maildir/maildirmisc.h"
#include "unicode/unicode.h"
#include "maildirfolder.H"
#include "maildiradd.H"
#include "mbox.H"
#include "misc.H"
#include <list>
#include <algorithm>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>

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

using namespace std;

mail::maildir::folder::folder(mail::maildir *maildirArg,
			      string pathArg)
	: mail::folder(maildirArg),
	  maildirAccount(maildirArg),
	  path(pathArg),
	  hasMessagesFlag(true),
	  hasSubfoldersFlag(true)
{
	name=pathArg;

	size_t p=name.rfind('.');

	if (p != std::string::npos)
		name=name.substr(p+1);

	// Convert the name of the folder from modified UTF-7
	// (Courier compatibility) to the current charset.

	char *s=libmail_u_convert_tobuf(name.c_str(),
					unicode_x_imap_modutf7,
					unicode_default_chset(),
					NULL);

	if (s)
	{
		try {
			name=s;
			free(s);
		} catch (...) {
			free(s);
		}
	}
}

mail::maildir::folder::~folder()
{
}

void mail::maildir::folder::sameServerAsHelperFunc() const
{
}

string mail::maildir::folder::getName() const
{
	if (path == "INBOX")
		return hasSubFolders() ? "Folders":"INBOX";

	return name;
}

string mail::maildir::folder::getPath() const
{
	return path;
}

bool mail::maildir::folder::hasMessages() const
{
	return hasMessagesFlag;
}

bool mail::maildir::folder::hasSubFolders() const
{
	return hasSubfoldersFlag;
}

bool mail::maildir::folder::isParentOf(string otherPath) const
{
	string s=path + ".";

	return (strncmp(otherPath.c_str(), s.c_str(), s.size()) == 0);
}

void mail::maildir::folder::hasMessages(bool flag)
{
	hasMessagesFlag=flag;
}

void mail::maildir::folder::hasSubFolders(bool flag)
{
	hasSubfoldersFlag=flag;
}

class mail::maildir::indexSort {
public:
	indexSort();
	~indexSort();

	bool operator()(const mail::maildir::maildirMessageInfo &a,
			const mail::maildir::maildirMessageInfo &b);
};

mail::maildir::indexSort::indexSort()
{
}

mail::maildir::indexSort::~indexSort()
{
}

// Sort messages in some reasonable order.  Rely on the timestamp component
// of the maildirfilename.

bool mail::maildir::indexSort::operator()
	(const mail::maildir::maildirMessageInfo &a,
	 const mail::maildir::maildirMessageInfo &b)
{
	unsigned long at=atol(a.lastKnownFilename.c_str()),
		bt=atol(b.lastKnownFilename.c_str());

	if ( at != bt)
		return at < bt;

	return strcmp(a.lastKnownFilename.c_str(),
		      b.lastKnownFilename.c_str()) < 0;
}

// Scan a maildirAccount

bool mail::maildir::scan(string folderStr, vector<maildirMessageInfo> &index,
			 bool scanNew)
{
	string p;

	char *d=maildir_name2dir(path.c_str(), folderStr.c_str());

	if (!d)
		return false;

	try {
		p=d;
		free(d);
	} catch (...) {
		free(d);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	static const char * const subdirs[]={"/cur","/new"};

	size_t i;

	for (i=0; i<(scanNew ? 2:1); i++)
	{
		string n=p + subdirs[i];

		DIR *dirp=opendir(n.c_str());

		try {
			struct dirent *de;

			while (dirp && (de=readdir(dirp)) != NULL)
			{
				if (de->d_name[0] == '.')
					continue;


				maildirMessageInfo newInfo;

				newInfo.lastKnownFilename=de->d_name;

				// Use the filename as the uid

				newInfo.uid=de->d_name;

				size_t p=newInfo.uid.find(MDIRSEP[0]);

				if (p != std::string::npos)
					newInfo.uid=newInfo.uid.substr(0, p);

				mail::maildir::updateFlags(de->d_name,
							   newInfo);
				newInfo.recent= i > 0;
				index.push_back(newInfo);
			}

			if (dirp)
				closedir(dirp);
		} catch (...) {
			if (dirp)
				closedir(dirp);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	}

	sort(index.begin(), index.end(), indexSort());
	return true;
}

void mail::maildir::folder::getParentFolder(callback::folderList &callback1,
					    callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	size_t n;

	n=path.rfind('.');

	if (n == std::string::npos)
		n=0;

	maildirAccount->findFolder(path.substr(0, n),
				   callback1,
				   callback2);
}

void mail::maildir::folder::readFolderInfo( mail::callback::folderInfo
					    &callback1,
					    mail::callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	callback1.messageCount=0;
	callback1.unreadCount=0;

	vector<maildirMessageInfo> dummyIndex;

	if (!maildirAccount->scan(path, dummyIndex, true))
	{
		callback1.success();
		callback2.fail("Invalid folder");
		return;
	}

	vector<maildirMessageInfo>::iterator b=dummyIndex.begin(),
		e=dummyIndex.end();

	while (b != e)
	{
		callback1.messageCount++;
		if ( b->unread)
			callback1.unreadCount++;
		b++;
	}

	callback1.success();
	callback2.success("OK");
}

mail::maildir::folder::listinfo::listinfo()
{
}

mail::maildir::folder::listinfo::~listinfo()
{
}

// Callback that lists maildirAccount folders.
// The callback filters only the folders under the list path

void mail::maildir::folder::maildir_list_callback(const char *folder,
						  void *vp)
{
	mail::maildir::folder::listinfo *li=
		(mail::maildir::folder::listinfo *)vp;

	if (strncmp(folder, li->path.c_str(), li->path.size()) ||
	    folder[li->path.size()] != '.')
		return; // Outside the hierarchy being listed.

	folder += li->path.size();
	++folder;

	// If the remaining portion of the name has another period, there's
	// a subdirectory there.  Otherwise, it's a file.
	// It's ok when we get multiple folders in the same subdirectory,
	// subdirs is a STL set, which gets rid of duplicates

	const char *p=strchr(folder, '.');

	if (p)
		li->subdirs.insert(string(folder, p));
	else
		li->list.insert(string(folder));
}

void mail::maildir::folder::readSubFolders( mail::callback::folderList
					    &callback1,
					    mail::callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	if (path.size() == 0)
	{
		maildirAccount->readTopLevelFolders(callback1, callback2);
		return;
	}

	listinfo li;

	li.path=path;

	maildir_list(maildirAccount->path.c_str(),
		     &mail::maildir::folder::maildir_list_callback,
		     &li);

	list<mail::folder *> folderList;
	list<mail::folder *>::iterator b, e;

	try {
		// Create a list of folder objects from the list of folder
		// names in listinfo.  Create a list in two passes.

		// First pass - build names of folders.  If the folder is
		// also found in the subdirectory list, make it a dual-purpose
		// folder/directory.

		buildFolderList(folderList, &li.list, &li.subdirs);

		// Second pass - build remaining subdirs.

		buildFolderList(folderList, NULL, &li.subdirs);

		// Cleanup for the callback
		vector<const mail::folder *> myList;

		b=folderList.begin();
		e=folderList.end();

		while (b != e)
			myList.push_back(*b++);

		callback1.success(myList);
		callback2.success("OK");

	} catch (...) {
		b=folderList.begin();
		e=folderList.end();

		while (b != e)
		{
			delete *b;

			b++;
		}
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	b=folderList.begin();
	e=folderList.end();

	while (b != e)
	{
		delete *b;

		b++;
	}
}

void mail::maildir::folder::buildFolderList(list<mail::folder *> &folderList,
					    set<string> *folders,
					    set<string> *dirs) const
{
	set<string>::iterator b, e;

	if (folders)
	{
		b=folders->begin();
		e=folders->end();
	}
	else
	{
		b=dirs->begin();
		e=dirs->end();
	}

	while (b != e)
	{
		string name= *b++;

		folder *p=new folder(maildirAccount, path + "." + name);

		if (!p)
			LIBMAIL_THROW(strerror(errno));

		try {
			if (folders)
			{
				p->hasMessages(true);
				p->hasSubFolders(false);

				if (dirs->count(name) > 0)
					// Also a subdir
				{
					p->hasSubFolders(true);
					dirs->erase(name);
					// Don't add this folder when we do
					// a directory.
				}
			}
			else
			{
				p->hasMessages(false);
				p->hasSubFolders(true);
			}

			folderList.push_back(p);
		} catch (...) {
			delete p;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	}
}

mail::addMessage *mail::maildir::folder::addMessage(mail::callback
							  &callback) const
{
	if (isDestroyed(callback))
		return NULL;

	string folderPath;

	char *p=maildir_name2dir(maildirAccount->path.c_str(), path.c_str());

	if (!p)
	{
		callback.fail(strerror(errno));
		return NULL;
	}

	try {
		folderPath=p;
		free(p);
	} catch (...) {
		free(p);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	mail::maildir::addmessage *m=new
		mail::maildir::addmessage(maildirAccount, folderPath, callback);

	if (!m)
	{
		callback.fail(strerror(errno));
		return NULL;
	}

	return m;
}

void mail::maildir::moveMessagesTo(const vector<size_t> &messages,
				   mail::folder *copyTo,
				   mail::callback &callback)
{
	sameServerFolderPtr=NULL;
	copyTo->sameServerAsHelperFunc();

	if (sameServerFolderPtr == NULL)
	{
		mail::account::moveMessagesTo(messages, copyTo, callback);
		return;
	}

	string destFolderPath;

	char *p=maildir_name2dir(path.c_str(),
				 sameServerFolderPtr->path.c_str());

	if (!p)
	{
		callback.fail(strerror(errno));
		return;
	}

	try {
		destFolderPath=p;
		free(p);
	} catch (...) {
		free(p);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	vector<size_t>::const_iterator b=messages.begin(), e=messages.end();

	while (b != e)
	{
		size_t n=*b++;

		string messageFn=getfilename(n);

		if (messageFn.size() > 0)
		{
			string destName= destFolderPath
				+ messageFn.substr(messageFn.rfind('/'));

			rename(messageFn.c_str(), destName.c_str());
		}
	}
	updateFolderIndexInfo(&callback, false);
}

void mail::maildir::folder::createSubFolder(string name, bool isDirectory,
					    mail::callback::folderList
					    &callback1,
					    mail::callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	// The name of the folder is translated from the local charset
	// to modified UTF-7 (Courier-IMAP compatibility), with the following
	// blacklisted characters:

	char *p=libmail_u_convert_tobuf(name.c_str(), unicode_default_chset(),
					unicode_x_imap_modutf7 " ./~:", NULL);

	if (!p)
	{
		callback2.fail(strerror(errno));
		return;
	}

	std::string nameutf7;

	errno=ENOMEM;
	try {
		nameutf7=p;
		free(p);
	} catch (...) {
		free(p);
		callback2.fail(strerror(errno));
		return;
	}

	mail::maildir::folder newFolder(maildirAccount, path + "." + nameutf7);

	newFolder.hasMessagesFlag= ! (newFolder.hasSubfoldersFlag=
				      isDirectory);

	if (!newFolder.doCreate(isDirectory))
	{
		callback2.fail(strerror(errno));
		return;
	}

	vector<const mail::folder *> folders;

	folders.push_back(&newFolder);
	callback1.success( folders );
	callback2.success("Mail folder created");
}

bool mail::maildir::folder::doCreate(bool isDirectory) const
{
	if (isDirectory)
		return true; // Pretend

	if (maildirAccount->ispop3maildrop)
	{
		errno=EPERM;
		return false; // POP3 maildrops don't have folders.
	}

	string subdir;

	char *d=maildir_name2dir(maildirAccount->path.c_str(), path.c_str());
	// Checks for name validity.

	if (!d)
		return false;

	try {
		subdir=d;
		free(d);
	} catch (...) {
		free(d);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	return mail::maildir::maildirmake(subdir, true);
}


bool mail::maildir::maildirmake(string subdir, bool isFolder)
{
	string nsubdir=subdir + "/new",
		csubdir=subdir + "/cur",
		tsubdir=subdir + "/tmp";

	if (mkdir(subdir.c_str(), 0700) == 0)
	{
		if (mkdir(nsubdir.c_str(), 0700) == 0)
		{
			if (mkdir(tsubdir.c_str(), 0700) == 0)
			{
				if (mkdir(csubdir.c_str(), 0700) == 0)
				{
					if (!isFolder)
						return true;

					string f=subdir + 
						"/maildirfolder";

					int fd=::open(f.c_str(),
						      O_CREAT |
						      O_RDWR, 0666);

					if (fd >= 0)
					{
						close(fd);
						return true;
					}
					rmdir(csubdir.c_str());
				}
				rmdir(tsubdir.c_str());
			}
			rmdir(nsubdir.c_str());
		}
		rmdir(subdir.c_str());
	}

	return false;
}

void mail::maildir::folder::create(bool isDirectory,
				   mail::callback &callback) const
{
	if (!doCreate(isDirectory))
	{
		callback.fail(strerror(errno));
	}
	else
	{
		callback.success("Mail folder created");
	}
}

void mail::maildir::folder::destroy(mail::callback &callback,
				    bool destroyDir) const
{
	if (isDestroyed(callback))
		return;

	if (!destroyDir) // Folder directories are imaginary, cannot be nuked
	{
		string s;
		char *d=maildir_name2dir(maildirAccount->path.c_str(),
					 path.c_str());
		if (!d)
		{
			callback.fail(strerror(errno));
			return;
		}

		try {
			s=d;
			free(d);
		} catch (...) {
			free(d);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		if (!mail::maildir::maildirdestroy(s))
		{
			callback.fail(strerror(errno));
			return;
		}
	}

	callback.success("Mail folder deleted");
}

void mail::maildir::folder::renameFolder(const mail::folder *newParent,
					 std::string newName,
					 mail::callback::folderList &callback1,
					 mail::callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	if (maildirAccount->folderPath.size() > 0)
	{
		size_t l=path.size();

		if (strncmp(maildirAccount->folderPath.c_str(),
			    path.c_str(), l) == 0 &&
		    ((maildirAccount->folderPath.c_str())[l] == 0 ||
		     (maildirAccount->folderPath.c_str())[l] == '.'))
		{
			callback2.fail("Cannot RENAME currently open folder.");
			return;
		}
	}

	// The name of the folder is translated from the local charset
	// to modified UTF-7 (Courier-IMAP compatibility), with the following
	// blacklisted characters:

	char *s=libmail_u_convert_tobuf(newName.c_str(),
					unicode_default_chset(),
					unicode_x_imap_modutf7 " ./~:", NULL);

	if (!s)
	{
		callback2.fail(strerror(errno));
		return;
	}

	std::string nameutf7;

	errno=ENOMEM;
	try {
		nameutf7=s;
		free(s);
	} catch (...) {
		free(s);
		callback2.fail(strerror(errno));
		return;
	}

	mail::maildir::folder newFolder(maildirAccount,
					(newParent ?
					 newParent->getPath() + ".":
					 string("")) + nameutf7);

	newFolder.hasMessages( hasMessages() );
	newFolder.hasSubFolders( hasSubFolders() );

	vector<const mail::folder *> folders;

	// Paths are INBOX.foo

	string from, to;

	char *p=maildir_name2dir(".", path.c_str());

	if (p)
		try {
			from=p+2; // Skip ./
			free(p);
		} catch (...) {
			free(p);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

	p=maildir_name2dir(".", newFolder.path.c_str());
	if (p)
		try {
			to=p+2; // Skip ./
			free(p);
		} catch (...) {
			free(p);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}


	if (from.size() > 0 &&
	    to.size() > 0 &&
	    maildir_rename(maildirAccount->path.c_str(),
			   from.c_str(), to.c_str(),
			   MAILDIR_RENAME_FOLDER |
			   MAILDIR_RENAME_SUBFOLDERS, NULL))
	{
		callback2.fail(strerror(errno));
	}
	else
	{
		folders.push_back(&newFolder);
		callback1.success( folders );
		callback2.success("Mail folder renamed");
	}
}

bool mail::maildir::maildirdestroy(string d)
{
	list<string> contents;

	DIR *dirp=opendir(d.c_str());

	try {
		struct dirent *de;

		while (dirp && (de=readdir(dirp)) != NULL)
		{
			if (strcmp(de->d_name, ".") == 0)
				continue;
			if (strcmp(de->d_name, "..") == 0)
				continue;

			contents.push_back(de->d_name);
		}

		if (dirp)
			closedir(dirp);
	} catch (...) {
		if (dirp)
			closedir(dirp);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	list<string>::iterator b=contents.begin(), e=contents.end();

	while (b != e)
	{
		string s=d + "/" + *b++;

		if (unlink(s.c_str()) < 0 && errno != ENOENT)
		{
			if (errno == EISDIR)
			{
				if (!maildirdestroy(s))
					return false;
				continue;
			}
			return false;
		}
	}
	rmdir(d.c_str());
	return true;
}

mail::folder *mail::maildir::folder::clone() const
{
	if (isDestroyed())
		return NULL;

	mail::maildir::folder *p=new mail::maildir::folder(maildirAccount,
							   path);

	if (p)
	{
		p->hasMessagesFlag=hasMessagesFlag;
		p->hasSubfoldersFlag=hasSubfoldersFlag;
		return p;
	}
	return NULL;
}


void mail::maildir::findFolder(string folder,
			       mail::callback::folderList &callback1,
			       mail::callback &callback2)
{
	mail::maildir::folder tempFolder(this, folder);

	vector<const mail::folder *> folderList;

	folderList.push_back(&tempFolder);

	callback1.success(folderList);
	callback2.success("OK");
}

string mail::maildir::translatePath(string path)
{
	return mail::mbox::translatePathCommon(path, ":/.~", ".");
}

static string encword(string s)
{
	string r="";

	string::iterator b=s.begin(), e=s.end();
	string::iterator p=b;

	while ( b != e )
	{
		if ( *b == ':' || *b == '\\')
		{
			r.insert(r.end(), p, b);
			r += "\\";
			p=b;
		}
		b++;
	}

	r.insert(r.end(), p, b);
	return r;
}


static string getword(string &s)
{
	string r="";

	string::iterator b=s.begin(), e=s.end(), p=b;

	while (b != e)
	{
		if (*b == ':')
			break;

		if (*b == '\\')
		{
			r.insert(r.end(), p, b);

			b++;
			p=b;

			if (b == e)
				break;
		}
		b++;
	}

	r.insert(r.end(), p, b);

	if (b != e)
	{
		b++;
		s=string(b, s.end());
	}

	return r;
}

string mail::maildir::folder::toString() const
{
	return encword(path) + ":" + encword(name) + ":" +
		(hasMessagesFlag ? "M":"") +
		(hasSubfoldersFlag ? "S":"");
}


mail::folder *mail::maildir::folderFromString(string folderName)
{
	string path=getword(folderName);
	string name=getword(folderName);

	mail::maildir::folder *f=new mail::maildir::folder(this, path);

	if (!f)
		return NULL;

	f->hasMessagesFlag= folderName.find('M') != std::string::npos;
	f->hasSubfoldersFlag= folderName.find('S') != std::string::npos;

	return f;
}

void mail::maildir::folder::open(mail::callback &openCallback,
				 mail::snapshot *restoreSnapshot,
				 mail::callback::folder &folderCallback) const
{
	if (isDestroyed(openCallback))
		return;

	maildirAccount->open(path, openCallback, folderCallback);
}


void mail::maildir::readTopLevelFolders(mail::callback::folderList &callback1,
					mail::callback &callback2)
{
	mail::maildir::folder inbox(this, INBOX);
	mail::maildir::folder folders(this, INBOX);

	inbox.hasSubfoldersFlag=false;
	folders.hasMessagesFlag=false;

	vector<const mail::folder *> folderList;

	folderList.push_back(&inbox);

	if (!ispop3maildrop)
		folderList.push_back(&folders);

	callback1.success(folderList);
	callback2.success("OK");
}

/*
** Copyright 2002-2004, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "mbox.H"
#include "misc.H"
#include "mboxlock.H"
#include "mboxopen.H"
#include "mboxexpunge.H"
#include "mboxmagictag.H"
#include "mboxadd.H"
#include "file.H"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "unicode/unicode.h"

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

#include <vector>

using namespace std;

mail::mbox::folder::folder(string pathArg,
			   mail::mbox &mboxArg)
	: mail::folder(&mboxArg), path(pathArg),
	  name(defaultName(pathArg)), mboxAccount(mboxArg),
	  saveHasMessages(false),
	  saveHasFolders(false)
{
}

mail::mbox::folder::folder(const mail::mbox::folder &f)
	: mail::folder(&f.mboxAccount), path(f.path), name(f.name),
	  mboxAccount(f.mboxAccount),
	  saveHasMessages(false),
	  saveHasFolders(false)
{
}

mail::mbox::folder::~folder()
{
}

string mail::mbox::folder::defaultName(string path)
{
	string::iterator b, e, c;

	b=path.begin();
	e=path.end();
	c=b;

	while (b != e)
		if (*b++ == '/')
			c=b;

	path=string(c, e);

	char *p=libmail_u_convert_tobuf(path.c_str(),
					unicode_x_imap_modutf7,
					unicode_default_chset(),
					NULL);

	if (p)
	{
		try {
			path=p;
			free(p);
		} catch (...) {
			free(p);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
	}

	return path;
}

void mail::mbox::folder::sameServerAsHelperFunc() const
{
}

string mail::mbox::folder::getName() const
{
	return name;
}

string mail::mbox::folder::getPath() const
{
	return path;
}

bool mail::mbox::folder::hasMessages() const
{
	struct stat stat_buf;

	return saveHasMessages || path == "INBOX" ||
		(stat(path.c_str(), &stat_buf) == 0
		 && !S_ISDIR(stat_buf.st_mode));
}

bool mail::mbox::folder::hasSubFolders() const
{
	return saveHasFolders || !hasMessages();
}

bool mail::mbox::folder::isParentOf(string pathArg) const
{
	return pathArg.size() > path.size() &&
		pathArg.substr(0, path.size() + 1 ) == (path + "/");
}

void mail::mbox::folder::hasMessages(bool arg)
{
	saveHasMessages=arg;
}

void mail::mbox::folder::hasSubFolders(bool arg)
{
	saveHasFolders=arg;
}

///////////////////////////////////////////////////////////////////////////
//

class mail::mbox::StatusTask : public mail::mbox::TimedTask {

	string folder;

	mail::callback::folderInfo &info;

	bool count(int fd);

public:
	StatusTask(string folderArg, mail::mbox &mboxAccount,
		   mail::callback &callbackArg,
		   mail::callback::folderInfo &infoArg);
	~StatusTask();

	bool doit();
};

mail::mbox::StatusTask::StatusTask(string folderArg, mail::mbox &mboxAccount,
				   mail::callback &callback,
				   mail::callback::folderInfo &infoArg)
	: TimedTask(mboxAccount, callback), folder(folderArg), info(infoArg)
{
}

mail::mbox::StatusTask::~StatusTask()
{
}

bool mail::mbox::StatusTask::doit()
{
	if (folder == mboxAccount.currentFolder)
	{
		info.messageCount=mboxAccount.index.size();

		size_t i;

		for (i=0; i<info.messageCount; i++)
			if (mboxAccount.index[i].unread)
				info.unreadCount++;
		info.success();
		callback.success("OK");
		done();
		return true;
	}

	if (info.fastInfo)  // mboxes are expensive
	{
		callback.success("OK");
		done();
		return true;
	}

	if (folder == "INBOX")
	{
		int fd=::open(mboxAccount.inboxSpoolPath.c_str(),
			      O_RDWR|O_CREAT, 0600);

		if (fd >= 0)
			close(fd);

		fd=::open(mboxAccount.inboxMailboxPath.c_str(), O_RDWR|O_CREAT, 0600);

		if (fd >= 0)
			close(fd);

		mail::mbox::lock inbox_lock(mboxAccount.inboxSpoolPath);
		mail::mbox::lock mailbox_lock(mboxAccount.inboxMailboxPath);

		if (!inbox_lock(true) || !count(inbox_lock.getfd()))
		{
			if (errno != EAGAIN && errno != EEXIST
			    && errno != ENOENT)
			{
				fail(string("INBOX: ") + strerror(errno));
				return true;
			}
		}

		if (!mailbox_lock(true) || !count(mailbox_lock.getfd()))
		{
			if (errno != EAGAIN && errno != EEXIST
			    && errno != ENOENT)
			{
				fail(string("INBOX: ") + strerror(errno));
				return true;
			}
		}
	}
	else
	{
		mail::mbox::lock mailbox_lock(folder);

		if (!mailbox_lock(true) || !count(mailbox_lock.getfd()))
		{
			if (errno != EAGAIN && errno != EEXIST)
			{
				fail(folder + ": " + strerror(errno));
				return true;
			}

			return false;
		}
	}

	info.success();
	callback.success("OK");
	done();
	return true;
}

bool mail::mbox::StatusTask::count(int fd)
{
	mail::file fp(fd, "r");

	if (!fp)
		return false;

	bool firstLine=true;

	while (!feof(fp))
	{
		string l=fp.getline();

		if (l.size() == 0)
			continue;

		if (strncmp(l.c_str(), "From ", 5) == 0)
		{
			firstLine=false;
			info.messageCount++;
			info.unreadCount++;

			mail::mboxMagicTag tag(fp.getline(),
					       mboxAccount.keywordHashtable);

			if (!tag.good())
				continue;

			mail::messageInfo msginfo=tag.getMessageInfo();

			if (!msginfo.unread)
				--info.unreadCount;
		}

		if (firstLine)
			break; // Not an mboxAccount file.
	}
	return true;
}

void mail::mbox::folder::getParentFolder(callback::folderList &callback1,
					 callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	size_t n;

	if (path == "INBOX" || path == mboxAccount.rootPath ||
	    (n=path.rfind('/')) == std::string::npos)
	{
		mail::mbox::folder dummy("", mboxAccount);

		vector<const mail::folder *> array;

		array.push_back(&dummy);
		callback1.success(array);
		callback2.success("OK");
		return;
	}

	mboxAccount.findFolder(path.substr(0, n), callback1, callback2);
}

void mail::mbox::folder::readFolderInfo( mail::callback::folderInfo
					 &callback1,
					 mail::callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	callback1.messageCount=0;
	callback1.unreadCount=0;

	if (path.size() == 0)
	{
		callback1.success();
		callback2.success("OK");
		return;
	}

	mboxAccount.installTask(new mail::mbox::StatusTask(path, mboxAccount, callback2,
						    callback1));
}

/////////////////////////////////////////////////////////////////////////////

void mail::mbox::folder::readSubFolders( mail::callback::folderList &callback1,
					 mail::callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	if (path.size() == 0)
	{
		mboxAccount.readTopLevelFolders(callback1, callback2);
		return;
	}

	vector<folder *> folderList;

	vector<folder *>::iterator b, e;

	DIR *dirp=opendir(path.c_str());

	try {
		struct dirent *de;

		while (dirp && (de=readdir(dirp)) != NULL)
		{
			char *p;

			if (strcmp(de->d_name, "..") == 0 ||
			    strcmp(de->d_name, ".") == 0 ||
			    ((p=strrchr(de->d_name, '~')) && p[1] == 0))
				continue;

			for (p=de->d_name; *p; p++)
				if (*p == '.')
					if (strcmp(p, ".lock") == 0 ||
					    strncmp(p, ".lock.", 6) == 0)
						break;

			if (*p)
				continue;

			string name=de->d_name;

			string fpath=path + "/" + name;

			folder *f=new folder(fpath, mboxAccount);

			if (!f)
				LIBMAIL_THROW("Out of memory.");

			try {
				folderList.push_back(f);
			} catch (...) {
				delete f;
				LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
			}
		}

		vector<const mail::folder *> folders;

		b=folderList.begin();
		e=folderList.end();

		while (b != e)
			folders.push_back(*b++);

		callback1.success(folders);

		b=folderList.begin();
		e=folderList.end();

		while (b != e)
			delete *b++;

		if (dirp)
			closedir(dirp);
	} catch (...) {
		b=folderList.begin();
		e=folderList.end();

		while (b != e)
			delete *b++;

		if (dirp)
			closedir(dirp);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	callback2.success("OK");
}

mail::addMessage *mail::mbox::folder::addMessage(mail::callback &callback)
	const
{
	mail::mbox::folder::add *f
		=new mail::mbox::folder::add(mboxAccount, path, callback);

	if (!f)
	{
		callback.fail(path + ": " + strerror(errno));
		return NULL;
	}

	return f;
}

void mail::mbox::folder::createSubFolder(string name,
					 bool isDirectory,
					 mail::callback::folderList &callback1,
					 mail::callback &callback2) const
{
	string fpath;

	char *p=libmail_u_convert_tobuf(name.c_str(), unicode_default_chset(),
					unicode_x_imap_modutf7 " ./~:", NULL);

	if (!p)
		LIBMAIL_THROW("Out of memory.");

	try {
		fpath=path + "/" + p;
		free(p);
	} catch (...) {
		free(p);
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}

	if (isDirectory) {
		if (mkdir(fpath.c_str(), 0700) < 0)
		{
			callback2.fail(fpath + ": " + strerror(errno));
			return;
		}
	} else {
		int fd= ::open(fpath.c_str(), O_RDWR|O_CREAT|O_EXCL, 0600);

		if (fd < 0)
		{
			callback2.fail(fpath + ": " + strerror(errno));
			return;
		}
		close(fd);
	}

	folder newFolder(fpath, mboxAccount);

	vector<const mail::folder *> folderList;

	folderList.push_back(&newFolder);
	callback1.success(folderList);
	callback2.success("OK");
}

void mail::mbox::folder::create(bool isDirectory,
				mail::callback &callback) const
{
	if (isDirectory) {
		if (mkdir(path.c_str(), 0700) < 0)
		{
			callback.fail(path + ": " + strerror(errno));
			return;
		}
	} else {
		int fd= ::open(path.c_str(), O_RDWR|O_CREAT|O_EXCL, 0600);

		if (fd < 0)
		{
			callback.fail(path + ": " + strerror(errno));
			return;
		}
		close(fd);
	}

	callback.success("OK");
}

void mail::mbox::folder::destroy(mail::callback &callback, bool destroyDir)
	const
{
	if (destroyDir) {

		// Clean up garbage first.

		DIR *dirp=opendir(path.c_str());

		try {
			struct dirent *de;

			while (dirp && (de=readdir(dirp)) != NULL)
			{
				const char *p=strrchr(de->d_name, '~');

				if (p && p[1] == 0) // Temp file?
				{
					string s=path + "/" +
						string((const char *)
						       de->d_name, p);

					struct stat stat_buf;

					if (stat(s.c_str(), &stat_buf) < 0)
					{
						s += "~";
						unlink(s.c_str());
					}
					continue;
				}

				for (p=de->d_name; *p; p++)
				{
					if (strncmp(p, ".lock.", 6) == 0)
					{
						string s=path + "/" +
							de->d_name;

						struct stat stat_buf;

						if (stat(s.c_str(), &stat_buf)
						    == 0 &&
						    stat_buf.st_mtime + 120
						    < time(NULL))
							break;
					}

					if (strcmp(p, ".lock") == 0)
					{
						string s=path + "/"
							+ string((const char *)
								 de->d_name,
								 p);
						struct stat stat_buf;

						if (stat(s.c_str(), &stat_buf)
						    < 0)
							break;
					}
				}

				if (*p)
				{
					string s=path + "/" + de->d_name;

					unlink(s.c_str());
				}
			}

			if (dirp)
				closedir(dirp);
		} catch (...) {
			if (dirp)
				closedir(dirp);
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}

		if (rmdir(path.c_str()) < 0)
		{
			callback.fail(path + ": " + strerror(errno));
			return;
		}
	} else {
		if (unlink(path.c_str()) < 0)
		{
			callback.fail(path + ": " + strerror(errno));
			return;
		}
	}

	callback.success("OK");
}

class mail::mbox::RenameTask : public mail::mbox::LockTask {

	string filename;
	string newpath;
	string newname;
	mail::callback::folderList &callback1;

public:
	RenameTask(mbox &mboxAccount,
		   mail::callback::folderList &callback1Arg,
		   mail::callback &callbackArg,
		   string filenameArg,
		   string newpathArg,
		   string newnameArg);
	~RenameTask();

	bool locked(mail::mbox::lock &mlock, std::string path);
	bool locked(mail::file &);
};

mail::mbox::RenameTask::RenameTask(mbox &mboxAccount,
				   mail::callback::folderList &callback1Arg,
				   mail::callback &callbackArg,
				   string filenameArg,
				   string newpathArg,
				   string newnameArg)
	: LockTask(mboxAccount, callbackArg, filenameArg),
	  filename(filenameArg),
	  newpath(newpathArg),
	  newname(newnameArg),
	  callback1(callback1Arg)
{
}

mail::mbox::RenameTask::~RenameTask()
{
}

bool mail::mbox::RenameTask::locked(mail::mbox::lock &mlock, std::string path)
{
	mail::callback *volatile cb= &callback;
	string msg="Folder renamed.";

	if (link(filename.c_str(), newpath.c_str()) < 0)
	{
		fail(strerror(errno));
		return true;
	}
	unlink(filename.c_str());

	mail::mbox::folder newFolder(newpath, mboxAccount);

	vector<const mail::folder *> folders;

	folders.push_back(&newFolder);
	callback1.success( folders );

	try {
		done();
	} catch (...) {
	}
	cb->success(msg);
	return true;
}

bool mail::mbox::RenameTask::locked(mail::file &)
{
	return true;
}

void mail::mbox::folder::renameFolder(const mail::folder *newParent,
				      std::string newName,
				      mail::callback::folderList &callback1,
				      mail::callback &callback) const
{
	if (isDestroyed(callback))
		return;

	if (path == "INBOX")
	{
		callback.fail("Not implemented.");
		return;
	}

	if (mboxAccount.currentFolder.size() > 0)
	{
		if (mboxAccount.currentFolder == path ||
		    (path + "/") ==
		    mboxAccount.currentFolder.substr(0, path.size()+1))
		{
			callback.fail("Cannot RENAME currently open folder.");
			return;
		}
	}

	// The name of the folder is translated from the local charset
	// to modified UTF-7 (Courier-IMAP compatibility), with the following
	// blacklisted characters:

	string nameutf7=mail::iconvert::convert(newName,
						unicode_default_chset(),
						unicode_x_imap_modutf7 " ./~:");

	string newpath=(newParent ? newParent->getPath() + "/":
			string("")) + nameutf7;

	if (newpath.size() == 0 || newpath[0] != '/')
		newpath=mboxAccount.rootPath + "/" + newpath;

	struct stat stat_buf;

	if (stat(path.c_str(), &stat_buf) == 0 &&
	    S_ISDIR(stat_buf.st_mode))
	{
		if (rename(path.c_str(), newpath.c_str()) < 0)
		{
			callback.fail(strerror(errno));
			return;
		}

		mail::mbox::folder newFolder(newpath, mboxAccount);

		vector<const mail::folder *> folders;

		folders.push_back(&newFolder);
		callback1.success( folders );
		callback.success("Mail folder renamed.");
		return;
	}

	mboxAccount.installTask(new mail::mbox::RenameTask(mboxAccount,
							   callback1,
							   callback,
							   path, newpath,
							   newName));
}

mail::folder *mail::mbox::folder::clone() const
{
	return new mail::mbox::folder(path, mboxAccount);
}

string mail::mbox::folder::toString() const
{
	string s="";
	string::const_iterator b=name.begin(), e=name.end();

	while (b != e)
	{
		if (*b == ':' || *b == '\\')
			s += "\\";

		s += *b++;
	}

	// If path is in the home directory, drop $HOME

	string p=path;

	if (!isDestroyed())
	{
		string h=mboxAccount.rootPath;

		if (p == h)
			p="";
		else
		{
			h += "/";
			if (p.size() > h.size() && p.substr(0, h.size()) == h)
				p=p.substr(h.size());
		}
	}

	return s + ":" + p;
}

void mail::mbox::folder::open(mail::callback &openCallback,
			      snapshot *restoreSnapshot,
			      mail::callback::folder &folderCallback) const
{
	if (mboxAccount.currentFolder.size() == 0 &&
	    mboxAccount.folderDirty)
	{
		closeCallback *cc=new closeCallback(&mboxAccount,
						    openCallback,
						    folderCallback,
						    path);

		if (!cc)
		{
			openCallback.fail("Out of memory.");
			return;
		}

		try {
			mboxAccount
				.installTask(new mail::mbox
					     ::ExpungeTask(mboxAccount, *cc,
							   false, NULL));
		} catch (...) {
			delete cc;
			LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
		}
		return;
	}

	mboxAccount.installTask(new mail::mbox::OpenTask(mboxAccount, path, openCallback,
						  &folderCallback));
}

mail::mbox::folder::closeCallback::closeCallback(mail::mbox *origmboxArg,
						 mail::callback
						 &origCallbackArg,
						 mail::callback::folder
						 &origFolderCallbackarg,
						 string origPathArg)
	: origmbox(origmboxArg), origCallback(origCallbackArg),
	  origFolderCallback(origFolderCallbackarg),
	  origPath(origPathArg)
{
}

mail::mbox::folder::closeCallback::~closeCallback()
{
}

void mail::mbox::folder::closeCallback::success(string msg)
{
	if (origmbox.isDestroyed())
	{
		origCallback.success(msg);
		delete this;
	}

	try {
		origmbox->installTask(new OpenTask(*origmbox,
						   origPath,
						   origCallback,
						   &origFolderCallback));
		delete this;
	} catch (...) {
		delete this;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

void mail::mbox::folder::closeCallback::fail(string msg)
{
	origCallback.fail(msg);
	delete this;
}

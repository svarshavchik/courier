/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "nntpfolder.H"
#include "nntpnewsrc.H"
#include "nntplistactive.H"
#include "nntpgroupinfo.H"
#include "nntpgroupopen.H"
#include "nntpadd.H"
#include <fstream>
#include <vector>
#include <list>
#include <map>
#include <errno.h>
#include <cstring>

using namespace std;

mail::nntp::folder::folder(nntp *myserverArg, string pathArg,
			   bool hasMessagesArg, bool hasSubFoldersArg)
	: mail::folder(myserverArg), myserver(myserverArg),
	  path(pathArg), hasMessagesFlag(hasMessagesArg),
	  hasSubFoldersFlag(hasSubFoldersArg)
{
	size_t p=pathArg.rfind('.');

	if (p == std::string::npos)
		p=0;
	else
		++p;

	name=pathArg.substr(p);

	if (strncasecmp(pathArg.c_str(), FOLDER_SUBSCRIBED ".",
			sizeof(FOLDER_SUBSCRIBED)) == 0)
		name=pathArg.substr(sizeof(FOLDER_SUBSCRIBED));

	if (path == FOLDER_SUBSCRIBED)
		name="Subscribed newsgroups";
	if (path == FOLDER_NEWSRC)
		name="All newsgroups";
	if (path == FOLDER_CHECKNEW)
		name="Check for new newsgroups";
	if (path == FOLDER_REFRESH)
		name="Refresh newsgroup list";

}

mail::nntp::folder::folder(const mail::nntp::folder &o)
	: mail::folder(o.myserver), myserver(o.myserver),
	  path(o.path), name(o.name), hasMessagesFlag(o.hasMessagesFlag),
	  hasSubFoldersFlag(o.hasSubFoldersFlag)
{
}

mail::nntp::folder &mail::nntp::folder::operator=(const mail::nntp::folder &o)
{
	path=o.path;
	name=o.name;
	hasMessagesFlag=o.hasMessagesFlag;
	hasSubFoldersFlag=o.hasSubFoldersFlag;

	return *this;
}

mail::nntp::folder::~folder()
{
}

void mail::nntp::folder::sameServerAsHelperFunc() const
{
}

std::string mail::nntp::folder::getName() const
{
	return name;
}

std::string mail::nntp::folder::getPath() const
{
	return path;
}

bool mail::nntp::folder::hasMessages() const
{
	return hasMessagesFlag;
}

bool mail::nntp::folder::hasSubFolders() const
{
	return hasSubFoldersFlag;
}

bool mail::nntp::folder::isParentOf(std::string p) const
{
	string s=path + ".";

	if (p.size() >= s.size() && p.substr(0, s.size()) == s)
		return true;
	return false;
}

void mail::nntp::folder::hasMessages(bool flag)
{
	hasMessagesFlag=flag;
}

void mail::nntp::folder::hasSubFolders(bool flag)
{
	hasSubFoldersFlag=flag;
}

string mail::nntp::folder::newsgroupName() const
{
	string s=path;

	if (s.size() > 0 && s[0] == '/')
	{
		size_t n=s.find('.');

		if (n == std::string::npos)
			s="";
		else
			s=s.substr(n+1);
	}

	if (s.size() > 0 && s.find('\r') == std::string::npos
	    && s.find('\n') == std::string::npos)
		return s;
	return "";
}

string mail::nntp::folder::isNewsgroup() const
{
	return newsgroupName();
}

void mail::nntp::folder::readFolderInfo( callback::folderInfo
					 &callback1,
					 callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	string s=newsgroupName();

	if (s.size() == 0)
	{
		callback2.success("Ok.");
		return;
	}

	myserver->installTask(new GroupInfoTask(&callback2, *myserver,
						s, callback1));
}

void mail::nntp::folder::getParentFolder(callback::folderList &callback1,
					 callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	string ppath("");

	size_t n=path.rfind('.');

	if (n != std::string::npos)
		ppath=path.substr(0, n);

	myserver->findFolder(ppath, callback1, callback2);
}

void mail::nntp::folder::readSubFolders( callback::folderList &callback1,
					 callback &callback2) const
{
	if (isDestroyed(callback2))
		return;

	if (path.size() == 0)
	{
		myserver->readTopLevelFolders(callback1, callback2);
		return;
	}

	if (path == FOLDER_REFRESH)
	{
		myserver->installTask(new ListActiveTask(&callback2,
							 *myserver));
		return;
	}

	list<mail::nntp::folder> folderList;

	if (path == FOLDER_CHECKNEW)
	{
		if (!myserver->hasNewgroups)
		{
			ifstream i(myserver->newsrcFilename.c_str());
			string serverDate;

			if (i.is_open())
			{
				string line;

				while (!getline(i, line).fail())
				{
					if (line.substr(0, 7) == "#DATE: ")
					{
						serverDate=line.substr(7);
						break;
					}
				}
			}

			if (serverDate.size() == 14)
			{
				myserver->
					installTask(new
						    ListActiveTask(&callback2,
								   *myserver,
								   serverDate,
								   &callback1)
						    );
				return;
			}
		}

		vector<string>::iterator b=myserver->newgroups.begin(),
			e=myserver->newgroups.end();

		while (b != e)
		{
			folderList.insert(folderList.end(),
					  folder(myserver,
						 FOLDER_SUBSCRIBED "."
						 + *b, true, false));
			b++;
		}
	}
	else if (path == FOLDER_SUBSCRIBED)
	{
		// List all subscribed folders, in a flat hierarchy

		ifstream i(myserver->newsrcFilename.c_str());

		if (i.is_open())
		{
			string line;

			while (!getline(i, line).fail())
			{
				newsrc parseLine(line);

				if (parseLine.newsgroupname.size() == 0)
					continue;

				if (!parseLine.subscribed)
					continue;

				folderList
					.insert(folderList.end(),
						folder(myserver,
						       FOLDER_SUBSCRIBED "."
						       + parseLine
						       .newsgroupname,
						       true,
						       false));
			}
		}
	}
	else if (path.substr(0, sizeof(FOLDER_NEWSRC)-1) == FOLDER_NEWSRC)
	{
		size_t p=path.find('.');
		string s=p == std::string::npos ? "":path.substr(p+1);
		string sdot=s + ".";

		// All newsrc folders, hierarchically

		map<string, hierEntry> subfolders;

		ifstream i(myserver->newsrcFilename.c_str());

		if (i.is_open())
		{
			string line;

			while (!getline(i, line).fail())
			{
				newsrc parseLine(line);

				if (parseLine.newsgroupname.size() == 0)
					continue;

				string n;

				// Take only those newsrc newsgroup that
				// are in the hierarchy we're opening

				if (s.size() == 0) // Top hierarchy
				{
					n=parseLine.newsgroupname;
				}
				else
				{
					if (parseLine.newsgroupname.size()
					    < sdot.size() ||
					    parseLine.newsgroupname
					    .substr(0, sdot.size())
					    != sdot)
						continue; // Not in this hier

					n=parseLine.newsgroupname
						.substr(sdot.size());
				}

				bool isSubFolder=false;

				p=n.find('.');

				if (p != std::string::npos)
				{
					n=n.substr(0, p);
					isSubFolder=true;
				}

				map<string, hierEntry>::iterator p=
					subfolders
					.insert( make_pair(n,
							   hierEntry()))
					.first;

				if (isSubFolder)
					p->second.foundSubFolders=true;
				else
					p->second.foundFolder=true;
			}
		}
		i.close();

		map<string, hierEntry>::iterator b, e;
		b=subfolders.begin();
		e=subfolders.end();

		while (b != e)
		{
			if (b->second.foundSubFolders)
				folderList.insert(folderList.end(),
						  folder(myserver,
							 path + "." + b->first,
							 false, true));

			if (b->second.foundFolder)
			{
				string s=path + "." + b->first;

				size_t n=s.find('.');

				folderList
					.insert(folderList.end(),
						folder(myserver,
						       FOLDER_SUBSCRIBED +
						       s.substr(n),
						       true,
						       false));
			}
			b++;
		}
	}

	// Convert the folder list to vector of pointers, then invoke the
	// callbacks.

	vector<const mail::folder *> ptrList;

	list<mail::nntp::folder>::iterator b, e;

	b=folderList.begin();
	e=folderList.end();

	while (b != e)
	{
		ptrList.push_back( &*b );
		b++;
	}

	callback1.success(ptrList);
	callback2.success("Ok.");
}

mail::addMessage *mail::nntp::folder::addMessage(callback &callback) const
{
	if (isDestroyed(callback))
		return NULL;

	return new mail::nntp::add(myserver, &callback);
}

void mail::nntp::folder::createSubFolder(std::string name, bool isDirectory,
					 callback::folderList
					 &callback1,
					 callback &callback2) const
{
	callback2.fail("Not implemented");
}

void mail::nntp::folder::create(bool isDirectory,
				callback &callback) const
{
	callback.fail("Not implemented");
}

//
// A request to delete a folder translate to an unsubscribe request.
//

void mail::nntp::folder::destroy(callback &callback, bool destroyDir) const
{
	if (isDestroyed(callback))
		return;

	string s=newsgroupName();

	if (s.size() == 0)
	{
		callback.fail("Not implemented");
		return;
	}

	myserver->updateCachedNewsrc();
	myserver->discardCachedNewsrc();

	string newNewsrcFilename=myserver->newsrcFilename + ".tmp";

	ofstream o(newNewsrcFilename.c_str());

	if (o.is_open())
	{
		ifstream i(myserver->newsrcFilename.c_str());

		string line;

		while (i.is_open() && !getline(i, line).fail())
		{
			newsrc parseLine(line);

			if (parseLine.newsgroupname.size() == 0)
			{
				o << line << endl;
				continue;
			}

			if (parseLine.newsgroupname == s)
			{
				parseLine.subscribed=false;
			}

			o << (string)parseLine << endl;
		}

		if (!o.fail() && !o.bad() &&
		    rename(newNewsrcFilename.c_str(),
			   myserver->newsrcFilename.c_str()) == 0)
		{
			callback.success("Unsubscribed.");
			return;
		}
	}
	callback.fail(strerror(errno));
}

void mail::nntp::folder::renameFolder(const mail::folder *newParent,
				      string newName,
				      callback::folderList &callback1,
				      callback &callback2) const
{
	callback2.fail("Not implemented");
}

mail::folder *mail::nntp::folder::clone() const
{
	if (isDestroyed())
	{
		errno=EIO;
		return NULL;
	}

	return new folder(myserver, path, hasMessagesFlag,
			  hasSubFoldersFlag);
}

string mail::nntp::folder::toString() const
{
	string s="";

	int i;

	for (i=0; i<3; i++)
	{
		string ss;

		if (i > 0)
			s += ":";

		switch (i) {
		case 0:
			if (hasMessagesFlag) ss += "F";
			if (hasSubFoldersFlag) ss += "D";
			break;
		case 1:
			ss=name;
			break;
		case 2:
			ss=path;
			break;
		}

		string::iterator b, e;
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
	return s;
}

void mail::nntp::folder::open(callback &openCallback,
			      snapshot *restoreSnapshot,
			      callback::folder &folderCallback) const
{
	if (isDestroyed(openCallback))
		return;

	string s=newsgroupName();

	if (s.size() > 0)
	{
		myserver->installTask(new GroupOpenTask(&openCallback,
							*myserver,
							s,
							restoreSnapshot,
							&folderCallback));
		return;
	}

	openCallback.fail("Not implemented");
}

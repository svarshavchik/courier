/*
** Copyright 2003-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "smap.H"
#include "misc.H"
#include "imapfolder.H"
#include "smapnoopexpunge.H"
#include "smapnewmail.H"
#include <limits.h>
#include <iostream>
#include <sstream>
#include <errno.h>
#include "rfc822/rfc822.h"
#include <cstring>

using namespace std;

mail::smapHandler::smapHandler(int timeoutValArg)
	: mail::imapHandler(timeoutValArg), defaultCB(NULL)
{
}

mail::smapHandler::~smapHandler()
{
	fail("Operation aborted."); // Just in case
}

int mail::smapHandler::process(imap &imapAccount, std::string &buffer)
{
	return ( (this->*(imapAccount.smapProtocolHandlerFunc))(imapAccount,
								buffer));
}

//
// Expecting a single line reply.
//

int mail::smapHandler::singleLineProcess(imap &imapAccount,
					 std::string &buffer)
{
	size_t p=buffer.find('\n');

	if (p == std::string::npos)
		return (0); // Wait until the whole line is read

	setTimeout();

	++p;
	string buffer_cpy=buffer;
	buffer_cpy.erase(p);

	// Parse line into individual words

	vector<const char *> words;

	string::iterator b=buffer_cpy.begin(), e=buffer_cpy.end();

	while (b != e)
	{
		if (unicode_isspace((unsigned char)*b))
		{
			b++;
			continue;
		}

		if (words.size() == 1)
		{
			if (strcmp(words[0], "+OK") == 0)
			{
				string s=string(b, e);

				while (s.size() > 0 &&
				       unicode_isspace((unsigned char)
					       s.end()[-1]))
					s.erase(s.end()-1, s.end());

				doDestroy=true;
				if (ok(fromutf8(s)))
				{
					if (doDestroy)
						imapAccount.
							uninstallHandler(this);
					return p;
				}
				return 0;
			}

			if (strcmp(words[0], "-ERR") == 0)
			{
				string s=string(b, e);

				while (s.size() > 0 &&
				       unicode_isspace((unsigned char)
					       s.end()[-1]))
					s.erase(s.end()-1, s.end());

				doDestroy=true;

				if (fail(fromutf8(s)))
				{
					if (doDestroy)
						imapAccount.
							uninstallHandler(this);
					return p;
				}
				return 0;
			}
		}

		words.push_back(&*b);

		if (*b != '"') // Not quoted - look for next space
		{
			while (b != e && !unicode_isspace((unsigned char)*b))
				b++;

			if (b != e)
				*b++=0;
			continue;
		}

		// Extract quoted word.

		string::iterator c=b;

		b++;

		while (b != e)
		{
			if (*b == '"')
			{
				b++;
				if (b == e || *b != '"')
					break;
			}
			*c++ = *b++;
		}
		*c=0;
	}
	if (!processLine(imapAccount, words))
		return 0;

	return p;
}

//
// Reading a dot-stuffed multiline response
//
int mail::smapHandler::multiLineProcessDotStuffed(imap &imapAccount,
						  std::string &buffer)
{
	size_t p=buffer.find('\n');

	if (p == std::string::npos)
		return (0); // Also wait until an entire line is read

	setTimeout();

	string buffer_cpy=buffer;
	buffer_cpy.erase(p);
	++p;

	// Swallow trailing CR

	if (buffer_cpy.size() > 0 && buffer_cpy.end()[-1] == '\r')
		buffer_cpy.erase(buffer_cpy.end()-1, buffer_cpy.end());

	if (buffer_cpy.size() > 0 && buffer_cpy[0] == '.')
	{
		buffer_cpy.erase(buffer_cpy.begin(),
				 buffer_cpy.begin()+1);
		if (buffer_cpy.size() == 0) // Lone dot
		{
			imapAccount.smapProtocolHandlerFunc=
				&smapHandler::singleLineProcess;
			endData(imapAccount);
			return p;
		}
	}

	processData(imapAccount, buffer_cpy + "\n");
	return p;
}

// Reading a multi-line binary response

int mail::smapHandler::multiLineProcessBinary(imap &imapAccount,
					      std::string &buffer)
{
	if (imapAccount.smapBinaryCount == 0) // Next chunk
	{
		size_t p=buffer.find('\n');

		if (p == std::string::npos)
			return (0); // Read one line

		setTimeout();

		++p;
		string buffer_cpy=buffer;
		buffer_cpy.erase(p);

		istringstream i(buffer_cpy);
		unsigned long nextChunk=0;

		i >> nextChunk;

		if (i.fail() || nextChunk == 0) // Empty line.  Done.
		{
			imapAccount.smapProtocolHandlerFunc=
				&smapHandler::singleLineProcess;
			endData(imapAccount);
			return p;
		}

		imapAccount.smapBinaryCount=nextChunk;
		return p;
	}

	// Ok, some # of bytes to go

	if (imapAccount.smapBinaryCount >= buffer.size())
	{
		imapAccount.smapBinaryCount -= buffer.size();
		setTimeout();
		processData(imapAccount, buffer);
		return buffer.size(); // Processed everything
	}

	string buffer_cpy=buffer;

	buffer_cpy.erase(imapAccount.smapBinaryCount);
	imapAccount.smapBinaryCount=0;
	setTimeout();
	processData(imapAccount, buffer_cpy);
	return buffer_cpy.size();
}

void mail::smapHandler::commaSplit(string s, vector<string> &a)
{
	while (s.size() > 0)
	{
		size_t n=s.find(',');

		if (n == std::string::npos)
		{
			a.push_back(s);
			break;
		}

		if (n > 0)
			a.push_back(s.substr(0, n));
		s=s.substr(n+1);
	}
}

//
// Default single line response handler
//

bool mail::smapHandler::processLine(imap &imapAccount,
				    std::vector<const char *> &words)
{
	if (words.size() == 0)
		return true;

	const char *p=words[0];

	if (*p == '{') // Start of a multiline response?
	{
		if (*++p == '.') // Dot-stuffed
		{
			string n(++p);
			istringstream i(n);

			unsigned long estimate=0;

			i >> estimate;

			words.erase(words.begin(), words.begin()+1);

			imapAccount.smapProtocolHandlerFunc=
				&smapHandler::multiLineProcessDotStuffed;
			beginProcessData(imapAccount, words, estimate);
			return true;
		}

		// Binary

		string n(p);
		istringstream i(n);
		unsigned long firstChunk=0;
		unsigned long estimate=0;
		char dummy=0;

		i >> firstChunk >> dummy >> estimate;

		imapAccount.smapBinaryCount=firstChunk;
		imapAccount.smapProtocolHandlerFunc=
			&smapHandler::multiLineProcessBinary;

		words.erase(words.begin(), words.begin()+1);
		beginProcessData(imapAccount, words, estimate);
		return true;
	}

	if (words.size() >= 3 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "FETCH") == 0)
	{
		string n(words[2]);
		istringstream i(n);
		size_t msgNum=0;

		i >> msgNum;

		if (!i.fail() && msgNum > 0 &&
		    imapAccount.currentFolder &&
		    msgNum <= imapAccount.currentFolder->index.size())
		{
			--msgNum;

			vector<const char *>::iterator b=words.begin()+3,
				e=words.end();

			for ( ; b != e; b++)
			{
				if (strncasecmp( *b, "FLAGS=", 6) == 0)
				{
					vector<string> flagList;

					commaSplit( (*b)+6, flagList);

					vector<string>::iterator fb, fe;

					fb=flagList.begin();
					fe=flagList.end();

					mail::messageInfo newMessageInfo;

					newMessageInfo.uid=
						imapAccount.currentFolder
						->index[msgNum].uid;
					newMessageInfo.unread=true;

					while (fb != fe)
					{
						const char *c= (*fb).c_str();

#define FLAG true
#define NOTFLAG false
#define DOFLAG(value, field, name) \
	if (strcasecmp(c, name) == 0) \
		newMessageInfo.field=value;

						LIBMAIL_SMAPFLAGS;

						fb++;
					}
					(mail::messageInfo &)
					imapAccount.currentFolder
						->index[msgNum]=newMessageInfo;

					if (msgNum < imapAccount.currentFolder
					    ->exists)
					{
						messageChanged(msgNum);
					}
#if 0
					cerr << "FLAGS[" << msgNum
					     << "]: deleted="
					     << newMessageInfo.deleted
					     << ", replied="
					     << newMessageInfo.replied
					     << ", unread="
					     << newMessageInfo.unread
					     << ", draft="
					     << newMessageInfo.draft
					     << ", marked="
					     << newMessageInfo.marked
					     << endl;
#endif
					fetchedIndexInfo();
				}

				if (strncasecmp( *b, "KEYWORDS=", 9) == 0)
				{
					vector<string> flagList;

					commaSplit( (*b)+9, flagList);

					vector<string>::iterator fb, fe;

					fb=flagList.begin();
					fe=flagList.end();

					mail::keywords::Message newMessage;

					while (fb != fe)
					{
						if (!newMessage
						    .addFlag(imapAccount.
							     keywordHashtable,
							     *fb))
							LIBMAIL_THROW(strerror(errno));
						++fb;
					}

					imapAccount.currentFolder
						->index[msgNum].keywords=
						newMessage;

					if (msgNum < imapAccount.currentFolder
					    ->exists)
					{
						messageChanged(msgNum);
					}
				}

				if (strncasecmp( *b, "UID=", 4) == 0)
				{
					imapAccount.currentFolder
						->index[msgNum].uid= (*b)+4;
#if 0
					cerr << "UID[" << msgNum << "]="
					     <<	imapAccount.currentFolder
						->index[msgNum].uid << endl;
#endif
					fetchedIndexInfo();
				}

				if (strncasecmp( *b, "SIZE=", 5) == 0)
				{
					unsigned long bytes=0;

					string s= *b + 5;
					istringstream i(s);

					i >> bytes;

					fetchedMessageSize(msgNum, bytes);
				}

				if (strncasecmp( *b, "INTERNALDATE=", 13) == 0)
				{
					time_t n= rfc822_parsedt(*b + 13);

					if (n)
						fetchedInternalDate(msgNum, n);
				}
			}
		}
		return true;
	}

	if (words.size() >= 3 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "EXISTS") == 0)
	{
		string n(words[2]);
		istringstream i(n);
		size_t msgNum=0;

		i >> msgNum;

		// Check against hostile servers

		if (msgNum > UINT_MAX / sizeof(mail::messageInfo))
			msgNum=UINT_MAX / sizeof(mail::messageInfo);

		if (!i.fail() && msgNum > 0 &&
		    imapAccount.currentFolder &&
		    !imapAccount.currentFolder->closeInProgress &&
		    msgNum > imapAccount .currentFolder->index.size())
		{
			existsOrExpungeSeen();
			imapAccount.currentFolder->existsMore(imapAccount,
							      msgNum);
		}
		return true;
	}

	if (words.size() >= 2 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "EXPUNGE") == 0)
	{
		vector< pair<size_t, size_t> > removedList;

		vector<const char *>::iterator b=words.begin()+2,
				e=words.end();

		while (b != e)
		{
			string n(*b++);
			istringstream i(n);

			size_t first, last;
			char dummy;

			size_t p=n.find('-');

			if (p != std::string::npos)
			{
				i >> first >> dummy >> last;
			}
			else
			{
				i >> first;
				dummy='-';
				last=first;
			}

			if (i.fail() || dummy != '-' || last < first ||
			    first <= 0 ||
			    (last >= (imapAccount.currentFolder ? (size_t)
				      (imapAccount.currentFolder->index.size()
				       + 1):0))
			    || (removedList.size() > 0 &&
				first <= removedList.end()[-1].second))
				continue; // Ignore bogosity

			removedList.push_back(make_pair(first, last));
		}

		if (removedList.size() == 0)
			return true;

		vector< pair<size_t, size_t> >::iterator
			rb=removedList.begin(), re=removedList.end();

		vector<imapFOLDERinfo::indexInfo> &index=
			imapAccount.currentFolder->index;

		while (rb != re)
		{
			--re;

			--re->first;
			index.erase(index.begin() + re->first,
				    index.begin() + re->second);

			if (imapAccount.currentFolder->exists >= re->first)
			{
				if (imapAccount.currentFolder->exists
				    < re->second)
					imapAccount.currentFolder->exists
						=re->first;
				else
					imapAccount.currentFolder->exists
						-= re->second - re->first;
			}
			--re->second;
		}

		if (!imapAccount.currentFolder->closeInProgress)
			existsOrExpungeSeen();

		messagesRemoved(removedList);
		return true;
	}

	if (words.size() >= 3 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "SNAPSHOT") == 0)
	{
		if (imapAccount.currentFolder)
			imapAccount.currentFolder->
				folderCallback.saveSnapshot(words[2]);

		return true;
	}

	return false;
}

// Default notification handler passes along the expunged/changed list to the
// application.

void mail::smapHandler::messagesRemoved(vector< pair<size_t, size_t> >
					&removedList)
{
	myimap->currentFolder->folderCallback.messagesRemoved(removedList);
}

void mail::smapHandler::messageChanged(size_t msgNum)
{
	myimap->currentFolder->folderCallback.messageChanged(msgNum);
}

void mail::smapHandler::existsOrExpungeSeen()
{
}

void mail::smapHandler::fetchedMessageSize(size_t msgNum,
					   unsigned long bytes)
{
}

void mail::smapHandler::fetchedInternalDate(size_t msgNum,
					    time_t internalDate)
{
}

void mail::smapHandler::fetchedIndexInfo()
{
}

void mail::smapHandler::beginProcessData(imap &imapAccount,
					 std::vector<const char *> &w,
					 unsigned long estimatedSize)
{
}

void mail::smapHandler::processData(imap &imapAccount,
				    std::string data)
{
}

void mail::smapHandler::endData(imap &imapAccount)
{
}

// Default ok/fail handlers invoke the callback function

bool mail::smapHandler::ok(std::string s)
{
	mail::callback *p=defaultCB;

	defaultCB=NULL;

	if (p)
		p->success(s);
	return true;
}

bool mail::smapHandler::fail(std::string s)
{
	mail::callback *p=defaultCB;

	defaultCB=NULL;

	if (p)
		p->fail(s);
	return true;
}

void mail::smapHandler::timedOut(const char *errmsg)
{
	mail::callback *p=defaultCB;

	defaultCB=NULL;

	if (p)
		callbackTimedOut(*p, errmsg);
}

// Convert folder names to a path string

string mail::smapHandler::words2path(vector<const char *> &w)
{
	string path="";

	vector<const char *>::iterator b=w.begin(), e=w.end();

	while (b != e)
	{
		if (path.size() > 0)
			path += "/";

		path += mail::iconvert::convert(*b, "utf-8",
						unicode_x_imap_modutf7 " /");

		b++;
	}

	return path;
}

void mail::smapHandler::path2words(string path, vector<string> &words)
{
	string::iterator b=path.begin(), e=path.end();

	while (b != e)
	{
		string::iterator c=b;

		while (c != e && *c != '/')
			c++;

		string component=string(b, c);

		b=c;
		if (b != e)
			b++;

		words.push_back(mail::iconvert::convert(component,
							unicode_x_imap_modutf7,
							"utf-8"));
	}

	if (words.size() == 0)
		words.push_back("");
}

///////////////////////////////////////////////////////////////////////////
//
// Currently open folder

mail::smapFOLDER::smapFOLDER(std::string pathArg,
			     mail::callback::folder &folderCallbackArg,
			     mail::imap &myserver)
	: imapFOLDERinfo(pathArg, folderCallbackArg),
	  imapHandler(myserver.noopSetting), openedFlag(false)
{
	mailCheckInterval=myserver.noopSetting;
}

mail::smapFOLDER::~smapFOLDER()
{
}

void mail::smapFOLDER::existsMore(mail::imap &imapAccount, size_t n)
{
	size_t o=index.size();
	
	imapFOLDERinfo::indexInfo newInfo;
	newInfo.unread=true;

	index.resize(n);

	while (o < n)
		index[o++]=newInfo;

	myimap->installForegroundTask(new smapNEWMAIL(NULL, false));
}

void mail::smapFOLDER::opened()
{
	openedFlag=true;
}

void mail::smapFOLDER::resetMailCheckTimer()
{
	setTimeout(mailCheckInterval);
}

int mail::smapFOLDER::getTimeout(mail::imap &imapAccount)
{
	int t= imapHandler::getTimeout(imapAccount);

	if (t == 0)
	{
		t=MAILCHECKINTERVAL;
		setTimeout(t);

		imapHandler *h=NULL;

		if (!closeInProgress &&
		    ((h=imapAccount.hasForegroundTask()) == NULL ||
		     strcmp(h->getName(), "IDLE") == 0))
		{
			imapAccount
				.installForegroundTask(new
						       smapNoopExpunge("NOOP",
								       imapAccount)
						       );
		}
	}

	return t;
}

void mail::smapFOLDER::installed(imap &imapAccount)
{
}

int mail::smapFOLDER::process(imap &imapAccount, std::string &buffer)
{
	return 0;
}

const char mail::smapFOLDER::name[]="smapFOLDER";

const char *mail::smapFOLDER::getName()
{
	return name;
}

void mail::smapFOLDER::timedOut(const char *errmsg)
{
}

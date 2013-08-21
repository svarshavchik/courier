/*
** Copyright 2002-2011, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include <cstring>

#include "addressbook.H"
#include "addressbookadd.H"
#include "addressbookget.H"
#include "addressbookopen.H"
#include "addressbooksearch.H"
#include "rfc2047decode.H"
#include "misc.H"
#include <errno.h>

using namespace std;

mail::addressbook::Entry::Entry()
{
}

mail::addressbook::Entry::~Entry()
{
}

mail::addressbook::addressbook(mail::account *serverArg,
			       mail::folder *folderArg)
	: server(serverArg),
	  folder(folderArg)
{
}

mail::addressbook::~addressbook()
{
}

void mail::addressbook::addEntry(Entry &newEntry, string olduid,
				 mail::callback &callback)
{
	vector<Index>::iterator b=index.begin(), e=index.end();

	while (b != e)
	{
		Index &i= *b++;

		if (olduid.size() > 0 && i.uid == olduid)
			continue;

		if (newEntry.nickname == i.nickname)
		{
			callback.fail("Address book entry with the same name already exists.");
			return;
		}
	}

	Add *add=new Add(this, newEntry, olduid, callback);

	try {
		add->go();
	} catch (...) {
		delete add;
	}
}

void mail::addressbook::importEntries(std::list<Entry> &newEntries,
				      mail::callback &callback)
{
	Add *add=new Add(this, newEntries, callback);

	try {
		add->go();
	} catch (...) {
		delete add;
	}
}

// Delete an address book entry.

void mail::addressbook::del(std::string uid, mail::callback &callback)
{
	vector<Index>::iterator b=index.begin(), e=index.end();

	while (b != e)
	{
		if (b->uid == uid)
		{
			vector<size_t> msgList;

			msgList.push_back(b - index.begin());

			server->removeMessages(msgList, callback);
			return;
		}

		b++;
	}

	callback.success("OK");
}

void mail::addressbook::open(mail::callback &callback)
{
	Open *o=new Open(this, callback);

	try {
		o->go();
	} catch (...) {
		delete o;
	}
}

mail::addressbook::Index::Index() : nickname("(corrupted entry)")
{
}

mail::addressbook::Index::~Index()
{
}

void mail::addressbook::setIndex(size_t messageNumber,
				 string subject)
{
	subject= mail::rfc2047::decoder().decode(subject, "utf-8");

	if (messageNumber < index.size())
	{
		Index newEntry;

		size_t i=subject.find('[');

		if (i != std::string::npos)
		{
			subject=subject.substr(i+1);

			i=subject.find(']');

			if (i != std::string::npos)
			{
				newEntry.nickname=subject.substr(0, i);
				if (newEntry.nickname.size() == 0)
					newEntry.nickname="(none)";

				char *p=libmail_u_convert_tobuf(newEntry
								.nickname
								.c_str(),
								"utf-8",
								unicode_default_chset(),
								NULL);

				if (!p)
					LIBMAIL_THROW(strerror(errno));

				try {
					newEntry.nickname=p;
					free(p);
				} catch (...) {
					free(p);
					LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
				}
			}
		}

		newEntry.uid=server->getFolderIndexInfo(messageNumber).uid;
		index[messageNumber]=newEntry;
	}
}

void mail::addressbook::newMessages()
{
}

void mail::addressbook::messagesRemoved(vector< pair<size_t, size_t> > &r)
{
	vector< pair<size_t, size_t> >::iterator b=r.begin(), e=r.end();

	while (b != e)
	{
		--e;

		size_t from=e->first, to=e->second;

		if (from >= index.size())
			continue;

		if (to > index.size())
			to=index.size();

		index.erase(index.begin() + from, index.begin() + to + 1);
	}
}

void mail::addressbook::messageChanged(size_t n)
{
}

void mail::addressbook::getIndex( list< pair<string, std::string> > &listArg,
	       mail::callback &callback)
{
	size_t n=index.size();
	size_t i;

	for (i=0; i<n; i++)
	{
		string nickname=index[i].nickname;
		string uid=index[i].uid;

		if (nickname.size() > 0 && uid.size() > 0)
			listArg.push_back( make_pair(nickname, uid));
	}

	callback.success("Address book index retrieved");
}

template<class T>
void mail::addressbook::searchNickname( string nickname,
					vector<T> &addrListArg,
					mail::callback &callback)
{
	Search<T> *s=new Search<T>(this, addrListArg, callback);

	if (!s)
	{
		callback.fail(strerror(errno));
		return;
	}

	try {
		size_t n=index.size();
		size_t i;

		for (i=0; i<n; i++)
			if (index[i].nickname == nickname)
				s->uidList.push_back(server->
						     getFolderIndexInfo(i)
						     .uid);
		s->go();
	} catch (...) {
		delete s;
		LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
	}
}

template
void mail::addressbook::searchNickname( string nickname,
					vector<mail::address> &addrListArg,
					mail::callback &callback);
template
void mail::addressbook::searchNickname( string nickname,
					vector<mail::emailAddress> &addrListArg,
					mail::callback &callback);

template<class T>
void mail::addressbook::getEntry( string uid,
				  vector<T> &addrListArg,
				  mail::callback &callback)
{
	size_t n=index.size();
	size_t i;

	for (i=0; i<n; i++)
		if (server->getFolderIndexInfo(i).uid == uid)
		{
			GetAddressList<T> *get=
				new GetAddressList<T>(this, i,
						      addrListArg,
						      callback);

			if (!get)
			{
				callback.fail(strerror(errno));
				return;
			}

			try {
				get->go();
			} catch (...) {
				delete get;
				LIBMAIL_THROW(LIBMAIL_THROW_EMPTY);
			}
			return;
		}

	callback.success("NOT FOUND.");
}

template
void mail::addressbook::getEntry( string uid,
				  vector<mail::address> &addrListArg,
				  mail::callback &callback);
template
void mail::addressbook::getEntry( string uid,
				  vector<mail::emailAddress>
				  &addrListArg,
				  mail::callback &callback);

/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "smapmsgrange.H"
#include "imap.H"

mail::smapUidSet::smapUidSet( mail::imap &imapAccount,
			      const std::vector<size_t> &v)
{
	if (imapAccount.currentFolder == NULL)
		return;

	std::vector<size_t>::const_iterator b=v.begin(), e=v.end();

	while (b != e)
	{
		size_t n= *b++;

		if (n >= imapAccount.currentFolder->exists)
			continue;

		insert(imapAccount.currentFolder->index[n].uid);
	}
}

mail::smapUidSet::~smapUidSet()
{
}

bool mail::smapUidSet::getNextRange( mail::imap &imapAccount,
				     std::ostringstream &s)
{
	size_t i;
	size_t n=imapAccount.currentFolder ?
		imapAccount.currentFolder->exists:0;

	std::list< std::pair<size_t, size_t> > rangeList;
	size_t rangeCount=0;

	for (i=0; i<n; i++)
	{
		std::set< std::string>::iterator pp=
			find(imapAccount.currentFolder->index[i].uid);

		if (pp == end())
			continue;

		if (!rangeList.empty())
		{
			std::list< std::pair<size_t, size_t> >::iterator
				p= --rangeList.end();

			if (p->second + 1 == i)
			{
				++p->second;
				erase(pp);
				continue;
			}
		}

		if (++rangeCount > 100)
			break;

		erase(pp);
		rangeList.insert(rangeList.end(), std::make_pair(i, i));
	}

	if (rangeList.empty())
		return false;

	while (!rangeList.empty())
	{
		std::list< std::pair<size_t, size_t> >::iterator
			b=rangeList.begin();

		s << " " << (b->first+1) << "-" << (b->second+1);
		rangeList.pop_front();
	}
	return true;
}

mail::smapMsgRange::smapMsgRange()
{
}

void mail::smapMsgRange::init( mail::imap &imapAccount,
			       const mail::smapUidSet &uidSet)
{
	size_t i;
	size_t n=imapAccount.currentFolder ?
		imapAccount.currentFolder->exists:0;

	for (i=0; i<n; i++)
	{
		if (uidSet.count(imapAccount.currentFolder->index[i].uid)
		    == 0)
			continue;

		if (!empty())
		{
			mail::smapMsgRange::iterator p= --end();

			if (p->second + 1 == i)
			{
				++p->second;
				continue;
			}
		}

		insert(end(), std::make_pair(i, i));
	}
}

mail::smapMsgRange::~smapMsgRange()
{
}

bool mail::smapMsgRange::operator>>( std::ostringstream &s)
{
	bool flag=false;
	int i;

	for (i=0; i<100; i++)
	{
		if (empty())
			break;

		flag=true;

		std::list< std::pair<size_t, size_t> >::iterator b=begin();

		s << " " << (b->first+1) << "-" << (b->second+1);

		pop_front();
	}

	return flag;
}

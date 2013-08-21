/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "nntpnewsrc.H"
#include <sstream>

using namespace std;

mail::nntp::newsrc::newsrc() : subscribed(true)
{
}

mail::nntp::newsrc::~newsrc()
{
}

mail::nntp::newsrc::newsrc(string s) : subscribed(true)
{
	if (s.size() == 0 || s[0] == '#')
		return;

	string::iterator b=s.begin(), e=s.end();

	while (b != e && !unicode_isspace((unsigned char)*b))
	{
		if (*b == ':')
		{
			newsgroupname=string(s.begin(), b);
			break;
		}

		if (*b == '!')
		{
			subscribed=false;
			newsgroupname=string(s.begin(), b);
			break;
		}
		b++;
	}

	while (b != e)
	{
		if (unicode_isspace((unsigned char)*b) || *b == ',')
		{
			b++;
			continue;
		}

		string::iterator p=b;

		while (b != e)
		{
			if (unicode_isspace((unsigned char)*b) || *b == ',')
				break;
			b++;
		}

		string range=string(p, b);

		msgnum_t firstMsg=0, lastMsg=0;
		char dummy;

		istringstream i(range);

		i >> firstMsg >> dummy >> lastMsg;

		if (firstMsg > 0)
		{
			if (dummy == '-' && lastMsg >= firstMsg)
				;
			else
				lastMsg=firstMsg;

			if (msglist.size() > 0)
			{
				msgnum_t ending=msglist.end()[-1].second;

				if (ending+1 == firstMsg)
				{
					msglist.end()[-1].second=lastMsg;
					continue;
				}

				if (ending >= firstMsg)
					continue; // Error -- ignore
			}

			msglist.push_back( make_pair(firstMsg, lastMsg));
		}
	}
}

mail::nntp::newsrc::operator string() const
{
	ostringstream o;

	o << newsgroupname << (subscribed ? ":":"!");

	string sep=" ";

	vector< pair<msgnum_t, msgnum_t> >::const_iterator b=msglist.begin(),
		e=msglist.end();

	while (b != e)
	{
		o << sep;
		sep=",";
		o << b->first;

		if (b->second > b->first)
			o << "-" << b->second;
		b++;
	}

	return o.str();
}

// Mark msg as read

void mail::nntp::newsrc::read(msgnum_t m)
{
	vector< pair<msgnum_t, msgnum_t> >::iterator b=msglist.begin(),
		e=msglist.end();

	// We typically do stuff at the end of the list

	while (b != e)
	{
		--e;

		if (e->first > m)
			continue;

		if (m <= e->second)
			return; // Already marked read

		bool growThis= e->second + 1 == m; // Grow this entry

		bool growNext= e != msglist.end() && m + 1 == e[1].first;

		if (growThis)
		{
			if (growNext) // Merge two ranges
			{
				e->second=e[1].second;
				msglist.erase(e+1);
				return;
			}

			++e->second;
			return;
		}
		if (growNext)
		{
			e[1].first--;
			return;
		}

		msglist.insert(e+1, make_pair(m, m));
		return;
	}

	if (msglist.size() > 0 && m + 1 && msglist[0].first)
	{
		msglist[0].first--;
		return;
	}

	msglist.insert(msglist.begin(), make_pair(m, m));
}

void mail::nntp::newsrc::unread(msgnum_t m)
{
	vector< pair<msgnum_t, msgnum_t> >::iterator b=msglist.begin(),
		e=msglist.end();

	while (b != e)
	{
		--e;

		if (e->second < m)
			break;

		if (e->first > m)
			continue;


		if (e->first == e->second) // Delete this range
		{
			msglist.erase(e);
			break;
		}

		if (e->first == m)
		{
			++e->first;
			break;
		}

		if (e->second == m)
		{
			--e->second;
			break;
		}

		// Split

		msglist.insert(e+1, make_pair(m+1, e->second));
		e->second=m-1;
		break;
	}
}

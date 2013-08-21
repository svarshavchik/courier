/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"aliases.h"
#include	"courier.h"
#include	<string.h>

#include	<string.h>

//
// MAXRECSIZE is the median GDBM record size makealiases creates.  The actual
// record size may be +/- 50%
//

#define	MAXRECSIZE	512

//
// In each record, you have addresses separated by newlines.  Search for
// a specific address, and return an offset to it, or -1 if not found.
//

int AliasRecord::search(const char *p)
{
	int	l=strlen(p);
	const char *q=list.c_str(), *qorig=q;

	while (q && *q)
	{
		if (strncmp(q, p, l) == 0 && (q[l] == '\n' || q[l] == '\0'))
			return (q-qorig);
		while (*q)
			if (*q++ == '\n')
				break;
	}
	return (-1);
}

//
// Add a list of addresses to this alias.  They are added only if they
// do not exist.  Optionally, addresses which are already in the alias
// are removed from the list, so upon exit the list contains only those
// addresses which were added.
//

void AliasRecord::Add(std::list<std::string> &addlist, bool removedeleted)
{
	std::list<std::string> mylist, *listptr= &addlist;
	std::string s;

	// If we want DON'T want to remove dupes, make a copy of the list.

	if (!removedeleted)
	{
		mylist.insert(mylist.end(), addlist.begin(), addlist.end());
		listptr= &mylist;
	}

	// Go through all the records in this alias, removing duplicates.

	if ( Init() == 0)
	{
		do
		{
			std::list<std::string>::iterator b, e, p;

			for (b=listptr->begin(),
				     e=listptr->end(); b != e; )
			{
				p=b++;

				if (search( p->c_str() ) >= 0)
				{
					listptr->erase(p);
					break;
				}
			}

			++recnum;
		} while (fetch(1) == 0);
		--recnum;
	}

	if (listptr->empty())	return;

	// Now, append the new addresses.

	std::list<std::string>::iterator b, e;

	for (b=listptr->begin(), e=listptr->end(); b != e; ++b)
	{
		if (list.size() && list.size() + b->size() >= MAXRECSIZE)
		{
			update();
			++recnum;
			list="";
		}
		list +=  *b;
		list += '\n';
	}
	update();
}

//
// When building the alias list, sometimes it may be necessary to remove
// an address from an alias if the address itself represents an alias.
//

void AliasRecord::Delete(const char *addr)
{
int	l=strlen(addr);

	if (Init())	return;	// Doesn't exist

	do
	{
	int	pos=search(addr);

		if (pos >= 0)
		{
			std::string newlist=list.substr(0, pos);

			const char *p=list.c_str()+pos;

			if (p[l])	l++;	// newline also deleted
			list=newlist+(p+l);

			if (list.size() >= MAXRECSIZE/2)
			{
				update();
				return;
			}

			// Try to merge small blocks which are left.

			std::string	save_list(list);
			int	save_recnum=recnum;
			std::string	save_recname(recname);

			do
			{
				save_recname=recname;
				++recnum;
			} while (fetch(1) == 0);

			if (--recnum == save_recnum)
			{
				// Deleted last record.

				if (recnum == 0)
				{
					// Only record, so just update it.
					update();
					return;
				}

				// Just copy the last record into the previous
				// one.

				save_recnum= --recnum;
				fetch(1);
			}

			list += save_list;
			gdbm->Delete(save_recname);
			recnum=save_recnum;
			update();
			return;
		}
		++recnum;
	} while ( fetch(1) == 0);
	--recnum;
}

void AliasRecord::update()
{
	mkrecname();
	if (gdbm && gdbm->Store(recname,
		list, "R"))
		clog_msg_errno();
}

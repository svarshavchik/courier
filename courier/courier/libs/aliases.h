/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	aliases_h
#define	aliases_h

#include	"config.h"
#include	"dbobj.h"

#include	<string>
#include	<list>

/////////////////////////////////////////////////////////////////////////////
//
// AliasRecord - query and update the aliases.dat file.
//
// (temporary scratch files used for building the final aliases.dat file
// also follows this format)
//
/////////////////////////////////////////////////////////////////////////////

struct rw_transport;

class AliasRecord {
protected:
	DbObj *gdbm;		// The GDBM file
	std::string listname;	// The name of the list (the GDBM key, usually)
	std::string recname;	// GDBM key. Usually same as listname, unless
				// we spill over a large list into multiple
				// records.  Calculated from listname+recnum.
	std::string	list;	// The list itself, the contents of the keyed
				// record.
	int	recnum;		// 0 for the first record with this key,
				// then 1, 2, ... for any spillover records.
	const char *feptr;	// When stepping through list, points to the
				// next record to retrieve.
public:
	AliasRecord() : gdbm(0), recnum(0), feptr(0)
		{
		}
	AliasRecord(DbObj &d) : gdbm(&d), recnum(0), feptr(0)
		{
		}
	void operator=(DbObj *p) { gdbm=p; }

	~AliasRecord();
	void	Init(std::string);
	int	Add(const char *);
	void	Add(std::list<std::string> &, bool);
	void	Delete(const char *);

	void	StartForEach();
	std::string NextForEach();
	int	Init();

	int	Found() { return ( Init() == 0 ? 1:0); }
private:
	int	fetch(int keep);
	void	update();
	int	search(const char *);
	void	mkrecname();
} ;

class AliasHandler {
public:
	AliasHandler()	{}
	virtual ~AliasHandler()	{}
	virtual void	Alias(const char *)=0;
} ;

class	AliasSearch {
	DbObj module_alias, local_alias;
	AliasRecord module_record, local_record;
	std::string modulename;

public:
	AliasSearch();

	~AliasSearch()	{}

	void	Open(const char *);
	void	Open(struct rw_transport *);
	int	Search(const char *, AliasHandler &);
	int	Found(const char *);
private:
	int	Try(AliasRecord &, const char *, AliasHandler &);
	int	TryVirtual(AliasRecord &, const char *, AliasHandler *);
	int	TrySearch(AliasRecord &, const char *);

	int	TryAliasD(const char *, AliasHandler *);
	} ;

#endif

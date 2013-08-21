#ifndef	cmlmarchive_h
#define	cmlmarchive_h
/*
** Copyright 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/


#include	"config.h"
#include	"cmlm.h"
#include	"mydirent.h"

#include	<string>

/////////////////////////////////////////////////////////////////////////////
//
// Message archive.  The Archive class is used to put or get messages from
// the archive.  The ArchiveList class lists messages in the archive.
//
/////////////////////////////////////////////////////////////////////////////

class Archive: private ExclusiveLock {

	unsigned long seq_no;

public:
	Archive();
	~Archive();

	int get_seq_no(unsigned long &);	// Return next seq no of msg
	int save_seq_no();		// Save the sequence number

	static	std::string filename(unsigned long);
} ;


class ArchiveList : private SharedLock {

	DIR	*dirp;
public:
	ArchiveList();
	~ArchiveList();
	int	Next(unsigned long &);
	std::string	Next();
} ;

#endif

/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapmsgrange_H
#define libmail_smapmsgrange_H

#include "imap.H"
#include "imapfolder.H"
#include "namespace.H"

#include <list>
#include <set>
#include <vector>
#include <sstream>

LIBMAIL_START

//
// Helper class for the helper class: after the time a request was made,
// and before it actually comes up in the scheduling queue, some messages
// might be expunged.  Therefore, at first save everyone's UID, and
// then rebuild the message range from the UID list, when our turn comes
// up.

class smapUidSet : public std::set< std::string> {
public:
	smapUidSet( mail::imap &, const std::vector<size_t> &);
	~smapUidSet();

	// Convert to smap message ranges on the fly

	bool getNextRange( mail::imap &imapAccount,
			   std::ostringstream &s);

};

//
// Helper class: take a list of messages and convert it to smap message ranges
//

class smapMsgRange : public std::list< std::pair<size_t, size_t> > {

public:
	smapMsgRange();

	void init( mail::imap &, const smapUidSet &);
	~smapMsgRange();

	bool operator>>( std::ostringstream &);
	// Pop off up to hunnert ranges into a stringstream
	// Returns false if no ranges are left to process.

};

LIBMAIL_END

#endif

/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#ifndef libmail_snapshot_H
#define libmail_snapshot_H

#include "libmail_config.h"
#include "namespace.H"

#include <string>
#include <set>

LIBMAIL_START

class messageInfo;

//
// Some drivers can open a folder faster if folder snapshots are used.
// At predetermined times, those drives invoke callback::folder::saveSnapshot()
// with a unique snapshot identifier.  The application shoud use
// mail::account::getFolderIndexSize(), then getFolderIndexInfo() and save
// the folder index's contents that way.
//
// When the application reopens th folder, the application can pass a pointer
// to the snapshot object to mail::folder::open().
//
// getSnapshotInfo() should return the snapshot ID, and the number of messages
// in the folder.  Eventually, restoreSnapshot() will be invoked.
// restoreSnapshot() receives a reference to a restore object,
// restoreSnapshot() should repeatedly invoke restore->restoreIndex() to
// populate the index.  restoreIndex() takes a message index number, and a
// messageInfo object.  The index can be restored in any order, but all index
// entries must be restored.
// Additionally, if any messages had any keywords set, restoreKeywords()
// must be invoked, in any order, to restore the list of keywords set for
// the given message.
//
// Alternatively, abortRestore() may be invoked if the application cannot
// restore the index fully, for some reason.

class snapshot {

public:
	class restore {
	public:
		restore();
		virtual ~restore();

		virtual void restoreIndex(size_t msgNum,
					  const mail::messageInfo &)=0;
		virtual void restoreKeywords(size_t msgNum,
					     const std::set<std::string> &)=0;
		virtual void abortRestore()=0;
	};

	snapshot();
	virtual ~snapshot();

	virtual void getSnapshotInfo(std::string &snapshotId,
				     size_t &nMessages)=0;

	virtual void restoreSnapshot(restore &)=0;
};

LIBMAIL_END

#endif

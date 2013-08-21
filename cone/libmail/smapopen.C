/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#include "smap.H"
#include "smapopen.H"
#include "smapnewmail.H"
#include "snapshot.H"
#include "imapfolder.H"
#include <string.h>
#include <errno.h>
#include <sstream>

using namespace std;

/////////////////////////////////////////////////////////////////////////
//
// Helper for restoring snapshots


class mail::smapOPEN::SnapshotRestoreHelper : public mail::snapshot::restore {

public:
	mail::imap &myimap;
	mail::imapFOLDERinfo *myFolderInfo;
	bool aborted;

	SnapshotRestoreHelper(mail::imapFOLDERinfo *myFolderInfoArg,
			      mail::imap &myimapArg);
	~SnapshotRestoreHelper();
	void restoreIndex(size_t msgNum,
			  const mail::messageInfo &info);
	void restoreKeywords(size_t msgNum,
			     const std::set<std::string> &set);
	void abortRestore();
};

mail::smapOPEN::SnapshotRestoreHelper::
SnapshotRestoreHelper(mail::imapFOLDERinfo *myFolderInfoArg,
		      mail::imap &myimapArg)
	: myimap(myimapArg), myFolderInfo(myFolderInfoArg), aborted(false)
{
}

mail::smapOPEN::SnapshotRestoreHelper::
~SnapshotRestoreHelper()
{
}

void mail::smapOPEN::SnapshotRestoreHelper::
restoreIndex(size_t msgNum,
	     const mail::messageInfo &info)
{
	if (msgNum < myFolderInfo->index.size())
		((mail::messageInfo &)myFolderInfo->index[msgNum])=info;
}

void mail::smapOPEN::SnapshotRestoreHelper::
restoreKeywords(size_t msgNum,
		const set<string> &kset)
{
	if (msgNum < myFolderInfo->index.size())
	{
		myFolderInfo->index[msgNum].keywords
			.setFlags(myimap.keywordHashtable,
				  kset);
	}
}

void mail::smapOPEN::SnapshotRestoreHelper::abortRestore()
{
	aborted=true;
}

/////////////////////////////////////////////////////////////////////////

const char *mail::smapOPEN::getName()
{
	return "OPEN";
}

mail::smapOPEN::smapOPEN(string pathArg,
			 mail::snapshot *restoreSnapshotArg,
			 mail::callback &openCallbackArg,
			 mail::callback::folder &folderCallbackArg)
	: path(pathArg), restoreSnapshot(restoreSnapshotArg),
	  folderCallback(folderCallbackArg), newSmapFolder(NULL), exists(0),
	  restoringSnapshot(false)
{
	defaultCB= &openCallbackArg;
}

mail::smapOPEN::~smapOPEN()
{
	if (newSmapFolder)
		delete newSmapFolder;
}

void mail::smapOPEN::installed(imap &imapAccount)
{
	newSmapFolder=new smapFOLDER(path, folderCallback, imapAccount);

	if (!newSmapFolder)
		LIBMAIL_THROW((strerror(errno)));

	vector<string> words;

	path2words(path, words);

	vector<string>::iterator b=words.begin(), e=words.end();

	string pstr="";

	while (b != e)
	{
		pstr += " ";
		pstr += imapAccount.quoteSMAP( *b );
		b++;
	}

	if (imapAccount.currentFolder)
		imapAccount.currentFolder->closeInProgress=true;

	string snapshotId;

	if (restoreSnapshot)
	{
		size_t nMessages;

		restoreSnapshot->getSnapshotInfo(snapshotId, nMessages);

		if (snapshotId.size() > 0)
		{
			newSmapFolder->index.resize(nMessages);

			SnapshotRestoreHelper myHelper(newSmapFolder,
						       imapAccount);
			restoreSnapshot->restoreSnapshot(myHelper);

			if (myHelper.aborted)
				snapshotId="";
		}

		vector<imapFOLDERinfo::indexInfo>::iterator
			b=newSmapFolder->index.begin(),
			e=newSmapFolder->index.end();

		while (b != e)
		{
			if (b->uid.size() == 0)
			{
				snapshotId=""; // Bad restore.
				break;
			}
			b++;
		}

		if (snapshotId.size() == 0)
			newSmapFolder->index.clear();

		imapAccount.imapcmd("", "SOPEN "
				    + mail::imap::quoteSMAP(snapshotId)
				    + " " + pstr + "\n");
	}
	else
		imapAccount.imapcmd("", "OPEN" + pstr + "\n");
}

bool mail::smapOPEN::processLine(imap &imapAccount,
				 std::vector<const char *> &words)
{
	if (words.size() >= 3 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "SNAPSHOTEXISTS") == 0)
	{
		restoringSnapshot=true;
		presnapshotExists=exists=newSmapFolder->index.size();
		return true;
	}

	if (words.size() >= 3 && strcmp(words[0], "*") == 0 &&
	    strcasecmp(words[1], "EXISTS") == 0)
	{
		string n(words[2]);
		istringstream i(n);
		i >> exists;

		if (restoringSnapshot && exists > newSmapFolder->index.size())
			newSmapFolder->index.resize(exists);
		return true;
	}

	if (restoringSnapshot)
	{
		imapFOLDERinfo *saveCurrentFolder=
			imapAccount.currentFolder;

		bool rc;

		imapAccount.currentFolder=newSmapFolder;

		try {
			rc=smapHandler::processLine(imapAccount, words);
		} catch (...) {
			imapAccount.currentFolder=saveCurrentFolder;
			throw;
		}
		imapAccount.currentFolder=saveCurrentFolder;
		return rc;
	}

	return smapHandler::processLine(imapAccount, words);
}

void mail::smapOPEN::messagesRemoved(vector< pair<size_t, size_t> >
				     &removedList)
{
	// Restoring a SNAPSHOT

	vector< pair<size_t, size_t> >::iterator
		b=removedList.begin(),
		e=removedList.end();

	while (b != e)
	{
		--e;

		size_t i=e->first;
		size_t j=e->second+1;

		if (exists >= j)
			exists -= j - i;
		else if (exists > i)
			exists=i;

		if (restoringSnapshot)
		{
			if (presnapshotExists >= j)
				presnapshotExists -= j - i;
			else if (presnapshotExists > i)
				presnapshotExists=i;
		}
	}
}

void mail::smapOPEN::messageChanged(size_t msgNum)
{
}

bool mail::smapOPEN::ok(std::string s)
{
	smapFOLDER *f=newSmapFolder;

	newSmapFolder=NULL;

	myimap->currentFolder=f;
	myimap->installBackgroundTask(f);

	if (!restoringSnapshot)
	{
		f->exists=0;
		f->index.clear();
	}
	else
	{
		f->exists=presnapshotExists;
	}

	if (exists > (restoringSnapshot ? presnapshotExists:0))
	{
		f->index.resize(exists);
		myimap->installForegroundTask(new smapNEWMAIL(defaultCB,
							      true));
		defaultCB=NULL;
	}

	return smapHandler::ok(s);
}


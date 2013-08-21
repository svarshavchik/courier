/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef specialfolder_h
#define specialfolder_h

#include "config.h"

#include <string>
#include <map>
#include <vector>

namespace mail {
	class folder;
}

///////////////////////////////////////////////////////////////////////
//
// Pre-defined folders, such as Drafts or Outbox.

class SpecialFolder {

	class DummyCallback;

public:
	std::string defaultNameUTF8;  // Default folder name

	// There may be more than one special folder (more than one Outbox,
	// for example).

	class Info {
	public:
		std::string serverUrl;	// Folder's account
		std::string serverPath; // Folder's path (toString() actually).
		std::string nameUTF8;	// Folder's user-defined name
		std::string lastArchivedMonth; // For outboxes.

		// Search accounts for this folder's account, and instantiate
		// a mail::folder object.  The actual folder is created, if
		// necessary.

		mail::folder *getFolder(class myServer *&serverPtr);

		// Server account has been edited, here's a new url.

		void doUpdateAccount(std::string oldUrl, std::string newUrl);
	};

	// All the Outboxes.
	std::vector< Info > folderList;

	void addFolder(std::string nameUTF8Arg, std::string url,
		       std::string path);

	Info *findFolder(std::string nameUTF8Arg);

	// There's only one draft folder:
	void setSingleFolder(std::string url, std::string path);
	bool getSingleFolder(std::string &url, std::string &path);
	mail::folder *getFolder(class myServer *&serverPtr);

	SpecialFolder(std::string defaultNameUTF8);
	~SpecialFolder();

	static std::map<std::string, SpecialFolder> folders;

	static void updateAccount(std::string oldUrl, std::string newUrl);

	static void deleteAccount(std::string url);

	void doUpdateAccount(std::string oldUrl, std::string newUrl);

	void doDeleteAccount(std::string url);

};

#define DRAFTS	"DRAFTS"
#define SENT	"SENT"

#endif

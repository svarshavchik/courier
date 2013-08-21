/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef myfolderfilter_H
#define myfolderfilter_H

#include "config.h"
#include "myfolder.H"
#include "myservercallback.H"
#include "myreadfolders.H"
#include "filter.H"
#include "searchcallback.H"

#include <string>
#include <vector>
#include <set>

// A Filter object is created whenever there's a mail filter defined
// for the folder.  This occurs at the time the folder is opened,
// and whenever new mail arrives in this folder.  A pointer to
// the Filter object is saved here, together with the message range
// that's being filtered (which is automatically updated by
// messagesRemoved, if necessary).
//
// The Filter object maintains a back pointer to myFolder.
//
// If myFolder is destroyed, the Filter object is NOT destroyed, but
// the back pointer is NULLed out (because the Filter object may have
// outstanding callback we can't do that).

class myFolder::FolderFilter : private myServer::Callback {

	SearchCallbackHandler sc;

public:
	myFolder *folder;

	size_t filterStart;
	size_t filterEnd;

	std::set<std::string> currentSelection; // Currently selected msgs

	std::vector< Filter::Step > steps;

	std::vector< Filter::Step >::iterator currentStep;

	FolderFilter(myFolder *folderArg, size_t filterStartArg,
		     size_t filterEndArg);
	~FolderFilter();

	// When the folder is opened, run() goes through all the
	// steps.

	void run();

	// When new mail arrives, start() begins filtering,
	// asynchronously.

	void start();

private:

	// Do current filtering step
	void doStep(myServer::Callback &cb);
	void foundFolderFunc(std::string);
	void setNewTags(std::string);
	void doStep2(myServer::Callback &cb,
		     mail::folder *destFolder);

	void getMessageNums(std::vector<size_t> &);

	// Catch filter step completions.

	void success(std::string message);
	void fail(std::string message);


	myReadFolders foundFolder;
	myServer::Callback *origCallback;
	mail::folder *currentFolder;

	void (myFolder::FolderFilter::*success_func)(std::string);

	void searchCompleted(std::string message);

	void stepCompleted(std::string message);
};

#endif

/*
** Copyright 2003, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef mysearchcallback_H
#define mysearchcallback_H

#include "myserver.H"
#include "libmail/mail.H"
#include "libmail/search.H"

#include <vector>

/////////////////////////////////////////////////////////////////
//
// Capture message numbers from mail::searchCallback;

class SearchCallbackHandler : public mail::searchCallback {

	myServer::Callback &callback;

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,

			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);

public:
	std::vector<size_t> messageNumbers;

	SearchCallbackHandler(myServer::Callback &callbackArg);
	~SearchCallbackHandler();

	void success(const std::vector<size_t> &found);
	void fail(std::string);
};

#endif

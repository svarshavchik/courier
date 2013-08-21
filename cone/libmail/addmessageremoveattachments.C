/*
** Copyright 2004-2008, Double Precision Inc.
**
** See COPYING for distribution information.
*/
#include "libmail_config.h"
#include "addmessage.H"
#include "attachments.H"
#include "headers.H"
#include <errno.h>
#include <vector>
#include <cstring>

using namespace std;

class mail::addMessage::assembleRemoveAttachmentsHelper
	: public mail::callback::message {

public:
	void doNextCopy(std::string message);


private:
	// Imported from mail::callback:
	void success(std::string message);
	void fail(std::string message);

	void reportProgress(size_t bytesCompleted,
			    size_t bytesEstimatedTotal,
			    size_t messagesCompleted,
			    size_t messagesEstimatedTotal);
	void messageTextCallback(size_t n, std::string text);

	size_t &handleRet;
	mail::ptr<mail::account> acct;
	string msgUid;
	size_t msgNum;

	mail::addMessage *add;
	mail::callback &origCallback;

	void (assembleRemoveAttachmentsHelper::*success_func)(std::string);
	void doMultiPart(std::string message);

	string collectedHeaders;

public:
	assembleRemoveAttachmentsHelper(size_t &handleRetArg,
					mail::account *acctArg,
					string msgUidArg,
					mail::addMessage *addArg,
					mail::callback &origCallbackArg)
		: handleRet(handleRetArg),
		  acct(acctArg),
		  msgUid(msgUidArg),
		  msgNum(0),
		  add(addArg),
		  origCallback(origCallbackArg)
	{
	}

	~assembleRemoveAttachmentsHelper() {}


	// We get a message's MIME structure, and the MIME ids of attachments
	// to remove.  Create a tree structure for each MIME section with
	// two data points:
	//
	// A) Whether or not either this MIME section should be copied, or
	//    at least one of the MIME subsections should be copied
	//
	// B) Whether or not either this MIME section should not be copied, or
	//    at least one of the MIME subsections should not be copied
	//
	class mimeAction;

	class mimeAction {
	public:
		const mail::mimestruct &section;

		bool somethingCopied;
		bool somethingNotCopied;

		//
		// The list of MIME actions is built into a list
		// (so we don't need to manually allocate these objects),
		// and the MIME subsections are saved in the following
		// vector of iterators to the subsection of this section

		vector< list<mimeAction>::iterator > subsections;
		size_t handleId;

		mimeAction(const mail::mimestruct &sectionArg, bool a, bool b)
			: section(sectionArg), somethingCopied(a),
			  somethingNotCopied(b) {}

		~mimeAction() {}
	};

	list<mimeAction> parseTree;

	list<mimeAction>::iterator
	computeToCopy(const mail::mimestruct &section,
		      const set<string> &whatNotToCopy); // Compute parseTree

	//
	// Now, iterate over the parse tree, and create the execution plan
	// of things to copy.
	//

	class plan {
	public:
		bool multiPartCopy;
		list<mimeAction>::iterator whichPart;

		plan(bool multipartCopyArg,
		     list<mimeAction>::iterator whichPartArg)
			: multiPartCopy(multipartCopyArg),
			  whichPart(whichPartArg)
		{
		}

		~plan() {}
	};


	vector< plan > copyPlan;
	vector< plan >::iterator ToDo;

	void computePlan( list<mimeAction>::iterator );
};

#define MIMEACTION mail::addMessage::assembleRemoveAttachmentsHelper::mimeAction

list<MIMEACTION>::iterator
mail::addMessage::assembleRemoveAttachmentsHelper
::computeToCopy(const mail::mimestruct &section,
		const set<string> &whatNotToCopy)
{
	list<MIMEACTION>::iterator p;

	if (whatNotToCopy.count(section.mime_id) > 0)
	{
		parseTree.push_back(MIMEACTION(section, false, true));
		p=parseTree.end();
		return --p;
	}

	size_t n=section.getNumChildren();

	if (n == 0)
	{
		// Copy this one, by default

		parseTree.push_back(MIMEACTION(section, true, false));
		p=parseTree.end();
		return --p;
	}

	vector< list<MIMEACTION>::iterator > subsections;

	size_t i;

	bool somethingCopied=false;
	bool somethingNotCopied=false;

	for (i=0; i<n; i++)
	{
		p=computeToCopy(*section.getChild(i), whatNotToCopy);

		if (p->somethingNotCopied)
			somethingNotCopied=true;

		if (!p->somethingCopied)
			continue;

		somethingCopied=true;
		subsections.push_back(p);
	}

	parseTree.push_back(MIMEACTION(section, somethingCopied,
				       somethingNotCopied));

	p=parseTree.end();
	--p;

	p->subsections=subsections;
	return p;
}


void mail::addMessage::assembleRemoveAttachmentsHelper
::computePlan(list<MIMEACTION>::iterator nodeArg)
{
	if (!nodeArg->somethingCopied)
		return; // Skip this entire MIME section

	if (!nodeArg->somethingNotCopied)
	{
		copyPlan.push_back(plan(false, nodeArg));
		// Copy this entire section, multipart or not
		return;
	}

	size_t i, n= nodeArg->subsections.size();

	for (i=0; i<n; i++)
		computePlan(nodeArg->subsections[i]);

	copyPlan.push_back(plan(true, nodeArg));
}

void mail::addMessage
::assembleRemoveAttachmentsFrom(size_t &handleRetArg,
				mail::account *acctArg,
				string msgUidArg,
				const mail::mimestruct &msgStructArg,
				const set<string> &removeUidListArg,
				mail::callback &cbArg)
{
	assembleRemoveAttachmentsHelper *helper=
		new assembleRemoveAttachmentsHelper(handleRetArg,
						    acctArg,
						    msgUidArg,
						    this,
						    cbArg);

	if (!helper)
	{
		cbArg.fail(strerror(errno));
		return;
	}

	try {
		list<MIMEACTION>::iterator topNode;

		topNode=helper->computeToCopy(msgStructArg, removeUidListArg);

		if (!topNode->somethingCopied)
		{
			delete helper;
			helper=NULL;
			cbArg.fail("Cannot remove every attachment.");
			return;
		}

		if (!topNode->somethingNotCopied)
		{
			delete helper;
			helper=NULL;
			cbArg.fail("No attachments selected for deletion");
			return;
		}
		helper->computePlan(topNode);
		helper->ToDo=helper->copyPlan.begin();
		helper->doNextCopy("Ok."); // Kick into action.
	} catch (...)
	{
		if (helper)
			delete helper;
		cbArg.fail(strerror(errno));
	}
}

void mail::addMessage
::assembleRemoveAttachmentsHelper::success(std::string message)
{
	(this->*success_func)(message);
}

void mail::addMessage
::assembleRemoveAttachmentsHelper::doNextCopy(std::string message)
{
	origCallback.reportProgress(0, 0, ToDo-copyPlan.begin()+1,
				    copyPlan.size());

	if (ToDo == copyPlan.end())
	{
		try {
			--ToDo;
			handleRet=ToDo->whichPart->handleId;

			origCallback.success(message);
		} catch (...)
		{
			delete this;
			throw;
		}
		delete this;
		return;
	}

	if (acct.isDestroyed())
	{
		fail("Server connection terminated.");
		return;
	}

	if (!chkMsgNum(acct, msgUid, msgNum))
	{
		fail("Message deleted in the middle of being copied.");
		return;
	}

	plan &toDoNext= *ToDo;
	mimeAction &toDoInfo= *toDoNext.whichPart;

	if (toDoNext.multiPartCopy)
	{
		collectedHeaders="";
		success_func= &assembleRemoveAttachmentsHelper::doMultiPart;

		acct->readMessageContent(msgNum, true,
					 toDoInfo.section,
					 readHeaders, *this);
		return;
	}

	++ToDo;
	success_func= &assembleRemoveAttachmentsHelper::doNextCopy;

	add->assembleImportAttachment(toDoInfo.handleId, acct, msgUid,
				      toDoInfo.section,
				      *this);
}

void mail::addMessage
::assembleRemoveAttachmentsHelper::doMultiPart(std::string message)
{
	if (acct.isDestroyed())
	{
		fail("Server connection terminated.");
		return;
	}

	plan &toDoNext= *ToDo;
	mimeAction &toDoInfo= *toDoNext.whichPart;

	vector<size_t> parts;

	vector< list<mimeAction>::iterator >::iterator b, e;

	for (b=toDoInfo.subsections.begin(),
		     e=toDoInfo.subsections.end(); b != e; ++b)
		parts.push_back( (*b)->handleId );

	string::iterator hstart, hend;

	string encoding=
		mail::Attachment::find_header("CONTENT-TRANSFER-ENCODING:",
					      collectedHeaders, hstart, hend);

	if (hstart != hend)
		collectedHeaders.erase(hstart, hend);

	string content_type_header=
		mail::Attachment::find_header("CONTENT-TYPE:",
					      collectedHeaders, hstart, hend);

	mail::Header::mime ct=
		mail::Header::mime::fromString(string(hstart, hend));
	if (hstart != hend)
		collectedHeaders.erase(hstart, hend);

	++ToDo;
	success_func= &assembleRemoveAttachmentsHelper::doNextCopy;

	add->assembleMultipart(toDoInfo.handleId, collectedHeaders,
			       parts,
			       ct.value,
			       ct.parameters,
			       *this);
}

void mail::addMessage
::assembleRemoveAttachmentsHelper::fail(std::string message)
{
	try {
		origCallback.fail(strerror(errno));
	} catch (...)
	{
		delete this;
		throw;
	}
	delete this;
}

void mail::addMessage
::assembleRemoveAttachmentsHelper::reportProgress(size_t bytesCompleted,
						  size_t bytesEstimatedTotal,
						  size_t messagesCompleted,
						  size_t messagesEstimatedTotal)
{
	origCallback.reportProgress(bytesCompleted, bytesEstimatedTotal,
				    ToDo-copyPlan.begin(),
				    copyPlan.size());
}

void mail::addMessage
::assembleRemoveAttachmentsHelper::messageTextCallback(size_t n,
						       std::string text)
{
	collectedHeaders += text;
}

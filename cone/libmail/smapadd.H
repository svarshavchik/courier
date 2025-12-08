/*
** Copyright 2003, S. Varshavchik.
**
** See COPYING for distribution information.
*/
#ifndef libmail_smapadd_H
#define libmail_smapadd_H

#include "libmail_config.h"
#include	"mail.H"
#include	"smap.H"

#include	<stdio.h>

#include	<vector>

LIBMAIL_START

class smapAdd : public smapHandler {

	const char *getName() override;

	std::vector<std::string> cmds;

	bool (smapAdd::*okfunc)(std::string);
	bool (smapAdd::*failfunc)(std::string);

	std::string finalmsg;

	unsigned long tfilebytecount;

	bool processLine(imap &imapAccount,
			 std::vector<const char *> &words) override;

	class WriteTmpFile : public mail::fd::WriteBuffer {
	public:
		smapAdd *myAdd;
		FILE *tfile;
		unsigned long tfilebytecount;

		unsigned long origbytecount;

		WriteTmpFile(smapAdd *myAddArg);
		~WriteTmpFile();

		bool fillWriteBuffer() override;
	};

	bool ready2send;

public:
	FILE *tfile;
	smapAdd(std::vector<std::string> cmdsArg,
		mail::callback &callback);
	~smapAdd();

	void installed(imap &) override;
	bool ok(std::string) override;
	bool fail(std::string message) override;

private:

	bool sendNextCmd(std::string);
	bool failCmd(std::string);

	bool doneok(std::string);
	bool donefail(std::string message);
};

LIBMAIL_END

#endif

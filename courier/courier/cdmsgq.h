/*
** Copyright 1998 - 2007 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	cdmsgq_h
#define	cdmsgq_h

#if	HAVE_CONFIG_H
#undef	PACKAGE
#undef	VERSION
#include	"config.h"
#endif

#include	<sys/types.h>
#include	<time.h>

#include	"cdrcptinfo.h"

#include	<string>
#include	<vector>

class drvinfo;
struct rfc822token;
struct comctlfile;

class msgq {
public:
    int cancelled;		// This message should be cancelled
    msgq *next, *prev;		// Sorted by nextdel
    msgq *nexthash, *prevhash;	// Same hash bucket
    ino_t        msgnum;	// Queue message number

    std::string  msgid;         // Message ID, for logging

    time_t       nextdel;	// Next delivery attempt (used to find the
				// filename in msgq)
    time_t       nextsenddel;	// Usually nextdel, but is reset when
				// the queue is flushed -- this is the time
				// when this message should be scheduled
				// for delivery.
    unsigned long nksize;       // Size in kilobytes
    std::vector<rcptinfo> rcptinfo_list;  	// The recipients
    unsigned rcptcount;		// # of incomplete deliveries for this msg
    msgq();
    ~msgq();

    static std::vector<msgq> queue;
    static std::vector<msgq *> queuehashfirst, queuehashlast;

static unsigned queuedelivering, queuewaiting, inprogress;

static void init(unsigned);
static void logmsgid(msgq *q);
static msgq *queuehead, *queuetail,
			// Message with a next-delivery time stamp
			// IN THE FUTURE
	*queuefree;
			// Unused msgqs

static msgq *findq(ino_t);

static int tmpscan();
static void queuescan();

static void flushmsg(ino_t, const char *);

private:

static drvinfo *backup_relay_driver;
static std::string backup_relay;

static int queuescan2(std::string);
static int queuescan3(std::string, std::string, const char *);

static drvinfo *getdelinfo(struct rfc822token *,
	const char *, std::string &, std::string &, std::string &);

public:
	void removeq();
	void removewq();
	void start_message();

static	void completed(drvinfo &, size_t);

private:
static void startdelivery(drvinfo *, delinfo *);

static void done(msgq *, int);

	const char *needs_dsn(struct ctlfile *);
	const char *needs_warndsn(struct ctlfile *);

} ;

#endif

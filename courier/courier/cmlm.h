#ifndef	cmlm_h
#define	cmlm_h
/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/


#include	"config.h"
#include	"afx/afx.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<stdio.h>
#include	<stdlib.h>

#include	<iostream>

#include	<string>
#include	<vector>
#include	<algorithm>
#include	<functional>
#include	<cctype>

#define	TMP	"tmp"
#define	TMPLOCK	"tmp.lock"

#define	OPTIONS	"options"
#define	DOMAINS	"sublist"
#define	ARCHIVE	"archive"
#define	UNSUBLIST "unsublist"
#define	COMMANDS "commands"
#define	COMMANDSDAT "commands.dat"

#define	BOUNCES	"bounces"
#define	BOUNCESDAT	"bounces.dat"
#define	BOUNCESLOCK	"bounces.lock"


#define	HOURLYLOCKFILE	"hourly.lock"
#define	DAILYLOCKFILE	"daily.lock"

#define	DIGESTLOCKFILE	"digest.lock"

#define	MODQUEUE "modqueue"
#define	MODQUEUELOCKFILE "modqueue.lock"

#define	HEADERADD	"headeradd"
#define	HEADERDEL	"headerdel"

#include	"datadir.h"

#define	TEMPLATEDIR	DATADIR "/couriermlm"

#define	MSGSEPARATOR	"===="

#include	"bindir.h"

#define	SENDMAIL	BINDIR "/sendmail"
#define	REFORMIME	BINDIR "/reformime"
#define	MAXRCPTS	20

#define	MSGLOCKFILE	ARCHIVE "/.lock"
#define	SEQNO	ARCHIVE "/.seqno"
#define	NEXTSEQNO	ARCHIVE "/.nextseqno"

//
//	Large domains have their own db file listing the subscribers.
//	Small domains are all lumped into one file, keyed by domain name,
//	data is address\0subinfo\0address\0subinfo...
//

#define	MISC	DOMAINS "/misc"
#define	MISCSIZE	2

#define	ALIASES	DOMAINS "/aliases"

#define	SUBLOCKFILE "sublist.lock"
#define	CMDLOCKFILE "commands.lock"

#define SUBLOGFILE "sublist.log"

class ExclusiveLock {
	int fd;
public:
	ExclusiveLock(const char *);
	~ExclusiveLock();
} ;

class SharedLock {
	int fd;
public:
	SharedLock(const char *);
	~SharedLock();
} ;

class SubSharedLock : public SharedLock {
public:
	SubSharedLock();
	~SubSharedLock();
} ;

class SubExclusiveLock : public ExclusiveLock {
public:
	SubExclusiveLock();
	~SubExclusiveLock();
} ;

class CommandLock : public ExclusiveLock {
public:
	CommandLock();
	~CommandLock();
} ;

void trapsigs(const char *);
void clearsigs(int);

std::string cmdget_s(const char *);

template<typename iterator_t, typename comp_t>
	iterator_t find_last_if(iterator_t b, iterator_t e,
				comp_t c)
{
	iterator_t r=e;

	while (b != e)
		if (c(*b++))
			r=b;
	return r;
}


inline void TrimLeft(std::string &s)
{
	s=std::string(std::find_if(s.begin(), s.end(),
				   std::not1(std::ptr_fun(::isspace))), s.end());
}

inline void TrimRight(std::string &s)
{
	s=std::string(s.begin(), find_last_if(s.begin(), s.end(),
					 std::not1(std::ptr_fun(::isspace)))
		      );
}

std::string get_verp_return(std::string addr);

std::string toverp(std::string);

std::string fromverp(std::string);

std::string readmsg();

std::string header_s(std::string a, const char *b);

std::string returnaddr(std::string, const char *);

std::string mkboundary_msg_s(afxipipestream &);

std::string mkfilename();
std::string mktmpfilename();
void post(std::istream &, const char *);

int is_subscriber(std::string);
int getinfo(std::string, int (*)(std::string));
std::string myname();

void addrlower(char *);
void addrlower(std::string &); // TODO

int isfound(std::string);
int sendmail(const char **, pid_t &);
int wait4sendmail(pid_t);
int sendmail_bcc(pid_t &, std::string, int nodsn = 0);

bool checkconfirm(std::string);

void ack_template(std::ostream &, const char *, std::string, std::string);
void simple_template(std::ostream &, const char *, std::string);
void copy_template(std::ostream &, const char *, std::string);
void copy_report(const char *, afxopipestream &);
int copyio(afxipipestream &, afxopipestream &);
int copyio_noseek(afxipipestream &, afxopipestream &);
int copyio_noseek_cnt(afxipipestream &, afxopipestream &, unsigned long *);
int cmddigest(const std::vector<std::string> &);

#endif

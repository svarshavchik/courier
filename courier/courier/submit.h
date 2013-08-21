/*
** Copyright 1998 - 2001 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	submit_h
#define	submit_h

#include	"afx/afx.h"
#include	"dbobj.h"
#include	"courier.h"
#if	HAVE_SYS_STAT_H
#include	<sys/stat.h>
#endif
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<fstream>
#include	<time.h>

#include	<list>
#include	<string>
#include	<set>

/////////////////////////////////////////////////////////////////////////////
//
// The SubmitFile class creates one or more control files in
// ${localstatedir}/tmp.
//
// The drudge work in submit.C calculates the actual sender/recipients,
// including aliases.dat lookups, and we get a raw list of addresses
//
// The SubmitFile class is implemented in submit2.C
//
// void Sender(const char *sender, const char *envid, char dsnformat)
//		- specifies envelope sender 'envid', and dsn format 'dsnformat'
// int ChkRecipient(const char *) - returns non-zero if this is a duplicate
// 				recipient, zero if not.
//
// void AddReceipient(const char *recipient, const char *orecipient,
//		const char *dsn, int isalias)
//                                - records recipient address 'recipient',
//				  which is original recipient 'orecipient',
//                                with delivery status notifications set to
//				  'dsn'.  'isalias' is non zero if this is
//				  a local alias, in which case the recipient
//				  is immediately marked as being received.
// void ReceipientFilter(struct rw_transport *module,
//		const char *host,
//		const char *address);
//				- After the message is received, it can be
//				optionally spam filtered.  Record recipient
//				information now, and then run each individual
//				recipient address's filter later.
//				ReceipientFilter supplies the recipient info
//				for the last address called to AddReceipient.
//				Addresses resulting from alias expansion are
//				not filtered.
//
// void interrupt() - should be called if process receives an interrupt.
//                    interrupt() deletes any temporary files that were created.
//
/////////////////////////////////////////////////////////////////////////////

struct	rfc2045;
struct	rw_transport;

class RcptFilterInfo {
public:
	unsigned num_control_file;
	unsigned num_receipient;

	struct rw_transport *driver;
	std::string host;
	std::string address;
	unsigned rcptnum;
} ;

class SubmitFile {
private:

static	SubmitFile *current_submit_file;

	unsigned num_control_files_created;

	std::string name1stctlfile();
	std::string namefile(const char *, unsigned n);

 public:
	std::set<std::string> all_files;
 private:
	unsigned	rcptcount;
	afxopipestream	ctlfile;
	afxiopipestream	datfile;
	ino_t		ctlinodenum;
	ino_t		ctlpid;
	time_t		ctltimestamp;
	std::string	basemsgid;

	struct		rfc2045 *rwrfcptr;
	const char 	*frommta;
	unsigned long	bytecount;
	unsigned long	sizelimit;
	int		diskfull;
	unsigned	diskspacecheck;
	std::string	sender, envid, dsnformat;
	std::string	receipient;
	std::set<std::string> addrlist_map;
	DbObj					addrlist_gdbm;

	std::list<RcptFilterInfo> rcptfilterlist;
	std::fstream	rcptfilterlist_file;

	const char *sending_module;

	void	openctl();
	void	closectl();
public:
	SubmitFile();
	~SubmitFile();

	std::string QueueID();
	void SendingModule(const char *p) {sending_module=p;}
	void Sender(const char *, const char *, const char *, char);
	int ChkRecipient(const char *);
	void AddReceipient(const char *, const char *, const char *, int);

	void ReceipientFilter(struct rw_transport *,
			const char *,
			const char *,
			unsigned);

	void MessageStart();
	void Message(const char *);
	int MessageEnd(unsigned, int, int);

	static std::string get_msgid_for_filtering(unsigned, void *);

static	void interrupt();
static	void trapsignals();


private:
	int datafilter(const char *, unsigned, const char *);

	void do_datafilter( unsigned &, int &, int,
			    struct rw_transport *, std::string, std::string, unsigned,

			    unsigned, unsigned, const char *, unsigned nrcpts);
} ;

#endif

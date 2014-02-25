/*
** Copyright 1998 - 2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	comctlfile_h
#define	comctlfile_h

#include	"courier.h"
#include	<sys/types.h>
#include	<time.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct iovec;

/*
	The following structure represents the control file of a message.
*/

struct ctlfile {
	int fd;				/* File descriptor */
	ino_t n;			/* Inode/Queue number */
	time_t mtime;			/* Last attempted delivery */
	time_t starttime;		/* The timestamp portion of the
					** message ID.  **NOT** initialized
					** by ctlfile_open, it sets it to 0
					** and caller must stat the data
					** file and fill this in */

	unsigned long msgsize;		/* Size of the message. NOT
					** automatically populated, it is
					** initialized to 0, and the
					** delivery code is responsible
					** for setting it when it open the
					** message data.
					** If > 0, the size is logged
					** upon succesful delivery.
					*/

	char *contents;			/* The entire control file */

	/* Parsed contents: */

	const char *sender;		/* Envelope sender */
	char **receipients;		/* Receipients */
	char **oreceipients;		/* Original receipients */
	char **dsnreceipients;		/* DSN flags */
	char **delstatus;		/* Per-receipient delivery status */
	unsigned nreceipients;		/* # of receipients */
	char **lines;			/* contents, null terminated lines */
	short	cancelled;		/* TRUE - message has been cancelled */
} ;

int ctlfile_openi(ino_t, struct ctlfile *, int);
int ctlfile_openit(ino_t, time_t, struct ctlfile *, int);
int ctlfile_openfn(const char *, struct ctlfile *, int, int);
void ctlfile_close(struct ctlfile *);
void ctlfile_append(struct ctlfile *, const char *);
int ctlfile_setvhost(struct ctlfile *);

void ctlfile_append_reply(struct ctlfile *, unsigned,
	const char *, char, const char *);
void ctlfile_append_replyfd(int, unsigned,
	const char *, char, const char *);

void ctlfile_append_info(struct ctlfile *, unsigned, const char *);
void ctlfile_append_infofd(int, unsigned, const char *);
void ctlfile_append_connectioninfo(struct ctlfile *,
					unsigned, char, const char *);
void ctlfile_append_connectioninfofd(int, unsigned, char, const char *);

int ctlfile_searchfirst(struct ctlfile *, char);
void ctlfile_appendv(struct ctlfile *, const struct iovec *, int);
void ctlfile_appendvfd(int, const struct iovec *, int);

const char *ctlfile_security(struct ctlfile *);

void ctlfile_nextattempt(struct ctlfile *, time_t);
void ctlfile_nextattemptfd(int, time_t);
time_t ctlfile_getnextattempt(struct ctlfile *);

#define	COMCTLFILE_SENDER	's'
#define	COMCTLFILE_RECEIPIENT	'r'
#define	COMCTLFILE_ORECEIPIENT	'R'
#define	COMCTLFILE_DSN		'N'
#define	COMCTLFILE_ENVID	'e'
#define	COMCTLFILE_AUTHNAME	'i'
#define	COMCTLFILE_DSNFORMAT	't'
#define	COMCTLFILE_EXPIRES	'E'
#define COMCTLFILE_FAXEXPIRES	'p'
#define	COMCTLFILE_WARNING	'W'
#define	COMCTLFILE_WARNINGSENT	'w'
#define	COMCTLFILE_8BIT		'8'
#define	COMCTLFILE_FROMMTA	'f'
#define	COMCTLFILE_DELINFO	'I'
	#define COMCTLFILE_DELINFO_PEER	'P'
	#define	COMCTLFILE_DELINFO_CONNECTIONERROR	'C'
	#define	COMCTLFILE_DELINFO_SENT			'S'
	#define	COMCTLFILE_DELINFO_REPLYTYPE		'T'
	#define	COMCTLFILE_DELINFO_REPLY		'R'
#define	COMCTLFILE_DELSUCCESS	'S'
#define	COMCTLFILE_DELFAIL	'F'
#define	COMCTLFILE_DELDEFERRED	'D'
#define	COMCTLFILE_DELCOMPLETE	'C'
#define	COMCTLFILE_VERP		'V'
#define COMCTLFILE_VHOST	'v'
#define	COMCTLFILE_CANCEL_MSG	'X'
#define	COMCTLFILE_CANCELLED_ACK 'x'
#define	COMCTLFILE_NEXTATTEMPT	'A'
#define	COMCTLFILE_MSGID	'M'
#define	COMCTLFILE_SUBMITDELAY	'L'
#define COMCTLFILE_SECURITY	'U'
#define COMCTLFILE_MSGSOURCE	'u'
#define COMCTLFILE_TRACK	'T'
#define COMCTLFILE_ENVVAR	'O'

/*
** HACK ALERT:  We want to log the last line of SMTP replies,
** showing the remote relay that we delivered to, to syslog, but not to the
** message log, because it is not part of the SMTP reply received (and it
** would show up in any DSNs as a noncompliant rfc822; format message).
** The following code is a special marker in this case, and causes special
** processing.  This flag is ONLY understood by ctlfile_append_reply().
*/

#define	COMCTLFILE_DELSUCCESS_NOLOG	'Z'


/*
** HACK ALERT, part II: A COMCTLFILE_DELFAIL_NOTRACK is a COMCTLFILE_DELFAIL,
** but the error is tracked for backscatter purposes.
*/

#define COMCTLFILE_DELFAIL_NOTRACK	'Y'


#define	SMTPREPLY_TYPE(s)	( *(s) == '5' ? COMCTLFILE_DELFAIL: \
	*(s) == '1' || *(s) == '2' || *(s) == '3' ? COMCTLFILE_DELSUCCESS:\
	COMCTLFILE_DELDEFERRED)

extern void clog_msg_msgid(struct ctlfile *);

#ifdef	__cplusplus
}
#endif

#endif

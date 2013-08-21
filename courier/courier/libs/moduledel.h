/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	moduledel_h
#define	moduledel_h

#if	HAVE_CONFIG_H
#include	"libs/courier_lib_config.h"
#endif

/*
** Some convenient functions used by modules to handle deliveries
** of messages.
**
** The functions implement receiving delivery dispatches from courierd,
** forking and running child processes to handle them, then notifying
** courierd when the deliveries has been completed.
**
** A parent process receives messages and forks child processes.
*/

#ifdef	__cplusplus
extern "C" {
#endif

#include	<sys/types.h>
#include	<time.h>

struct	moduledel {
	ino_t	inum;
	const char *msgid;
	const char *sender;
	const char *delid;
	const char *host;
	unsigned nreceipients;
	char **receipients;
	} ;

extern unsigned module_nchildren;
extern unsigned *module_delids;
extern pid_t *module_pids;

/*
** module_getdel reads a line from stdin, and returns a pointer to
** moduledel structure, ready for action.  All memory is internally
** allocated, and overwritten on each call.
**
** In most cases, that's all that's needed.  But, when things get tricky:
**
** The first argument to module_getline is a pointer to a function that
** returns the next character read from an arbitrary input stream.
** The second argument is a pointer that is supplied to as the sole
** argument to the function.  module_getline repeatedly calls the function
** until it returns a newline character, then returns the line read
** using an internally-allocated buffer. module_getline ignores
** EINTR errors.
**
** module_parsecols() takes a null-terminated line, and returns an
** array pointing to the start of each column in this delivery instruction.
** Note that each column is not null-terminated, only the last one.
** All but the last column is terminated by the tab character.
**
** module_parsedel takes the output of module_parsecols, and creates
** an internal moduledel structure out of it, this time actually replacing
** all tab characters with null bytes, and using an internally allocated
** array to store the pointers to each field.
*/

struct moduledel *module_getdel();

char *module_getline( int (*)(void *), void *);
char **module_parsecols(char *);
struct moduledel *module_parsedel(char **);

/* Some macros */

#define	MODULEDEL_MSGNUM(c)	((c)[0])
#define	MODULEDEL_MSGID(c)	((c)[1])
#define	MODULEDEL_SENDER(c)	((c)[2])
#define	MODULEDEL_DELID(c)	((c)[3])
#define	MODULEDEL_HOST(c)	((c)[4])

/*
** module_init is called to initialize the parent processes.  The number
** of child processes is calculated, and a signal handler is set up to
** handle terminated child processes.
**
** When a child process is terminated, module_completed will be called.
** Pass a non-null pointer to a function to have this function called
** instead of module_completed.
** The first argument will be the child process index # (0 up to n-1, where
** n is the maximum number of child processes).  The second argument will
** be the delivery ID that the process hath received.
*/

void module_init(void (*)(unsigned, unsigned));
void module_completed(unsigned, unsigned);

/* Set a watchdog timer for delivery attempts */

void module_delivery_timeout(time_t n);

/*
** module_fork calls fork(), and returns the exit value from fork().
** The first argument is the delivery ID, which will be passed as the
** second argument to the module_completed function.
** If the second argument is not null, it points to the slot number
** we want to track the child process as, otherwise we'll find the
** first unused one.
*/

pid_t module_fork(unsigned, unsigned *);

/* module_fork_noblock forks a child process without blocking SIGCHLD,
** Use that if we want to fork off a new child process while in a middle
** of the signal handler called due to previous child process termination.
*/

pid_t module_fork_noblock(unsigned, unsigned *);

/*
** Some complicated modules impose value-added stuff on top of child
** management.  They can child_blockset and child_blockclr to suspend
** child process termination handling in order to access per-process
** data in an atomic fashion.
*/
void module_blockset();
void module_blockclr();

/* Remove signal handler, restore to default */

void module_restore();

/*
** When the main courierd process dies, kill all pending child procs
*/

void module_signal(int);

#ifdef	__cplusplus
}
#endif

#endif

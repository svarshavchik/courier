#ifndef	libfilter_h
#define	libfilter_h

/*
** Copyright 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/


#ifdef  __cplusplus
extern "C" {
#endif

/*

Create and initialize the filesystem socket used to accept filtering requests.
Typically we have a configuration file (modfile), then specifies whether
everything or just the selected mail sources are filtered.

lf_init reads modfile, then creates either "allname", which should be a
complete pathname to $(allfiltersocketdir), or "notallname", which should be a
complete pathname to $(filtersocketdir).

alltmpname and notalltmpname are temporary names, in the corresponding
directories (beginning with a .), that are used to create the socket.

lf_init() returns a non-zero file descriptor, or -1 for error.

See dupfilter.c for example usage.

*/

int lf_init(const char *modfile,
		const char *allname,
		const char *alltmpname,
		const char *notallname,
		const char *notalltmpname);

/*
** Checks if this process is started by courierfilter. Returns true
** if file descriptor 3 exists. If it doesn't, the binary must have
** been started from the command line. This can be used to build
** filters that have some usefulness when they're invoked manually.
*/

int lf_initializing();

/*

After success in lf_init, the filter may do some additional initialization,
then call lf_init_completed(), passing the opened file descriptor from lf_init.

lf_init_completed closes the file descriptor #3, which is a signal to
filterctl that initialization has been completed.

*/

void lf_init_completed(int);

/*

lf_accept is used to accept a new connection to the filter.

If lf_accept returns <0, the system is shutting down.

*/

int lf_accept(int);

/*

You will now read the following from the file descriptor returned by lf_accept:

1) The pathname to the file containing the contents of the message to filter,
   followed by a newline.

2) One or more pathnames to files containing message control files.
   Each pathname is terminated by a newline

3) A newline character (empty pathname).

** YOU MUST read all of the above information, even if you do not need to read
   the contents of the control files.

After reading the last newline, you write one of the following messages to
the file descriptor:

200 Ok<NL>

500 Error message<NL>

Then close the descriptor.

*/

#ifdef  __cplusplus
}
#endif

#endif

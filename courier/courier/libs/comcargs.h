/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	comcargs_h
#define	comcargs_h

#ifdef __cplusplus
extern "C" {
#endif

struct courier_args {
	const char *argname;
	const char **argval;
	void (*argfunc)(const char *);
} ;

int cargs(int argc, char **argv, const struct courier_args *);

#ifdef	__cplusplus
}
#endif

#endif

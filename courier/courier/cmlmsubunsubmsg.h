#ifndef	cmlmsubunsubmsg_h
#define	cmlmsubunsubmsg_h
/*
** Copyright 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/


int dosub(const char *);
int domodsub(const char *);
int dounsub(const char *);
int doalias(const char *);

int dosubunsubconfirm(const char *,
		      const char *, int (*)(const char *, std::string),
		      const char *, const char *, int);

#endif

#ifndef cmlmsubunsub_h
#define cmlmsubunsub_h

/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/


#include	"config.h"
#include	"cmlm.h"
#include	"afx/afx.h"

#include	<vector>

int cmdsub(const std::vector<std::string> &);	// Manual subscribe
int cmdunsub(const std::vector<std::string> &);	// Manual unsubscribe

int docmdsub(const char *, std::string);
int docmdsub(const char *, std::string, bool addheader);
// Do the subscription action

int docmdunsub(const char *, std::string);	// Do the unsubscription action

int docmdunsub_bounce(std::string, std::string); // Unsub due to bounces
#endif

#ifndef	cmlmcmdmisc_h
#define	cmlmcmdmisc_h

/*
** Copyright 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/


#include	"config.h"
#include	"afx/afx.h"

#include <string>
#include <vector>

int cmdcreate(const std::vector<std::string> &);
int cmdupdate(const std::vector<std::string> &);
int cmdset(const std::vector<std::string> &);
int cmdset(const std::vector<std::string> &argv, bool autoencode);
int docmdalias(const char *, std::string);
int cmdlsub(const std::vector<std::string> &);

int cmdexport(const std::vector<std::string> &);
int cmdimport(const std::vector<std::string> &);
int cmdlaliases(const std::vector<std::string> &);
int cmdinfo(const std::vector<std::string> &);

#endif

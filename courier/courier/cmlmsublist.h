#ifndef cmlmsublist_h
#define cmlmsublist_h

/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/


#include	"config.h"
#include	"cmlm.h"
#include	"mydirent.h"
#include	"dbobj.h"
#include	"afx/afx.h"
#include	<stdio.h>
#include	<stdlib.h>

#include	<string>
#include	<list>

//
//  This class is used to read the subscriber database.
//

class SubscriberList : public SubSharedLock {

	DIR	*dirp;
	DbObj	domain_file;

	std::list<std::string> misclist;

	bool	(SubscriberList::*next_func)(std::string &);

	bool	domain( std::string &), shared( std::string &);
	bool	openalias( std::string &), nextalias( std::string &);
public:
	std::string	posting_alias;
	std::string	sub_info;

	SubscriberList( bool wantalias=false);
	bool Next(std::string &r) { return ( (this->*next_func)(r) ); }

	static void splitmisc(std::string s,
			      std::list<std::string> &misclist);

	static std::string joinmisc(const std::list<std::string> &misclist);

	~SubscriberList();
} ;

#endif

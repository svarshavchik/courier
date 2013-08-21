#ifndef	cmlmstartmail_h
#define	cmlmstartmail_h
/*
** Copyright 2000-2007 Double Precision, Inc.
** See COPYING for distribution information.
*/


#include	"config.h"
#include	<iostream>
#include	<vector>
#include	<string>

class StartMail {

	std::istream	&msg;
	std::string verp_ret;
	std::vector<std::string> recipients;
	size_t	n;

public:
	StartMail(std::istream &, std::string);
	~StartMail();
	void To(std::string);
	void Send();
} ;

#endif

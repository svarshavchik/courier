#ifndef	cmlmfilter_h
#define	cmlmfilter_h

#include	"config.h"


/*
** Copyright 2018 S. Varshavchik.
** See COPYING for distribution information.
*/

int rcptfilter_msg();
int smtpfilter_msg();
int rcptfilter_ctlmsg();
int smtpfilter_ctlmsg();

#endif

/*
** Copyright 1998 - 2004 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	comsubmitclient_h
#define comsubmitclient_h

#include	"courier.h"
#include	<stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern FILE *submit_to;
extern FILE *fromsubmit;

/* Start a submit child process. */

int submit_fork(
	char **,		/* argv */
	char **,		/* envp */
	void (*)(const char *)); /* Print function */
void submit_print_stdout(const char *);
		/* This is suitable for the print function arg */

/* Write a command to the submit process. */

void submit_write_message(
		const char *);	/* null terminated string, NOT \n terminated */

/*
	Read response from the submit process.

	submit_readrc -- just read it.
	submit_readrcprint -- read it and print the response
	submit_readrcpritncrlf - read it and print the response, lines 
	                         terminated by CRLF

	submit_readrcprinterr -- read it and print only error responses
*/

int submit_readrc();
int submit_readrcprint();
int submit_readrcprinterr();
int submit_readrcprintcrlf();

/* Log errors, with the following prefix: */

void submit_log_error_prefix( void (*)(void) );
		/* This function will generate the prefix. */

/* Wait for the submit process to terminate. */

int submit_wait();

/* Kills the submit process */

void submit_cancel();

void submit_cancel_async();	/* This can be called from a signal handler,
		just before we exit on a signal */


void submit_set_teergrube(void (*)(void));
	/* Set callback function on an error, used to implement teergrube */

#ifdef	__cplusplus
}
#endif

#endif

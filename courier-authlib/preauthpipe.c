/*
** Copyright 1998 - 2000 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if HAVE_CONFIG_H
#include "courier_auth_config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
//#if HAVE_UNISTD_H
//#include <unistd.h>
//#endif

#include	"authpipe.h"
#include	"courierauth.h"
#include	"courierauthdebug.h"

#include	"authpipelib.h"


int authpipecommon(const char *user, const char *pass,
        int (*callback)(struct authinfo *, void *),
                        void *arg)
{
#define STRING_BUFFER_SIZE 1024
#define SBSM1 STRING_BUFFER_SIZE - 1
	struct authinfo auth;
	int rc;

	/* fds */
	int dataIn, dataOut;

	char *whatToDo;

	char response[STRING_BUFFER_SIZE];

	/* FIXME might need to get the group as well */
	/* sysusername password homedir address fullname options */
	char sysusername[STRING_BUFFER_SIZE];
	char homedir[STRING_BUFFER_SIZE];
	char maildir[STRING_BUFFER_SIZE];
	char address[STRING_BUFFER_SIZE];
	char fullname[STRING_BUFFER_SIZE];
	char options[STRING_BUFFER_SIZE];

	memset(&auth, 0, sizeof(auth));

	auth.clearpasswd = pass;
	/* every response should start with 'OK ' */
	auth.sysusername = &sysusername[3];
	auth.homedir = &homedir[3];
	auth.maildir = &maildir[3];
	auth.address = &address[3];
	auth.fullname = &fullname[3];
	auth.options = &options[3];

	whatToDo = pass ? "AUTH\n" : "CHECK\n";
	DPRINTF("pipe: will %s\n", whatToDo);


	if (getPipe(&dataIn, &dataOut)) return -1;

	if (prog2pipe(dataOut, dataIn,  whatToDo) <= 0 ||
	    prog2pipe(dataOut, dataIn,  user) <= 0 ||
	    prog2pipe(dataOut, dataIn,  "\n") <= 0) return -1;
	if (pass && 
	    (prog2pipe(dataOut, dataIn, pass) <= 0 ||
	     prog2pipe(dataOut, dataIn, "\n") <= 0)) return -1;

	rc = pipe2prog(dataOut, dataIn, response, SBSM1);

	/* either 'OK ' did not come, or pipe broke */
	if (rc < 0) return -1;  /* pipe broke */
	else if ((rc >= 3 &&
	          response[0] == 'O' && response[1] == 'K' && 
		  response[2] == ' ') ||
	         (rc == 2 && response[0] == 'O' && response[1] == 'K'))
	{
		/* do nothing; pipe responded with 'OK '
		 * or with 'OK' (which is incorrect, but we will accept it */
	}
	else /* did not return OK */
	{
		DPRINTF("pipeauth: could not auth. %s, because: %s\n",
				user, response);
		return -1;
	}

	if (getFromPipe(dataOut, dataIn, "USERNAME", sysusername, SBSM1) <= 0 ||
	    getFromPipe(dataOut, dataIn, "HOMEDIR", homedir, SBSM1)  <= 0||
	    getFromPipe(dataOut, dataIn, "MAILDIR", maildir, SBSM1)  <= 0||
	    getFromPipe(dataOut, dataIn, "ADDRESS", address, SBSM1)  <= 0||
	    getFromPipe(dataOut, dataIn, "FULLNAME", fullname, SBSM1) <= 0 ||
	    getFromPipe(dataOut, dataIn, "OPTIONS", options, SBSM1) <= 0)
	{
		DPRINTF("pipeauth: could not finish requests for %s\n", user);
		/* must be some sort of pipe error, ... */
		return 1;
	}

	if (pass == 0) return (0);	/* Just get the authentication info */
	return ((*callback)(&auth, arg));
}


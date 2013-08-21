/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"comcargs.h"
#include	<string.h>
#include	<stdlib.h>

/*
	Parse command line arguments.
*/

const char *progname;

int cargs(int argc, char **argv, const struct courier_args *argp)
{
int	argn;
int	i,l=0;
const char *argval;

	if ((progname=strrchr(argv[0], '/')) != 0)
		++progname;
	else
		progname=argv[0];

	for (argn=1; argn<argc; argn++)
	{
		if (argv[argn][0] != '-')	break;
		if (argv[argn][1] == 0)
		{
			++argn;
			break;
		}
		if (argv[argn][0] != '-')	break;
		for (i=0; argp[i].argname; i++)
		{
			l=strlen(argp[i].argname);
			if (strncmp(argv[argn]+1, argp[i].argname, l) == 0 &&
				(argv[argn][l+1] == '=' ||
					argv[argn][l+1] == '\0'))
				break;
		}
		if (!argp[i].argname)
		{
			clog_open_stderr(progname);
			clog_msg_start_err();
			clog_msg_str("Invalid argument: ");
			clog_msg_str(argv[argn]);
			clog_msg_send();
			exit(1);
		}

		if (*(argval=argv[argn]+1+l))
			++argval;

		if (argp[i].argval)
			*argp[i].argval=argval;

		if (argp[i].argfunc)
			(*argp[i].argfunc)(argval);
	}
	return (argn);
}

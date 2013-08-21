/*
**
** Copyright 2004 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "courier_socks_config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "socks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include "configfile.h"

static void parseconfigfile(void *voidarg)
{
	FILE *f;
	DIR *dirp;
	struct dirent *de;

	D(DEBUG_CONFIG) fprintf(stderr, DEBUG_PREFIX ": Parsing %s\n", CONFIGFILE);

	f=fopen(CONFIGFILE, "r");

	if (f != NULL)
	{
		struct stat stat_buf;

		/*
		** On Linux open() of a directory succeeds, just to fail
		** later.  Check to see if we just opened a directory.
		*/

		if (fstat(fileno(f), &stat_buf) < 0 ||
		    S_ISDIR(stat_buf.st_mode))
		{
			fclose(f);
			f=NULL;
		}
	}

	if (f != NULL)
	{
		PARSE(f, CONFIGFILE);
		sys_fclose(f);
	}
	else	/* Parse configuration file directory */
		if ((dirp=opendir(CONFIGFILE)) != NULL)
	{
		while ((de=readdir(dirp)) != NULL)
		{
			char *n;

			if (strchr(de->d_name, '.') ||
			    strcmp(de->d_name, "CVS") == 0 ||
			    strchr(de->d_name, '~'))
				continue;

			n=malloc(sizeof(CONFIGFILE "/") + strlen(de->d_name));
			if (!n)
			{
				perror("malloc");
				exit(1);
			}
			strcat(strcpy(n, CONFIGFILE "/"), de->d_name);
			D(DEBUG_CONFIG)
				fprintf(stderr, DEBUG_PREFIX ": Parsing %s\n", n);

			if ((f=fopen(n, "r")) != NULL)
			{
				PARSE(f,n);
				sys_fclose(f);
			}
			free(n);
		}
		closedir(dirp);
	}
}

#define knownconfigword(c) \
	(strcmp((c), "allowenv") == 0 || \
	strcmp((c), "proxy") == 0 || \
	strcmp((c), "noproxy") == 0 || \
	strcmp((c), "port") == 0 || \
	strcmp((c), "authproxy") == 0 || \
	strcmp((c), "anonproxy") == 0 || \
	strcmp((c), "debug") == 0 || \
	strcmp((c), "user") == 0 || \
	strcmp((c), "prefork") == 0)

/*
** Copyright 1998 - 2006 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"cdpendelinfo.h"
#include	"cddlvrhost.h"
#include	"cddelinfo.h"
#include	"cddrvinfo.h"
#include	"cdrcptinfo.h"
#include	"cdmsgq.h"

#include	"courier.h"
#include	"sysconfdir.h"
#include	"libexecdir.h"
#include	"courierd.h"
#include	"rw.h"
#include	"maxlongsize.h"
#include	<stdlib.h>
#include	<string.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<signal.h>

drvinfo *drvinfo::modules;
unsigned drvinfo::nmodules;

drvinfo *drvinfo::module_dsn;

drvinfo::drvinfo()
{
}

drvinfo::~drvinfo()
{
}

void drvinfo::init()
{
unsigned	cnt, i;
struct	rw_transport *p;

	module_dsn=0;
	queuelo=0;
	for (cnt=0, p=rw_transport_first; p; p=p->next)
		++cnt;
	if ((modules=new drvinfo[cnt]) == 0)	clog_msg_errno();
	cnt=0;

	for (p=rw_transport_first; p; p=p->next)
	{
	char	*namebuf=(char *)courier_malloc(
		sizeof( SYSCONFDIR "/module.")+strlen(p->name));

		strcat(strcpy(namebuf, SYSCONFDIR "/module."),
				p->name);

	FILE	*config=fopen(namebuf, "r");

		if (!config)
		{
			clog_msg_start_err();
			clog_msg_str("Cannot open ");
			clog_msg_str(namebuf);
			clog_msg_send();
			exit(1);
		}
		free(namebuf);
		
	unsigned maxdels=1;
	unsigned maxhost=1;
	unsigned maxrcpt=1;
	char	*prog=0;
	char	buf[BUFSIZ];

		while (fgets(buf, sizeof(buf), config) != 0)
		{
		char	*p;

			if ((p=strchr(buf, '#')) != 0)	*p=0;
			p=strtok(buf, " \t\r\n=");
			if (strcmp(p, "MAXDELS") == 0)
			{
				p=strtok(NULL, " \t\r\n=");
				if (p)	maxdels=atoi(p);
			}
			else if (strcmp(p, "MAXHOST") == 0)
			{
				p=strtok(NULL, " \t\r\n=");
				if (p)	maxhost=atoi(p);
			}
			else if (strcmp(p, "MAXRCPT") == 0)
			{
				p=strtok(NULL, " \t\r\n=");
				if (p)	maxrcpt=atoi(p);
			}
			else if (strcmp(p, "PROG") == 0 && !prog)
			{
				p=strtok(NULL, "\t\r\n=");
				if (p)
				{
					prog=(char *)
						courier_malloc(strlen(p)+1);
					strcpy(prog, p);
				}
			}
		}
		fclose(config);
		if (!prog)	continue;	// Input only module

		modules[cnt].module=p;
		p->udata=(void *)&modules[cnt];
		modules[cnt].delinfo_list.resize(maxdels);
		modules[cnt].hosts_list.resize(maxdels);
		queuelo += maxdels;
		modules[cnt].maxhost=maxhost;
		modules[cnt].maxrcpt=maxrcpt;
		modules[cnt].delpfreefirst=0;

		if (strcmp(p->name, DSN) == 0)
			module_dsn= &modules[cnt];

		for (i=0; i<maxdels; i++)
		{
			modules[cnt].delinfo_list[i].delid=i;
			modules[cnt].delinfo_list[i].freenext=
				modules[cnt].delpfreefirst;
			modules[cnt].delpfreefirst=
				&modules[cnt].delinfo_list[i];
		}

		modules[cnt].hdlvrpfree=0;
		for (i=0; i<maxdels; i++)
		{
			modules[cnt].hosts_list[i].next=
				modules[cnt].hdlvrpfree;
			modules[cnt].hdlvrpfree=&modules[cnt].hosts_list[i];
		}
		modules[cnt].hdlvrpfirst=0;
		modules[cnt].hdlvrplast=0;

	int	pipe0[2], pipe1[2];

		if (pipe(pipe0) || pipe(pipe1))
			clog_msg_errno();

		switch (modules[cnt].module_pid=fork())	{
		case 0:
#if 0
// replaced by close-on-exec, below

			for (i=cnt; i; )
			{
				--i;
				fclose(modules[i].module_to);
				close(modules[i].module_from.fd);
			}
#endif
			close(pipe0[1]);
			close(pipe1[0]);
			dup2(pipe0[0], 0);
			dup2(pipe1[1], 1);
			close(pipe0[0]);
			close(pipe1[1]);
			signal(SIGCHLD, SIG_DFL);

			if (chdir(MODULEDIR) ||
				chdir(modules[cnt].module->name))
			{
				clog_msg_start_err();
				clog_msg_str(MODULEDIR "/");
				clog_msg_str(modules[cnt].module->name);
				clog_msg_str(" does not exist.");
				clog_msg_send();
				clog_msg_errno();
			}

			{
			char maxdels_buf[MAXLONGSIZE+15];
			char maxhost_buf[MAXLONGSIZE+15];
			char maxrcpt_buf[MAXLONGSIZE+15];
			const char *sh=getenv("SHELL");

				sprintf(maxdels_buf, "MAXDELS=%ld",
					(long)maxdels);
				sprintf(maxhost_buf, "MAXHOST=%ld",
					(long)maxhost);
				sprintf(maxrcpt_buf, "MAXRCPT=%ld",
					(long)maxrcpt);

				putenv(maxdels_buf);
				putenv(maxhost_buf);
				putenv(maxrcpt_buf);

			const char *ch=courierdir();
			char	*p=(char *)courier_malloc(strlen(ch)+
					sizeof("COURIER_HOME="));

				strcat(strcpy(p,"COURIER_HOME="), ch);
				putenv(p);

				if (!sh)	sh="/bin/sh";
				execl(sh, sh, "-c", prog, (char *)0);
				clog_msg_start_err();
				clog_msg_str(MODULEDIR "/");
				clog_msg_str(modules[cnt].module->name);
				clog_msg_str(": ");
				clog_msg_str(prog);
				clog_msg_send();
				clog_msg_errno();
			}
		case -1:
			clog_msg_errno();
			break;
		}

		close(pipe0[0]);
		close(pipe1[1]);

		if (fcntl(pipe0[1], F_SETFD, FD_CLOEXEC) ||
			fcntl(pipe1[0], F_SETFD, FD_CLOEXEC))
			clog_msg_errno();

		if ((modules[cnt].module_to=fdopen(pipe0[1], "w")) == NULL)
			clog_msg_errno();

		mybuf_init( &modules[cnt].module_from, pipe1[0] );

		clog_msg_start_info();
		clog_msg_str("Started ");
		clog_msg_str(prog);
		clog_msg_str(", pid=");
		clog_msg_int(modules[cnt].module_pid);
		clog_msg_str(", maxdels=");
		clog_msg_int(maxdels);
		clog_msg_str(", maxhost=");
		clog_msg_int(maxhost);
		clog_msg_str(", maxrcpt=");
		clog_msg_int(maxrcpt);
		clog_msg_send();
		free(prog);
		cnt++;
	}
	nmodules=cnt;

	if (queuelo < 200)	queuelo=200;

char	*cfname=config_localfilename("queuelo");
char	*conf=config_read1l(cfname);

	if (conf)
	{
		queuelo=atoi(conf);
		free(conf);
	}
	free(cfname);
	if (queuelo < 20)	queuelo=20;

	cfname=config_localfilename("queuehi");
	conf=config_read1l(cfname);
	if (conf)
	{
		queuehi=atoi(conf);
		free(conf);
	}
	else
		queuehi=queuelo < 500 ? queuelo * 2:queuelo+1000;
	if (queuehi < queuelo)	queuehi=queuelo;
	free(cfname);

	clog_msg_start_info();
	clog_msg_str("queuelo=");
	clog_msg_int(queuelo);
	clog_msg_str(", queuehi=");
	clog_msg_int(queuehi);
	clog_msg_send();

	if (module_dsn == 0)
	{
		clog_msg_start_err();
		clog_msg_str("Driver dsn not found.");
		clog_msg_send();
		exit(1);
	}
}

//
// Cleanup during nice termination
//
void drvinfo::cleanup()
{
unsigned i;

	// First, close the command pipe to all the modules

	for (i=0; i<nmodules; i++)
		fclose(modules[i].module_to);

	// Now, wait for all the modules to exit.

	for (i=0; i<nmodules; i++)
	{
		while (mybuf_get(&modules[i].module_from) >= 0)
			;

		close(modules[i].module_from.fd);
	}

	delete[] modules;
}

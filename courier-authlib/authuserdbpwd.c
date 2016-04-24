/*
** Copyright 2001-2005 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<signal.h>
#include	<pwd.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	"userdb/userdb.h"
#include	"auth.h"
#include	"courierauthstaticlist.h"
#include	"authwait.h"
#include	"sbindir.h"
#include	"courierauthdebug.h"
#include        "libhmac/hmac.h"



static int bad(const char *q)
{
	const char *p;

	for (p=q; *p; p++)
		if ((int)(unsigned char)*p < ' ' || *p == '|' || *p == '='
		    || *p == '\'' || *p == '"')
			return (1);
	return (0);
}


static char *hmacpw(const char *pw, const char *hash)
{
	int     i;

	for (i=0; hmac_list[i] &&
		     strcmp(hmac_list[i]->hh_name, hash); i++)
                                ;
	if (hmac_list[i])
	{
		struct hmac_hashinfo    *hmac=hmac_list[i];
		unsigned char *p=malloc(hmac->hh_L*2);
		char *q=malloc(hmac->hh_L*4+1);
		unsigned i;

                if (!p || !q)
                {
                        perror("malloc");
                        exit(1);
                }

                hmac_hashkey(hmac, pw, strlen(pw), p, p+hmac->hh_L);
                for (i=0; i<hmac->hh_L*2; i++)
                        sprintf(q+i*2, "%02x", (int)p[i]);
		free(p);
		return (q);
	}
	return (NULL);
}

static int dochangepwd1(const char *, const char *, const char *, const char *,
			const char *);

static int try_auth_userdb_passwd(const char *hmac_flag,
				  const char *service,
				  const char *uid,
				  const char *opwd_buf,
				  const char *npwd_buf);
static int makeuserdb();

int auth_userdb_passwd(const char *service,
		       const char *uid,
		       const char *opwd_buf,
		       const char *npwd_buf)
{
	int rc;
	int rc2;

	if (bad(uid) || strchr(uid, '/'))
	{
		errno=EPERM;
		DPRINTF("userdb: %s is not a valid userid.\n",
			uid);
		return -1;
	}

	if (bad(service) ||
	    bad(opwd_buf) ||
	    bad(npwd_buf))
	{
		errno=EPERM;
		DPRINTF("userdb: Invalid service or password string for %s.\n",
			uid);
		return (-1);
	}

	rc=try_auth_userdb_passwd(NULL, service, uid, opwd_buf, npwd_buf);

	if (rc > 0)
		return rc;

	{
		int i;


		for (i=0; hmac_list[i]; i++)
		{
			const char *n=hmac_list[i]->hh_name;

			char *hmacservice=malloc(strlen(service)+strlen(n)
						 +sizeof("-hmac-"));

			if (hmacservice == NULL)
				return (1);

			strcat(strcat(strcpy(hmacservice, service),
				      "-hmac-"), n);

			rc2=try_auth_userdb_passwd(n, hmacservice, uid,
						   opwd_buf, npwd_buf);

			if (rc2 > 0)
			{
				free(hmacservice);
				return (1);
			}

			if (rc2 == 0)
				rc=0;

			strcat(strcpy(hmacservice, "hmac-"), n);

			rc2=try_auth_userdb_passwd(n, hmacservice, uid,
						   opwd_buf, npwd_buf);
			free(hmacservice);
			if (rc2 > 0)
				return (1);

			if (rc2 == 0)
				rc=0;

		}
	}

	if (rc == 0)
	{
		rc=makeuserdb();

		if (rc)
		{
			DPRINTF("makeuserdb: error: %s", strerror(errno));
		}
	}

	DPRINTF("authuserdb: return code %d", rc);
	return rc;
}

static int try_auth_userdb_passwd(const char *hmac_flag,
				  const char *service,
				  const char *uid,
				  const char *opwd_buf,
				  const char *npwd_buf)
{
	char *opwd;
	char *npwd;
	int rc;

	if (hmac_flag)
	{
		DPRINTF("Trying to change password for %s",
			hmac_flag);

		DPWPRINTF("Old password=%s, new password=%s",
			  opwd_buf, npwd_buf);

		opwd=hmacpw(opwd_buf, hmac_flag);
		if (!opwd)
			return 1;

		npwd=hmacpw(npwd_buf, hmac_flag);

		if (!npwd)
		{
			free(opwd);
			return (1);
		}
	}
	else
	{
		DPRINTF("Trying to change system password for %s",
			service);

		DPWPRINTF("Old password=%s, new password=%s",
			  opwd_buf, npwd_buf);

		opwd=strdup(opwd_buf);
		if (!opwd)
		{
			return (1);
		}

		npwd=userdb_mkmd5pw(npwd_buf);
		if (!npwd || !(npwd=strdup(npwd)))
		{
			free(opwd);
			errno=EPERM;
			return (1);
		}
	}


	rc=dochangepwd1(service, uid, opwd, npwd, hmac_flag);

	free(opwd);
	free(npwd);
	return (rc);
}

static int dochangepwd2(const char *service, const char *uid,
			char    *u,
			const struct  userdbs *udb, const char *npwd);

static int dochangepwd1(const char *service, const char *uid,
			const char *opwd, const char *npwd,
			const char *hmac_flag)
{
	char *udbs;
	char *services;
	char *passwords;
	int rc;

	char    *u;
	struct  userdbs *udb;


	udbs=userdbshadow(USERDB "shadow.dat", uid);

	if (!udbs)
	{
		errno=ENOENT;
		return (-1);
	}

	if ((services=malloc(strlen(service)+sizeof("pw"))) == 0)
	{
		perror("malloc");
		free(udbs);
		errno=EPERM;
		return (1);
	}

	strcat(strcpy(services, service), "pw");

	DPRINTF("Checking for password called \"%s\"", services);

	passwords=userdb_gets(udbs, services);
	free(services);

	if (passwords == 0 && hmac_flag == 0)
	{
		DPRINTF("Not found, checking for \"systempw\"");
		passwords=userdb_gets(udbs, "systempw");
		service="system";
	}

	if (!passwords || (hmac_flag ? strcmp(opwd, passwords):
			   authcheckpassword(opwd, passwords)))
	{
		if (!passwords)
		{
			DPRINTF("Password not found.");
		}
		else
		{
			DPRINTF("Password didn't match.");
		}

		if (passwords)
			free(passwords);
		free(udbs);
		errno=EPERM;
		return (-1);
	}
	free(passwords);
	free(udbs);

        userdb_init(USERDB ".dat");
        if ( (u=userdb(uid)) == 0 ||
	     (udb=userdb_creates(u)) == 0)
        {
		if (u)
			free(u);
		errno=EPERM;
		return (1);
        }

	rc=dochangepwd2(service, uid, u, udb, npwd);

	userdb_frees(udb);
	free(u);
	return (rc);
}

static int dochangepwd2(const char *services, const char *uid,
			char    *u,
			const struct  userdbs *udb, const char *npwd)
{
	char *argv[10];
	pid_t p, p2;
	int waitstat;

	argv[0]=SBINDIR "/userdb";
	argv[1]=malloc(strlen(udb->udb_source ? udb->udb_source:"")
		       +strlen(uid)+1);

	if (!argv[1])
	{
		errno=EPERM;
		return (1);
	}
	strcpy(argv[1],udb->udb_source ? udb->udb_source:"");
	strcat(argv[1],uid);
	argv[2]="set";

	argv[3]=malloc(strlen(services)+strlen(npwd)+10);
	if (!argv[3])
	{
		free(argv[1]);
		errno=EPERM;
		return (1);
	}

	sprintf(argv[3], "%spw=%s", services, npwd);
	signal(SIGCHLD, SIG_DFL);
	argv[4]=0;

	DPRINTF("Executing %s %s %s %s%s",
		argv[0],
		argv[1],
		argv[2],
		courier_authdebug_login_level >= 2 ? argv[3]:services,
		courier_authdebug_login_level >= 2 ? "":"pw=******");

	p=fork();

	if (p < 0)
	{
		free(argv[3]);
		free(argv[1]);
		errno=EPERM;
		return (1);
	}

	if (p == 0)
	{
		execv(argv[0], argv);
		perror(argv[0]);
		exit(1);
	}

	free(argv[1]);
	free(argv[3]);

	while ((p2=wait(&waitstat)) != p)
	{
		if (p2 < 0 && errno == ECHILD)
		{
			perror("wait");
			errno=EPERM;
			return (1);
		}
	}

	if (!WIFEXITED(waitstat) || WEXITSTATUS(waitstat))
	{
		DPRINTF("Command failed: with exit code %d",
			(int)WEXITSTATUS(waitstat));
		errno=EPERM;
		return (1);
	}
	DPRINTF("Command succeeded: with exit code %d",
		(int)WEXITSTATUS(waitstat));
	return 0;
}

static int makeuserdb()
{
	char *argv[2];
	pid_t p, p2;
	int waitstat;

	DPRINTF("Executing makeuserdb");
	p=fork();

	if (p < 0)
	{
		perror("fork");
		return (1);
	}

	if (p == 0)
	{
		argv[0]= SBINDIR "/makeuserdb";
		argv[1]=0;

		execv(argv[0], argv);
		perror(argv[0]);
		exit(1);
	}

	while ((p2=wait(&waitstat)) != p)
	{
		if (p2 < 0 && errno == ECHILD)
		{
			errno=EPERM;
			return (1);
		}
	}

	if (!WIFEXITED(waitstat) || WEXITSTATUS(waitstat))
	{
		errno=EPERM;
		return (1);
	}
	return (0);
}

/*
** Copyright 1998 - 2012 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_auth_config.h"
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<signal.h>
#include	<errno.h>
#if	HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	"auth.h"
#include	"authwait.h"
#include	"courierauthstaticlist.h"
#include	"courierauthdebug.h"

#if	HAVE_SECURITY_PAM_APPL_H
#include	<security/pam_appl.h>
#endif

#if	HAVE_PAM_PAM_APPL_H
#include	<Pam/pam_appl.h>
#endif

extern char tcpremoteip[];

static const char *pam_username, *pam_password, *pam_service;

extern void auth_pwd_enumerate( void(*cb_func)(const char *name,
					       uid_t uid,
					       gid_t gid,
					       const char *homedir,
					       const char *maildir,
					       const char *options,
					       void *void_arg),
				void *void_arg);

static int pam_conv(int num_msg, const struct pam_message **msg,
                    struct pam_response **resp, void *appdata_ptr)
{
int i = 0;
struct pam_response *repl = NULL;

	repl = malloc(sizeof(struct pam_response) * num_msg);
	if (!repl) return PAM_CONV_ERR;

	for (i=0; i<num_msg; i++)
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_ON:
			repl[i].resp_retcode = PAM_SUCCESS;
			repl[i].resp = strdup(pam_username);
			if (!repl[i].resp)
			{
				perror("strdup");
				exit(1);
			}
			break;
		case PAM_PROMPT_ECHO_OFF:
			repl[i].resp_retcode = PAM_SUCCESS;
			repl[i].resp = strdup(pam_password);
			if (!repl[i].resp)
			{
				perror("strdup");
				exit(1);
			}
			break;
		case PAM_TEXT_INFO:
		case PAM_ERROR_MSG:
			if (write(2, msg[i]->msg, strlen(msg[i]->msg)) < 0 ||
			    write(2, "\n", 1) < 0)
				; /* ignore gcc warning */

			repl[i].resp_retcode = PAM_SUCCESS;
			repl[i].resp = NULL;
			break;
		default:
			free (repl);
			return PAM_CONV_ERR;
		}

	*resp=repl;
	return PAM_SUCCESS;
}

static struct pam_conv conv = {
          pam_conv,
          NULL
      };

static int dopam(pam_handle_t **pamh, int *started)
{
	int	retval;

	DPRINTF("pam_service=%s, pam_username=%s",
		pam_service ? pam_service : "<null>",
		pam_username ? pam_username : "<null>");

	*started=1;

	retval=pam_start(pam_service, pam_username, &conv, pamh);
	if (retval != PAM_SUCCESS)
	{
		DPRINTF("pam_start failed, result %d [Hint: bad PAM configuration?]", retval);
		*started=0;
	}

#ifdef PAM_RHOST
	if (retval == PAM_SUCCESS && tcpremoteip[0])
	{
		retval=pam_set_item(*pamh, PAM_RHOST, tcpremoteip);
		if (retval != PAM_SUCCESS)
		{
			DPRINTF("pam_set_item(PAM_RHOST) failed, result %d",
				retval);
		}
	}
#endif
#if 0
	if (retval == PAM_SUCCESS)
	{
		retval=pam_set_item(*pamh, PAM_AUTHTOK, pam_password);
		if (retval != PAM_SUCCESS)
		{
			DPRINTF("pam_set_item failed, result %d", retval);
		}
	}
#endif

	if (retval == PAM_SUCCESS)
	{
		retval=pam_authenticate(*pamh, 0);
		if (retval != PAM_SUCCESS)
		{
			DPRINTF("pam_authenticate failed, result %d", retval);
		}
	}

#if 0

#if	HAVE_PAM_SETCRED
	if (retval == PAM_SUCCESS)
	{
		retval=pam_setcred(*pamh, PAM_ESTABLISH_CRED);
		if (retval != PAM_SUCCESS)
		{
			DPRINTF("pam_setcred failed, result %d", retval);
		}
	}
#endif
#endif

	if (retval == PAM_SUCCESS)
	{
		retval=pam_acct_mgmt(*pamh, 0);
		if (retval != PAM_SUCCESS)
		{
			DPRINTF("pam_acct_mgmt failed, result %d", retval);
		}
	}
	if (retval == PAM_SUCCESS)
	{
		DPRINTF("dopam successful");
	}
	return (retval);
}

struct callback_info {
	int (*callback_func)(struct authinfo *, void *);
	void *callback_arg;
	} ;

static int callback_pam(struct authinfo *a, void *argptr)
{
struct callback_info *ci=(struct callback_info *)argptr;
pam_handle_t	*pamh=NULL;
int	pipefd[2];
int	retval;
pid_t	p;
int	waitstat;
char	*s;
char	buf[1];


	a->clearpasswd=pam_password;
	s=strdup(a->sysusername);
	if (!s)
	{
		perror("malloc");
		return (1);
	}


/*
**	OK, in order to transparently support PAM sessions inside this
**	authentication module, what we need to do is to fork(), and let
**	the child run in its parent place.  Once the child process exits,
**	the parent calls pam_end_session, and clears the PAM library.
**
**	This means that upon return from auth_pam(), your process ID
**	might've changed!!!
**
**	However, if the authentication fails, we can simply exit, without
**	running as a child process.
**
**	Additionally, the PAM library might allocate some resources that
**	authenticated clients should not access.  Therefore, we fork
**	*before* we try to authenticate.  If the authentication succeeds,
**	the child process will run in the parent's place.  The child
**	process waits until the parent tells it whether the authentication
**	worked.  If it worked, the child keeps running.  If not, the child
**	exits, which the parent waits for.
**
**	The authentication status is communicated to the child process via
**	a pipe.
*/
	if (pipe(pipefd) < 0)
	{
		perror("pipe");
		free(s);
		return (1);
	}

	if ((p=fork()) == -1)
	{
		perror("fork");
		free(s);
		return (1);
	}

	if (p == 0)
	{
		int started;

		close(pipefd[0]);
		retval=dopam(&pamh, &started);
		if (retval == PAM_SUCCESS)
			if (write(pipefd[1], "", 1) < 0)
				; /* ignore gcc warning */
		close(pipefd[1]);

		if (started)
			pam_end(pamh, retval);
		_exit(0);
	}


	close(pipefd[1]);
	while (wait(&waitstat) != p)
		;
	if (read(pipefd[0], buf, 1) > 0)
	{
		int rc;

		close(pipefd[0]);
		a->address=s;
		rc=(*ci->callback_func)(a, ci->callback_arg);
		free(s);
		return rc;
	}
	close(pipefd[0]);
	free(s);
	errno=EPERM;
	return (-1);
}

extern int auth_pam_pre(const char *userid, const char *service,
        int (*callback)(struct authinfo *, void *),
                        void *arg);

int auth_pam(const char *service, const char *type, char *authdata,
	     int (*callback_func)(struct authinfo *, void *),
	     void *callback_arg)
{
	struct	callback_info	ci;

	if (strcmp(type, AUTHTYPE_LOGIN))
	{
		DPRINTF("authpam only handles authtype=" AUTHTYPE_LOGIN);
		errno=EPERM;
		return (-1);
	}

	if ((pam_username=strtok(authdata, "\n")) == 0 ||
		(pam_password=strtok(0, "\n")) == 0)
	{
		DPRINTF("incomplete username or missing password");
		errno=EPERM;
		return (-1);
	}

	pam_service=service;

	ci.callback_func=callback_func;
	ci.callback_arg=callback_arg;
        return auth_pam_pre(pam_username, service, &callback_pam, &ci);
}

static void auth_pam_cleanup()
{
}

static struct authstaticinfo authpam_info={
	"authpam",
	auth_pam,
	auth_pam_pre,
	auth_pam_cleanup,
	auth_syspasswd,
	auth_pam_cleanup,
	auth_pwd_enumerate};


struct authstaticinfo *courier_authpam_init()
{
	return &authpam_info;
}

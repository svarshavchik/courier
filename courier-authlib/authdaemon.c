/*
** Copyright 2000-2015 Double Precision, Inc.  See COPYING for
** distribution information.
*/

#include	"auth.h"
#include	"courierauthstaticlist.h"
#include	"courierauthsasl.h"
#include	"authwait.h"
#include	"courierauthdebug.h"
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<signal.h>
#include	<unistd.h>
#include	<errno.h>
#include	<sys/time.h>
#include	<sys/select.h>
#include	"numlib/numlib.h"
#include	<sys/stat.h>
#include	<pwd.h>
#include	<sys/types.h>
#include	<dirent.h>

extern int authdaemondo_meta(struct auth_meta *meta,
			     const char *authreq,
			     int (*func)(struct authinfo *, void *),
			     void *arg);

extern void auth_daemon_enumerate( void(*cb_func)(const char *name,
						  uid_t uid,
						  gid_t gid,
						  const char *homedir,
						  const char *maildir,
						  const char *options,
						  void *void_arg),
				   void *void_arg);

int auth_generic(const char *service,
		 const char *authtype,
		 char *authdata,
		 int (*callback_func)(struct authinfo *, void *),
		 void *callback_arg)
{
	struct auth_meta dummy;

	memset(&dummy, 0, sizeof(dummy));

	return auth_generic_meta(&dummy, service, authtype, authdata,
				 callback_func, callback_arg);
}

int auth_generic_meta(struct auth_meta *meta,
		      const char *service,
		      const char *authtype,
		      char *authdata,
		      int (*callback_func)(struct authinfo *, void *),
		      void *callback_arg)
{
	char	tbuf[NUMBUFSIZE];
	size_t	l=strlen(service)+strlen(authtype)+strlen(authdata)+2;
	char	*n=libmail_str_size_t(l, tbuf);
	char	*buf=malloc(strlen(n)+l+20);
	int	rc;

	courier_authdebug_login_init();

	if (!buf)
		return 1;

	strcat(strcat(strcpy(buf, "AUTH "), n), "\n");
	strcat(strcat(buf, service), "\n");
	strcat(strcat(buf, authtype), "\n");
	strcat(buf, authdata);

	rc=strcmp(authtype, "EXTERNAL") == 0
		? auth_getuserinfo_meta(meta, service, authdata, callback_func,
					callback_arg)
		: authdaemondo_meta(meta, buf, callback_func, callback_arg);
	free(buf);

	if (courier_authdebug_login_level)
	{
	struct timeval t;

		/* short delay to try and allow authdaemond's courierlogger
		   to finish writing; otherwise items can appear out of order */
		t.tv_sec = 0;
		t.tv_usec = 100000;
		select(0, 0, 0, 0, &t);
	}

	return rc;
}

int auth_callback_default(struct authinfo *ainfo)
{
	if (ainfo->address == NULL)
	{
		fprintf(stderr, "WARN: No address!!\n");
		return (-1);
	}

	if (ainfo->sysusername)
		libmail_changeusername(ainfo->sysusername,
				       &ainfo->sysgroupid);
	else if (ainfo->sysuserid)
		libmail_changeuidgid(*ainfo->sysuserid,
				     ainfo->sysgroupid);
	else
	{
		fprintf(stderr, "WARN: %s: No UID/GID!!\n", ainfo->address);
		return (-1);
	}

	if (!ainfo->homedir)
	{
		errno=EINVAL;
		fprintf(stderr, "WARN: %s: No homedir!!\n", ainfo->address);
		return (1);
	}

	if (chdir(ainfo->homedir))
	{
		fprintf(stderr, "WARN: %s: chdir(%s) failed!!\n",
			ainfo->address, ainfo->homedir);
		perror("WARN: error");
		return (1);
	}

	return 0;
}

static void do_copy(const char *from,
		    const char *to,
		    struct stat *stat_buf,
		    uid_t uid,
		    gid_t gid)
{
	FILE *fp=fopen(from, "r");
	FILE *fp2;
	int c;

	if (!fp)
	{
		fprintf(stderr, "WARN: %s: %s!!\n", from, strerror(errno));
		return;
	}

	fp2=fopen(to, "w");

	if (!fp2)
	{
		fprintf(stderr, "WARN: %s: %s!!\n", to, strerror(errno));
		fclose(fp);
		return;
	}

	while ((c=getc(fp)) != EOF)
		putc(c, fp2);
	fclose(fp);
	fclose(fp2);

	if (chmod(to, stat_buf->st_mode & ~S_IFMT) ||
	    chown(to, uid, gid) < 0)
	{
		fprintf(stderr, "WARN: %s: %s!!\n", to, strerror(errno));
	}
}

static void do_symlink(const char *from,
		       const char *to,
		       uid_t uid,
		       gid_t gid)
{
	char buf[1024];
	ssize_t l=readlink(from, buf, sizeof(buf)-1);

	if (l < 0)
	{
		fprintf(stderr, "WARN: %s: %s!!\n", from, strerror(errno));
		return;
	}
	buf[l]=0;

	if (symlink(buf, to) < 0 || lchown(to, uid, gid) < 0)
	{
		fprintf(stderr, "WARN: %s: %s!!\n", from, strerror(errno));
		return;
	}
}

static int do_mkhomedir(const char *skeldir,
			const char *homedir,
			uid_t uid,
			gid_t gid)
{
	struct stat stat_buf;
	DIR *d;
	struct dirent *de;

	mkdir(homedir, 0);

	if (stat(skeldir, &stat_buf) < 0 ||
	    chmod(homedir, stat_buf.st_mode & ~S_IFMT) ||
	    chown(homedir, uid, gid) < 0)
	{
		fprintf(stderr, "WARN: %s: %s!!\n", skeldir, strerror(errno));
		return 0;
	}

	if ((d=opendir(skeldir)) != NULL)
	{
		while ((de=readdir(d)) != NULL)
		{
			char *fskeldir, *fhomedir;

			if (strcmp(de->d_name, ".") == 0)
				continue;
			if (strcmp(de->d_name, "..") == 0)
				continue;

			fskeldir=malloc(strlen(skeldir)+2+strlen(de->d_name));
			fhomedir=malloc(strlen(homedir)+2+strlen(de->d_name));

			if (fskeldir && fhomedir)
			{
				strcat(strcat(strcpy(fskeldir, skeldir), "/"),
				       de->d_name);
				strcat(strcat(strcpy(fhomedir, homedir), "/"),
				       de->d_name);

				if (lstat(fskeldir, &stat_buf) == 0)
				{
					if (S_ISDIR(stat_buf.st_mode))
					{
						do_mkhomedir(fskeldir, fhomedir,
							     uid, gid);
					}
					else if (S_ISLNK(stat_buf.st_mode))
					{
						do_symlink(fskeldir, fhomedir,
							   uid, gid);
					}
					else if (S_ISREG(stat_buf.st_mode))
					{
						do_copy(fskeldir, fhomedir,
							&stat_buf,
							uid, gid);
					}
				}
			}

			if (fskeldir)
				free(fskeldir);
			if (fhomedir)
				free(fhomedir);
		}
		closedir(d);
	}
	return 0;
}

static int mkhomedir(struct authinfo *ainfo,
		     const char *skel)
{
	if (ainfo->sysusername)
	{
		struct passwd *pw=getpwnam(ainfo->sysusername);
		return do_mkhomedir(skel, ainfo->homedir,
				    pw->pw_uid,
				    pw->pw_gid);
	}

	return do_mkhomedir(skel, ainfo->homedir,
			    *ainfo->sysuserid, ainfo->sysgroupid);
}

int auth_mkhomedir(struct authinfo *ainfo)
{
	struct stat stat_buf;

	const char *p=getenv("AUTH_MKHOMEDIR_SKEL");

	if (p && *p && ainfo->address &&
	    (ainfo->sysusername || ainfo->sysuserid) &&
	    ainfo->homedir && stat(ainfo->homedir, &stat_buf) < 0 &&
	    errno == ENOENT && stat(p, &stat_buf) == 0)
	{
		int rc;
		mode_t old_umask=umask(0777);

		rc=mkhomedir(ainfo, p);

		umask(old_umask);

		if (rc)
			return rc;
	}
	return 0;
}

int auth_callback_default_autocreate(struct authinfo *ainfo)
{
	int rc=auth_mkhomedir(ainfo);

	if (rc)
		return rc;

	return auth_callback_default(ainfo);
}

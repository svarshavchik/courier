#include "comtrack.h"
#include "courier.h"
#include "localstatedir.h"
#include "numlib/numlib.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

static const char email_address_statuses[]={
	TRACK_ADDRACCEPTED, TRACK_ADDRDEFERRED, TRACK_ADDRFAILED, 0
};

static const char broken_starttls_statuses[]={
	TRACK_BROKEN_STARTTLS, 0
};

static void all_lower(char *c)
{
	while (*c)
	{
		*c=tolower((int)(unsigned char)*c);
		++c;
	}
}

static int dopurge(const char *trackdir);

void trackpurge(const char *trackdir)
{
	struct stat stat_buf;

	if (stat(trackdir, &stat_buf) == 0 && stat_buf.st_uid != geteuid())
	{
		clog_msg_start_err();
		clog_msg_str("Warning: ");
		clog_msg_str(trackdir);
		clog_msg_str(" is not owned by the correct uid");
		clog_msg_send();
	}

	if (dopurge(trackdir))
	{
		clog_msg_start_err();
		clog_msg_str("Cannot open ");
		clog_msg_str(trackdir);
		clog_msg_send();
		clog_msg_prerrno();
		return;
	}
}

static int dopurge(const char *trackdir)
{
	DIR *dirp;
	struct dirent *de;

	time_t curTime=time(NULL) / 3600;

	dirp=opendir(trackdir);

	if (!dirp)
		return -1;

	while ((de=readdir(dirp)) != NULL)
	{
		char *p;

		if (de->d_name[0] == '.')
			continue;

		if (atoi(de->d_name) >= curTime - (TRACK_NHOURS+1))
			continue;

		p=malloc(strlen(trackdir)+2+strlen(de->d_name));

		if (p)
		{
			strcat(strcat(strcpy(p, trackdir), "/"), de->d_name);
			unlink(p);
			free(p);
		}
	}
	closedir(dirp);
	return 0;
}

static char *make_track_filename(const char *trackdir,
				 time_t offset)
{
	char *namebuf;
	char buf2[NUMBUFSIZE];

	libmail_str_time_t(offset, buf2);

	namebuf=malloc(strlen(trackdir)+strlen(buf2)+2);

	if (!namebuf)
		return NULL;

	strcat(strcat(strcpy(namebuf, trackdir), "/"), buf2);

	return namebuf;
}

static FILE *open_track_file(const char *trackdir,
			     time_t offset)
{
	FILE *fp;
	char *p=make_track_filename(trackdir, offset);

	if (!p)
		return NULL;

	fp=fopen(p, "r");

	free(p);
	return fp;
}

static int track_find_record(const char *trackdir,
			     const char *address, time_t *timestamp,
			     const char *search_for_statuses,
			     void (*lower)(char *))

{
	time_t curTime=time(NULL) / 3600;
	int i;
	char linebuf[BUFSIZ];
	char *p;
	char *addrbuf=strdup(address);
	int results=TRACK_NOTFOUND;

	if (!addrbuf)
		return TRACK_NOTFOUND;

	lower(addrbuf);

	for (i=0; i <= TRACK_NHOURS; ++i)
	{
		FILE *fp=open_track_file(trackdir, curTime-i);

		if (fp == NULL)
			continue;

		while (fgets(linebuf, sizeof(linebuf), fp))
		{
			p=strchr(linebuf, '\n');
			if (p) *p=0;

			p=strchr(linebuf, ' ');
			if (!p)
				continue;
			++p;

			if (*p && strchr(search_for_statuses, *p) &&
			    strcmp(p+1, addrbuf) == 0)
			{
				if ((*timestamp=libmail_atotime_t(linebuf))
				    != 0)
					results=*p;
			}
		}
		fclose(fp);
		if (results != TRACK_NOTFOUND)
			break;
	}
	free(addrbuf);
	return results;
}

int track_find_email(const char *address, time_t *timestamp)
{
	return track_find_record(TRACKDIR, address, timestamp,
				 email_address_statuses,
				 domainlower);
}

int track_find_broken_starttls(const char *address, time_t *timestamp)
{
	return track_find_record(TRACKDIR,
				 address, timestamp, broken_starttls_statuses,
				 all_lower);
}

static void track_save_record(const char *trackdir,
			      const char *address, int status,
			      void (*lower)(char *),
			      int autopurge)
{
	char buf2[NUMBUFSIZE];
	FILE *fp;
	time_t curTime=time(NULL);
	time_t t=curTime / 3600;
	char *addrbuf;
	struct stat stat_buf;
	char *namebuf;

	addrbuf=strdup(address);

	if (!addrbuf)
		return;

	lower(addrbuf);

	namebuf=make_track_filename(trackdir, t);

	if (namebuf)
	{
		if (autopurge && stat(namebuf, &stat_buf))
		{
			dopurge(trackdir);
		}

		if (stat(trackdir, &stat_buf) == 0 &&
		    stat_buf.st_uid == geteuid())
			/*
			** Sanity check: avoid creating root-owner files, if
			** verifysmtp is executed by root.
			*/
		{
			fp=fopen(namebuf, "a");

			libmail_str_time_t(curTime, buf2);

			if (fp)
			{
				fprintf(fp, "%s %c%s\n", buf2, (char)status,
					addrbuf);
				fclose(fp);
			}
		}
		free(namebuf);
	}
	free(addrbuf);
}

void track_save_email(const char *address, int status)
{
	return track_save_record(TRACKDIR, address, status, domainlower, 0);
}

void track_save_broken_starttls(const char *address)
{
	track_save_record(TRACKDIR, address, TRACK_BROKEN_STARTTLS, all_lower,
			  0);
}

static int track_read(const char *trackdir,
		      int (*cb_func)(time_t timestamp, int status,
				     const char *address, void *voidarg),
		      const char *acceptable,
		      void *voidarg)
{
	time_t curTime=time(NULL) / 3600;
	int i;
	char linebuf[BUFSIZ];
	char *p;

	for (i=0; i <= TRACK_NHOURS; ++i)
	{
		FILE *fp=open_track_file(trackdir, curTime - i);

		if (fp == NULL)
			continue;

		while (fgets(linebuf, sizeof(linebuf), fp))
		{
			p=strchr(linebuf, '\n');
			if (p) *p=0;

			p=strchr(linebuf, ' ');
			if (!p)
				continue;
			++p;

			if (*p)
			{
				int rc=(*cb_func)(libmail_atotime_t(linebuf),
						  *p, p+1, voidarg);

				if (rc)
				{
					fclose(fp);
					return rc;
				}
			}
		}
		fclose(fp);
	}

	return (0);
}

int track_read_email(int (*cb_func)(time_t timestamp, int status,
				    const char *address, void *voidarg),
		     void *voidarg)
{
	return track_read(TRACKDIR, cb_func, email_address_statuses, voidarg);
}


void track_save_verify_success(const char *trackdir,
			       const char *address,
			       int autopurge)
{
	track_save_record(trackdir, address, TRACK_VERIFY_SUCCESS, domainlower,
			  autopurge);
}

void track_save_verify_hardfail(const char *trackdir, const char *address,
			       int autopurge)
{
	track_save_record(trackdir,
			  address, TRACK_VERIFY_HARDFAIL, domainlower,
			  autopurge);
}

static const char verify_statuses[]={
	TRACK_VERIFY_SUCCESS, TRACK_VERIFY_HARDFAIL, 0
};

int track_find_verify(const char *trackdir,
		      const char *address, time_t *timestamp)
{
	return track_find_record(trackdir, address, timestamp, verify_statuses,
				 domainlower);
}

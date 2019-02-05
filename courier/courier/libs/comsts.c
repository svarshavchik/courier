/*
** Copyright 2019 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"comsts.h"
#include	"rfc1035/rfc1035.h"
#include	"waitlib/waitlib.h"
#include	"liblock/liblock.h"
#include	"wget.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#if	HAVE_FCNTL_H
#include	<fcntl.h>
#endif
#include	<dirent.h>
#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<errno.h>

static char *policy_filename_for_domain(const char *domain);
static FILE *open_and_lock_cached_policy_file(const char *filename,
					      int *readwrite);
static int load(struct sts_id *id, FILE *fp);

static int get_new_policy(struct sts_id *id,
			  const char *domain);
static void save(struct sts_id *id, FILE *cached_policy_fp);

static int sts_init2(struct sts_id *id,
		     const char *domain,
		     const char *policy_filename,
		     FILE *cached_policy_fp,
		     int readwrite,
		     void *ignore);

extern int sts_cache_size();

static void sts_init1(struct sts_id *id, const char *domain,
		      int (*process)(struct sts_id *id,
				     const char *domain,
				     const char *policy_filename,
				     FILE *cached_policy_fp,
				     int readwrite,
				     void *arg),
		      void *arg)
{
	FILE *cached_policy_fp;
	int rc;
	char *policy_filename;
	int readwrite;

	int cache_size=sts_cache_size();
	struct stat stat_buf1;
	struct stat stat_buf2;

	memset(id, 0, sizeof(*id));

	if (!cache_size || !domain)
		return;

	policy_filename=policy_filename_for_domain(domain);

	if (!policy_filename)
		return;

	cached_policy_fp=NULL;

	for(;;)
	{
		if (cached_policy_fp)
			fclose(cached_policy_fp);

		cached_policy_fp=open_and_lock_cached_policy_file
			(policy_filename, &readwrite);

		if (cached_policy_fp == NULL)
			break;

		if (stat(policy_filename, &stat_buf1) < 0)
			continue;

		if (fstat(fileno(cached_policy_fp), &stat_buf2) == 0 &&
		    (stat_buf1.st_dev != stat_buf2.st_dev ||
		     stat_buf1.st_ino != stat_buf2.st_ino))
			continue;

		break;
	}

	rc=(*process)(id, domain, policy_filename, cached_policy_fp,
		      readwrite, arg);

	if (cached_policy_fp)
	{
		if (rc && readwrite)
			unlink(policy_filename);
		fclose(cached_policy_fp);
	}

	free(policy_filename);
	if (rc)
		sts_deinit(id);
}

void sts_init(struct sts_id *id, const char *domain)
{
	sts_init1(id, domain, sts_init2, 0);
}

void sts_policy_purge(const char *domain)
{
	char  *policy_filename=policy_filename_for_domain(domain);

	if (!policy_filename)
		return;

	if (unlink(policy_filename) < 0)
		perror(policy_filename);

	free(policy_filename);
}

static int sts_policy_override2(struct sts_id *id,
				const char *domain,
				const char *policy_filename,
				FILE *cached_policy_fp,
				int readwrite,
				void *mode_ptr);

void sts_policy_override(const char *domain, const char *mode_str)
{
	enum sts_mode mode;
	struct sts_id dummy;

	if (strcmp(mode_str, "none") == 0)
		mode=sts_mode_none;
	else if (strcmp(mode_str, "testing") == 0)
		mode=sts_mode_testing;
	else if (strcmp(mode_str, "enforce") == 0)
		mode=sts_mode_enforce;
	else
	{
		printf("Unknown STS policy: %s\n", mode_str);
		return;
	}
	sts_init1(&dummy, domain, sts_policy_override2, &mode);
	sts_deinit(&dummy);
}

static int sts_policy_override2(struct sts_id *id,
				const char *domain,
				const char *policy_filename,
				FILE *cached_policy_fp,
				int readwrite,
				void *mode_ptr)
{
	char *new_policy;

	if (!cached_policy_fp || load(id, cached_policy_fp))
	{
		printf("%s: cached policy not found.\n", domain);
	}
	else if (!readwrite)
	{
		printf("%s: no write permission.\n", policy_filename);
	}
	else if ((new_policy=courier_malloc(strlen(id->policy)+
					    strlen(domain)+1024)) != 0)
	{
		char *put_p=new_policy;
		const char *p;
		char lastc='\n';
		int skipping=0;

		for (p=id->policy; *p; p++)
		{
			if (lastc == '\n')
				skipping=strncmp(p, "mode:", 5) == 0 ||
					strncmp(p, "info:", 5) == 0;

			if (!skipping)
				*put_p++=*p;

			lastc=*p;
		}
		if (lastc != '\n')
			*put_p++='\n';

		sprintf(put_p,
			"info: this policy has been manually overridden, "
			"as follows\n"
			"info: use \"testmxlookup --sts-purge %s\" to "
			"restore the default policy:\n",
			domain);

		switch (*(enum sts_mode *)mode_ptr) {
		case sts_mode_none:
			strcat(put_p, "mode: none\n");
			break;
		case sts_mode_testing:
			strcat(put_p, "mode: testing\n");
			break;
		case sts_mode_enforce:
			strcat(put_p, "mode: enforce\n");
			break;
		}
		free(id->policy);
		id->policy=new_policy;
		save(id, cached_policy_fp);
	}
	return 0;
}

static char *sts_getid(const char *domain);
static char *sts_download(const char *domain);

static unsigned max_age(struct sts_id *id);
static void reset_timestamp(struct sts_id *id);

static int sts_init2(struct sts_id *id,
		     const char *domain,
		     const char *policy_filename,
		     FILE *cached_policy_fp,
		     int readwrite,
		     void *ignore)
{
	int rc;

	if (cached_policy_fp)
	{
		if (load(id, cached_policy_fp) == 0)
		{
			char *new_id;
			char *new_policy;

			time_t now=time(NULL);

			if (now >= id->timestamp && now < id->expiration)
			{
				/*
				** Half-way through, and we can update?
				*/

				if (!readwrite)
					return 0;

				/*
				** Wait until the temporary failure stub
				** expires. Don't try again.
				*/

				if (id->tempfail)
					return 0;

				if (now < id->timestamp + (id->expiration-
							   id->timestamp)/2)
					return 0;

				new_id=sts_getid(domain);
				if (!new_id)
					return 0;

				if (strcmp(new_id, id->id) == 0)
				{
					free(new_id);

					/*
					** Policy ID does not change.
					** We update the loaded timestamp,
					** and rewrite the "1\n" at the
					** beginning of the cached file in
					** order to update its timestamp.
					*/

					reset_timestamp(id);

					if (fseek(cached_policy_fp,
						  0L, SEEK_SET) == 0)
					{
						fprintf(cached_policy_fp,
							"1\n");
						fflush(cached_policy_fp);
					}

					return 0;
				}

				new_policy=sts_download(domain);

				if (!new_policy)
				{
					free(new_id);
					return 0;
				}

				free(id->id);
				free(id->policy);
				id->id=new_id;
				id->policy=new_policy;
				reset_timestamp(id);
				save(id, cached_policy_fp);

				return 0;
			}
		}
	}

	rc=get_new_policy(id, domain);

	if (rc == 0)
		reset_timestamp(id);

	if (cached_policy_fp && readwrite && rc == 0)
		save(id, cached_policy_fp);

	return rc;
}

static void reset_timestamp(struct sts_id *id)
{
	id->timestamp=time(NULL);
	id->expiration=id->timestamp+max_age(id);
}

static char *policy_filename_for_domain(const char *domain)
{
	char *filename;
	char *p;

	if (strchr(domain, '/'))
		return 0;

	if ((filename=courier_malloc(strlen(domain)+sizeof(STSDIR "/")))
	    == NULL)
		return 0;

	strcat(strcpy(filename, STSDIR "/"), domain);

	for (p=filename+sizeof(STSDIR); *p; ++p)
		if (*p >= 'A' && *p <= 'Z')
		*p += 'a'-'A';

	return filename;
}

static FILE *open_and_lock_cached_policy_file(const char *filename,
					      int *readwrite)
{
	int fd;
	int save_errno;

	*readwrite=1;
	fd=open(filename, O_RDWR | O_CREAT, 0644);

	if (fd >= 0 && ll_lock_ex(fd) >= 0)
	{
		FILE *fp=fdopen(fd, "r+");

		if (fp)
			return fp;
	}

	/*
	** Non-root will not be able to write to the cache.
	**
	** Do not pollute terminal output, but if this is run from the
	** script let's bark this somewhere where someone will hopefully
	** notice this.
	*/
	save_errno=errno;
	if (!isatty(2))
	{
		errno=save_errno;
		perror(filename);
	}
	if (fd >= 0)
		close(fd);

	*readwrite=0;
	fd=open(filename, O_RDONLY);

	if (fd >= 0 && ll_lockfd(fd, ll_readlock|ll_whence_start|ll_wait,
				 0, 0) >= 0)
	{
		FILE *fp=fdopen(fd, "r");

		if (fp)
			return fp;
	}

	if (fd >= 0)
		close(fd);
	return NULL;
}

static int get_new_policy(struct sts_id *id,
			  const char *domain)
{
	char *new_id=sts_getid(domain);

	if (new_id && id->id && id->policy &&
	    strcmp(new_id, id->id) == 0)
		return 0; // We can still use the cached policy.

	sts_deinit(id);

	if (!new_id)
		return -1;

	if ((id->policy=sts_download(domain)) == NULL)
	{
		if ((id->policy=courier_strdup
		     ("version: version: STSv1\n"
		      "mode: none\n"
		      "max_age: 86400\n"
		      "info: this is a temporary cache entry that occured when"
		      " retrieving\n"
		      "info: this domain's STS policy. Another attempt will be"
		      " made after\n"
		      "info: this cache entry expires.\n"
		      ))
		    == NULL)
		{
			free(new_id);
			return -1;
		}
		id->tempfail=1;
	}
	id->id=new_id;
	return 0;
}

static char *sts_getid1();

/*
** TXT lookup.
**
** Returns malloc-ed id from the looked up TXT record, if there is one.
**
**/

static char *sts_getid(const char *domain)
{
	struct rfc1035_res res;
	struct rfc1035_reply *replyp;
	int n;
	char lookup_domain[RFC1035_MAXNAMESIZE];
	char txtbuf[256];
	char *id_str=NULL;

	/* Domain cannot be too long */

	if (strlen(domain) >= RFC1035_MAXNAMESIZE-10)
		return 0;

	strcat(strcpy(lookup_domain, "_mta-sts."), domain);

	rfc1035_init_resolv(&res);

	(void)rfc1035_resolve_cname(&res,
				    lookup_domain,
				    RFC1035_TYPE_TXT,
				    RFC1035_CLASS_IN, &replyp, 0);

	if (!replyp)
	{
		rfc1035_destroy_resolv(&res);
		return 0;
	}

	for (n=0; (n=rfc1035_replysearch_an(&res,
					    replyp,
					    lookup_domain, RFC1035_TYPE_TXT,
					    RFC1035_CLASS_IN, n)) >= 0; ++n)
	{
		char *p;

		rfc1035_rr_gettxt(replyp->allrrs[n], 0, txtbuf);

		p=strtok(txtbuf, "; ");

		if (!p || strcmp(p, "v=STSv1"))
			continue;

		if (id_str)
		{
			/* RFC 8461 prohibits two or more. */

			free(id_str);
			id_str=NULL;
			break;
		}

		id_str=sts_getid1();

		if (!id_str)
			break;
	}

	rfc1035_replyfree(replyp);
	rfc1035_destroy_resolv(&res);

	return id_str;
}

/*
** Strtok-parse the TXT record, returning the malloc-ed ID value.
*/

static char *sts_getid1()
{
	char *p;
	char *id_str=NULL;

	while ((p=strtok(NULL, "; ")) != 0)
	{
		if (strncmp(p, "id=", 3))
			continue;

		if (id_str) /* RFC 8461 prohibits two or more. */
		{
			free(id_str);
			return 0;
		}

		if ((id_str=courier_strdup(p+3)) == NULL)
			break;
	}

	return id_str;
}

void sts_deinit(struct sts_id *id)
{
	if (id->id)
		free(id->id);
	if (id->policy)
		free(id->policy);
	memset(id, 0, sizeof(*id));
}

static char *sts_download1(const char *url);
static char *find_in_policy(struct sts_id *id,
			    const char *header);

/*
** Run wget to download the domain's policy.
**
** mallocs the well-known URL based on the domain, calls sts_download1()
** to download the URL, frees the URL.
*/

static char *sts_download(const char *domain)
{
	char *url=courier_malloc(strlen(domain)+100);
	char *policy;
	char *v;
	struct sts_id temp_sts;

	if (!url)
		return 0;

	sprintf(url, "https://mta-sts.%s/.well-known/mta-sts.txt", domain);

	policy=sts_download1(url);
	free(url);

	memset(&temp_sts, 0, sizeof(temp_sts));

	temp_sts.policy=policy;

	v=find_in_policy(&temp_sts, "version");

	if (!v || strcmp(v, "STSv1"))
	{
		free(policy);
		policy=0;
	}
	return policy;
}

static int sts_download2(const char *url, pid_t *pidptr);

static char *sts_download3(const char *domain, int pipefd);

/*
** Call sts_download2() to fork off a child process to run wget, returning
** the process pid and the pipe from the process's stdout.
**
** Call sts_download3() to read from the pipe.
**
** Close the pipe, wait for the child process to exit.
**
*/

static char *sts_download1(const char *url)
{
	int fd;
	pid_t pid;
	int waitstat;
	pid_t child_pid;

	char *policy;

	fd=sts_download2(url, &pid);

	if (fd < 0)
		return 0;

	policy=sts_download3(url, fd);

	close(fd);

	while ((child_pid=wait(&waitstat)) >= 0)
	{
		if (child_pid == pid)
		{
			if (WIFEXITED(waitstat) &&
			    WEXITSTATUS(waitstat) == 0)
			{
				return policy;
			}
			break;
		}
	}

	/* Non zero-exit code from wget. Ignore the read policy. */

	if (policy)
		free(policy);
	return 0;
}


static int sts_download2(const char *url, pid_t *pidptr)
{
	int pipefd[2];

	if (pipe(pipefd) < 0)
	{
		perror("pipe");
		return -1;
	}

	*pidptr=fork();

	if (*pidptr < 0)
	{
		perror("fork");

		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}

	if (*pidptr == 0)
	{
		close(1);
		dup(pipefd[1]);
		close(pipefd[1]);
		close(pipefd[0]);

		execl(WGET, WGET, "-O", "-",
		      "--timeout", "60",
		      "-q", url, NULL);
		perror(WGET);
		exit(1);
	}
	close(pipefd[1]);

	return pipefd[0];
}

static char *sts_download3(const char *url, int pipefd)
{
	size_t bufsize=1024 * 64;   /* Specified in RFC 8461 */
	char *buffer=malloc(bufsize);
	char *smaller_buffer;
	size_t i, j;

	if (!buffer)
		return 0;

	i=0;

	for (;;)
	{
		ssize_t n;

		if (i >= bufsize-1)
		{
			fprintf(stderr, "Too much data read from %s\n",
				url);
			break;
		}

		n=read(pipefd, buffer+i, bufsize-1-i);

		if (n < 0)
		{
			perror(url);
			break;
		}

		if (n > 0)
		{
			i += n;
			continue;
		}

		j=0;
		for (n=0; n<i; n++)
		{
			if (buffer[n] == '\r')
				continue;
			if ((buffer[j++]=buffer[n]) == 0)
				buffer[j-1]=' ';
		}
		buffer[j]=0;

		smaller_buffer=courier_strdup(buffer);
		free(buffer);
		return smaller_buffer;
	}

	free(buffer);
	return 0;
}

static void save(struct sts_id *id, FILE *cached_policy_fp)
{
	if (fseek(cached_policy_fp, 0, SEEK_SET) < 0 ||
	    ftruncate(fileno(cached_policy_fp), 0) < 0)
		return;

	if (fprintf(cached_policy_fp, "0\nid:%s\n\n%s", id->id, id->policy) < 0
	    ||
	    fseek(cached_policy_fp, 0L, SEEK_SET) < 0 ||
	    fprintf(cached_policy_fp, "1\n") < 0)
		return;

	fflush(cached_policy_fp);
}

static char *policy_line;
static char policy_line_buffer[1024];

static void open_policy(struct sts_id *id)
{
	policy_line=id->policy;
}

static char *next_policy_field()
{
	size_t i=0;

	if (!policy_line || !*policy_line)
		return 0;

	while (policy_line && *policy_line)
	{
		if (*policy_line == '\n')
		{
			++policy_line;
			break;
		}
		if (i < sizeof(policy_line_buffer)-1)
			policy_line_buffer[i++]=*policy_line;
		++policy_line;
	}
	policy_line_buffer[i]=0;

	return strtok(policy_line_buffer, ": \r\n");
}

static char *find_in_policy(struct sts_id *id,
			    const char *header)
{
	char *p;

	open_policy(id);

	while ((p=next_policy_field()) != 0)
	{
		if (strcmp(p, header) == 0)
		{
			return strtok(NULL, " \n");
		}
	}
	return NULL;
}

static unsigned max_age(struct sts_id *id)
{
	char *p=find_in_policy(id, "max_age");

	if (p)
		return atoi(p);

	return 600;
}

enum sts_mode get_sts_mode(struct sts_id *id)
{
	char *p=find_in_policy(id, "mode");

	if (p)
	{
		if (strcmp(p, "testing") == 0)
			return sts_mode_testing;

		if (strcmp(p, "enforce") == 0)
			return sts_mode_enforce;
	}
	return sts_mode_none;
}

int sts_mx_validate(struct sts_id *id, const char *domainname)
{
	char *field;

	open_policy(id);

	while ((field=next_policy_field(id)) != 0)
	{
		if (strcmp(field, "mx"))
			continue;
		field=strtok(NULL, " \r\n");

		if (strncmp(field, "*.", 2) == 0)
			++field; /* config_domaincmp convention */

		if (config_domaincmp(domainname, field, strlen(field)) == 0)
		    return 0;
	}

	return -1;
}

static int load2(struct sts_id *id, FILE *fp);

static int load(struct sts_id *id, FILE *fp)
{
	int rc;

	rc=load2(id, fp);

	id->cached=1;
	if (rc)
		sts_deinit(id);
	return rc;
}

static int load2(struct sts_id *id, FILE *fp)
{
	char buffer[1024];
	char *p;
	long pos;
	struct stat stat_buf;
	size_t s;

	if (!fgets(buffer, sizeof(buffer), fp) ||
	    strcmp(buffer, "1\n"))
		return -1;

	if (id->id)
	{
		free(id->id);
		id->id=0;
	}

	id->tempfail=0;
	while (fgets(buffer, sizeof(buffer), fp))
	{
		if (strcmp(buffer, "\n") == 0)
			break;

		if (strcmp(buffer, "tempfail") == 0)
		{
			id->tempfail=1;
		}

		if (strcmp(strtok(buffer, ": "), "id"))
			continue;

		if ((p=strtok(NULL, ": \r\n")) == 0)
			p="";

		if ((id->id=courier_strdup(p)) == 0)
			break;
	}

	if (!id->id)
		return -1;

	while (strcmp(buffer, "\n"))
	{
		if (fgets(buffer, sizeof(buffer), fp) == 0)
			return -1;
	}

	if ((pos=ftell(fp)) < 0 || fstat(fileno(fp), &stat_buf) < 0)
		return -1;

	s=stat_buf.st_size-pos;

	if ((id->policy=courier_malloc(s+1)) == NULL ||
	    fread(id->policy, s, 1, fp) != 1)
		return -1;

	id->policy[s]=0;
	id->timestamp=stat_buf.st_mtime;
	id->expiration=id->timestamp+max_age(id);

	return 0;
}

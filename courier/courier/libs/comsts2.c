/*
** Copyright 2019 S. Varshavchik.
** See COPYING for distribution information.
*/

#include	"courier.h"
#include	"comsts.h"
#include	<dirent.h>
#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<sys/stat.h>

int sts_cache_size()
{
	FILE *fp=fopen(STSDIR "/.cache_size", "r");
	char buffer[1024];
	int n;

	strcpy(buffer, "1000"); /* Default cache size */

	if (fp)
	{
		if (fgets(buffer, sizeof(buffer), fp) == NULL)
		{
			strcpy(buffer, "1000"); /* Default cache size */
		}

		fclose(fp);
	}
	n=atoi(buffer);

	if (n < 0)
		n=0;

	return n;
}

void sts_cache_enable()
{
	if (unlink(STSDIR "/.cache_size"))
		perror(STSDIR);
}

void sts_cache_size_set(int n)
{
	FILE *fp=fopen(STSDIR "/.cache_size", "w");

	if (!fp || fprintf(fp, "%d\n", n) < 0 || fflush(fp) < 0 || fclose(fp))
		perror(STSDIR);
}

void sts_cache_disable()
{
	sts_cache_size_set(0);
}

struct sts_file_list {
	struct sts_file_list *next;
	char *filename;
	time_t timestamp;
};

static int sort_by_timestamp(const void *a, const void *b)
{
	const struct sts_file_list *const *fa=a, * const *fb=b;

	return (*fa)->timestamp < (*fb)->timestamp ? -1
		: (*fa)->timestamp > (*fb)->timestamp ? 1
		: 0;
}

int sts_expire()
{
	struct sts_file_list *list=0;
	DIR *dirp;
	struct dirent *de;
	size_t cnt=0;
	size_t i;
	size_t max_size=sts_cache_size();
	struct sts_file_list **array;

	if (max_size == 0)
		return max_size;

	dirp=opendir(STSDIR);

	if (!dirp)
		return max_size;

	while ((de=readdir(dirp)) != 0)
	{
		struct sts_file_list *n;

		if (de->d_name[0] == '.')
			continue;

		n=courier_malloc(sizeof(struct sts_file_list));

		if (!n)
			break;

		if ((n->filename=courier_malloc(sizeof(STSDIR "/") +
						strlen(de->d_name))) == 0)
		{
			free(n);
			break;
		}
		strcat(strcpy(n->filename, STSDIR "/"), de->d_name);
		n->next=list;
		list=n;
		++cnt;
	}
	closedir(dirp);

	if (cnt <= max_size)
		return max_size;

	if ((array=calloc(cnt, sizeof(*array))) != 0)
	{
		struct sts_file_list *p;

		cnt=0;

		for (p=list; p; p=p->next)
		{
			array[cnt]=p;
			++cnt;
		}

		for (i=0; i<cnt; ++i)
		{
			struct stat stat_buf;

			if (stat(array[i]->filename, &stat_buf) < 0)
			{
				array[i]->filename[0]=0;
				stat_buf.st_mtime=0;
			}
			array[i]->timestamp=stat_buf.st_mtime;
		}

		qsort(array, cnt, sizeof(*array), sort_by_timestamp);

		for (i=0; i<cnt-max_size; ++i)
		{
			if (array[i]->filename[0]) /* stat() error, above */
				unlink(array[i]->filename);
		}

		free(array);
	}

	while (list)
	{
		struct sts_file_list *p;

		p=list->next;
		free(list->filename);
		free(list);
		list=p;
	}

	return max_size;
}

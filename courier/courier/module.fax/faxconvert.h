/*
** Copyright 2002 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	faxconvert_h
#define	faxconvert_h

#include	"config.h"

struct sort_file_list {
	struct sort_file_list *next;
	char *filename;
} ;

struct sort_file_list *read_dir_sort_filenames(const char *,
					       const char *);

void rmdir_contents(const char *);

#endif

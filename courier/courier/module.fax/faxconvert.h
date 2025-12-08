/*
** Copyright 2002 S. Varshavchik.
** See COPYING for distribution information.
*/

#ifndef	faxconvert_h
#define	faxconvert_h

#include	"config.h"
#include	<vector>
#include	<string>

#ifndef FILTERBINDIR
#include "filterbindir.h"
#endif

#ifndef FAXTMPDIR
#include "faxtmpdir.h"
#endif

#define SUBTMPDIR FAXTMPDIR "/.tmp"

std::vector<std::string> read_dir_sort_filenames(const char *,
						 const char *);

int courierfax_start_filter(const char *, pid_t *);
int courierfax_end_filter(const char *, pid_t);

void rmdir_contents(const char *);

void courierfax(struct moduledel *);

void courierfax_failed(unsigned, struct ctlfile *, unsigned, int, const char *);
void invoke_sendfax(char **);

#endif

/*
** Copyright 1998 - 1999 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"courier_lib_config.h"
#endif

#include	"courier.h"

#include	<sys/types.h>

#if HAVE_STATVFS
#include	<sys/statvfs.h>
#define	DOIT
#else

#if HAVE_STATFS

#if HAVE_SYS_STATFS_H
#include	<sys/statfs.h>
#else

#if HAVE_SYS_PARAM_H
#include	<sys/param.h>
#endif

#if HAVE_SYS_MOUNT_H
#include	<sys/mount.h>
#endif
#endif




#define	DOIT
#else
int config_statfs(const char *ptr, unsigned long *nblocks,
	unsigned long *ninodes, unsigned *blksize)
{
	return (-1);
}
#endif
#endif

#ifdef	DOIT

int freespace(const char *ptr,
	unsigned long *tblocks, unsigned long *nblocks,
	unsigned long *tinodes, unsigned long *ninodes,
	unsigned *blksize)
{
#if HAVE_STATVFS
struct statvfs buf;

	if (statvfs(ptr, &buf))	return (-1);

#define	BTOTAL buf.f_blocks
#define	BAVAIL buf.f_bavail
#define	ITOTAL buf.f_files
#define	IAVAIL buf.f_favail

	*blksize=buf.f_bsize;

#else
#if HAVE_STATFS
struct statfs buf;

	if (statfs(ptr, &buf))	return (-1);

#define	BTOTAL buf.f_blocks
#define	BAVAIL buf.f_bfree
#define	ITOTAL buf.f_files
#define	IAVAIL buf.f_ffree

	*blksize=buf.f_bsize;
#endif
#endif

	*tblocks= BTOTAL;
	*nblocks= BAVAIL;
	*tinodes= ITOTAL;
	*ninodes= IAVAIL;

	if (*tblocks != BTOTAL) *tblocks= ~0;	/* Overflow */
	if (*nblocks != BAVAIL) *nblocks= ~0;	/* Overflow */
	if (*tinodes != ITOTAL) *tinodes= ~0;	/* Overflow */
	if (*ninodes != IAVAIL) *ninodes= ~0;	/* Overflow */

	return (0);
}
#endif

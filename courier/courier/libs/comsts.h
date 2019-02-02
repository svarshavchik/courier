/*
** Copyright 2019 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef	comsts_h
#define	comsts_h

#include "localstatedir.h"
#if TIME_WITH_SYS_TIME
#include	<sys/time.h>
#include	<time.h>
#else
#if HAVE_SYS_TIME_H
#include	<sys/time.h>
#else
#include	<time.h>
#endif
#endif

#ifdef	__cplusplus
extern "C" {
#endif
#if 0
}
#endif

#define STSDIR LOCALSTATEDIR "/sts"

/*
** STS policy for a domain.
**
** Loaded by sts_init(). If the domain does not have a policy, the id and
** policy values are null pointers.
*/

struct sts_id {
	char *id;
	char *policy;
	int cached;
	int tempfail;
	time_t timestamp;
	time_t expiration;
};

/*
** Download the STS policy for the domain and cache it. Use the cached
** policy, if one exists. If the cached policy is more than half to its
** expiration time, check if the domain has published a new policy. If not,
** reset the expiration clock; or download and cache the updated policy.
**
** The sts_id is cleared if there is no published STS policy for the domain,
** or if the cached policy expired (and it gets cleared).
*/

void sts_init(struct sts_id *id, const char *domain);

/*
** Deallocate an sts_id
*/
void sts_deinit(struct sts_id *id);

enum sts_mode {sts_mode_none, sts_mode_testing, sts_mode_enforce};

/*
** Return domain's STS mode.
*/

enum sts_mode get_sts_mode(struct sts_id *id);

/*
** Check if the given hostname is valid as per the STS policy.
**
** Returns 0 if we find a matching "mx" line.
**
** Returns a negative value, if not.
*/
int sts_mx_validate(struct sts_id *id, const char *domainname);

/*
** Expire old cache entries, returns the cache size.
*/

int sts_expire();

/*
** Manually override the domain's policy.
**
** mode: "none", "testing", or "enforce".
*/

void sts_policy_override(const char *domain, const char *mode);

/*
** Manually remove the cached policy for the domain.
*/
void sts_policy_purge(const char *domain);

/*
** Enable the STS cache with the default cache size.
*/

void sts_cache_enable();

/*
** Enable the STS cache and set its size.
*/

void sts_cache_size_set(int);

/*
** Disable the STS cache.
*/
void sts_cache_disable();


#if 0
{
#endif

#ifdef	__cplusplus
}
#endif

#endif

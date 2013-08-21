/*
** Copyright 1998 - 2000 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include	"config.h"
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	"msghash.h"
#include	"duphash.h"


/*

This module implements a rotating FIFO of last X message hashes
seen.  When more than Y hashes have been seen, we have a dupe sign,
and they are cancelled.

*/

void duphash_init(struct duphash *h,
	unsigned n,		/* Size of the FIFO queue */
	unsigned d)		/* How many dupes to trigger cancels */
{
struct duphashinfo *di;
int	i;

	h->duphashbufsize=n;
	h->duplevel=d;

	if ((h->msgsbuf=(struct duphashmsginfo *)
		malloc( sizeof(struct duphashmsginfo)*n)) == 0 ||
		(h->hashesbuf=(struct duphashinfo *)
			malloc(sizeof(struct duphashinfo)*n)) == 0)
	{
		perror("malloc");
		exit(1);
	}

	h->head=h->tail=0;
	h->freelist=0;
	for (i=0; i<n; i++)
	{
		di= h->hashesbuf + i;
		di->next=h->freelist;
		h->freelist=di;
	}

	for (i=0; i < 256; i++)
		h->hashbuckets[i]=0;
}

/* Remove the oldest msg from the circular queue */

static void dup_poptail(struct duphash *p)
{
struct duphashmsginfo *msgp=p->msgsbuf + p->tail;
struct duphashinfo *hashp=msgp->hash;

	/* Unlink msg from its hash */

	if (msgp->prev)	msgp->prev->next=msgp->next;
	else	hashp->firstmsg=msgp->next;

	if (msgp->next)	msgp->next->prev=msgp->prev;
	else	hashp->lastmsg=msgp->prev;

	/* If this msg was an intentional dupe, account for it no longer
	** being in the FIFO
	*/

	if (msgp->dupmsg)
		--hashp->ndupmsgs;

	if ( --hashp->nmsgs == 0 )	/* No other msgs with same hash */
	{
		/* Remove the hash from its hashbucket */

		if (hashp->prev)
			hashp->prev->next=hashp->next;
		else
			p->hashbuckets[hashp->md5[0] & 255]=hashp->next;
		if (hashp->next)
			hashp->next->prev=hashp->prev;

		/* Return the unused hash to the freelist */
		hashp->next=p->freelist;
		p->freelist=hashp;
	}
	p->tail = (p->tail+1) % p->duphashbufsize;
}

/* Add a new md5 hash to the circular queue */

static struct duphashmsginfo *dup_addhead(struct duphash *p, MD5_DIGEST *d,
	int is_submit_dupe)	/* This is an intentional dupe */
{
unsigned bucket=(*d)[0] & 255;
struct duphashmsginfo *msgp;
struct duphashinfo *hashp;
unsigned nexthead= (p->head+1) % p->duphashbufsize;

	/* If queue is full, pop one msg */

	if (nexthead == p->tail)
		dup_poptail(p);

	/* Search the hashbucket for this hash */

	for (hashp= p->hashbuckets[bucket]; hashp; hashp=hashp->next)
		if (memcmp(&hashp->md5, d, sizeof(hashp->md5)) == 0)
			break;
	if (!hashp)
	{
		/* Not found, remove an unused hash from the freelist */

		hashp=p->freelist;
		p->freelist=hashp->next;

		/* Attach the new hash to its hashbucket */

		hashp->prev=0;
		hashp->next= p->hashbuckets[bucket];
		p->hashbuckets[bucket]=hashp;

		if (hashp->next)
			hashp->next->prev=hashp;

		/* Initialize empty hash */
		memcpy(&hashp->md5, (*d), sizeof(hashp->md5));
		hashp->firstmsg=0;
		hashp->lastmsg=0;
		hashp->nmsgs=0;
		hashp->ndupmsgs=0;
	}

	msgp=p->msgsbuf + p->head;

	/* attach msg to its hash */

	msgp->hash=hashp;
	++hashp->nmsgs;
	msgp->dupmsg=0;
	if (is_submit_dupe)
	{
		msgp->dupmsg=1;
		++hashp->ndupmsgs;
	}
	msgp->next=hashp->firstmsg;
	msgp->prev=0;

	if (hashp->firstmsg)
		hashp->firstmsg->prev=msgp;
	else
		hashp->lastmsg=msgp;
	hashp->firstmsg=msgp;
	p->head=nexthead;
	return (msgp);
}

int duphash_check(struct duphash *p, MD5_DIGEST *d, const char *msgid,
	int is_submit_dupe, void (*cancelfunc)(const char *))
{
struct duphashmsginfo *msgp=dup_addhead(p, d, is_submit_dupe);
int	cancelled=0;

	/* Finish initialized msgp */

	msgp->cancelled=0;
	msgp->msgid[0]=0;
	strncat(msgp->msgid, msgid, sizeof(msgp->msgid)-1);

	if ((msgp->next && msgp->next->cancelled) ||
		(msgp->prev && msgp->prev->cancelled))
	{
		msgp->cancelled=cancelled=1;	/* Message being cancelled */
	}
	else if (msgp->hash->nmsgs -
		msgp->hash->ndupmsgs >= p->duplevel) /* New dupe detected */
	{
		msgp->cancelled=cancelled=1;
		for (msgp=msgp->hash->firstmsg; msgp; msgp=msgp->next)
		{
			if (!msgp->cancelled)
				(*cancelfunc)(msgp->msgid);
		}
	}
	return (cancelled);
}

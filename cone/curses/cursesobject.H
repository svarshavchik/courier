/*
** Copyright 2002, Double Precision Inc.
**
** See COPYING for distribution information.
*/

#ifndef cursesobject_H
#define cursesobject_H

///////////////////////////////////////////////////////////////////////////
//
// Helper class that detects when the object being pointed to is destroyed.
//
// Subclass cursesObj.  Declare CursesPtr<T>, where T is cursesObj's
// subclass.  After CursesObj is destroyed, CursesPtr<T>::operator T *()
// will return NULL.
//
// The cursesObj object maintains a list of all CursesPtr<T>s that point to
// it.  cursesObj's destructor nulls them out.

#include "../curses/curses_config.h"
#include <set>
#include <cstdio>

class cursesPtrBase {
public:
	cursesPtrBase();
	virtual ~cursesPtrBase();
	virtual void ptrDestroyed()=0;
};

template<class T> class cursesPtr : public cursesPtrBase {

	T *ptr;

public:
	cursesPtr(T *ptrArg) : ptr(NULL)
	{
		if (ptrArg)
			ptrArg->cursesBase.insert(this);
		ptr=ptrArg;
	}

	cursesPtr(const cursesPtr &o) : ptr(NULL)
	{
		(*this)=o;
	}

	cursesPtr &operator=(const cursesPtr &o)
	{
		if (o.ptr == NULL ||
		    o.ptr->cursesBase.count(this) == 0)
		{
			if (o.ptr)
				o.ptr->cursesBase.insert(this);

			if (ptr && ptr->cursesBase.count(this) > 0)
				ptr->cursesBase.erase(ptr->cursesBase
							 .find(this));
		}
		ptr=o.ptr;

		return *this;
	}

	~cursesPtr()
	{
		if (ptr && ptr->cursesBase.count(this) > 0)
			ptr->cursesBase.erase(ptr->cursesBase
						 .find(this));
	}

	operator T *() const
	{
		return ptr;
	}

	T * operator->() const
	{
		return ptr;
	}

	bool isDestroyed() const { return ptr == 0; }

	void ptrDestroyed() { ptr=NULL; }

};

class CursesObj {

public:
	std::set<cursesPtrBase *> cursesBase;

	CursesObj();
	virtual ~CursesObj();

	// UNDEFINED:
	CursesObj(const CursesObj &);
	CursesObj &operator=(const CursesObj &);
};

#endif

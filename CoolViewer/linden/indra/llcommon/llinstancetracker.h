/**
 * @file llinstancetracker.h
 * @brief LLInstanceTracker is a mixin class that automatically tracks object
 *		instances with or without an associated key
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LLINSTANCETRACKER_H
#define LL_LLINSTANCETRACKER_H

#include <atomic>
#include <map>
#include <typeinfo>

#include "boost/iterator/transform_iterator.hpp"
#include "boost/iterator/indirect_iterator.hpp"

#include "llfastset.h"
#include "llstringtable.h"

/**
 * Base class manages "class-static" data that must actually have singleton
 * semantics: one instance per process, rather than one instance per module as
 * sometimes happens with data simply declared static.
 */
class LL_COMMON_API LLInstanceTrackerBase
{
protected:
	// It is not essential to derive your STATICDATA (for use with getStatic())
	// from StaticBase; it is just that both known implementations do.
	struct LL_COMMON_API StaticBase
	{
		StaticBase()
		:	sIterationNestDepth(0)
		{
		}

		void incrementDepth();
		void decrementDepth();
		U32 getDepth();

	private:
		std::atomic<S32> sIterationNestDepth;
	};
};

enum EInstanceTrackerAllowKeyCollisions
{
	InstanceTrackerAllowKeyCollisions,
	InstanceTrackerDisallowKeyCollisions
};

// This mix-in class adds support for tracking all instances of the specified
// class parameter T. The (optional) key associates a value of type KEY with a
// given instance of T, for quick lookup. If KEY is not provided, then
// instances are stored into a simple unordered_set.
// NOTE: see explicit specialization below for default KEY==void case
// NOTE: this class is not thread-safe unless used as read-only
template<typename T, typename KEY = void,
		 EInstanceTrackerAllowKeyCollisions ALLOW_KEY_COLLISIONS =
			InstanceTrackerDisallowKeyCollisions>
class LLInstanceTracker : public LLInstanceTrackerBase
{
	typedef LLInstanceTracker<T, KEY> self_t;
	typedef typename std::multimap<KEY, T*> InstanceMap;

	struct StaticData : public StaticBase
	{
		InstanceMap sMap;
	};

	static StaticData& getStatic()				{ static StaticData sData; return sData; }
	static InstanceMap& getMap_()				{ return getStatic().sMap; }

public:
	class instance_iter : public boost::iterator_facade<instance_iter, T,
														boost::forward_traversal_tag>
	{
	public:
		typedef boost::iterator_facade<instance_iter, T,
									   boost::forward_traversal_tag> super_t;

		LL_INLINE instance_iter(const typename InstanceMap::iterator& it)
		:	mIterator(it)
		{
			getStatic().incrementDepth();
		}

		LL_INLINE ~instance_iter()
		{
			getStatic().decrementDepth();
		}

	private:
		friend class boost::iterator_core_access;

		LL_INLINE void increment()				{ ++mIterator; }

		LL_INLINE bool equal(instance_iter const& other) const
		{
			return mIterator == other.mIterator;
		}

		LL_INLINE T& dereference() const
		{
			return *(mIterator->second);
		}

		typename InstanceMap::iterator mIterator;
	};

	class key_iter : public boost::iterator_facade<key_iter, KEY,
												   boost::forward_traversal_tag>
	{
	public:
		typedef boost::iterator_facade<key_iter, KEY,
									   boost::forward_traversal_tag> super_t;

		LL_INLINE key_iter(typename InstanceMap::iterator it)
		:	mIterator(it)
		{
			getStatic().incrementDepth();
		}

		LL_INLINE key_iter(const key_iter& other)
		:	mIterator(other.mIterator)
		{
			getStatic().incrementDepth();
		}

		LL_INLINE ~key_iter()
		{
			getStatic().decrementDepth();
		}

	private:
		friend class boost::iterator_core_access;

		LL_INLINE void increment()				{ ++mIterator; }

		LL_INLINE bool equal(key_iter const& other) const
		{
			return mIterator == other.mIterator;
		}

		LL_INLINE KEY& dereference() const
		{
			return const_cast<KEY&>(mIterator->first);
		}

		typename InstanceMap::iterator mIterator;
	};

	static T* getInstance(const KEY& k)
	{
		const InstanceMap& map(getMap_());
		typename InstanceMap::const_iterator found = map.find(k);
		return found == map.end() ? NULL : found->second;
	}

	static instance_iter beginInstances()
	{
		return instance_iter(getMap_().begin());
	}

	static instance_iter endInstances()
	{
		return instance_iter(getMap_().end());
	}

	static S32 instanceCount()					{ return getMap_().size(); }

	static key_iter beginKeys()
	{
		return key_iter(getMap_().begin());
	}

	static key_iter endKeys()
	{
		return key_iter(getMap_().end());
	}

#if 0	// Unused
	LL_INLINE virtual void setKey(KEY key)		{ remove_(); add_(key); }
#endif
	// While iterating over instances, we might want to request the key
	LL_INLINE virtual const KEY& getKey() const	{ return mInstanceKey; }

protected:
	LLInstanceTracker(KEY key)
	{
		// Make sure static data outlives all instances
		getStatic();
		add_(key);
	}

	virtual ~LLInstanceTracker()
	{
		// It is unsafe to delete instances of this type while all instances
		// are being iterated over.
		llassert_always(getStatic().getDepth() == 0);
		remove_();
	}

private:
	LLInstanceTracker(const LLInstanceTracker&);
	const LLInstanceTracker& operator=(const LLInstanceTracker&);

	void add_(KEY key)
	{
		mInstanceKey = key;
		InstanceMap& map = getMap_();
		typename InstanceMap::iterator it = map.lower_bound(key);
		if (ALLOW_KEY_COLLISIONS == InstanceTrackerDisallowKeyCollisions &&
			it != map.end() && it->first == key)
		{
			llerrs << "Key " << key
				   << " already exists in instance map for "
				   << typeid(T).name() << llendl;
		}
		else
		{
			map.insert(it, std::make_pair(key, static_cast<T*>(this)));
		}
	}

	void remove_()
	{
		typename InstanceMap::iterator iter = getMap_().find(mInstanceKey);
		if (iter != getMap_().end())
		{
			getMap_().erase(iter);
		}
	}

private:
	KEY mInstanceKey;
};

// Explicit specialization for default case where KEY is void using an
// unordered set<T*>
template<typename T, EInstanceTrackerAllowKeyCollisions ALLOW_KEY_COLLISIONS>
class LLInstanceTracker<T, void, ALLOW_KEY_COLLISIONS> : public LLInstanceTrackerBase
{
	typedef LLInstanceTracker<T, void> self_t;
	typedef typename fast_hset<T*> InstanceSet;

	struct StaticData: public StaticBase
	{
		InstanceSet sSet;
	};

	static StaticData& getStatic()				{ static StaticData sData; return sData; }
	static InstanceSet& getSet_()				{ return getStatic().sSet; }

public:
	/**
	  * Does a particular instance still exist ? Of course, if you already have
	  * a T* in hand, you need not call getInstance() to locate the instance,
	  * unlike the case where getInstance() accepts some kind of key.
	  * Nonetheless this method is still useful to validate a particular T*,
	  * since each instance's destructor removes itself from the underlying
	  * set.
	 */
	static T* getInstance(T* k)
	{
		const InstanceSet& set(getSet_());
		typename InstanceSet::const_iterator found = set.find(k);
		return found == set.end() ? NULL : *found;
	}

	static S32 instanceCount()					{ return getSet_().size(); }

	class instance_iter : public boost::iterator_facade<instance_iter, T,
														boost::forward_traversal_tag>
	{
	public:
		LL_INLINE instance_iter(const typename InstanceSet::iterator& it)
		:	mIterator(it)
		{
			getStatic().incrementDepth();
		}

		LL_INLINE instance_iter(const instance_iter& other)
		:	mIterator(other.mIterator)
		{
			getStatic().incrementDepth();
		}

		LL_INLINE ~instance_iter()
		{
			getStatic().decrementDepth();
		}

	private:
		friend class boost::iterator_core_access;

		LL_INLINE void increment()				{ ++mIterator; }

		LL_INLINE bool equal(instance_iter const& other) const
		{
			return mIterator == other.mIterator;
		}

		LL_INLINE T& dereference() const
		{
			return **mIterator;
		}

		typename InstanceSet::iterator mIterator;
	};

	static instance_iter beginInstances()		{ return instance_iter(getSet_().begin()); }
	static instance_iter endInstances()			{ return instance_iter(getSet_().end()); }

protected:
	LLInstanceTracker()
	{
		// Make sure static data outlives all instances
		getStatic();
		getSet_().insert(static_cast<T*>(this));
	}

	virtual ~LLInstanceTracker()
	{
		// It is unsafe to delete instances of this type while all instances
		// are being iterated over.
		llassert_always(getStatic().getDepth() == 0);
		getSet_().erase(static_cast<T*>(this));
	}

	LLInstanceTracker(const LLInstanceTracker& other)
	{
		getSet_().insert(static_cast<T*>(this));
	}
};

#endif

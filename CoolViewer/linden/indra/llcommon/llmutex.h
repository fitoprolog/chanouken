/**
 * @file llmutex.h
 * @brief Base classes for mutex and condition handling.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLMUTEX_H
#define LL_LLMUTEX_H

// For now, disable the fibers-aware mutexes unconditionnaly, since they cause
// crashes (weird issue with "Illegal deletion of LLDrawable"). HB
#define LL_USE_FIBER_AWARE_MUTEX 0

// Guard against "comparison between signed and unsigned integer expressions"
// warnings (sometimes seen when compiling with gcc on system with glibc v2.35:
// probably because of the recent changes and integration of pthread in glibc)
// in boost/thread/pthread/thread_data.hpp, itself being included from
// boost/thread/condition_variable.hpp
#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#if !LL_USE_FIBER_AWARE_MUTEX
# include "boost/thread/mutex.hpp"
# include "boost/thread/locks.hpp"
# include "boost/thread/condition_variable.hpp"
# define LL_MUTEX_TYPE boost::mutex
# define LL_UNIQ_LOCK_TYPE boost::unique_lock<boost::mutex>
# define LL_COND_TYPE boost::condition_variable
#else
# include <mutex>
# include "boost/fiber/mutex.hpp"
# include "boost/fiber/condition_variable.hpp"
# define LL_MUTEX_TYPE boost::fibers::mutex
# define LL_UNIQ_LOCK_TYPE std::unique_lock<boost::fibers::mutex>
# define LL_COND_TYPE boost::fibers::condition_variable
#endif

#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

#include "llerror.h"
#include "lltimer.h"		// For ms_sleep()

class LL_COMMON_API LLMutex
{
protected:
	LOG_CLASS(LLMutex);

public:
	typedef enum
	{
		NO_THREAD = 0xFFFFFFFF
	} e_locking_thread;

	LL_INLINE LLMutex()
	:	mCount(0),
		mLockingThread(NO_THREAD)
	{
	}

	LL_INLINE virtual ~LLMutex()
	{
	}

	void lock();				// Blocking
	void unlock();
	bool trylock();				// Non-blocking, returns true if lock held.

	// Undefined behavior when called on mutex not being held:
	bool isLocked();

	bool isSelfLocked();		// Returns true if locked in a same thread

	// Returns the ID of the locking thread
	LL_INLINE U32 lockingThread() const		{ return mLockingThread; }

protected:
	LL_MUTEX_TYPE	mMutex;
	mutable U32		mCount;
	mutable U32		mLockingThread;
	bool			mIsLocalPool;
};

// Actually a condition/mutex pair (since each condition needs to be associated
// with a mutex).
class LL_COMMON_API LLCondition : public LLMutex
{
public:
	LL_INLINE LLCondition()
	{
	}

	LL_INLINE ~LLCondition()
	{
	}

	// This method blocks
	LL_INLINE void wait()
	{
		LL_UNIQ_LOCK_TYPE lock(mMutex);
		mCond.wait(lock);
	}

	LL_INLINE void signal()
	{
		mCond.notify_one();
	}

	LL_INLINE void broadcast()
	{
		mCond.notify_all();
	}

protected:
	LL_COND_TYPE mCond;
};

// Scoped locking class
class LLMutexLock
{
public:
	LL_INLINE LLMutexLock(LLMutex* mutex)
	:	mMutex(mutex)
	{
		if (mMutex)
		{
			mMutex->lock();
		}
	}

	LL_INLINE LLMutexLock(LLMutex& mutex)
	:	mMutex(&mutex)
	{
		mMutex->lock();
	}

	LL_INLINE ~LLMutexLock()
	{
		if (mMutex)
		{
			mMutex->unlock();
		}
	}

private:
	LLMutex* mMutex;
};

// Scoped locking class similar in function to LLMutexLock but uses the
// trylock() method to conditionally acquire lock without blocking. Caller
// resolves the resulting condition by calling the isLocked() method and either
// punts or continues as indicated.
//
// Mostly of interest to callers needing to avoid stalls and that can guarantee
// another attempt at a later time.
class LLMutexTrylock
{
public:
	LL_INLINE LLMutexTrylock(LLMutex* mutex)
	:	mMutex(mutex)
	{
		mLocked = mMutex && mMutex->trylock();
	}

	LL_INLINE LLMutexTrylock(LLMutex& mutex)
	:	mMutex(&mutex)
	{
		mLocked = mMutex->trylock();
	}

	LL_INLINE LLMutexTrylock(LLMutex* mutex, U32 attempts)
	:	mMutex(mutex),
		mLocked(false)
	{
		if (mMutex)
		{
			for (U32 i = 0; i < attempts; ++i)
			{
				mLocked = mMutex->trylock();
				if (mLocked)
				{
					break;
				}
				ms_sleep(10);
			}
		}
	}

	LL_INLINE ~LLMutexTrylock()
	{
		if (mLocked)
		{
			mMutex->unlock();
		}
	}

	LL_INLINE bool isLocked() const				{ return mLocked; }

private:
	LLMutex*	mMutex;
	bool		mLocked;
};

#endif // LL_LLMUTEX_H

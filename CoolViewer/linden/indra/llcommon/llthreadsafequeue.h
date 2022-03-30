/**
 * @file llthreadsafequeue.h
 * @brief Implements a thread (and fiber) safe FIFO.
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
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

#ifndef LL_LLTHREADSAFEQUEUE_H
#define LL_LLTHREADSAFEQUEUE_H

#include <deque>
#include <mutex>
#include <string>

#include "boost/fiber/condition_variable.hpp"

#include "stdtypes.h"

template<typename ElementT>
class LLThreadSafeQueue
{
public:
	typedef ElementT value_type;

	LLThreadSafeQueue(U32 capacity = 1024);

	// Add an element to the front of queue (will block if the queue has
	// reached capacity).
	// This call will raise an interrupt error if the queue is deleted while
	// the caller is blocked.
	void pushFront(const ElementT& element);

	// Try to add an element to the front ofqueue without blocking. Returns
	// true only if the element was actually added.
	bool tryPushFront(const ElementT& element);

	// Pop the element at the end of the queue (will block if the queue is
	// empty).
	// This call will raise an interrupt error if the queue is deleted while
	// the caller is blocked.
	ElementT popBack();

	// Pop an element from the end of the queue if there is one available.
	// Returns true only if an element was popped.
	bool tryPopBack(ElementT& element);

	// Returns the size of the queue.
	size_t size();

private:
	std::deque<ElementT>				mStorage;
	U32									mCapacity;

	boost::fibers::mutex				mLock;
	boost::fibers::condition_variable	mCapacityCond;
	boost::fibers::condition_variable	mEmptyCond;
};

template<typename ElementT>
LLThreadSafeQueue<ElementT>::LLThreadSafeQueue(U32 capacity)
:	mCapacity(capacity)
{
}

template<typename ElementT>
void LLThreadSafeQueue<ElementT>::pushFront(const ElementT& element)
{
	std::unique_lock<decltype(mLock)> lock1(mLock);
	while (true)
	{
		if (mStorage.size() < mCapacity)
		{
			mStorage.push_front(element);
			mEmptyCond.notify_one();
			return;
		}

		// Storage full. Wait for signal.
		mCapacityCond.wait(lock1);
	}
}

template<typename ElementT>
bool LLThreadSafeQueue<ElementT>::tryPushFront(const ElementT& element)
{
	std::unique_lock<decltype(mLock)> lock1(mLock, std::defer_lock);
	if (!lock1.try_lock() || mStorage.size() >= mCapacity)
	{
		return false;
	}

	mStorage.push_front(element);
	mEmptyCond.notify_one();
	return true;
}

template<typename ElementT>
ElementT LLThreadSafeQueue<ElementT>::popBack()
{
	std::unique_lock<decltype(mLock)> lock1(mLock);
	while (true)
	{
		if (!mStorage.empty())
		{
			ElementT value = mStorage.back();
			mStorage.pop_back();
			mCapacityCond.notify_one();
			return value;
		}

		// Storage empty. Wait for signal.
		mEmptyCond.wait(lock1);
	}
}

template<typename ElementT>
bool LLThreadSafeQueue<ElementT>::tryPopBack(ElementT& element)
{
	std::unique_lock<decltype(mLock)> lock1(mLock, std::defer_lock);
	if (!lock1.try_lock() || mStorage.empty())
	{
		return false;
	}

	element = mStorage.back();
	mStorage.pop_back();
	mCapacityCond.notify_one();
	return true;
}

template<typename ElementT>
size_t LLThreadSafeQueue<ElementT>::size()
{
	std::unique_lock<decltype(mLock)> lock(mLock);
	return mStorage.size();
}

#endif	// LL_LLTHREADSAFEQUEUE_H

/**
 * @file llcorehttpreadyqueue.h
 * @brief Internal declaration for the operation ready queue
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef	_LLCORE_HTTP_READY_QUEUE_H_
#define	_LLCORE_HTTP_READY_QUEUE_H_

#include <queue>

#include "llcorehttpinternal.h"
#include "llcorehttpoprequest.h"

namespace LLCore
{

// HttpReadyQueue provides a simple priority queue for HttpOpRequest objects.
//
// This uses the priority_queue adaptor class to provide the queue as well as
// the ordering scheme while allowing us access to the raw container if we
// follow a few simple rules. One of the more important of those rules is that
// any iterator becomes invalid on element erasure. So pay attention.
//
// If LLCORE_HTTP_READY_QUEUE_IGNORES_PRIORITY tests true, the class implements
// a std::priority_queue interface but on std::deque behavior to eliminate
// sensitivity to priority. In the future, this will likely become the only
// behavior or it may become a run-time election.
//
// Threading: not thread-safe. Expected to be used entirely by a single thread,
// typically a worker thread of some sort.

#if LLCORE_HTTP_READY_QUEUE_IGNORES_PRIORITY

typedef std::deque<HttpOpRequest::ptr_t> HttpReadyQueueBase;

#else

typedef std::priority_queue<HttpOpRequest::ptr_t,
							std::deque<HttpOpRequest::ptr_t>,
							LLCore::HttpOpRequestCompare> HttpReadyQueueBase;

#endif // LLCORE_HTTP_READY_QUEUE_IGNORES_PRIORITY

class HttpReadyQueue final : public HttpReadyQueueBase
{
public:
	HttpReadyQueue()
	:	HttpReadyQueueBase()
	{
	}

	~HttpReadyQueue()
	{
	}

protected:
	HttpReadyQueue(const HttpReadyQueue&);	// Not defined
	void operator=(const HttpReadyQueue&);	// Not defined

public:

#if LLCORE_HTTP_READY_QUEUE_IGNORES_PRIORITY
	// Types and methods needed to make a std::deque look more like a
	// std::priority_queue, at least for our purposes.
	typedef HttpReadyQueueBase container_type;

	LL_INLINE const_reference top() const					{ return front(); }
	LL_INLINE void pop()									{ pop_front(); }
	LL_INLINE void push(const value_type& v)				{ emplace_back(v); }

#endif // LLCORE_HTTP_READY_QUEUE_IGNORES_PRIORITY

	LL_INLINE const container_type& get_container() const	{ return *this; }
	LL_INLINE container_type& get_container()				{ return *this;	}

}; // end class HttpReadyQueue

}  // End namespace LLCore

#endif	// _LLCORE_HTTP_READY_QUEUE_H_

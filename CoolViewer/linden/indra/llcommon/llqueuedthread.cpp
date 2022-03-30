/**
 * @file llqueuedthread.cpp
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

#include "linden_common.h"

#include "llqueuedthread.h"

#include "llstl.h"
#include "lltimer.h"	// ms_sleep()

///////////////////////////////////////////////////////////////////////////////
// LLQueuedThread class
///////////////////////////////////////////////////////////////////////////////

// Main thread
LLQueuedThread::LLQueuedThread(const std::string& name, bool threaded,
							   bool should_pause)
:	LLThread(name),
	mThreaded(threaded),
	mNextHandle(0),
	mIdleThread(true),
	mStarted(false)
{
	if (mThreaded)
	{
		if (should_pause)
		{
			pause();	// Call this before starting the thread.
		}
		start();
	}
}

// Main thread
LLQueuedThread::~LLQueuedThread()
{
	if (!mThreaded)
	{
		endThread();
	}
	shutdown();
	// ~LLThread() will be called here
}

void LLQueuedThread::shutdown()
{
	setQuitting();

	unpause(); // Main thread
	if (mThreaded)
	{
		S32 timeout = 1000;
		for ( ; timeout > 0; --timeout)
		{
			if (isStopped())
			{
				break;
			}
			ms_sleep(10);
		}
		if (timeout == 0)
		{
			llwarns << mName << " timed out !" << llendl;
		}
	}
	else
	{
		mStatus = STOPPED;
	}

	S32 active_count = 0;

	lockData();
	QueuedRequest* req;
	while ((req = (QueuedRequest*)mRequestHash.pop_element()))
	{
		S32 status = req->getStatus();
		if (status == STATUS_QUEUED || status == STATUS_INPROGRESS)
		{
			++active_count;
			req->setStatus(STATUS_ABORTED); // Avoid assert in deleteRequest
		}
		req->deleteRequest();
	}
	unlockData();

	if (active_count)
	{
		llwarns << "Called with active requests: " << active_count << llendl;
	}
}

// Main thread
//virtual
S32 LLQueuedThread::update(F32 max_time_ms)
{
	if (!mStarted && !mThreaded)
	{
		startThread();
		mStarted = true;
	}
	return updateQueue(max_time_ms);
}

S32 LLQueuedThread::updateQueue(F32 max_time_ms)
{
	if (mThreaded)
	{
		S32 pending = getPending();
		if (pending)
		{
			unpause();
		}
		return pending;
	}

	thread_local LLTimer timer;

	if (max_time_ms > 0.f)
	{
		timer.setTimerExpirySec(max_time_ms * .001f);
	}
	S32 pending;
	do
	{
		pending = processNextRequest();
	}
	while (pending && (max_time_ms <= 0.f ||!timer.hasExpired()));
	return pending;
}

void LLQueuedThread::incQueue()
{
	// Something has been added to the queue
	if (mThreaded && !isPaused())
	{
		wake(); // Wake the thread up if necessary.
	}
}

// May be called from any thread
//virtual
S32 LLQueuedThread::getPending()
{
	S32 res;
	lockData();
	res = mRequestQueue.size();
	unlockData();
	return res;
}

// Main thread
void LLQueuedThread::waitOnPending()
{
	while (true)
	{
		update(0.f);

		if (mIdleThread)
		{
			break;
		}
		if (mThreaded)
		{
			yield();
		}
	}
}

// Main thread
void LLQueuedThread::printQueueStats()
{
	lockData();
	if (!mRequestQueue.empty())
	{
		QueuedRequest* req = *mRequestQueue.begin();
		llinfos << llformat("Pending Requests:%d Current status:%d",
							mRequestQueue.size(), req->getStatus()) << llendl;
	}
	else
	{
		llinfos << "Queued thread idle" << llendl;
	}
	unlockData();
}

// Main thread
LLQueuedThread::handle_t LLQueuedThread::generateHandle()
{
	lockData();
	while (mNextHandle == nullHandle() || mRequestHash.find(mNextHandle))
	{
		++mNextHandle;
	}
	const LLQueuedThread::handle_t res = mNextHandle++;
	unlockData();
	return res;
}

// Main thread
bool LLQueuedThread::addRequest(QueuedRequest* req)
{
	if (mStatus == QUITTING)
	{
		return false;
	}

	lockData();
	req->setStatus(STATUS_QUEUED);
	mRequestQueue.insert(req);
	mRequestHash.insert(req);
	unlockData();

	incQueue();

	return true;
}

// Main thread
bool LLQueuedThread::waitForResult(LLQueuedThread::handle_t handle,
								   bool auto_complete)
{
	llassert(handle != nullHandle());
	bool res = false;
	bool waspaused = isPaused();
	bool done = false;
	while (!done)
	{
		update(0.f);		// Unpauses
		lockData();
		QueuedRequest* req = (QueuedRequest*)mRequestHash.find(handle);
		if (!req)
		{
			done = true;	// Request does not exist
		}
		else if (req->getStatus() == STATUS_COMPLETE)
		{
			res = true;
			if (auto_complete)
			{
				mRequestHash.erase(handle);
				req->deleteRequest();
			}
			done = true;
		}
		unlockData();

		if (!done && mThreaded)
		{
			yield();
		}
	}
	if (waspaused)
	{
		pause();
	}
	return res;
}

// Main thread
LLQueuedThread::QueuedRequest* LLQueuedThread::getRequest(handle_t handle)
{
	if (handle == nullHandle())
	{
		return 0;
	}
	lockData();
	QueuedRequest* res = (QueuedRequest*)mRequestHash.find(handle);
	unlockData();
	return res;
}

LLQueuedThread::status_t LLQueuedThread::getRequestStatus(handle_t handle)
{
	status_t res = STATUS_EXPIRED;
	lockData();
	QueuedRequest* req = (QueuedRequest*)mRequestHash.find(handle);
	if (req)
	{
		res = req->getStatus();
	}
	unlockData();
	return res;
}

void LLQueuedThread::abortRequest(handle_t handle, bool autocomplete)
{
	lockData();
	QueuedRequest* req = (QueuedRequest*)mRequestHash.find(handle);
	if (req)
	{
		req->setFlags(FLAG_ABORT | (autocomplete ? FLAG_AUTO_COMPLETE : 0));
	}
	unlockData();
}

// Main thread
void LLQueuedThread::setFlags(handle_t handle, U32 flags)
{
	lockData();
	QueuedRequest* req = (QueuedRequest*)mRequestHash.find(handle);
	if (req)
	{
		req->setFlags(flags);
	}
	unlockData();
}

void LLQueuedThread::setPriority(handle_t handle, U32 priority)
{
	lockData();
	QueuedRequest* req = (QueuedRequest*)mRequestHash.find(handle);
	if (req)
	{
		if (req->getStatus() == STATUS_INPROGRESS)
		{
			// Not in list
			req->setPriority(priority);
		}
		else if (req->getStatus() == STATUS_QUEUED)
		{
			// Remove from list then re-insert
			if (mRequestQueue.erase(req) != 1)
			{
				llwarns << "Request " << mName
						<< " was not in the requests queue !" << llendl;
				llassert(false);
			}
			req->setPriority(priority);
			mRequestQueue.insert(req);
		}
	}
	unlockData();
}

bool LLQueuedThread::completeRequest(handle_t handle)
{
	bool res = false;
	lockData();
	QueuedRequest* req = (QueuedRequest*)mRequestHash.find(handle);
	if (req)
	{
		S32 status = req->getStatus();
		llassert_always(status != STATUS_QUEUED &&
						status != STATUS_INPROGRESS);
		mRequestHash.erase(handle);
		req->deleteRequest();
		res = true;
	}
	unlockData();
	return res;
}

///////////////////////////////////////////////////////////////////////////////
// LLQueuedThread class: runs on its *own* thread
///////////////////////////////////////////////////////////////////////////////

//virtual
S32 LLQueuedThread::processNextRequest()
{
	QueuedRequest* req;
	// Get next request from pool
	lockData();
	while (true)
	{
		req = NULL;
		if (mRequestQueue.empty())
		{
			break;
		}
		req = *mRequestQueue.begin();
		mRequestQueue.erase(mRequestQueue.begin());
		if (!req) continue;

		if (mStatus == QUITTING || (req->getFlags() & FLAG_ABORT))
		{
			LL_DEBUGS("QueuedThread") << mName << ": aborting request "
									  << std::hex << (uintptr_t)req << std::dec
									  << LL_ENDL;
			req->setStatus(STATUS_ABORTED);
			req->finishRequest(false);
			if (req->getFlags() & FLAG_AUTO_COMPLETE)
			{
				LL_DEBUGS("QueuedThread") << mName
										  << ": deleting auto-complete request "
										  << std::hex << (uintptr_t)req << std::dec
										  << LL_ENDL;
				mRequestHash.erase(req);
				req->deleteRequest();
			}
			continue;
		}
		llassert_always(req->getStatus() == STATUS_QUEUED);
		break;
	}
	U32 start_priority = 0;
	if (req)
	{
		LL_DEBUGS("QueuedThread") << mName << ": flagging request "
								  << std::hex << (uintptr_t)req << std::dec
								  << " as being in progress" << LL_ENDL;
		req->setStatus(STATUS_INPROGRESS);
		start_priority = req->getPriority();
	}
	unlockData();

	// This is the only place we will call req->setStatus() after it has
	// initially been set to STATUS_QUEUED, so it is safe to access req.
	if (req)
	{
		// Process request
		if (req->getFlags() & FLAG_ASYNC)
		{
			req->processRequest();
			return getPending();
		}
		// Non async case
		bool ok = req->processRequest();
		setRequestResult(req, ok);
		if (!ok && mThreaded && start_priority < PRIORITY_NORMAL)
		{
#if 0		// This is too restrictive and slows things down uselessly...
			ms_sleep(1); // Sleep the thread a little
#else		// Just yield instead ! HB
			yield();
#endif
		}
	}

	return getPending();
}

void LLQueuedThread::setRequestResult(QueuedRequest* req, bool result)
{
	if (result)
	{
		lockData();
		LL_DEBUGS("QueuedThread") << mName << ": flagging request "
								  << std::hex << (uintptr_t)req << std::dec
								  << " as complete" << LL_ENDL;
		req->setStatus(STATUS_COMPLETE);
		req->finishRequest(true);
		if (req->getFlags() & FLAG_AUTO_COMPLETE)
		{
			LL_DEBUGS("QueuedThread") << mName
									  << ": deleting auto-complete request "
									  << std::hex << (uintptr_t)req << std::dec
									  << LL_ENDL;
			mRequestHash.erase(req);
			req->deleteRequest();
		}
		unlockData();
	}
	else
	{
		lockData();
		LL_DEBUGS("QueuedThread") << mName << ": decreasing request "
								  << std::hex << (uintptr_t)req << std::dec
								  << " priority" << LL_ENDL;
		req->setStatus(STATUS_QUEUED);
		mRequestQueue.insert(req);
		unlockData();
	}
}

//virtual
bool LLQueuedThread::runCondition()
{
	// mRunCondition must be locked here
	return !mIdleThread || !mRequestQueue.empty();
}

//virtual
void LLQueuedThread::run()
{
	// Call checkPause() immediately so we do not try to do anything before the
	// class is fully constructed
	checkPause();
	startThread();
	mStarted = true;

	while (true)
	{
		// This will block on the condition until runCondition() returns true,
		// the thread is unpaused, or the thread leaves the RUNNING state.
		checkPause();

		if (isQuitting())
		{
			endThread();
			break;
		}

		mIdleThread = false;

		threadedUpdate();

		if (processNextRequest() == 0)
		{
			mIdleThread = true;
#if 0		// Eeek !... Do not do that !  1ms is like a slice of eternity...
			ms_sleep(1);
#else		// Yielding is more than enough, here !  HB
			yield();
#endif
		}
	}
	llinfos << "Queued thread " << mName << " exiting." << llendl;
}

///////////////////////////////////////////////////////////////////////////////
// LLQueuedThread::QueuedRequest sub-class
///////////////////////////////////////////////////////////////////////////////

LLQueuedThread::QueuedRequest::QueuedRequest(LLQueuedThread::handle_t handle,
											 U32 priority, U32 flags)
:	LLSimpleHashEntry<LLQueuedThread::handle_t>(handle),
	mStatus(STATUS_UNKNOWN),
	mPriority(priority),
	mFlags(flags)
{
}

LLQueuedThread::QueuedRequest::~QueuedRequest()
{
	llassert_always(mStatus == STATUS_DELETE);
}

//virtual
void LLQueuedThread::QueuedRequest::deleteRequest()
{
	llassert_always(mStatus != STATUS_INPROGRESS);
	setStatus(STATUS_DELETE);
	delete this;
}

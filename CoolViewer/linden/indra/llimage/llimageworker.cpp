/**
 * @file llimageworker.cpp
 * @brief Base class for images.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llimageworker.h"

#include "llapp.h"			// For LLApp::isExiting()
#include "hbtracy.h"

//static
bool LLImageDecodeThread::sCanUseThreads = false;

// MAIN THREAD
LLImageDecodeThread::LLImageDecodeThread(U32 pool_size)
:	LLQueuedThread("Image decode main thread"),
	mMultiThreaded(false),
	mLastPoolAllocation(0),
	mFailedPoolAllocations(0)
{
	if (pool_size == 1)
	{
		// The user requested explicitely mono-threaded decoding...
		llinfos << "Initializing with just the worker main thread." << llendl;
		return;
	}

	// We will multi-thread
	mMultiThreaded = true;

	if (!pool_size)
	{
		pool_size = boost::thread::hardware_concurrency();
		U32 cores = boost::thread::physical_concurrency();
		if (!pool_size)
		{
			llwarns << "Could not determine hardware thread concurrency on this platform !"
					<<  llendl;
			pool_size = 4U;	// Use a sane default: 4 workers in the pool
		}
		else if (pool_size != cores && pool_size > 4)
		{
			// For multi-core CPUs with SMT and and more than 4 threads,
			// reserve two threads to the main loop, but limit the number of
			// threads in the pool to 32 maximum (more than that is totally
			// useless, even when flying over main land with 512m draw
			// distance).
			pool_size = llmin(pool_size - 2U, 32U);
		}
		else if (pool_size > 1)
		{
			// For multi-core CPUs without SMT or with 4 threads or less,
			// reserve one core or thread to the main loop.
			pool_size -= 1;
		}
	}

	llinfos << "Initializing with " << pool_size
			<< " worker sub-threads (plus the worker main thread)." << llendl;

	const char* sub_thread_name = "Image decode sub-thread %d";
	mThreadPool.reserve(pool_size);
	for (U32 i = 0; i < pool_size; ++i)
	{
		PoolWorkerThread* threadp =
			new PoolWorkerThread(llformat(sub_thread_name, i + 1));
		mThreadPool.push_back(threadp);
		threadp->start();
	}
}

//virtual
LLImageDecodeThread::~LLImageDecodeThread()
{
	if (mMultiThreaded)
	{
		for (U32 i = 0, count = mThreadPool.size(); i < count; ++i)
		{
			delete mThreadPool[i];
		}
		llinfos << "Failed sub-thread pool allocations (i.e. decodes done by the main decoder thread): "
				<< mFailedPoolAllocations << llendl;
	}
}

//virtual
S32 LLImageDecodeThread::update(F32 max_time_ms)
{
	LL_TRACY_TIMER(TRC_IMG_DECODE_UPDATE);

	mCreationMutex.lock();

	for (creation_list_t::iterator iter = mCreationList.begin();
		 iter != mCreationList.end(); ++iter)
	{
		CreationInfo& info = *iter;
		ImageRequest* req = new ImageRequest(info.handle, info.image,
											 info.priority, info.discard,
											 info.needs_aux, info.responder,
			                                 this);
		bool res = addRequest(req);
		if (!res)
		{
			llerrs << "Request added after LLLFSThread::cleanupClass()"
				   << llendl;
		}
	}

	mCreationList.clear();

	S32 res = LLQueuedThread::update(max_time_ms);

	mCreationMutex.unlock();

	return res;
}

bool LLImageDecodeThread::sendToPool(ImageRequest* req)
{
	LL_TRACY_TIMER(TRC_IMG_DECODE_SEND2POOL);

	for (U32 i = 0, count = mThreadPool.size(); i < count; ++i)
	{
		if (mLastPoolAllocation >= count)
		{
			mLastPoolAllocation = 0;
		}
		if (mThreadPool[mLastPoolAllocation++]->setRequest(req))
		{
			return true;
		}
	}
	++mFailedPoolAllocations;
	return false;
}

LLImageDecodeThread::handle_t LLImageDecodeThread::decodeImage(LLImageFormatted* image,
															   U32 priority,
															   S32 discard,
															   bool needs_aux,
															   Responder* responder)
{
	mCreationMutex.lock();

	handle_t handle = generateHandle();
	mCreationList.push_back(CreationInfo(handle, image, priority, discard,
										 needs_aux, responder));

	mCreationMutex.unlock();

	return handle;
}

LLImageDecodeThread::ImageRequest::ImageRequest(handle_t handle,
												LLImageFormatted* image,
												U32 priority, S32 discard,
												bool needs_aux,
												Responder* responder,
												LLImageDecodeThread* queue)
:	LLQueuedThread::QueuedRequest(handle, priority, FLAG_AUTO_COMPLETE),
	mFormattedImage(image),
	mDiscardLevel(discard),
	mNeedsAux(needs_aux),
	mDecodedRaw(false),
	mDecodedAux(false),
	mResponder(responder),
	mQueue(queue)
{
	if (queue->useAsyncRequests())
	{
		setFlags(FLAG_ASYNC);
	}
}

//virtual
LLImageDecodeThread::ImageRequest::~ImageRequest()
{
	mDecodedImageRaw = NULL;
	mDecodedImageAux = NULL;
	mFormattedImage = NULL;
}

bool LLImageDecodeThread::ImageRequest::processRequestIntern()
{
	bool done = true;
	while (true)
	{
		if (!mDecodedRaw && mFormattedImage.notNull())
		{
			// Decode primary channels
			if (mDecodedImageRaw.isNull())
			{
				// Parse formatted header
				if (!mFormattedImage->updateData())
				{
					break;	// Done (failed)
				}
				if (mFormattedImage->getWidth() *
					mFormattedImage->getHeight() *
					mFormattedImage->getComponents() == 0)
				{
					break;	// Done (failed)
				}
				if (mDiscardLevel >= 0)
				{
					mFormattedImage->setDiscardLevel(mDiscardLevel);
				}
				mDecodedImageRaw = new LLImageRaw(mFormattedImage->getWidth(),
												  mFormattedImage->getHeight(),
												  mFormattedImage->getComponents());
			}
			if (mDecodedImageRaw.notNull() && mDecodedImageRaw->getData())
			{
				done = mFormattedImage->decode(mDecodedImageRaw);
				// Some decoders are removing data when task is complete and
				// there were errors
				mDecodedRaw = done && mDecodedImageRaw->getData();
			}
			else
			{
				llwarns_once << "Failed to allocate raw image !" << llendl;
				break;	// Done (failed)
			}
		}

		if (done && mNeedsAux && !mDecodedAux && mFormattedImage.notNull())
		{
			// Decode aux channel
			if (mDecodedImageAux.isNull())
			{
				mDecodedImageAux = new LLImageRaw(mFormattedImage->getWidth(),
												  mFormattedImage->getHeight(),
												  1);
			}
			if (mDecodedImageAux.notNull() && mDecodedImageAux->getData())
			{
				done = mFormattedImage->decodeChannels(mDecodedImageAux, 4, 4);
				// Some decoders are removing data when task is complete and
				// there were errors
				mDecodedAux = done && mDecodedImageAux->getData();
			}
			else
			{
				llwarns_once << "Failed to allocate raw image !" << llendl;
			}
		}
		break;
	}

	if (mFlags & FLAG_ASYNC)
	{
		setStatus(STATUS_COMPLETE);
		finishRequest(true);
		// Always autocomplete
		mQueue->completeRequest(mHashKey);
	}

	return done;
}

// Returns true when done, whether or not decode was successful.
bool LLImageDecodeThread::ImageRequest::processRequest()
{
	LL_TRACY_TIMER(TRC_IMG_PROCESS_REQ);

	if (!(mFlags & FLAG_ASYNC))
	{
		return processRequestIntern();
	}
	if (!mQueue->sendToPool(this))
	{
		return processRequestIntern();
	}
	return true;
}

void LLImageDecodeThread::ImageRequest::finishRequest(bool completed)
{
	if (mResponder.notNull())
	{
		bool success = completed && mDecodedRaw &&
					   mDecodedImageRaw && mDecodedImageRaw->getDataSize() &&
					   (!mNeedsAux || mDecodedAux);
		mResponder->completed(success, mDecodedImageRaw, mDecodedImageAux);
	}
	// Will automatically be deleted
}

///////////////////////////////////////////////////////////////////////////////
// PoolWorkerThread sub-class (c)2021 Kathrine Jansma
///////////////////////////////////////////////////////////////////////////////

LLImageDecodeThread::PoolWorkerThread::PoolWorkerThread(const std::string& name)
:	LLThread(name),
	mCurrentRequest(NULL),
	mProcessedRequests(0)
{
}

//virtual
LLImageDecodeThread::PoolWorkerThread::~PoolWorkerThread()
{
	llinfos << mName << " shutting down. Number of processed requests: "
			<< mProcessedRequests << llendl;
}

//virtual
void LLImageDecodeThread::PoolWorkerThread::run()
{
	while (!isQuitting() && !LLApp::isExiting())
	{
		ImageRequest* req = mCurrentRequest.exchange(NULL);
		if (req)
		{
			req->processRequestIntern();
		}
		checkPause();
	}
}

bool LLImageDecodeThread::PoolWorkerThread::setRequest(ImageRequest* req)
{
	ImageRequest* tmp = NULL;
	bool ok = mCurrentRequest.compare_exchange_strong(tmp, req);
	if (ok)
	{
		++mProcessedRequests;
	}
	wake();
	return ok;
}

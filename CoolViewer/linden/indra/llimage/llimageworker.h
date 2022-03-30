/**
 * @file llimageworker.h
 * @brief Object for managing images and their textures.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_LLIMAGEWORKER_H
#define LL_LLIMAGEWORKER_H

#include <atomic>

#include "llerror.h"
#include "llimage.h"
#include "llpointer.h"
#include "llqueuedthread.h"

class LLImageDecodeThread final : public LLQueuedThread
{
protected:
	LOG_CLASS(LLImageDecodeThread);

public:
	class Responder : public LLThreadSafeRefCount
	{
	protected:
		LOG_CLASS(LLImageDecodeThread::Responder);

		~Responder() override = default;

	public:
		virtual void completed(bool success, LLImageRaw* raw,
							   LLImageRaw* aux) = 0;
	};

	class ImageRequest final : public LLQueuedThread::QueuedRequest
	{
	protected:
		LOG_CLASS(LLImageDecodeThread::ImageRequest);

		~ImageRequest() override; // Use deleteRequest()

	public:
		ImageRequest(handle_t handle, LLImageFormatted* image,
					 U32 priority, S32 discard, bool needs_aux,
					 Responder* responder,
			         LLImageDecodeThread* queue);

		bool processRequest() override;
		bool processRequestIntern();
		void finishRequest(bool completed) override;

	private:
		// Input
		LLPointer<LLImageFormatted>					mFormattedImage;
		LLPointer<LLImageRaw>						mDecodedImageRaw;
		LLPointer<LLImageRaw>						mDecodedImageAux;
		LLPointer<LLImageDecodeThread::Responder>	mResponder;
		LLImageDecodeThread*						mQueue;
		S32											mDiscardLevel;
		bool										mNeedsAux;
		// Output
		bool										mDecodedRaw;
		bool										mDecodedAux;
	};

	// PoolWorkerThread sub-class (c)2021 Kathrine Jansma
	class PoolWorkerThread final : public LLThread
	{
	protected:
		LOG_CLASS(LLImageDecodeThread::PoolWorkerThread);

	public:
		PoolWorkerThread(const std::string& name);

		~PoolWorkerThread() override;

		void run() override;

		LL_INLINE bool isBusy()
		{
			ImageRequest* req = mCurrentRequest.load();
			if (!req)
			{
				return false;
			}
			S32 status = req->getStatus();
			return status == STATUS_QUEUED || status == STATUS_INPROGRESS;
		}

		LL_INLINE bool runCondition() override
		{
			return isBusy();
		}

		bool setRequest(ImageRequest* req);

	private:
		std::atomic<ImageRequest*>	mCurrentRequest;
		U32							mProcessedRequests;
	};

	// 'pool_size' is the number of LLThreads that will be launched. When
	// omitted or equal to 0, this number is determined automatically
	// depending on the available threading concurrency.
	LLImageDecodeThread(U32 pool_size = 0);

	~LLImageDecodeThread() override;

	handle_t decodeImage(LLImageFormatted* image, U32 priority, S32 discard,
						 bool needs_aux, Responder* responder);
	S32 update(F32 max_time_ms) override;

	bool sendToPool(ImageRequest* req);

	LL_INLINE bool useAsyncRequests()
	{
		return mMultiThreaded && sCanUseThreads;
	}

private:
	typedef std::vector<PoolWorkerThread*> thread_pool_vec_t;
	thread_pool_vec_t	mThreadPool;

	struct CreationInfo
	{
		LLPointer<Responder>	responder;
		handle_t				handle;
		LLImageFormatted*		image;
		U32						priority;
		S32						discard;
		bool					needs_aux;

		CreationInfo(handle_t h, LLImageFormatted* i, U32 p, S32 d, bool aux,
					 Responder* r)
		:	handle(h),
			image(i),
			priority(p),
			discard(d),
			needs_aux(aux),
			responder(r)
		{
		}
	};

	typedef std::list<CreationInfo> creation_list_t;
	creation_list_t	mCreationList;

	LLMutex			mCreationMutex;

	U32				mLastPoolAllocation;
	U32				mFailedPoolAllocations;

	// true when the worker got sub-threads
	bool			mMultiThreaded;

public:
	// *HACK: enabled (true) only after preloading of UI textures (trying to
	// work around incompletely loaded rounded_square.tga texture). HB
	static bool		sCanUseThreads;
};

#endif

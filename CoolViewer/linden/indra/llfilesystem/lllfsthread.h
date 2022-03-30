/**
 * @file lllfsthread.h
 * @brief LLLFSThread base class
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

#ifndef LL_LLLFSTHREAD_H
#define LL_LLLFSTHREAD_H

#include <string>

// For io_uring/IOCP support
#if LL_LINUX
# include <fcntl.h>
# include <liburing.h>
#elif LL_WINDOWS
# include "llwin32headerslean.h"
#endif
#if LL_LINUX || LL_WINDOWS
# include "llatomic.h"
#endif

#include "llpointer.h"
#include "llqueuedthread.h"

class LLLFSThread : public LLQueuedThread
{
protected:
	LOG_CLASS(LLLFSThread);

public:
	enum operation_t
	{
		FILE_READ,
		FILE_WRITE,
		FILE_RENAME,
		FILE_REMOVE
	};

	class Responder : public LLThreadSafeRefCount
	{
	protected:
		LOG_CLASS(LLLFSThread::Responder);

		~Responder() override = default;

	public:
		virtual void completed(S32 bytes) = 0;
	};

	class Request : public QueuedRequest
	{
	protected:
		LOG_CLASS(LLLFSThread::Request);

		~Request() override = default;	// Use deleteRequest() to destroy

	public:
		Request(LLLFSThread* thread, handle_t handle, U32 priority,
				operation_t op, const std::string& filename, U8* buffer,
				S32 offset, S32 numbytes, Responder* responder);

		LL_INLINE S32 getBytes()					{ return mBytes; }
		LL_INLINE S32 getBytesRead()				{ return mBytesRead; }
#if LL_LINUX || LL_WINDOWS
		// We need to set bytes from the manager thread
		LL_INLINE void setBytesRead(S32 val)		{ mBytesRead = val; }
#endif

		LL_INLINE S32 getOperation()				{ return mOperation; }
		LL_INLINE U8* getBuffer()					{ return mBuffer; }
		LL_INLINE const std::string& getFilename()	{ return mFileName; }

		bool processRequest() override;
		void finishRequest(bool completed) override;
		void deleteRequest() override;

	private:
#if LL_LINUX || LL_WINDOWS
		LLLFSThread*			mThread;
#endif
		std::string				mFileName;
		LLPointer<Responder>	mResponder;

		// Destination for reads, source for writes, new UUID for rename
		U8*						mBuffer;
		// Offset into file, -1 = append (WRITE only)
		S32						mOffset;
		// Bytes to read from file, -1 = all
		S32						mBytes;
		// Bytes read from file
		S32						mBytesRead;

		operation_t				mOperation;

#if LL_LINUX
		S32						mFile;
#elif LL_WINDOWS
		HANDLE					mFile;
		// OVERLAPPED for the read/write operation
		OVERLAPPED				mOverlapped;
#endif
	};

	LLLFSThread(bool threaded = true);

	// Return a Request handle
	handle_t read(const std::string& filename, U8* buffer, S32 offset,
				  S32 numbytes, Responder* responder, U32 pri=0);
	handle_t write(const std::string& filename, U8* buffer, S32 offset,
				   S32 numbytes, Responder* responder, U32 pri=0);

	// Use to order IO operations
	LL_INLINE U32 priorityCounter()					{ return mPriorityCounter-- & PRIORITY_LOWBITS; }

	// Setups sLocal
	static void initClass(bool local_is_threaded = true,
						  bool use_io_uring = false);
	static S32 updateClass(U32 ms_elapsed);
	// Deletes sLocal
	static void cleanupClass();

#if LL_LINUX || LL_WINDOWS
protected:
	// LLQueuedThread overrides
	bool runCondition() override;
	void threadedUpdate() override;
#endif

private:
#if LL_LINUX || LL_WINDOWS
	LLAtomicS32			mPendingResults;
#endif
	U32					mPriorityCounter;

public:
#if LL_LINUX
	struct io_uring		mRing;		// The io_uring
#elif LL_WINDOWS
	HANDLE				mIOCPPort;	// IoCompletionPort for this thread
#endif

	static LLLFSThread*	sLocal;		// Default local file thread
	static bool			sUseIoUring;
	static bool			sRingInitialized;
};

#endif // LL_LLLFSTHREAD_H

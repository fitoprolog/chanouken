/**
 * @file lllfsthread.cpp
 * @brief LLLFSThread base class
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

#include "lllfsthread.h"

#include "llstl.h"

#if LL_LINUX
# define INVALID_HANDLE_VALUE -1
#endif

// Static members
LLLFSThread* LLLFSThread::sLocal = NULL;
bool LLLFSThread::sUseIoUring = false;
bool LLLFSThread::sRingInitialized = false;

//============================================================================
// Ran from MAIN thread

//static
void LLLFSThread::initClass(bool local_is_threaded, bool use_io_uring)
{
	llassert(sLocal == NULL);
#if LL_LINUX || LL_WINDOWS
	sUseIoUring = use_io_uring;
#endif
	// NOTE: when io_uring/IOCP is in use, we need threading ! HB
	sLocal = new LLLFSThread(local_is_threaded || use_io_uring);
}

//static
S32 LLLFSThread::updateClass(U32 ms_elapsed)
{
	sLocal->update((F32)ms_elapsed);
	return sLocal->getPending();
}

//static
void LLLFSThread::cleanupClass()
{
	sLocal->setQuitting();
	while (sLocal->getPending())
	{
		sLocal->update(0);
	}
#if LL_LINUX
	if (sUseIoUring)
	{
		io_uring_queue_exit(&(sLocal->mRing));
	}
#elif LL_WINDOWS
	if (sUseIoUring && sLocal->mIOCPPort != INVALID_HANDLE_VALUE)
	{
		CloseHandle(sLocal->mIOCPPort);
	}
#endif
	delete sLocal;
	sLocal = NULL;
}

//============================================================================

LLLFSThread::LLLFSThread(bool threaded)
:	LLQueuedThread("LFS", threaded),
#if LL_LINUX || LL_WINDOWS
	mPendingResults(0),
#endif
	mPriorityCounter(PRIORITY_LOWBITS)
{
#if LL_LINUX || LL_WINDOWS
	if (sUseIoUring)
	{
# if LL_LINUX
		// *FIXME: not sure if this is a good queue depth; it depends on how
		// many cqe/sqe requests we have in flight.
		constexpr int QUEUE_DEPTH = 8192;
		S32 ret = io_uring_queue_init(QUEUE_DEPTH, &mRing, 0);
		if (ret)
		{
			llwarns << "Failed to initialize io_uring. Error code: " << ret
					<< llendl;
			sUseIoUring = false;	// Disable io_uring support
		}
		else
		{
			// Make sure it does not get inherited by sub-processes
			io_uring_ring_dontfork(&mRing);
			sRingInitialized = true;
		}
# elif LL_WINDOWS
		mIOCPPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL,
										   0);
		sRingInitialized = true;
# endif
	}
#endif
}

LLLFSThread::handle_t LLLFSThread::read(const std::string& filename,
										U8* buffer, S32 offset, S32 numbytes,
										Responder* responder, U32 priority)
{
	handle_t handle = generateHandle();

	if (priority == 0)
	{
		priority = PRIORITY_NORMAL | priorityCounter();
	}
	else if (priority < PRIORITY_LOW)
	{
		// All reads are at least PRIORITY_LOW
		priority |= PRIORITY_LOW;
	}

	Request* req = new Request(this, handle, priority, FILE_READ, filename,
							   buffer, offset, numbytes, responder);

	bool res = addRequest(req);
	if (!res)
	{
		llerrs << "Call done after LLLFSThread::cleanupClass()" << llendl;
	}

	return handle;
}

LLLFSThread::handle_t LLLFSThread::write(const std::string& filename,
										 U8* buffer, S32 offset, S32 numbytes,
										 Responder* responder, U32 priority)
{
	handle_t handle = generateHandle();

	if (priority == 0)
	{
		priority = PRIORITY_LOW | priorityCounter();
	}

	Request* req = new Request(this, handle, priority, FILE_WRITE, filename,
							   buffer, offset, numbytes, responder);

	bool res = addRequest(req);
	if (!res)
	{
		llerrs << "Call done after LLLFSThread::cleanupClass()" << llendl;
	}

	return handle;
}

#if LL_LINUX || LL_WINDOWS
// Lets have some fun with overlapped I/O and io_uring ;-)
// (c)2021 Kathrine Jansma
// Requests map to OVERLAPPED ReadFile / WriteFile calls basically
// Responder => The OVERLAPPED Result gets delivered to the responder
// We pick up the completions later and have the OVERLAPPED result delivered to
// the responder. Completions MAY happen out of order !

//virtual
bool LLLFSThread::runCondition()
{
	return !mIdleThread || mPendingResults || !mRequestQueue.empty();
}

//virtual
void LLLFSThread::threadedUpdate()
{
	if (!sRingInitialized)
	{
		return;
	}

# if LL_LINUX
	struct io_uring_cqe* cqe;
# elif LL_WINDOWS
	DWORD bytes_processed = 0;
	LPOVERLAPPED result;
	DWORD_PTR key;
# endif

	// Dequeue finished results
	bool have_work = true;
	while (have_work)
	{
# if LL_LINUX
		// Check for finished cqe's without blocking; it might be more
		// efficient to use the batch variant.
		have_work = io_uring_peek_cqe(&mRing, &cqe) == 0;
		if (have_work)
		{
			llassert_always(mPendingResults > 0);
			--mPendingResults;
			if (!cqe)	// Paranoia ?
			{
				break;
			}
			S32 bytes_handled = cqe->res;

			LLLFSThread::Request* req =
				(LLLFSThread::Request*)io_uring_cqe_get_data(cqe);
			if (!req)	// Paranoia
			{
				io_uring_cqe_seen(&mRing, cqe);
				break;
			}
			if (bytes_handled >= 0)
			{
				req->setBytesRead(bytes_handled);
			}
			else	// An I/O error occurred: mark as such.
			{
				req->setBytesRead(0);
			}
			// true = resquest is over and must auto-complete (self-destruct)
			setRequestResult(req, true);
			io_uring_cqe_seen(&mRing, cqe);
		}
# elif LL_WINDOWS
		have_work = GetQueuedCompletionStatus(mIOCPPort, &bytes_processed,
											  &key, &result, 0);
		DWORD last_error = GetLastError();
		LLLFSThread::Request* req = (LLLFSThread::Request*)key;
		if (have_work)
		{
			llassert_always(mPendingResults > 0);
			--mPendingResults;
			if (req)
			{
				req->setBytesRead(bytes_processed);
				setRequestResult(req, true);
			}
		}
		else if (last_error == WAIT_TIMEOUT)
		{
			break;
		}
		else if (!result)
		{
			if (!req || last_error != ERROR_ABANDONED_WAIT_0)
			{
				llwarns << "IOCP GetQueuedCompletionStatus failed with code: "
						<< last_error << llendl;
			}
			else
			{
				llwarns << "IOCP for '" << req->getFilename()
						<< "' closed while waiting for completion status !"
						<< llendl;
			}
			break;
		}
		else
		{
			llassert_always(mPendingResults > 0);
			--mPendingResults;
			if (req)
			{
				llwarns << "IOCP operation for " << req->getFilename()
						<< " failed with code: " << last_error << llendl;
				req->setBytesRead(0);	// An error occurred: mark as such.
				// true = resquest is over and must auto-complete
				// (self-destruct)
				setRequestResult(req, true);
				// We may have further IOCP results pending
				have_work = true;
			}
		}
# endif
	}
}
#endif	// LL_LINUX || LL_WINDOWS

//============================================================================

LLLFSThread::Request::Request(LLLFSThread* thread,
							  handle_t handle, U32 priority,
							  operation_t op, const std::string& filename,
							  U8* buffer, S32 offset, S32 numbytes,
							  Responder* responder)
:	QueuedRequest(handle, priority,
				  sUseIoUring ? FLAG_AUTO_COMPLETE | FLAG_ASYNC
							  : FLAG_AUTO_COMPLETE),
	mOperation(op),
	mFileName(filename),
#if LL_LINUX || LL_WINDOWS
	mFile(INVALID_HANDLE_VALUE),
	mThread(thread),
#endif
	mBuffer(buffer),
	mOffset(offset),
	mBytes(numbytes),
	mBytesRead(sUseIoUring ? -1 : 0),
	mResponder(responder)
{
	if (numbytes <= 0)
	{
		llwarns << "Request with numbytes = " << numbytes << llendl;
	}
}

// virtual, called from own thread
void LLLFSThread::Request::finishRequest(bool completed)
{
#if LL_LINUX || LL_WINDOWS
	if (sUseIoUring && mFile != INVALID_HANDLE_VALUE)
	{
# if LL_LINUX
		close(mFile);
# elif LL_WINDOWS
		CloseHandle(mFile);
# endif
		mFile = INVALID_HANDLE_VALUE;
	}
#endif
	if (mResponder.notNull())
	{
		mResponder->completed(completed ? mBytesRead : 0);
		mResponder = NULL;
	}
}

void LLLFSThread::Request::deleteRequest()
{
	if (getStatus() == STATUS_QUEUED)
	{
		llwarns << "Deleting a queued request !" << llendl;
		llassert(false);
	}
	if (getStatus() == STATUS_INPROGRESS)
	{
		llwarns << "Deleting a running request !" << llendl;
		llassert(false);
	}
	if (mResponder.notNull())
	{
		mResponder->completed(0);
		mResponder = NULL;
	}
#if LL_LINUX || LL_WINDOWS
	if (sUseIoUring && mFile != INVALID_HANDLE_VALUE)
	{
# if LL_LINUX
		// *FIXME: not sure how to correctly cancel it...
		close(mFile);
# elif LL_WINDOWS
		CancelIoEx(mFile, NULL);
		// We must wait for IO to complete, before deleting the OVERLAPPED
		// member
		DWORD bytesHandled;
		GetOverlappedResult(mFile, &mOverlapped, &bytesHandled, TRUE);
		CloseHandle(mFile);
# endif
		mFile = INVALID_HANDLE_VALUE;
	}
#endif
	LLQueuedThread::QueuedRequest::deleteRequest();
}

bool LLLFSThread::Request::processRequest()
{
	bool complete = false;
	if (mOperation == FILE_READ)
	{
#if LL_LINUX || LL_WINDOWS
		if (sUseIoUring)
		{
# if LL_LINUX
			S32 infile = open(mFileName.c_str(), O_RDONLY);
			if (infile < 0)
			{
				llwarns << "Unable to read file: " << mFileName << llendl;
				mBytesRead = 0;		// Failed
				return true;
			}
			mFile = infile;

			S32 off;
			if (mOffset < 0)
			{
				off = lseek(mFile, 0, SEEK_END);
			}
			else
			{
				off = lseek(mFile, mOffset, SEEK_SET);
			}
			if (off < 0)
			{
				llwarns << "Unable to read file (seek failed): " << mFileName
						<< llendl;
				mBytesRead = 0;		// Failed
				return true;
			}

			// Attach the file handle to the io_uring
			struct io_uring_sqe* sqe = io_uring_get_sqe(&(mThread->mRing));
			if (!sqe)
			{
				llwarns << "Unable to do async read !" << llendl;
				mBytesRead = 0;		// Failed
				return true;
			}
			io_uring_prep_read(sqe, mFile, mBuffer, mBytes, off);
			io_uring_sqe_set_data(sqe, this);
			io_uring_submit(&(mThread->mRing));
			++mThread->mPendingResults;
# elif LL_WINDOWS
			HANDLE infile =
				CreateFile(ll_convert_string_to_wide(mFileName).c_str(),
						   GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
						   FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN,
						   NULL);
			if (infile == INVALID_HANDLE_VALUE)
			{
				llwarns << "Unable to read file: " << mFileName << llendl;
				mBytesRead = 0; 	// Failed
				return true;
			}
			mFile = infile;

			// Calculate offset for overlapped I/O
			if (mOffset >= 0)
			{
				mOverlapped.Offset = mOffset;
				mOverlapped.OffsetHigh = 0;
			}
			else
			{
				ULARGE_INTEGER FileSize;
				FileSize.LowPart = GetFileSize(mFile, &FileSize.HighPart);
				mOverlapped.Offset = FileSize.LowPart + mOffset;
				mOverlapped.OffsetHigh = FileSize.HighPart;
			}
			mOverlapped.hEvent = NULL;
			// Attach file handle to the IOCP
			CreateIoCompletionPort(mFile, mThread->mIOCPPort, (ULONG_PTR)this,
								   0);
			DWORD status = ReadFile(mFile, mBuffer, mBytes, NULL,
									&mOverlapped);
			DWORD last_error = GetLastError();
			if (!status && last_error != ERROR_IO_PENDING)
			{
				llwarns << "Unable to do async read: " << last_error << llendl;
				mBytesRead = 0;		// Failed
				return true;
			}
			// Async or sync case
			++mThread->mPendingResults;
# endif
		}
		else
#endif	// LL_LINUX || LL_WINDOWS
		{
			LLFile infile(mFileName, "rb");
			if (!infile)
			{
				llwarns << "Unable to read file: " << mFileName << llendl;
				mBytesRead = 0;		// Failed
				return true;
			}
			if (infile.seek(mOffset) < 0)
			{
				llwarns << "Unable to read file (seek failed): " << mFileName
						<< llendl;
				mBytesRead = 0;	// Failed
				return true;
			}
			mBytesRead = infile.read(mBuffer, mBytes);
		}
		complete = true;
	}
	else if (mOperation == FILE_WRITE)
	{
#if LL_LINUX || LL_WINDOWS
		if (sUseIoUring)
		{
# if LL_LINUX
			S32 outfile = open(mFileName.c_str(), O_CREAT | O_WRONLY,
							   S_IRUSR | S_IWUSR);
			if (outfile < 0)
			{
				llwarns << "Unable to write file: " << mFileName << llendl;
				mBytesRead = 0;		// Failed
				return true;
			}
			mFile = outfile;

			S32 off;
			if (mOffset < 0)
			{
				off = lseek(mFile, 0, SEEK_END);
			}
			else
			{
				off = lseek(mFile, mOffset, SEEK_SET);
			}
			if (off < 0)
			{
				llwarns << "Unable to write file (seek failed): " << mFileName
						<< llendl;
				mBytesRead = 0;		// Failed
				return true;
			}

			// Attach the file handle to the io_uring
			struct io_uring_sqe* sqe = io_uring_get_sqe(&(mThread->mRing));
			if (!sqe)
			{
				llwarns << "Unable to do async write !" << llendl;
				mBytesRead = 0;		// Failed
				return true;
			}
			io_uring_prep_write(sqe, mFile, mBuffer, mBytes, off);
			io_uring_sqe_set_data(sqe, this);
			io_uring_submit(&(mThread->mRing));
			++mThread->mPendingResults;
# elif LL_WINDOWS
			HANDLE outfile =
				CreateFile(ll_convert_string_to_wide(mFileName).c_str(),
						   GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ,
						   NULL, OPEN_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
			if (outfile == INVALID_HANDLE_VALUE)
			{
				llwarns << "Unable to write file: " << mFileName << llendl;
				mBytesRead = 0; 	// Failed
				return true;
			}
			mFile = outfile;

			if (mOffset < 0)
			{
				// Append mode
				ULARGE_INTEGER FileSize;
				FileSize.LowPart = GetFileSize(outfile, &FileSize.HighPart);
				mOverlapped.Offset = FileSize.LowPart;
				mOverlapped.OffsetHigh = FileSize.HighPart;
			}
			else
			{
				mOverlapped.Offset = mOffset;
				mOverlapped.OffsetHigh = 0;
				mOverlapped.hEvent = NULL;
			}
			// Attach file handle to the IOCP
			CreateIoCompletionPort(mFile, mThread->mIOCPPort, (ULONG_PTR)this,
								   0);
			DWORD status = WriteFile(mFile, mBuffer, mBytes, NULL,
									 &mOverlapped);
			DWORD last_error = GetLastError();
			if (!status && last_error != ERROR_IO_PENDING)
			{
				llwarns << "Unable to do async write: " << last_error
						<< llendl;
				mBytesRead = 0;		// Failed
				return true;
			}
			// Async or sync case
			++mThread->mPendingResults;
# endif
		}
		else
#endif	// LL_LINUX || LL_WINDOWS
		{
			bool exists = LLFile::exists(mFileName);
			const char* flags = exists ? (mOffset < 0 ? "ab" : "r+b") : "wb";
			LLFile outfile(mFileName, flags);
			if (!outfile)
			{
				llwarns << "Unable to write file: " << mFileName << llendl;
				mBytesRead = 0;		// Failed
				return true;
			}
			if (mOffset >= 0)
			{
				S32 seek = outfile.seek(mOffset);
				if (seek < 0)
				{
					llwarns << "Unable to write file (seek failed): "
							<< mFileName << llendl;
					mBytesRead = 0;	// Failed
					return true;
				}
			}
			mBytesRead = outfile.write(mBuffer, mBytes);
		}
		complete = true;
	}
	else
	{
		llerrs << "Invalid operation: " << (S32)mOperation << llendl;
	}
	return complete;
}

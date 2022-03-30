/**
 * @file lltexturecache.cpp
 * @brief Object which handles local texture caching
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

#include "llviewerprecompiledheaders.h"

#include "lltexturecache.h"

#include "lldir.h"
#include "lldiriterator.h"
#include "llimage.h"
#include "lllfsthread.h"

#include "llappviewer.h"
#include "llviewercontrol.h"

// Cache organization:
// cache/texture.entries
//  Unordered array of Entry structs
// cache/texture.cache
//  First TEXTURE_CACHE_ENTRY_SIZE bytes of each texture in texture.entries in
//  the same order
// cache/textures/[0-F]/UUID.texture
//  Actual texture body files

// Version of our texture cache: increment each time its structure changes.
// Note: we use an unusually large number, which should ensure that no cache
// written by another viewer than the Cool VL Viewer would be considered valid
// (even though the cache directory is normally already different).
constexpr F32 TEXTURE_CACHE_VERSION = 10001.f;
constexpr U32 ADDRESS_SIZE = sizeof(void*) * 8;	// in bits

// % amount of cache left after a purge.
constexpr S64 TEXTURE_PURGED_CACHE_SIZE = 80;
// % amount for LRU list (low overhead to regenerate)
constexpr F32 TEXTURE_CACHE_LRU_SIZE = .10f;
// w, h, c, level
constexpr S32 TEXTURE_FAST_CACHE_ENTRY_OVERHEAD = sizeof(S32) * 4;
constexpr S32 TEXTURE_FAST_CACHE_DATA_SIZE = 16 * 16 * 4;
constexpr S32 TEXTURE_FAST_CACHE_ENTRY_SIZE =
	TEXTURE_FAST_CACHE_DATA_SIZE + TEXTURE_FAST_CACHE_ENTRY_OVERHEAD;

//////////////////////////////////////////////////////////////////////////////
// Worker classes
//////////////////////////////////////////////////////////////////////////////

class LLTextureCacheWorker : public LLWorkerClass
{
	friend class LLTextureCache;
	friend class LLTextureCacheLocalFileWorker;

protected:
	LOG_CLASS(LLTextureCacheWorker);

private:
	class ReadResponder : public LLLFSThread::Responder
	{
	protected:
		LOG_CLASS(LLTextureCacheWorker::ReadResponder);

	public:
		ReadResponder(LLTextureCache* cache, handle_t handle)
		:	mCache(cache),
			mHandle(handle)
		{
		}

		void completed(S32 bytes) override
		{
			mCache->lockWorkers();
			LLTextureCacheWorker* reader = mCache->getReader(mHandle);
			if (reader)
			{
				reader->ioComplete(bytes);
			}
			mCache->unlockWorkers();
		}

	public:
		LLTextureCache*					mCache;
		LLTextureCacheWorker::handle_t	mHandle;
	};

	class WriteResponder : public LLLFSThread::Responder
	{
	protected:
		LOG_CLASS(LLTextureCacheWorker::WriteResponder);

	public:
		WriteResponder(LLTextureCache* cache, handle_t handle)
		:	mCache(cache),
			mHandle(handle)
		{
		}

		void completed(S32 bytes) override
		{
			mCache->lockWorkers();
			LLTextureCacheWorker* writer = mCache->getWriter(mHandle);
			if (writer)
			{
				writer->ioComplete(bytes);
			}
			mCache->unlockWorkers();
		}

	public:
		LLTextureCache*					mCache;
		LLTextureCacheWorker::handle_t	mHandle;
	};

public:
	LLTextureCacheWorker(LLTextureCache* cache, U32 priority, const LLUUID& id,
						 U8* data, S32 datasize, S32 offset, S32 imagesize,
						 LLTextureCache::Responder* responder)
	:	LLWorkerClass(cache, "LLTextureCacheWorker"),
		mID(id),
		mCache(cache),
		mPriority(priority),
		mReadData(NULL),
		mWriteData(data),
		mDataSize(datasize),
		mOffset(offset),
		mImageSize(imagesize),			 // For writes
		mImageFormat(IMG_CODEC_J2C),
		mImageLocal(false),
		mResponder(responder),
		mFileHandle(LLLFSThread::nullHandle()),
		mBytesToRead(0),
		mBytesRead(0),
		mStartTime(gFrameTimeSeconds)
	{
		mPriority &= LLWorkerThread::PRIORITY_LOWBITS;
	}

	~LLTextureCacheWorker() override
	{
		llassert_always(!haveWork());
		free_texture_mem(mReadData);
	}

	const LLUUID& getID()					{ return mID; }

	// Override these methods:
	virtual bool doRead() = 0;
	virtual bool doWrite() = 0;

	// Called from LLWorkerThread::processRequest()
	bool doWork(S32 param) override;

	handle_t read()
	{
		addWork(0, LLWorkerThread::PRIORITY_HIGH | mPriority);
		return mRequestHandle;
	}

	handle_t write()
	{
		addWork(1, LLWorkerThread::PRIORITY_HIGH | mPriority);
		return mRequestHandle;
	}

	bool complete()							{ return checkWork(); }

	void ioComplete(S32 bytes)
	{
		mBytesRead = bytes;
		setPriority(LLWorkerThread::PRIORITY_HIGH | mPriority);
	}

	F32 starTime() const					{ return mStartTime; }

private:
	// Called from addWork() (MAIN THREAD)
	void startWork(S32 param) override		{}

	// Called from finishRequest() (WORK THREAD)
	void finishWork(S32 param, bool completed) override;

	// Called from doWork() (MAIN THREAD)
	void endWork(S32, bool aborted) override;

protected:
	LLPointer<LLTextureCache::Responder>	mResponder;
	LLUUID									mID;
	LLTimer									mTimer;
	LLTextureCache*							mCache;
	U8*										mReadData;
	U8*										mWriteData;
	LLAtomicS32								mBytesRead;
	S32										mBytesToRead;
	U32										mPriority;
	S32										mDataSize;
	S32										mOffset;
	S32										mImageSize;
	F32										mStartTime;
	LLLFSThread::handle_t					mFileHandle;
	EImageCodec								mImageFormat;
	bool									mImageLocal;
};

class LLTextureCacheLocalFileWorker final : public LLTextureCacheWorker
{
protected:
	LOG_CLASS(LLTextureCacheLocalFileWorker);

public:
	LLTextureCacheLocalFileWorker(LLTextureCache* cache, U32 priority,
								  const std::string& filename,
								  const LLUUID& id, U8* data, S32 datasize,
								  S32 offset, S32 imagesize, // For writes
								  LLTextureCache::Responder* responder)
	:	LLTextureCacheWorker(cache, priority, id, data, datasize, offset,
							 imagesize, responder),
		mFileName(filename)
	{
	}

	bool doRead() override;

	// No writes for local files !
	bool doWrite() override					{ return true; }

private:
	std::string	mFileName;
};

bool LLTextureCacheLocalFileWorker::doRead()
{
	S32 local_size = LLFile::getFileSize(mFileName);

	if (local_size > 0 && mFileName.size() > 4)
	{
		mDataSize = local_size; // Only a complete file is valid

		std::string extension = mFileName.substr(mFileName.size() - 3, 3);
		mImageFormat = LLImageBase::getCodecFromExtension(extension);
		if (mImageFormat == IMG_CODEC_INVALID)
		{
			LL_DEBUGS("TextureCache") << "Unrecognized file extension "
									  << extension << " for local texture "
									  << mFileName << LL_ENDL;
			mDataSize = 0; // No data
			return true;
		}
	}
	else
	{
		// File does not exist: no data
		mDataSize = 0;
		return true;
	}
	if (LLTextureCache::sUseLFSThread)
	{
		if (mFileHandle == LLLFSThread::nullHandle())
		{
			mImageLocal = true;
			mImageSize = local_size;
			if (!mDataSize || mDataSize + mOffset > local_size)
			{
				mDataSize = local_size - mOffset;
			}
			if (mDataSize <= 0)
			{
				// No more data to read
				mDataSize = 0;
				return true;
			}

			mReadData = (U8*)allocate_texture_mem(mDataSize);
			if (!mReadData)
			{
				// Out of memory !
				mDataSize = 0;
				return true;
			}

			mBytesRead = -1;
			mBytesToRead = mDataSize;
			setPriority(LLWorkerThread::PRIORITY_LOW | mPriority);
			mFileHandle =
				LLLFSThread::sLocal->read(mFileName, mReadData, mOffset,
										  mDataSize,
										  new ReadResponder(mCache,
															mRequestHandle));
		}
		else if (mBytesRead >= 0)
		{
			if (mBytesRead != mBytesToRead)
			{
 				LL_DEBUGS("TextureCache") << "Error reading local file: "
										  << mFileName << " - Bytes: "
										  << mDataSize << " - Offset: "
										  << mOffset << LL_ENDL;
				mDataSize = 0;	// Failed
				free_texture_mem(mReadData);
				mReadData = NULL;
			}
			return true;
		}

		return false;
	}
	if (!mDataSize || mDataSize > local_size - mOffset)
	{
		mDataSize = local_size - mOffset;
	}

	mReadData = (U8*)allocate_texture_mem(mDataSize);
	if (!mReadData)
	{
		// Out of memory !
		mDataSize = 0;
		return true;
	}

	S32 bytes = LLFile::readEx(mFileName, mReadData, mOffset, mDataSize);
	if (bytes != mDataSize)
	{
 		LL_DEBUGS("TextureCache") << "Error reading from local file: "
								  << mFileName << " - Bytes: " << mDataSize
								  << " Offset: " << mOffset << LL_ENDL;
		mDataSize = 0;
		free_texture_mem(mReadData);
		mReadData = NULL;
	}
	else
	{
		mImageSize = local_size;
		mImageLocal = true;
	}

	return true;
}

class LLTextureCacheRemoteWorker final : public LLTextureCacheWorker
{
public:
	LLTextureCacheRemoteWorker(LLTextureCache* cache, U32 priority,
							   const LLUUID& id, U8* data, S32 datasize,
							   S32 offset, S32 imagesize, // for writes
							   LLPointer<LLImageRaw> raw, S32 discardlevel,
							   LLTextureCache::Responder* responder)
	:	LLTextureCacheWorker(cache, priority, id, data, datasize, offset,
							 imagesize, responder),
		mState(INIT),
		mRawImage(raw),
		mRawDiscardLevel(discardlevel)
	{
	}

	bool doRead() override;
	bool doWrite() override;

private:
	enum e_state
	{
		INIT = 0,
		LOCAL = 1,
		CACHE = 2,
		HEADER = 3,
		BODY = 4
	};

	e_state					mState;
	LLPointer<LLImageRaw>	mRawImage;
	S32						mRawDiscardLevel;
};

// This is where a texture is read from the cache system (header and body)
// Current assumption are:
// - the whole data are in a raw form, will be stored at mReadData
// - the size of this raw data is mDataSize and can be smaller than
//   TEXTURE_CACHE_ENTRY_SIZE (the size of a record in the header cache)
// - the code supports offset reading but this is actually never exercised in
//   the viewer
bool LLTextureCacheRemoteWorker::doRead()
{
	bool done = false;
	S32 idx = -1;

#if 0
	// First state/stage: find out if the file is local
	S32 local_size = 0;
	std::string local_filename;
	if (mState == INIT)
	{
		std::string filename = mCache->getLocalFileName(mID);
		// Is it a JPEG2000 file ?
		{
			local_filename = filename + ".j2c";
			local_size = LLFile::getFileSize(local_filename);
			if (local_size > 0)
			{
				mImageFormat = IMG_CODEC_J2C;
			}
		}
		// If not, is it a jpeg file ?
		if (local_size == 0)
		{
			local_filename = filename + ".jpg";
			local_size = LLFile::getFileSize(local_filename);
			if (local_size > 0)
			{
				mImageFormat = IMG_CODEC_JPEG;
				mDataSize = local_size; // Only a complete .jpg file is valid
			}
		}
		// Hmm... What about a targa file (used for UI texture mostly) ?
		if (local_size == 0)
		{
			local_filename = filename + ".tga";
			local_size = LLFile::getFileSize(local_filename);
			if (local_size > 0)
			{
				mImageFormat = IMG_CODEC_TGA;
				mDataSize = local_size; // Only a complete .tga file is valid
			}
		}
		// Determine the next stage: if we found a file, then LOCAL else CACHE
		mState = local_size > 0 ? LOCAL : CACHE;
	}

	// Second state/stage: if the file is local, load it and leave
	if (!done && mState == LOCAL)
	{
		llassert(local_size != 0);	// Assuming there is a non empty local file
		if (!mDataSize || mDataSize > local_size - mOffset)
		{
			mDataSize = local_size - mOffset;
		}

		// Allocate read buffer
		mReadData = (U8*)allocate_texture_mem(mDataSize);
		if (!mReadData)
		{
			// Out of memory !
			mDataSize = 0;
			return true;
		}

		S32 bytes_read = LLFile::readEx(local_filename, mReadData, mOffset,
										mDataSize);
		if (bytes_read != mDataSize)
		{
 			llwarns << "Error reading file from local cache: "
					<< local_filename << " Bytes: " << mDataSize << " Offset: "
					<< mOffset << " / " << mDataSize << llendl;
			mDataSize = 0;
			free_texture_mem(mReadData);
			mReadData = NULL;
		}
		else
		{
			LL_DEBUGS("TextureCache") << "Texture " << mID.asString()
									  << " found in local_assets" << LL_ENDL;
			mImageSize = local_size;
			mImageLocal = true;
		}
		// We are done...
		done = true;
	}
#else
	// First state/stage: do not bother with LOCAL: switch to CACHE state
	if (mState == INIT || mState == LOCAL)
	{
		mState = CACHE;
	}
#endif

	// Second state/stage: identify the cache or not...
	if (!done && mState == CACHE)
	{
		LLTextureCache::Entry entry;
		idx = mCache->getHeaderCacheEntry(mID, entry);
		if (idx < 0)
		{
			// The texture is *not* cached. We are done here...
			mDataSize = 0; // no data
			done = true;
		}
		else
		{
			mImageSize = entry.mImageSize;
			// If the read offset is bigger than the header cache, we read
			// directly from the body.
			// Note that currently, we *never* read with offset from the cache,
			// so the result is *always* HEADER
			mState = mOffset < TEXTURE_CACHE_ENTRY_SIZE ? HEADER : BODY;
		}
	}

	// Third state/stage: read data from the header cache (texture.entries)
	// file
	if (!done && mState == HEADER)
	{
		// We need an entry here or reading the header makes no sense:
		llassert_always(idx >= 0 && mOffset < TEXTURE_CACHE_ENTRY_SIZE);

		S32 offset = idx * TEXTURE_CACHE_ENTRY_SIZE + mOffset;
		// Compute the size we need to read (in bytes)
		S32 size = TEXTURE_CACHE_ENTRY_SIZE - mOffset;
		size = llmin(size, mDataSize);

		// Allocate the read buffer
		mReadData = (U8*)allocate_texture_mem(size);
		if (!mReadData)
		{
			// Out of memory !
			mDataSize = -1;	// Failed
			return true;
		}

		S32 bytes_read = LLFile::readEx(mCache->mHeaderDataFileName, mReadData,
										offset, size);
		if (bytes_read != size)
		{
			llwarns << "LLTextureCacheWorker: " << mID
					<< " incorrect number of bytes read from header: "
					<< bytes_read << " / " << size << llendl;
			free_texture_mem(mReadData);
			mReadData = NULL;
			mDataSize = -1;	// Failed
			done = true;
		}
		// If we already read all we expected, we are actually done
		if (mDataSize <= bytes_read)
		{
			done = true;
		}
		else
		{
			mState = BODY;
		}
	}

	// Fourth state/stage: read the rest of the data from the UUID based cached
	// file
	if (!done && mState == BODY)
	{
		std::string filename = mCache->getTextureFileName(mID);
		S32 filesize = LLFile::getFileSize(filename);
		if (filesize && filesize + TEXTURE_CACHE_ENTRY_SIZE > mOffset)
		{
			S32 max_datasize = TEXTURE_CACHE_ENTRY_SIZE + filesize - mOffset;
			mDataSize = llmin(max_datasize, mDataSize);

			// Reserve the whole data buffer first
			U8* data = (U8*)allocate_texture_mem(mDataSize);
			if (!data)
			{
				// Out of memory !
				mDataSize = -1;	// Failed
				return true;
			}

			S32 data_offset, file_size, file_offset;
			// Set the data file pointers taking the read offset into account.
			// 2 cases:
			if (mOffset < TEXTURE_CACHE_ENTRY_SIZE)
			{
				// Offset within the header record. That means we read
				// something from the header cache.
				// Note: most common case is (mOffset = 0), so this is the
				// "normal" code path.

				// I.e. TEXTURE_CACHE_ENTRY_SIZE if mOffset nul (common case):
				data_offset = TEXTURE_CACHE_ENTRY_SIZE - mOffset;
				file_offset = 0;
				file_size = mDataSize - data_offset;
				// Copy the raw data we've been holding from the header cache
				// into the new sized buffer
				llassert_always(mReadData);
				memcpy(data, mReadData, data_offset);
				free_texture_mem(mReadData);
				mReadData = NULL;
			}
			else
			{
				// Offset bigger than the header record. That means we have not
				// read anything yet.
				data_offset = 0;
				file_offset = mOffset - TEXTURE_CACHE_ENTRY_SIZE;
				file_size = mDataSize;
				// No data from header cache to copy in that case, we skipped
				// it all
			}

			// Now use that buffer as the object read buffer
			llassert_always(mReadData == NULL);
			mReadData = data;

			// Read the data at last
			S32 bytes_read = LLFile::readEx(filename, mReadData + data_offset,
										   file_offset, file_size);
			if (bytes_read != file_size)
			{
				LL_DEBUGS("TextureCache") << "Texture: "  << mID
										  << ". Incorrect number of bytes read from body: "
										  << bytes_read	<< " / " << file_size
										  << LL_ENDL;
				free_texture_mem(mReadData);
				mReadData = NULL;
				mDataSize = -1;	// Failed
				done = true;
			}
		}
		else
		{
			// No body, we are done.
			mDataSize = llmax(TEXTURE_CACHE_ENTRY_SIZE - mOffset, 0);
			LL_DEBUGS("TextureCache") << "No body file for texture: " << mID
									  << LL_ENDL;
		}
		// Nothing else to do at that point...
		done = true;
	}

	// Clean up and exit
	return done;
}

// This is where *everything* about a texture is written down into the cache
// system (entry map, header and body)
// Current assumption are:
// - the whole data are in a raw form, starting at mWriteData
// - the size of this raw data is mDataSize and can be smaller than
//   TEXTURE_CACHE_ENTRY_SIZE (the size of a record in the header cache)
// - the code *does not* support offset writing so there are no difference
//   between buffer addresses and start of data
bool LLTextureCacheRemoteWorker::doWrite()
{
	bool done = false;
	S32 idx = -1;

	// First stage: check that what we are trying to cache is in an OK shape
	if (mState == INIT)
	{
		if (mOffset != 0 ||  mDataSize <= 0 || mImageSize < mDataSize ||
			mRawDiscardLevel < 0 || !mRawImage || mRawImage->isBufferInvalid())
		{
			llwarns << "INIT state check failed for texture: " << mID
					<< " . Aborted." << llendl;
			mDataSize = -1;	// Failed
			done = true;
		}
		else
		{
			mState = CACHE;
		}
	}

	// No LOCAL state for write(): because it does not make much sense to cache
	// a local file...

	// Second stage: set an entry in the headers entry (texture.entries) file
	if (!done && mState == CACHE)
	{
		LLTextureCache::Entry entry;
		// Checks if this image is already in the entry list
		idx = mCache->getHeaderCacheEntry(mID, entry);
		if (idx < 0)
		{
			// Create the new entry.
			idx = mCache->setHeaderCacheEntry(mID, entry, mImageSize,
											  mDataSize);
#if LL_FAST_TEX_CACHE
			if (idx >= 0)
			{
				// Write to the fast cache.
				if (!mCache->writeToFastCache(idx, mRawImage,
											  mRawDiscardLevel))
				{
					mDataSize = -1;	// Failed
					done = true;
				}
			}
#endif
		}
		else
		{
			// Update the existing entry.
			done = mCache->updateEntry(idx, entry, mImageSize, mDataSize);
		}

		if (!done)
		{
			if (idx < 0)
			{
				llwarns << "Texture: "  << mID
						<< ". Unable to create header entry for writing !"
						<< llendl;
				mDataSize = -1;	// Failed
				done = true;
			}
			else
			{
				mState = HEADER;
			}
		}
	}

	// Third stage write the header record in the header file (texture.cache)
	if (!done && mState == HEADER)
	{
		if (idx < 0)
		{
			// We need an entry here or storing the header makes no sense:
			llwarns << "Index check failed for texture: "  << mID << llendl;
			mDataSize = -1;	// Failed
			done = true;
		}
		else
		{
			// Skip to the correct spot in the header file:
			S32 offset = idx * TEXTURE_CACHE_ENTRY_SIZE;
			// Record size is fixed for the header:
			S32 size = TEXTURE_CACHE_ENTRY_SIZE;
			S32 bytes_written;

			if (mDataSize < TEXTURE_CACHE_ENTRY_SIZE)
			{
				// We need to write a full record in the header cache so, if
				// the amount of data is smaller than a record, we need to
				// transfer the data to a buffer padded with 0 and write that.
				U8* pad_buffer =
					(U8*)allocate_texture_mem(TEXTURE_CACHE_ENTRY_SIZE);
				if (!pad_buffer)
				{
					// Out of memory !
					mDataSize = -1;	// Failed
					mRawImage = NULL;
					return true;
				}

				// Init with zeros
				memset(pad_buffer, 0, TEXTURE_CACHE_ENTRY_SIZE);
				// Copy the write buffer
				memcpy(pad_buffer, mWriteData, mDataSize);
				bytes_written = LLFile::writeEx(mCache->mHeaderDataFileName,
												pad_buffer, offset, size);
				free_texture_mem(pad_buffer);
			}
			else
			{
				// Write the header record (== first TEXTURE_CACHE_ENTRY_SIZE
				// bytes of the raw file) in the header file
				bytes_written = LLFile::writeEx(mCache->mHeaderDataFileName,
												mWriteData, offset, size);
			}

			if (bytes_written <= 0)
			{
				llwarns << "Unable to write header entry for texture: " << mID
						<< llendl;
				mDataSize = -1;	// Failed
				done = true;
			}

			// If we wrote everything (may be more with padding) in the header
			// cache, we are done so we do not have a body to store
			if (mDataSize <= bytes_written)
			{
				done = true;
			}
			else
			{
				mState = BODY;
			}
		}
	}

	// Fourth stage: write the body file, i.e. the rest of the texture in an
	// "UUID" file name
	if (!done && mState == BODY)
	{
		if (mDataSize <= TEXTURE_CACHE_ENTRY_SIZE)
		{
			// It would not make sense to be here otherwise...
			mDataSize = -1;	// Failed
			done = true;
		}
		else
		{
			S32 file_size = mDataSize - TEXTURE_CACHE_ENTRY_SIZE;
			// Build the cache file name from the UUID
			std::string filename = mCache->getTextureFileName(mID);
			LL_DEBUGS("TextureCache") << "Writing Body: " << filename
									  << " Bytes: " << file_size << LL_ENDL;
			S32 bytes_written =
				LLFile::writeEx(filename,
								mWriteData + TEXTURE_CACHE_ENTRY_SIZE, 0,
								file_size);
			if (bytes_written <= 0)
			{
				llwarns << "Texture "  << mID
						<< ". Incorrect number of bytes written to body: "
						<< bytes_written << " / " << file_size << llendl;
				mDataSize = -1;	// Failed
				done = true;
			}
		}

		// Nothing else to do at that point...
		done = true;
	}
	mRawImage = NULL;

	// Clean up and exit
	return done;
}

//virtual
bool LLTextureCacheWorker::doWork(S32 param)
{
	bool res = false;

	if (param == 0) // read
	{
		res = doRead();
	}
	else if (param == 1) // write
	{
		res = doWrite();
	}
	else
	{
		llassert_always(false);
	}

	return res;
}

//virtual (WORKER THREAD)
void LLTextureCacheWorker::finishWork(S32 param, bool completed)
{
	if (mResponder.notNull())
	{
		bool success = completed && mDataSize > 0;
		if (param == 0)	// Read
		{
			if (success)
			{
				mResponder->setData(mReadData, mDataSize, mImageSize,
									mImageFormat, mImageLocal);
				mReadData = NULL;	// Responder owns data
				mDataSize = 0;
			}
			else
			{
				free_texture_mem(mReadData);
				mReadData = NULL;
			}
		}
		else			// Write
		{
			mWriteData = NULL;		// We never owned data
			mDataSize = 0;
		}
		mCache->addCompleted(mResponder, success);
	}
}

//virtual (MAIN THREAD)
void LLTextureCacheWorker::endWork(S32, bool aborted)
{
	if (!aborted && mDataSize < 0)
	{
		// Failed
		mCache->removeFromCache(mID);
	}
}

//////////////////////////////////////////////////////////////////////////////
// LLTextureCache class proper
//////////////////////////////////////////////////////////////////////////////

// Static members
U32 LLTextureCache::sCacheMaxEntries = 1024 * 1024;
S64 LLTextureCache::sCacheMaxTexturesSize = 0; // No limit
bool LLTextureCache::sUseLFSThread = false;

static const char* entries_filename = "texture.entries";
static const char* cache_filename = "texture.cache";
static const char* old_textures_dirname = "textures";
static const char* textures_dirname = "texturecache";
#if LL_FAST_TEX_CACHE
static const char* fast_cache_filename = "FastCache.cache";
#endif

typedef boost::unique_lock<shared_mutex> unique_lock;
typedef boost::shared_lock<shared_mutex> shared_lock;
typedef boost::upgrade_lock<shared_mutex> upgrade_lock;
typedef boost::upgrade_to_unique_lock<shared_mutex> upgrade_to_unique_lock;

LLTextureCache::LLTextureCache(bool use_lfs_thread)
:	LLWorkerThread("Texture cache"),
	mHeaderFile(NULL),
	mTexturesSizeTotal(0),
#if LL_FAST_TEX_CACHE
	mFastCachep(NULL),
	mFastCachePadBuffer(NULL),
#endif
	mNumReads(0),
	mNumWrites(0),
	mDoPurge(false),
	// Do not allow to change the texture cache until setReadOnly() is called:
	mReadOnly(true)
{
	sUseLFSThread = use_lfs_thread;
}

LLTextureCache::~LLTextureCache()
{
	purgeTextureFilesTimeSliced(true);
	clearDeleteList();
	writeUpdatedEntries();
#if LL_FAST_TEX_CACHE
	delete mFastCachep;
	free_texture_mem(mFastCachePadBuffer);
#endif
}

//virtual
S32 LLTextureCache::update(F32 max_time_ms)
{
	S32 res = LLWorkerThread::update(max_time_ms);

	// Note: using static variables to avoid reallocations at each update().
	// This is only doable because we do not have to deal with multiple
	// LLTextureCache instances with update() called from various threads
	// (would otherwise need to be turned into LLTextureCache members). HB
	static handle_list_t priority_list;
	static responder_list_t completed_list;

	mListMutex.lock();
	priority_list.swap(mPrioritizeWriteList);
	completed_list.swap(mCompletedList);
	mListMutex.unlock();

	mWorkersMutex.lock();

	handle_map_t::iterator writers_end = mWriters.end();
	for (handle_list_t::iterator iter1 = priority_list.begin(),
								 end = priority_list.end();
		 iter1 != end; ++iter1)
	{
		handle_t handle = *iter1;
		handle_map_t::iterator iter2 = mWriters.find(handle);
		if (iter2 != writers_end)
		{
			LLTextureCacheWorker* worker = iter2->second;
			if (worker)
			{
				worker->setPriority(LLWorkerThread::PRIORITY_HIGH |
									worker->mPriority);
			}
			else
			{
				llwarns << "Found a NULL worker in mPrioritizeWriteList !"
						<< llendl;
			}
		}
	}
	priority_list.clear();

	F32 now = gFrameTimeSeconds;

	// *HACK: to remove stale read responder handles.
	// *FIXME: find out why they get stale (possible issue with ambiguous
	// LLResponder::completed(bool) (as defined in llcommon/llthread.h) and
	// LLLFSThread::Responder::completed(S32) methods and the ugly mix of
	// inheritances between them in the LLTextureCacheWorker::*Responder
	// sub-classes)...
	// Or maybe a missing finishWork() call due to some race condition since,
	// so far, I found out that all such stale responders are in the
	// "completed" state but somehow failed to get pushed into mCompletedList.
	// (which is filled via addCompleted(), called from finishWork()). HB
	handle_t aborted_handle = nullHandle();
	static F32 last_check = 0.f;
	constexpr F32 MAX_CHECK_INTERVAL = 10.f;	// in seconds.
	constexpr F32 WORKER_TIMEOUT = 180.f;		// in seconds.
	if (mNumReads && now - last_check > MAX_CHECK_INTERVAL)
	{
		for (handle_map_t::iterator it = mReaders.begin(),
									end = mReaders.end();
			 it != end; ++it)
		{
			LLTextureCacheWorker* worker = it->second;
			if (now - worker->starTime() > WORKER_TIMEOUT)
			{
				aborted_handle = it->first;
				break;
			}
		}
		if (aborted_handle != nullHandle())
		{
			last_check = now;
		}
	}

	mWorkersMutex.unlock();

	if (aborted_handle != nullHandle())
	{
		llwarns << "Found expired read responder: " << aborted_handle
				<< " - removing it." << llendl;
		readComplete(aborted_handle, true, true);
	}

	// Call 'completed' with workers list unlocked (may call readComplete()
	// or writeComplete()
	for (responder_list_t::iterator iter = completed_list.begin(),
									end = completed_list.end();
		 iter != end; ++iter)
	{
		Responder* responder = iter->first;
		responder->completed(iter->second);
	}
	completed_list.clear();

	static F32 last_update = 0.f;
	constexpr F32 MAX_UPDATE_INTERVAL = 300.f; // in seconds.
	if (!res && now - last_update > MAX_UPDATE_INTERVAL)
	{
		last_update = now;
		writeUpdatedEntries();
	}

	return res;
}

// Searches for local copy of UUID-based image file
std::string LLTextureCache::getLocalFileName(const LLUUID& id)
{
	// Does not include extension
	std::string idstr = id.asString();
	// *TODO: should we be storing cached textures in skin directory ?
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_SKINS,
														  "default",
														  "textures", idstr);
	return filename;
}

std::string LLTextureCache::getTextureFileName(const LLUUID& id)
{
	std::string idstr = id.asString();
	return mTexturesDirName + LL_DIR_DELIM_STR + idstr[0] + LL_DIR_DELIM_STR +
		   idstr + ".texture";
}

bool LLTextureCache::isInCache(const LLUUID& id)
{
	bool in_cache;
	{
		shared_lock lock(mHeaderMutex);
		in_cache = mHeaderIDMap.count(id) != 0;
	}
	return in_cache;
}

bool LLTextureCache::isInLocal(const LLUUID& id)
{
	S32 local_size = 0;
	std::string local_filename;

	std::string filename = getLocalFileName(id);
	// Is it a JPEG2000 file ?
	{
		local_filename = filename + ".j2c";
		local_size = LLFile::getFileSize(local_filename);
		if (local_size > 0)
		{
			return true;
		}
	}

	// If not, is it a jpeg file ?
	{
		local_filename = filename + ".jpg";
		local_size = LLFile::getFileSize(local_filename);
		if (local_size > 0)
		{
			return true;
		}
	}

	// Hmm... What about a targa file ? (used for UI texture mostly)
	{
		local_filename = filename + ".tga";
		local_size = LLFile::getFileSize(local_filename);
		if (local_size > 0)
		{
			return true;
		}
	}

	return false;
}

void LLTextureCache::setDirNames(ELLPath location)
{
	mHeaderEntriesFileName = gDirUtilp->getExpandedFilename(location,
															textures_dirname,
															entries_filename);
	mHeaderDataFileName = gDirUtilp->getExpandedFilename(location,
														 textures_dirname,
														 cache_filename);
	mTexturesDirName = gDirUtilp->getExpandedFilename(location,
													  textures_dirname);
#if LL_FAST_TEX_CACHE
	mFastCacheFileName = gDirUtilp->getExpandedFilename(location,
														textures_dirname,
														fast_cache_filename);
#endif
}

void LLTextureCache::purgeCache(ELLPath location)
{
	unique_lock lock(mHeaderMutex); 

	if (!mReadOnly)
	{
		setDirNames(location);
		llassert_always(mHeaderFile == NULL);

		// Remove the legacy cache if exists
		std::string texture_dir = mTexturesDirName;
		mTexturesDirName =
			gDirUtilp->getExpandedFilename(location, old_textures_dirname);
		if (LLFile::isdir(mTexturesDirName))
		{
			std::string file_name =
				gDirUtilp->getExpandedFilename(location, entries_filename);
			LLFile::remove(file_name);

			file_name = gDirUtilp->getExpandedFilename(location,
													   cache_filename);
			LLFile::remove(file_name);

			purgeAllTextures(true);
		}
		mTexturesDirName = texture_dir;

#if LL_FAST_TEX_CACHE
		// Remove the fast cache
		LLFile::remove(mFastCacheFileName);
#endif
	}

	// Remove the current texture cache.
	purgeAllTextures(true);
}

// Called from the main thread.
S64 LLTextureCache::initCache(ELLPath location, S64 max_size)
{
	// We should not start accessing the texture cache before initialized
	llassert_always(getPending() == 0);

	S64 entry_size = (9 * max_size) / 25; // 0.36 * max_size
	S64 max_entries = entry_size / (TEXTURE_CACHE_ENTRY_SIZE +
									TEXTURE_FAST_CACHE_ENTRY_SIZE);
	sCacheMaxEntries = (S32)(llmin((S64)sCacheMaxEntries, max_entries));
	entry_size = sCacheMaxEntries * (TEXTURE_CACHE_ENTRY_SIZE +
									 TEXTURE_FAST_CACHE_ENTRY_SIZE);
	max_size -= entry_size;
	if (sCacheMaxTexturesSize > 0)
	{
		sCacheMaxTexturesSize = llmin(sCacheMaxTexturesSize, max_size);
	}
	else
	{
		sCacheMaxTexturesSize = max_size;
	}
	max_size -= sCacheMaxTexturesSize;

	llinfos << "Headers: " << sCacheMaxEntries << " Textures size: "
			<< sCacheMaxTexturesSize / (1024 * 1024) << " MB" << llendl;

	setDirNames(location);

	if (!mReadOnly)
	{
		LLFile::mkdir(mTexturesDirName);

		const char* subdirs = "0123456789abcdef";
		for (S32 i = 0; i < 16; ++i)
		{
			LLFile::mkdir(mTexturesDirName + LL_DIR_DELIM_STR + subdirs[i]);
		}
	}
	readHeaderCache();

	// Calculate mTexturesSize and make some room in the texture cache if we
	// need it
	purgeTextures(true);

	// We should not start accessing the texture cache before initialized
	llassert_always(getPending() == 0);

#if LL_FAST_TEX_CACHE
	openFastCache(true);
#endif

	return max_size; // unused cache space
}

//----------------------------------------------------------------------------
// mHeaderMutex must be locked for the following methods !

LLFile* LLTextureCache::openHeaderEntriesFile(bool readonly, S32 offset)
{
	llassert_always(mHeaderFile == NULL);
	const char* flags = readonly ? "rb" : "r+b";
	// All code calling openHeaderEntriesFile, immediately calls
	// closeHeaderEntriesFile(), so this file is very short-lived.
	mHeaderFile = new LLFile(mHeaderEntriesFileName, flags);
	if (offset > 0)
	{
		mHeaderFile->seek(offset);
	}
	return mHeaderFile;
}

void LLTextureCache::closeHeaderEntriesFile()
{
	if (mHeaderFile)
	{
		delete mHeaderFile;
		mHeaderFile = NULL;
	}
}

void LLTextureCache::readEntriesHeader()
{
	llassert_always(mHeaderFile == NULL);

	// mHeaderEntriesInfo initializes to default values so it is safe not to
	// read it
	if (LLFile::exists(mHeaderEntriesFileName))
	{
		LLFile::readEx(mHeaderEntriesFileName, (void*)&mHeaderEntriesInfo, 0,
					   sizeof(EntriesInfo));
	}
	else	// Create an empty entries header.
	{
		mHeaderEntriesInfo.mVersion = TEXTURE_CACHE_VERSION;
		mHeaderEntriesInfo.mAddressSize = ADDRESS_SIZE;
		mHeaderEntriesInfo.mEntries = 0;
		writeEntriesHeader();
	}
}

void LLTextureCache::writeEntriesHeader()
{
	llassert_always(mHeaderFile == NULL);
	if (!mReadOnly)
	{
		LLFile::writeEx(mHeaderEntriesFileName, (void*)&mHeaderEntriesInfo, 0,
					    sizeof(EntriesInfo));
	}
}

S32 LLTextureCache::openAndReadEntry(const LLUUID& id, Entry& entry,
									 bool create)
{
	{
		LLMutexLock lock(&mLRUMutex);
		mLRU.erase(id);
	}

	upgrade_lock lock(mHeaderMutex);
	S32 idx = -1;

	id_map_t::iterator iter1 = mHeaderIDMap.find(id);
	if (iter1 != mHeaderIDMap.end())
	{
		idx = iter1->second;
	}

	if (idx < 0)
	{
		if (create && !mReadOnly)
		{
			upgrade_to_unique_lock uniqueLock(lock);
			if (mHeaderEntriesInfo.mEntries < sCacheMaxEntries)
			{
				// Add an entry to the end of the list
				idx = mHeaderEntriesInfo.mEntries++;

			}
			else if (!mFreeList.empty())
			{
				idx = *(mFreeList.begin());
				mFreeList.erase(mFreeList.begin());
			}
			else
			{
				LLMutexLock lock(&mLRUMutex);
				// Look for a still valid entry in the LRU
				for (uuid_list_t::iterator iter2 = mLRU.begin();
					 iter2 != mLRU.end(); )
				{
					uuid_list_t::iterator curiter2 = iter2++;
					LLUUID oldid = *curiter2;
					// Erase entry from LRU regardless
					mLRU.erase(curiter2);
					// Look up entry and use it if it is valid
					id_map_t::iterator iter3 = mHeaderIDMap.find(oldid);
					if (iter3 != mHeaderIDMap.end() && iter3->second >= 0)
					{
						idx = iter3->second;
						// Remove the existing cached texture to release the
						// entry index.
						removeCachedTexture(oldid);
						break;
					}
				}
				// If (idx < 0) at this point, we will rebuild the LRU and
				// retry if called from setHeaderCacheEntry(), otherwise this
				// should not happen and will trigger an error
			}
			if (idx >= 0)
			{
				entry.mID = id;
				entry.mImageSize = -1; // mark it is a brand-new entry.
				entry.mBodySize = 0;
			}
		}
	}
	else
	{
		// Read the entry
		S32 bytes_read = sizeof(Entry);
		idx_entry_map_t::iterator iter = mUpdatedEntryMap.find(idx);
		if (iter != mUpdatedEntryMap.end())
		{
			entry = iter->second;
		}
		else
		{
			bytes_read = readEntryFromHeaderImmediatelyShared(idx, entry);
		}
		if (bytes_read != sizeof(Entry))
		{
			upgrade_to_unique_lock uniqueLock(lock);
			clearCorruptedCache() ; // Clear the cache.
			idx = -1;
		}
		// It happens on 64 bits systems, do not know why
		else if (entry.mImageSize <= entry.mBodySize)
		{
			upgrade_to_unique_lock uniquelk(lock);
			llwarns << "Corrupted entry: " << id << " - Entry image size: "
					<< entry.mImageSize << " - Entry body size: "
					<< entry.mBodySize << llendl;

			// Erase this entry and the cached texture from the cache.
			std::string tex_filename = getTextureFileName(id);
			removeEntry(idx, entry, tex_filename);
			mUpdatedEntryMap.erase(idx);
			idx = -1;
		}
	}
	return idx;
}

// mHeaderMutex must be locked before calling this.
void LLTextureCache::writeEntryToHeaderImmediately(S32& idx, Entry& entry,
												   bool write_header)
{
	LLFile* file;
	S32 bytes_written;
	S32 offset = sizeof(EntriesInfo) + idx * sizeof(Entry);
	if (write_header)
	{
		file = openHeaderEntriesFile(false, 0);
		bytes_written = file->write((U8*)&mHeaderEntriesInfo,
									sizeof(EntriesInfo));
		if (bytes_written != sizeof(EntriesInfo))
		{
			clearCorruptedCache();	// Clear the cache.
			idx = -1;				// Mark the index as invalid.
			return;
		}

		mHeaderFile->seek(offset);
	}
	else
	{
		file = openHeaderEntriesFile(false, offset);
	}
	bytes_written = file->write((U8*)&entry, (S32)sizeof(Entry));
	if (bytes_written != sizeof(Entry))
	{
		clearCorruptedCache();		// Clear the cache.
		idx = -1;					// Mark the index as invalid.
		return;
	}

	closeHeaderEntriesFile();
	mUpdatedEntryMap.erase(idx);
}

// mHeaderMutex must be locked before calling this.
void LLTextureCache::readEntryFromHeaderImmediately(S32& idx, Entry& entry)
{
	S32 offset = sizeof(EntriesInfo) + idx * sizeof(Entry);
	LLFile* file = openHeaderEntriesFile(true, offset);
	S32 bytes_read = file->read((U8*)&entry, (S32)sizeof(Entry));
	closeHeaderEntriesFile();

	if (bytes_read != sizeof(Entry))
	{
		clearCorruptedCache();	// Clear the cache.
		idx = -1;				// Mark the index as invalid.
	}
}

S32 LLTextureCache::readEntryFromHeaderImmediatelyShared(S32& idx,
														 Entry& entry)
{
	S32 offset = sizeof(EntriesInfo) + idx * sizeof(Entry);
	LLFile* file = new LLFile(mHeaderEntriesFileName, "rb");
	if (!file->getStream())
	{
		delete file;
		llwarns << "Could not read: " << mHeaderEntriesFileName << llendl;
		return 0;
	}
	if (offset > 0)
	{
		file->seek(offset);
	}
	S32 bytes_read = file->read((U8*)&entry, (S32)sizeof(Entry));
	delete file;
	return bytes_read;
}

// mHeaderMutex must be locked before calling this.
// Updates an existing entry time stamp, delays writing.
void LLTextureCache::updateEntryTimeStamp(S32 idx, Entry& entry)
{
	static const U32 max_entries_without_time_stamp =
		(U32)(LLTextureCache::sCacheMaxEntries * 0.75f);

	if (mHeaderEntriesInfo.mEntries < max_entries_without_time_stamp)
	{
		// There are enough empty entry index space, no need to stamp time.
		return;
	}

	if (idx >= 0 && !mReadOnly)
	{
		entry.mTime = time(NULL);
		mUpdatedEntryMap[idx] = entry;
	}
}

// Updates an existing entry if needed, writing to header file immediately.
bool LLTextureCache::updateEntry(S32& idx, Entry& entry, S32 new_image_size,
								 S32 new_data_size)
{
	S32 new_body_size = llmax(0, new_data_size - TEXTURE_CACHE_ENTRY_SIZE);

	if (new_image_size <= entry.mImageSize && new_body_size <= entry.mBodySize)
	{
		// Nothing changed, or a higher resolution version is already in cache.
		return true;
	}

	bool purge = false;

	unique_lock lock(mHeaderMutex);

	bool update_header = false;
	if (entry.mImageSize < 0) //is a brand-new entry
	{
		mHeaderIDMap[entry.mID] = idx;
		mTexturesSizeMap[entry.mID] = new_body_size;
		mTexturesSizeTotal += new_body_size;

		// Update Header
		update_header = true;
	}
	else if (entry.mBodySize != new_body_size)
	{
		// Aready in mHeaderIDMap.
		mTexturesSizeMap[entry.mID] = new_body_size;
		mTexturesSizeTotal -= entry.mBodySize;
		mTexturesSizeTotal += new_body_size;
	}
	entry.mTime = time(NULL);
	entry.mImageSize = new_image_size;
	entry.mBodySize = new_body_size;

	writeEntryToHeaderImmediately(idx, entry, update_header);

	if (mTexturesSizeTotal > sCacheMaxTexturesSize)
	{
		purge = true;
	}

	lock.unlock();

	if (purge)
	{
		mDoPurge = true;
	}

	return false;
}

// mHeaderMutex must be locked before calling this.
U32 LLTextureCache::openAndReadEntries(std::vector<Entry>& entries)
{
	U32 num_entries = mHeaderEntriesInfo.mEntries;

	mHeaderIDMap.clear();
	mTexturesSizeMap.clear();
	mFreeList.clear();
	mTexturesSizeTotal = 0;

	LLFile* file = NULL;
	if (mUpdatedEntryMap.empty())
	{
		file = openHeaderEntriesFile(true, (S32)sizeof(EntriesInfo));
	}
	else	// Update the header file first.
	{
		file = openHeaderEntriesFile(false, 0);
		if (!file->getStream())
		{
			return 0;
		}
		updatedHeaderEntriesFile();
		file->seek((S32)sizeof(EntriesInfo));
	}
	for (U32 idx = 0; idx < num_entries; ++idx)
	{
		Entry entry;
		S32 bytes_read = file->read((U8*)&entry, (S32)sizeof(Entry));
		if (bytes_read < (S32)sizeof(Entry))
		{
			llwarns << "Corrupted header entries, failed at " << idx << " / "
					<< num_entries << llendl;
			closeHeaderEntriesFile();
			purgeAllTextures(false);
			return 0;
		}
		entries.emplace_back(entry);
		if (entry.mImageSize > entry.mBodySize)
		{
			mHeaderIDMap[entry.mID] = idx;
			mTexturesSizeMap[entry.mID] = entry.mBodySize;
			mTexturesSizeTotal += entry.mBodySize;
		}
		else
		{
			mFreeList.insert(idx);
		}
	}
	closeHeaderEntriesFile();
	return num_entries;
}

void LLTextureCache::writeEntriesAndClose(const std::vector<Entry>& entries)
{
	S32 num_entries = entries.size();
	llassert_always(num_entries == (S32)mHeaderEntriesInfo.mEntries);

	if (!mReadOnly)
	{
		LLFile* file = openHeaderEntriesFile(false, (S32)sizeof(EntriesInfo));
		for (S32 idx = 0; idx < num_entries; ++idx)
		{
			S32 bytes_written = file->write((U8*)(&entries[idx]),
											(S32)sizeof(Entry));
			if (bytes_written != sizeof(Entry))
			{
				clearCorruptedCache();	// clear the cache.
				return;
			}
		}
		closeHeaderEntriesFile();
	}
}

void LLTextureCache::writeUpdatedEntries()
{
	unique_lock lock(mHeaderMutex);
	if (!mReadOnly && !mUpdatedEntryMap.empty())
	{
		openHeaderEntriesFile(false, 0);
		updatedHeaderEntriesFile();
		closeHeaderEntriesFile();
	}
}

// mHeaderMutex must be locked and mHeaderFile must be created before calling
// this.
void LLTextureCache::updatedHeaderEntriesFile()
{
	if (!mReadOnly && !mUpdatedEntryMap.empty() && mHeaderFile)
	{
		// EntriesInfo
		mHeaderFile->seek(0);
		S32 bytes_written = mHeaderFile->write((U8*)&mHeaderEntriesInfo,
											   sizeof(EntriesInfo));
		if (bytes_written != sizeof(EntriesInfo))
		{
			clearCorruptedCache(); // Clear the cache.
			return;
		}

		// Write each updated entry
		S32 entry_size = (S32)sizeof(Entry);
		S32 prev_idx = -1;
		S32 delta_idx;
		for (idx_entry_map_t::iterator iter = mUpdatedEntryMap.begin(),
									   end = mUpdatedEntryMap.end();
			 iter != end; ++iter)
		{
			delta_idx = iter->first - prev_idx - 1;
			prev_idx = iter->first;
			if (delta_idx)
			{
				mHeaderFile->seek(delta_idx * entry_size, true);
			}

			bytes_written = mHeaderFile->write((U8*)&iter->second, entry_size);
			if (bytes_written != entry_size)
			{
				clearCorruptedCache(); // Clear the cache.
				return;
			}
		}
		mUpdatedEntryMap.clear();
	}
}

// Called from either the main thread or the worker thread
void LLTextureCache::readHeaderCache()
{
	{
		LLMutexLock lock(&mLRUMutex);
		mLRU.clear(); // Always clear the LRU
	}

	bool repeat_reading = false;

	{
		unique_lock lock(mHeaderMutex);

		readEntriesHeader();
		if (mHeaderEntriesInfo.mVersion != TEXTURE_CACHE_VERSION ||
			mHeaderEntriesInfo.mAddressSize != ADDRESS_SIZE)
		{
			if (!mReadOnly)
			{
				purgeAllTextures(false);
			}
			return;
		}

		std::vector<Entry> entries;
		U32 num_entries = openAndReadEntries(entries);
		if (!num_entries)
		{
			return;
		}

		U32 empty_entries = 0;
		typedef std::pair<U32, S32> lru_data_t;
		std::set<lru_data_t> lru;
		std::set<U32> purge_list;
		for (U32 i = 0; i < num_entries; ++i)
		{
			Entry& entry = entries[i];
			if (entry.mImageSize <= 0)
			{
				// This will be in the Free List, do not put it in the LRU
				++empty_entries;
			}
			else
			{
				lru.emplace(entry.mTime, i);
				if (entry.mBodySize > 0)
				{
					if (entry.mBodySize > entry.mImageSize)
					{
						// Should not happen, failsafe only
						llwarns << "Bad entry: " << i << ": " << entry.mID
								<< ": BodySize: " << entry.mBodySize
								<< llendl;
						purge_list.insert(i);
					}
				}
			}
		}
		if (num_entries - empty_entries > sCacheMaxEntries)
		{
			// Special case: cache size was reduced, need to remove entries.
			// Note: After we prune entries, we will call this again and
			// create the LRU
			U32 entries_to_purge = num_entries - empty_entries -
								   sCacheMaxEntries;
			llinfos << "Texture Cache Entries: " << num_entries << " Max: "
					<< sCacheMaxEntries << " Empty: " << empty_entries
					<< " Purging: " << entries_to_purge << llendl;
			for (std::set<lru_data_t>::iterator iter = lru.begin(),
												end = lru.end();
				 iter != end; ++iter)
			{
				purge_list.insert(iter->second);
				if (purge_list.size() >= entries_to_purge)
				{
					break;
				}
			}
		}
		else
		{
			LLMutexLock lock(&mLRUMutex);
			S32 lru_entries = (S32)((F32)sCacheMaxEntries *
									TEXTURE_CACHE_LRU_SIZE);
			for (std::set<lru_data_t>::iterator iter = lru.begin(),
												end = lru.end();
				 iter != end; ++iter)
			{
				mLRU.emplace(entries[iter->second].mID);
				if (--lru_entries <= 0)
				{
					break;
				}
			}
		}

		if (!purge_list.size())
		{
			// Entries are not changed, nothing to do.
			return;
		}

		for (std::set<U32>::iterator iter = purge_list.begin(),
									 end = purge_list.end();
			 iter != end; ++iter)
		{
			std::string tex_filename =
				getTextureFileName(entries[*iter].mID);
			removeEntry((S32)*iter, entries[*iter], tex_filename);
		}
		// If we removed any entries, we need to rebuild the entries list,
		// write the header, and call this again
		std::vector<Entry> new_entries;
		for (U32 i = 0; i < num_entries; ++i)
		{
			const Entry& entry = entries[i];
			if (entry.mImageSize > 0)
			{
				new_entries.emplace_back(entry);
			}
		}
		mFreeList.clear();     // Recreating list, no longer valid.
		llassert_always(new_entries.size() <= sCacheMaxEntries);
		mHeaderEntriesInfo.mEntries = new_entries.size();
		writeEntriesHeader();
		writeEntriesAndClose(new_entries);
		repeat_reading = true;
	}

	// Repeat with new entries file
	if (repeat_reading)
	{
		readHeaderCache();
	}
}

// mHeaderMutex must be locked before calling this.
void LLTextureCache::clearCorruptedCache()
{
	llwarns << "The texture cache is corrupted:clearing it." << llendl;

	closeHeaderEntriesFile();	// Close possible file handler
	purgeAllTextures(false);	// Clear the cache.

	if (!mReadOnly)
	{
		// Regenerate the directory tree if it does not exist
		LLFile::mkdir(mTexturesDirName);

		const char* subdirs = "0123456789abcdef";
		for (S32 i = 0; i < 16; ++i)
		{
			LLFile::mkdir(mTexturesDirName + LL_DIR_DELIM_STR + subdirs[i]);
		}
	}
}

void LLTextureCache::purgeAllTextures(bool purge_directories)
{
	if (!mReadOnly)
	{
		const char* subdirs = "0123456789abcdef";
		std::string dirname;
		for (S32 i = 0; i < 16; ++i)
		{
			dirname = mTexturesDirName + LL_DIR_DELIM_STR + subdirs[i];
			llinfos << "Deleting files in directory: " << dirname << llendl;
			LLDirIterator::deleteFilesInDir(dirname);
			if (purge_directories)
			{
				LLFile::rmdir(dirname);
			}
		}
		if (purge_directories)
		{
			LLDirIterator::deleteFilesInDir(mTexturesDirName);
			LLFile::rmdir(mTexturesDirName);
		}
	}
	mHeaderIDMap.clear();
	mTexturesSizeMap.clear();
	mTexturesSizeTotal = 0;
	mFreeList.clear();
	mTexturesSizeTotal = 0;
	mUpdatedEntryMap.clear();

	// Info with 0 entries
	mHeaderEntriesInfo.mVersion = TEXTURE_CACHE_VERSION;
	mHeaderEntriesInfo.mAddressSize = ADDRESS_SIZE;
	mHeaderEntriesInfo.mEntries = 0;
	writeEntriesHeader();

	llinfos << "The entire texture cache is cleared." << llendl;
}

void LLTextureCache::purgeTextures(bool validate)
{
	mDoPurge = false;

	if (mReadOnly)
	{
		return;
	}

	if (!validate && mTexturesSizeTotal < sCacheMaxTexturesSize)
	{
		return;
	}

	if (!mThreaded)
	{
		// *FIX:Mani - watchdog off.
		gAppViewerp->pauseMainloopTimeout();
	}

	unique_lock lock(mHeaderMutex);

	// Read the entries list
	std::vector<Entry> entries;
	U32 num_entries = openAndReadEntries(entries);
	if (!num_entries)
	{
		gAppViewerp->resumeMainloopTimeout();
		return; // Nothing to purge
	}

	llinfos << "Purging the cache from old textures..." << llendl;

	// Use mTexturesSizeMap to collect UUIDs of textures with bodies
	typedef std::set<std::pair<U32, S32> > time_idx_set_t;
	time_idx_set_t time_idx_set;
	id_map_t::iterator header_id_map_end = mHeaderIDMap.end();
	for (size_map_t::iterator iter1 = mTexturesSizeMap.begin(),
							  end = mTexturesSizeMap.end();
		 iter1 != end; ++iter1)
	{
		if (iter1->second > 0)
		{
			id_map_t::iterator iter2 = mHeaderIDMap.find(iter1->first);
			if (iter2 != header_id_map_end)
			{
				S32 idx = iter2->second;
				time_idx_set.emplace(entries[idx].mTime, idx);
			}
			else
			{
				llwarns << "mTexturesSizeMap / mHeaderIDMap corrupted."
						<< llendl;
				clearCorruptedCache();
				gAppViewerp->resumeMainloopTimeout();
				return;
			}
		}
	}

	// Validate 1/32th of the files on startup
	constexpr U32 FRACTION = 8;	// 256 / 8 = 32
	U32 validate_idx = 0;
	if (validate)
	{
		validate_idx = (gSavedSettings.getU32("CacheValidateCounter") /
						FRACTION) * FRACTION;
		U32 next_idx = (validate_idx + FRACTION) % 256;
		gSavedSettings.setU32("CacheValidateCounter", next_idx);
		LL_DEBUGS("TextureCache") << "Validating indexes " << validate_idx
								  << " to " << validate_idx + FRACTION - 1
								  << LL_ENDL;
	}

	S64 cache_size = mTexturesSizeTotal;
	S64 purged_cache_size =
		(TEXTURE_PURGED_CACHE_SIZE * sCacheMaxTexturesSize) / (S64)100;
	S32 purge_count = 0;
	for (time_idx_set_t::iterator iter = time_idx_set.begin(),
								  end = time_idx_set.end();
		 iter != end; ++iter)
	{
		S32 idx = iter->second;
		bool purge_entry = false;
		std::string filename = getTextureFileName(entries[idx].mID);
		if (cache_size >= purged_cache_size)
		{
			purge_entry = true;
		}
		else if (validate)
		{
			// make sure file exists and is the correct size
			U32 uuididx = entries[idx].mID.mData[0];
			if (uuididx >= validate_idx && uuididx < validate_idx + 4)
			{
 				LL_DEBUGS("TextureCache") << "Validating: " << filename
										  << "Size: " << entries[idx].mBodySize
										  << LL_ENDL;
				S32 bodysize = LLFile::getFileSize(filename);
				if (bodysize != entries[idx].mBodySize)
				{
					llwarns << "Purging corrupted cached texture (body size "
							<< bodysize << " != " << entries[idx].mBodySize
							<< "): " << filename << llendl;
					purge_entry = true;
				}
			}
		}
		else
		{
			break;
		}

		if (purge_entry)
		{
			++purge_count;
			mFilesToDelete[entries[idx].mID] = filename;
			cache_size -= entries[idx].mBodySize;
			// remove the entry but not the file:
			removeEntry(idx, entries[idx], filename, false);
		}
	}

	LL_DEBUGS("TextureCache") << "Writing Entries: " << num_entries << LL_ENDL;

	if (purge_count > 0)
	{
		writeEntriesAndClose(entries);

		llinfos << "Purged: " << purge_count << " - Entries: " << num_entries
				<< " - Cache size: " << mTexturesSizeTotal / 1048576 << " MB"
				<< " - Files scheduled for deletion: " << mFilesToDelete.size()
				<< llendl;
	}
	else
	{
		llinfos << "Nothing to purge." << llendl;
	}

	// *FIX: Mani - watchdog back on.
	gAppViewerp->resumeMainloopTimeout();

	mSlicedPurgeTimer.reset();
}

void LLTextureCache::purgeTextureFilesTimeSliced(bool force)
{
	constexpr F32 delay_between_passes = 2.f;	// seconds
	constexpr F32 max_time_per_pass = 0.1f;		// seconds

	if (!force && mSlicedPurgeTimer.getElapsedTimeF32() <= delay_between_passes)
	{
		return;
	}

	if (!mFilesToDelete.empty())
	{
		llinfos << "Time-sliced purging with " << mFilesToDelete.size()
				<< " files scheduled for deletion" << llendl;

		mSlicedPurgeTimer.reset();

		unique_lock lock(mHeaderMutex);
		U32 purged = 0;
		std::string filename;

		for (LLTextureCache::purge_map_t::iterator iter = mFilesToDelete.begin();
			 iter != mFilesToDelete.end(); )
		{
			LLTextureCache::purge_map_t::iterator curiter = iter++;
			// Only remove files for textures that have not been cached again
			// since we selected them for removal !
			if (!mHeaderIDMap.count(curiter->first))
			{
				filename = curiter->second;
				LLFile::remove(filename);
			}
			else
			{
				LL_DEBUGS("TextureCache") << curiter->second
										  << " selected for removal, but texture cached again since !"
										  << LL_ENDL;
			}
			mFilesToDelete.erase(curiter);
			++purged;

			if (!force &&
				mSlicedPurgeTimer.getElapsedTimeF32() > max_time_per_pass)
			{
				break;
			}
		}

		if (mFilesToDelete.empty())
		{
			llinfos << "Time-sliced purge finished with " << purged
					<< " files deleted in "
					<< mSlicedPurgeTimer.getElapsedTimeF32() << "s" << llendl;
		}
		else
		{
			llinfos << "time sliced purge: " << purged << " files deleted in "
					<< mSlicedPurgeTimer.getElapsedTimeF32() << "s ("
					<< mFilesToDelete.size() << " files left for next pass)"
					<< llendl;
		}

		mSlicedPurgeTimer.reset();
	}
}

//////////////////////////////////////////////////////////////////////////////

// Call lockWorkers() first !
LLTextureCacheWorker* LLTextureCache::getReader(handle_t handle)
{
	LLTextureCacheWorker* res = NULL;
	handle_map_t::iterator iter = mReaders.find(handle);
	if (iter != mReaders.end())
	{
		res = iter->second;
	}
	return res;
}

LLTextureCacheWorker* LLTextureCache::getWriter(handle_t handle)
{
	LLTextureCacheWorker* res = NULL;
	handle_map_t::iterator iter = mWriters.find(handle);
	if (iter != mWriters.end())
	{
		res = iter->second;
	}
	return res;
}

//////////////////////////////////////////////////////////////////////////////
// Called from work thread

// Reads imagesize from the header, updates timestamp
S32 LLTextureCache::getHeaderCacheEntry(const LLUUID& id, Entry& entry)
{
	S32 idx = openAndReadEntry(id, entry, false);
	if (idx >= 0)
	{
		unique_lock lock(mHeaderMutex);
		updateEntryTimeStamp(idx, entry); // Updates time
	}
	return idx;
}

// Writes imagesize to the header, updates timestamp
S32 LLTextureCache::setHeaderCacheEntry(const LLUUID& id, Entry& entry,
										S32 imagesize, S32 datasize)
{
	S32 idx = openAndReadEntry(id, entry, true); // read or create

	if (idx < 0) // Retry once
	{
		readHeaderCache(); // We could not write an entry, so refresh the LRU

		idx = openAndReadEntry(id, entry, true);
	}

	if (idx >= 0)
	{
		updateEntry(idx, entry, imagesize, datasize);
	}
	else
	{
		llwarns << "Failed to set cache entry for image: " << id << llendl;
		clearCorruptedCache();
	}

	return idx;
}

//////////////////////////////////////////////////////////////////////////////

// Calls from texture pipeline thread (i.e. LLTextureFetch)

LLTextureCache::handle_t LLTextureCache::readFromFile(const std::string& filename,
													  const LLUUID& id,
													  U32 priority,
													  S32 offset, S32 size,
													  ReadResponder* responder)
{
	if (offset == 0)	// To avoid spam from possible successive chunks reads.
	{
		LL_DEBUGS("TextureCache") << "Request to read texture from file: "
								  << filename << LL_ENDL;
	}

	// Note: checking to see if an entry exists can cause a stall, so let the
	// thread handle it
	mWorkersMutex.lock();
	LLTextureCacheWorker* worker =
		new LLTextureCacheLocalFileWorker(this, priority, filename, id, NULL,
										  size, offset, 0, responder);
	handle_t handle = worker->read();
	mReaders[handle] = worker;
	mNumReads = mReaders.size();
	mWorkersMutex.unlock();
	return handle;
}

LLTextureCache::handle_t LLTextureCache::readFromCache(const LLUUID& id,
													   U32 priority,
													   S32 offset, S32 size,
													   ReadResponder* responder)
{
	// Note: checking to see if an entry exists can cause a stall, so let the
	// thread handle it
	mWorkersMutex.lock();
	LLTextureCacheWorker* worker =
		new LLTextureCacheRemoteWorker(this, priority, id, NULL, size, offset,
									   0, NULL, 0, responder);
	handle_t handle = worker->read();
	mReaders[handle] = worker;
	mNumReads = mReaders.size();
	mWorkersMutex.unlock();
	return handle;
}

bool LLTextureCache::readComplete(handle_t handle, bool abort, bool verbose)
{
	mWorkersMutex.lock();
	handle_map_t::iterator iter = mReaders.find(handle);
	LLTextureCacheWorker* worker = NULL;
	bool complete = false;
	if (iter != mReaders.end())
	{
		worker = iter->second;
		complete = !worker || worker->complete();
		if (!complete && abort)
		{
			if (verbose)
			{
				llinfos << "Request " << handle << " was not complete."
						<< llendl;
			}
			abortRequest(handle, true);
		}
		if (complete || abort)
		{
			mReaders.erase(iter);
			mNumReads = mReaders.size();
		}
	}
	if (worker && (complete || abort))
	{
		if (verbose)
		{
			llinfos << "Request " << handle << " scheduled for delete."
					<< " Associated texture Id: " << worker->getID()
					<< llendl;
		}
		mWorkersMutex.unlock();
		worker->scheduleDelete();
	}
	else
	{
		mWorkersMutex.unlock();
	}
	return complete || abort;
}

LLTextureCache::handle_t LLTextureCache::writeToCache(const LLUUID& id,
													  U32 priority,
													  U8* data, S32 datasize,
													  S32 imagesize,
													  LLPointer<LLImageRaw> rawimage,
													  S32 discardlevel,
													  WriteResponder* responder)
{
	if (mReadOnly)
	{
		delete responder;
		return LLWorkerThread::nullHandle();
	}

	if (mDoPurge)
	{
		purgeTextures(false);
	}
	static LLCachedControl<bool> purge_time_sliced(gSavedSettings,
												   "CachePurgeTimeSliced");
	purgeTextureFilesTimeSliced(!purge_time_sliced);

	// This may happen when a texture fails to decode...
	if (rawimage.isNull() || !rawimage->getData())
	{
		delete responder;
		return LLWorkerThread::nullHandle();
	}

	mWorkersMutex.lock();
	LLTextureCacheWorker* worker =
		new LLTextureCacheRemoteWorker(this, priority, id, data, datasize, 0,
									   imagesize, rawimage, discardlevel,
									   responder);
	handle_t handle = worker->write();
	mWriters[handle] = worker;
	mNumWrites = mWriters.size();
	mWorkersMutex.unlock();
	return handle;
}

#if LL_FAST_TEX_CACHE
// Called from the main thread
LLPointer<LLImageRaw> LLTextureCache::readFromFastCache(const LLUUID& id,
														S32& discardlevel)
{
	static LLCachedControl<bool> fast_cache_enabled(gSavedSettings,
													"FastCacheFetchEnabled");
	if (!fast_cache_enabled)
	{
		// Fast cache disabled !
		return NULL;
	}

	U32 offset;
	{
		shared_lock lock(mHeaderMutex);
		id_map_t::const_iterator iter = mHeaderIDMap.find(id);
		if (iter == mHeaderIDMap.end())
		{
			return NULL; // Not in the cache
		}

		offset = iter->second;
	}
	offset *= TEXTURE_FAST_CACHE_ENTRY_SIZE;

	U8* data;
	S32 head[4];
	{
		mFastCacheMutex.lock();

		openFastCache();

		mFastCachep->seek(offset);

		S32 bytes = mFastCachep->read(head, TEXTURE_FAST_CACHE_ENTRY_OVERHEAD);
		if (bytes != TEXTURE_FAST_CACHE_ENTRY_OVERHEAD)
		{
			llwarns_once << "Error reading fast cache entry for " << id
						 << ". Got " << bytes
						 << " bytes for the header while expecting "
						 << TEXTURE_FAST_CACHE_ENTRY_OVERHEAD << " bytes."
						 << llendl;
			closeFastCache();
			mFastCacheMutex.unlock();
			return NULL;
		}

		S32 image_size = head[0] * head[1] * head[2];
		if (image_size <= 0 || image_size > TEXTURE_FAST_CACHE_DATA_SIZE ||
			head[3] < 0)
		{
			// Invalid texture for the fast cache
			closeFastCache();
			mFastCacheMutex.unlock();
			return NULL;
		}
		discardlevel = head[3];

		data = (U8*)allocate_texture_mem(image_size);
		if (!data)
		{
			// Out of memory !
			closeFastCache();
			mFastCacheMutex.unlock();
			return NULL;
		}

		if (mFastCachep->read(data, image_size) != image_size)
		{
			free_texture_mem(data);
			closeFastCache();
			mFastCacheMutex.unlock();
			return NULL;
		}
		closeFastCache();

		mFastCacheMutex.unlock();
	}
	LLPointer<LLImageRaw> raw = new LLImageRaw(data, head[0], head[1], head[2],
											   true);
	return raw;
}

bool LLTextureCache::writeToFastCache(S32 id, LLPointer<LLImageRaw> raw,
									  S32 discardlevel)
{
	static LLCachedControl<bool> fast_cache_enabled(gSavedSettings,
													"FastCacheFetchEnabled");
	if (!fast_cache_enabled)
	{
		// Fast cache disabled !
		return true;
	}

	// Rescale image if needed
	if (raw.isNull() || raw->isBufferInvalid() || !raw->getData())
	{
		llwarns << "Attempted to write NULL raw image " << id
				<< " to fast cache. Ignoring." << llendl;
		return false;
	}

	S32 w = raw->getWidth();
	S32 h = raw->getHeight();
	S32 c = raw->getComponents();

	S32 i = 0;
	// Search for a discard level that will fit into fast cache
	while ((w >> i) * (h >> i) * c > TEXTURE_FAST_CACHE_DATA_SIZE)
	{
		++i;
	}

	if (i)
	{
		w >>= i;
		h >>= i;
		if (w * h * c > 0) // valid
		{
			// Make a duplicate to keep the original raw image untouched:
			raw = raw->scaled(w, h);
			if (raw.isNull() || raw->isBufferInvalid())
			{
				return false;
			}
			else
			{
				discardlevel += i;
			}
		}
	}

	// Copy data
	memcpy(mFastCachePadBuffer, &w, sizeof(S32));
	memcpy(mFastCachePadBuffer + sizeof(S32), &h, sizeof(S32));
	memcpy(mFastCachePadBuffer + sizeof(S32) * 2, &c, sizeof(S32));
	memcpy(mFastCachePadBuffer + sizeof(S32) * 3, &discardlevel, sizeof(S32));

	S32 copy_size = w * h * c;
	if (copy_size > 0 && raw->getData()) // valid
	{
		copy_size = llmin(copy_size, TEXTURE_FAST_CACHE_DATA_SIZE);
		memcpy(mFastCachePadBuffer + TEXTURE_FAST_CACHE_ENTRY_OVERHEAD,
			   raw->getData(), copy_size);
	}
	S32 offset = id * TEXTURE_FAST_CACHE_ENTRY_SIZE;

	mFastCacheMutex.lock();

	openFastCache();

	mFastCachep->seek(offset);
		
	S32 bytes = mFastCachep->write(mFastCachePadBuffer,
								   TEXTURE_FAST_CACHE_ENTRY_SIZE);
	if (bytes != TEXTURE_FAST_CACHE_ENTRY_SIZE)
	{
		llwarns_once << "Error writing fast cache entry for " << id
					 << ". Wrote " << bytes << " bytes while expecting "
					 << TEXTURE_FAST_CACHE_ENTRY_SIZE << " bytes." << llendl;
		closeFastCache(true);
		mFastCacheMutex.unlock();
		return false;
	}

	closeFastCache(true);

	mFastCacheMutex.unlock();

	return true;
}

void LLTextureCache::openFastCache(bool first_time)
{
	if (!mFastCachep)
	{
		if (first_time)
		{
			if (!mFastCachePadBuffer)
			{
				mFastCachePadBuffer =
					(U8*)allocate_texture_mem(TEXTURE_FAST_CACHE_ENTRY_SIZE);
			}
			if (!mFastCachePadBuffer)
			{
				// Out of memory and not really recoverable...
				llerrs << "Out of memory while opening the fast cache !"
					   << llendl;
			}
			if (LLFile::exists(mFastCacheFileName))
			{
				mFastCachep = new LLFile(mFastCacheFileName, "r+b");
			}
			else
			{
				mFastCachep = new LLFile(mFastCacheFileName, "w+b");
			}
		}
		else
		{
			mFastCachep = new LLFile(mFastCacheFileName, "r+b");
		}

		mFastCacheTimer.reset();
	}
}

void LLTextureCache::closeFastCache(bool forced)
{
	constexpr F32 timeout = 10.f; //seconds

	if (!mFastCachep)
	{
		return;
	}

	if (!forced && mFastCacheTimer.getElapsedTimeF32() < timeout)
	{
		return;
	}

	delete mFastCachep;
	mFastCachep = NULL;
}
#endif

bool LLTextureCache::writeComplete(handle_t handle, bool abort)
{
	mWorkersMutex.lock();
	handle_map_t::iterator iter = mWriters.find(handle);
	llassert(iter != mWriters.end());
	if (iter != mWriters.end())
	{
		LLTextureCacheWorker* worker = iter->second;
		if (worker->complete() || abort)
		{
			mWriters.erase(handle);
			mNumWrites = mWriters.size();
			mWorkersMutex.unlock();
			worker->scheduleDelete();
			return true;
		}
	}
	mWorkersMutex.unlock();
	return false;
}

void LLTextureCache::prioritizeWrite(handle_t handle)
{
	// Do not prioritize yet, we might be working on this now which could
	// create a deadlock
	mListMutex.lock();
	mPrioritizeWriteList.push_back(handle);
	mListMutex.unlock();
}

void LLTextureCache::addCompleted(Responder* responder, bool success)
{
	mListMutex.lock();
	mCompletedList.emplace_back(responder, success);
	mListMutex.unlock();
}

//////////////////////////////////////////////////////////////////////////////

// Called after mHeaderMutex is locked.
void LLTextureCache::removeCachedTexture(const LLUUID& id)
{
	if (mTexturesSizeMap.find(id) != mTexturesSizeMap.end())
	{
		mTexturesSizeTotal -= mTexturesSizeMap[id];
		mTexturesSizeMap.erase(id);
	}
	mHeaderIDMap.erase(id);
	LLFile::remove(getTextureFileName(id));
}

// Called after mHeaderMutex is locked.
void LLTextureCache::removeEntry(S32 idx, Entry& entry, std::string& filename,
								 bool remove_file)
{
 	bool file_maybe_exists = true;	// Always attempt to remove when idx is invalid.

	if (idx >= 0) //valid entry
	{
		if (entry.mBodySize == 0)	// Always attempt to remove when mBodySize > 0.
		{
			// Sanity check. Should not exist when body size is 0.
			if (LLFile::exists(filename))
			{
				llwarns << "Entry has body size of zero but file " << filename
						<< " exists. Deleting this file, too." << llendl;
			}
			else
			{
				file_maybe_exists = false;
			}
		}
		mTexturesSizeTotal -= entry.mBodySize;
		entry.mImageSize = -1;
		entry.mBodySize = 0;
		mHeaderIDMap.erase(entry.mID);
		mTexturesSizeMap.erase(entry.mID);
		mFreeList.insert(idx);
	}

	if (file_maybe_exists && remove_file)
	{
		LLFile::remove(filename);
	}
}

bool LLTextureCache::removeFromCache(const LLUUID& id)
{
	//llinfos << "Removing texture from cache: " << id << llendl;
	bool ret = false;
	if (!mReadOnly)
	{
		Entry entry;
		S32 idx = openAndReadEntry(id, entry, false);
		std::string tex_filename = getTextureFileName(id);

		unique_lock lock(mHeaderMutex);
		removeEntry(idx, entry, tex_filename);
		if (idx >= 0)
		{
			writeEntryToHeaderImmediately(idx, entry);
			ret = true;
		}
	}
	return ret;
}

//////////////////////////////////////////////////////////////////////////////

LLTextureCache::ReadResponder::ReadResponder()
:	mImageSize(0),
	mImageLocal(false)
{
}

void LLTextureCache::ReadResponder::setData(U8* data, S32 datasize,
											S32 imagesize, S32 imageformat,
											bool imagelocal)
{
	if (mFormattedImage.notNull())
	{
		llassert_always(mFormattedImage->getCodec() == imageformat);
		mFormattedImage->appendData(data, datasize);
	}
	else
	{
		mFormattedImage = LLImageFormatted::createFromType(imageformat);
		mFormattedImage->setData(data,datasize);
	}
	mImageSize = imagesize;
	mImageLocal = imagelocal;
}

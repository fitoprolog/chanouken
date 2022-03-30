/**
 * @file llaudiodecodemgr.cpp
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include <iterator> // for VS2010
#include <deque>

#include "llaudiodecodemgr.h"

#include "llassetstorage.h"
#include "llatomic.h"
#include "llaudioengine.h"
#include "lldir.h"
#include "llendianswizzle.h"
#include "llfilesystem.h"
#include "lllfsthread.h"
#include "llmemory.h"
#include "llrefcount.h"
#include "llstring.h"

#include "llvorbisencode.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

extern LLAudioEngine* gAudiop;

LLAudioDecodeMgr* gAudioDecodeMgrp = NULL;

constexpr S32 WAV_HEADER_SIZE = 44;

//////////////////////////////////////////////////////////////////////////////

class LLVorbisDecodeState : public LLRefCount
{
protected:
	LOG_CLASS(LLVorbisDecodeState);

	virtual ~LLVorbisDecodeState();

public:
	class WriteResponder : public LLLFSThread::Responder
	{
	public:
		WriteResponder(LLVorbisDecodeState* decoder)
		:	mDecoder(decoder)
		{
		}

		void completed(S32 bytes) override
		{
			mDecoder->ioComplete(bytes);
		}

	public:
		LLPointer<LLVorbisDecodeState> mDecoder;
	};

	LLVorbisDecodeState(const LLUUID& uuid, const std::string& out_filename);

	bool initDecode();
	bool decodeSection(); // Return true if done.
	bool finishDecode();

	void flushBadFile();

	LL_INLINE void ioComplete(S32 bytes)		{ mBytesRead = bytes; }
	LL_INLINE bool isValid() const				{ return mValid; }
	LL_INLINE bool isDone() const				{ return mDone; }
	LL_INLINE const LLUUID& getUUID() const		{ return mUUID; }

protected:
	LLFileSystem*			mInFilep;

	std::string				mOutFilename;

	std::vector<U8>			mWAVBuffer;

	LLUUID					mUUID;

	LLLFSThread::handle_t	mFileHandle;
	OggVorbis_File			mVF;
	LLAtomicS32				mBytesRead;

	S32						mCurrentSection;

	bool					mValid;
	bool					mDone;
};

size_t cache_read(void* ptr, size_t size, size_t nmemb, void* datasource)
{
	if (size <= 0 || nmemb <= 0) return 0;

	LLFileSystem* file = (LLFileSystem*)datasource;

	if (file->read((U8*)ptr, (S32)(size * nmemb)))
	{
		S32 read = file->getLastBytesRead();
		return read / size;
	}
	else
	{
		return 0;
	}
}

S32 cache_seek(void* datasource, ogg_int64_t offset, S32 whence)
{
	LLFileSystem* file = (LLFileSystem*)datasource;

	// Cache has 31 bits files
	if (offset > S32_MAX)
	{
		return -1;
	}

	S32 origin = 0;
	switch (whence)
	{
		case SEEK_SET:
			break;

		case SEEK_END:
			origin = file->getSize();
			break;

		case SEEK_CUR:
			origin = -1;
			break;

		default:
			llerrs << "Invalid whence argument" << llendl;
	}

	return file->seek((S32)offset, origin) ? 0 : -1;
}

S32 cache_close(void* datasource)
{
	LLFileSystem* file = (LLFileSystem*)datasource;
	delete file;
	return 0;
}

long cache_tell(void* datasource)
{
	return ((LLFileSystem*)datasource)->tell();
}

LLVorbisDecodeState::LLVorbisDecodeState(const LLUUID& uuid,
										 const std::string& out_filename)
:	mUUID(uuid),
	mOutFilename(out_filename),
	mFileHandle(LLLFSThread::nullHandle()),
	mInFilep(NULL),
	mBytesRead(-1),
	mCurrentSection(0),
	mDone(false),
	mValid(false)
{
	// No default value for mVF, is it an OGG structure ?
}

LLVorbisDecodeState::~LLVorbisDecodeState()
{
	if (!mDone)
	{
		delete mInFilep;
		mInFilep = NULL;
	}
}

bool LLVorbisDecodeState::initDecode()
{
	ov_callbacks cache_callbacks;
	cache_callbacks.read_func = cache_read;
	cache_callbacks.seek_func = cache_seek;
	cache_callbacks.close_func = cache_close;
	cache_callbacks.tell_func = cache_tell;

	mInFilep = new LLFileSystem(mUUID);
	if (!mInFilep || !mInFilep->getSize())
	{
		llwarns << "Unable to open vorbis source VFile for reading" << llendl;
		delete mInFilep;
		mInFilep = NULL;
		return false;
	}

	S32 r = ov_open_callbacks(mInFilep, &mVF, NULL, 0, cache_callbacks);
	if (r < 0)
	{
		llwarns << r
				<< ": input to Vorbis decode does not appear to be an Ogg bitstream: "
				<< mUUID << llendl;
		return false;
	}

	S32 sample_count = ov_pcm_total(&mVF, -1);
	size_t size_guess = (size_t)sample_count;
	vorbis_info* vi = ov_info(&mVF, -1);
	size_guess *= vi->channels;
	size_guess *= 2;
	size_guess += 2048;

	bool abort_decode = false;

	if (vi->channels < 1 || vi->channels > LLVORBIS_CLIP_MAX_CHANNELS)
	{
		abort_decode = true;
		llwarns << "Bad channel count: " << vi->channels << llendl;
	}

	if ((size_t)sample_count > LLVORBIS_CLIP_REJECT_SAMPLES)
	{
		abort_decode = true;
		llwarns << "Illegal sample count: " << sample_count << llendl;
	}

	if (size_guess > LLVORBIS_CLIP_REJECT_SIZE)
	{
		abort_decode = true;
		llwarns << "Illegal sample size: " << size_guess << llendl;
	}

	if (abort_decode)
	{
		llwarns << "Cancelling initDecode. Bad asset: " << mUUID
				<< "Bad asset encoded by: "
				<< ov_comment(&mVF, -1)->vendor << llendl;
		delete mInFilep;
		mInFilep = NULL;
		return false;
	}

	try
	{
		mWAVBuffer.reserve(size_guess);
		mWAVBuffer.resize(WAV_HEADER_SIZE);
	}
	catch (const std::bad_alloc&)
	{
		llwarns << "Failure to allocate buffer for asset: " << mUUID << llendl;
		LLMemory::allocationFailed(size_guess);
		delete mInFilep;
		mInFilep = NULL;
		return false;
	}

	// Write the .wav format header

	// "RIFF"
	mWAVBuffer[0] = 0x52;
	mWAVBuffer[1] = 0x49;
	mWAVBuffer[2] = 0x46;
	mWAVBuffer[3] = 0x46;

	// Length = datalen + 36 (to be filled in later)
	mWAVBuffer[4] = 0x00;
	mWAVBuffer[5] = 0x00;
	mWAVBuffer[6] = 0x00;
	mWAVBuffer[7] = 0x00;

	// "WAVE"
	mWAVBuffer[8] = 0x57;
	mWAVBuffer[9] = 0x41;
	mWAVBuffer[10] = 0x56;
	mWAVBuffer[11] = 0x45;

	// "fmt "
	mWAVBuffer[12] = 0x66;
	mWAVBuffer[13] = 0x6D;
	mWAVBuffer[14] = 0x74;
	mWAVBuffer[15] = 0x20;

	// Chunk size = 16
	mWAVBuffer[16] = 0x10;
	mWAVBuffer[17] = 0x00;
	mWAVBuffer[18] = 0x00;
	mWAVBuffer[19] = 0x00;

	// Format (1 = PCM)
	mWAVBuffer[20] = 0x01;
	mWAVBuffer[21] = 0x00;

	// Number of channels
	mWAVBuffer[22] = 0x01;
	mWAVBuffer[23] = 0x00;

	// Samples per second
	mWAVBuffer[24] = 0x44;
	mWAVBuffer[25] = 0xAC;
	mWAVBuffer[26] = 0x00;
	mWAVBuffer[27] = 0x00;

	// Average bytes per second
	mWAVBuffer[28] = 0x88;
	mWAVBuffer[29] = 0x58;
	mWAVBuffer[30] = 0x01;
	mWAVBuffer[31] = 0x00;

	// Bytes to output at a single time
	mWAVBuffer[32] = 0x02;
	mWAVBuffer[33] = 0x00;

	// 16 bits per sample
	mWAVBuffer[34] = 0x10;
	mWAVBuffer[35] = 0x00;

	// "data"
	mWAVBuffer[36] = 0x64;
	mWAVBuffer[37] = 0x61;
	mWAVBuffer[38] = 0x74;
	mWAVBuffer[39] = 0x61;

	// These are the length of the data chunk, to be filled in later
	mWAVBuffer[40] = 0x00;
	mWAVBuffer[41] = 0x00;
	mWAVBuffer[42] = 0x00;
	mWAVBuffer[43] = 0x00;

#if 0
	char** ptr = ov_comment(&mVF, -1)->user_comments;
	vorbis_info* vi = ov_info(&vf, -1);
	while (*ptr)
	{
		fprintf(stderr,"%s\n", *ptr++);
	}
	fprintf(stderr, "\nBitstream is %d channel, %ldHz\n", vi->channels,
			vi->rate);
	fprintf(stderr, "\nDecoded length: %ld samples\n",
			(long)ov_pcm_total(&vf, -1));
	fprintf(stderr, "Encoded by: %s\n\n", ov_comment(&vf, -1)->vendor);
#endif

	return true;
}

bool LLVorbisDecodeState::decodeSection()
{
	if (!mInFilep)
	{
		llwarns << "No cache file to decode in vorbis !" << llendl;
		return true;
	}
	if (mDone)
	{
 		LL_DEBUGS("Audio") << "Already done with decode, aborting !"
						   << LL_ENDL;
		return true;
	}

	char pcmout[4096];
	bool eof = false;
	long ret = ov_read(&mVF, pcmout, sizeof(pcmout), 0, 2, 1,
					   &mCurrentSection);
	if (ret == 0)
	{
#if 0
		LL_DEBUGS("Audio") << "Vorbis EOF" << LL_ENDL;
#endif
		// EOF
		eof = true;
		mDone = true;
		mValid = true;
	}
	else if (ret < 0)
	{
		// Error in the stream. Not a problem, just reporting it in case we
		// (the app) cares. In this case, we don't.
		llwarns << "Bad vorbis decode." << llendl;

		mValid = false;
		mDone = true;

		// We are done, return true.
		return true;
	}
	else
	{
#if 0
		LL_DEBUGS("Audio") << "Vorbis read " << ret << "bytes" << LL_ENDL;
#endif
		// We do not bother dealing with sample rate changes, etc, but you will
		// have to
		std::copy(pcmout, pcmout + ret, std::back_inserter(mWAVBuffer));
	}
	return eof;
}

bool LLVorbisDecodeState::finishDecode()
{
	if (!isValid())
	{
		llwarns_once << "Bogus vorbis decode state for " << getUUID()
					 << ", aborting !" << llendl;
		return true; // We've finished
	}

	if (mFileHandle == LLLFSThread::nullHandle())
	{
		ov_clear(&mVF);

		// Write "data" chunk length, in little-endian format
		S32 data_length = mWAVBuffer.size() - WAV_HEADER_SIZE;
		mWAVBuffer[40] = (data_length) & 0x000000FF;
		mWAVBuffer[41] = (data_length >> 8) & 0x000000FF;
		mWAVBuffer[42] = (data_length >> 16) & 0x000000FF;
		mWAVBuffer[43] = (data_length >> 24) & 0x000000FF;

		// Write overall "RIFF" length, in little-endian format
		data_length += 36;
		mWAVBuffer[4] = (data_length) & 0x000000FF;
		mWAVBuffer[5] = (data_length >> 8) & 0x000000FF;
		mWAVBuffer[6] = (data_length >> 16) & 0x000000FF;
		mWAVBuffer[7] = (data_length >> 24) & 0x000000FF;

		// Vorbis encode/decode messes up loop point transitions (pop)do a
		// cheap-and-cheesy crossfade
		S16* samplep;
		char pcmout[4096];

		S32 fade_length = llmin(128, (data_length - 36) / 8);
		if ((S32)mWAVBuffer.size() >= WAV_HEADER_SIZE + 2 * fade_length)
		{
			memcpy(pcmout, &mWAVBuffer[WAV_HEADER_SIZE], 2 * fade_length);
		}
		llendianswizzle(&pcmout, 2, fade_length);

		samplep = (S16*)pcmout;
		for (S32 i = 0 ; i < fade_length; ++i)
		{
			*samplep = llfloor(F32(*samplep) * (F32)i / (F32)fade_length);
			++samplep;
		}

		llendianswizzle(&pcmout, 2, fade_length);
		if (WAV_HEADER_SIZE + 2 * fade_length < (S32)mWAVBuffer.size())
		{
			memcpy(&mWAVBuffer[WAV_HEADER_SIZE], pcmout, 2 * fade_length);
		}
		S32 near_end = mWAVBuffer.size() - 2 * fade_length;
		if ((S32)mWAVBuffer.size() >= near_end + 2 * fade_length)
		{
			memcpy(pcmout, &mWAVBuffer[near_end], (2 * fade_length));
		}
		llendianswizzle(&pcmout, 2, fade_length);
		samplep = (S16*)pcmout;
		for (S32 i = fade_length - 1; i >= 0; --i)
		{
			*samplep = llfloor(F32(*samplep) * (F32)i / (F32)fade_length);
			++samplep;
		}

		llendianswizzle(&pcmout, 2, fade_length);
		if (near_end + 2 * fade_length < (S32)mWAVBuffer.size())
		{
			memcpy(&mWAVBuffer[near_end], pcmout, 2 * fade_length);
		}

		if (data_length == 36)
		{
			llwarns_once << "Bad Vorbis decode for " << getUUID()
						 << ", aborting." << llendl;
			mValid = false;
			return true; // We have finished
		}
		mBytesRead = -1;
		mFileHandle = LLLFSThread::sLocal->write(mOutFilename,
												 &mWAVBuffer[0], 0,
												 mWAVBuffer.size(),
												 new WriteResponder(this));
	}

	if (mFileHandle != LLLFSThread::nullHandle())
	{
		if (mBytesRead < 0)
		{
			return false; // Not done
		}
		if (mBytesRead == 0)
		{
			llwarns_once << "Unable to write file for " << getUUID() << llendl;
			mValid = false;
			return true; // We have finished
		}
	}

	mDone = true;

#if 0
	LL_DEBUGS("Audio") << "Finished decode for " << getUUID() << LL_ENDL;
#endif

	return true;
}

void LLVorbisDecodeState::flushBadFile()
{
	if (mInFilep)
	{
		llwarns << "Flushing bad vorbis file from cache for " << mUUID
				<< llendl;
		mInFilep->remove();
		delete mInFilep;
		mInFilep = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////

class LLAudioDecodeMgr::Impl
{
	friend class LLAudioDecodeMgr;

protected:
	LOG_CLASS(LLAudioDecodeMgr::Impl);

public:
	Impl() = default;

	void processQueue(F32 num_secs = 0.005f);

protected:
	LLPointer<LLVorbisDecodeState>	mCurrentDecodep;
	std::deque<LLUUID>				mDecodeQueue;
	uuid_list_t						mBadAssetList;
};

void LLAudioDecodeMgr::Impl::processQueue(F32 num_secs)
{
	LLUUID uuid;

	LLTimer decode_timer;

	bool done = false;
	while (!done)
	{
		if (mCurrentDecodep)
		{
			bool res;
			// Decode in a loop until we are done or have run out of time.
			while (!(res = mCurrentDecodep->decodeSection()) &&
				   decode_timer.getElapsedTimeF32() < num_secs) ;

			if (mCurrentDecodep->isDone() && !mCurrentDecodep->isValid())
			{
				// We had an error when decoding, abort.
				LLUUID asset_id = mCurrentDecodep->getUUID();
				llwarns << asset_id
						<< " has invalid vorbis data, aborting decode"
						<< llendl;
				mCurrentDecodep->flushBadFile();
				if (gAudiop)
				{
					LLAudioData* adp = gAudiop->getAudioData(asset_id);
					adp->setHasValidData(false);
				}
				mBadAssetList.emplace(asset_id);
				mCurrentDecodep = NULL;
				done = true;
			}

			if (!res)
			{
				// We have used up out time slice, bail...
				done = true;
			}
			else if (mCurrentDecodep)
			{
				if (gAudiop && mCurrentDecodep->finishDecode())
				{
					// We finished !
					if (mCurrentDecodep->isValid() &&
						mCurrentDecodep->isDone())
					{
						LLAudioData* adp;
						adp = gAudiop->getAudioData(mCurrentDecodep->getUUID());
						adp->setHasDecodedData(true);
						adp->setHasValidData(true);
						// At this point, we could see if anyone needs this
						// sound immediately, but I am not sure that there is a
						// reason to; we need to poll all of the playing sounds
						// anyway.
					}
					else
					{
						llwarns_once << "Vorbis decode failed for "
									 << mCurrentDecodep->getUUID() << llendl;
					}
					mCurrentDecodep = NULL;
				}
				done = true; // done for now
			}
		}

		if (!done)
		{
			if (mDecodeQueue.empty())
			{
				// Nothing else on the queue.
				done = true;
			}
			else
			{
				LLUUID uuid = mDecodeQueue.front();
				mDecodeQueue.pop_front();
				if (!gAudiop || gAudiop->hasDecodedFile(uuid))
				{
					// This file has already been decoded, do not decode it
					// again.
					done = true;
					continue;
				}

				LL_DEBUGS("Audio") << "Decoding " << uuid
								   << " from audio queue !" << LL_ENDL;

				std::string uuid_str;
				std::string d_path;

				LLTimer timer;
				timer.reset();

				uuid.toString(uuid_str);
				d_path = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
														uuid_str) + ".dsf";

				mCurrentDecodep = new LLVorbisDecodeState(uuid, d_path);
				if (!mCurrentDecodep->initDecode())
				{
					// We had an error when decoding, abort.
					LLUUID asset_id = mCurrentDecodep->getUUID();
					llwarns << asset_id
							<< " has invalid vorbis data, aborting decode"
							<< llendl;
					mCurrentDecodep->flushBadFile();
					if (gAudiop)
					{
						LLAudioData* adp = gAudiop->getAudioData(asset_id);
						adp->setHasValidData(false);
					}
					mBadAssetList.emplace(asset_id);
					mCurrentDecodep = NULL;
					done = true;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

LLAudioDecodeMgr::LLAudioDecodeMgr()
{
	mImpl = new Impl;
}

LLAudioDecodeMgr::~LLAudioDecodeMgr()
{
	delete mImpl;
}

void LLAudioDecodeMgr::processQueue(F32 num_secs)
{
	mImpl->processQueue(num_secs);
}

bool LLAudioDecodeMgr::addDecodeRequest(const LLUUID& uuid)
{
	if (mImpl->mBadAssetList.count(uuid))
	{
		// Do not try to decode identified corrupted assets.
		return false;
	}

	if (gAudiop && gAudiop->hasDecodedFile(uuid))
	{
		// Already have a decoded version, we do not need to decode it.
		return true;
	}

	if (gAssetStoragep &&
		gAssetStoragep->hasLocalAsset(uuid, LLAssetType::AT_SOUND))
	{
		// Just put it on the decode queue if not already there.
		std::deque<LLUUID>::iterator end = mImpl->mDecodeQueue.end();
		if (std::find(mImpl->mDecodeQueue.begin(), end, uuid) == end)
		{
			mImpl->mDecodeQueue.push_back(uuid);
		}
		return true;
	}

	return false;
}

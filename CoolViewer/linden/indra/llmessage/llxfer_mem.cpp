/**
 * @file llxfer_mem.cpp
 * @brief implementation of LLXfer_Mem class for a single xfer
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

#include "llxfer_mem.h"

#include "llmath.h"

LLXfer_Mem::LLXfer_Mem()
:	LLXfer(-1)
{
	init();
}

LLXfer_Mem::~LLXfer_Mem()
{
	cleanup();
}

void LLXfer_Mem::init()
{
	mRemoteFilename.clear();
	mRemotePath = LL_PATH_NONE;
	mDeleteRemoteOnCompletion = false;
}

void LLXfer_Mem::cleanup()
{
	LLXfer::cleanup();
}

void LLXfer_Mem::setXferSize(S32 xfer_size)
{
	mXferSize = xfer_size;

	delete[] mBuffer;
	mBuffer = new char[xfer_size];

	mBufferLength = 0;
	mBufferStartOffset = 0;
	mBufferContainsEOF = true;
}

S32 LLXfer_Mem::startSend(U64 xfer_id, const LLHost& remote_host)
{
	S32 retval = LL_ERR_NOERR;  // presume success

	if (mXferSize <= 0)
	{
		return LL_ERR_FILE_EMPTY;
	}

    mRemoteHost = remote_host;
	mID = xfer_id;
   	mPacketNum = -1;

	mStatus = e_LL_XFER_PENDING;

	return retval;
}

S32 LLXfer_Mem::processEOF()
{
	S32 retval = 0;

	mStatus = e_LL_XFER_COMPLETE;

	llinfos << "xfer complete: " << getFileName() << llendl;

	if (mCallback)
	{
		mCallback((void*)mBuffer, mBufferLength, mCallbackDataHandle,
				  mCallbackResult, LLExtStat::NONE);
	}

	return retval;
}

S32 LLXfer_Mem::initializeRequest(U64 xfer_id,
								  const std::string& remote_filename,
								  ELLPath remote_path,
								  const LLHost& remote_host,
								  bool delete_remote_on_completion,
								  void (*callback)(void*, S32, void**, S32, LLExtStat),
								  void** user_data)
{
 	S32 retval = 0;  // presume success

	mRemoteHost = remote_host;

	// create a temp filename string using a GUID
	mID = xfer_id;
	mCallback = callback;
	mCallbackDataHandle = user_data;
	mCallbackResult = LL_ERR_NOERR;

	mRemoteFilename = remote_filename;
	mRemotePath = remote_path;
	mDeleteRemoteOnCompletion = delete_remote_on_completion;

	llinfos << "Requesting file: " << remote_filename << llendl;

	if (mBuffer)
	{
		delete[] mBuffer;
		mBuffer = NULL;
	}

	mBufferLength = 0;
	mPacketNum = 0;
 	mStatus = e_LL_XFER_PENDING;
	return retval;
}

S32 LLXfer_Mem::startDownload()
{
 	S32 retval = 0;  // presume success
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_RequestXfer);
	msg->nextBlockFast(_PREHASH_XferID);
	msg->addU64Fast(_PREHASH_ID, mID);
	msg->addStringFast(_PREHASH_Filename, mRemoteFilename);
	msg->addU8("FilePath", (U8) mRemotePath);
	msg->addBool("DeleteOnCompletion", mDeleteRemoteOnCompletion);
	msg->addBool("UseBigPackets", mChunkSize == LL_XFER_LARGE_PAYLOAD);
	msg->addUUIDFast(_PREHASH_VFileID, LLUUID::null);
	msg->addS16Fast(_PREHASH_VFileType, -1);

	msg->sendReliable(mRemoteHost);
	mStatus = e_LL_XFER_IN_PROGRESS;

	return retval;
}

U32 LLXfer_Mem::getXferTypeTag()
{
	return LLXfer::XFER_MEM;
}

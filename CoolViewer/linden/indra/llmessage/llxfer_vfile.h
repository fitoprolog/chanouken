/**
 * @file llxfer_vfile.h
 * @brief definition of LLXfer_VFile class for a single xfer_vfile.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLXFER_VFILE_H
#define LL_LLXFER_VFILE_H

#include "llassetstorage.h"
#include "llpreprocessor.h"
#include "llxfer.h"

class LLFileSystem;

class LLXfer_VFile : public LLXfer
{
protected:
	LOG_CLASS(LLXfer_VFile);

public:
	LLXfer_VFile();
	LLXfer_VFile(const LLUUID& local_id, LLAssetType::EType type);
	virtual ~LLXfer_VFile();

	virtual void init(const LLUUID& local_id, LLAssetType::EType type);
	virtual void cleanup();

	virtual S32 initializeRequest(U64 xfer_id, const LLUUID& local_id,
								  const LLUUID& remote_id,
								  const LLAssetType::EType type,
								  const LLHost& remote_host,
								  void (*callback)(void**, S32, LLExtStat),
								  void** user_data);
	virtual S32 startDownload();

	virtual S32 processEOF();

	virtual S32 startSend(U64 xfer_id, const LLHost& remote_host);
	virtual void closeFileHandle();
	virtual S32 reopenFileHandle();

	virtual S32 suck(S32 start_position);
	virtual S32 flush();

	LL_INLINE virtual bool matchesLocalFile(const LLUUID& id,
											LLAssetType::EType type)
	{
		return id == mLocalID && type == mType;
	}

	LL_INLINE virtual bool matchesRemoteFile(const LLUUID& id,
											 LLAssetType::EType type)
	{
		return id == mRemoteID && type == mType;
	}

	virtual void setXferSize(S32 xfer_size);

	LL_INLINE virtual S32 getMaxBufferSize()		{ return LL_MAX_XFER_FILE_BUFFER; }


	// Hacky: doesn't matter what this is as long as it's different from the
	// other classes:
	LL_INLINE virtual U32 getXferTypeTag()			{ return LLXfer::XFER_VFILE; }

	LL_INLINE virtual std::string getFileName()		{ return mName; }

protected:
	LLUUID				mLocalID;
	LLUUID				mRemoteID;
	LLUUID				mTempID;
	LLAssetType::EType	mType;

	LLFileSystem*		mVFile;

	std::string			mName;

	bool				mDeleteTempFile;
};

#endif

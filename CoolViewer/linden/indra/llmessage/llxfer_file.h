/**
 * @file llxfer_file.h
 * @brief definition of LLXfer_File class for a single xfer_file.
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

#ifndef LL_LLXFER_FILE_H
#define LL_LLXFER_FILE_H

#include "lldir.h"
#include "llpreprocessor.h"
#include "llxfer.h"

class LLXfer_File : public LLXfer
{
protected:
	LOG_CLASS(LLXfer_File);

public:
	LLXfer_File(S32 chunk_size);
	LLXfer_File(const std::string& local_filename,
				bool delete_local_on_completion, S32 chunk_size);
	virtual ~LLXfer_File();

	virtual void init(const std::string& local_filename,
					  bool delete_local_on_completion, S32 chunk_size);
	virtual void cleanup();

	virtual S32 initializeRequest(U64 xfer_id,
								  const std::string& local_filename,
								  const std::string& remote_filename,
								  ELLPath remote_path,
								  const LLHost& remote_host,
								  bool delete_remote_on_completion,
								  void (*callback)(void**, S32, LLExtStat),
								  void** user_data);
	virtual S32 startDownload();

	virtual S32 processEOF();

	virtual S32 startSend(U64 xfer_id, const LLHost& remote_host);
	virtual void closeFileHandle();
	virtual S32 reopenFileHandle();

	virtual S32 suck(S32 start_position);
	virtual S32 flush();

	LL_INLINE virtual bool matchesLocalFilename(const std::string& filename)
	{
		return filename == mLocalFilename;
	}

	LL_INLINE virtual bool matchesRemoteFilename(const std::string& filename,
												 ELLPath remote_path)
	{
		return filename == mRemoteFilename && remote_path == mRemotePath;
	}

	LL_INLINE virtual S32  getMaxBufferSize()		{ return LL_MAX_XFER_FILE_BUFFER; }

	// Hacky: doesn't matter what this is as long as it's different from the
	// other classes:
	LL_INLINE virtual U32 getXferTypeTag()			{ return LLXfer::XFER_FILE; }

	LL_INLINE virtual std::string getFileName()		{ return mLocalFilename; }

protected:
 	LLFILE*		mFp;
	ELLPath		mRemotePath;
	bool		mDeleteLocalOnCompletion;
	bool		mDeleteRemoteOnCompletion;
	std::string	mLocalFilename;
	std::string	mRemoteFilename;
	std::string	mTempFilename;
};

#endif

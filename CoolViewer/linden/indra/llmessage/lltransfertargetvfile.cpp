/**
 * @file lltransfertargetvfile.cpp
 * @brief Transfer system for receiving a vfile.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "lltransfertargetvfile.h"

#include "lldatapacker.h"
#include "llfilesystem.h"

//static
void LLTransferTargetVFile::updateQueue(bool shutdown)
{
}

LLTransferTargetParamsVFile::LLTransferTargetParamsVFile()
:	LLTransferTargetParams(LLTTT_VFILE),
	mAssetType(LLAssetType::AT_NONE),
	mCompleteCallback(NULL),
	mRequestDatap(NULL),
	mErrCode(0)
{
}

void LLTransferTargetParamsVFile::setAsset(const LLUUID& asset_id,
										   LLAssetType::EType asset_type)
{
	mAssetID = asset_id;
	mAssetType = asset_type;
}

void LLTransferTargetParamsVFile::setCallback(LLTTVFCompleteCallback cb,
											  LLBaseDownloadRequest& request)
{
	mCompleteCallback = cb;
	if (mRequestDatap)
	{
		delete mRequestDatap;
	}
	mRequestDatap = request.getCopy();
}

bool LLTransferTargetParamsVFile::unpackParams(LLDataPacker& dp)
{
	// if the source provided a new key, assign that to the asset id.
	if (dp.hasNext())
	{
		LLUUID dummy_id;
		dp.unpackUUID(dummy_id, "AgentID");
		dp.unpackUUID(dummy_id, "SessionID");
		dp.unpackUUID(dummy_id, "OwnerID");
		dp.unpackUUID(dummy_id, "TaskID");
		dp.unpackUUID(dummy_id, "ItemID");
		dp.unpackUUID(mAssetID, "AssetID");
		S32 dummy_type;
		dp.unpackS32(dummy_type, "AssetType");
	}

	// if we never got an asset id, this will always fail.
	if (mAssetID.isNull())
	{
		return false;
	}
	return true;
}

LLTransferTargetVFile::LLTransferTargetVFile(const LLUUID& uuid,
											 LLTransferSourceType src_type)
:	LLTransferTarget(LLTTT_VFILE, uuid, src_type),
	mNeedsCreate(true)
{
	mTempID.generate();
}

LLTransferTargetVFile::~LLTransferTargetVFile()
{
	if (mParams.mRequestDatap)
	{
		// *TODO: consider doing it in LLTransferTargetParamsVFile's destructor
		delete mParams.mRequestDatap;
		mParams.mRequestDatap = NULL;
	}
}

// virtual
bool LLTransferTargetVFile::unpackParams(LLDataPacker& dp)
{
	if (LLTST_SIM_INV_ITEM == mSourceType)
	{
		return mParams.unpackParams(dp);
	}
	return true;
}

void LLTransferTargetVFile::applyParams(const LLTransferTargetParams& params)
{
	if (params.getType() != mType)
	{
		llwarns << "Target parameter type doesn't match!" << llendl;
		return;
	}

	mParams = (LLTransferTargetParamsVFile&)params;
}

LLTSCode LLTransferTargetVFile::dataCallback(S32 packet_id, U8* in_datap,
											 S32 in_size)
{
	if (!gAssetStoragep)
	{
		llwarns << "Aborting vfile transfer after asset storage shut down !"
				<< llendl;
		return LLTS_ERROR;
	}

	LLFileSystem vf(mTempID, LLFileSystem::APPEND);
	if (mNeedsCreate)
	{
		mNeedsCreate = false;
	}

	if (!in_size)
	{
		return LLTS_OK;
	}

	if (!vf.write(in_datap, in_size))
	{
		llwarns << "Failure in data callback !" << llendl;
		return LLTS_ERROR;
	}

	return LLTS_OK;
}

void LLTransferTargetVFile::completionCallback(LLTSCode status)
{
	//llinfos << "LLTransferTargetVFile::completionCallback" << llendl;

	if (!gAssetStoragep)
	{
		llwarns << "Aborting vfile transfer after asset storage shut down !"
				<< llendl;
		return;
	}

	// Still need to gracefully handle error conditions.
	S32 err_code = 0;
	switch (status)
	{
		case LLTS_DONE:
		{
			if (!mNeedsCreate)
			{
				if (!LLFileSystem::renameFile(mTempID, mParams.getAssetID()))
				{
					llwarns << "Rename of cache file from temp asset Id "
							<< mTempID << " to asset Id "
							<< mParams.getAssetID() << " failed." << llendl;
				}
			}
			err_code = LL_ERR_NOERR;
			LL_DEBUGS("FileTransfer") << "Callback completed for "
									  << mParams.getAssetID() << ","
									  << LLAssetType::lookup(mParams.getAssetType())
									  << " with temp id " << mTempID
									  << LL_ENDL;
			break;
		}

		case LLTS_ERROR:
		case LLTS_ABORT:
		case LLTS_UNKNOWN_SOURCE:
		default:
		{
			// We are aborting this transfer, we do not want to keep this file.
			llwarns << "Aborting vfile transfer for " << mParams.getAssetID()
					<< llendl;
			LLFileSystem vf(mTempID, LLFileSystem::APPEND);
			vf.remove();
		}
	}

	switch (status)
	{
		case LLTS_DONE:
			err_code = LL_ERR_NOERR;
			break;

		case LLTS_UNKNOWN_SOURCE:
			err_code = LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE;
			break;

		case LLTS_INSUFFICIENT_PERMISSIONS:
			err_code = LL_ERR_INSUFFICIENT_PERMISSIONS;
			break;

		case LLTS_ERROR:
		case LLTS_ABORT:
		default:
			err_code = LL_ERR_ASSET_REQUEST_FAILED;
	}

	if (mParams.mRequestDatap)
	{
		if (mParams.mCompleteCallback)
		{
			mParams.mCompleteCallback(err_code, mParams.getAssetID(),
									  mParams.getAssetType(),
									  mParams.mRequestDatap, LLExtStat::NONE);
		}
		delete mParams.mRequestDatap;
		mParams.mRequestDatap = NULL;
	}
}

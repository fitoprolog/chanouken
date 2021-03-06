/**
 * @file llvlmanager.h
 * @brief LLVLManager class definition
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

#ifndef LL_LLVLMANAGER_H
#define LL_LLVLMANAGER_H

// This class manages the data coming in for viewer layers from the network.

#include <vector>

#include "llerror.h"

const char CLOUD_LAYER_CODE			= '8';
const char WIND_LAYER_CODE			= '7';
const char LAND_LAYER_CODE			= 'L';
const char WATER_LAYER_CODE			= 'W';

const char AURORA_CLOUD_LAYER_CODE	= ':';
const char AURORA_WIND_LAYER_CODE	= '9';
const char AURORA_LAND_LAYER_CODE	= 'M';
const char AURORA_WATER_LAYER_CODE	= 'X';

class LLVLData;
class LLViewerRegion;

class LLVLManager
{
protected:
	LOG_CLASS(LLVLManager);

public:
	LLVLManager();
	~LLVLManager();

	void addLayerData(LLVLData* vl_datap, S32 mesg_size);

	void unpackData(S32 num_packets = 10);

	LL_INLINE S32 getLandBits() const		{ return mLandBits; }
	LL_INLINE S32 getWindBits() const		{ return mWindBits; }
	LL_INLINE S32 getCloudBits() const		{ return mCloudBits; }

	LL_INLINE S32 getTotalBytes() const
	{
		return (mLandBits + mWindBits + mCloudBits) / 8;
	}

	LL_INLINE void resetBitCounts()
	{
		mLandBits = mWindBits = mCloudBits = 0;
	}

	void cleanupData(LLViewerRegion* regionp);
protected:

	std::vector<LLVLData*>	mPacketData;
	U32						mLandBits;
	U32						mWindBits;
	U32						mCloudBits;
};

class LLVLData
{
public:
	LL_INLINE LLVLData(LLViewerRegion* regionp, S8 type, U8* data, S32 size)
	:	mRegionp(regionp),
		mType(type),
		mData(data),
		mSize(size)
	{
	}

	LL_INLINE ~LLVLData()
	{
		delete[] mData;
		mData = NULL;
		mRegionp = NULL;
	}

public:
	LLViewerRegion*	mRegionp;
	U8*				mData;
	S32				mSize;
	S8				mType;
};

extern LLVLManager gVLManager;

#endif // LL_LLVIEWERLAYERMANAGER_H

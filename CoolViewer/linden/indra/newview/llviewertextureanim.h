/** 
 * @file llviewertextureanim.h
 * @brief LLViewerTextureAnim class header file
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

#ifndef LL_LLVIEWERTEXTUREANIM_H
#define LL_LLVIEWERTEXTUREANIM_H

#include "lltextureanim.h"
#include "llframetimer.h"

class LLVOVolume;

class LLViewerTextureAnim : public LLTextureAnim
{
protected:
	LOG_CLASS(LLViewerTextureAnim);

private:
	static std::vector<LLViewerTextureAnim*> sInstanceList;
	S32 mInstanceIndex;

public:
	static void initClass();
	static void updateClass();
	static void dumpStats();

	LLViewerTextureAnim(LLVOVolume* vobj);
	virtual ~LLViewerTextureAnim();

	void reset() override;

	S32 animateTextures(F32& off_s, F32& off_t, F32& scale_s, F32& scale_t,
						F32& rotate);
	enum
	{
		TRANSLATE = 0x01 // Result code JUST for animateTextures
	};

public:
	F32				mOffS;
	F32				mOffT;
	F32				mScaleS;
	F32				mScaleT;
	F32				mRot;

protected:
	LLVOVolume*		mVObj;
	LLFrameTimer	mTimer;
	F64				mLastTime;
	F32				mLastFrame;
};

#endif

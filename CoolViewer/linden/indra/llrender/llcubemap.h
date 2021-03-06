/**
 * @file llcubemap.h
 * @brief LLCubeMap class definition
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

#ifndef LL_LLCUBEMAP_H
#define LL_LLCUBEMAP_H

#include <vector>

#include "llgl.h"
#include "llimagegl.h"

class LLVector3;

// Environment map hack !
class LLCubeMap : public LLRefCount
{
	friend class LLTexUnit;

protected:
	LOG_CLASS(LLCubeMap);

	~LLCubeMap() override = default;

public:
	LLCubeMap(bool init_as_srgb = false);

	void init(const std::vector<LLPointer<LLImageRaw> >& rawimages);
	void initGL();
	void initRawData(const std::vector<LLPointer<LLImageRaw> >& rawimages);
	void initGLData();

	void bind();
	void enableTexture(S32 stage);
	void disableTexture();

	void setMatrix(S32 stage);
	void restoreMatrix();
	void setReflection();

	void finishPaint();

	GLuint getGLName();

	LLVector3 map(U8 side, U16 v_val, U16 h_val) const;
	bool project(F32& v_val, F32& h_val, bool& outside, U8 side,
				 const LLVector3& dir) const;
	bool project(F32& v_min, F32& v_max, F32& h_min, F32& h_max, U8 side,
				 LLVector3 dir[4]) const;
	void paintIn(LLVector3 dir[4], const LLColor4U& col);
	void destroyGL();

protected:
	S32						mTextureStage;
	S32						mMatrixStage;
	U32						mTargets[6];
	LLPointer<LLImageGL>	mImages[6];
	LLPointer<LLImageRaw>	mRawImages[6];
	bool					mIsSRGB;

public:
	static bool				sUseCubeMaps;
};

#endif

/**
 * @file lldrawpoolwater.h
 * @brief LLDrawPoolWater class definition
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

#ifndef LL_LLDRAWPOOLWATER_H
#define LL_LLDRAWPOOLWATER_H

#include "lldrawpool.h"

// Disabled (12/2021) due to occasional false-negative occlusion tests
// producing water reflection errors (SL-16461). If/when water occlusion
// queries become 100% reliable, re-enable this optimization.
#define LL_OPTIMIZED_WATER_OCCLUSIONS 0

class LLFace;
class LLGLSLShader;
class LLHeavenBody;
class LLWaterSurface;

class LLDrawPoolWater final : public LLFacePool
{
public:
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD0
	};

	LL_INLINE U32 getVertexDataMask() override				{ return VERTEX_DATA_MASK; }

	LLDrawPoolWater();

	static void restoreGL();

	LL_INLINE S32 getNumPostDeferredPasses() override		{ return 0; /*getNumPasses();*/ }
	void beginPostDeferredPass(S32 pass) override;
	void endPostDeferredPass(S32 pass) override;
	LL_INLINE void renderPostDeferred(S32 pass) override	{ render(pass); }
	LL_INLINE S32 getNumDeferredPasses() override			{ return 1; }
	void renderDeferred(S32 pass = 0) override;

	S32 getNumPasses() override;
	void render(S32 pass = 0) override;
	void prerender() override;

	void renderReflection(LLFace* face);
	void renderWater();

	void setOpaqueTexture(const LLUUID& tex_id);
	void setTransparentTextures(const LLUUID& tex1_id,
								const LLUUID& tex2_id = LLUUID::null);
	void setNormalMaps(const LLUUID& tex1_id,
					   const LLUUID& tex2_id = LLUUID::null);

protected:
	void renderOpaqueLegacyWater();

	// Windlight shading
	void renderWaterWL(LLGLSLShader* shader, F32 eyedepth);
	// Extended environment shading
	void renderWaterEE(LLGLSLShader* shader, bool edge, F32 eyedepth);

protected:
	LLPointer<LLViewerTexture>	mWaterImagep[2];
	LLPointer<LLViewerTexture>	mWaterNormp[2];
	LLPointer<LLViewerTexture>	mOpaqueWaterImagep;
	LLVector3					mLightDir;
	LLColor4					mLightColor;
	LLColor3					mLightDiffuse;
	F32							mLightExp;
	bool						mIsSunUp;

public:
	static LLColor4				sWaterFogColor;
	static bool					sNeedsReflectionUpdate;
	static bool					sNeedsTexturesReload;
};

#endif // LL_LLDRAWPOOLWATER_H

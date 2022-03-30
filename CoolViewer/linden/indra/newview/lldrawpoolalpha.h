/**
 * @file lldrawpoolalpha.h
 * @brief LLDrawPoolAlpha class definition
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

#ifndef LL_LLDRAWPOOLALPHA_H
#define LL_LLDRAWPOOLALPHA_H

#include "lldrawpool.h"
#include "llframetimer.h"
#include "llrender.h"

class LLFace;
class LLColor4;
class LLGLSLShader;
class LLTexUnit;

class LLDrawPoolAlpha final : public LLRenderPass
{
protected:
	LOG_CLASS(LLDrawPoolAlpha);

public:
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_COLOR |
							LLVertexBuffer::MAP_TEXCOORD0
	};
	LL_INLINE U32 getVertexDataMask() override	{ return VERTEX_DATA_MASK; }

	LLDrawPoolAlpha(U32 type = LLDrawPool::POOL_ALPHA);

	S32 getNumPostDeferredPasses() override;
	void beginPostDeferredPass(S32 pass) override;
	void endPostDeferredPass(S32 pass) override;
	void renderPostDeferred(S32 pass) override;

	void beginRenderPass(S32 pass = 0) override;
	void endRenderPass(S32 pass) override;
	LL_INLINE S32 getNumPasses() override		{ return 1; }

	void render(S32 pass = 0) override;
	void prerender() override;

	void renderGroupAlpha(LLSpatialGroup* group, U32 type, U32 mask,
						  bool texture = true);
	void renderAlpha(U32 mask, S32 pass);
	void renderAlphaHighlight(U32 mask);

private:
	// Extented environment rendering methods
	void renderFullbrights(U32 mask, const std::vector<LLDrawInfo*>& fullb);
	void renderEmissives(U32 mask, const std::vector<LLDrawInfo*>& emissives);

	bool texSetup(LLDrawInfo* draw, bool use_shaders, bool use_material,
				  LLGLSLShader* shader, LLTexUnit* unit0);

	void renderAlphaEE(U32 mask, S32 pass);

	// Windlight environment rendering method
	void renderAlphaWL(U32 mask, S32 pass);

public:
	static bool sShowDebugAlpha;

private:
	LLGLSLShader* mCurrentShader;
	LLGLSLShader* mTargetShader;
	LLGLSLShader* mSimpleShader;
	LLGLSLShader* mFullbrightShader;
	LLGLSLShader* mEmissiveShader;

	// Our 'normal' alpha blend function for this pass
	LLRender::eBlendFactor mColorSFactor;
	LLRender::eBlendFactor mColorDFactor;
	LLRender::eBlendFactor mAlphaSFactor;
	LLRender::eBlendFactor mAlphaDFactor;
};

#endif // LL_LLDRAWPOOLALPHA_H

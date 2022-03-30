/**
 * @file lldrawpoolmaterials.h
 * @brief LLDrawPoolMaterials class definition
 * @author Jonathan "Geenz" Goodman
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#ifndef LL_LLDRAWPOOLMATERIALS_H
#define LL_LLDRAWPOOLMATERIALS_H

#include "llvertexbuffer.h"
#include "llvector2.h"
#include "llvector3.h"
#include "llcolor4u.h"

#include "lldrawpool.h"

class LLViewerTexture;
class LLDrawInfo;
class LLGLSLShader;

class LLDrawPoolMaterials final : public LLRenderPass
{
public:
	LLDrawPoolMaterials();

	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_TEXCOORD1 |
							LLVertexBuffer::MAP_TEXCOORD2 |
							LLVertexBuffer::MAP_COLOR |
							LLVertexBuffer::MAP_TANGENT
	};

	LL_INLINE U32 getVertexDataMask() override		{ return VERTEX_DATA_MASK; }

	LL_INLINE void render(S32 pass = 0) override	{}
	LL_INLINE S32 getNumPasses() override			{ return 0; }
	void prerender() override;

	LL_INLINE S32 getNumDeferredPasses() override	{ return 12; }
	void beginDeferredPass(S32 pass) override;
	void endDeferredPass(S32 pass) override;
	void renderDeferred(S32 pass) override;

	void bindSpecularMap(LLViewerTexture* tex);
	void bindNormalMap(LLViewerTexture* tex);

	void pushBatch(LLDrawInfo& params, U32 mask, bool texture,
				   bool batch_textures = false) override;

private:
	LLGLSLShader* mShader;
};

#endif	// LL_LLDRAWPOOLMATERIALS_H

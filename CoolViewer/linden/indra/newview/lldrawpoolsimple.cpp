/**
 * @file lldrawpoolsimple.cpp
 * @brief LLDrawPoolSimple class implementation
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

#include "llviewerprecompiledheaders.h"

#include "lldrawpoolsimple.h"

#include "llfasttimer.h"
#include "llrender.h"

#include "lldrawable.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewershadermgr.h"

static LLGLSLShader* sSimpleShader = NULL;
static LLGLSLShader* sFullbrightShader = NULL;
static LLGLSLShader* sPostDeferredShader = NULL;

void LLDrawPoolGlow::beginPostDeferredPass(S32 pass)
{
	gDeferredEmissiveProgram.bind();
	gDeferredEmissiveProgram.uniform1f(LLShaderMgr::TEXTURE_GAMMA, 2.2f);
	if (gUseNewShaders)
	{
		gDeferredEmissiveProgram.uniform1i(LLShaderMgr::NO_ATMO,
										   LLPipeline::sRenderingHUDs ? 1 : 0);
	}
}

void LLDrawPoolGlow::renderPostDeferred(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_GLOW);

	LLGLEnable blend(GL_BLEND);
	LLGLDisable test(GL_ALPHA_TEST);
	gGL.flush();
	// Get rid of Z-fighting with non-glow pass.
	LLGLEnable poly_offset(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.f, -1.f);
	gGL.setSceneBlendType(LLRender::BT_ADD);

	LLGLDepthTest depth(GL_TRUE, GL_FALSE);
	gGL.setColorMask(false, true);

	{
		LL_FAST_TIMER(FTM_RENDER_GLOW_PUSH);
		U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;
		pushBatches(LLRenderPass::PASS_GLOW, mask, true, true);
	}

	gGL.setColorMask(true, false);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	stop_glerror();
}

void LLDrawPoolGlow::endPostDeferredPass(S32 pass)
{
	gDeferredEmissiveProgram.unbind();
	LLRenderPass::endRenderPass(pass);
}

S32 LLDrawPoolGlow::getNumPasses()
{
	if (gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT) > 0)
	{
		return 1;
	}
	return 0;
}

void LLDrawPoolGlow::render(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_GLOW);

	LLGLEnable blend(GL_BLEND);
	LLGLDisable test(GL_ALPHA_TEST);
	gGL.flush();
	// Get rid of Z-fighting with non-glow pass.
	LLGLEnable poly_offset(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.f, -1.f);
	gGL.setSceneBlendType(LLRender::BT_ADD);

	U32 shader_level =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
	// Should never get here without basic shaders enabled
	llassert(shader_level > 0);

	LLGLSLShader* shader =
		LLPipeline::sUnderWaterRender ? &gObjectEmissiveWaterProgram
									  : &gObjectEmissiveProgram;
	shader->bind();
	if (LLPipeline::sRenderDeferred)
	{
		shader->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 2.2f);
	}
	else
	{
		shader->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 1.f);
	}

	if (gUseNewShaders)
	{
		shader->uniform1i(LLShaderMgr::NO_ATMO,
						  LLPipeline::sRenderingHUDs ? 1 : 0);
	}

	LLGLDepthTest depth(GL_TRUE, GL_FALSE);
	gGL.setColorMask(false, true);

	U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;
	pushBatches(LLRenderPass::PASS_GLOW, mask, true, true);

	gGL.setColorMask(true, false);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	if (shader_level > 0 && sFullbrightShader)
	{
		shader->unbind();
	}
	stop_glerror();
}

LLDrawPoolSimple::LLDrawPoolSimple()
:	LLRenderPass(POOL_SIMPLE)
{
}

void LLDrawPoolSimple::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

void LLDrawPoolSimple::beginRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_SIMPLE);

	if (LLPipeline::sImpostorRender)
	{
		sSimpleShader = &gObjectSimpleImpostorProgram;
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		sSimpleShader = &gObjectSimpleWaterProgram;
	}
	else
	{
		sSimpleShader = &gObjectSimpleProgram;
	}

	if (mShaderLevel > 0)
	{
		sSimpleShader->bind();
		if (gUseNewShaders)
		{
			sSimpleShader->uniform1i(LLShaderMgr::NO_ATMO,
									 LLPipeline::sRenderingHUDs ? 1 : 0);
		}
	}
	else
	{
		// Do not use shaders !
		LLGLSLShader::bindNoShader();
	}
}

void LLDrawPoolSimple::endRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_SIMPLE);
	LLRenderPass::endRenderPass(pass);
	if (mShaderLevel > 0)
	{
		sSimpleShader->unbind();
	}
}

void LLDrawPoolSimple::render(S32)
{
	LLGLDisable blend(GL_BLEND);

	LL_FAST_TIMER(FTM_RENDER_SIMPLE);
	gPipeline.enableLightsDynamic();

	if (mShaderLevel > 0)
	{
		U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;

		pushBatches(LLRenderPass::PASS_SIMPLE, mask, true, true);

		if (LLPipeline::sRenderDeferred)
		{
			// If deferred rendering is enabled, bump faces are not registered
			// as simple render bump faces here as simple so bump faces will
			// appear under water
			pushBatches(LLRenderPass::PASS_BUMP, mask, true, true);
			pushBatches(LLRenderPass::PASS_MATERIAL, mask, true, true);
			pushBatches(LLRenderPass::PASS_SPECMAP, mask, true, true);
			pushBatches(LLRenderPass::PASS_NORMMAP, mask, true, true);
			pushBatches(LLRenderPass::PASS_NORMSPEC, mask, true, true);
		}
	}
	else
	{
		LLGLDisable alpha_test(GL_ALPHA_TEST);
		renderTexture(LLRenderPass::PASS_SIMPLE, getVertexDataMask());
	}
	stop_glerror();
}

//==============================================================
// LLDrawPoolAlphaMask & LLDrawPoolFullbrightAlphaMask
//==============================================================

LLDrawPoolAlphaMask::LLDrawPoolAlphaMask()
:	LLRenderPass(POOL_ALPHA_MASK)
{
}

void LLDrawPoolAlphaMask::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

void LLDrawPoolAlphaMask::beginRenderPass(S32 pass)
{
	LL_TRACY_TIMER(TRC_RENDER_ALPHA_MASK);

	if (LLPipeline::sUnderWaterRender)
	{
		sSimpleShader = &gObjectSimpleWaterAlphaMaskProgram;
	}
	else
	{
		sSimpleShader = &gObjectSimpleAlphaMaskProgram;
	}

	if (mShaderLevel > 0)
	{
		sSimpleShader->bind();
		if (gUseNewShaders)
		{
			sSimpleShader->uniform1i(LLShaderMgr::NO_ATMO,
									 LLPipeline::sRenderingHUDs ? 1 : 0);
		}
	}
	else
	{
		// Do not use shaders !
		LLGLSLShader::bindNoShader();
	}
}

void LLDrawPoolAlphaMask::endRenderPass(S32 pass)
{
	LL_TRACY_TIMER(TRC_RENDER_ALPHA_MASK);

	LLRenderPass::endRenderPass(pass);
	if (mShaderLevel > 0)
	{
		sSimpleShader->unbind();
	}
}

void LLDrawPoolAlphaMask::render(S32 pass)
{
	LL_TRACY_TIMER(TRC_RENDER_ALPHA_MASK);

	LLGLDisable blend(GL_BLEND);

	if (mShaderLevel > 0)
	{
		sSimpleShader->bind();
		sSimpleShader->setMinimumAlpha(0.33f);
		if (gUseNewShaders)
		{
			sSimpleShader->uniform1i(LLShaderMgr::NO_ATMO,
									 LLPipeline::sRenderingHUDs ? 1 : 0);
		}

		U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;
		pushMaskBatches(LLRenderPass::PASS_ALPHA_MASK, mask, true, true);
		pushMaskBatches(LLRenderPass::PASS_MATERIAL_ALPHA_MASK, mask, true,
						true);
		pushMaskBatches(LLRenderPass::PASS_SPECMAP_MASK, mask, true, true);
		pushMaskBatches(LLRenderPass::PASS_NORMMAP_MASK, mask, true, true);
		pushMaskBatches(LLRenderPass::PASS_NORMSPEC_MASK, mask, true, true);
	}
	else
	{
		LLGLEnable test(GL_ALPHA_TEST);
		pushMaskBatches(LLRenderPass::PASS_ALPHA_MASK, getVertexDataMask(),
						true, false);
	}
	stop_glerror();
}

LLDrawPoolFullbrightAlphaMask::LLDrawPoolFullbrightAlphaMask()
:	LLRenderPass(POOL_FULLBRIGHT_ALPHA_MASK)
{
}

void LLDrawPoolFullbrightAlphaMask::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

void LLDrawPoolFullbrightAlphaMask::beginRenderPass(S32)
{
	LL_TRACY_TIMER(TRC_RENDER_ALPHA_MASK);

	if (LLPipeline::sUnderWaterRender)
	{
		sSimpleShader = &gObjectFullbrightWaterAlphaMaskProgram;
	}
	else
	{
		sSimpleShader = &gObjectFullbrightAlphaMaskProgram;
	}

	if (mShaderLevel > 0)
	{
		sSimpleShader->bind();
		if (gUseNewShaders)
		{
			sSimpleShader->uniform1i(LLShaderMgr::NO_ATMO,
									 LLPipeline::sRenderingHUDs ? 1 : 0);
		}
	}
	else
	{
		// Do not use shaders !
		LLGLSLShader::bindNoShader();
	}
}

void LLDrawPoolFullbrightAlphaMask::endRenderPass(S32 pass)
{
	LL_TRACY_TIMER(TRC_RENDER_ALPHA_MASK);

	LLRenderPass::endRenderPass(pass);
	if (mShaderLevel > 0)
	{
		sSimpleShader->unbind();
	}
}

void LLDrawPoolFullbrightAlphaMask::render(S32)
{
	LL_TRACY_TIMER(TRC_RENDER_ALPHA_MASK);

	if (mShaderLevel > 0)
	{
		if (sSimpleShader)
		{
			sSimpleShader->bind();
			sSimpleShader->setMinimumAlpha(0.33f);
			bool render_hud = LLPipeline::sRenderingHUDs;
			if (gUseNewShaders)
			{
				sSimpleShader->uniform1i(LLShaderMgr::NO_ATMO,
										 render_hud ? 1 : 0);
			}
			if (render_hud || !LLPipeline::sRenderDeferred)
			{
				sSimpleShader->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 1.f);
			}
			else
			{
				sSimpleShader->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 2.2f);
			}
		}
		U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;
		pushMaskBatches(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK, mask, true,
						true);
#if 0
		LLGLSLShader::bindNoShader();
#endif
	}
	else
	{
		LLGLEnable test(GL_ALPHA_TEST);
		gPipeline.enableLightsFullbright();
		pushMaskBatches(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK,
						getVertexDataMask(), true, false);
		gPipeline.enableLightsDynamic();
	}
	stop_glerror();
}

//===============================
// DEFERRED IMPLEMENTATION
//===============================

void LLDrawPoolSimple::beginDeferredPass(S32)
{
	LL_FAST_TIMER(FTM_RENDER_SIMPLE);
	gDeferredDiffuseProgram.bind();
	if (gUseNewShaders)
	{
		gDeferredDiffuseProgram.uniform1i(LLShaderMgr::NO_ATMO,
										  LLPipeline::sRenderingHUDs ? 1 : 0);
	}
}

void LLDrawPoolSimple::endDeferredPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_SIMPLE);
	LLRenderPass::endRenderPass(pass);

	gDeferredDiffuseProgram.unbind();
}

void LLDrawPoolSimple::renderDeferred(S32)
{
	LLGLDisable blend(GL_BLEND);
	LLGLDisable alpha_test(GL_ALPHA_TEST);

	LL_FAST_TIMER(FTM_RENDER_SIMPLE);
	U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;
	pushBatches(LLRenderPass::PASS_SIMPLE, mask, true, true);
	stop_glerror();
}

void LLDrawPoolAlphaMask::renderDeferred(S32)
{
	LL_FAST_TIMER(FTM_RENDER_GRASS);

	gDeferredDiffuseAlphaMaskProgram.bind();
	gDeferredDiffuseAlphaMaskProgram.setMinimumAlpha(0.33f);

	if (gUseNewShaders)
	{
		gDeferredDiffuseAlphaMaskProgram.uniform1i(LLShaderMgr::NO_ATMO,
												   LLPipeline::sRenderingHUDs ?
													1 : 0);
	}

	U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;
	pushMaskBatches(LLRenderPass::PASS_ALPHA_MASK, mask, true, true);
	gDeferredDiffuseAlphaMaskProgram.unbind();
	stop_glerror();
}

// Grass drawpool
LLDrawPoolGrass::LLDrawPoolGrass()
:	LLRenderPass(POOL_GRASS)
{
}

void LLDrawPoolGrass::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

void LLDrawPoolGrass::beginRenderPass(S32)
{
	LL_FAST_TIMER(FTM_RENDER_GRASS);

	if (LLPipeline::sUnderWaterRender)
	{
		sSimpleShader = &gObjectAlphaMaskNonIndexedWaterProgram;
	}
	else
	{
		sSimpleShader = &gObjectAlphaMaskNonIndexedProgram;
	}

	if (mShaderLevel > 0)
	{
		sSimpleShader->bind();
		sSimpleShader->setMinimumAlpha(0.5f);
		if (gUseNewShaders)
		{
			sSimpleShader->uniform1i(LLShaderMgr::NO_ATMO,
									 LLPipeline::sRenderingHUDs ? 1 : 0);
		}
	}
	else
	{
		// Do not use shaders !
		gGL.flush();
		LLGLSLShader::bindNoShader();
	}
}

void LLDrawPoolGrass::endRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_GRASS);
	LLRenderPass::endRenderPass(pass);

	if (mShaderLevel > 0)
	{
		sSimpleShader->unbind();
	}
}

void LLDrawPoolGrass::render(S32)
{
	LLGLDisable blend(GL_BLEND);

	LL_FAST_TIMER(FTM_RENDER_GRASS);
	LLGLEnable test(GL_ALPHA_TEST);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	// Render grass
	LLRenderPass::renderTexture(LLRenderPass::PASS_GRASS, getVertexDataMask());
	stop_glerror();
}

void LLDrawPoolGrass::renderDeferred(S32)
{
	LL_FAST_TIMER(FTM_RENDER_GRASS);
	gDeferredNonIndexedDiffuseAlphaMaskProgram.bind();
	gDeferredNonIndexedDiffuseAlphaMaskProgram.setMinimumAlpha(0.5f);

	if (gUseNewShaders)
	{
		gDeferredNonIndexedDiffuseAlphaMaskProgram.uniform1i(LLShaderMgr::NO_ATMO,
															 LLPipeline::sRenderingHUDs ?
																1 : 0);
	}

	// Render grass
	LLRenderPass::renderTexture(LLRenderPass::PASS_GRASS, getVertexDataMask());
	stop_glerror();
}

// Fullbright drawpool
LLDrawPoolFullbright::LLDrawPoolFullbright()
:	LLRenderPass(POOL_FULLBRIGHT)
{
}

void LLDrawPoolFullbright::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

void LLDrawPoolFullbright::beginPostDeferredPass(S32)
{
	if (LLPipeline::sUnderWaterRender)
	{
		gDeferredFullbrightWaterProgram.bind();
	}
	else
	{
		gDeferredFullbrightProgram.bind();
		if (gUseNewShaders)
		{
			gDeferredFullbrightProgram.uniform1i(LLShaderMgr::NO_ATMO,
												 LLPipeline::sRenderingHUDs ?
													1 : 0);
		}
	}
}

void LLDrawPoolFullbright::renderPostDeferred(S32)
{
	LL_FAST_TIMER(FTM_RENDER_FULLBRIGHT);

	U32 mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 |
			   LLVertexBuffer::MAP_COLOR | LLVertexBuffer::MAP_TEXTURE_INDEX;

	if (gUseNewShaders)
	{
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
		pushBatches(LLRenderPass::PASS_FULLBRIGHT, mask, true,
					true);
	}
	else
	{
		gGL.setColorMask(true, true);
		LLGLDisable blend(GL_BLEND);
		pushBatches(LLRenderPass::PASS_FULLBRIGHT, mask, true,
					true);
		gGL.setColorMask(true, false);
	}
	stop_glerror();
}

void LLDrawPoolFullbright::endPostDeferredPass(S32 pass)
{
	if (LLPipeline::sUnderWaterRender)
	{
		gDeferredFullbrightWaterProgram.unbind();
	}
	else
	{
		gDeferredFullbrightProgram.unbind();
	}
	LLRenderPass::endRenderPass(pass);
}

void LLDrawPoolFullbright::beginRenderPass(S32)
{
	LL_FAST_TIMER(FTM_RENDER_FULLBRIGHT);

	if (LLPipeline::sUnderWaterRender)
	{
		sFullbrightShader = &gObjectFullbrightWaterProgram;
	}
	else
	{
		sFullbrightShader = &gObjectFullbrightProgram;
	}
}

void LLDrawPoolFullbright::endRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_FULLBRIGHT);
	LLRenderPass::endRenderPass(pass);

	if (mShaderLevel > 0)
	{
		sFullbrightShader->unbind();
	}
}

void LLDrawPoolFullbright::render(S32)
{
	LL_FAST_TIMER(FTM_RENDER_FULLBRIGHT);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	if (mShaderLevel > 0)
	{
		sFullbrightShader->bind();
		sFullbrightShader->uniform1f(LLShaderMgr::FULLBRIGHT, 1.f);
		sFullbrightShader->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 1.f);
		if (gUseNewShaders)
		{
			sFullbrightShader->uniform1i(LLShaderMgr::NO_ATMO,
										 LLPipeline::sRenderingHUDs ? 1 : 0);
		}

		U32 mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 |
				   LLVertexBuffer::MAP_COLOR |
				   LLVertexBuffer::MAP_TEXTURE_INDEX;
		pushBatches(LLRenderPass::PASS_FULLBRIGHT, mask, true, true);
		pushBatches(LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE, mask, true,
					true);
		pushBatches(LLRenderPass::PASS_SPECMAP_EMISSIVE, mask, true, true);
		pushBatches(LLRenderPass::PASS_NORMMAP_EMISSIVE, mask, true, true);
		pushBatches(LLRenderPass::PASS_NORMSPEC_EMISSIVE, mask, true, true);
	}
	else
	{
		gPipeline.enableLightsFullbright();
		U32 mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 |
				   LLVertexBuffer::MAP_COLOR;
		renderTexture(LLRenderPass::PASS_FULLBRIGHT, mask);
		pushBatches(LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE, mask);
		pushBatches(LLRenderPass::PASS_SPECMAP_EMISSIVE, mask);
		pushBatches(LLRenderPass::PASS_NORMMAP_EMISSIVE, mask);
		pushBatches(LLRenderPass::PASS_NORMSPEC_EMISSIVE, mask);
	}

	stop_glerror();
}

void LLDrawPoolFullbrightAlphaMask::beginPostDeferredPass(S32)
{
	F32 gamma = 2.2f;
	bool render_hud = LLPipeline::sRenderingHUDs;
	S32 no_atmo = render_hud ? 1 : 0;
	if (render_hud || !LLPipeline::sRenderDeferred)
	{
		sPostDeferredShader = &gObjectFullbrightAlphaMaskProgram;
		gamma = 1.f;
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		sPostDeferredShader = &gDeferredFullbrightAlphaMaskWaterProgram;
		no_atmo = 1;
	}
	else
	{
		sPostDeferredShader = &gDeferredFullbrightAlphaMaskProgram;
	}
	sPostDeferredShader->bind();
	sPostDeferredShader->uniform1f(LLShaderMgr::TEXTURE_GAMMA, gamma);
	if (gUseNewShaders)
	{
		sPostDeferredShader->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
	}
}

void LLDrawPoolFullbrightAlphaMask::renderPostDeferred(S32)
{
	LL_TRACY_TIMER(TRC_RENDER_FULLBRIGHT);
	LLGLDisable blend(GL_BLEND);
	U32 mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 |
			   LLVertexBuffer::MAP_COLOR | LLVertexBuffer::MAP_TEXTURE_INDEX;
	pushMaskBatches(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK, mask, true,
					true);
	stop_glerror();
}

void LLDrawPoolFullbrightAlphaMask::endPostDeferredPass(S32 pass)
{
	if (sPostDeferredShader)
	{
		sPostDeferredShader->unbind();
		sPostDeferredShader = NULL;
	}
	LLRenderPass::endRenderPass(pass);
}

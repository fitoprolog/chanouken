/**
 * @file lldrawpoolalpha.cpp
 * @brief LLDrawPoolAlpha class implementation
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

#include "lldrawpoolalpha.h"

#include "llcubemap.h"
#include "llfasttimer.h"
#include "llgl.h"
#include "llrender.h"

#include "lldrawable.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h" 	// For debugging
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"	// For debugging

bool LLDrawPoolAlpha::sShowDebugAlpha = false;

static bool sDeferredRender = false;

LLDrawPoolAlpha::LLDrawPoolAlpha(U32 type)
:	LLRenderPass(type),
	mCurrentShader(NULL),
	mTargetShader(NULL),
	mSimpleShader(NULL),
	mFullbrightShader(NULL),
	mEmissiveShader(NULL),
	mColorSFactor(LLRender::BF_UNDEF),
	mColorDFactor(LLRender::BF_UNDEF),
	mAlphaSFactor(LLRender::BF_UNDEF),
	mAlphaDFactor(LLRender::BF_UNDEF)
{
}

void LLDrawPoolAlpha::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

S32 LLDrawPoolAlpha::getNumPostDeferredPasses()
{
	static LLCachedControl<bool> render_depth_of_field(gSavedSettings,
													   "RenderDepthOfField");
	// Skip depth buffer filling pass when rendering impostors or when
	// DoF is disabled.
	return LLPipeline::sImpostorRender || !render_depth_of_field ? 1 : 2;
}

void LLDrawPoolAlpha::beginPostDeferredPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_ALPHA);

	F32 gamma = LLPipeline::RenderDeferredDisplayGamma;

    if (LLPipeline::sRenderDeferred)
    {
		mEmissiveShader = &gDeferredEmissiveProgram;
    }
    else if (LLPipeline::sUnderWaterRender)
	{
		mEmissiveShader = &gObjectEmissiveWaterProgram;
	}
	else
	{
		mEmissiveShader = &gObjectEmissiveProgram;
	}

	S32 no_atmo = 0;
	if (gUseNewShaders)
	{
		if (LLPipeline::sRenderingHUDs)
		{
			no_atmo = 1;
		}
		mEmissiveShader->bind();
		mEmissiveShader->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
		mEmissiveShader->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 2.2f);
		mEmissiveShader->uniform1f(LLShaderMgr::DISPLAY_GAMMA, 1.f / gamma);
	}

	if (pass == 0)
	{
		if (LLPipeline::sImpostorRender)
		{
			mFullbrightShader = &gDeferredFullbrightProgram;
			mSimpleShader = &gDeferredAlphaImpostorProgram;
		}
		else if (LLPipeline::sUnderWaterRender)
		{
			mFullbrightShader = &gDeferredFullbrightWaterProgram;
			mSimpleShader = &gDeferredAlphaWaterProgram;
		}
		else
		{
			mFullbrightShader = &gDeferredFullbrightProgram;
			mSimpleShader = &gDeferredAlphaProgram;
		}

		mFullbrightShader->bind();
		mFullbrightShader->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 2.2f);
		mFullbrightShader->uniform1f(LLShaderMgr::DISPLAY_GAMMA, 1.f / gamma);
		if (gUseNewShaders)
		{
			mFullbrightShader->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
		}
		mFullbrightShader->unbind();

		// Prime simple shader (loads shadow relevant uniforms)
		gPipeline.bindDeferredShader(*mSimpleShader);

		mSimpleShader->uniform1f(LLShaderMgr::DISPLAY_GAMMA, 1.f / gamma);
		if (gUseNewShaders)
		{
			mSimpleShader->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
		}
	}
	else if (!LLPipeline::sImpostorRender)
	{
		// Update depth buffer sampler
		gPipeline.mScreen.flush();
		gPipeline.mDeferredDepth.copyContents(gPipeline.mDeferredScreen, 0, 0,
											  gPipeline.mDeferredScreen.getWidth(),
											  gPipeline.mDeferredScreen.getHeight(),
											  0, 0,
											  gPipeline.mDeferredDepth.getWidth(),
											  gPipeline.mDeferredDepth.getHeight(),
											  GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		gPipeline.mDeferredDepth.bindTarget();
		mSimpleShader = mFullbrightShader = &gObjectFullbrightAlphaMaskProgram;
		gObjectFullbrightAlphaMaskProgram.bind();
		gObjectFullbrightAlphaMaskProgram.setMinimumAlpha(0.33f);
	}

	sDeferredRender = true;

	// Start out with no shaders.
	if (mShaderLevel > 0)
	{
		mCurrentShader = mTargetShader = NULL;
	}

	gPipeline.enableLightsDynamic();
}

void LLDrawPoolAlpha::endPostDeferredPass(S32 pass)
{
	if (pass == 1 && !LLPipeline::sImpostorRender)
	{
		gPipeline.mDeferredDepth.flush();
		gPipeline.mScreen.bindTarget();
		gObjectFullbrightAlphaMaskProgram.unbind();
	}

	sDeferredRender = false;
	endRenderPass(pass);
}

void LLDrawPoolAlpha::renderPostDeferred(S32 pass)
{
	render(pass);
}

void LLDrawPoolAlpha::beginRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_ALPHA);

	if (LLPipeline::sImpostorRender)
	{
		mSimpleShader = &gObjectSimpleImpostorProgram;
		mFullbrightShader = &gObjectFullbrightProgram;
		mEmissiveShader = &gObjectEmissiveProgram;
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		mSimpleShader = &gObjectSimpleWaterProgram;
		mFullbrightShader = &gObjectFullbrightWaterProgram;
		mEmissiveShader = &gObjectEmissiveWaterProgram;
	}
	else
	{
		mSimpleShader = &gObjectSimpleProgram;
		mFullbrightShader = &gObjectFullbrightProgram;
		mEmissiveShader = &gObjectEmissiveProgram;
	}

	if (!gUseNewShaders)
	{
		if (mShaderLevel > 0)
		{
			// Start out with no shaders.
			mCurrentShader = mTargetShader = NULL;
			LLGLSLShader::bindNoShader();
		}

		gPipeline.enableLightsDynamic();
		return;
	}

	S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;

	if (mShaderLevel > 0)
	{
		if (LLPipeline::sImpostorRender)
		{
			mFullbrightShader->bind();
			mFullbrightShader->setMinimumAlpha(0.5f);
			mFullbrightShader->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
			mSimpleShader->bind();
			mSimpleShader->setMinimumAlpha(0.5f);
			mSimpleShader->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
		}
		else
		{
			mFullbrightShader->bind();
			mFullbrightShader->setMinimumAlpha(0.f);
			mFullbrightShader->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
			mSimpleShader->bind();
			mSimpleShader->setMinimumAlpha(0.f);
			mSimpleShader->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
		}
	}

	gPipeline.enableLightsDynamic();

	LLGLSLShader::bindNoShader();
	mCurrentShader = mTargetShader = NULL;
}

void LLDrawPoolAlpha::endRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_ALPHA);
	LLRenderPass::endRenderPass(pass);

	if (mShaderLevel > 0)
	{
		LLGLSLShader::bindNoShader();
	}
}

void LLDrawPoolAlpha::render(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_ALPHA);

	LLGLSPipelineAlpha gls_pipeline_alpha;

	if (sDeferredRender && pass == 1)
	{
		// Depth only
		gGL.setColorMask(false, false);
	}
	else
	{
		gGL.setColorMask(true, true);
	}

	bool write_depth = (sDeferredRender && pass == 1) ||
					   // We want depth written so that rendered alpha will
					   // contribute to the alpha mask used for impostors
					   LLPipeline::sImpostorRenderAlphaDepthPass;
	LLGLDepthTest depth(GL_TRUE, write_depth ? GL_TRUE : GL_FALSE);

	if (sDeferredRender && pass == 1)
	{
		gGL.blendFunc(LLRender::BF_SOURCE_ALPHA,
					  LLRender::BF_ONE_MINUS_SOURCE_ALPHA);
	}
	else
	{
		// Regular alpha blend
		mColorSFactor = LLRender::BF_SOURCE_ALPHA;
		mColorDFactor = LLRender::BF_ONE_MINUS_SOURCE_ALPHA;

		// Glow suppression
		mAlphaSFactor = LLRender::BF_ZERO;
		mAlphaDFactor = LLRender::BF_ONE_MINUS_SOURCE_ALPHA;

		gGL.blendFunc(mColorSFactor, mColorDFactor, mAlphaSFactor,
					  mAlphaDFactor);

		if (!gUseNewShaders && mShaderLevel > 0)
		{
			if (LLPipeline::sImpostorRender)
			{
				mFullbrightShader->bind();
				mFullbrightShader->setMinimumAlpha(0.5f);
				mSimpleShader->bind();
				mSimpleShader->setMinimumAlpha(0.5f);
			}
			else
			{
				mFullbrightShader->bind();
				mFullbrightShader->setMinimumAlpha(0.f);
				mSimpleShader->bind();
				mSimpleShader->setMinimumAlpha(0.f);
			}
		}
	}

	if (mShaderLevel > 0)
	{
		renderAlpha(getVertexDataMask() |
					LLVertexBuffer::MAP_TEXTURE_INDEX |
					LLVertexBuffer::MAP_TANGENT |
					LLVertexBuffer::MAP_TEXCOORD1 |
					LLVertexBuffer::MAP_TEXCOORD2,
					pass);
	}
	else
	{
		// getVertexDataMask base mask if fixed-function
		renderAlpha(getVertexDataMask(), pass);
	}

	gGL.setColorMask(true, false);

	if (sDeferredRender && pass == 1)
	{
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
	}

	if (sShowDebugAlpha)
	{
		bool shaders = gPipeline.canUseVertexShaders();
		if (shaders)
		{
			gHighlightProgram.bind();
		}
		else
		{
			gPipeline.enableLightsFullbright();
		}

		// Highlight (semi) transparent faces
		gGL.diffuseColor4f(1.f, 0.f, 0.f, 1.f);

		gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sSmokeImagep);
		constexpr U32 mask = LLVertexBuffer::MAP_VERTEX |
							 LLVertexBuffer::MAP_TEXCOORD0;
		renderAlphaHighlight(mask);
		pushBatches(LLRenderPass::PASS_ALPHA_MASK, mask, false);
		pushBatches(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK, mask, false);
		pushBatches(LLRenderPass::PASS_ALPHA_INVISIBLE, mask, false);

		// Highlight invisible faces in green
		gGL.diffuseColor4f(0.f, 1.f, 0.f, 1.f);
		pushBatches(LLRenderPass::PASS_INVISIBLE, mask, false);

		if (LLPipeline::sRenderDeferred)
		{
			// Highlight alpha masking textures in blue when in deferred
			// rendering mode.
			gGL.diffuseColor4f(0.f, 0.f, 1.f, 1.f);
			pushBatches(LLRenderPass::PASS_MATERIAL_ALPHA_MASK, mask, false);
		}

		if (shaders)
		{
			gHighlightProgram.unbind();
		}
	}
}

void LLDrawPoolAlpha::renderAlphaHighlight(U32 mask)
{
	for (LLCullResult::sg_iterator i = gPipeline.beginAlphaGroups(),
								   end = gPipeline.endAlphaGroups();
		 i != end; ++i)
	{
		LLSpatialGroup* group = *i;
		if (!group || group->isDead()) continue;

		LLSpatialPartition* partp = group->getSpatialPartition();
		if (!partp || !partp->mRenderByGroup) continue;

		LLSpatialGroup::drawmap_elem_t& draw_info =
			group->mDrawMap[LLRenderPass::PASS_ALPHA];

		for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(),
													  end2 = draw_info.end();
			 k != end2; ++k)
		{
			LLDrawInfo& params = **k;

			if (params.mParticle)
			{
				continue;
			}

			LLRenderPass::applyModelMatrix(params);
			if (params.mGroup)
			{
				params.mGroup->rebuildMesh();
			}
			params.mVertexBuffer->setBuffer(mask);
			params.mVertexBuffer->drawRange(params.mDrawMode,
											params.mStart, params.mEnd,
											params.mCount, params.mOffset);
			gPipeline.addTrianglesDrawn(params.mCount, params.mDrawMode);
		}
	}
}

void LLDrawPoolAlpha::renderAlpha(U32 mask, S32 pass)
{
	if (gUseNewShaders)
	{
		renderAlphaEE(mask, pass);
	}
	else
	{
		renderAlphaWL(mask, pass);
	}
}

// Extended environment rendering methods

static LL_INLINE void draw_it(LLDrawInfo* draw, U32 mask)
{
	draw->mVertexBuffer->setBuffer(mask);
	LLRenderPass::applyModelMatrix(*draw);
	draw->mVertexBuffer->drawRange(draw->mDrawMode, draw->mStart, draw->mEnd,
								   draw->mCount, draw->mOffset);
	gPipeline.addTrianglesDrawn(draw->mCount, draw->mDrawMode);
}

bool LLDrawPoolAlpha::texSetup(LLDrawInfo* draw, bool use_shaders,
							   bool use_material, LLGLSLShader* shader,
							   LLTexUnit* unit0)
{
	LL_TRACY_TIMER(TRC_RENDER_ALPHA_TEX_BINDS);
	bool tex_setup = false;

	if (use_material && shader && sDeferredRender)
	{
		LL_TRACY_TIMER(TRC_RENDER_ALPHA_DEFERRED_TEX_BINDS);
		if (draw->mNormalMap)
		{
			draw->mNormalMap->addTextureStats(draw->mVSize);
			shader->bindTexture(LLShaderMgr::BUMP_MAP, draw->mNormalMap);
		}

		if (draw->mSpecularMap)
		{
			draw->mSpecularMap->addTextureStats(draw->mVSize);
			shader->bindTexture(LLShaderMgr::SPECULAR_MAP, draw->mSpecularMap);
		}
	}
	else if (shader == mSimpleShader)
	{
		shader->bindTexture(LLShaderMgr::BUMP_MAP,
							LLViewerFetchedTexture::sFlatNormalImagep);
		shader->bindTexture(LLShaderMgr::SPECULAR_MAP,
							LLViewerFetchedTexture::sWhiteImagep);
	}
	U32 count = draw->mTextureList.size();
	if (count > LL_NUM_TEXTURE_LAYERS)
	{
		llwarns << "We have only " << LL_NUM_TEXTURE_LAYERS
				<< " TexUnits and this batch contains " << count
				<< " textures. Rendering will be incomplete !" << llendl;
		count = LL_NUM_TEXTURE_LAYERS;
	}
	if (use_shaders && count > 1)
	{
		for (U32 i = 0; i < count; ++i)
		{
			LLViewerTexture* tex = draw->mTextureList[i].get();
			if (tex)
			{
				gGL.getTexUnit(i)->bind(tex);
			}
		}
	}
	// Not batching textures or batch has only 1 texture; we might need a
	// texture matrix.
	else if (draw->mTexture.notNull())
	{
		draw->mTexture->addTextureStats(draw->mVSize);
		if (use_shaders && use_material)
		{
			shader->bindTexture(LLShaderMgr::DIFFUSE_MAP, draw->mTexture);
		}
		else
		{
			unit0->bind(draw->mTexture);
		}
		if (draw->mTextureMatrix)
		{
			tex_setup = true;
			unit0->activate();
			gGL.matrixMode(LLRender::MM_TEXTURE);
			gGL.loadMatrix(draw->mTextureMatrix->getF32ptr());
			++gPipeline.mTextureMatrixOps;
		}
	}
	else
	{
		unit0->unbind(LLTexUnit::TT_TEXTURE);
	}

	return tex_setup;
}

void LLDrawPoolAlpha::renderFullbrights(U32 mask,
										const std::vector<LLDrawInfo*>& fullb)
{
	gPipeline.enableLightsFullbright();

	if (!mFullbrightShader) return;	// Paranoia

	mFullbrightShader->bind();
	mFullbrightShader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, 1.f);
	bool use_shaders = gPipeline.canUseVertexShaders();
	mask &= ~(LLVertexBuffer::MAP_TANGENT | LLVertexBuffer::MAP_TEXCOORD1 |
			  LLVertexBuffer::MAP_TEXCOORD2);
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	for (U32 i = 0, count = fullb.size(); i < count; ++i)
	{
		LLDrawInfo* draw = fullb[i];
		bool tex_setup = texSetup(draw, use_shaders, false, mFullbrightShader,
								  unit0);
		gGL.blendFunc((LLRender::eBlendFactor)draw->mBlendFuncSrc,
					  (LLRender::eBlendFactor)draw->mBlendFuncDst,
					  mAlphaSFactor, mAlphaDFactor);
		draw_it(draw, mask);
		// Restore tex setup
		if (tex_setup)
		{
			unit0->activate();
			// Note: activate() did change matrix mode to MM_TEXTURE, so the
			// loadIdentity() call does apply to MM_TEXTURE.
			gGL.loadIdentity();
			gGL.matrixMode(LLRender::MM_MODELVIEW);
		}
	}
	mFullbrightShader->unbind();
}

LL_INLINE static void draw_emissive(U32 mask, LLDrawInfo* draw)
{
	draw->mVertexBuffer->setBuffer((mask & ~LLVertexBuffer::MAP_COLOR) |
								   LLVertexBuffer::MAP_EMISSIVE);
	draw->mVertexBuffer->drawRange(draw->mDrawMode, draw->mStart, draw->mEnd,
								   draw->mCount, draw->mOffset);
	gPipeline.addTrianglesDrawn(draw->mCount, draw->mDrawMode);
}

void LLDrawPoolAlpha::renderEmissives(U32 mask,
									  const std::vector<LLDrawInfo*>& ems)
{
	if (!mEmissiveShader) return;	// Paranoia

	mEmissiveShader->bind();
	mEmissiveShader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, 1.f);

	gPipeline.enableLightsDynamic();

	// Install glow-accumulating blend mode
	gGL.blendFunc(LLRender::BF_ZERO, LLRender::BF_ONE, // Do not touch color
				  LLRender::BF_ONE, LLRender::BF_ONE); // Add to alpha (glow)

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	bool use_shaders = gPipeline.canUseVertexShaders();
	for (U32 i = 0, count = ems.size(); i < count; ++i)
	{
		LLDrawInfo* draw = ems[i];
		bool tex_setup = texSetup(draw, use_shaders, false, mEmissiveShader,
								  unit0);
		draw_emissive(mask, draw);
		// Restore tex setup
		if (tex_setup)
		{
			unit0->activate();
			// Note: activate() did change matrix mode to MM_TEXTURE, so the
			// loadIdentity() call does apply to MM_TEXTURE.
			gGL.loadIdentity();
			gGL.matrixMode(LLRender::MM_MODELVIEW);
		}
	}

	// Restore our alpha blend mode
	gGL.blendFunc(mColorSFactor, mColorDFactor, mAlphaSFactor, mAlphaDFactor);

	mEmissiveShader->unbind();
}

static bool check_vb_mask(U32 mask, U32 expected_mask)
{
	U32 missing = expected_mask & ~mask;
	if (!missing)
	{
		return true;
	}

	if (gDebugGL)
	{
		llwarns << "Missing required components:"
				<< LLVertexBuffer::listMissingBits(missing) << llendl;
	}

	static LLCachedControl<bool> ignore(gSavedSettings,
										"RenderIgnoreBadVBMask");
	return ignore;
}

void LLDrawPoolAlpha::renderAlphaEE(U32 mask, S32 pass)
{
	static LLCachedControl<bool> batch_fullbrights(gSavedSettings,
												   "RenderAlphaBatchFullbrights");
	static LLCachedControl<bool> batch_emissives(gSavedSettings,
												 "RenderAlphaBatchEmissives");
	bool initialized_lighting = false;
	bool light_enabled = true;
	bool use_shaders = gPipeline.canUseVertexShaders();
	bool depth_only = pass == 1 && !LLPipeline::sImpostorRender;
	// No shaders = no glow.
#if 1
	bool draw_glow = !depth_only && mShaderLevel > 0;
#else
	bool draw_glow = mShaderLevel > 0;
#endif

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	static std::vector<LLDrawInfo*> emissives, fullbrights;
	for (LLCullResult::sg_iterator i = gPipeline.beginAlphaGroups(),
								   end = gPipeline.endAlphaGroups();
		 i != end; ++i)
	{
		LLSpatialGroup* group = *i;
		if (!group || group->isDead())
		{
			continue;
		}

		LLSpatialPartition* partp = group->getSpatialPartition();
		if (!partp || !partp->mRenderByGroup)
		{
			llassert(partp);
			continue;
		}

		U32 part_type = partp->mPartitionType;
		bool is_particle =
			part_type == LLViewerRegion::PARTITION_PARTICLE ||
			part_type == LLViewerRegion::PARTITION_HUD_PARTICLE ||
			part_type == LLViewerRegion::PARTITION_CLOUD;

		LLGLDisable cull(is_particle ? GL_CULL_FACE : 0);

		LLSpatialGroup::drawmap_elem_t& draw_info =
			group->mDrawMap[LLRenderPass::PASS_ALPHA];
		for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(),
													  end2 = draw_info.end();
			 k != end2; ++k)
		{
			LLDrawInfo& params = **k;
			if (!check_vb_mask(params.mVertexBuffer->getTypeMask(), mask))
			{
				continue;
			}
			// Fix for bug - NORSPEC-271
			// If the face is more than 90% transparent, then do not update the
			// depth buffer for DoF. We do not want the nearly invisible
			// objects to cause DoF effects...
			if (depth_only)
			{
				LLFace*	face = params.mFace;
				if (face)
				{
					const LLTextureEntry* tep = face->getTextureEntry();
					if (tep && tep->getColor().mV[3] < 0.1f)
					{
						continue;
					}
				}
			}
			if (batch_fullbrights && params.mFullbright)
			{
				fullbrights.push_back(&params);
				continue;
			}

			LLRenderPass::applyModelMatrix(params);

			if (params.mGroup)
			{
				params.mGroup->rebuildMesh();
			}

			LLMaterial* mat = sDeferredRender ? params.mMaterial.get() : NULL;

			if (params.mFullbright)
			{
				if (light_enabled || !initialized_lighting)
				{
					initialized_lighting = true;
					if (use_shaders)
					{
						mTargetShader = mFullbrightShader;
					}
					else
					{
						gPipeline.enableLightsFullbright();
					}
					light_enabled = false;
				}
			}
			else if (!light_enabled || !initialized_lighting)
			{
				initialized_lighting = true;
				if (use_shaders)
				{
					mTargetShader = mSimpleShader;
				}
				else
				{
					gPipeline.enableLightsDynamic();
				}
				light_enabled = true;
			}
			if (mat)
			{
				U32 mask = params.mShaderMask;
				llassert(mask < LLMaterial::SHADER_COUNT);
				if (LLPipeline::sUnderWaterRender)
				{
					mTargetShader = &gDeferredMaterialWaterProgram[mask];
				}
				else
				{
					mTargetShader = &gDeferredMaterialProgram[mask];
				}

				// If we need shaders and we are not already using the
				// proper shader, then bind it.
				if (mCurrentShader != mTargetShader)
				{
					gPipeline.bindDeferredShader(*mTargetShader);
					mCurrentShader = mTargetShader;
				}
			}
			else
			{
				mTargetShader = params.mFullbright ? mFullbrightShader
												   : mSimpleShader;
			}

			if (use_shaders && mCurrentShader != mTargetShader)
			{
				mCurrentShader = mTargetShader;
				mCurrentShader->bind();
			}
			else if (!use_shaders && mCurrentShader)
			{
				LLGLSLShader::bindNoShader();
				mCurrentShader = NULL;
			}

			LLVector4 spec_color(1.f, 1.f, 1.f, 1.f);
			F32 env_intensity = 0.f;
			F32 brightness = 1.f;
			// If we have a material, supply the appropriate data here.
			if (mat && use_shaders)
			{
				spec_color = params.mSpecColor;
				env_intensity = params.mEnvIntensity;
				brightness = params.mFullbright ? 1.f : 0.f;
			}
			if (mCurrentShader)
			{
				mCurrentShader->uniform4f(LLShaderMgr::SPECULAR_COLOR,
										  spec_color.mV[0], spec_color.mV[1],
										  spec_color.mV[2], spec_color.mV[3]);
				mCurrentShader->uniform1f(LLShaderMgr::ENVIRONMENT_INTENSITY,
										  env_intensity);
				mCurrentShader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS,
										  brightness);
			}

			bool tex_setup = texSetup(&params, use_shaders, use_shaders && mat,
									  mCurrentShader, unit0);
			{
				LL_TRACY_TIMER(TRC_RENDER_ALPHA_DRAW);
				gGL.blendFunc((LLRender::eBlendFactor)params.mBlendFuncSrc,
							  (LLRender::eBlendFactor) params.mBlendFuncDst,
							   mAlphaSFactor, mAlphaDFactor);
				U32 data_mask = mask & ~(params.mFullbright ?
										 (LLVertexBuffer::MAP_TANGENT |
										  LLVertexBuffer::MAP_TEXCOORD1 |
										  LLVertexBuffer::MAP_TEXCOORD2) :
										 0);
				params.mVertexBuffer->setBuffer(data_mask);
				params.mVertexBuffer->drawRange(params.mDrawMode,
												params.mStart, params.mEnd,
												params.mCount, params.mOffset);
				gPipeline.addTrianglesDrawn(params.mCount, params.mDrawMode);
			}

			// If this alpha mesh has glow, then draw it a second time to add
			// the destination-alpha (=glow). Interleaving these state-changing
			// calls is expensive, but glow must be drawn Z-sorted with alpha.
			if (mCurrentShader && draw_glow &&
				(!is_particle || params.mHasGlow) &&
				params.mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_EMISSIVE))
			{
				LL_TRACY_TIMER(TRC_RENDER_ALPHA_EMISSIVE);
				if (batch_emissives)
				{
					emissives.push_back(&params);
				}
				else
				{
					// Install glow-accumulating blend mode: do not touch color
					// and add to alpha (glow).
					gGL.blendFunc(LLRender::BF_ZERO, LLRender::BF_ONE,
								  LLRender::BF_ONE, LLRender::BF_ONE);

					mEmissiveShader->bind();
					draw_emissive(mask, &params);

					// Restore our alpha blend mode
					gGL.blendFunc(mColorSFactor, mColorDFactor, mAlphaSFactor,
								  mAlphaDFactor);

					mCurrentShader->bind();
				}
			}

			// Restore tex setup
			if (tex_setup)
			{
				unit0->activate();
				// Note: activate() did change matrix mode to MM_TEXTURE, so
				// the loadIdentity() call does apply to MM_TEXTURE.
				gGL.loadIdentity();
				gGL.matrixMode(LLRender::MM_MODELVIEW);
			}
		}

		bool rebind = false;
		if (!fullbrights.empty())
		{
			light_enabled = false;
			renderFullbrights(mask, fullbrights);
			fullbrights.clear();
			rebind = true;
		}

		if (!emissives.empty())
		{
			light_enabled = true;
			renderEmissives(mask, emissives);
			emissives.clear();
			rebind = true;
		}

		if (rebind && mCurrentShader)
		{
			mCurrentShader->bind();
		}
	}

	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	LLVertexBuffer::unbind();

	if (!light_enabled)
	{
		gPipeline.enableLightsDynamic();
	}
}

// Legacy Windlight rendering method

void LLDrawPoolAlpha::renderAlphaWL(U32 mask, S32 pass)
{
	bool initialized_lighting = false;
	bool light_enabled = true;
	bool use_shaders = gPipeline.canUseVertexShaders();
	bool depth_only = pass == 1 && !LLPipeline::sImpostorRender;
	// No shaders = no glow.
#if 1
	bool draw_glow = !depth_only && mShaderLevel > 0;
#else
	bool draw_glow = mShaderLevel > 0;
#endif

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	for (LLCullResult::sg_iterator i = gPipeline.beginAlphaGroups(),
								   end = gPipeline.endAlphaGroups();
		 i != end; ++i)
	{
		LLSpatialGroup* group = *i;
		if (!group || group->isDead())
		{
			continue;
		}

		LLSpatialPartition* partp = group->getSpatialPartition();
		if (!partp || !partp->mRenderByGroup)
		{
			llassert(partp);
			continue;
		}

		U32 part_type = partp->mPartitionType;
		bool is_particle =
			part_type == LLViewerRegion::PARTITION_PARTICLE ||
			part_type == LLViewerRegion::PARTITION_HUD_PARTICLE ||
			part_type == LLViewerRegion::PARTITION_CLOUD;

		LLGLDisable cull(is_particle ? GL_CULL_FACE : 0);

		LLSpatialGroup::drawmap_elem_t& draw_info =
			group->mDrawMap[LLRenderPass::PASS_ALPHA];
		for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(),
													  end2 = draw_info.end();
			 k != end2; ++k)
		{
			LLDrawInfo& params = **k;
			if (!check_vb_mask(params.mVertexBuffer->getTypeMask(), mask))
			{
				continue;
			}
			// Fix for bug - NORSPEC-271
			// If the face is more than 90% transparent, then do not update the
			// depth buffer for DoF. We do not want the nearly invisible
			// objects to cause DoF effects...
			if (depth_only)
			{
				LLFace*	face = params.mFace;
				if (face)
				{
					const LLTextureEntry* tep = face->getTextureEntry();
					if (tep && tep->getColor().mV[3] < 0.1f)
					{
						continue;
					}
				}
			}

			LLRenderPass::applyModelMatrix(params);

			if (params.mGroup)
			{
				params.mGroup->rebuildMesh();
			}

			LLMaterial* mat = sDeferredRender ? params.mMaterial.get() : NULL;

			if (!use_shaders)
			{
				llassert(!mTargetShader && !mCurrentShader &&
						 !LLGLSLShader::sCurBoundShaderPtr);
				bool fullbright = depth_only || params.mFullbright;
				if (fullbright == light_enabled || !initialized_lighting)
				{
					light_enabled = !fullbright;
					if (light_enabled)
					{
						// Turn off lighting if it has not already
						gPipeline.enableLightsDynamic();
					}
					else
					{
						// Turn on lighting if it has not already
						gPipeline.enableLightsFullbright();
					}
					initialized_lighting = true;
				}
			}
			else if (mat)
			{
				U32 mask = params.mShaderMask;
				llassert(mask < LLMaterial::SHADER_COUNT);
				if (LLPipeline::sUnderWaterRender)
				{
					mTargetShader = &gDeferredMaterialWaterProgram[mask];
				}
				else
				{
					mTargetShader = &gDeferredMaterialProgram[mask];
				}

				// If we need shaders and we are not already using the
				// proper shader, then bind it.
				if (mCurrentShader != mTargetShader)
				{
					gPipeline.bindDeferredShader(*mTargetShader);
				}
			}
			else
			{
				mTargetShader = params.mFullbright ? mFullbrightShader
												   : mSimpleShader;
				// If we need shaders and we are not already using the proper
				// shader, then bind it.
				if (mCurrentShader != mTargetShader)
				{
					mTargetShader->bind();
				}
			}
			mCurrentShader = mTargetShader;

			if (mat && mCurrentShader)
			{
				const LLVector4& spec_color = params.mSpecColor;
				mCurrentShader->uniform4f(LLShaderMgr::SPECULAR_COLOR,
										  spec_color.mV[0], spec_color.mV[1],
										  spec_color.mV[2], spec_color.mV[3]);
				mCurrentShader->uniform1f(LLShaderMgr::ENVIRONMENT_INTENSITY,
										  params.mEnvIntensity);
				mCurrentShader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS,
										  params.mFullbright ? 1.f : 0.f);

				if (params.mNormalMap)
				{
					params.mNormalMap->addTextureStats(params.mVSize);
					mCurrentShader->bindTexture(LLShaderMgr::BUMP_MAP,
												params.mNormalMap);
				}

				if (params.mSpecularMap)
				{
					params.mSpecularMap->addTextureStats(params.mVSize);
					mCurrentShader->bindTexture(LLShaderMgr::SPECULAR_MAP,
												params.mSpecularMap);
				}
			}
#if 1
			else if (sDeferredRender && mCurrentShader &&
					 mCurrentShader == mSimpleShader)
			{
				mCurrentShader->uniform4f(LLShaderMgr::SPECULAR_COLOR,
										  1.f, 1.f, 1.f, 1.f);
				mCurrentShader->uniform1f(LLShaderMgr::ENVIRONMENT_INTENSITY,
										  0.f);
				mCurrentShader->bindTexture(LLShaderMgr::BUMP_MAP,
											LLViewerFetchedTexture::sFlatNormalImagep);
				mCurrentShader->bindTexture(LLShaderMgr::SPECULAR_MAP,
											LLViewerFetchedTexture::sWhiteImagep);
			}
#endif
			U32 textures_count = params.mTextureList.size();
			if (textures_count > 1)
			{
				for (U32 i = 0; i < textures_count; ++i)
				{
					const LLPointer<LLViewerTexture>& tex =
						params.mTextureList[i];
					if (tex.notNull())
					{
						gGL.getTexUnit(i)->bind(tex);
					}
				}
			}

			bool tex_setup = false;
			if (!use_shaders || textures_count <= 1)
			{
				// Not batching textures or batch has only 1 texture.
				// Might need a texture matrix.
				if (params.mTexture.notNull())
				{
					params.mTexture->addTextureStats(params.mVSize);
					if (mat && use_shaders && mCurrentShader)
					{
						mCurrentShader->bindTexture(LLShaderMgr::DIFFUSE_MAP,
													params.mTexture);
					}
					else
					{
						unit0->bind(params.mTexture);
					}

					if (params.mTextureMatrix)
					{
						tex_setup = true;
						unit0->activate();
						gGL.matrixMode(LLRender::MM_TEXTURE);
						gGL.loadMatrix(params.mTextureMatrix->getF32ptr());
						++gPipeline.mTextureMatrixOps;
					}
				}
				else
				{
					unit0->unbind(LLTexUnit::TT_TEXTURE);
				}
			}

			gGL.blendFunc((LLRender::eBlendFactor)params.mBlendFuncSrc,
						  (LLRender::eBlendFactor)params.mBlendFuncDst,
						  mAlphaSFactor, mAlphaDFactor);

			// If using shaders, pull the attribute mask from it, else use the
			// passed base mask.
			U32 final_mask = mCurrentShader ? mCurrentShader->mAttributeMask
											: mask;
			params.mVertexBuffer->setBuffer(final_mask);

			params.mVertexBuffer->drawRange(params.mDrawMode,
											params.mStart, params.mEnd,
											params.mCount, params.mOffset);
			gPipeline.addTrianglesDrawn(params.mCount, params.mDrawMode);

			// If this alpha mesh has glow, then draw it a second time to add
			// the destination-alpha (=glow).
			// Interleaving these state-changing calls could be expensive, but
			// glow must be drawn Z-sorted with alpha.
			if (mCurrentShader && draw_glow &&
				(!is_particle || params.mHasGlow) &&
				params.mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_EMISSIVE))
			{
				LL_TRACY_TIMER(TRC_RENDER_ALPHA_GLOW);

				// Install glow-accumulating blend mode.
				// Do not touch color and add to alpha (glow).
				gGL.blendFunc(LLRender::BF_ZERO, LLRender::BF_ONE,
							  LLRender::BF_ONE, LLRender::BF_ONE);

				mEmissiveShader->bind();

				// Pull the attribute mask from shader
				final_mask = mEmissiveShader->mAttributeMask;
				params.mVertexBuffer->setBuffer(final_mask);
				// Do the actual drawing, again
				params.mVertexBuffer->drawRange(params.mDrawMode,
												params.mStart, params.mEnd,
												params.mCount, params.mOffset);
				gPipeline.addTrianglesDrawn(params.mCount, params.mDrawMode);

				mCurrentShader->bind();

				// Restore our alpha blend mode
				gGL.blendFunc(mColorSFactor, mColorDFactor, mAlphaSFactor,
							  mAlphaDFactor);
			}

			// Restore tex setup
			if (tex_setup)
			{
				unit0->activate();
				// Note: activate() did change matrix mode to MM_TEXTURE, so
				// the loadIdentity() call does apply to MM_TEXTURE.
				gGL.loadIdentity();
				gGL.matrixMode(LLRender::MM_MODELVIEW);
			}
		}
	}

	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	LLVertexBuffer::unbind();

	if (!light_enabled)
	{
		gPipeline.enableLightsDynamic();
	}
}

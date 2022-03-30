/**
 * @file lldrawpoolmaterials.cpp
 * @brief LLDrawPoolMaterials class implementation
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

#include "llviewerprecompiledheaders.h"

#include "lldrawpoolmaterials.h"

#include "llfasttimer.h"

#include "llpipeline.h"
#include "llviewershadermgr.h"

S32 diffuse_channel = -1;

LLDrawPoolMaterials::LLDrawPoolMaterials()
:	LLRenderPass(LLDrawPool::POOL_MATERIALS)
{
}

void LLDrawPoolMaterials::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

void LLDrawPoolMaterials::beginDeferredPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_MATERIALS);

	U32 shader_idx[] = {
		0, //LLRenderPass::PASS_MATERIAL,
		//1, //LLRenderPass::PASS_MATERIAL_ALPHA,
		2, //LLRenderPass::PASS_MATERIAL_ALPHA_MASK,
		3, //LLRenderPass::PASS_MATERIAL_ALPHA_GLOW,
		4, //LLRenderPass::PASS_SPECMAP,
		//5, //LLRenderPass::PASS_SPECMAP_BLEND,
		6, //LLRenderPass::PASS_SPECMAP_MASK,
		7, //LLRenderPass::PASS_SPECMAP_GLOW,
		8, //LLRenderPass::PASS_NORMMAP,
		//9, //LLRenderPass::PASS_NORMMAP_BLEND,
		10, //LLRenderPass::PASS_NORMMAP_MASK,
		11, //LLRenderPass::PASS_NORMMAP_GLOW,
		12, //LLRenderPass::PASS_NORMSPEC,
		//13, //LLRenderPass::PASS_NORMSPEC_BLEND,
		14, //LLRenderPass::PASS_NORMSPEC_MASK,
		15, //LLRenderPass::PASS_NORMSPEC_GLOW,
	};

	if (LLPipeline::sUnderWaterRender)
	{
		mShader = &gDeferredMaterialWaterProgram[shader_idx[pass]];
	}
	else
	{
		mShader = &gDeferredMaterialProgram[shader_idx[pass]];
	}
	mShader->bind();

	if (gUseNewShaders)
	{
		mShader->uniform1i(LLShaderMgr::NO_ATMO,
						   LLPipeline::sRenderingHUDs ? 1 : 0);
	}

	diffuse_channel = mShader->enableTexture(LLShaderMgr::DIFFUSE_MAP);
}

void LLDrawPoolMaterials::endDeferredPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_MATERIALS);

	mShader->unbind();

	LLRenderPass::endRenderPass(pass);
}

void LLDrawPoolMaterials::renderDeferred(S32 pass)
{
	U32 type_list[] = {
		LLRenderPass::PASS_MATERIAL,
		//LLRenderPass::PASS_MATERIAL_ALPHA,
		LLRenderPass::PASS_MATERIAL_ALPHA_MASK,
		LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
		LLRenderPass::PASS_SPECMAP,
		//LLRenderPass::PASS_SPECMAP_BLEND,
		LLRenderPass::PASS_SPECMAP_MASK,
		LLRenderPass::PASS_SPECMAP_EMISSIVE,
		LLRenderPass::PASS_NORMMAP,
		//LLRenderPass::PASS_NORMMAP_BLEND,
		LLRenderPass::PASS_NORMMAP_MASK,
		LLRenderPass::PASS_NORMMAP_EMISSIVE,
		LLRenderPass::PASS_NORMSPEC,
		//LLRenderPass::PASS_NORMSPEC_BLEND,
		LLRenderPass::PASS_NORMSPEC_MASK,
		LLRenderPass::PASS_NORMSPEC_EMISSIVE,
	};

	llassert(pass < (S32)(sizeof(type_list) / sizeof(U32)));

	U32 type = type_list[pass];

	U32 mask = mShader->mAttributeMask;

	LLCullResult::drawinfo_iterator begin = gPipeline.beginRenderMap(type);
	LLCullResult::drawinfo_iterator end = gPipeline.endRenderMap(type);
	for (LLCullResult::drawinfo_iterator i = begin; i != end; ++i)
	{
		LLDrawInfo& params = **i;

		mShader->uniform4f(LLShaderMgr::SPECULAR_COLOR,
						   params.mSpecColor.mV[0], params.mSpecColor.mV[1],
						   params.mSpecColor.mV[2], params.mSpecColor.mV[3]);
		mShader->uniform1f(LLShaderMgr::ENVIRONMENT_INTENSITY,
						   params.mEnvIntensity);

		if (params.mNormalMap)
		{
			params.mNormalMap->addTextureStats(params.mVSize);
			bindNormalMap(params.mNormalMap);
		}

		if (params.mSpecularMap)
		{
			params.mSpecularMap->addTextureStats(params.mVSize);
			bindSpecularMap(params.mSpecularMap);
		}

		mShader->setMinimumAlpha(params.mAlphaMaskCutoff);
		mShader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS,
						   params.mFullbright ? 1.f : 0.f);

		pushBatch(params, mask, true);
	}
}

void LLDrawPoolMaterials::bindSpecularMap(LLViewerTexture* tex)
{
	mShader->bindTexture(LLShaderMgr::SPECULAR_MAP, tex);
}

void LLDrawPoolMaterials::bindNormalMap(LLViewerTexture* tex)
{
	mShader->bindTexture(LLShaderMgr::BUMP_MAP, tex);
}

void LLDrawPoolMaterials::pushBatch(LLDrawInfo& params, U32 mask, bool texture,
									bool batch_textures)
{
	applyModelMatrix(params);

	bool tex_setup = false;

	S32 count = params.mTextureList.size();
	if (batch_textures && count > 1)
	{
		for (S32 i = 0; i < count; ++i)
		{
			const LLPointer<LLViewerTexture>& tex = params.mTextureList[i];
			if (tex.notNull())
			{
				gGL.getTexUnit(i)->bind(tex);
			}
		}
	}
	else
	{
		// Not batching textures or batch has only 1 texture: might need a
		// texture matrix
		if (params.mTextureMatrix)
		{
#if 0
			if (mShiny)
#endif
			{
				gGL.getTexUnit(0)->activate();
				gGL.matrixMode(LLRender::MM_TEXTURE);
			}

			gGL.loadMatrix(params.mTextureMatrix->getF32ptr());
			++gPipeline.mTextureMatrixOps;

			tex_setup = true;
		}

		if (mShaderLevel > 1 && texture)
		{
			if (params.mTexture.notNull())
			{
				gGL.getTexUnit(diffuse_channel)->bind(params.mTexture);
				params.mTexture->addTextureStats(params.mVSize);
			}
			else
			{
				gGL.getTexUnit(diffuse_channel)->unbind(LLTexUnit::TT_TEXTURE);
			}
		}
	}

	if (params.mGroup)
	{
		params.mGroup->rebuildMesh();
	}
	params.mVertexBuffer->setBuffer(mask);
	params.mVertexBuffer->drawRange(params.mDrawMode, params.mStart,
									params.mEnd, params.mCount,
									params.mOffset);
	gPipeline.addTrianglesDrawn(params.mCount, params.mDrawMode);
	if (tex_setup)
	{
		gGL.getTexUnit(0)->activate();
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
}

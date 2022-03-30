/**
 * @file lldrawpoolavatar.cpp
 * @brief LLDrawPoolAvatar class implementation
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

#include "lldrawpoolavatar.h"

#include "llfasttimer.h"
#include "llmatrix4a.h"
#include "llnoise.h"
#include "llrenderutils.h"			// For gSphere
#include "llvolume.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldrawable.h"
#include "lldrawpoolalpha.h"		// For sShowDebugAlpha
#include "lldrawpoolbump.h"
#include "llface.h"
#include "llgridmanager.h"			// For gIsInSecondLife
#include "llmeshrepository.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llskinningutil.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerpartsim.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llvoavatarself.h"
#include "llvovolume.h"

static U32 sBufferUsage = GL_STREAM_DRAW_ARB;
static U32 sShaderLevel = 0;

LLGLSLShader* LLDrawPoolAvatar::sVertexProgram = NULL;
bool LLDrawPoolAvatar::sSkipOpaque = false;
bool LLDrawPoolAvatar::sSkipTransparent = false;
S32 LLDrawPoolAvatar::sShadowPass = -1;
S32 LLDrawPoolAvatar::sDiffuseChannel = 0;
F32 LLDrawPoolAvatar::sMinimumAlpha = 0.2f;

static bool sIsDeferredRender = false;
static bool sIsPostDeferredRender = false;
static bool sIsMatsRender = false;

extern bool gUseGLPick;

constexpr F32 CLOTHING_GRAVITY_EFFECT = 0.7f;
constexpr F32 ONE255TH = 1.f / 255.f;

static bool sRenderingSkinned = false;
static S32 sNormalChannel = -1;
static S32 sSpecularChannel = -1;
static S32 sCubeChannel = -1;

LLDrawPoolAvatar::LLDrawPoolAvatar(U32 type)
:	LLFacePool(type),
//MK
	mAvatar(NULL)
//mk
{
}

//virtual
LLDrawPoolAvatar::~LLDrawPoolAvatar()
{
	if (!isDead())
	{
		llwarns << "Destroying a pool (" << std::hex << (intptr_t)this
				<< std::dec << ") still containing faces" << llendl;
	}
}

//virtual
bool LLDrawPoolAvatar::isDead()
{
	if (!LLFacePool::isDead())
	{
		return false;
	}

	for (U32 i = 0; i < NUM_RIGGED_PASSES; ++i)
	{
		if (!mRiggedFace[i].empty())
		{
			return false;
		}
	}

	return true;
}

S32 LLDrawPoolAvatar::getShaderLevel() const
{
	return (S32)gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_AVATAR);
}

void LLDrawPoolAvatar::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_AVATAR);

	sShaderLevel = mShaderLevel;

	if (sShaderLevel > 0)
	{
		sBufferUsage = GL_DYNAMIC_DRAW_ARB;
	}
	else
	{
		sBufferUsage = GL_STREAM_DRAW_ARB;
	}

	if (!mDrawFace.empty())
	{
		const LLFace* facep = mDrawFace[0];
		if (facep && facep->getDrawable())
		{
			LLVOAvatar* avatarp =
				(LLVOAvatar*)facep->getDrawable()->getVObj().get();
			if (avatarp)
			{
				updateRiggedVertexBuffers(avatarp);
			}
		}
	}
}

LLMatrix4& LLDrawPoolAvatar::getModelView()
{
	static LLMatrix4 ret;
	ret = LLMatrix4(gGLModelView.getF32ptr());
	return ret;
}

void LLDrawPoolAvatar::beginDeferredPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_CHARACTERS);

	sSkipTransparent = true;
	sIsDeferredRender = true;

	if (LLPipeline::sImpostorRender)
	{
		// Impostor pass does not have rigid or impostor rendering
		pass += 2;
	}

	switch (pass)
	{
		case 0:
			beginDeferredImpostor();
			break;

		case 1:
			beginDeferredRigid();
			break;

		case 2:
			beginDeferredSkinned();
			break;

		case 3:
			beginDeferredRiggedSimple();
			break;

		case 4:
			beginDeferredRiggedBump();
			break;

		default:
			beginDeferredRiggedMaterial(pass - 5);
	}
}

void LLDrawPoolAvatar::endDeferredPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_CHARACTERS);

	sSkipTransparent = false;
	sIsDeferredRender = false;

	if (LLPipeline::sImpostorRender)
	{
		pass += 2;
	}

	switch (pass)
	{
		case 0:
			endDeferredImpostor();
			break;

		case 1:
			endDeferredRigid();
			break;

		case 2:
			endDeferredSkinned();
			break;

		case 3:
			endDeferredRiggedSimple();
			break;

		case 4:
			endDeferredRiggedBump();
			break;

		default:
			endDeferredRiggedMaterial(pass - 5);
	}
}

void LLDrawPoolAvatar::renderDeferred(S32 pass)
{
	render(pass);
}

S32 LLDrawPoolAvatar::getNumPostDeferredPasses()
{
	return 10;
}

void LLDrawPoolAvatar::beginPostDeferredPass(S32 pass)
{
	switch (pass)
	{
		case 0:
			beginPostDeferredAlpha();
			break;

		case 1:
			beginRiggedFullbright();
			break;

		case 2:
			beginRiggedFullbrightShiny();
			break;

		case 3:
			beginDeferredRiggedAlpha();
			break;

		case 4:
			beginRiggedFullbrightAlpha();
			break;

		case 9:
			beginRiggedGlow();
			break;

		default:
			beginDeferredRiggedMaterialAlpha(pass - 5);
	}
}

void LLDrawPoolAvatar::beginPostDeferredAlpha()
{
	sSkipOpaque = true;
	sShaderLevel = mShaderLevel;
	sVertexProgram = &gDeferredAvatarAlphaProgram;
	sRenderingSkinned = true;

	gPipeline.bindDeferredShader(*sVertexProgram);

	sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);

	sDiffuseChannel = sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
}

void LLDrawPoolAvatar::beginDeferredRiggedAlpha()
{
	sVertexProgram = &gDeferredSkinnedAlphaProgram;
	gPipeline.bindDeferredShader(*sVertexProgram);
	sDiffuseChannel = sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
	gPipeline.enableLightsDynamic();
}

void LLDrawPoolAvatar::beginDeferredRiggedMaterialAlpha(S32 pass)
{
	switch (pass)
	{
		case 0:
			pass = 1;
			break;

		case 1:
			pass = 5;
			break;

		case 2:
			pass = 9;
			break;

		default:
			pass = 13;
	}

	pass += LLMaterial::SHADER_COUNT;

	if (LLPipeline::sUnderWaterRender)
	{
		sVertexProgram = &gDeferredMaterialWaterProgram[pass];
	}
	else
	{
		sVertexProgram = &gDeferredMaterialProgram[pass];
	}
	gPipeline.bindDeferredShader(*sVertexProgram);

	sDiffuseChannel = sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
	sNormalChannel = sVertexProgram->enableTexture(LLShaderMgr::BUMP_MAP);
	sSpecularChannel =
		sVertexProgram->enableTexture(LLShaderMgr::SPECULAR_MAP);
	gPipeline.enableLightsDynamic();
}

void LLDrawPoolAvatar::endDeferredRiggedAlpha()
{
	LLVertexBuffer::unbind();
	gPipeline.unbindDeferredShader(*sVertexProgram);
	sDiffuseChannel = 0;
	sNormalChannel = sSpecularChannel = -1;
	sVertexProgram = NULL;
}

void LLDrawPoolAvatar::endPostDeferredPass(S32 pass)
{
	switch (pass)
	{
		case 0:
			endPostDeferredAlpha();
			break;

		case 1:
			endRiggedFullbright();
			break;

		case 2:
			endRiggedFullbrightShiny();
			break;

		case 3:
			endDeferredRiggedAlpha();
			break;

		case 4:
			endRiggedFullbrightAlpha();
			break;

		case 5:
			endRiggedGlow();
			break;

		default:
			endDeferredRiggedAlpha();
	}
}

void LLDrawPoolAvatar::endPostDeferredAlpha()
{
	// If we are in software-blending, remember to set the fence _after_ we
	// draw so we wait till this rendering is done
	sRenderingSkinned = false;
	sSkipOpaque = false;

	gPipeline.unbindDeferredShader(*sVertexProgram);
	sDiffuseChannel = 0;
	sShaderLevel = mShaderLevel;
}

void LLDrawPoolAvatar::renderPostDeferred(S32 pass)
{
	// Map post deferred pass numbers to what render() expects
	static const S32 actual_pass[] =
	{
		2,	// skinned
		4,	// rigged fullbright
		6,	// rigged fullbright shiny
		7,	// rigged alpha
		8,	// rigged fullbright alpha
		9,	// rigged material alpha 1
		10,	// rigged material alpha 2
		11,	// rigged material alpha 3
		12,	// rigged material alpha 4
		13,	// rigged glow
	};

	S32 p = actual_pass[pass];

	if (LLPipeline::sImpostorRender)
	{
		// *HACK: for impostors so actual pass ends up being proper pass
		p -= 2;
	}

	sIsPostDeferredRender = true;
	render(p);
	sIsPostDeferredRender = false;
}

S32 LLDrawPoolAvatar::getNumShadowPasses()
{
	return NUM_SHADOW_PASSES;
}

void LLDrawPoolAvatar::beginShadowPass(S32 pass)
{
	LL_FAST_TIMER(FTM_SHADOW_AVATAR);

	if (pass == SHADOW_PASS_AVATAR_OPAQUE)
	{
		sVertexProgram = &gDeferredAvatarShadowProgram;

		if (sShaderLevel > 0)  // For hardware blending
		{
			sRenderingSkinned = true;
			sVertexProgram->bind();
		}

		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	}
	else if (pass == SHADOW_PASS_AVATAR_ALPHA_BLEND)
	{
		sVertexProgram = &gDeferredAvatarAlphaShadowProgram;

		// Bind diffuse tex so we can reference the alpha channel...
		if (!gUseNewShaders ||
			sVertexProgram->getUniformLocation(LLViewerShaderMgr::DIFFUSE_MAP) != -1)
		{
			sDiffuseChannel =
				sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
		}
		else
		{
			sDiffuseChannel = 0;
		}

		if (sShaderLevel > 0)  // For hardware blending
		{
			sRenderingSkinned = true;
			sVertexProgram->bind();
		}

		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	}
	else if (pass == SHADOW_PASS_AVATAR_ALPHA_MASK)
	{
		sVertexProgram = &gDeferredAvatarAlphaMaskShadowProgram;

		// Bind diffuse tex so we can reference the alpha channel...
		if (!gUseNewShaders ||
			sVertexProgram->getUniformLocation(LLViewerShaderMgr::DIFFUSE_MAP) != -1)
		{
			sDiffuseChannel =
				sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
		}
		else
		{
			sDiffuseChannel = 0;
		}

		if (sShaderLevel > 0)  // For hardware blending
		{
			sRenderingSkinned = true;
			sVertexProgram->bind();
		}

		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	}
	else if (pass == SHADOW_PASS_ATTACHMENT_ALPHA_BLEND)
	{
		sVertexProgram = &gDeferredAttachmentAlphaShadowProgram;

		// Bind diffuse tex so we can reference the alpha channel...
		if (!gUseNewShaders ||
			sVertexProgram->getUniformLocation(LLViewerShaderMgr::DIFFUSE_MAP) != -1)
		{
			sDiffuseChannel =
				sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
		}
		else
		{
			sDiffuseChannel = 0;
		}

		if (sShaderLevel > 0)  // For hardware blending
		{
			sRenderingSkinned = true;
			sVertexProgram->bind();
		}

		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	}
	else if (pass == SHADOW_PASS_ATTACHMENT_ALPHA_MASK)
	{
		sVertexProgram = &gDeferredAttachmentAlphaMaskShadowProgram;

		// Bind diffuse tex so we can reference the alpha channel...
		if (!gUseNewShaders ||
			sVertexProgram->getUniformLocation(LLViewerShaderMgr::DIFFUSE_MAP) != -1)
		{
			sDiffuseChannel =
				sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
		}
		else
		{
			sDiffuseChannel = 0;
		}

		if (sShaderLevel > 0)  // For hardware blending
		{
			sRenderingSkinned = true;
			sVertexProgram->bind();
		}

		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	}
	else	// SHADOW_PASS_ATTACHMENT_OPAQUE
	{
		sVertexProgram = &gDeferredAttachmentShadowProgram;

		// Bind diffuse tex so we can reference the alpha channel...
		if (!gUseNewShaders ||
			sVertexProgram->getUniformLocation(LLViewerShaderMgr::DIFFUSE_MAP) != -1)
		{
			sDiffuseChannel =
				sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
		}
		else
		{
			sDiffuseChannel = 0;
		}

		sVertexProgram->bind();
	}
}

void LLDrawPoolAvatar::endShadowPass(S32 pass)
{
	LL_FAST_TIMER(FTM_SHADOW_AVATAR);

	if (pass == SHADOW_PASS_ATTACHMENT_OPAQUE)
	{
		LLVertexBuffer::unbind();
	}

	if (sShaderLevel > 0)
	{
		sVertexProgram->unbind();
	}

	sVertexProgram = NULL;
	sRenderingSkinned = false;
	sShadowPass = -1;
}

void LLDrawPoolAvatar::renderShadow(S32 pass)
{
	LL_FAST_TIMER(FTM_SHADOW_AVATAR);

	if (mDrawFace.empty())
	{
		return;
	}

	const LLFace* facep = mDrawFace[0];
	if (!facep || !facep->getDrawable())
	{
		return;
	}

	LLVOAvatar* avatarp = (LLVOAvatar*)facep->getDrawable()->getVObj().get();
	if (!avatarp || avatarp->isDead() || avatarp->isUIAvatar() ||
		avatarp->mDrawable.isNull() || avatarp->isVisuallyMuted() ||
		avatarp->isImpostor())
	{
		return;
	}

	sShadowPass = pass;

	if (pass == SHADOW_PASS_AVATAR_OPAQUE)
	{
		sSkipTransparent = true;
		avatarp->renderSkinned();
		sSkipTransparent = false;
		return;
	}

	if (pass == SHADOW_PASS_AVATAR_ALPHA_BLEND ||
		pass == SHADOW_PASS_AVATAR_ALPHA_MASK)
	{
		sSkipOpaque = true;
		avatarp->renderSkinned();
		sSkipOpaque = false;
		return;
	}

	static LLCachedControl<bool> shadows(gSavedSettings, "RenderRiggedShadow");
	if (!shadows)
	{
		return;
	}

	// Rigged alpha
	if (pass == SHADOW_PASS_ATTACHMENT_ALPHA_BLEND)
	{
		sSkipOpaque = true;
		renderRigged(avatarp, RIGGED_MATERIAL_ALPHA);
		renderRigged(avatarp, RIGGED_MATERIAL_ALPHA_EMISSIVE);
		renderRigged(avatarp, RIGGED_ALPHA);
		renderRigged(avatarp, RIGGED_FULLBRIGHT_ALPHA);
		renderRigged(avatarp, RIGGED_GLOW);
		renderRigged(avatarp, RIGGED_SPECMAP_BLEND);
		renderRigged(avatarp, RIGGED_NORMMAP_BLEND);
		renderRigged(avatarp, RIGGED_NORMSPEC_BLEND);
		sSkipOpaque = false;
		return;
	}

	// Rigged alpha mask
	if (pass == SHADOW_PASS_ATTACHMENT_ALPHA_MASK)
	{
		sSkipOpaque = true;
		renderRigged(avatarp, RIGGED_MATERIAL_ALPHA_MASK);
		renderRigged(avatarp, RIGGED_NORMMAP_MASK);
		renderRigged(avatarp, RIGGED_SPECMAP_MASK);
		renderRigged(avatarp, RIGGED_NORMSPEC_MASK);
		renderRigged(avatarp, RIGGED_GLOW);
		sSkipOpaque = false;
		return;
	}

	// SHADOW_PASS_ATTACHMENT_OPAQUE
	sSkipTransparent = true;
	renderRigged(avatarp, RIGGED_MATERIAL);
	renderRigged(avatarp, RIGGED_SPECMAP);
	renderRigged(avatarp, RIGGED_SPECMAP_EMISSIVE);
	renderRigged(avatarp, RIGGED_NORMMAP);
	renderRigged(avatarp, RIGGED_NORMMAP_EMISSIVE);
	renderRigged(avatarp, RIGGED_NORMSPEC);
	renderRigged(avatarp, RIGGED_NORMSPEC_EMISSIVE);
	renderRigged(avatarp, RIGGED_SIMPLE);
	renderRigged(avatarp, RIGGED_FULLBRIGHT);
	renderRigged(avatarp, RIGGED_SHINY);
	renderRigged(avatarp, RIGGED_FULLBRIGHT_SHINY);
	renderRigged(avatarp, RIGGED_GLOW);
	renderRigged(avatarp, RIGGED_DEFERRED_BUMP);
	renderRigged(avatarp, RIGGED_DEFERRED_SIMPLE);
	sSkipTransparent = false;
}

S32 LLDrawPoolAvatar::getNumPasses()
{
	return LLPipeline::sImpostorRender ? 8 : 10;
}

S32 LLDrawPoolAvatar::getNumDeferredPasses()
{
	return LLPipeline::sImpostorRender ? 19 : 21;
}

void LLDrawPoolAvatar::render(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_CHARACTERS);
	if (LLPipeline::sImpostorRender)
	{
		renderAvatars(NULL, pass + 2);
		return;
	}
	renderAvatars(NULL, pass); // Render all avatars
}

void LLDrawPoolAvatar::beginRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_CHARACTERS);
	//reset vertex buffer mappings
	LLVertexBuffer::unbind();

	if (LLPipeline::sImpostorRender)
	{
		// Impostor render does not have impostors or rigid rendering
		pass += 2;
	}

	switch (pass)
	{
		case 0:
			beginImpostor();
			break;

		case 1:
			beginRigid();
			break;

		case 2:
			beginSkinned();
			break;

		case 3:
			beginRiggedSimple();
			break;

		case 4:
			beginRiggedFullbright();
			break;

		case 5:
			beginRiggedShinySimple();
			break;

		case 6:
			beginRiggedFullbrightShiny();
			break;

		case 7:
			beginRiggedAlpha();
			break;

		case 8:
			beginRiggedFullbrightAlpha();
			break;

		case 9:
			beginRiggedGlow();
	}

	if (pass == 0)
	{
		// Make sure no stale colors are left over from a previous render
		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	}
}

void LLDrawPoolAvatar::endRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_CHARACTERS);

	if (LLPipeline::sImpostorRender)
	{
		pass += 2;
	}

	switch (pass)
	{
		case 0:
			endImpostor();
			break;

		case 1:
			endRigid();
			break;

		case 2:
			endSkinned();
			break;

		case 3:
			endRiggedSimple();
			break;

		case 4:
			endRiggedFullbright();
			break;

		case 5:
			endRiggedShinySimple();
			break;

		case 6:
			endRiggedFullbrightShiny();
			break;

		case 7:
			endRiggedAlpha();
			break;

		case 8:
			endRiggedFullbrightAlpha();
			break;

		case 9:
			endRiggedGlow();
	}
}

void LLDrawPoolAvatar::beginImpostor()
{
	if (!LLPipeline::sReflectionRender)
	{
		LLVOAvatar::sRenderDistance = llclamp(LLVOAvatar::sRenderDistance,
											  16.f, 256.f);
		LLVOAvatar::sNumVisibleAvatars = 0;
	}

	gImpostorProgram.bind();
	gImpostorProgram.setMinimumAlpha(0.01f);

	gPipeline.enableLightsFullbright();
	sDiffuseChannel = 0;
}

void LLDrawPoolAvatar::endImpostor()
{
	gImpostorProgram.unbind();

	gPipeline.enableLightsDynamic();
}

void LLDrawPoolAvatar::beginRigid()
{
	if (gPipeline.canUseVertexShaders())
	{
		if (LLPipeline::sUnderWaterRender)
		{
			sVertexProgram = &gObjectAlphaMaskNoColorWaterProgram;
		}
		else
		{
			sVertexProgram = &gObjectAlphaMaskNoColorProgram;
		}

		if (sVertexProgram)
		{
			// Eyeballs render with the specular shader
			sVertexProgram->bind();
			sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);
			if (gUseNewShaders)
			{
				sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
										  LLPipeline::sRenderingHUDs ? 1 : 0);
			}
		}
	}
	else
	{
		sVertexProgram = NULL;
	}
}

void LLDrawPoolAvatar::endRigid()
{
	sShaderLevel = mShaderLevel;
	if (sVertexProgram)
	{
		sVertexProgram->unbind();
	}
}

void LLDrawPoolAvatar::beginDeferredImpostor()
{
	if (!LLPipeline::sReflectionRender)
	{
		LLVOAvatar::sRenderDistance = llclamp(LLVOAvatar::sRenderDistance,
											  16.f, 256.f);
		LLVOAvatar::sNumVisibleAvatars = 0;
	}

	sVertexProgram = &gDeferredImpostorProgram;
	sSpecularChannel =
		sVertexProgram->enableTexture(LLShaderMgr::SPECULAR_MAP);
	sNormalChannel =
		sVertexProgram->enableTexture(LLShaderMgr::DEFERRED_NORMAL);
	sDiffuseChannel = sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
	sVertexProgram->bind();
	sVertexProgram->setMinimumAlpha(0.01f);
}

void LLDrawPoolAvatar::endDeferredImpostor()
{
	sShaderLevel = mShaderLevel;
	sVertexProgram->disableTexture(LLShaderMgr::DEFERRED_NORMAL);
	sVertexProgram->disableTexture(LLShaderMgr::SPECULAR_MAP);
	sVertexProgram->disableTexture(LLShaderMgr::DIFFUSE_MAP);
	gPipeline.unbindDeferredShader(*sVertexProgram);
    sVertexProgram = NULL;
    sDiffuseChannel = 0;
}

void LLDrawPoolAvatar::beginDeferredRigid()
{
	sVertexProgram = &gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram;
	sDiffuseChannel = sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
	sVertexProgram->bind();
	sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);
	if (gUseNewShaders)
	{
		sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
								  LLPipeline::sRenderingHUDs ? 1 : 0);
	}
}

void LLDrawPoolAvatar::endDeferredRigid()
{
	sShaderLevel = mShaderLevel;
	sVertexProgram->disableTexture(LLShaderMgr::DIFFUSE_MAP);
	sVertexProgram->unbind();
	gGL.getTexUnit(0)->activate();
}

void LLDrawPoolAvatar::beginSkinned()
{
	if (sShaderLevel > 0)
	{
		if (LLPipeline::sUnderWaterRender)
		{
			sVertexProgram = &gAvatarWaterProgram;
			sShaderLevel = llmin(1U, sShaderLevel);
		}
		else
		{
			sVertexProgram = &gAvatarProgram;
		}
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		sVertexProgram = &gObjectAlphaMaskNoColorWaterProgram;
	}
	else
	{
		sVertexProgram = &gObjectAlphaMaskNoColorProgram;
	}

	if (sShaderLevel > 0)  // For hardware blending
	{
		sRenderingSkinned = true;

		sVertexProgram->bind();
		sVertexProgram->enableTexture(LLShaderMgr::BUMP_MAP);
		if (gUseNewShaders)
		{
			sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
									  LLPipeline::sRenderingHUDs ? 1 : 0);
		}
		gGL.getTexUnit(0)->activate();
	}
	else if (gPipeline.canUseVertexShaders())
	{
		// Software skinning, use a basic shader for windlight.
		// *TODO: find a better fallback method for software skinning.
		sVertexProgram->bind();
		if (gUseNewShaders)
		{
			sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
									  LLPipeline::sRenderingHUDs ? 1 : 0);
		}
	}

	sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);
}

void LLDrawPoolAvatar::endSkinned()
{
	// If we are in software-blending, remember to set the fence _after_ we
	// draw so we wait till this rendering is done
	if (sShaderLevel > 0)
	{
		sRenderingSkinned = false;
		sVertexProgram->disableTexture(LLShaderMgr::BUMP_MAP);
		gGL.getTexUnit(0)->activate();
		sVertexProgram->unbind();
		sShaderLevel = mShaderLevel;
	}
	else if (gPipeline.canUseVertexShaders())
	{
		// Software skinning, use a basic shader for windlight.
		// *TODO: find a better fallback method for software skinning.
		sVertexProgram->unbind();
	}

	gGL.getTexUnit(0)->activate();
}

void LLDrawPoolAvatar::beginRiggedSimple()
{
	if (sShaderLevel > 0)
	{
		if (LLPipeline::sUnderWaterRender)
		{
			sVertexProgram = &gSkinnedObjectSimpleWaterProgram;
		}
		else
		{
			sVertexProgram = &gSkinnedObjectSimpleProgram;
		}
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		sVertexProgram = &gObjectSimpleNonIndexedWaterProgram;
	}
	else
	{
		sVertexProgram = &gObjectSimpleNonIndexedProgram;
	}

	if (sShaderLevel > 0 || gPipeline.canUseVertexShaders())
	{
		sDiffuseChannel = 0;
		sVertexProgram->bind();
		if (gUseNewShaders)
		{
			sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
									  LLPipeline::sRenderingHUDs ? 1 : 0);
		}
	}
}

void LLDrawPoolAvatar::endRiggedSimple()
{
	LLVertexBuffer::unbind();
	if (sShaderLevel > 0 || gPipeline.canUseVertexShaders())
	{
		sVertexProgram->unbind();
		sVertexProgram = NULL;
	}
}

void LLDrawPoolAvatar::beginRiggedAlpha()
{
	beginRiggedSimple();
}

void LLDrawPoolAvatar::endRiggedAlpha()
{
	endRiggedSimple();
}

void LLDrawPoolAvatar::beginRiggedFullbrightAlpha()
{
	beginRiggedFullbright();
}

void LLDrawPoolAvatar::endRiggedFullbrightAlpha()
{
	endRiggedFullbright();
}

void LLDrawPoolAvatar::beginRiggedGlow()
{
	if (sShaderLevel > 0)
	{
		if (LLPipeline::sUnderWaterRender)
		{
			sVertexProgram = &gSkinnedObjectEmissiveWaterProgram;
		}
		else
		{
			sVertexProgram = &gSkinnedObjectEmissiveProgram;
		}
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		sVertexProgram = &gObjectEmissiveNonIndexedWaterProgram;
	}
	else
	{
		sVertexProgram = &gObjectEmissiveNonIndexedProgram;
	}

	if (sShaderLevel > 0 || gPipeline.canUseVertexShaders())
	{
		sDiffuseChannel = 0;
		sVertexProgram->bind();

		sVertexProgram->uniform1f(LLShaderMgr::TEXTURE_GAMMA,
								  LLPipeline::sRenderDeferred ? 2.2f : 1.1f);

		if (gUseNewShaders)
		{
			sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
									  LLPipeline::sRenderingHUDs ? 1 : 0);
		}

		F32 gamma = LLPipeline::RenderDeferredDisplayGamma;
		sVertexProgram->uniform1f(LLShaderMgr::DISPLAY_GAMMA, 1.f / gamma);
	}
}

void LLDrawPoolAvatar::endRiggedGlow()
{
	endRiggedFullbright();
}

void LLDrawPoolAvatar::beginRiggedFullbright()
{
	if (sShaderLevel > 0)
	{
		if (LLPipeline::sUnderWaterRender)
		{
			sVertexProgram = &gSkinnedObjectFullbrightWaterProgram;
		}
		else if (LLPipeline::sRenderDeferred)
		{
			sVertexProgram = &gDeferredSkinnedFullbrightProgram;
		}
		else
		{
			sVertexProgram = &gSkinnedObjectFullbrightProgram;
		}
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		sVertexProgram = &gObjectFullbrightNonIndexedWaterProgram;
	}
	else
	{
		sVertexProgram = &gObjectFullbrightNonIndexedProgram;
	}

	if (sShaderLevel > 0 || gPipeline.canUseVertexShaders())
	{
		sDiffuseChannel = 0;
		sVertexProgram->bind();

		bool render_hud = LLPipeline::sRenderingHUDs;
		if (render_hud || !LLPipeline::sRenderDeferred)
		{
			sVertexProgram->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 1.f);
		}
		else
		{
			sVertexProgram->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 2.2f);

			F32 gamma = LLPipeline::RenderDeferredDisplayGamma;
			sVertexProgram->uniform1f(LLShaderMgr::DISPLAY_GAMMA, 1.f / gamma);
		}
		if (gUseNewShaders)
		{
			sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
									  render_hud ? 1 : 0);
		}
	}
}

void LLDrawPoolAvatar::endRiggedFullbright()
{
	LLVertexBuffer::unbind();
	if (sShaderLevel > 0 || gPipeline.canUseVertexShaders())
	{
		sVertexProgram->unbind();
		sVertexProgram = NULL;
	}
}

void LLDrawPoolAvatar::beginRiggedShinySimple()
{
	if (sShaderLevel > 0)
	{
		if (LLPipeline::sUnderWaterRender)
		{
			sVertexProgram = &gSkinnedObjectShinySimpleWaterProgram;
		}
		else
		{
			sVertexProgram = &gSkinnedObjectShinySimpleProgram;
		}
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		sVertexProgram = &gObjectShinyNonIndexedWaterProgram;
	}
	else
	{
		sVertexProgram = &gObjectShinyNonIndexedProgram;
	}

	if (sShaderLevel > 0 || gPipeline.canUseVertexShaders())
	{
		sVertexProgram->bind();
		if (gUseNewShaders)
		{
			sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
									  LLPipeline::sRenderingHUDs ? 1 : 0);
		}
		LLDrawPoolBump::bindCubeMap(sVertexProgram, 2, sDiffuseChannel,
									sCubeChannel, false);
	}
}

void LLDrawPoolAvatar::endRiggedShinySimple()
{
	LLVertexBuffer::unbind();
	if (sShaderLevel > 0 || gPipeline.canUseVertexShaders())
	{
		LLDrawPoolBump::unbindCubeMap(sVertexProgram, 2, sDiffuseChannel,
									  false);
		sVertexProgram->unbind();
		sVertexProgram = NULL;
	}
}

void LLDrawPoolAvatar::beginRiggedFullbrightShiny()
{
	if (sShaderLevel > 0)
	{
		if (LLPipeline::sUnderWaterRender)
		{
			sVertexProgram = &gSkinnedObjectFullbrightShinyWaterProgram;
		}
		else if (LLPipeline::sRenderDeferred)
		{
			sVertexProgram = &gDeferredSkinnedFullbrightShinyProgram;
		}
		else
		{
			sVertexProgram = &gSkinnedObjectFullbrightShinyProgram;
		}
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		sVertexProgram = &gObjectFullbrightShinyNonIndexedWaterProgram;
	}
	else
	{
		sVertexProgram = &gObjectFullbrightShinyNonIndexedProgram;
	}

	if (sShaderLevel > 0 || gPipeline.canUseVertexShaders())
	{
		sVertexProgram->bind();
		bool render_hud = LLPipeline::sRenderingHUDs;
		if (gUseNewShaders)
		{
			sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
									  render_hud ? 1 : 0);
		}
		LLDrawPoolBump::bindCubeMap(sVertexProgram, 2, sDiffuseChannel,
									sCubeChannel, false);

		if (render_hud || !LLPipeline::sRenderDeferred)
		{
			sVertexProgram->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 1.f);
		}
		else
		{
			sVertexProgram->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 2.2f);
			F32 gamma = LLPipeline::RenderDeferredDisplayGamma;
			sVertexProgram->uniform1f(LLShaderMgr::DISPLAY_GAMMA, 1.f / gamma);
		}
	}
}

void LLDrawPoolAvatar::endRiggedFullbrightShiny()
{
	LLVertexBuffer::unbind();
	if (sShaderLevel > 0 || gPipeline.canUseVertexShaders())
	{
		LLDrawPoolBump::unbindCubeMap(sVertexProgram, 2, sDiffuseChannel,
									  false);
		sVertexProgram->unbind();
		sVertexProgram = NULL;
	}
}

void LLDrawPoolAvatar::beginDeferredRiggedSimple()
{
	sVertexProgram = &gDeferredSkinnedDiffuseProgram;
	sDiffuseChannel = 0;
	sVertexProgram->bind();
	if (gUseNewShaders)
	{
		sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
								  LLPipeline::sRenderingHUDs ? 1 : 0);
	}
}

void LLDrawPoolAvatar::endDeferredRiggedSimple()
{
	LLVertexBuffer::unbind();
	sVertexProgram->unbind();
	sVertexProgram = NULL;
}

void LLDrawPoolAvatar::beginDeferredRiggedBump()
{
	sVertexProgram = &gDeferredSkinnedBumpProgram;
	sVertexProgram->bind();
	if (gUseNewShaders)
	{
		sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
								  LLPipeline::sRenderingHUDs ? 1 : 0);
	}
	sNormalChannel = sVertexProgram->enableTexture(LLShaderMgr::BUMP_MAP);
	sDiffuseChannel = sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
}

void LLDrawPoolAvatar::endDeferredRiggedBump()
{
	LLVertexBuffer::unbind();
	sVertexProgram->disableTexture(LLShaderMgr::BUMP_MAP);
	sVertexProgram->disableTexture(LLShaderMgr::DIFFUSE_MAP);
	sVertexProgram->unbind();
	sNormalChannel = -1;
	sDiffuseChannel = 0;
	sVertexProgram = NULL;
}

void LLDrawPoolAvatar::beginDeferredRiggedMaterial(S32 pass)
{
	if (pass == 1 || pass == 5 || pass == 9 || pass == 13)
	{
		return;	// skip alpha passes
	}
	if (LLPipeline::sUnderWaterRender)
	{
		sVertexProgram =
			&gDeferredMaterialWaterProgram[pass + LLMaterial::SHADER_COUNT];
	}
	else
	{
		sVertexProgram =
			&gDeferredMaterialProgram[pass + LLMaterial::SHADER_COUNT];
	}
	sVertexProgram->bind();
	if (gUseNewShaders)
	{
		sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
								  LLPipeline::sRenderingHUDs ? 1 : 0);
	}
	sNormalChannel = sVertexProgram->enableTexture(LLShaderMgr::BUMP_MAP);
	sSpecularChannel =
		sVertexProgram->enableTexture(LLShaderMgr::SPECULAR_MAP);
	sDiffuseChannel = sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
}

void LLDrawPoolAvatar::endDeferredRiggedMaterial(S32 pass)
{
	if (pass == 1 || pass == 5 || pass == 9 || pass == 13)
	{
		return;	// skip alpha passes
	}
	LLVertexBuffer::unbind();
	sVertexProgram->disableTexture(LLShaderMgr::BUMP_MAP);
	sVertexProgram->disableTexture(LLShaderMgr::SPECULAR_MAP);
	sVertexProgram->disableTexture(LLShaderMgr::DIFFUSE_MAP);
	sVertexProgram->unbind();
	sNormalChannel = -1;
	sDiffuseChannel = 0;
	sVertexProgram = NULL;
}

void LLDrawPoolAvatar::beginDeferredSkinned()
{
	sShaderLevel = mShaderLevel;
	sVertexProgram = &gDeferredAvatarProgram;

	sRenderingSkinned = true;

	sVertexProgram->bind();
	sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);
	if (gUseNewShaders)
	{
		sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO,
								  LLPipeline::sRenderingHUDs ? 1 : 0);
	}

	sDiffuseChannel = sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);

	gGL.getTexUnit(0)->activate();
}

void LLDrawPoolAvatar::endDeferredSkinned()
{
	// If we are in software-blending, remember to set the fence _after_ we
	// draw so we wait till this rendering is done
	sRenderingSkinned = false;
	sVertexProgram->unbind();

	sVertexProgram->disableTexture(LLShaderMgr::DIFFUSE_MAP);

	sShaderLevel = mShaderLevel;

	gGL.getTexUnit(0)->activate();
}

void LLDrawPoolAvatar::renderAvatars(LLVOAvatar* single_avatar, S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_AVATARS);

	if (pass == -1)
	{
		for (S32 i = 1, count = getNumPasses(); i < count; ++i)
		{
			// Skip foot shadows
			prerender();
			beginRenderPass(i);
			renderAvatars(single_avatar, i);
			endRenderPass(i);
		}
		return;
	}

//MK
	bool rlvcam = gRLenabled && gRLInterface.mVisionRestricted;
//mk
	if (!rlvcam && mDrawFace.empty() && !single_avatar)
	{
		return;
	}

	LLVOAvatar* avatarp;
	if (single_avatar)
	{
		avatarp = single_avatar;
	}
//MK
	else if (mDrawFace.empty())
	{
		if (!isAgentAvatarValid() || getAvatar() != gAgentAvatarp)
		{
			return;
		}
		avatarp = gAgentAvatarp;
	}
//mk
	else
	{
		const LLFace* facep = mDrawFace[0];
		if (!facep || !facep->getDrawable())
		{
			return;
		}
		avatarp = (LLVOAvatar*)facep->getDrawable()->getVObj().get();
	}

    if (!avatarp || avatarp->isDead() || avatarp->mDrawable.isNull())
	{
		return;
	}

	static LLCachedControl<bool> hit_box(gSavedSettings, "RenderDebugHitBox");
	if (pass == 1 && hit_box)
	{
		gDebugProgram.bind();

		// Set up drawing mode and remove any texture in use
		LLGLEnable blend(GL_BLEND);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		// Save current world matrix
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();

		const LLColor4& avatar_color = avatarp->getMinimapColor();
		gGL.diffuseColor4f(avatar_color.mV[VX], avatar_color.mV[VY],
						   avatar_color.mV[VZ], avatar_color.mV[VW]);

		const LLVector3& pos = avatarp->getPositionAgent();
		const LLVector3& size = avatarp->getScale();
		LLQuaternion rot = avatarp->getRotationRegion();

		// Set up and rotate the hit box to avatar orientation and half the
		// avatar size in either direction.
		static const LLVector3 sv1 = LLVector3(0.5f, 0.5f, 0.5f);
		static const LLVector3 sv2 = LLVector3(-0.5f, 0.5f, 0.5f);
		static const LLVector3 sv3 = LLVector3(-0.5f, -0.5f, 0.5f);
		static const LLVector3 sv4 = LLVector3(0.5f, -0.5f, 0.5f);
		LLVector3 v1 = size.scaledVec(sv1) * rot;
		LLVector3 v2 = size.scaledVec(sv2) * rot;
		LLVector3 v3 = size.scaledVec(sv3) * rot;
		LLVector3 v4 = size.scaledVec(sv4) * rot;

		// Box corners coordinates
		LLVector3 pospv1 = pos + v1;
		LLVector3 posmv1 = pos - v1;
		LLVector3 pospv2 = pos + v2;
		LLVector3 posmv2 = pos - v2;
		LLVector3 pospv3 = pos + v3;
		LLVector3 posmv3 = pos - v3;
		LLVector3 pospv4 = pos + v4;
		LLVector3 posmv4 = pos - v4;

		// Render the box

		gGL.begin(LLRender::LINES);

		// Top
		gGL.vertex3fv(pospv1.mV);
		gGL.vertex3fv(pospv2.mV);
		gGL.vertex3fv(pospv2.mV);
		gGL.vertex3fv(pospv3.mV);
		gGL.vertex3fv(pospv3.mV);
		gGL.vertex3fv(pospv4.mV);
		gGL.vertex3fv(pospv4.mV);
		gGL.vertex3fv(pospv1.mV);

		// Bottom
		gGL.vertex3fv(posmv1.mV);
		gGL.vertex3fv(posmv2.mV);
		gGL.vertex3fv(posmv2.mV);
		gGL.vertex3fv(posmv3.mV);
		gGL.vertex3fv(posmv3.mV);
		gGL.vertex3fv(posmv4.mV);
		gGL.vertex3fv(posmv4.mV);
		gGL.vertex3fv(posmv1.mV);
		
		// Right
		gGL.vertex3fv(pospv1.mV);
		gGL.vertex3fv(posmv3.mV);
		gGL.vertex3fv(pospv4.mV);
		gGL.vertex3fv(posmv2.mV);
		
		// Left
		gGL.vertex3fv(pospv2.mV);
		gGL.vertex3fv(posmv4.mV);
		gGL.vertex3fv(pospv3.mV);
		gGL.vertex3fv(posmv1.mV);

		gGL.end();

		// Restore world matrix
		gGL.popMatrix();

		gDebugProgram.unbind();
	}

	if (!single_avatar && !avatarp->isFullyLoaded())
	{
		if (pass == 0 &&
			(!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES) ||
			 LLViewerPartSim::getMaxPartCount() <= 0))
		{
			// Debug code to draw a sphere in place of avatar
			gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep);

			gGL.setColorMask(true, true);

			LLVector3 pos = avatarp->getPositionAgent();
			gGL.color4f(1.f, 1.f, 1.f, 0.7f);

			gGL.pushMatrix();

			gGL.translatef((F32)pos.mV[VX], (F32)pos.mV[VY], (F32)pos.mV[VZ]);
			gGL.scalef(0.15f, 0.15f, 0.3f);
			gSphere.renderGGL();

			gGL.popMatrix();

			gGL.setColorMask(true, false);
		}
		// Do not render please
		return;
	}

	if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_AVATAR))
	{
		return;
	}

	bool impostor = !single_avatar && avatarp->isImpostor();
	bool only_pass0 = impostor ||
					  (single_avatar && !avatarp->needsImpostorUpdate() &&
					   avatarp->getVisualMuteSettings() ==
						LLVOAvatar::AV_DO_NOT_RENDER);
 	if (pass != 0 && only_pass0)
	{
		// Do not draw anything but the impostor for impostored avatars
		return;
	}

	if (pass == 0 && !impostor && LLPipeline::sUnderWaterRender)
	{
		// Do not draw foot shadows under water
		return;
	}

	if (single_avatar)
	{
		LLVOAvatar* attached_av = avatarp->getAttachedAvatar();
		// Do not render any animesh for visually muted avatars
		if (attached_av && attached_av->isVisuallyMuted())
		{
			return;
		}
	}

	if (pass == 0)
	{
		if (!LLPipeline::sReflectionRender)
		{
			++LLVOAvatar::sNumVisibleAvatars;
		}

		if (only_pass0)
		{
			if (LLPipeline::sRenderDeferred && !LLPipeline::sReflectionRender &&
				avatarp->mImpostor.isComplete())
			{
				U32 num_tex = avatarp->mImpostor.getNumTextures();
				if (sNormalChannel > -1 && num_tex >= 3)
				{
					avatarp->mImpostor.bindTexture(2, sNormalChannel);
				}
				if (sSpecularChannel > -1 && num_tex >= 2)
				{
					avatarp->mImpostor.bindTexture(1, sSpecularChannel);
				}
			}
			avatarp->renderImpostor(avatarp->getMutedAVColor(),
									sDiffuseChannel);
		}
		return;
	}

	if (pass == 1)
	{
		// Render rigid meshes (eyeballs) first
		avatarp->renderRigid();
		return;
	}

	if (pass == 3)
	{
		if (sIsDeferredRender)
		{
			renderDeferredRiggedSimple(avatarp);
		}
		else
		{
			renderRiggedSimple(avatarp);

			if (LLPipeline::sRenderDeferred)
			{
				// Render "simple" materials
				renderRigged(avatarp, RIGGED_MATERIAL);
				renderRigged(avatarp, RIGGED_MATERIAL_ALPHA_MASK);
				renderRigged(avatarp, RIGGED_MATERIAL_ALPHA_EMISSIVE);
				renderRigged(avatarp, RIGGED_NORMMAP);
				renderRigged(avatarp, RIGGED_NORMMAP_MASK);
				renderRigged(avatarp, RIGGED_NORMMAP_EMISSIVE);
				renderRigged(avatarp, RIGGED_SPECMAP);
				renderRigged(avatarp, RIGGED_SPECMAP_MASK);
				renderRigged(avatarp, RIGGED_SPECMAP_EMISSIVE);
				renderRigged(avatarp, RIGGED_NORMSPEC);
				renderRigged(avatarp, RIGGED_NORMSPEC_MASK);
				renderRigged(avatarp, RIGGED_NORMSPEC_EMISSIVE);
			}
		}
		return;
	}

	if (pass == 4)
	{
		if (sIsDeferredRender)
		{
			renderDeferredRiggedBump(avatarp);
		}
		else
		{
			renderRiggedFullbright(avatarp);
		}

		return;
	}

	if (sIsDeferredRender && pass >= 5 && pass <= 21)
	{
		S32 p = pass - 5;
		if (p != 1 && p != 5 && p != 9 && p != 13)
		{
			renderDeferredRiggedMaterial(avatarp, p);
		}
		return;
	}

	if (pass == 5)
	{
		renderRiggedShinySimple(avatarp);
		return;
	}

	if (pass == 6)
	{
//MK
		{
			LL_TRACY_TIMER(TRC_RLV_RENDER_LIMITS);
			// Possibly draw a big black sphere around our avatar if the camera
			// render is limited
			if (gRLenabled && avatarp == gAgentAvatarp && avatarp->getVisible())
			{
				gRLInterface.drawRenderLimit(false);
			}
		}
//mk
		renderRiggedFullbrightShiny(avatarp);
		return;
	}

	if (pass == 7)
	{
		renderRiggedAlpha(avatarp);

		// Render transparent materials under water
		if (LLPipeline::sRenderDeferred && !sIsPostDeferredRender)
		{
			LLGLEnable blend(GL_BLEND);

			gGL.setColorMask(true, true);
			gGL.blendFunc(LLRender::BF_SOURCE_ALPHA,
						  LLRender::BF_ONE_MINUS_SOURCE_ALPHA,
						  LLRender::BF_ZERO,
						  LLRender::BF_ONE_MINUS_SOURCE_ALPHA);

			renderRigged(avatarp, RIGGED_MATERIAL_ALPHA);
			renderRigged(avatarp, RIGGED_SPECMAP_BLEND);
			renderRigged(avatarp, RIGGED_NORMMAP_BLEND);
			renderRigged(avatarp, RIGGED_NORMSPEC_BLEND);
			gGL.setSceneBlendType(LLRender::BT_ALPHA);
			gGL.setColorMask(true, false);
		}
		return;
	}

	if (pass == 8)
	{
		renderRiggedFullbrightAlpha(avatarp);
		return;
	}

	if (pass >= 9 && pass < 13)
	{
		if (LLPipeline::sRenderDeferred && sIsPostDeferredRender)
		{
			S32 p = 0;
			switch (pass)
			{
				case 9:
					p = 1;
					break;

				case 10:
					p = 5;
					break;

				case 11:
					p = 9;
					break;

				case 12:
					p = 13;
			}

			LLGLEnable blend(GL_BLEND);
			renderDeferredRiggedMaterial(avatarp, p);
			return;
		}
		else if (pass == 9)
		{
			renderRiggedGlow(avatarp);
			return;
		}
	}

	if (pass == 13)
	{
		renderRiggedGlow(avatarp);
		return;
	}

	if (sShaderLevel >= SHADER_LEVEL_CLOTH)
	{
		LLMatrix4 rot_mat;
		gViewerCamera.getMatrixToLocal(rot_mat);
		LLMatrix4 cfr(OGL_TO_CFR_ROTATION);
		rot_mat *= cfr;

		LLVector4 wind;
		wind.set(avatarp->mWindVec);
		wind.mV[VW] = 0.f;
		wind = wind * rot_mat;
		wind.mV[VW] = avatarp->mWindVec.mV[VW];

		sVertexProgram->uniform4fv(LLShaderMgr::AVATAR_WIND, 1, wind.mV);
		F32 phase = -(avatarp->mRipplePhase);

		F32 freq = 7.f + 2.f * noise1(avatarp->mRipplePhase);
		LLVector4 sin_params(freq, freq, freq, phase);
		sVertexProgram->uniform4fv(LLShaderMgr::AVATAR_SINWAVE, 1,
								   sin_params.mV);

		LLVector4 gravity(0.f, 0.f, -CLOTHING_GRAVITY_EFFECT, 0.f);
		gravity = gravity * rot_mat;
		sVertexProgram->uniform4fv(LLShaderMgr::AVATAR_GRAVITY, 1, gravity.mV);
	}

	if (!single_avatar || avatarp == single_avatar)
	{
		avatarp->renderSkinned();
	}
}

bool LLDrawPoolAvatar::getRiggedGeometry(LLFace* face,
										 LLPointer<LLVertexBuffer>& buffer,
										 U32 data_mask,
										 const LLMeshSkinInfo* skin,
										 LLVolume* volume,
										 const LLVolumeFace& vol_face)
{
	if (vol_face.mNumVertices > 65536 || vol_face.mNumVertices < 0 ||
		vol_face.mNumIndices < 0)
	{
		return false;
	}

	face->setGeomIndex(0);
	face->setIndicesIndex(0);

	// Rigged faces do not batch textures
	if (face->getTextureIndex() != FACE_DO_NOT_BATCH_TEXTURES)
	{
		face->setDrawInfo(NULL);
		face->setTextureIndex(FACE_DO_NOT_BATCH_TEXTURES);
	}

	if (buffer.isNull() || buffer->getTypeMask() != data_mask ||
		!buffer->isWriteable())
	{
		// Make a new buffer
		if (sShaderLevel > 0)
		{
			buffer = new LLVertexBuffer(data_mask, GL_DYNAMIC_DRAW_ARB);
		}
		else
		{
			buffer = new LLVertexBuffer(data_mask, GL_STREAM_DRAW_ARB);
		}
		if (!buffer->allocateBuffer(vol_face.mNumVertices,
									vol_face.mNumIndices, true))
		{
			llwarns << "Failed to allocate vertex buffer for "
					<< vol_face.mNumVertices << " vertices and "
					<< vol_face.mNumIndices << " indices" << llendl;
			// Allocate a dummy triangle
			buffer->allocateBuffer(1, 3, true);
			memset((U8*)buffer->getMappedData(), 0, buffer->getSize());
			memset((U8*)buffer->getMappedIndices(), 0,
				   buffer->getIndicesSize());
		}
	}
	// Resize existing buffer
	else if (!buffer->resizeBuffer(vol_face.mNumVertices,
								   vol_face.mNumIndices))
	{
		llwarns << "Failed to resize vertex buffer for "
				<< vol_face.mNumVertices << " vertices and "
				<< vol_face.mNumIndices << " indices" << llendl;
		// Allocate a dummy triangle
		buffer->resizeBuffer(1, 3);
		memset((U8*)buffer->getMappedData(), 0, buffer->getSize());
		memset((U8*)buffer->getMappedIndices(), 0, buffer->getIndicesSize());
	}

	face->setSize(buffer->getNumVerts(), buffer->getNumIndices());
	face->setVertexBuffer(buffer);

	U16 offset = 0;

	LLMatrix4 mat_vert = skin->mBindShapeMatrix;

	LLMatrix4a m(mat_vert);
	m.invert();
	m.transpose();
	F32 mat3[] = {	m.mMatrix[0][0], m.mMatrix[0][1], m.mMatrix[0][2],
					m.mMatrix[1][0], m.mMatrix[1][1], m.mMatrix[1][2],
					m.mMatrix[2][0], m.mMatrix[2][1], m.mMatrix[2][2] };
	LLMatrix3 mat_normal(mat3);

	// Let getGeometryVolume know if alpha should override shiny
	U32 type = gPipeline.getPoolTypeFromTE(face->getTextureEntry(),
										   face->getTexture());

	if (type == LLDrawPool::POOL_ALPHA)
	{
		face->setPoolType(LLDrawPool::POOL_ALPHA);
	}
	else
	{
		face->setPoolType(mType);	// Either POOL_AVATAR or POOL_PUPPET
	}

	// MAINT-3211: Fix for texture animations not working properly on rigged
	// attachments when worn from inventory.
	if (face->mTextureMatrix)
	{
		face->setState(LLFace::TEXTURE_ANIM);
	}
	else
	{
		face->clearState(LLFace::TEXTURE_ANIM);
	}

	face->getGeometryVolume(*volume, face->getTEOffset(), mat_vert,
							mat_normal, offset, true);

	buffer->flush();

	return true;
}

// Warn about and derender a bogus rigged mesh object
void LLDrawPoolAvatar::riggedFaceError(LLVOAvatar* avatar,
									   const LLVolumeFace& vol_face,
									   LLVOVolume* vobj)
{
	const LLUUID& obj_id = vobj->getID();
	llwarns << "Bad face. mNumVertices = " << vol_face.mNumVertices
			<< " - mNumIndices = " << vol_face.mNumIndices
			<< " - Derendering bogus rigged mesh " << obj_id
			<< " pertaining to: " << avatar->getFullname(true) << llendl;
	gObjectList.sBlackListedObjects.emplace(obj_id);
	gObjectList.killObject(vobj);
}

void LLDrawPoolAvatar::updateRiggedFaceVertexBuffer(LLVOAvatar* avatar,
													LLFace* face,
													LLVOVolume* vobj,
													LLVolume* volume,
													const LLVolumeFace& vol_face)
{
	LLVector4a* weights = vol_face.mWeights;
	if (!weights)	// A rather common occurrence
	{
		return;
	}

	if (vobj->getLOD() == -1)
	{
		return;
	}

	if (vol_face.mNumVertices > 65536 || vol_face.mNumVertices < 0 ||
		vol_face.mNumIndices < 0)
	{
		riggedFaceError(avatar, vol_face, vobj);
		return;
	}

	LLPointer<LLVertexBuffer> buffer = face->getVertexBuffer();
	LLPointer<LLDrawable> drawable = face->getDrawable();

	U32 data_mask = face->getRiggedVertexBufferDataMask();
	const LLMeshSkinInfo* skin = NULL;

	if (buffer.isNull() || buffer->getTypeMask() != data_mask ||
		buffer->getNumVerts() != vol_face.mNumVertices ||
		buffer->getNumIndices() != vol_face.mNumIndices ||
		(drawable && drawable->isState(LLDrawable::REBUILD_ALL)))
	{
		skin = vobj->getSkinInfo();
		// *FIXME: ugly const_cast
		LLSkinningUtil::scrubInvalidJoints(avatar,
										   const_cast<LLMeshSkinInfo*>(skin));
		if (!vol_face.mWeightsScrubbed)
		{
			LLSkinningUtil::scrubSkinWeights(weights, vol_face.mNumVertices,
											 skin);
			vol_face.mWeightsScrubbed = true;
		}

		if (drawable && drawable->isState(LLDrawable::REBUILD_ALL))
		{
			// Rebuild EVERY face in the drawable, not just this one, to avoid
			// missing drawable wide rebuild issues
			for (S32 i = 0, count = drawable->getNumFaces(); i < count; ++i)
			{
				LLFace* facep = drawable->getFace(i);
				if (!facep) continue;	// Paranoia

				U32 face_data_mask = facep->getRiggedVertexBufferDataMask();
				if (face_data_mask == 0) continue;

				LLPointer<LLVertexBuffer> cur_buff = facep->getVertexBuffer();
				const LLVolumeFace& cur_vol_face = volume->getVolumeFace(i);
				if (!getRiggedGeometry(facep, cur_buff, face_data_mask, skin,
									   volume, cur_vol_face))
				{
					riggedFaceError(avatar, cur_vol_face, vobj);
					return; // Bogus object derendered
				}
			}
			drawable->clearState(LLDrawable::REBUILD_ALL);

			buffer = face->getVertexBuffer();
		}
		else if (!getRiggedGeometry(face, buffer, data_mask, skin, volume,
									vol_face))
		{
			riggedFaceError(avatar, vol_face, vobj);
			return; // Bogus object derendered
		}
	}

	if (sShaderLevel > 0 || face->mLastSkinTime > avatar->getLastSkinTime())
	{
		return;
	}

	// Perform software vertex skinning for this face

	LLStrider<LLVector3> position;
	if (!buffer->getVertexStrider(position))
	{
		return;
	}
	LLVector4a* pos = (LLVector4a*)position.get();

	LLStrider<LLVector3> normal;
	bool has_normal = buffer->hasDataType(LLVertexBuffer::TYPE_NORMAL);
	if (has_normal && !buffer->getNormalStrider(normal))
	{
		return;
	}
	LLVector4a* norm = has_normal ? (LLVector4a*)normal.get() : NULL;

	// Build matrix palette
	if (!skin)
	{
		skin = vobj->getSkinInfo();
	}
	U32 count = 0;
	const LLMatrix4a* mat = avatar->getRiggedMatrix4a(skin, count);
	LLSkinningUtil::checkSkinWeights(weights, buffer->getNumVerts(), skin);

	LLMatrix4a bind_shape_matrix;
	bind_shape_matrix.loadu(skin->mBindShapeMatrix);

	count = buffer->getNumVerts();
	LLVector4a t, dst;
	LLMatrix4a final_mat;
	for (U32 j = 0; j < count; ++j)
	{
		final_mat.clear();
		LLSkinningUtil::getPerVertexSkinMatrix(weights[j], mat, final_mat);

		LLVector4a& v = vol_face.mPositions[j];
		bind_shape_matrix.affineTransform(v, t);
		final_mat.affineTransform(t, dst);
		pos[j] = dst;

		if (norm)
		{
			LLVector4a& n = vol_face.mNormals[j];
			bind_shape_matrix.rotate(n, t);
			final_mat.rotate(t, dst);
			dst.normalize3fast();
			norm[j] = dst;
		}
	}
}

void LLDrawPoolAvatar::renderRigged(LLVOAvatar* avatar, U32 type, bool glow)
{
	if (!avatar->shouldRenderRigged() || !gMeshRepo.meshRezEnabled())
	{
		return;
	}

//MK
	bool restricted_vision = gRLenabled && !avatar->isSelf() &&
							 gRLInterface.mVisionRestricted;
	LLVector3 joint_pos;
	if (restricted_vision)
	{
		LLJoint* ref_joint = gRLInterface.getCamDistDrawFromJoint();
		if (ref_joint)
		{
			// Calculate the position of the avatar here so we do not have to
			// do it for each face
			joint_pos = ref_joint->getWorldPosition();
		}
		else
		{
			restricted_vision = false;
		}
	}
	// If our vision is restricted and the outer sphere is opaque, any avatar
	// farther than its radius does not need to be rendered at all.
	if (restricted_vision && isAgentAvatarValid() &&
		gRLInterface.mCamDistDrawAlphaMax >= 0.999f)
	{
		LLVector3d dist =
			gAgent.getPosGlobalFromAgent(avatar->getPositionAgent()) -
			gAgent.getPosGlobalFromAgent(avatar->getPositionAgent());
		if ((F32)dist.lengthSquared() >
				gRLInterface.mCamDistDrawMax * gRLInterface.mCamDistDrawMax)
		{
			return;
		}
	}
//mk

	bool in_mouselook = gAgent.cameraMouselook();
	bool not_debugging_alpha = !LLDrawPoolAlpha::sShowDebugAlpha;
	const LLMeshSkinInfo* last_skin = NULL;
	for (U32 i = 0, count = mRiggedFace[type].size(); i < count; ++i)
	{
		LLFace* face = mRiggedFace[type][i];
		LLDrawable* drawable = face->getDrawable();
		if (!drawable)
		{
			continue;
		}

		LLVOVolume* vobj = drawable->getVOVolume();
		if (!vobj)
		{
			continue;
		}

		LLVolume* volume = vobj->getVolume();
		S32 te_offset = face->getTEOffset();
		if (!volume || volume->getNumVolumeFaces() <= te_offset ||
			!volume->isMeshAssetLoaded())
		{
			continue;
		}

		// Skip when set for not rendering rigged attachments in mouse-look.
		static LLCachedControl<bool> head_render(gSavedSettings,
												 "MouselookRenderRigged");
		if (in_mouselook && !head_render)
		{
			LLSpatialBridge* bridge =
				drawable->isRoot() ? drawable->getSpatialBridge()
								   : drawable->getParent()->getSpatialBridge();
			if (bridge && bridge->mDrawableType == 0)
			{
				continue;
			}
		}

//MK
		if (restricted_vision)
		{
			F32 dist = (face->getPositionAgent() - joint_pos).lengthSquared();
			if (dist > gRLInterface.mCamDistDrawMax *
					   gRLInterface.mCamDistDrawMax)
			{
				// If the vision is restricted, rendering alpha rigged
				// attachments may allow to cheat through the vision spheres.
				U32 type = gPipeline.getPoolTypeFromTE(face->getTextureEntry(),
													   face->getTexture());
				if (type == LLDrawPool::POOL_ALPHA)
				{
					continue;
				}
			}
		}
//mk

#if 0
		const LLVolumeFace& vol_face = volume->getVolumeFace(te);
		updateRiggedFaceVertexBuffer(avatar, face, vobj, volume, vol_face);
#endif

		const LLTextureEntry* te = face->getTextureEntry();
		if (not_debugging_alpha && te && te->getColor().mV[VW] < 0.001f)
		{
			continue;	// Invisible face: skip rendering entirely
		}

		LLMaterial* mat = te ? te->getMaterialParams().get() : NULL;
		if (sShadowPass >= 0)
		{
			bool is_alpha_mask = false;
			bool is_alpha_blend = false;
			if (mat)
			{
				S32 alpha_mode = mat->getDiffuseAlphaMode();
				if (alpha_mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
				{
					is_alpha_mask = true;
				}
				else if (alpha_mode == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND)
				{
					is_alpha_blend = true;
				}
			}
			else
			{
				LLViewerTexture* tex = face->getTexture(LLRender::DIFFUSE_MAP);
				is_alpha_mask = tex && tex->getIsAlphaMask();

				is_alpha_blend = te && te->getAlpha() <= 0.99f;
				if (!is_alpha_blend && !is_alpha_mask && tex)
				{
					GLenum image_format = tex->getPrimaryFormat();
					is_alpha_blend = image_format == GL_RGBA ||
									 image_format == GL_ALPHA;
				}
			}
			if (is_alpha_mask && sSkipTransparent &&
				sShadowPass != SHADOW_PASS_ATTACHMENT_ALPHA_MASK)
			{
				// This is alpha mask contents and we are doing opaques or a
				// non-alpha-mask shadow pass, so skip it.
				continue;
			}
			if (is_alpha_blend &&
				(sSkipTransparent ||
				 sShadowPass != SHADOW_PASS_ATTACHMENT_ALPHA_BLEND))
			{
				// This is alpha mask contents and we are doing opaques or a
				// non-alpha-blend shadow pass, so skip it.
				continue;
			}
			if (!is_alpha_mask && !is_alpha_blend && sSkipOpaque)
			{
				// This is opaque contents and we are skipping opaques...
				continue;
			}
		}

		U32 data_mask = LLFace::getRiggedDataMask(type);

		LLVertexBuffer* buff = face->getVertexBuffer();
		if (buff)
		{
			if (sShaderLevel > 0)
			{
				// Upload matrix palette to shader
				LL_TRACY_TIMER(TRC_FIT_RIGGED);
				U32 count = 0;
				const LLMeshSkinInfo* skin = vobj->getSkinInfo();
				if (!skin)
				{
					continue;
				}
				// Do not upload the same matrix palette more than once
				if (skin != last_skin)
				{
					last_skin = skin;
					const F32* mp = avatar->getRiggedMatrix(skin, count);
					sVertexProgram->uniformMatrix3x4fv(LLShaderMgr::AVATAR_MATRIX,
													   count, false, mp);
				}
			}
			else
			{
				data_mask &= ~LLVertexBuffer::MAP_WEIGHT4;
			}

			U16 start = face->getGeomStart();
			U16 end = start + face->getGeomCount() - 1;
			S32 offset = face->getIndicesStart();
			U32 count = face->getIndicesCount();
#if 0
			if (glow)
			{
				gGL.diffuseColor4f(0.f, 0.f, 0.f,
								   face->getTextureEntry()->getGlow());
			}
#endif
			if (mat && (sIsMatsRender || !LLPipeline::sRenderDeferred))
			{
				LLViewerTexture* diffuse =
					face->getTexture(LLRender::DIFFUSE_MAP);
				LLViewerTexture* specular = NULL;
				LLViewerTexture* normal = NULL;
				if (LLPipeline::sImpostorRender)
				{
					specular = gImgBlackSquare;
				}
				else
				{
					specular = face->getTexture(LLRender::SPECULAR_MAP);
					normal = face->getTexture(LLRender::NORMAL_MAP);
				}
				// Note: order is important here and LLRender::DIFFUSE_MAP must
				// come last because it modifies gGL.mCurrTextureUnitIndex
				if (specular && sSpecularChannel > -1)
				{
					gGL.getTexUnit(sSpecularChannel)->bind(specular);
				}
				if (normal && sNormalChannel > -1)
				{
					gGL.getTexUnit(sNormalChannel)->bind(normal);
				}
				if (diffuse && sDiffuseChannel > -1)
				{
					gGL.getTexUnit(sDiffuseChannel)->bind(diffuse, true);
				}

				LLColor4 col;
				F32 spec, env;
				if (mat->getSpecularID().isNull())
				{
					env = te->getShiny() * 0.25f;
					col.set(env, env, env, 0.f);
					spec = env;
				}
				else
				{
					col = mat->getSpecularLightColor();
					spec = mat->getSpecularLightExponent() * ONE255TH;
					env = mat->getEnvironmentIntensity() * ONE255TH;
				}

				bool fullbright = te->getFullbright();

				sVertexProgram->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS,
										  fullbright ? 1.f : 0.f);

				sVertexProgram->uniform4f(LLShaderMgr::SPECULAR_COLOR,
										  col.mV[0], col.mV[1], col.mV[2],
										  spec);

				sVertexProgram->uniform1f(LLShaderMgr::ENVIRONMENT_INTENSITY,
										  env);

				U8 alpha_mode = mat->getDiffuseAlphaMode();
				if (alpha_mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
				{
					sVertexProgram->setMinimumAlpha(mat->getAlphaMaskCutoff() *
													ONE255TH);
				}
				else
				{
					sVertexProgram->setMinimumAlpha(0.f);
				}

				for (U32 i = 0; i < LLRender::NUM_TEXTURE_CHANNELS; ++i)
				{
					LLViewerTexture* tex = face->getTexture(i);
					if (tex)
					{
						tex->addTextureStats(avatar->getPixelArea());
					}
				}
			}
			else
			{
				if (sDiffuseChannel > -1)
				{
					gGL.getTexUnit(sDiffuseChannel)->bind(face->getTexture());
				}
				sVertexProgram->setMinimumAlpha(0.f);
				if (sNormalChannel > -1)
				{
					LLDrawPoolBump::bindBumpMap(face, sNormalChannel);
				}
			}

			if (face->mTextureMatrix && vobj->mTexAnimMode)
			{
				if (gUseNewShaders)
				{
					U32 tex_index = gGL.getCurrentTexUnitIndex();
					if (tex_index <= 1)
					{
						gGL.matrixMode((U32)LLRender::MM_TEXTURE0 + tex_index);
						gGL.pushMatrix();
						gGL.loadMatrix(face->mTextureMatrix->getF32ptr());
					}
					buff->setBuffer(data_mask);
					buff->drawRange(LLRender::TRIANGLES, start, end, count,
									offset);
					if (tex_index <= 1)
					{
						gGL.matrixMode((U32)LLRender::MM_TEXTURE0 + tex_index);
						gGL.popMatrix();
						gGL.matrixMode(LLRender::MM_MODELVIEW);
					}
				}
				else
				{
					gGL.matrixMode(LLRender::MM_TEXTURE);
					gGL.loadMatrix(face->mTextureMatrix->getF32ptr());
					buff->setBuffer(data_mask);
					buff->drawRange(LLRender::TRIANGLES, start, end, count,
									offset);
					gGL.loadIdentity();
					gGL.matrixMode(LLRender::MM_MODELVIEW);
				}
			}
			else
			{
				buff->setBuffer(data_mask);
				buff->drawRange(LLRender::TRIANGLES, start, end, count,
								offset);
			}

			gPipeline.addTrianglesDrawn(count, LLRender::TRIANGLES);
			stop_glerror();
		}
	}
}

void LLDrawPoolAvatar::renderDeferredRiggedSimple(LLVOAvatar* avatar)
{
	renderRigged(avatar, RIGGED_DEFERRED_SIMPLE);
}

void LLDrawPoolAvatar::renderDeferredRiggedBump(LLVOAvatar* avatar)
{
	renderRigged(avatar, RIGGED_DEFERRED_BUMP);
}

void LLDrawPoolAvatar::renderDeferredRiggedMaterial(LLVOAvatar* avatar,
													S32 pass)
{
	sIsMatsRender = true;
	renderRigged(avatar, pass);
	sIsMatsRender = false;
}

void LLDrawPoolAvatar::updateRiggedVertexBuffers(LLVOAvatar* avatar)
{
	LL_FAST_TIMER(FTM_RIGGED_VBO);

	// Update rigged vertex buffers
	for (U32 type = 0; type < NUM_RIGGED_PASSES; ++type)
	{
		for (U32 i = 0; i < mRiggedFace[type].size(); ++i)
		{
			LLFace* face = mRiggedFace[type][i];
			LLDrawable* drawable = face->getDrawable();
			if (!drawable)
			{
				continue;
			}

			LLVOVolume* vobj = drawable->getVOVolume();
			if (!vobj)
			{
				continue;
			}

			LLVolume* volume = vobj->getVolume();
			S32 te = face->getTEOffset();
			if (!volume || volume->getNumVolumeFaces() <= te)
			{
				continue;
			}

			LLUUID mesh_id = volume->getParams().getSculptID();
			if (mesh_id.isNull())
			{
				continue;
			}

			const LLVolumeFace& vol_face = volume->getVolumeFace(te);
			updateRiggedFaceVertexBuffer(avatar, face, vobj, volume, vol_face);
		}
	}
}

void LLDrawPoolAvatar::renderRiggedSimple(LLVOAvatar* avatar)
{
	renderRigged(avatar, RIGGED_SIMPLE);
}

void LLDrawPoolAvatar::renderRiggedFullbright(LLVOAvatar* avatar)
{
	renderRigged(avatar, RIGGED_FULLBRIGHT);
}

void LLDrawPoolAvatar::renderRiggedShinySimple(LLVOAvatar* avatar)
{
	renderRigged(avatar, RIGGED_SHINY);
}

void LLDrawPoolAvatar::renderRiggedFullbrightShiny(LLVOAvatar* avatar)
{
	renderRigged(avatar, RIGGED_FULLBRIGHT_SHINY);
}

void LLDrawPoolAvatar::renderRiggedAlpha(LLVOAvatar* avatar)
{
	if (!mRiggedFace[RIGGED_ALPHA].empty())
	{
		LLGLEnable blend(GL_BLEND);

		gGL.setColorMask(true, true);
		gGL.blendFunc(LLRender::BF_SOURCE_ALPHA,
					  LLRender::BF_ONE_MINUS_SOURCE_ALPHA,
					  LLRender::BF_ZERO,
					  LLRender::BF_ONE_MINUS_SOURCE_ALPHA);

		renderRigged(avatar, RIGGED_ALPHA);
#if 1
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
#endif
		gGL.setColorMask(true, false);
	}
}

void LLDrawPoolAvatar::renderRiggedFullbrightAlpha(LLVOAvatar* avatar)
{
	if (!mRiggedFace[RIGGED_FULLBRIGHT_ALPHA].empty())
	{
		LLGLEnable blend(GL_BLEND);

		gGL.setColorMask(true, true);
		gGL.blendFunc(LLRender::BF_SOURCE_ALPHA,
					  LLRender::BF_ONE_MINUS_SOURCE_ALPHA,
					  LLRender::BF_ZERO,
					  LLRender::BF_ONE_MINUS_SOURCE_ALPHA);

		renderRigged(avatar, RIGGED_FULLBRIGHT_ALPHA);
#if 1
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
#endif
		gGL.setColorMask(true, false);
	}
}

void LLDrawPoolAvatar::renderRiggedGlow(LLVOAvatar* avatar)
{
	if (!mRiggedFace[RIGGED_GLOW].empty())
	{
		LLGLEnable blend(GL_BLEND);
		LLGLDisable test(GL_ALPHA_TEST);
		gGL.flush();

		LLGLEnable polyOffset(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.f, -1.f);
		gGL.setSceneBlendType(LLRender::BT_ADD);

		LLGLDepthTest depth(GL_TRUE, GL_FALSE);
		gGL.setColorMask(false, true);

		renderRigged(avatar, RIGGED_GLOW, true);

		gGL.setColorMask(true, false);
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
	}
}

bool LLDrawPoolAvatar::addRiggedFace(LLFace* facep, U32 type)
{
	if (!facep) return false;

	if (type >= NUM_RIGGED_PASSES)
	{
		llwarns << "Invalid rigged face type." << llendl;
		llassert(false);
		return false;
	}

	if (facep->getPool() && facep->getPool() != this)
	{
		llwarns << "Tried to add a rigged face that is already in another pool."
				<< llendl;
		llassert(false);
		return false;
	}

	if (facep->getRiggedIndex(type) != -1)
	{
		llwarns << "Tried to add a rigged face that is referenced elsewhere."
				<< llendl;
		llassert(false);
		return false;
	}

	facep->setRiggedIndex(type, mRiggedFace[type].size());
	facep->setPool(this);
	mRiggedFace[type].push_back(facep);

	return true;
}

void LLDrawPoolAvatar::removeRiggedFace(LLFace* facep)
{
	if (!facep) return;

	if (facep->getPool() && facep->getPool() != this)
	{
		llwarns << "Tried to remove a rigged face from the wrong pool."
				<< llendl;
		llassert(false);
		return;
	}

	facep->setPool(NULL);

	for (U32 i = 0; i < NUM_RIGGED_PASSES; ++i)
	{
		S32 index = facep->getRiggedIndex(i);
		if (index > -1)
		{
			if ((S32)mRiggedFace[i].size() > index &&
				mRiggedFace[i][index] == facep)
			{
				facep->setRiggedIndex(i, -1);
				mRiggedFace[i].erase(mRiggedFace[i].begin() + index);
				for (U32 j = index, count = mRiggedFace[i].size(); j < count;
					 ++j)
				{
					// Bump indexes down for faces referenced after erased face
					mRiggedFace[i][j]->setRiggedIndex(i, j);
				}
			}
			else
			{
				llwarns << "Face reference data corrupt for rigged type " << i
						<< ". Ignoring." << llendl;
				llassert(false);
			}
		}
	}
}

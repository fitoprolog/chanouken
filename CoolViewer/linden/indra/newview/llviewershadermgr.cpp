/**
 * @file llviewershadermgr.cpp
 * @brief Viewer shader manager implementation.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llviewershadermgr.h"

#include "lldir.h"
#include "llfeaturemanager.h"
#include "lljoint.h"
#include "llrender.h"
#include "hbtracy.h"

#include "llenvironment.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llvosky.h"
#include "llworld.h"
#include "llwlskyparammgr.h"
#include "llwlwaterparammgr.h"

static LLStaticHashedString sTexture0("texture0");
static LLStaticHashedString sTexture1("texture1");
static LLStaticHashedString sTex0("tex0");
static LLStaticHashedString sGlowMap("glowMap");
static LLStaticHashedString sScreenMap("screenMap");

LLViewerShaderMgr* gViewerShaderMgrp;

bool LLViewerShaderMgr::sInitialized = false;
bool LLViewerShaderMgr::sSkipReload = false;

LLVector4 gShinyOrigin;

// Utility shaders

LLGLSLShader gOcclusionProgram;
LLGLSLShader gOcclusionCubeProgram;
// SL-14113 This used to be used for the stars with Atmospheric shaders: OFF
LLGLSLShader gCustomAlphaProgram;
LLGLSLShader gGlowCombineProgram;
LLGLSLShader gSplatTextureRectProgram;
LLGLSLShader gGlowCombineFXAAProgram;
LLGLSLShader gOneTextureNoColorProgram;
LLGLSLShader gDebugProgram;
LLGLSLShader gClipProgram;
LLGLSLShader gDownsampleDepthProgram;
LLGLSLShader gDownsampleDepthRectProgram;
LLGLSLShader gAlphaMaskProgram;
LLGLSLShader gBenchmarkProgram;

// Object shaders

LLGLSLShader gObjectSimpleProgram;
LLGLSLShader gObjectSimpleImpostorProgram;
LLGLSLShader gObjectPreviewProgram;
LLGLSLShader gObjectSimpleWaterProgram;
LLGLSLShader gObjectSimpleAlphaMaskProgram;
LLGLSLShader gObjectSimpleWaterAlphaMaskProgram;
LLGLSLShader gObjectFullbrightProgram;
LLGLSLShader gObjectFullbrightWaterProgram;
LLGLSLShader gObjectEmissiveProgram;
LLGLSLShader gObjectEmissiveWaterProgram;
LLGLSLShader gObjectFullbrightAlphaMaskProgram;
LLGLSLShader gObjectFullbrightWaterAlphaMaskProgram;
LLGLSLShader gObjectFullbrightShinyProgram;
LLGLSLShader gObjectFullbrightShinyWaterProgram;
LLGLSLShader gObjectShinyProgram;
LLGLSLShader gObjectShinyWaterProgram;
LLGLSLShader gObjectBumpProgram;
LLGLSLShader gTreeProgram;
LLGLSLShader gTreeWaterProgram;
LLGLSLShader gObjectFullbrightNoColorProgram;
LLGLSLShader gObjectFullbrightNoColorWaterProgram;

LLGLSLShader gObjectSimpleNonIndexedProgram;
LLGLSLShader gObjectSimpleNonIndexedTexGenProgram;
LLGLSLShader gObjectSimpleNonIndexedTexGenWaterProgram;
LLGLSLShader gObjectSimpleNonIndexedWaterProgram;
LLGLSLShader gObjectAlphaMaskNonIndexedProgram;
LLGLSLShader gObjectAlphaMaskNonIndexedWaterProgram;
LLGLSLShader gObjectAlphaMaskNoColorProgram;
LLGLSLShader gObjectAlphaMaskNoColorWaterProgram;
LLGLSLShader gObjectFullbrightNonIndexedProgram;
LLGLSLShader gObjectFullbrightNonIndexedWaterProgram;
LLGLSLShader gObjectEmissiveNonIndexedProgram;
LLGLSLShader gObjectEmissiveNonIndexedWaterProgram;
LLGLSLShader gObjectFullbrightShinyNonIndexedProgram;
LLGLSLShader gObjectFullbrightShinyNonIndexedWaterProgram;
LLGLSLShader gObjectShinyNonIndexedProgram;
LLGLSLShader gObjectShinyNonIndexedWaterProgram;

// Attachments skinning shaders

LLGLSLShader gSkinnedObjectSimpleProgram;
LLGLSLShader gSkinnedObjectFullbrightProgram;
LLGLSLShader gSkinnedObjectEmissiveProgram;
LLGLSLShader gSkinnedObjectFullbrightShinyProgram;
LLGLSLShader gSkinnedObjectShinySimpleProgram;

LLGLSLShader gSkinnedObjectSimpleWaterProgram;
LLGLSLShader gSkinnedObjectFullbrightWaterProgram;
LLGLSLShader gSkinnedObjectEmissiveWaterProgram;
LLGLSLShader gSkinnedObjectFullbrightShinyWaterProgram;
LLGLSLShader gSkinnedObjectShinySimpleWaterProgram;

// Environment shaders

LLGLSLShader gMoonProgram;
LLGLSLShader gStarsProgram;
LLGLSLShader gTerrainProgram;
LLGLSLShader gTerrainWaterProgram;
LLGLSLShader gWaterProgram;
LLGLSLShader gUnderWaterProgram;
LLGLSLShader gWaterEdgeProgram;

// Interface shaders

LLGLSLShader gHighlightProgram;
LLGLSLShader gHighlightNormalProgram;
LLGLSLShader gHighlightSpecularProgram;

// Avatar shader handles

LLGLSLShader gAvatarProgram;
LLGLSLShader gAvatarWaterProgram;
LLGLSLShader gAvatarEyeballProgram;
LLGLSLShader gAvatarPickProgram;
LLGLSLShader gImpostorProgram;

// WindLight shader handles

LLGLSLShader gWLSkyProgram;
LLGLSLShader gWLCloudProgram;

LLGLSLShader gWLSunProgram;
LLGLSLShader gWLMoonProgram;

// Effects Shaders

LLGLSLShader gGlowProgram;
LLGLSLShader gGlowExtractProgram;
LLGLSLShader gPostColorFilterProgram;
LLGLSLShader gPostNightVisionProgram;

// Deferred rendering shaders

LLGLSLShader gDeferredImpostorProgram;
LLGLSLShader gDeferredWaterProgram;
LLGLSLShader gDeferredUnderWaterProgram;
LLGLSLShader gDeferredDiffuseProgram;
LLGLSLShader gDeferredDiffuseAlphaMaskProgram;
LLGLSLShader gDeferredNonIndexedDiffuseProgram;
LLGLSLShader gDeferredNonIndexedDiffuseAlphaMaskProgram;
LLGLSLShader gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram;
LLGLSLShader gDeferredSkinnedDiffuseProgram;
LLGLSLShader gDeferredSkinnedBumpProgram;
LLGLSLShader gDeferredSkinnedAlphaProgram;
LLGLSLShader gDeferredBumpProgram;
LLGLSLShader gDeferredTerrainProgram;
LLGLSLShader gDeferredTerrainWaterProgram;
LLGLSLShader gDeferredTreeProgram;
LLGLSLShader gDeferredTreeShadowProgram;
LLGLSLShader gDeferredAvatarProgram;
LLGLSLShader gDeferredAvatarAlphaProgram;
LLGLSLShader gDeferredLightProgram;
LLGLSLShader gDeferredMultiLightProgram[LL_DEFERRED_MULTI_LIGHT_COUNT];
LLGLSLShader gDeferredSpotLightProgram;
LLGLSLShader gDeferredMultiSpotLightProgram;
LLGLSLShader gDeferredSunProgram;
LLGLSLShader gDeferredBlurLightProgram;
LLGLSLShader gDeferredSoftenProgram;
LLGLSLShader gDeferredSoftenWaterProgram;
LLGLSLShader gDeferredShadowProgram;
LLGLSLShader gDeferredShadowCubeProgram;
LLGLSLShader gDeferredShadowAlphaMaskProgram;
LLGLSLShader gDeferredShadowFullbrightAlphaMaskProgram;
LLGLSLShader gDeferredAvatarShadowProgram;
LLGLSLShader gDeferredAvatarAlphaShadowProgram;
LLGLSLShader gDeferredAvatarAlphaMaskShadowProgram;
LLGLSLShader gDeferredAttachmentShadowProgram;
LLGLSLShader gDeferredAttachmentAlphaShadowProgram;
LLGLSLShader gDeferredAttachmentAlphaMaskShadowProgram;
LLGLSLShader gDeferredAlphaProgram;
LLGLSLShader gDeferredAlphaImpostorProgram;
LLGLSLShader gDeferredAlphaWaterProgram;
LLGLSLShader gDeferredAvatarEyesProgram;
LLGLSLShader gDeferredFullbrightProgram;
LLGLSLShader gDeferredFullbrightAlphaMaskProgram;
LLGLSLShader gDeferredFullbrightWaterProgram;
LLGLSLShader gDeferredFullbrightAlphaMaskWaterProgram;
LLGLSLShader gDeferredEmissiveProgram;
LLGLSLShader gDeferredPostProgram;
LLGLSLShader gDeferredCoFProgram;
LLGLSLShader gDeferredDoFCombineProgram;
LLGLSLShader gDeferredPostGammaCorrectProgram;
LLGLSLShader gFXAAProgram;
LLGLSLShader gDeferredPostNoDoFProgram;
LLGLSLShader gDeferredWLSkyProgram;
LLGLSLShader gDeferredWLCloudProgram;
LLGLSLShader gDeferredWLSunProgram;
LLGLSLShader gDeferredWLMoonProgram;
LLGLSLShader gDeferredStarProgram;
LLGLSLShader gDeferredFullbrightShinyProgram;
LLGLSLShader gDeferredSkinnedFullbrightShinyProgram;
LLGLSLShader gDeferredSkinnedFullbrightProgram;
LLGLSLShader gNormalMapGenProgram;

// Deferred materials shaders

LLGLSLShader gDeferredMaterialProgram[LLMaterial::SHADER_COUNT * 2];
LLGLSLShader gDeferredMaterialWaterProgram[LLMaterial::SHADER_COUNT * 2];

LLViewerShaderMgr::LLViewerShaderMgr()
:	mShaderLevel(SHADER_COUNT, 0),
	mMaxAvatarShaderLevel(0)
{
	sInitialized = true;
	init();
}

LLViewerShaderMgr::~LLViewerShaderMgr()
{
	mShaderLevel.clear();
	mShaderList.clear();
}

void LLViewerShaderMgr::init()
{
	sVertexShaderObjects.clear();
	sFragmentShaderObjects.clear();

	std::string subdir = gUseNewShaders ? "ee" : "wl";
	mShaderDirPrefix = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
													  "shaders", subdir,
													  "class");
	mShaderList.clear();

	if (!gGLManager.mHasRequirements)
	{
		llwarns << "Failed to pass minimum requirements for out shaders."
				<< llendl;
		sInitialized = false;
		return;
	}

	// Make sure WL Sky is the first program. ONLY shaders that need WL param
	// management should be added here.
	mShaderList.push_back(&gWLSkyProgram);
	mShaderList.push_back(&gWLCloudProgram);
	if (gUseNewShaders)
	{
		mShaderList.push_back(&gWLSunProgram);
		mShaderList.push_back(&gWLMoonProgram);
	}
	mShaderList.push_back(&gAvatarProgram);
	mShaderList.push_back(&gObjectShinyProgram);
	mShaderList.push_back(&gObjectShinyNonIndexedProgram);
	mShaderList.push_back(&gWaterProgram);
	if (gUseNewShaders)
	{
		mShaderList.push_back(&gWaterEdgeProgram);
	}
	mShaderList.push_back(&gAvatarEyeballProgram);
	mShaderList.push_back(&gObjectSimpleProgram);
	mShaderList.push_back(&gObjectSimpleImpostorProgram);
	mShaderList.push_back(&gObjectPreviewProgram);
	mShaderList.push_back(&gImpostorProgram);
	mShaderList.push_back(&gObjectFullbrightNoColorProgram);
	mShaderList.push_back(&gObjectFullbrightNoColorWaterProgram);
	mShaderList.push_back(&gObjectSimpleAlphaMaskProgram);
	mShaderList.push_back(&gObjectBumpProgram);
	mShaderList.push_back(&gObjectEmissiveProgram);
	mShaderList.push_back(&gObjectEmissiveWaterProgram);
	mShaderList.push_back(&gObjectFullbrightProgram);
	mShaderList.push_back(&gObjectFullbrightAlphaMaskProgram);
	mShaderList.push_back(&gObjectFullbrightShinyProgram);
	mShaderList.push_back(&gObjectFullbrightShinyWaterProgram);
	mShaderList.push_back(&gObjectSimpleNonIndexedProgram);
	mShaderList.push_back(&gObjectSimpleNonIndexedTexGenProgram);
	mShaderList.push_back(&gObjectSimpleNonIndexedTexGenWaterProgram);
	mShaderList.push_back(&gObjectSimpleNonIndexedWaterProgram);
	mShaderList.push_back(&gObjectAlphaMaskNonIndexedProgram);
	mShaderList.push_back(&gObjectAlphaMaskNonIndexedWaterProgram);
	mShaderList.push_back(&gObjectAlphaMaskNoColorProgram);
	mShaderList.push_back(&gObjectAlphaMaskNoColorWaterProgram);
	mShaderList.push_back(&gTreeProgram);
	mShaderList.push_back(&gTreeWaterProgram);
	mShaderList.push_back(&gObjectFullbrightNonIndexedProgram);
	mShaderList.push_back(&gObjectFullbrightNonIndexedWaterProgram);
	mShaderList.push_back(&gObjectEmissiveNonIndexedProgram);
	mShaderList.push_back(&gObjectEmissiveNonIndexedWaterProgram);
	mShaderList.push_back(&gObjectFullbrightShinyNonIndexedProgram);
	mShaderList.push_back(&gObjectFullbrightShinyNonIndexedWaterProgram);
	mShaderList.push_back(&gSkinnedObjectSimpleProgram);
	mShaderList.push_back(&gSkinnedObjectFullbrightProgram);
	mShaderList.push_back(&gSkinnedObjectEmissiveProgram);
	mShaderList.push_back(&gSkinnedObjectFullbrightShinyProgram);
	mShaderList.push_back(&gSkinnedObjectShinySimpleProgram);
	mShaderList.push_back(&gSkinnedObjectSimpleWaterProgram);
	mShaderList.push_back(&gSkinnedObjectFullbrightWaterProgram);
	mShaderList.push_back(&gSkinnedObjectEmissiveWaterProgram);
	mShaderList.push_back(&gSkinnedObjectFullbrightShinyWaterProgram);
	mShaderList.push_back(&gSkinnedObjectShinySimpleWaterProgram);
	mShaderList.push_back(&gTerrainProgram);
	mShaderList.push_back(&gTerrainWaterProgram);
	mShaderList.push_back(&gObjectSimpleWaterProgram);
	mShaderList.push_back(&gObjectFullbrightWaterProgram);
	mShaderList.push_back(&gObjectSimpleWaterAlphaMaskProgram);
	mShaderList.push_back(&gObjectFullbrightWaterAlphaMaskProgram);
	mShaderList.push_back(&gAvatarWaterProgram);
	mShaderList.push_back(&gObjectShinyWaterProgram);
	mShaderList.push_back(&gObjectShinyNonIndexedWaterProgram);
	mShaderList.push_back(&gUnderWaterProgram);
	mShaderList.push_back(&gDeferredSunProgram);
	mShaderList.push_back(&gDeferredSoftenProgram);
	mShaderList.push_back(&gDeferredSoftenWaterProgram);
	if (!gUseNewShaders)
	{
		mShaderList.push_back(&gDeferredMaterialProgram[1]);
		mShaderList.push_back(&gDeferredMaterialProgram[5]);
		mShaderList.push_back(&gDeferredMaterialProgram[9]);
		mShaderList.push_back(&gDeferredMaterialProgram[13]);
		mShaderList.push_back(&gDeferredMaterialProgram[1 + LLMaterial::SHADER_COUNT]);
		mShaderList.push_back(&gDeferredMaterialProgram[5 + LLMaterial::SHADER_COUNT]);
		mShaderList.push_back(&gDeferredMaterialProgram[9 + LLMaterial::SHADER_COUNT]);
		mShaderList.push_back(&gDeferredMaterialProgram[13 + LLMaterial::SHADER_COUNT]);
		mShaderList.push_back(&gDeferredMaterialWaterProgram[1]);
		mShaderList.push_back(&gDeferredMaterialWaterProgram[5]);
		mShaderList.push_back(&gDeferredMaterialWaterProgram[9]);
		mShaderList.push_back(&gDeferredMaterialWaterProgram[13]);
		mShaderList.push_back(&gDeferredMaterialWaterProgram[1 + LLMaterial::SHADER_COUNT]);
		mShaderList.push_back(&gDeferredMaterialWaterProgram[5 + LLMaterial::SHADER_COUNT]);
		mShaderList.push_back(&gDeferredMaterialWaterProgram[9 + LLMaterial::SHADER_COUNT]);
		mShaderList.push_back(&gDeferredMaterialWaterProgram[13 + LLMaterial::SHADER_COUNT]);
	}
	mShaderList.push_back(&gDeferredAlphaProgram);
	mShaderList.push_back(&gDeferredAlphaImpostorProgram);
	mShaderList.push_back(&gDeferredAlphaWaterProgram);
	mShaderList.push_back(&gDeferredSkinnedAlphaProgram);
	mShaderList.push_back(&gDeferredFullbrightProgram);
	mShaderList.push_back(&gDeferredFullbrightAlphaMaskProgram);
	mShaderList.push_back(&gDeferredFullbrightWaterProgram);
	mShaderList.push_back(&gDeferredFullbrightAlphaMaskWaterProgram);
	mShaderList.push_back(&gDeferredFullbrightShinyProgram);
	mShaderList.push_back(&gDeferredSkinnedFullbrightShinyProgram);
	mShaderList.push_back(&gDeferredSkinnedFullbrightProgram);
	mShaderList.push_back(&gDeferredEmissiveProgram);
	mShaderList.push_back(&gDeferredAvatarEyesProgram);
	mShaderList.push_back(&gDeferredWaterProgram);
	mShaderList.push_back(&gDeferredUnderWaterProgram);
	if (gUseNewShaders)
	{
		mShaderList.push_back(&gDeferredTerrainWaterProgram);
	}
	mShaderList.push_back(&gDeferredAvatarAlphaProgram);
	mShaderList.push_back(&gDeferredWLSkyProgram);
	mShaderList.push_back(&gDeferredWLCloudProgram);
	if (gUseNewShaders)
	{
		mShaderList.push_back(&gDeferredWLMoonProgram);
		mShaderList.push_back(&gDeferredWLSunProgram);
	}
}

//static
void LLViewerShaderMgr::createInstance()
{
	if (gViewerShaderMgrp)
	{
		llwarns << "Instance already exists !" << llendl;
		llassert(false);
		return;
	}
	gViewerShaderMgrp = new LLViewerShaderMgr();
}

//static
void LLViewerShaderMgr::releaseInstance()
{
	sInitialized = false;
	if (gViewerShaderMgrp)
	{
		delete gViewerShaderMgrp;
		gViewerShaderMgrp = NULL;
	}
}

void LLViewerShaderMgr::initAttribsAndUniforms()
{
	static bool last_use_new_shaders = false;
	if (sReservedUniforms.empty() || last_use_new_shaders != gUseNewShaders)
	{
		last_use_new_shaders = gUseNewShaders;
		LLShaderMgr::initAttribsAndUniforms();
	}
}

// Shader Management
void LLViewerShaderMgr::setShaders()
{
	// We get called recursively by gSavedSettings callbacks, so return on
	// reentrance
	static bool reentrance = false;
	if (reentrance)
	{
		// Always refresh cached settings however
		gPipeline.refreshCachedSettings();
		return;
	}
	if (!gPipeline.mInitialized || !sInitialized || sSkipReload)
	{
		return;
	}
	reentrance = true;

	if (gGLManager.mGLSLVersionMajor == 1 &&
		gGLManager.mGLSLVersionMinor <= 20)
	{
		// NEVER use indexed texture rendering when GLSL version is 1.20 or
		// earlier
		LLGLSLShader::sIndexedTextureChannels = 1;
	}
	else
	{
		// NEVER use more than 16 texture channels (work around for prevalent
		// driver bug)
		S32 max_tex_idx =
			llclamp((S32)gSavedSettings.getU32("RenderMaxTextureIndex"),
					1, 16);
		if (max_tex_idx > gGLManager.mNumTextureImageUnits)
		{
			max_tex_idx = gGLManager.mNumTextureImageUnits;
		}
		LLGLSLShader::sIndexedTextureChannels = max_tex_idx;
	}

	// Make sure the compiled shader maps are cleared before we recompile
	// shaders, set the shaders directory prefix depending on legacy or new
	// rendering, and reinitialise the shaders list.
	init();

	initAttribsAndUniforms();
	gPipeline.releaseGLBuffers();

	LLPipeline::sWaterReflections = gGLManager.mHasCubeMap;
	LLPipeline::updateRenderDeferred();

	// *HACK: to reset buffers that change behavior with shaders
	gPipeline.resetVertexBuffers();

	if (gViewerWindowp)
	{
		gViewerWindowp->setCursor(UI_CURSOR_WAIT);
	}

	// Lighting
	gPipeline.setLightingDetail(-1);

	// Shaders
	llinfos << "\n~~~~~~~~~~~~~~~~~~\n Loading Shaders:\n~~~~~~~~~~~~~~~~~~"
			<< llendl;
	llinfos << llformat("Using GLSL %d.%d", gGLManager.mGLSLVersionMajor,
						gGLManager.mGLSLVersionMinor) << llendl;

	for (S32 i = 0; i < SHADER_COUNT; ++i)
	{
		mShaderLevel[i] = 0;
	}
	mMaxAvatarShaderLevel = 0;

	LLVertexBuffer::unbind();

	// GL_ARB_depth_clamp was so far always disabled because of an issue with
	// projectors... Let's use it when available depending on a debug setting,
	// to see how it fares...
	gGLManager.mUseDepthClamp = gGLManager.mHasDepthClamp &&
								gSavedSettings.getBool("RenderUseDepthClamp");
	if (!gGLManager.mHasDepthClamp)
	{
		llinfos << "Missing feature GL_ARB_depth_clamp. Void water might disappear in rare cases."
				<< llendl;
	}
	else if (gGLManager.mUseDepthClamp)
	{
		llinfos << "Depth clamping usage is enabled for shaders, which may possibly cause issues with projectors. Change RenderDepthClampShadows and/or RenderUseDepthClamp to FALSE (in this order of preference) if you wish to disable it, and please report successful combination(s) of those settings on the Cool VL Viewer support forum."
				<< llendl;
	}

	bool use_windlight =
		gSavedSettings.getBool("WindLightUseAtmosShaders") &&
		gFeatureManager.isFeatureAvailable("WindLightUseAtmosShaders");

	bool use_deferred = use_windlight &&
		gFeatureManager.isFeatureAvailable("RenderDeferred") &&
		gSavedSettings.getBool("RenderDeferred") &&
		gSavedSettings.getBool("RenderAvatarVP");

	S32 light_class, interface_class, env_class, obj_class, effect_class,
		wl_class, water_class, deferred_class;

	if (gUseNewShaders)
	{
		interface_class = env_class = obj_class = effect_class =
						  water_class = 2;

		if (!use_deferred)
		{
			deferred_class = 0;
		}
		else if (gSavedSettings.getS32("RenderShadowDetail") > 0)
		{
			// Shadows on
			deferred_class = 2;
		}
		else
		{
			// No shadows
			deferred_class = 1;
		}

		if (use_windlight)
		{
			wl_class = 2;
			light_class = 3;
		}
		else
		{
			wl_class = 1;
			light_class = 2;
		}
	}
	else
	{
		light_class = interface_class = env_class = obj_class = effect_class =
					  wl_class = water_class = 2;
		deferred_class = 0;
		if (use_deferred && gSavedSettings.getBool("WindLightUseAtmosShaders"))
		{
			if (gSavedSettings.getS32("RenderShadowDetail") > 0)
			{
				// Shadows on
				deferred_class = 2;
			}
			else
			{
				// No shadows
				deferred_class = 1;
			}
		}

		if (!use_windlight)
		{
			// User has disabled WindLight in their settings, downgrade
			// windlight shaders to stub versions.
			wl_class = 1;
		}
	}

	// Trigger a full rebuild of the fallback skybox / cubemap if we have
	// toggled windlight shaders
	if (!wl_class || mShaderLevel[SHADER_WINDLIGHT] != wl_class)
	{
		if (gSky.mVOSkyp.notNull())
		{
			gSky.mVOSkyp->forceSkyUpdate();
		}
	}

	// Load lighting shaders
	mShaderLevel[SHADER_LIGHTING] = light_class;
	mShaderLevel[SHADER_INTERFACE] = interface_class;
	mShaderLevel[SHADER_ENVIRONMENT] = env_class;
	mShaderLevel[SHADER_WATER] = water_class;
	mShaderLevel[SHADER_OBJECT] = obj_class;
	mShaderLevel[SHADER_EFFECT] = effect_class;
	mShaderLevel[SHADER_WINDLIGHT] = wl_class;
	mShaderLevel[SHADER_DEFERRED] = deferred_class;

	bool loaded = loadBasicShaders();
	if (!loaded)
	{
		sInitialized = false;
		reentrance = false;
		mShaderLevel[SHADER_LIGHTING] = 0;
		mShaderLevel[SHADER_INTERFACE] = 0;
		mShaderLevel[SHADER_ENVIRONMENT] = 0;
		mShaderLevel[SHADER_WATER] = 0;
		mShaderLevel[SHADER_OBJECT] = 0;
		mShaderLevel[SHADER_EFFECT] = 0;
		mShaderLevel[SHADER_WINDLIGHT] = 0;
		mShaderLevel[SHADER_DEFERRED] = 0;
		gPipeline.mVertexShadersLoaded = -1;
		llwarns << "Failed to load the basic shaders !" << llendl;
		return;
	}
	gPipeline.mVertexShadersLoaded = 1;

	// Load all shaders to set max levels
	loaded = loadShadersEnvironment();
	if (loaded)
	{
		loaded = loadShadersWater();
	}

	if (loaded)
	{
		loaded = loadShadersWindLight();
	}

	if (loaded)
	{
		loaded = loadShadersEffects();
	}

	if (loaded)
	{
		loaded = loadShadersInterface();
	}

	if (loaded)
	{
		// Load max avatar shaders to set the max level
		mShaderLevel[SHADER_AVATAR] = 3;
		mMaxAvatarShaderLevel = 3;

		if (gSavedSettings.getBool("RenderAvatarVP") && loadShadersObject())
		{
			// Skinning shader is enabled and rigged attachment shaders loaded
			// correctly
			bool avatar_cloth = gSavedSettings.getBool("RenderAvatarCloth");
			// Cloth is a class3 shader
			S32 avatar_class = avatar_cloth ? 3 : 1;

			// Set the actual level
			mShaderLevel[SHADER_AVATAR] = avatar_class;

			loaded = loadShadersAvatar();

			if (mShaderLevel[SHADER_AVATAR] != avatar_class)
			{
				if (mShaderLevel[SHADER_AVATAR] == 0)
				{
					gSavedSettings.setBool("RenderAvatarVP", false);
				}
				if (llmax(mShaderLevel[SHADER_AVATAR] - 1, 0) >= 3)
				{
					avatar_cloth = true;
				}
				else
				{
					avatar_cloth = false;
				}
				gSavedSettings.setBool("RenderAvatarCloth", avatar_cloth);
			}
		}
		else
		{
			// Skinning shader not possible, neither is deferred rendering
			mShaderLevel[SHADER_AVATAR] = 0;
			mShaderLevel[SHADER_DEFERRED] = 0;

			if (gSavedSettings.getBool("RenderAvatarVP"))
			{
				gSavedSettings.setBool("RenderDeferred", false);
				gSavedSettings.setBool("RenderAvatarCloth", false);
				gSavedSettings.setBool("RenderAvatarVP", false);
			}

			loadShadersAvatar(); // unloads

			loaded = loadShadersObject();
		}
	}

	// Some shader absolutely could not load, try to fall back to a simpler
	// setting
	if (!loaded && gSavedSettings.getBool("WindLightUseAtmosShaders"))
	{
		// Disable windlight and try again
		gSavedSettings.setBool("WindLightUseAtmosShaders", false);
		reentrance = false;
		setShaders();
		return;
	}

	if (loaded && !loadShadersDeferred())
	{
		// Everything else succeeded but deferred failed, disable deferred and
		// try again
		gSavedSettings.setBool("RenderDeferred", false);
		reentrance = false;
		setShaders();
		return;
	}

	if (gViewerWindowp)
	{
		gViewerWindowp->setCursor(UI_CURSOR_ARROW);
	}

	gPipeline.refreshCachedSettings();
	gPipeline.createGLBuffers();

	reentrance = false;
}

void LLViewerShaderMgr::unloadShaders()
{
	gBenchmarkProgram.unload();

	gOcclusionProgram.unload();
	gOcclusionCubeProgram.unload();
	gDebugProgram.unload();
	gClipProgram.unload();
	gDownsampleDepthProgram.unload();
	gDownsampleDepthRectProgram.unload();
	gAlphaMaskProgram.unload();
	gUIProgram.unload();
	gCustomAlphaProgram.unload();
	gGlowCombineProgram.unload();
	gSplatTextureRectProgram.unload();
	gGlowCombineFXAAProgram.unload();
	gOneTextureNoColorProgram.unload();
	gSolidColorProgram.unload();

	gObjectFullbrightNoColorProgram.unload();
	gObjectFullbrightNoColorWaterProgram.unload();
	gObjectSimpleProgram.unload();
	gObjectSimpleImpostorProgram.unload();
	gObjectPreviewProgram.unload();
	gImpostorProgram.unload();
	gObjectSimpleAlphaMaskProgram.unload();
	gObjectBumpProgram.unload();
	gObjectSimpleWaterProgram.unload();
	gObjectSimpleWaterAlphaMaskProgram.unload();
	gObjectFullbrightProgram.unload();
	gObjectFullbrightWaterProgram.unload();
	gObjectEmissiveProgram.unload();
	gObjectEmissiveWaterProgram.unload();
	gObjectFullbrightAlphaMaskProgram.unload();
	gObjectFullbrightWaterAlphaMaskProgram.unload();

	gObjectShinyProgram.unload();
	gObjectFullbrightShinyProgram.unload();
	gObjectFullbrightShinyWaterProgram.unload();
	gObjectShinyWaterProgram.unload();

	gObjectSimpleNonIndexedProgram.unload();
	gObjectSimpleNonIndexedTexGenProgram.unload();
	gObjectSimpleNonIndexedTexGenWaterProgram.unload();
	gObjectSimpleNonIndexedWaterProgram.unload();
	gObjectAlphaMaskNonIndexedProgram.unload();
	gObjectAlphaMaskNonIndexedWaterProgram.unload();
	gObjectAlphaMaskNoColorProgram.unload();
	gObjectAlphaMaskNoColorWaterProgram.unload();
	gObjectFullbrightNonIndexedProgram.unload();
	gObjectFullbrightNonIndexedWaterProgram.unload();
	gObjectEmissiveNonIndexedProgram.unload();
	gObjectEmissiveNonIndexedWaterProgram.unload();
	gTreeProgram.unload();
	gTreeWaterProgram.unload();

	gObjectShinyNonIndexedProgram.unload();
	gObjectFullbrightShinyNonIndexedProgram.unload();
	gObjectFullbrightShinyNonIndexedWaterProgram.unload();
	gObjectShinyNonIndexedWaterProgram.unload();

	gSkinnedObjectSimpleProgram.unload();
	gSkinnedObjectFullbrightProgram.unload();
	gSkinnedObjectEmissiveProgram.unload();
	gSkinnedObjectFullbrightShinyProgram.unload();
	gSkinnedObjectShinySimpleProgram.unload();

	gSkinnedObjectSimpleWaterProgram.unload();
	gSkinnedObjectFullbrightWaterProgram.unload();
	gSkinnedObjectEmissiveWaterProgram.unload();
	gSkinnedObjectFullbrightShinyWaterProgram.unload();
	gSkinnedObjectShinySimpleWaterProgram.unload();

	gWaterProgram.unload();
	gWaterEdgeProgram.unload();
	gUnderWaterProgram.unload();
	gMoonProgram.unload();
	gStarsProgram.unload();
	gTerrainProgram.unload();
	gTerrainWaterProgram.unload();
	gGlowProgram.unload();
	gGlowExtractProgram.unload();
	gAvatarProgram.unload();
	gAvatarWaterProgram.unload();
	gAvatarEyeballProgram.unload();
	gAvatarPickProgram.unload();
	gHighlightProgram.unload();

	gWLSkyProgram.unload();
	gWLCloudProgram.unload();
	gWLSunProgram.unload();
	gWLMoonProgram.unload();

	gPostColorFilterProgram.unload();
	gPostNightVisionProgram.unload();

	gDeferredDiffuseProgram.unload();
	gDeferredDiffuseAlphaMaskProgram.unload();
	gDeferredNonIndexedDiffuseAlphaMaskProgram.unload();
	gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.unload();
	gDeferredNonIndexedDiffuseProgram.unload();
	gDeferredSkinnedDiffuseProgram.unload();
	gDeferredSkinnedBumpProgram.unload();
	gDeferredSkinnedAlphaProgram.unload();

	for (U32 i = 0; i < LL_DEFERRED_MULTI_LIGHT_COUNT; ++i)
	{
		gDeferredMultiLightProgram[i].unload();
	}

	for (S32 i = 0; i < LLMaterial::SHADER_COUNT * 2; ++i)
	{
		gDeferredMaterialProgram[i].unload();
		gDeferredMaterialWaterProgram[i].unload();
	}

	mShaderLevel[SHADER_LIGHTING] = 0;
	mShaderLevel[SHADER_OBJECT] = 0;
	mShaderLevel[SHADER_AVATAR] = 0;
	mShaderLevel[SHADER_ENVIRONMENT] = 0;
	mShaderLevel[SHADER_WATER] = 0;
	mShaderLevel[SHADER_INTERFACE] = 0;
	mShaderLevel[SHADER_EFFECT] = 0;
	mShaderLevel[SHADER_WINDLIGHT] = 0;
}

// Loads basic dependency shaders first. All of these have to load for any
// shaders to function.
bool LLViewerShaderMgr::loadBasicShaders()
{
	S32 sum_lights_class = 3;
	// Class one cards will get the lower sum lights. Class zero we are not
	// going to think about since a class zero card COULD be a ridiculous new
	// card and old cards should have the features masked
	if (gFeatureManager.getGPUClass() == GPU_CLASS_1)
	{
		sum_lights_class = 2;
	}
	// If we have sun and moon only checked, then only sum those lights.
	if (gPipeline.getLightingDetail() == 0)
	{
		sum_lights_class = 1;
	}
#if LL_DARWIN
	// Work around driver crashes on older Macs when using deferred rendering
	// NORSPEC-59
	if (gGLManager.mIsMobileGF)
	{
		sum_lights_class = 3;
	}
#endif
	// Use the feature table to mask out the max light level to use. Also make
	// sure it is at least 1.
	sum_lights_class =
		llclamp(sum_lights_class, 1,
				(S32)gSavedSettings.getS32("RenderShaderLightingMaxLevel"));

	// Load the Basic Vertex Shaders at the appropriate level (in order of
	// shader function call depth for reference purposes, deepest level first).

	S32 wl_level = mShaderLevel[SHADER_WINDLIGHT];
	S32 light_level = mShaderLevel[SHADER_LIGHTING];

	std::vector<std::pair<std::string, S32> > shaders;
	shaders.emplace_back("windlight/atmosphericsVarsV.glsl", wl_level);
	shaders.emplace_back("windlight/atmosphericsVarsWaterV.glsl", wl_level);
	shaders.emplace_back("windlight/atmosphericsHelpersV.glsl", wl_level);
	shaders.emplace_back("lighting/lightFuncV.glsl", light_level);
	shaders.emplace_back("lighting/sumLightsV.glsl", sum_lights_class);
	shaders.emplace_back("lighting/lightV.glsl", light_level);
	shaders.emplace_back("lighting/lightFuncSpecularV.glsl", light_level);
	shaders.emplace_back("lighting/sumLightsSpecularV.glsl", sum_lights_class);
	shaders.emplace_back("lighting/lightSpecularV.glsl", light_level);
	if (gUseNewShaders)
	{
		shaders.emplace_back("windlight/atmosphericsFuncs.glsl", wl_level);
	}
	shaders.emplace_back("windlight/atmosphericsV.glsl", wl_level);
	shaders.emplace_back("avatar/avatarSkinV.glsl", 1);
	shaders.emplace_back("avatar/objectSkinV.glsl", 1);
	if (gGLManager.mGLSLVersionMajor >= 2 ||
		gGLManager.mGLSLVersionMinor >= 30)
	{
		shaders.emplace_back("objects/indexedTextureV.glsl", 1);
	}
	shaders.emplace_back("objects/nonindexedTextureV.glsl", 1);

	LLGLSLShader::defines_map_t attribs;
	attribs["MAX_JOINTS_PER_MESH_OBJECT"] =
		llformat("%d", LL_MAX_JOINTS_PER_MESH_OBJECT);

	if (gUseNewShaders)
	{
		if (gSavedSettings.getBool("RenderDisableAmbient"))
		{
			attribs["AMBIENT_KILL"] = "1";
		}
		if (gSavedSettings.getBool("RenderDisableSunlight"))
		{
			attribs["SUNLIGHT_KILL"] = "1";
		}
		if (gSavedSettings.getBool("RenderDisableLocalLight"))
		{
			attribs["LOCAL_LIGHT_KILL"] = "1";
		}
	}

	stop_glerror();

	// We no longer have to bind the shaders to global GLhandles, they are
	// automatically added to a map now.
	for (U32 i = 0, count = shaders.size(); i < count; ++i)
	{
		// Note usage of GL_VERTEX_SHADER_ARB
		if (loadShaderFile(shaders[i].first, shaders[i].second,
						   GL_VERTEX_SHADER_ARB, &attribs) == 0)
		{
			return false;
		}
	}

	// Load the Basic Fragment Shaders at the appropriate level (in order of
	// shader function call depth for reference purposes, deepest level first).

	shaders.clear();

	S32 ch = 1;
	if (gGLManager.mGLSLVersionMajor > 1 || gGLManager.mGLSLVersionMinor >= 30)
	{
		// Use indexed texture rendering for GLSL >= 1.30
		ch = llmax(LLGLSLShader::sIndexedTextureChannels - 1, 1);
	}

	std::vector<S32> index_channels;
	index_channels.push_back(-1);
	shaders.emplace_back("windlight/atmosphericsVarsF.glsl", wl_level);
	index_channels.push_back(-1);
	shaders.emplace_back("windlight/atmosphericsVarsWaterF.glsl", wl_level);
	if (gUseNewShaders)
	{
		index_channels.push_back(-1);
		shaders.emplace_back("windlight/atmosphericsHelpersF.glsl",
							 wl_level);
	}
	index_channels.push_back(-1);
	shaders.emplace_back("windlight/gammaF.glsl", wl_level);
	if (gUseNewShaders)
	{
		index_channels.push_back(-1);
		shaders.emplace_back("windlight/atmosphericsFuncs.glsl", wl_level);
	}
	index_channels.push_back(-1);
	shaders.emplace_back("windlight/atmosphericsF.glsl", wl_level);
	index_channels.push_back(-1);
	shaders.emplace_back("windlight/transportF.glsl", wl_level);
	index_channels.push_back(-1);
	shaders.emplace_back("environment/waterFogF.glsl",
						 mShaderLevel[SHADER_WATER]);
	if (gUseNewShaders)
	{
		S32 env_level = mShaderLevel[SHADER_ENVIRONMENT];
		index_channels.push_back(-1);
		shaders.emplace_back("environment/encodeNormF.glsl", env_level);
		index_channels.push_back(-1);
		shaders.emplace_back("environment/srgbF.glsl", env_level);
		index_channels.push_back(-1);
		shaders.emplace_back("deferred/deferredUtil.glsl", 1);
		index_channels.push_back(-1);
		shaders.emplace_back("deferred/shadowUtil.glsl", 1);
		index_channels.push_back(-1);
		shaders.emplace_back("deferred/aoUtil.glsl", 1);
	}
	index_channels.push_back(-1);
	shaders.emplace_back("lighting/lightNonIndexedF.glsl", light_level);
	index_channels.push_back(-1);
	shaders.emplace_back("lighting/lightAlphaMaskNonIndexedF.glsl",
						 light_level);
	index_channels.push_back(-1);
	shaders.emplace_back("lighting/lightFullbrightNonIndexedF.glsl",
						 light_level);
	index_channels.push_back(-1);
	shaders.emplace_back("lighting/lightFullbrightNonIndexedAlphaMaskF.glsl",
						 light_level);
	index_channels.push_back(-1);
	shaders.emplace_back("lighting/lightWaterNonIndexedF.glsl", light_level);
	index_channels.push_back(-1);
	shaders.emplace_back("lighting/lightWaterAlphaMaskNonIndexedF.glsl",
						 light_level);
	index_channels.push_back(-1);
	shaders.emplace_back("lighting/lightFullbrightWaterNonIndexedF.glsl",
						 light_level);
	index_channels.push_back(-1);
	shaders.emplace_back("lighting/lightFullbrightWaterNonIndexedAlphaMaskF.glsl",
						 light_level);
	index_channels.push_back(-1);
	shaders.emplace_back("lighting/lightShinyNonIndexedF.glsl", light_level);
	index_channels.push_back(-1);
	shaders.emplace_back("lighting/lightFullbrightShinyNonIndexedF.glsl",
						 light_level);
	index_channels.push_back(-1);
	shaders.emplace_back("lighting/lightShinyWaterNonIndexedF.glsl",
						 light_level);
	index_channels.push_back(-1);
	shaders.emplace_back("lighting/lightFullbrightShinyWaterNonIndexedF.glsl",
						 light_level);
	index_channels.push_back(ch);
	shaders.emplace_back("lighting/lightF.glsl", light_level);
	index_channels.push_back(ch);
	shaders.emplace_back("lighting/lightAlphaMaskF.glsl", light_level);
	index_channels.push_back(ch);
	shaders.emplace_back("lighting/lightFullbrightF.glsl", light_level);
	index_channels.push_back(ch);
	shaders.emplace_back("lighting/lightFullbrightAlphaMaskF.glsl",
						 light_level);
	index_channels.push_back(ch);
	shaders.emplace_back("lighting/lightWaterF.glsl", light_level);
	index_channels.push_back(ch);
	shaders.emplace_back("lighting/lightWaterAlphaMaskF.glsl", light_level);
	index_channels.push_back(ch);
	shaders.emplace_back("lighting/lightFullbrightWaterF.glsl", light_level);
	index_channels.push_back(ch);
	shaders.emplace_back("lighting/lightFullbrightWaterAlphaMaskF.glsl",
						 light_level);
	index_channels.push_back(ch);
	shaders.emplace_back("lighting/lightShinyF.glsl", light_level);
	index_channels.push_back(ch);
	shaders.emplace_back("lighting/lightFullbrightShinyF.glsl", light_level);
	index_channels.push_back(ch);
	shaders.emplace_back("lighting/lightShinyWaterF.glsl", light_level);
	index_channels.push_back(ch);
	shaders.emplace_back("lighting/lightFullbrightShinyWaterF.glsl",
						 light_level);

	for (U32 i = 0; i < shaders.size(); ++i)
	{
		// Note usage of GL_FRAGMENT_SHADER_ARB
		if (loadShaderFile(shaders[i].first, shaders[i].second,
						   GL_FRAGMENT_SHADER_ARB, &attribs,
						   index_channels[i]) == 0)
		{
			return false;
		}
	}

	llinfos << "Basic shaders loaded." << llendl;

	return true;
}

bool LLViewerShaderMgr::loadShadersEnvironment()
{
	S32 shader_level = mShaderLevel[SHADER_ENVIRONMENT];

	if (shader_level == 0)
	{
		gTerrainProgram.unload();
		return true;
	}

	gTerrainProgram.setup("Terrain shader", shader_level,
						  "environment/terrainV.glsl",
						  "environment/terrainF.glsl");
	gTerrainProgram.mFeatures.mIndexedTextureChannels = 0;
	gTerrainProgram.mFeatures.calculatesLighting = true;
	gTerrainProgram.mFeatures.calculatesAtmospherics = true;
	gTerrainProgram.mFeatures.hasAtmospherics = true;
	gTerrainProgram.mFeatures.disableTextureIndex = true;
	gTerrainProgram.mFeatures.hasGamma = true;
	if (gUseNewShaders)
	{
		gTerrainProgram.mFeatures.hasTransport = true;
		gTerrainProgram.mFeatures.hasSrgb = true;
	}
	bool success = gTerrainProgram.createShader();

	if (success && gUseNewShaders)
	{
		gStarsProgram.setup("Environment stars shader", shader_level,
							"environment/starsV.glsl",
							"environment/starsF.glsl");
		gStarsProgram.addConstant(LLGLSLShader::CONST_STAR_DEPTH);
		success = gStarsProgram.createShader();
	}

	if (success && gUseNewShaders)
	{
		gMoonProgram.setup("Environment Moon shader", shader_level,
						   "environment/moonV.glsl", "environment/moonF.glsl");
		gMoonProgram.addConstant(LLGLSLShader::CONST_CLOUD_MOON_DEPTH);
		success = gMoonProgram.createShader();
		if (success)
		{
			gMoonProgram.bind();
			gMoonProgram.uniform1i(sTex0, 0);
		}
	}

	if (success)
	{
		gWorld.updateWaterObjects();
		llinfos << "Environment shaders loaded at level: " << shader_level
				<< llendl;
		return true;
	}

	mShaderLevel[SHADER_ENVIRONMENT] = 0;
	return false;
}

bool LLViewerShaderMgr::loadShadersWater()
{
	S32 shader_level = mShaderLevel[SHADER_WATER];

	if (shader_level == 0)
	{
		gWaterProgram.unload();
		gWaterEdgeProgram.unload();
		gUnderWaterProgram.unload();
		gTerrainWaterProgram.unload();
		return true;
	}

	// Load water shader
	gWaterProgram.setup("Water shader", shader_level,
						"environment/waterV.glsl",
						"environment/waterF.glsl");
	gWaterProgram.mFeatures.calculatesAtmospherics = true;
	gWaterProgram.mFeatures.hasGamma = true;
	gWaterProgram.mFeatures.hasTransport = true;
	if (gUseNewShaders)
	{
		gWaterProgram.mFeatures.hasSrgb = true;
		gWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
	}
	bool success = gWaterProgram.createShader();

	if (success && gUseNewShaders)
	{
		// Load under water edge shader
		gWaterEdgeProgram.setup("Water edge shader", shader_level,
								"environment/waterV.glsl",
								"environment/waterF.glsl");
		gWaterEdgeProgram.mFeatures.calculatesAtmospherics = true;
		gWaterEdgeProgram.mFeatures.hasGamma = true;
		gWaterEdgeProgram.mFeatures.hasTransport = true;
		gWaterEdgeProgram.mFeatures.hasSrgb = true;
		gWaterEdgeProgram.addPermutation("WATER_EDGE", "1");
		gWaterEdgeProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gWaterEdgeProgram.createShader();
	}

	if (success)
	{
		// Load under water vertex shader
		gUnderWaterProgram.setup("Underwater shader", shader_level,
								 "environment/waterV.glsl",
								 "environment/underWaterF.glsl");
		gUnderWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		gUnderWaterProgram.mFeatures.calculatesAtmospherics = true;
		if (gUseNewShaders)
		{
			gUnderWaterProgram.mFeatures.hasWaterFog = true;
		}
		success = gUnderWaterProgram.createShader();
	}

	bool terrain_water_success = true;
	if (success)
	{
		// Load terrain water shader
		gTerrainWaterProgram.setup("Terrain water shader",
								   mShaderLevel[SHADER_ENVIRONMENT],
								   gUseNewShaders ?
										"environment/terrainWaterV.glsl" :
										"environment/terrainV.glsl",
								   "environment/terrainWaterF.glsl");
		gTerrainWaterProgram.mFeatures.calculatesLighting = true;
		gTerrainWaterProgram.mFeatures.calculatesAtmospherics = true;
		gTerrainWaterProgram.mFeatures.hasAtmospherics = true;
		gTerrainWaterProgram.mFeatures.hasWaterFog = true;
		gTerrainWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gTerrainWaterProgram.mFeatures.disableTextureIndex = true;
		gTerrainWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		if (gUseNewShaders && LLPipeline::sRenderDeferred)
		{
			gTerrainWaterProgram.addPermutation("ALM", "1");
		}
		terrain_water_success = gTerrainWaterProgram.createShader();
	}

	// Keep track of water shader levels
	if (gWaterProgram.mShaderLevel != shader_level ||
		gUnderWaterProgram.mShaderLevel != shader_level)
	{
		mShaderLevel[SHADER_WATER] =
			llmin(gWaterProgram.mShaderLevel, gUnderWaterProgram.mShaderLevel);
	}

	if (!success)
	{
		mShaderLevel[SHADER_WATER] = 0;
		return false;
	}

	// If we failed to load the terrain water shaders and we need them (using
	// class2 water), then drop down to class1 water.
	if (mShaderLevel[SHADER_WATER] > 1 && !terrain_water_success)
	{
		--mShaderLevel[SHADER_WATER];
		return loadShadersWater();
	}

	gWorld.updateWaterObjects();

	llinfos << "Water shaders loaded at level: " << mShaderLevel[SHADER_WATER]
			<< llendl;

	return true;
}

bool LLViewerShaderMgr::loadShadersEffects()
{
	S32 shader_level = mShaderLevel[SHADER_EFFECT];

	if (shader_level == 0)
	{
		gGlowProgram.unload();
		gGlowExtractProgram.unload();
		gPostColorFilterProgram.unload();
		gPostNightVisionProgram.unload();
		return true;
	}

	gGlowProgram.setup("Glow shader (post)", shader_level,
					   "effects/glowV.glsl", "effects/glowF.glsl");
	bool success = gGlowProgram.createShader();
	if (success)
	{
		gGlowExtractProgram.setup("Glow extract shader (post)", shader_level,
								  "effects/glowExtractV.glsl",
								  "effects/glowExtractF.glsl");
		success = gGlowExtractProgram.createShader();
	}

	LLPipeline::sCanRenderGlow = success;

	if (success)
	{
		llinfos << "Effects shaders loaded at level: " << shader_level
				<< llendl;
	}

	return success;
}

bool LLViewerShaderMgr::loadShadersDeferred()
{
	S32 shader_level = mShaderLevel[SHADER_DEFERRED];
	bool use_sun_shadow = shader_level > 1;

	bool ambient_kill = false;
	bool local_light_kill = false;
	bool sunlight_kill = false;
	if (gUseNewShaders)
	{
		ambient_kill = gSavedSettings.getBool("RenderDisableAmbient");
		local_light_kill = gSavedSettings.getBool("RenderDisableLocalLight");
		sunlight_kill = gSavedSettings.getBool("RenderDisableSunlight");
	}

	if (shader_level == 0)
	{
		gDeferredTreeProgram.unload();
		gDeferredTreeShadowProgram.unload();
		gDeferredDiffuseProgram.unload();
		gDeferredDiffuseAlphaMaskProgram.unload();
		gDeferredNonIndexedDiffuseAlphaMaskProgram.unload();
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.unload();
		gDeferredNonIndexedDiffuseProgram.unload();
		gDeferredSkinnedDiffuseProgram.unload();
		gDeferredSkinnedBumpProgram.unload();
		gDeferredSkinnedAlphaProgram.unload();
		gDeferredBumpProgram.unload();
		gDeferredImpostorProgram.unload();
		gDeferredTerrainProgram.unload();
		gDeferredTerrainWaterProgram.unload();
		gDeferredLightProgram.unload();
		for (U32 i = 0; i < LL_DEFERRED_MULTI_LIGHT_COUNT; ++i)
		{
			gDeferredMultiLightProgram[i].unload();
		}
		gDeferredSpotLightProgram.unload();
		gDeferredMultiSpotLightProgram.unload();
		gDeferredSunProgram.unload();
		gDeferredBlurLightProgram.unload();
		gDeferredSoftenProgram.unload();
		gDeferredSoftenWaterProgram.unload();
		gDeferredShadowProgram.unload();
		gDeferredShadowCubeProgram.unload();
		gDeferredShadowAlphaMaskProgram.unload();
		gDeferredShadowFullbrightAlphaMaskProgram.unload();
		gDeferredAvatarShadowProgram.unload();
		gDeferredAvatarAlphaShadowProgram.unload();
		gDeferredAvatarAlphaMaskShadowProgram.unload();
		gDeferredAttachmentShadowProgram.unload();
		gDeferredAttachmentAlphaShadowProgram.unload();
		gDeferredAttachmentAlphaMaskShadowProgram.unload();
		gDeferredAvatarProgram.unload();
		gDeferredAvatarAlphaProgram.unload();
		gDeferredAlphaProgram.unload();
		gDeferredAlphaWaterProgram.unload();
		gDeferredFullbrightProgram.unload();
		gDeferredFullbrightAlphaMaskProgram.unload();
		gDeferredFullbrightWaterProgram.unload();
		gDeferredFullbrightAlphaMaskWaterProgram.unload();
		gDeferredEmissiveProgram.unload();
		gDeferredAvatarEyesProgram.unload();
		gDeferredPostProgram.unload();
		gDeferredCoFProgram.unload();
		gDeferredDoFCombineProgram.unload();
		gDeferredPostGammaCorrectProgram.unload();
		gFXAAProgram.unload();
		gDeferredWaterProgram.unload();
		gDeferredUnderWaterProgram.unload();
		gDeferredWLSkyProgram.unload();
		gDeferredWLCloudProgram.unload();
		gDeferredWLSunProgram.unload();
		gDeferredWLMoonProgram.unload();
		gDeferredStarProgram.unload();
		gDeferredFullbrightShinyProgram.unload();
		gDeferredSkinnedFullbrightShinyProgram.unload();
		gDeferredSkinnedFullbrightProgram.unload();
		gNormalMapGenProgram.unload();
		for (U32 i = 0; i < LLMaterial::SHADER_COUNT * 2; ++i)
		{
			gDeferredMaterialProgram[i].unload();
			gDeferredMaterialWaterProgram[i].unload();
		}
		return true;
	}

	gDeferredDiffuseProgram.setup("Deferred diffuse shader", shader_level,
								  "deferred/diffuseV.glsl",
								  "deferred/diffuseIndexedF.glsl");
	gDeferredDiffuseProgram.mFeatures.mIndexedTextureChannels =
		LLGLSLShader::sIndexedTextureChannels;
	if (gUseNewShaders)
	{
		gDeferredDiffuseProgram.mFeatures.encodesNormal = true;
		gDeferredDiffuseProgram.mFeatures.hasSrgb = true;
	}
	bool success = gDeferredDiffuseProgram.createShader();

	if (success)
	{
		gDeferredDiffuseAlphaMaskProgram.setup("Deferred diffuse alpha mask shader",
											   shader_level,
											   "deferred/diffuseV.glsl",
											   "deferred/diffuseAlphaMaskIndexedF.glsl");
		gDeferredDiffuseAlphaMaskProgram.mFeatures.mIndexedTextureChannels =
			LLGLSLShader::sIndexedTextureChannels;
		if (gUseNewShaders)
		{
			gDeferredDiffuseAlphaMaskProgram.mFeatures.encodesNormal = true;
		}
		success = gDeferredDiffuseAlphaMaskProgram.createShader();
	}

	if (success)
	{
		gDeferredNonIndexedDiffuseAlphaMaskProgram.setup("Deferred diffuse non-indexed alpha mask shader",
														 shader_level,
														 "deferred/diffuseV.glsl",
														 "deferred/diffuseAlphaMaskF.glsl");
		if (gUseNewShaders)
		{
			gDeferredNonIndexedDiffuseAlphaMaskProgram.mFeatures.encodesNormal = true;
		}
		success = gDeferredNonIndexedDiffuseAlphaMaskProgram.createShader();
	}

	if (success)
	{
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.setup("Deferred diffuse non-indexed alpha mask shader",
																shader_level,
																"deferred/diffuseNoColorV.glsl",
																"deferred/diffuseAlphaMaskNoColorF.glsl");
		if (gUseNewShaders)
		{
			gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mFeatures.encodesNormal = true;
		}
		success =
			gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.createShader();
	}

	if (success)
	{
		gDeferredNonIndexedDiffuseProgram.setup("Non indexed deferred diffuse shader",
												shader_level,
												"deferred/diffuseV.glsl",
												"deferred/diffuseF.glsl");
		if (gUseNewShaders)
		{
			gDeferredNonIndexedDiffuseProgram.mFeatures.encodesNormal = true;
			gDeferredNonIndexedDiffuseProgram.mFeatures.hasSrgb = true;
		}
		success = gDeferredNonIndexedDiffuseProgram.createShader();
	}

	if (success)
	{
		gDeferredSkinnedDiffuseProgram.setup("Deferred skinned diffuse shader",
											 shader_level,
											 "deferred/diffuseSkinnedV.glsl",
											 "deferred/diffuseF.glsl");
		gDeferredSkinnedDiffuseProgram.mFeatures.hasObjectSkinning = true;
		if (gUseNewShaders)
		{
			gDeferredSkinnedDiffuseProgram.mFeatures.encodesNormal = true;
			gDeferredSkinnedDiffuseProgram.mFeatures.hasSrgb = true;
		}
		success = gDeferredSkinnedDiffuseProgram.createShader();
	}

	if (success)
	{
		gDeferredSkinnedBumpProgram.setup("Deferred skinned bump shader",
										  shader_level,
										  "deferred/bumpSkinnedV.glsl",
										  "deferred/bumpF.glsl");
		gDeferredSkinnedBumpProgram.mFeatures.hasObjectSkinning = true;
		if (gUseNewShaders)
		{
			gDeferredSkinnedBumpProgram.mFeatures.encodesNormal = true;
		}
		success = gDeferredSkinnedBumpProgram.createShader();
	}

	std::string has_shadows = use_sun_shadow ? "1" : "0";

	if (success)
	{
		gDeferredSkinnedAlphaProgram.setup("Deferred skinned alpha shader",
										   shader_level,
										   "deferred/alphaV.glsl",
										   "deferred/alphaF.glsl");
		gDeferredSkinnedAlphaProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedAlphaProgram.mFeatures.isAlphaLighting = true;
		gDeferredSkinnedAlphaProgram.mFeatures.disableTextureIndex = true;
		gDeferredSkinnedAlphaProgram.mFeatures.hasShadows = true;
		if (gUseNewShaders)
		{
			gDeferredSkinnedAlphaProgram.mFeatures.hasSrgb = true;
			gDeferredSkinnedAlphaProgram.mFeatures.encodesNormal = true;
			gDeferredSkinnedAlphaProgram.mFeatures.calculatesAtmospherics = true;
			gDeferredSkinnedAlphaProgram.mFeatures.hasAtmospherics = true;
			gDeferredSkinnedAlphaProgram.mFeatures.hasTransport = true;
			gDeferredSkinnedAlphaProgram.mFeatures.hasGamma = true;
		}
		gDeferredSkinnedAlphaProgram.addPermutation("USE_DIFFUSE_TEX", "1");
		gDeferredSkinnedAlphaProgram.addPermutation("HAS_SKIN", "1");
		gDeferredSkinnedAlphaProgram.addPermutation("USE_VERTEX_COLOR", "1");
		if (gUseNewShaders)
		{
			if (use_sun_shadow)
			{
				gDeferredSkinnedAlphaProgram.addPermutation("HAS_SHADOW", "1");
			}
			if (ambient_kill)
			{
				gDeferredSkinnedAlphaProgram.addPermutation("AMBIENT_KILL", "1");
			}
			if (sunlight_kill)
			{
				gDeferredSkinnedAlphaProgram.addPermutation("SUNLIGHT_KILL",
															"1");
			}
			if (local_light_kill)
			{
				gDeferredSkinnedAlphaProgram.addPermutation("LOCAL_LIGHT_KILL",
															"1");
			}
		}
		else
		{
			gDeferredSkinnedAlphaProgram.addPermutation("HAS_SHADOW",
														has_shadows);
		}
		success = gDeferredSkinnedAlphaProgram.createShader();
		// *HACK: to include uniforms for lighting without linking in lighting
		// file
		gDeferredSkinnedAlphaProgram.mFeatures.calculatesLighting = true;
		gDeferredSkinnedAlphaProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredBumpProgram.setup("Deferred bump shader", shader_level,
								   "deferred/bumpV.glsl",
								   "deferred/bumpF.glsl");
		if (gUseNewShaders)
		{
			gDeferredBumpProgram.mFeatures.encodesNormal = true;
		}
		success = gDeferredBumpProgram.createShader();
	}

	std::string name;
	for (U32 i = 0; success && i < LLMaterial::SHADER_COUNT * 2; ++i)
	{
		U32 alpha_mode = i & 0x3;
		std::string alpha_mode_str = llformat("%d", alpha_mode);
		bool has_specular_map = i & 0x4;
		bool has_normal_map = i & 0x8;
		bool has_skin = i & 0x10;

		LLGLSLShader* shader = &gDeferredMaterialProgram[i];
		name = llformat("Deferred material shader %d", i);
		shader->setup(name.c_str(), shader_level, "deferred/materialV.glsl",
					  "deferred/materialF.glsl");
		shader->addPermutation("DIFFUSE_ALPHA_MODE", alpha_mode_str);
		shader->mFeatures.hasShadows = use_sun_shadow;
		if (gUseNewShaders)
		{
			if (use_sun_shadow)
			{
				shader->addPermutation("HAS_SUN_SHADOW", "1");
			}
			if (has_normal_map)
			{
				shader->addPermutation("HAS_NORMAL_MAP", "1");
			}
			if (has_specular_map)
			{
				shader->addPermutation("HAS_SPECULAR_MAP", "1");
			}
			if (has_skin)
			{
				shader->addPermutation("HAS_SKIN", "1");
			}
			if (ambient_kill)
			{
				shader->addPermutation("AMBIENT_KILL", "1");
			}
			if (sunlight_kill)
			{
				shader->addPermutation("SUNLIGHT_KILL", "1");
			}
			if (local_light_kill)
			{
				shader->addPermutation("LOCAL_LIGHT_KILL", "1");
			}
			shader->mFeatures.hasSrgb = true;
			shader->mFeatures.encodesNormal = true;
			if (alpha_mode == 1)
			{
				shader->mFeatures.hasTransport = true;
				shader->mFeatures.calculatesAtmospherics = true;
				shader->mFeatures.hasAtmospherics = true;
				shader->mFeatures.hasGamma = true;
			}
		}
		else
		{
			shader->addPermutation("HAS_SUN_SHADOW", has_shadows);
			shader->addPermutation("HAS_NORMAL_MAP",
								   has_normal_map ? "1" : "0");
			shader->addPermutation("HAS_SPECULAR_MAP",
								   has_specular_map ? "1" : "0");
			shader->addPermutation("HAS_SKIN", has_skin ? "1" : "0");
		}

		if (has_skin)
		{
			shader->mFeatures.hasObjectSkinning = true;
		}

		success = shader->createShader();
		if (!success) break;
		if (gUseNewShaders)
		{
			mShaderList.push_back(shader);
		}

		shader = &gDeferredMaterialWaterProgram[i];
		name = llformat("Deferred underwater material shader %d", i);
		shader->setup(name.c_str(), shader_level, "deferred/materialV.glsl",
					  "deferred/materialF.glsl");
		shader->mShaderGroup = LLGLSLShader::SG_WATER;
		shader->addPermutation("WATER_FOG", "1");
		shader->addPermutation("DIFFUSE_ALPHA_MODE", alpha_mode_str);
		shader->mFeatures.hasShadows = use_sun_shadow;
		if (gUseNewShaders)
		{
			if (use_sun_shadow)
			{
				shader->addPermutation("HAS_SUN_SHADOW", "1");
			}
			if (has_normal_map)
			{
				shader->addPermutation("HAS_NORMAL_MAP", "1");
			}
			if (has_specular_map)
			{
				shader->addPermutation("HAS_SPECULAR_MAP", "1");
			}
			if (has_skin)
			{
				shader->addPermutation("HAS_SKIN", "1");
			}
			if (ambient_kill)
			{
				shader->addPermutation("AMBIENT_KILL", "1");
			}
			if (sunlight_kill)
			{
				shader->addPermutation("SUNLIGHT_KILL", "1");
			}
			if (local_light_kill)
			{
				shader->addPermutation("LOCAL_LIGHT_KILL", "1");
			}
			shader->mFeatures.hasSrgb = true;
			shader->mFeatures.encodesNormal = true;
			if (alpha_mode == 1)
			{
				shader->mFeatures.hasWaterFog = true;
				shader->mFeatures.calculatesAtmospherics = true;
				shader->mFeatures.hasAtmospherics = true;
				shader->mFeatures.hasGamma = true;
				shader->mFeatures.hasTransport = true;
			}
		}
		else
		{
			shader->addPermutation("HAS_SUN_SHADOW",
								   use_sun_shadow ? "1" : "0");
			shader->addPermutation("HAS_NORMAL_MAP",
								   has_normal_map ? "1" : "0");
			shader->addPermutation("HAS_SPECULAR_MAP",
								   has_specular_map ? "1" : "0");
			shader->addPermutation("HAS_SKIN", has_skin ? "1" : "0");
		}

		if (has_skin)
		{
			shader->mFeatures.hasObjectSkinning = true;
		}

		success = shader->createShader();
		if (success && gUseNewShaders)
		{
			 mShaderList.push_back(shader);
		}
	}

	gDeferredMaterialProgram[1].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[5].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[9].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[13].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[1 + LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[5 + LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[9 + LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[13 + LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;

	gDeferredMaterialWaterProgram[1].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[5].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[9].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[13].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[1 + LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[5 + LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[9 + LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[13 + LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;

	if (success)
	{
		gDeferredTreeProgram.setup("Deferred tree shader", shader_level,
								   "deferred/treeV.glsl",
								   "deferred/treeF.glsl");
		if (gUseNewShaders)
		{
			gDeferredTreeProgram.mFeatures.encodesNormal = true;
		}
		success = gDeferredTreeProgram.createShader();
	}

	if (success)
	{
		gDeferredTreeShadowProgram.setup("Deferred tree shadow shader",
										 shader_level,
										 "deferred/treeShadowV.glsl",
										 "deferred/treeShadowF.glsl");
		gDeferredTreeShadowProgram.mFeatures.hasShadows = true;
		if (gUseNewShaders)
		{
			gDeferredTreeShadowProgram.mFeatures.isDeferred = true;
		}
		success = gDeferredTreeShadowProgram.createShader();
	}

	if (success)
	{
		gDeferredImpostorProgram.setup("Deferred impostor shader",
									   shader_level,
									   "deferred/impostorV.glsl",
									   "deferred/impostorF.glsl");
		if (gUseNewShaders)
		{
			gDeferredImpostorProgram.mFeatures.hasSrgb = true;
			gDeferredImpostorProgram.mFeatures.encodesNormal = true;
		}
		success = gDeferredImpostorProgram.createShader();
	}

	if (success)
	{
		gDeferredLightProgram.setup("Deferred light shader",
									shader_level,
									"deferred/pointLightV.glsl",
									"deferred/pointLightF.glsl");
		gDeferredLightProgram.mFeatures.hasShadows = true;
		if (gUseNewShaders)
		{
			gDeferredLightProgram.mFeatures.isDeferred = true;
			gDeferredLightProgram.mFeatures.hasSrgb = true;
			if (ambient_kill)
			{
				gDeferredLightProgram.addPermutation("AMBIENT_KILL", "1");
			}
			if (sunlight_kill)
			{
				gDeferredLightProgram.addPermutation("SUNLIGHT_KILL", "1");
			}
			if (local_light_kill)
			{
				gDeferredLightProgram.addPermutation("LOCAL_LIGHT_KILL", "1");
			}
		}
		success = gDeferredLightProgram.createShader();
	}

	for (U32 i = 0; success && i < LL_DEFERRED_MULTI_LIGHT_COUNT; ++i)
	{
		LLGLSLShader* shader = &gDeferredMultiLightProgram[i];
		name = llformat("Deferred multilight shader %d", i);
		shader->setup(name.c_str(), shader_level,
					  "deferred/multiPointLightV.glsl",
					  "deferred/multiPointLightF.glsl");
		shader->addPermutation("LIGHT_COUNT", llformat("%d", i + 1));
		shader->mFeatures.hasShadows = true;
		if (gUseNewShaders)
		{
			shader->mFeatures.isDeferred = true;
			shader->mFeatures.hasSrgb = true;
			if (ambient_kill)
			{
				shader->addPermutation("AMBIENT_KILL", "1");
			}
			if (sunlight_kill)
			{
				shader->addPermutation("SUNLIGHT_KILL", "1");
			}
			if (local_light_kill)
			{
				shader->addPermutation("LOCAL_LIGHT_KILL", "1");
			}
		}
		success = shader->createShader();
	}

	if (success)
	{
		gDeferredSpotLightProgram.setup("Deferred spotlight shader",
										shader_level,
										"deferred/pointLightV.glsl",
										"deferred/spotLightF.glsl");
		gDeferredSpotLightProgram.mFeatures.hasShadows = true;
		if (gUseNewShaders)
		{
			gDeferredSpotLightProgram.mFeatures.hasSrgb = true;
			gDeferredSpotLightProgram.mFeatures.isDeferred = true;
			if (ambient_kill)
			{
				gDeferredSpotLightProgram.addPermutation("AMBIENT_KILL", "1");
			}
			if (sunlight_kill)
			{
				gDeferredSpotLightProgram.addPermutation("SUNLIGHT_KILL", "1");
			}
			if (local_light_kill)
			{
				gDeferredSpotLightProgram.addPermutation("LOCAL_LIGHT_KILL",
														 "1");
			}
		}
		success = gDeferredSpotLightProgram.createShader();
	}

	if (success)
	{
		gDeferredMultiSpotLightProgram.setup("Deferred multispotlight shader",
											 shader_level,
											 "deferred/multiPointLightV.glsl",
											 "deferred/multiSpotLightF.glsl");
		gDeferredMultiSpotLightProgram.mFeatures.hasShadows = true;
		if (gUseNewShaders)
		{
			gDeferredMultiSpotLightProgram.mFeatures.hasSrgb = true;
			gDeferredMultiSpotLightProgram.mFeatures.isDeferred = true;
			if (local_light_kill)
			{
				gDeferredMultiSpotLightProgram.addPermutation("LOCAL_LIGHT_KILL",
															  "1");
			}
		}
		success = gDeferredMultiSpotLightProgram.createShader();
	}

	bool use_ao = gSavedSettings.getBool("RenderDeferredSSAO");

	if (success)
	{
		const char* fragment;
		const char* vertex = "deferred/sunLightV.glsl";
		if (use_ao)
		{
			fragment = "deferred/sunLightSSAOF.glsl";
		}
		else
		{
			fragment = "deferred/sunLightF.glsl";
			if (shader_level == 1)
			{
				// No shadows, no SSAO, no frag coord
				vertex = "deferred/sunLightNoFragCoordV.glsl";
			}
		}
		gDeferredSunProgram.setup("Deferred Sun shader", shader_level,
								  vertex, fragment);
		gDeferredSunProgram.mFeatures.hasAmbientOcclusion = use_ao;
		gDeferredSunProgram.mFeatures.hasShadows = true;
		if (gUseNewShaders)
		{
			gDeferredSunProgram.mFeatures.isDeferred = true;
		}
		success = gDeferredSunProgram.createShader();
	}

	if (success)
	{
		gDeferredBlurLightProgram.setup("Deferred blur light shader",
										shader_level,
										"deferred/blurLightV.glsl",
										"deferred/blurLightF.glsl");
		if (gUseNewShaders)
		{
			gDeferredBlurLightProgram.mFeatures.isDeferred = true;
		}
		success = gDeferredBlurLightProgram.createShader();
	}

	if (success)
	{
		gDeferredAlphaProgram.setup("Deferred alpha shader", shader_level,
									"deferred/alphaV.glsl",
									"deferred/alphaF.glsl");
		if (shader_level < 1)
		{
			gDeferredAlphaProgram.mFeatures.mIndexedTextureChannels =
				LLGLSLShader::sIndexedTextureChannels;
		}
		else
		{
			// Shave off some texture units for shadow maps
			gDeferredAlphaProgram.mFeatures.mIndexedTextureChannels =
				llmax(LLGLSLShader::sIndexedTextureChannels - 6, 1);
		}
		gDeferredAlphaProgram.mFeatures.isAlphaLighting = true;
		// *HACK: to disable auto-setup of texture channels
		gDeferredAlphaProgram.mFeatures.disableTextureIndex = true;
		gDeferredAlphaProgram.mFeatures.hasShadows = use_sun_shadow;
		if (gUseNewShaders)
		{
			gDeferredAlphaProgram.mFeatures.hasSrgb = true;
			gDeferredAlphaProgram.mFeatures.encodesNormal = true;
			gDeferredAlphaProgram.mFeatures.calculatesAtmospherics = true;
			gDeferredAlphaProgram.mFeatures.hasAtmospherics = true;
			gDeferredAlphaProgram.mFeatures.hasGamma = true;
			gDeferredAlphaProgram.mFeatures.hasTransport = true;
			if (use_sun_shadow)
			{
				gDeferredAlphaProgram.addPermutation("HAS_SHADOW", "1");
			}
			if (ambient_kill)
			{
				gDeferredAlphaProgram.addPermutation("AMBIENT_KILL", "1");
			}
			if (sunlight_kill)
			{
				gDeferredAlphaProgram.addPermutation("SUNLIGHT_KILL", "1");
			}
			if (local_light_kill)
			{
				gDeferredAlphaProgram.addPermutation("LOCAL_LIGHT_KILL", "1");
			}
		}
		else
		{
			gDeferredAlphaProgram.addPermutation("HAS_SHADOW", has_shadows);
		}
		gDeferredAlphaProgram.addPermutation("USE_INDEXED_TEX", "1");
		gDeferredAlphaProgram.addPermutation("USE_VERTEX_COLOR", "1");
		success = gDeferredAlphaProgram.createShader();
		// *HACK: set after creation to disable auto-setup of texture channels
		gDeferredAlphaProgram.mFeatures.calculatesLighting = true;
		gDeferredAlphaProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredAlphaImpostorProgram.setup("Deferred alpha impostor shader",
											shader_level,
											"deferred/alphaV.glsl",
											"deferred/alphaF.glsl");
		if (shader_level < 1)
		{
			gDeferredAlphaImpostorProgram.mFeatures.mIndexedTextureChannels =
				LLGLSLShader::sIndexedTextureChannels;
		}
		else
		{
			// Shave off some texture units for shadow maps
			gDeferredAlphaImpostorProgram.mFeatures.mIndexedTextureChannels =
				llmax(LLGLSLShader::sIndexedTextureChannels - 6, 1);
		}
		gDeferredAlphaImpostorProgram.mFeatures.isAlphaLighting = true;
		gDeferredAlphaImpostorProgram.mFeatures.hasShadows = use_sun_shadow;
		if (gUseNewShaders)
		{
			gDeferredAlphaImpostorProgram.mFeatures.hasSrgb = true;
			gDeferredAlphaImpostorProgram.mFeatures.encodesNormal = true;
			if (use_sun_shadow)
			{
				gDeferredAlphaImpostorProgram.addPermutation("HAS_SHADOW", "1");
			}
		}
		else
		{
			gDeferredAlphaImpostorProgram.mFeatures.disableTextureIndex = true;
			gDeferredAlphaImpostorProgram.addPermutation("HAS_SHADOW",
														 has_shadows);
		}
		gDeferredAlphaImpostorProgram.addPermutation("USE_INDEXED_TEX", "1");
		gDeferredAlphaImpostorProgram.addPermutation("USE_VERTEX_COLOR", "1");
		gDeferredAlphaImpostorProgram.addPermutation("FOR_IMPOSTOR", "1");
		success = gDeferredAlphaImpostorProgram.createShader();
		// *HACK: set after creation to disable auto-setup of texture channels
		gDeferredAlphaImpostorProgram.mFeatures.calculatesLighting = true;
		gDeferredAlphaImpostorProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredAlphaWaterProgram.setup("Deferred alpha underwater shader",
										 shader_level,
										 "deferred/alphaV.glsl",
										 "deferred/alphaF.glsl");
		gDeferredAlphaWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		if (shader_level < 1)
		{
			gDeferredAlphaWaterProgram.mFeatures.mIndexedTextureChannels =
				LLGLSLShader::sIndexedTextureChannels;
		}
		else
		{
			// Shave off some texture units for shadow maps
			gDeferredAlphaWaterProgram.mFeatures.mIndexedTextureChannels =
				llmax(LLGLSLShader::sIndexedTextureChannels - 6, 1);
		}
		gDeferredAlphaWaterProgram.mFeatures.isAlphaLighting = true;
		// *HACK: to disable auto-setup of texture channels
		gDeferredAlphaWaterProgram.mFeatures.disableTextureIndex = true;
		gDeferredAlphaWaterProgram.mFeatures.hasShadows = use_sun_shadow;
		if (gUseNewShaders)
		{
			gDeferredAlphaWaterProgram.mFeatures.hasWaterFog = true;
			gDeferredAlphaWaterProgram.mFeatures.hasSrgb = true;
			gDeferredAlphaWaterProgram.mFeatures.encodesNormal = true;
			gDeferredAlphaWaterProgram.mFeatures.calculatesAtmospherics = true;
			gDeferredAlphaWaterProgram.mFeatures.hasAtmospherics = true;
			gDeferredAlphaWaterProgram.mFeatures.hasGamma = true;
			gDeferredAlphaWaterProgram.mFeatures.hasTransport = true;
			if (use_sun_shadow)
			{
				gDeferredAlphaWaterProgram.addPermutation("HAS_SHADOW", "1");
			}
			if (ambient_kill)
			{
				gDeferredAlphaWaterProgram.addPermutation("AMBIENT_KILL", "1");
			}
			if (sunlight_kill)
			{
				gDeferredAlphaWaterProgram.addPermutation("SUNLIGHT_KILL",
														  "1");
			}
			if (local_light_kill)
			{
				gDeferredAlphaWaterProgram.addPermutation("LOCAL_LIGHT_KILL",
														  "1");
			}
		}
		else
		{
			gDeferredAlphaWaterProgram.addPermutation("HAS_SHADOW",
													  has_shadows);
		}
		gDeferredAlphaWaterProgram.addPermutation("USE_INDEXED_TEX", "1");
		gDeferredAlphaWaterProgram.addPermutation("WATER_FOG", "1");
		gDeferredAlphaWaterProgram.addPermutation("USE_VERTEX_COLOR", "1");
		success = gDeferredAlphaWaterProgram.createShader();
		// *HACK: set after creation to disable auto-setup of texture channels
		gDeferredAlphaWaterProgram.mFeatures.calculatesLighting = true;
		gDeferredAlphaWaterProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredAvatarEyesProgram.setup("Deferred alpha eyes shader",
										 shader_level,
										 "deferred/avatarEyesV.glsl",
										 "deferred/diffuseF.glsl");
		gDeferredAvatarEyesProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredAvatarEyesProgram.mFeatures.hasGamma = true;
		gDeferredAvatarEyesProgram.mFeatures.hasTransport = true;
		gDeferredAvatarEyesProgram.mFeatures.disableTextureIndex = true;
		if (gUseNewShaders)
		{
			gDeferredAvatarEyesProgram.mFeatures.hasSrgb = true;
			gDeferredAvatarEyesProgram.mFeatures.encodesNormal = true;
		}
		success = gDeferredAvatarEyesProgram.createShader();
	}

	if (success)
	{
		gDeferredFullbrightProgram.setup("Deferred full bright shader",
										 shader_level,
										 "deferred/fullbrightV.glsl",
										 "deferred/fullbrightF.glsl");
		gDeferredFullbrightProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightProgram.mFeatures.hasTransport = true;
		if (gUseNewShaders)
		{
			gDeferredFullbrightProgram.mFeatures.hasSrgb = true;
		}
		gDeferredFullbrightProgram.mFeatures.mIndexedTextureChannels =
			LLGLSLShader::sIndexedTextureChannels;
		success = gDeferredFullbrightProgram.createShader();
	}

 	if (success)
	{
		gDeferredFullbrightAlphaMaskProgram.setup("Deferred full bright alpha masking shader",
												  shader_level,
												  "deferred/fullbrightV.glsl",
												  "deferred/fullbrightF.glsl");
		gDeferredFullbrightAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightAlphaMaskProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightAlphaMaskProgram.mFeatures.hasTransport = true;
		if (gUseNewShaders)
		{
			gDeferredFullbrightAlphaMaskProgram.mFeatures.hasSrgb = true;
		}
		gDeferredFullbrightAlphaMaskProgram.mFeatures.mIndexedTextureChannels =
			LLGLSLShader::sIndexedTextureChannels;
		gDeferredFullbrightAlphaMaskProgram.addPermutation("HAS_ALPHA_MASK",
														   "1");
		success = gDeferredFullbrightAlphaMaskProgram.createShader();
	}

	if (success)
	{
		gDeferredFullbrightWaterProgram.setup("Deferred full bright underwater shader",
											  shader_level,
											  "deferred/fullbrightV.glsl",
											  "deferred/fullbrightF.glsl");
		gDeferredFullbrightWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightWaterProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightWaterProgram.mFeatures.hasTransport = true;
		if (gUseNewShaders)
		{
			gDeferredFullbrightWaterProgram.mFeatures.hasWaterFog = true;
			gDeferredFullbrightWaterProgram.mFeatures.hasSrgb = true;
		}
		gDeferredFullbrightWaterProgram.mFeatures.mIndexedTextureChannels =
			LLGLSLShader::sIndexedTextureChannels;
		gDeferredFullbrightWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		gDeferredFullbrightWaterProgram.addPermutation("WATER_FOG", "1");
		success = gDeferredFullbrightWaterProgram.createShader();
	}

	if (success)
	{
		gDeferredFullbrightAlphaMaskWaterProgram.setup("Deferred full bright underwater alpha masking shader",
													   shader_level,
													   "deferred/fullbrightV.glsl",
													   "deferred/fullbrightF.glsl");
		gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.hasTransport = true;
		if (gUseNewShaders)
		{
			gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.hasWaterFog = true;
			gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.hasSrgb = true;
		}
		gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.mIndexedTextureChannels =
			LLGLSLShader::sIndexedTextureChannels;
		gDeferredFullbrightAlphaMaskWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		gDeferredFullbrightAlphaMaskWaterProgram.addPermutation("HAS_ALPHA_MASK","1");
		gDeferredFullbrightAlphaMaskWaterProgram.addPermutation("WATER_FOG","1");
		success = gDeferredFullbrightAlphaMaskWaterProgram.createShader();
	}

	if (success)
	{
		gDeferredFullbrightShinyProgram.setup("Deferred fullbrightshiny shader",
											  shader_level,
											  "deferred/fullbrightShinyV.glsl",
											  "deferred/fullbrightShinyF.glsl");
		gDeferredFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightShinyProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightShinyProgram.mFeatures.hasTransport = true;
		if (gUseNewShaders)
		{
			gDeferredFullbrightShinyProgram.mFeatures.hasAtmospherics = true;
			gDeferredFullbrightShinyProgram.mFeatures.hasSrgb = true;
		}
		gDeferredFullbrightShinyProgram.mFeatures.mIndexedTextureChannels =
			LLGLSLShader::sIndexedTextureChannels - 1;
		success = gDeferredFullbrightShinyProgram.createShader();
	}

	if (success)
	{
		gDeferredSkinnedFullbrightProgram.setup("Skinned full bright shader",
												mShaderLevel[SHADER_OBJECT],
												"objects/fullbrightSkinnedV.glsl",
												"deferred/fullbrightF.glsl");
		gDeferredSkinnedFullbrightProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredSkinnedFullbrightProgram.mFeatures.hasGamma = true;
		gDeferredSkinnedFullbrightProgram.mFeatures.hasTransport = true;
		gDeferredSkinnedFullbrightProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedFullbrightProgram.mFeatures.disableTextureIndex = true;
		if (gUseNewShaders)
		{
			gDeferredSkinnedFullbrightProgram.mFeatures.hasAtmospherics = true;
			gDeferredSkinnedFullbrightProgram.mFeatures.hasSrgb = true;
		}
		success = gDeferredSkinnedFullbrightProgram.createShader();
	}

	if (success)
	{
		gDeferredSkinnedFullbrightShinyProgram.setup("Skinned full bright shiny shader",
													 mShaderLevel[SHADER_OBJECT],
													 "objects/fullbrightShinySkinnedV.glsl",
													 "deferred/fullbrightShinyF.glsl");
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.hasGamma = true;
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.hasTransport = true;
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.disableTextureIndex = true;
		if (gUseNewShaders)
		{
			gDeferredSkinnedFullbrightShinyProgram.mFeatures.hasAtmospherics = true;
			gDeferredSkinnedFullbrightShinyProgram.mFeatures.hasSrgb = true;
		}
		success = gDeferredSkinnedFullbrightShinyProgram.createShader();
	}

	if (success)
	{
		gDeferredEmissiveProgram.setup("Deferred emissive shader",
									   shader_level,
									   "deferred/emissiveV.glsl",
									   "deferred/emissiveF.glsl");
		gDeferredEmissiveProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredEmissiveProgram.mFeatures.hasGamma = true;
		gDeferredEmissiveProgram.mFeatures.hasTransport = true;
		gDeferredEmissiveProgram.mFeatures.mIndexedTextureChannels =
			LLGLSLShader::sIndexedTextureChannels;
		success = gDeferredEmissiveProgram.createShader();
	}

	if (success)
	{
		gDeferredWaterProgram.setup("Deferred water shader", shader_level,
									"deferred/waterV.glsl",
									"deferred/waterF.glsl");
		gDeferredWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredWaterProgram.mFeatures.hasGamma = true;
		gDeferredWaterProgram.mFeatures.hasTransport = true;
		if (gUseNewShaders)
		{
			gDeferredWaterProgram.mFeatures.encodesNormal = true;
			gDeferredWaterProgram.mFeatures.hasSrgb = true;
			gDeferredWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		}
		success = gDeferredWaterProgram.createShader();
	}

	if (success)
	{
		gDeferredUnderWaterProgram.setup("Deferred under water shader",
										 shader_level,
										 "deferred/waterV.glsl",
										 "deferred/underWaterF.glsl");
		gDeferredUnderWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredUnderWaterProgram.mFeatures.hasGamma = true;
		gDeferredUnderWaterProgram.mFeatures.hasTransport = true;
		if (gUseNewShaders)
		{
			gDeferredUnderWaterProgram.mFeatures.hasWaterFog = true;
			gDeferredUnderWaterProgram.mFeatures.hasSrgb = true;
			gDeferredUnderWaterProgram.mFeatures.encodesNormal = true;
			gDeferredUnderWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		}
		success = gDeferredUnderWaterProgram.createShader();
	}

	S32 soften_level = shader_level;
	if (use_ao)
	{
		// When using SSAO, take screen space light map into account as if
		// shadows are enabled
		soften_level = llmax(soften_level, 2);
	}

	if (success)
	{
		gDeferredSoftenProgram.setup("Deferred soften shader", soften_level,
									 "deferred/softenLightV.glsl",
									 "deferred/softenLightF.glsl");
		gDeferredSoftenProgram.mFeatures.hasShadows = use_sun_shadow;
		if (gUseNewShaders)
		{
			gDeferredSoftenProgram.mFeatures.hasSrgb = true;
			gDeferredSoftenProgram.mFeatures.calculatesAtmospherics = true;
			gDeferredSoftenProgram.mFeatures.hasAtmospherics = true;
			gDeferredSoftenProgram.mFeatures.hasTransport = true;
			gDeferredSoftenProgram.mFeatures.hasGamma = true;
			gDeferredSoftenProgram.mFeatures.isDeferred = true;
			if (ambient_kill)
			{
				gDeferredSoftenProgram.addPermutation("AMBIENT_KILL", "1");
			}
			if (sunlight_kill)
			{
				gDeferredSoftenProgram.addPermutation("SUNLIGHT_KILL", "1");
			}
			if (local_light_kill)
			{
				gDeferredSoftenProgram.addPermutation("LOCAL_LIGHT_KILL", "1");
			}
		}
		success = gDeferredSoftenProgram.createShader();
	}

	if (success)
	{
		gDeferredSoftenWaterProgram.setup("Deferred soften underwater shader",
										  soften_level,
										  "deferred/softenLightV.glsl",
										  "deferred/softenLightF.glsl");
		gDeferredSoftenWaterProgram.addPermutation("WATER_FOG", "1");
		gDeferredSoftenWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		gDeferredSoftenWaterProgram.mFeatures.hasShadows = use_sun_shadow;
		if (gUseNewShaders)
		{
			gDeferredSoftenWaterProgram.mFeatures.hasWaterFog = true;
			gDeferredSoftenWaterProgram.mFeatures.hasSrgb = true;
			gDeferredSoftenWaterProgram.mFeatures.calculatesAtmospherics = true;
			gDeferredSoftenWaterProgram.mFeatures.hasAtmospherics = true;
			gDeferredSoftenWaterProgram.mFeatures.hasTransport = true;
			gDeferredSoftenWaterProgram.mFeatures.hasGamma = true;
			gDeferredSoftenWaterProgram.mFeatures.isDeferred = true;
			if (ambient_kill)
			{
				gDeferredSoftenWaterProgram.addPermutation("AMBIENT_KILL",
														   "1");
			}
			if (sunlight_kill)
			{
				gDeferredSoftenWaterProgram.addPermutation("SUNLIGHT_KILL",
														   "1");
			}
			if (local_light_kill)
			{
				gDeferredSoftenWaterProgram.addPermutation("LOCAL_LIGHT_KILL",
														   "1");
			}
		}
		success = gDeferredSoftenWaterProgram.createShader();
	}

	std::string depth_clamp = gGLManager.mUseDepthClamp ? "1" : "0";

	if (success)
	{
		gDeferredShadowProgram.setup("Deferred shadow shader", shader_level,
									 "deferred/shadowV.glsl",
									 "deferred/shadowF.glsl");
		gDeferredShadowProgram.mFeatures.hasShadows = true;
		if (gUseNewShaders)
		{
			gDeferredShadowProgram.mFeatures.isDeferred = true;
			if (gGLManager.mUseDepthClamp)
			{
				gDeferredShadowProgram.addPermutation("DEPTH_CLAMP", "1");
			}
		}
		else
		{
			gDeferredShadowProgram.addPermutation("DEPTH_CLAMP", depth_clamp);
		}
		success = gDeferredShadowProgram.createShader();
	}

	if (success)
	{
		gDeferredShadowCubeProgram.setup("Deferred shadow cube shader",
										 shader_level,
										 "deferred/shadowCubeV.glsl",
										 "deferred/shadowF.glsl");
		gDeferredShadowCubeProgram.mFeatures.hasShadows = true;
		if (gUseNewShaders)
		{
			gDeferredShadowCubeProgram.mFeatures.isDeferred = true;
			if (gGLManager.mUseDepthClamp)
			{
				gDeferredShadowCubeProgram.addPermutation("DEPTH_CLAMP", "1");
			}
		}
		else
		{
			gDeferredShadowCubeProgram.addPermutation("DEPTH_CLAMP",
													  depth_clamp);
		}
		success = gDeferredShadowCubeProgram.createShader();
	}

	if (gUseNewShaders && success)
	{
		gDeferredShadowFullbrightAlphaMaskProgram.setup("Deferred shadow full bright alpha mask shader",
														shader_level,
														"deferred/shadowAlphaMaskV.glsl",
														"deferred/shadowAlphaMaskF.glsl");
		gDeferredShadowFullbrightAlphaMaskProgram.mFeatures.mIndexedTextureChannels =
			LLGLSLShader::sIndexedTextureChannels;
		if (gGLManager.mUseDepthClamp)
		{
			gDeferredShadowFullbrightAlphaMaskProgram.addPermutation("DEPTH_CLAMP", "1");
		}
		gDeferredShadowFullbrightAlphaMaskProgram.addPermutation("IS_FULLBRIGHT",
																 "1");
		success = gDeferredShadowFullbrightAlphaMaskProgram.createShader();
	}

	if (success)
	{
		gDeferredShadowAlphaMaskProgram.setup("Deferred shadow alpha mask shader",
											  shader_level,
											  "deferred/shadowAlphaMaskV.glsl",
											  "deferred/shadowAlphaMaskF.glsl");
		gDeferredShadowAlphaMaskProgram.mFeatures.mIndexedTextureChannels =
			LLGLSLShader::sIndexedTextureChannels;
		if (!gUseNewShaders)
		{
			gDeferredShadowAlphaMaskProgram.addPermutation("DEPTH_CLAMP",
														   depth_clamp);
		}
		else if (gGLManager.mUseDepthClamp)
		{
			gDeferredShadowAlphaMaskProgram.addPermutation("DEPTH_CLAMP", "1");
		}
		success = gDeferredShadowAlphaMaskProgram.createShader();
	}

	if (success)
	{
		gDeferredAvatarShadowProgram.setup("Deferred avatar shadow shader",
										   shader_level,
										   "deferred/avatarShadowV.glsl",
										   "deferred/avatarShadowF.glsl");
		gDeferredAvatarShadowProgram.mFeatures.hasSkinning = true;
		if (!gUseNewShaders)
		{
			gDeferredAvatarShadowProgram.addPermutation("DEPTH_CLAMP",
														depth_clamp);
		}
		else if (gGLManager.mUseDepthClamp)
		{
			gDeferredAvatarShadowProgram.addPermutation("DEPTH_CLAMP", "1");
		}
		success = gDeferredAvatarShadowProgram.createShader();
	}

	if (success)
	{
		gDeferredAvatarAlphaShadowProgram.setup("Deferred avatar alpha shadow shader",
												shader_level,
												"deferred/avatarAlphaShadowV.glsl",
												"deferred/avatarAlphaShadowF.glsl");
		gDeferredAvatarAlphaShadowProgram.mFeatures.hasSkinning = true;
		gDeferredAvatarAlphaShadowProgram.addPermutation("DEPTH_CLAMP",
														 depth_clamp);
		success = gDeferredAvatarAlphaShadowProgram.createShader();
	}

	if (success)
	{
		gDeferredAvatarAlphaMaskShadowProgram.setup("Deferred avatar alpha mask shadow shader",
													shader_level,
													"deferred/avatarAlphaShadowV.glsl",
													"deferred/avatarAlphaMaskShadowF.glsl");
		gDeferredAvatarAlphaMaskShadowProgram.mFeatures.hasSkinning = true;
		gDeferredAvatarAlphaMaskShadowProgram.addPermutation("DEPTH_CLAMP",
															 depth_clamp);
		success = gDeferredAvatarAlphaMaskShadowProgram.createShader();
	}

	if (success)
	{
		gDeferredAttachmentShadowProgram.setup("Deferred attachment shadow shader",
											   shader_level,
											   "deferred/attachmentShadowV.glsl",
											   "deferred/attachmentShadowF.glsl");
		gDeferredAttachmentShadowProgram.mFeatures.hasObjectSkinning = true;
		if (!gUseNewShaders)
		{
			gDeferredAttachmentShadowProgram.addPermutation("DEPTH_CLAMP",
															depth_clamp);
		}
		else if (gGLManager.mUseDepthClamp)
		{
			gDeferredAttachmentShadowProgram.addPermutation("DEPTH_CLAMP",
															"1");
		}
		success = gDeferredAttachmentShadowProgram.createShader();
	}

	if (success)
	{
		gDeferredAttachmentAlphaShadowProgram.setup("Deferred attachment alpha shadow shader",
													shader_level,
													"deferred/attachmentAlphaShadowV.glsl",
													"deferred/attachmentAlphaShadowF.glsl");
		gDeferredAttachmentAlphaShadowProgram.mFeatures.hasObjectSkinning = true;
		gDeferredAttachmentAlphaShadowProgram.addPermutation("DEPTH_CLAMP",
															 depth_clamp);
		success = gDeferredAttachmentAlphaShadowProgram.createShader();
	}

	if (success)
	{
		gDeferredAttachmentAlphaMaskShadowProgram.setup("Deferred attachment alpha mask shadow shader",
														shader_level,
														"deferred/attachmentAlphaShadowV.glsl",
														"deferred/attachmentAlphaMaskShadowF.glsl");
		gDeferredAttachmentAlphaMaskShadowProgram.mFeatures.hasObjectSkinning = true;
		gDeferredAttachmentAlphaMaskShadowProgram.addPermutation("DEPTH_CLAMP",
																 depth_clamp);
		success = gDeferredAttachmentAlphaMaskShadowProgram.createShader();
	}

	if (success)
	{
		gDeferredTerrainProgram.setup("Deferred terrain shader", shader_level,
									  "deferred/terrainV.glsl",
									  "deferred/terrainF.glsl");
		if (gUseNewShaders)
		{
			gDeferredTerrainProgram.mFeatures.encodesNormal = true;
			gDeferredTerrainProgram.mFeatures.hasSrgb = true;
			gDeferredTerrainProgram.mFeatures.disableTextureIndex = true;
		}
		success = gDeferredTerrainProgram.createShader();
	}

	if (gUseNewShaders && success)
	{
		gDeferredTerrainWaterProgram.setup("Deferred terrain underwater shader",
										   shader_level,
										   "deferred/terrainV.glsl",
										   "deferred/terrainF.glsl");
		gDeferredTerrainWaterProgram.mFeatures.encodesNormal = true;
		gDeferredTerrainWaterProgram.mFeatures.hasSrgb = true;
		// *HACK: to disable auto-setup of texture channels
		gDeferredTerrainWaterProgram.mFeatures.disableTextureIndex = true;
		gDeferredTerrainWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		gDeferredTerrainWaterProgram.addPermutation("WATER_FOG", "1");
		success = gDeferredTerrainWaterProgram.createShader();
	}

	if (success)
	{
		gDeferredAvatarProgram.setup("Deferred avatar shader", shader_level,
									 "deferred/avatarV.glsl",
									 "deferred/avatarF.glsl");
		gDeferredAvatarProgram.mFeatures.hasSkinning = true;
		if (gUseNewShaders)
		{
			gDeferredAvatarProgram.mFeatures.encodesNormal = true;
		}
		gDeferredAvatarProgram.addPermutation("AVATAR_CLOTH",
											  mShaderLevel[SHADER_AVATAR] == 3 ?
												"1" : "0");
		success = gDeferredAvatarProgram.createShader();
	}

	if (success)
	{
		gDeferredAvatarAlphaProgram.setup("Avatar alpha shader", shader_level,
										  "deferred/alphaV.glsl",
										  "deferred/alphaF.glsl");
		gDeferredAvatarAlphaProgram.mFeatures.hasSkinning = true;
		gDeferredAvatarAlphaProgram.mFeatures.isAlphaLighting = true;
		gDeferredAvatarAlphaProgram.mFeatures.disableTextureIndex = true;
		gDeferredAvatarAlphaProgram.mFeatures.hasShadows = true;
		if (gUseNewShaders)
		{
			gDeferredAvatarAlphaProgram.mFeatures.hasSrgb = true;
			gDeferredAvatarAlphaProgram.mFeatures.encodesNormal = true;
			gDeferredAvatarAlphaProgram.mFeatures.calculatesAtmospherics = true;
			gDeferredAvatarAlphaProgram.mFeatures.hasAtmospherics = true;
			gDeferredAvatarAlphaProgram.mFeatures.hasTransport = true;
			gDeferredAvatarAlphaProgram.mFeatures.hasGamma = true;
			gDeferredAvatarAlphaProgram.mFeatures.isDeferred = true;
			if (use_sun_shadow)
			{
				gDeferredAvatarAlphaProgram.addPermutation("HAS_SHADOW", "1");
			}
			if (ambient_kill)
			{
				gDeferredAvatarAlphaProgram.addPermutation("AMBIENT_KILL", "1");
			}
			if (sunlight_kill)
			{
				gDeferredAvatarAlphaProgram.addPermutation("SUNLIGHT_KILL",
														  "1");
			}
			if (local_light_kill)
			{
				gDeferredAvatarAlphaProgram.addPermutation("LOCAL_LIGHT_KILL",
														  "1");
			}
		}
		else
		{
			gDeferredAvatarAlphaProgram.addPermutation("HAS_SHADOW",
													   has_shadows);
		}
		gDeferredAvatarAlphaProgram.addPermutation("USE_DIFFUSE_TEX", "1");
		gDeferredAvatarAlphaProgram.addPermutation("IS_AVATAR_SKIN", "1");
		success = gDeferredAvatarAlphaProgram.createShader();
		// *HACK: set after creation to disable auto-setup of texture channels
		gDeferredAvatarAlphaProgram.mFeatures.calculatesLighting = true;
		gDeferredAvatarAlphaProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredPostGammaCorrectProgram.setup("Deferred gamma correction post process",
											   shader_level,
											   "deferred/postDeferredNoTCV.glsl",
											   "deferred/postDeferredGammaCorrect.glsl");
		if (gUseNewShaders)
		{
			gDeferredPostGammaCorrectProgram.mFeatures.hasSrgb = true;
			gDeferredPostGammaCorrectProgram.mFeatures.isDeferred = true;
		}
		success = gDeferredPostGammaCorrectProgram.createShader();
	}

	if (success)
	{
		gFXAAProgram.setup("FXAA shader", shader_level,
						   "deferred/postDeferredV.glsl",
						   "deferred/fxaaF.glsl");
		if (gUseNewShaders)
		{
			gFXAAProgram.mFeatures.isDeferred = true;
		}
		success = gFXAAProgram.createShader();
	}

	if (success)
	{
		gDeferredPostProgram.setup("Deferred post shader", shader_level,
								   "deferred/postDeferredNoTCV.glsl",
								   "deferred/postDeferredF.glsl");
		if (gUseNewShaders)
		{
			gDeferredPostProgram.mFeatures.isDeferred = true;
		}
		success = gDeferredPostProgram.createShader();
	}

	if (success)
	{
		gDeferredCoFProgram.setup("Deferred CoF shader", shader_level,
								  "deferred/postDeferredNoTCV.glsl",
								  "deferred/cofF.glsl");
		if (gUseNewShaders)
		{
			gDeferredCoFProgram.mFeatures.isDeferred = true;
		}
		success = gDeferredCoFProgram.createShader();
	}

	if (success)
	{
		gDeferredDoFCombineProgram.setup("Deferred DoF combine shader",
										 shader_level,
										 "deferred/postDeferredNoTCV.glsl",
										 "deferred/dofCombineF.glsl");
		if (gUseNewShaders)
		{
			gDeferredDoFCombineProgram.mFeatures.isDeferred = true;
		}
		success = gDeferredDoFCombineProgram.createShader();
	}

	if (success)
	{
		gDeferredPostNoDoFProgram.setup("Deferred post shader",
										shader_level,
										"deferred/postDeferredNoTCV.glsl",
										"deferred/postDeferredNoDoFF.glsl");
		if (gUseNewShaders)
		{
			gDeferredPostNoDoFProgram.mFeatures.isDeferred = true;
		}
		success = gDeferredPostNoDoFProgram.createShader();
	}

	if (success)
	{
		gDeferredWLSkyProgram.setup("Deferred Windlight sky shader",
									shader_level,
									"deferred/skyV.glsl",
									"deferred/skyF.glsl");
		if (gUseNewShaders)
		{
			gDeferredWLSkyProgram.mFeatures.calculatesAtmospherics = true;
			gDeferredWLSkyProgram.mFeatures.hasTransport = true;
			gDeferredWLSkyProgram.mFeatures.hasGamma = true;
			gDeferredWLSkyProgram.mFeatures.hasSrgb = true;
		}
		gDeferredWLSkyProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gDeferredWLSkyProgram.createShader();
	}

	if (success)
	{
		gDeferredWLCloudProgram.setup("Deferred Windlight cloud shader",
									  shader_level,
									  "deferred/cloudsV.glsl",
									  "deferred/cloudsF.glsl");
		if (gUseNewShaders)
		{
			gDeferredWLCloudProgram.mFeatures.calculatesAtmospherics = true;
			gDeferredWLCloudProgram.mFeatures.hasTransport = true;
			gDeferredWLCloudProgram.mFeatures.hasGamma = true;
			gDeferredWLCloudProgram.mFeatures.hasSrgb = true;
			gDeferredWLCloudProgram.addConstant(LLGLSLShader::CONST_CLOUD_MOON_DEPTH);
		}
		gDeferredWLCloudProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gDeferredWLCloudProgram.createShader();
	}

	if (gUseNewShaders && success)
	{
		gDeferredWLSunProgram.setup("Deferred Windlight Sun program",
									shader_level,
									"deferred/sunDiscV.glsl",
									"deferred/sunDiscF.glsl");
		gDeferredWLSunProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredWLSunProgram.mFeatures.hasTransport = true;
		gDeferredWLSunProgram.mFeatures.hasGamma = true;
		gDeferredWLSunProgram.mFeatures.hasAtmospherics = true;
		gDeferredWLSunProgram.mFeatures.isFullbright = true;
		gDeferredWLSunProgram.mFeatures.disableTextureIndex = true;
		gDeferredWLSunProgram.mFeatures.hasSrgb = true;
		gDeferredWLSunProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gDeferredWLSunProgram.createShader();
	}

	if (gUseNewShaders && success)
	{
		gDeferredWLMoonProgram.setup("Deferred Windlight Moon program",
									 shader_level,
									 "deferred/moonV.glsl",
									 "deferred/moonF.glsl");
		gDeferredWLMoonProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredWLMoonProgram.mFeatures.hasTransport = true;
		gDeferredWLMoonProgram.mFeatures.hasGamma = true;
		gDeferredWLMoonProgram.mFeatures.hasAtmospherics = true;
		gDeferredWLMoonProgram.mFeatures.hasSrgb = true;
		gDeferredWLMoonProgram.mFeatures.isFullbright = true;
		gDeferredWLMoonProgram.mFeatures.disableTextureIndex = true;
		gDeferredWLMoonProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		gDeferredWLMoonProgram.addConstant(LLGLSLShader::CONST_CLOUD_MOON_DEPTH);
		success = gDeferredWLMoonProgram.createShader();
	}

	if (success)
	{
		gDeferredStarProgram.setup("Deferred star program", shader_level,
								   "deferred/starsV.glsl",
								   "deferred/starsF.glsl");
		gDeferredStarProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		if (gUseNewShaders)
		{
			gDeferredStarProgram.addConstant(LLGLSLShader::CONST_STAR_DEPTH);
		}
		success = gDeferredStarProgram.createShader();
	}

	if (success)
	{
		gNormalMapGenProgram.setup("Normal map generation program",
								   shader_level,
								   "deferred/normgenV.glsl",
								   "deferred/normgenF.glsl");
		gNormalMapGenProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gNormalMapGenProgram.createShader();
	}

	if (success)
	{
		llinfos << "Deferred shaders loaded at level: " << shader_level
				<< llendl;
	}

	return success;
}

bool LLViewerShaderMgr::loadShadersObject()
{
	S32 shader_level = mShaderLevel[SHADER_OBJECT];

	if (shader_level == 0)
	{
		gObjectShinyProgram.unload();
		gObjectFullbrightShinyProgram.unload();
		gObjectFullbrightShinyWaterProgram.unload();
		gObjectShinyWaterProgram.unload();
		gObjectFullbrightNoColorProgram.unload();
		gObjectFullbrightNoColorWaterProgram.unload();
		gObjectSimpleProgram.unload();
		gObjectSimpleImpostorProgram.unload();
		gObjectPreviewProgram.unload();
		gImpostorProgram.unload();
		gObjectSimpleAlphaMaskProgram.unload();
		gObjectBumpProgram.unload();
		gObjectSimpleWaterProgram.unload();
		gObjectSimpleWaterAlphaMaskProgram.unload();
		gObjectEmissiveProgram.unload();
		gObjectEmissiveWaterProgram.unload();
		gObjectFullbrightProgram.unload();
		gObjectFullbrightAlphaMaskProgram.unload();
		gObjectFullbrightWaterProgram.unload();
		gObjectFullbrightWaterAlphaMaskProgram.unload();
		gObjectShinyNonIndexedProgram.unload();
		gObjectFullbrightShinyNonIndexedProgram.unload();
		gObjectFullbrightShinyNonIndexedWaterProgram.unload();
		gObjectShinyNonIndexedWaterProgram.unload();
		gObjectSimpleNonIndexedTexGenProgram.unload();
		gObjectSimpleNonIndexedTexGenWaterProgram.unload();
		gObjectSimpleNonIndexedWaterProgram.unload();
		gObjectAlphaMaskNonIndexedProgram.unload();
		gObjectAlphaMaskNonIndexedWaterProgram.unload();
		gObjectAlphaMaskNoColorProgram.unload();
		gObjectAlphaMaskNoColorWaterProgram.unload();
		gObjectFullbrightNonIndexedProgram.unload();
		gObjectFullbrightNonIndexedWaterProgram.unload();
		gObjectEmissiveNonIndexedProgram.unload();
		gObjectEmissiveNonIndexedWaterProgram.unload();
		gSkinnedObjectSimpleProgram.unload();
		gSkinnedObjectFullbrightProgram.unload();
		gSkinnedObjectEmissiveProgram.unload();
		gSkinnedObjectFullbrightShinyProgram.unload();
		gSkinnedObjectShinySimpleProgram.unload();
		gSkinnedObjectSimpleWaterProgram.unload();
		gSkinnedObjectFullbrightWaterProgram.unload();
		gSkinnedObjectEmissiveWaterProgram.unload();
		gSkinnedObjectFullbrightShinyWaterProgram.unload();
		gSkinnedObjectShinySimpleWaterProgram.unload();
		gTreeProgram.unload();
		gTreeWaterProgram.unload();
		return true;
	}

	gObjectSimpleNonIndexedProgram.setup("Non indexed shader", shader_level,
										 "objects/simpleV.glsl",
										 "objects/simpleF.glsl");
	gObjectSimpleNonIndexedProgram.mFeatures.calculatesLighting = true;
	gObjectSimpleNonIndexedProgram.mFeatures.calculatesAtmospherics = true;
	gObjectSimpleNonIndexedProgram.mFeatures.hasGamma = true;
	gObjectSimpleNonIndexedProgram.mFeatures.hasAtmospherics = true;
	gObjectSimpleNonIndexedProgram.mFeatures.hasLighting = true;
	gObjectSimpleNonIndexedProgram.mFeatures.disableTextureIndex = true;
	gObjectSimpleNonIndexedProgram.mFeatures.hasAlphaMask = true;
	bool success = gObjectSimpleNonIndexedProgram.createShader();

	if (success)
	{
		gObjectSimpleNonIndexedTexGenProgram.setup("Non indexed tex-gen shader",
												   shader_level,
												   "objects/simpleTexGenV.glsl",
												   "objects/simpleF.glsl");
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.hasGamma = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.hasLighting = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.disableTextureIndex = true;
		success = gObjectSimpleNonIndexedTexGenProgram.createShader();
	}


	if (success)
	{
		gObjectSimpleNonIndexedWaterProgram.setup("Non indexed water shader",
												  shader_level,
												  "objects/simpleV.glsl",
												  "objects/simpleWaterF.glsl");
		gObjectSimpleNonIndexedWaterProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleNonIndexedWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleNonIndexedWaterProgram.mFeatures.hasWaterFog = true;
		gObjectSimpleNonIndexedWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleNonIndexedWaterProgram.mFeatures.hasLighting = true;
		gObjectSimpleNonIndexedWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectSimpleNonIndexedWaterProgram.mShaderGroup =
			LLGLSLShader::SG_WATER;
		success = gObjectSimpleNonIndexedWaterProgram.createShader();
	}

	if (success)
	{
		gObjectSimpleNonIndexedTexGenWaterProgram.setup("Non indexed tex-gen water shader",
														shader_level,
														"objects/simpleTexGenV.glsl",
														"objects/simpleWaterF.glsl");
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.hasWaterFog = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.hasLighting = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mShaderGroup =
			LLGLSLShader::SG_WATER;
		success = gObjectSimpleNonIndexedTexGenWaterProgram.createShader();
	}

	if (success)
	{
		gObjectAlphaMaskNonIndexedProgram.setup("Non indexed alpha mask shader",
												shader_level,
												"objects/simpleNonIndexedV.glsl",
												"objects/simpleF.glsl");
		gObjectAlphaMaskNonIndexedProgram.mFeatures.calculatesLighting = true;
		gObjectAlphaMaskNonIndexedProgram.mFeatures.calculatesAtmospherics = true;
		gObjectAlphaMaskNonIndexedProgram.mFeatures.hasGamma = true;
		gObjectAlphaMaskNonIndexedProgram.mFeatures.hasAtmospherics = true;
		gObjectAlphaMaskNonIndexedProgram.mFeatures.hasLighting = true;
		gObjectAlphaMaskNonIndexedProgram.mFeatures.disableTextureIndex = true;
		gObjectAlphaMaskNonIndexedProgram.mFeatures.hasAlphaMask = true;
		success = gObjectAlphaMaskNonIndexedProgram.createShader();
	}

	if (success)
	{
		gObjectAlphaMaskNonIndexedWaterProgram.setup("Non indexed alpha mask water shader",
													 shader_level,
													 "objects/simpleNonIndexedV.glsl",
													 "objects/simpleWaterF.glsl");
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.calculatesLighting = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.hasWaterFog = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.hasLighting = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.hasAlphaMask = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mShaderGroup =
			LLGLSLShader::SG_WATER;
		success = gObjectAlphaMaskNonIndexedWaterProgram.createShader();
	}

	if (success)
	{
		gObjectAlphaMaskNoColorProgram.setup("No color alpha mask shader",
											 shader_level,
											 "objects/simpleNoColorV.glsl",
											 "objects/simpleF.glsl");
		gObjectAlphaMaskNoColorProgram.mFeatures.calculatesLighting = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.calculatesAtmospherics = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.hasGamma = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.hasAtmospherics = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.hasLighting = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.disableTextureIndex = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.hasAlphaMask = true;
		success = gObjectAlphaMaskNoColorProgram.createShader();
	}

	if (success)
	{
		gObjectAlphaMaskNoColorWaterProgram.setup("No color alpha mask water shader",
												  shader_level,
												  "objects/simpleNoColorV.glsl",
												  "objects/simpleWaterF.glsl");
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.calculatesLighting = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.hasWaterFog = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.hasLighting = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.hasAlphaMask = true;
		gObjectAlphaMaskNoColorWaterProgram.mShaderGroup =
			LLGLSLShader::SG_WATER;
		success = gObjectAlphaMaskNoColorWaterProgram.createShader();
	}

	if (success)
	{
		gTreeProgram.setup("Tree shader", shader_level,
						   "objects/treeV.glsl", "objects/simpleF.glsl");
		gTreeProgram.mFeatures.calculatesLighting = true;
		gTreeProgram.mFeatures.calculatesAtmospherics = true;
		gTreeProgram.mFeatures.hasGamma = true;
		gTreeProgram.mFeatures.hasAtmospherics = true;
		gTreeProgram.mFeatures.hasLighting = true;
		gTreeProgram.mFeatures.disableTextureIndex = true;
		gTreeProgram.mFeatures.hasAlphaMask = true;
		success = gTreeProgram.createShader();
	}

	if (success)
	{
		gTreeWaterProgram.setup("Tree water shader", shader_level,
								"objects/treeV.glsl",
								"objects/simpleWaterF.glsl");
		gTreeWaterProgram.mFeatures.calculatesLighting = true;
		gTreeWaterProgram.mFeatures.calculatesAtmospherics = true;
		gTreeWaterProgram.mFeatures.hasWaterFog = true;
		gTreeWaterProgram.mFeatures.hasAtmospherics = true;
		gTreeWaterProgram.mFeatures.hasLighting = true;
		gTreeWaterProgram.mFeatures.disableTextureIndex = true;
		gTreeWaterProgram.mFeatures.hasAlphaMask = true;
		gTreeWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gTreeWaterProgram.createShader();
	}

	if (success)
	{
		gObjectFullbrightNonIndexedProgram.setup("Non indexed full bright shader",
												 shader_level,
												 "objects/fullbrightV.glsl",
												 "objects/fullbrightF.glsl");
		gObjectFullbrightNonIndexedProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightNonIndexedProgram.mFeatures.hasGamma = true;
		gObjectFullbrightNonIndexedProgram.mFeatures.hasTransport = true;
		gObjectFullbrightNonIndexedProgram.mFeatures.isFullbright = true;
		gObjectFullbrightNonIndexedProgram.mFeatures.disableTextureIndex = true;
		success = gObjectFullbrightNonIndexedProgram.createShader();
	}

	if (success)
	{
		gObjectFullbrightNonIndexedWaterProgram.setup("Non indexed full bright water shader",
													  shader_level,
													  "objects/fullbrightV.glsl",
													  "objects/fullbrightWaterF.glsl");
		gObjectFullbrightNonIndexedWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightNonIndexedWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightNonIndexedWaterProgram.mFeatures.hasWaterFog = true;
		gObjectFullbrightNonIndexedWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightNonIndexedWaterProgram.mFeatures.disableTextureIndex = true;
		if (gUseNewShaders)
		{
			gObjectFullbrightNonIndexedWaterProgram.mFeatures.hasSrgb = true;
		}
		gObjectFullbrightNonIndexedWaterProgram.mShaderGroup =
			LLGLSLShader::SG_WATER;
		success = gObjectFullbrightNonIndexedWaterProgram.createShader();
	}

	if (success)
	{
		gObjectEmissiveNonIndexedProgram.setup("Non indexed emissive shader",
											   shader_level,
											   "objects/emissiveV.glsl",
											   "objects/fullbrightF.glsl");
		gObjectEmissiveNonIndexedProgram.mFeatures.calculatesAtmospherics = true;
		gObjectEmissiveNonIndexedProgram.mFeatures.hasGamma = true;
		gObjectEmissiveNonIndexedProgram.mFeatures.hasTransport = true;
		gObjectEmissiveNonIndexedProgram.mFeatures.isFullbright = true;
		gObjectEmissiveNonIndexedProgram.mFeatures.disableTextureIndex = true;
		if (gUseNewShaders)
		{
			gObjectEmissiveNonIndexedProgram.mFeatures.hasSrgb = true;
		}
		success = gObjectEmissiveNonIndexedProgram.createShader();
	}

	if (success)
	{
		gObjectEmissiveNonIndexedWaterProgram.setup("Non indexed emissive water shader",
													shader_level,
													"objects/emissiveV.glsl",
													"objects/fullbrightWaterF.glsl");
		gObjectEmissiveNonIndexedWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectEmissiveNonIndexedWaterProgram.mFeatures.isFullbright = true;
		gObjectEmissiveNonIndexedWaterProgram.mFeatures.hasWaterFog = true;
		gObjectEmissiveNonIndexedWaterProgram.mFeatures.hasTransport = true;
		gObjectEmissiveNonIndexedWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectEmissiveNonIndexedWaterProgram.mShaderGroup =
			LLGLSLShader::SG_WATER;
		success = gObjectEmissiveNonIndexedWaterProgram.createShader();
	}

	if (success)
	{
		gObjectFullbrightNoColorProgram.setup("Non indexed no color full bright shader",
											  shader_level,
											  "objects/fullbrightNoColorV.glsl",
											  "objects/fullbrightF.glsl");
		gObjectFullbrightNoColorProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightNoColorProgram.mFeatures.hasGamma = true;
		gObjectFullbrightNoColorProgram.mFeatures.hasTransport = true;
		gObjectFullbrightNoColorProgram.mFeatures.isFullbright = true;
		gObjectFullbrightNoColorProgram.mFeatures.disableTextureIndex = true;
		if (gUseNewShaders)
		{
			gObjectFullbrightNoColorProgram.mFeatures.hasSrgb = true;
		}
		success = gObjectFullbrightNoColorProgram.createShader();
	}

	if (success)
	{
		gObjectFullbrightNoColorWaterProgram.setup("Non indexed no color full bright water shader",
												   shader_level,
												   "objects/fullbrightNoColorV.glsl",
												   "objects/fullbrightWaterF.glsl");
		gObjectFullbrightNoColorWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightNoColorWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightNoColorWaterProgram.mFeatures.hasWaterFog = true;
		gObjectFullbrightNoColorWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightNoColorWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectFullbrightNoColorWaterProgram.mShaderGroup =
			LLGLSLShader::SG_WATER;
		success = gObjectFullbrightNoColorWaterProgram.createShader();
	}

	if (success)
	{
		gObjectShinyNonIndexedProgram.setup("Non indexed shiny shader",
											shader_level,
											"objects/shinyV.glsl",
											"objects/shinyF.glsl");
		gObjectShinyNonIndexedProgram.mFeatures.calculatesAtmospherics = true;
		gObjectShinyNonIndexedProgram.mFeatures.calculatesLighting = true;
		gObjectShinyNonIndexedProgram.mFeatures.hasGamma = true;
		gObjectShinyNonIndexedProgram.mFeatures.hasAtmospherics = true;
		gObjectShinyNonIndexedProgram.mFeatures.isShiny = true;
		gObjectShinyNonIndexedProgram.mFeatures.disableTextureIndex = true;
		success = gObjectShinyNonIndexedProgram.createShader();
	}

	if (success)
	{
		gObjectShinyNonIndexedWaterProgram.setup("Non indexed shiny water shader",
												 shader_level,
												 "objects/shinyV.glsl",
												 "objects/shinyWaterF.glsl");
		gObjectShinyNonIndexedWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectShinyNonIndexedWaterProgram.mFeatures.calculatesLighting = true;
		gObjectShinyNonIndexedWaterProgram.mFeatures.isShiny = true;
		gObjectShinyNonIndexedWaterProgram.mFeatures.hasWaterFog = true;
		gObjectShinyNonIndexedWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectShinyNonIndexedWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectShinyNonIndexedWaterProgram.mShaderGroup =
			LLGLSLShader::SG_WATER;
		success = gObjectShinyNonIndexedWaterProgram.createShader();
	}

	if (success)
	{
		gObjectFullbrightShinyNonIndexedProgram.setup("Non indexed full bright shiny shader",
													  shader_level,
													  "objects/fullbrightShinyV.glsl",
													  "objects/fullbrightShinyF.glsl");
		gObjectFullbrightShinyNonIndexedProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightShinyNonIndexedProgram.mFeatures.isFullbright = true;
		gObjectFullbrightShinyNonIndexedProgram.mFeatures.isShiny = true;
		gObjectFullbrightShinyNonIndexedProgram.mFeatures.hasGamma = true;
		gObjectFullbrightShinyNonIndexedProgram.mFeatures.hasTransport = true;
		gObjectFullbrightShinyNonIndexedProgram.mFeatures.disableTextureIndex = true;
		success = gObjectFullbrightShinyNonIndexedProgram.createShader();
	}

	if (success)
	{
		gObjectFullbrightShinyNonIndexedWaterProgram.setup("Non indexed full bright shiny water shader",
														   shader_level,
														   "objects/fullbrightShinyV.glsl",
														   "objects/fullbrightShinyWaterF.glsl");
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.isShiny = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.hasGamma = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.hasWaterFog = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mShaderGroup =
			LLGLSLShader::SG_WATER;
		success = gObjectFullbrightShinyNonIndexedWaterProgram.createShader();
	}

	if (success)
	{
		gImpostorProgram.setup("Impostor shader", shader_level,
							   "objects/impostorV.glsl",
							   "objects/impostorF.glsl");
		gImpostorProgram.mFeatures.disableTextureIndex = true;
		if (gUseNewShaders)
		{
			gImpostorProgram.mFeatures.hasSrgb = true;
		}
		success = gImpostorProgram.createShader();
	}

	if (success)
	{
		gObjectPreviewProgram.setup("Preview shader", shader_level,
								    "objects/previewV.glsl",
								    "objects/previewF.glsl");
		gObjectPreviewProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectPreviewProgram.mFeatures.disableTextureIndex = true;
		success = gObjectPreviewProgram.createShader();
		// *HACK: set after creation to disable auto-setup of texture channels
		gObjectPreviewProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gObjectSimpleProgram.setup("Simple shader", shader_level,
								   "objects/simpleV.glsl",
								   "objects/simpleF.glsl");
		gObjectSimpleProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleProgram.mFeatures.hasGamma = true;
		gObjectSimpleProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleProgram.mFeatures.hasLighting = true;
		gObjectSimpleProgram.mFeatures.mIndexedTextureChannels = 0;
		success = gObjectSimpleProgram.createShader();
	}

	if (success)
	{
		gObjectSimpleImpostorProgram.setup("Simple impostor shader",
										   shader_level,
										   "objects/simpleV.glsl",
										   "objects/simpleF.glsl");
		gObjectSimpleImpostorProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleImpostorProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleImpostorProgram.mFeatures.hasGamma = true;
		gObjectSimpleImpostorProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleImpostorProgram.mFeatures.hasLighting = true;
		gObjectSimpleImpostorProgram.mFeatures.mIndexedTextureChannels = 0;
		// Force alpha mask version of lighting so we can weed out transparent
		// pixels from impostor temp buffer:
		gObjectSimpleImpostorProgram.mFeatures.hasAlphaMask = true;
		success = gObjectSimpleImpostorProgram.createShader();
	}

	if (success)
	{
		gObjectSimpleWaterProgram.setup("Simple water shader", shader_level,
									    "objects/simpleV.glsl",
									    "objects/simpleWaterF.glsl");
		gObjectSimpleWaterProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleWaterProgram.mFeatures.hasWaterFog = true;
		gObjectSimpleWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleWaterProgram.mFeatures.hasLighting = true;
		gObjectSimpleWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectSimpleWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectSimpleWaterProgram.createShader();
	}

	if (success)
	{
		gObjectBumpProgram.setup("Bump shader", shader_level,
								 "objects/bumpV.glsl", "objects/bumpF.glsl");
		if (gUseNewShaders)
		{
			gObjectBumpProgram.mFeatures.encodesNormal = true;
		}
		success = gObjectBumpProgram.createShader();
		if (success)
		{
			// LLDrawpoolBump assumes "texture0" has channel 0 and "texture1"
			// has channel 1
			gObjectBumpProgram.bind();
			gObjectBumpProgram.uniform1i(sTexture0, 0);
			gObjectBumpProgram.uniform1i(sTexture1, 1);
			gObjectBumpProgram.unbind();
		}
	}

	if (success)
	{
		gObjectSimpleAlphaMaskProgram.setup("Simple alpha mask shader",
											shader_level,
											"objects/simpleV.glsl",
											"objects/simpleF.glsl");
		gObjectSimpleAlphaMaskProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleAlphaMaskProgram.mFeatures.hasGamma = true;
		gObjectSimpleAlphaMaskProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleAlphaMaskProgram.mFeatures.hasLighting = true;
		gObjectSimpleAlphaMaskProgram.mFeatures.hasAlphaMask = true;
		gObjectSimpleAlphaMaskProgram.mFeatures.mIndexedTextureChannels = 0;
		success = gObjectSimpleAlphaMaskProgram.createShader();
	}

	if (success)
	{
		gObjectSimpleWaterAlphaMaskProgram.setup("Simple water alpha mask shader",
												 shader_level,
												 "objects/simpleV.glsl",
												 "objects/simpleWaterF.glsl");
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.hasWaterFog = true;
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.hasLighting = true;
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.hasAlphaMask = true;
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectSimpleWaterAlphaMaskProgram.mShaderGroup =
			LLGLSLShader::SG_WATER;
		success = gObjectSimpleWaterAlphaMaskProgram.createShader();
	}

	if (success)
	{
		gObjectFullbrightProgram.setup("Fullbright shader", shader_level,
									   "objects/fullbrightV.glsl",
									   "objects/fullbrightF.glsl");
		gObjectFullbrightProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightProgram.mFeatures.hasGamma = true;
		gObjectFullbrightProgram.mFeatures.hasTransport = true;
		gObjectFullbrightProgram.mFeatures.isFullbright = true;
		if (gUseNewShaders)
		{
			gObjectFullbrightProgram.mFeatures.hasSrgb = true;
		}
		gObjectFullbrightProgram.mFeatures.mIndexedTextureChannels = 0;
		success = gObjectFullbrightProgram.createShader();
	}

	if (success)
	{
		gObjectFullbrightWaterProgram.setup("Fullbright water shader",
											shader_level,
											"objects/fullbrightV.glsl",
											"objects/fullbrightWaterF.glsl");
		gObjectFullbrightWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightWaterProgram.mFeatures.hasWaterFog = true;
		gObjectFullbrightWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectFullbrightWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectFullbrightWaterProgram.createShader();
	}

	if (success)
	{
		gObjectEmissiveProgram.setup("Emissive shader", shader_level,
									 "objects/emissiveV.glsl",
									 "objects/fullbrightF.glsl");
		gObjectEmissiveProgram.mFeatures.calculatesAtmospherics = true;
		gObjectEmissiveProgram.mFeatures.hasGamma = true;
		gObjectEmissiveProgram.mFeatures.hasTransport = true;
		gObjectEmissiveProgram.mFeatures.isFullbright = true;
		if (gUseNewShaders)
		{
			gObjectEmissiveProgram.mFeatures.hasSrgb = true;
		}
		gObjectEmissiveProgram.mFeatures.mIndexedTextureChannels = 0;
		success = gObjectEmissiveProgram.createShader();
	}

	if (success)
	{
		gObjectEmissiveWaterProgram.setup("Emissive water shader",
										  shader_level,
										  "objects/emissiveV.glsl",
										  "objects/fullbrightWaterF.glsl");
		gObjectEmissiveWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectEmissiveWaterProgram.mFeatures.isFullbright = true;
		gObjectEmissiveWaterProgram.mFeatures.hasWaterFog = true;
		gObjectEmissiveWaterProgram.mFeatures.hasTransport = true;
		gObjectEmissiveWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectEmissiveWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectEmissiveWaterProgram.createShader();
	}

	if (success)
	{
		gObjectFullbrightAlphaMaskProgram.setup("Fullbright alpha mask shader",
												shader_level,
												"objects/fullbrightV.glsl",
												"objects/fullbrightF.glsl");
		gObjectFullbrightAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightAlphaMaskProgram.mFeatures.hasGamma = true;
		gObjectFullbrightAlphaMaskProgram.mFeatures.hasTransport = true;
		gObjectFullbrightAlphaMaskProgram.mFeatures.isFullbright = true;
		gObjectFullbrightAlphaMaskProgram.mFeatures.hasAlphaMask = true;
		if (gUseNewShaders)
		{
			gObjectFullbrightAlphaMaskProgram.mFeatures.hasSrgb = true;
		}
		gObjectFullbrightAlphaMaskProgram.mFeatures.mIndexedTextureChannels = 0;
		success = gObjectFullbrightAlphaMaskProgram.createShader();
	}

	if (success)
	{
		gObjectFullbrightWaterAlphaMaskProgram.setup("Fullbright water shader",
													 shader_level,
													 "objects/fullbrightV.glsl",
													 "objects/fullbrightWaterF.glsl");
		gObjectFullbrightWaterAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightWaterAlphaMaskProgram.mFeatures.isFullbright = true;
		gObjectFullbrightWaterAlphaMaskProgram.mFeatures.hasWaterFog = true;
		gObjectFullbrightWaterAlphaMaskProgram.mFeatures.hasTransport = true;
		gObjectFullbrightWaterAlphaMaskProgram.mFeatures.hasAlphaMask = true;
		gObjectFullbrightWaterAlphaMaskProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectFullbrightWaterAlphaMaskProgram.mShaderGroup =
			LLGLSLShader::SG_WATER;
		success = gObjectFullbrightWaterAlphaMaskProgram.createShader();
	}

	if (success)
	{
		gObjectShinyProgram.setup("Shiny shader", shader_level,
								  "objects/shinyV.glsl",
								  "objects/shinyF.glsl");
		gObjectShinyProgram.mFeatures.calculatesAtmospherics = true;
		gObjectShinyProgram.mFeatures.calculatesLighting = true;
		gObjectShinyProgram.mFeatures.hasGamma = true;
		gObjectShinyProgram.mFeatures.hasAtmospherics = true;
		gObjectShinyProgram.mFeatures.isShiny = true;
		gObjectShinyProgram.mFeatures.mIndexedTextureChannels = 0;
		success = gObjectShinyProgram.createShader();
	}

	if (success)
	{
		gObjectShinyWaterProgram.setup("Shiny water shader", shader_level,
									   "objects/shinyV.glsl",
									   "objects/shinyWaterF.glsl");
		gObjectShinyWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectShinyWaterProgram.mFeatures.calculatesLighting = true;
		gObjectShinyWaterProgram.mFeatures.isShiny = true;
		gObjectShinyWaterProgram.mFeatures.hasWaterFog = true;
		gObjectShinyWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectShinyWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectShinyWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectShinyWaterProgram.createShader();
	}

	if (success)
	{
		gObjectFullbrightShinyProgram.setup("Fullbright shiny shader",
											shader_level,
											"objects/fullbrightShinyV.glsl",
											"objects/fullbrightShinyF.glsl");
		gObjectFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightShinyProgram.mFeatures.isFullbright = true;
		gObjectFullbrightShinyProgram.mFeatures.isShiny = true;
		gObjectFullbrightShinyProgram.mFeatures.hasGamma = true;
		gObjectFullbrightShinyProgram.mFeatures.hasTransport = true;
		gObjectFullbrightShinyProgram.mFeatures.mIndexedTextureChannels = 0;
		success = gObjectFullbrightShinyProgram.createShader();
	}

	if (success)
	{
		gObjectFullbrightShinyWaterProgram.setup("Fullbright shiny water shader",
												 shader_level,
												 "objects/fullbrightShinyV.glsl",
												 "objects/fullbrightShinyWaterF.glsl");
		gObjectFullbrightShinyWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.isShiny = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.hasGamma = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.hasWaterFog = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectFullbrightShinyWaterProgram.mShaderGroup =
			LLGLSLShader::SG_WATER;
		success = gObjectFullbrightShinyWaterProgram.createShader();
	}

	if (mShaderLevel[SHADER_AVATAR] > 0)
	{
		// Load skinned attachment shaders
		if (success)
		{
			gSkinnedObjectSimpleProgram.setup("Skinned simple shader",
											  shader_level,
											  "objects/simpleSkinnedV.glsl",
											  "objects/simpleF.glsl");
			gSkinnedObjectSimpleProgram.mFeatures.calculatesLighting = true;
			gSkinnedObjectSimpleProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectSimpleProgram.mFeatures.hasGamma = true;
			gSkinnedObjectSimpleProgram.mFeatures.hasAtmospherics = true;
			gSkinnedObjectSimpleProgram.mFeatures.hasLighting = true;
			gSkinnedObjectSimpleProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectSimpleProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectSimpleProgram.mFeatures.disableTextureIndex = true;
			success = gSkinnedObjectSimpleProgram.createShader();
		}

		if (success)
		{
			gSkinnedObjectFullbrightProgram.setup("Skinned full bright shader",
												  shader_level,
												  "objects/fullbrightSkinnedV.glsl",
												  "objects/fullbrightF.glsl");
			gSkinnedObjectFullbrightProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectFullbrightProgram.mFeatures.hasGamma = true;
			gSkinnedObjectFullbrightProgram.mFeatures.hasTransport = true;
			gSkinnedObjectFullbrightProgram.mFeatures.isFullbright = true;
			gSkinnedObjectFullbrightProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectFullbrightProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectFullbrightProgram.mFeatures.disableTextureIndex = true;
			if (gUseNewShaders)
			{
				gSkinnedObjectFullbrightProgram.mFeatures.hasSrgb = true;
			}
			success = gSkinnedObjectFullbrightProgram.createShader();
		}

		if (success)
		{
			gSkinnedObjectEmissiveProgram.setup("Skinned emissive shader",
												shader_level,
												"objects/emissiveSkinnedV.glsl",
												"objects/fullbrightF.glsl");
			gSkinnedObjectEmissiveProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectEmissiveProgram.mFeatures.hasGamma = true;
			gSkinnedObjectEmissiveProgram.mFeatures.hasTransport = true;
			gSkinnedObjectEmissiveProgram.mFeatures.isFullbright = true;
			gSkinnedObjectEmissiveProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectEmissiveProgram.mFeatures.disableTextureIndex = true;
			if (gUseNewShaders)
			{
				gSkinnedObjectEmissiveProgram.mFeatures.hasSrgb = true;
			}
			success = gSkinnedObjectEmissiveProgram.createShader();
		}

		if (success)
		{
			gSkinnedObjectEmissiveWaterProgram.setup("Skinned emissive water shader",
													 shader_level,
													 "objects/emissiveSkinnedV.glsl",
													 "objects/fullbrightWaterF.glsl");
			gSkinnedObjectEmissiveWaterProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectEmissiveWaterProgram.mFeatures.hasGamma = true;
			gSkinnedObjectEmissiveWaterProgram.mFeatures.hasTransport = true;
			gSkinnedObjectEmissiveWaterProgram.mFeatures.isFullbright = true;
			gSkinnedObjectEmissiveWaterProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectEmissiveWaterProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectEmissiveWaterProgram.mFeatures.hasWaterFog = true;
			success = gSkinnedObjectEmissiveWaterProgram.createShader();
		}

		if (success)
		{
			gSkinnedObjectFullbrightShinyProgram.setup("Skinned full bright shiny shader",
													   shader_level,
													   "objects/fullbrightShinySkinnedV.glsl",
													   "objects/fullbrightShinyF.glsl");
			gSkinnedObjectFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.hasGamma = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.hasTransport = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.isShiny = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.isFullbright = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.disableTextureIndex = true;
			success = gSkinnedObjectFullbrightShinyProgram.createShader();
		}

		if (success)
		{
			gSkinnedObjectShinySimpleProgram.setup("Skinned shiny simple shader",
												   shader_level,
												   "objects/shinySimpleSkinnedV.glsl",
												   "objects/shinyF.glsl");
			gSkinnedObjectShinySimpleProgram.mFeatures.calculatesLighting = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.hasGamma = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.hasAtmospherics = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.isShiny = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.disableTextureIndex = true;
			success = gSkinnedObjectShinySimpleProgram.createShader();
		}

		if (success)
		{
			gSkinnedObjectSimpleWaterProgram.setup("Skinned simple water shader",
												   shader_level,
												   "objects/simpleSkinnedV.glsl",
												   "objects/simpleWaterF.glsl");
			gSkinnedObjectSimpleWaterProgram.mFeatures.calculatesLighting = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.hasGamma = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.hasAtmospherics = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.hasLighting = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.hasWaterFog = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectSimpleWaterProgram.mShaderGroup =
				LLGLSLShader::SG_WATER;
			success = gSkinnedObjectSimpleWaterProgram.createShader();
		}

		if (success)
		{
			gSkinnedObjectFullbrightWaterProgram.setup("Skinned full bright water shader",
													   shader_level,
													   "objects/fullbrightSkinnedV.glsl",
													   "objects/fullbrightWaterF.glsl");
			gSkinnedObjectFullbrightWaterProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.hasGamma = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.hasTransport = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.isFullbright = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.hasWaterFog = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectFullbrightWaterProgram.mShaderGroup =
				LLGLSLShader::SG_WATER;
			success = gSkinnedObjectFullbrightWaterProgram.createShader();
		}

		if (success)
		{
			gSkinnedObjectFullbrightShinyWaterProgram.setup("Skinned full bright shiny water shader",
															shader_level,
															"objects/fullbrightShinySkinnedV.glsl",
															"objects/fullbrightShinyWaterF.glsl");
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.hasGamma = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.hasTransport = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.isShiny = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.isFullbright = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.hasWaterFog = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mShaderGroup =
				LLGLSLShader::SG_WATER;
			success = gSkinnedObjectFullbrightShinyWaterProgram.createShader();
		}

		if (success)
		{
			gSkinnedObjectShinySimpleWaterProgram.setup("Skinned shiny simple water shader",
														shader_level,
														"objects/shinySimpleSkinnedV.glsl",
														"objects/shinyWaterF.glsl");
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.calculatesLighting = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.hasGamma = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.hasAtmospherics = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.isShiny = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.hasWaterFog = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectShinySimpleWaterProgram.mShaderGroup =
				LLGLSLShader::SG_WATER;
			success = gSkinnedObjectShinySimpleWaterProgram.createShader();
		}
	}

	if (success)
	{
		llinfos << "Object shaders loaded at level: " << shader_level
				<< llendl;
	}
	else
	{
		mShaderLevel[SHADER_OBJECT] = 0;
	}

	return success;
}

bool LLViewerShaderMgr::loadShadersAvatar()
{
	S32 shader_level = mShaderLevel[SHADER_AVATAR];

	if (shader_level == 0)
	{
		gAvatarProgram.unload();
		gAvatarWaterProgram.unload();
		gAvatarEyeballProgram.unload();
		gAvatarPickProgram.unload();
		return true;
	}

	gAvatarProgram.setup("Avatar shader", shader_level,
						 "avatar/avatarV.glsl", "avatar/avatarF.glsl");
	gAvatarProgram.mFeatures.hasSkinning = true;
	gAvatarProgram.mFeatures.calculatesAtmospherics = true;
	gAvatarProgram.mFeatures.calculatesLighting = true;
	gAvatarProgram.mFeatures.hasGamma = true;
	gAvatarProgram.mFeatures.hasAtmospherics = true;
	gAvatarProgram.mFeatures.hasLighting = true;
	gAvatarProgram.mFeatures.hasAlphaMask = true;
	gAvatarProgram.mFeatures.disableTextureIndex = true;
	bool success = gAvatarProgram.createShader();

	if (success)
	{
		gAvatarWaterProgram.setup("Avatar water shader",
								  // Note: no cloth under water:
								  llmin(shader_level, 1),
								  "avatar/avatarV.glsl",
								  "objects/simpleWaterF.glsl");
		gAvatarWaterProgram.mFeatures.hasSkinning = true;
		gAvatarWaterProgram.mFeatures.calculatesAtmospherics = true;
		gAvatarWaterProgram.mFeatures.calculatesLighting = true;
		gAvatarWaterProgram.mFeatures.hasWaterFog = true;
		gAvatarWaterProgram.mFeatures.hasAtmospherics = true;
		gAvatarWaterProgram.mFeatures.hasLighting = true;
		gAvatarWaterProgram.mFeatures.hasAlphaMask = true;
		gAvatarWaterProgram.mFeatures.disableTextureIndex = true;
		gAvatarWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gAvatarWaterProgram.createShader();
	}

	// Keep track of avatar levels
	if (gAvatarProgram.mShaderLevel != mShaderLevel[SHADER_AVATAR])
	{
		shader_level = mMaxAvatarShaderLevel = mShaderLevel[SHADER_AVATAR] =
					   gAvatarProgram.mShaderLevel;
	}

	if (success)
	{
		gAvatarPickProgram.setup("Avatar pick shader", shader_level,
								 "avatar/pickAvatarV.glsl",
								 "avatar/pickAvatarF.glsl");
		gAvatarPickProgram.mFeatures.hasSkinning = true;
		gAvatarPickProgram.mFeatures.disableTextureIndex = true;
		success = gAvatarPickProgram.createShader();
	}

	if (success)
	{
		gAvatarEyeballProgram.setup("Avatar eyeball program", shader_level,
									"avatar/eyeballV.glsl",
									"avatar/eyeballF.glsl");
		gAvatarEyeballProgram.mFeatures.calculatesLighting = true;
		gAvatarEyeballProgram.mFeatures.isSpecular = true;
		gAvatarEyeballProgram.mFeatures.calculatesAtmospherics = true;
		gAvatarEyeballProgram.mFeatures.hasGamma = true;
		gAvatarEyeballProgram.mFeatures.hasAtmospherics = true;
		gAvatarEyeballProgram.mFeatures.hasLighting = true;
		gAvatarEyeballProgram.mFeatures.hasAlphaMask = true;
		gAvatarEyeballProgram.mFeatures.disableTextureIndex = true;
		success = gAvatarEyeballProgram.createShader();
	}

	if (success)
	{
		llinfos << "Avatar shaders loaded at level: " << shader_level
				<< llendl;
	}
	else
	{
		mShaderLevel[SHADER_AVATAR] = 0;
		mMaxAvatarShaderLevel = 0;
	}

	return success;
}

bool LLViewerShaderMgr::loadShadersInterface()
{
	S32 shader_level = mShaderLevel[SHADER_INTERFACE];

	if (shader_level == 0)
	{
		gHighlightProgram.unload();
		return true;
	}

	gHighlightProgram.setup("Highlight shader", shader_level,
							"interface/highlightV.glsl",
							"interface/highlightF.glsl");
	bool success = gHighlightProgram.createShader();

	if (success)
	{
		gHighlightNormalProgram.setup("Highlight normals shader", shader_level,
									  "interface/highlightNormV.glsl",
									  "interface/highlightF.glsl");
		success = gHighlightNormalProgram.createShader();
	}

	if (success)
	{
		gHighlightSpecularProgram.setup("Highlight specular shader",
										shader_level,
										"interface/highlightSpecV.glsl",
										"interface/highlightF.glsl");
		success = gHighlightSpecularProgram.createShader();
	}

	if (success)
	{
		gUIProgram.setup("UI shader", shader_level,
						 "interface/uiV.glsl", "interface/uiF.glsl");
		success = gUIProgram.createShader();
	}

	if (success)
	{
		gCustomAlphaProgram.setup("Custom alpha shader", shader_level,
								  "interface/customalphaV.glsl",
								  "interface/customalphaF.glsl");
		success = gCustomAlphaProgram.createShader();
	}

	if (success)
	{
		gSplatTextureRectProgram.setup("Splat texture rect shader",
									   shader_level,
									   "interface/splattexturerectV.glsl",
									   "interface/splattexturerectF.glsl");
		success = gSplatTextureRectProgram.createShader();
		if (success)
		{
			gSplatTextureRectProgram.bind();
			gSplatTextureRectProgram.uniform1i(sScreenMap, 0);
			gSplatTextureRectProgram.unbind();
		}
	}

	if (success)
	{
		gGlowCombineProgram.setup("Glow combine shader", shader_level,
								  "interface/glowcombineV.glsl",
								  "interface/glowcombineF.glsl");
		success = gGlowCombineProgram.createShader();
		if (success)
		{
			gGlowCombineProgram.bind();
			gGlowCombineProgram.uniform1i(sGlowMap, 0);
			gGlowCombineProgram.uniform1i(sScreenMap, 1);
			gGlowCombineProgram.unbind();
		}
	}

	if (success)
	{
		gGlowCombineFXAAProgram.setup("Glow combine FXAA shader",
									  shader_level,
									  "interface/glowcombineFXAAV.glsl",
									  "interface/glowcombineFXAAF.glsl");
		success = gGlowCombineFXAAProgram.createShader();
		if (success)
		{
			gGlowCombineFXAAProgram.bind();
			gGlowCombineFXAAProgram.uniform1i(sGlowMap, 0);
			gGlowCombineFXAAProgram.uniform1i(sScreenMap, 1);
			gGlowCombineFXAAProgram.unbind();
		}
	}

	if (success)
	{
		gOneTextureNoColorProgram.setup("One texture no color shader",
										shader_level,
										"interface/onetexturenocolorV.glsl",
										"interface/onetexturenocolorF.glsl");
		success = gOneTextureNoColorProgram.createShader();
		if (success)
		{
			gOneTextureNoColorProgram.bind();
			gOneTextureNoColorProgram.uniform1i(sTex0, 0);
		}
	}

	if (success)
	{
		gSolidColorProgram.setup("Solid color shader", shader_level,
								 "interface/solidcolorV.glsl",
								 "interface/solidcolorF.glsl");
		success = gSolidColorProgram.createShader();
		if (success)
		{
			gSolidColorProgram.bind();
			gSolidColorProgram.uniform1i(sTex0, 0);
			gSolidColorProgram.unbind();
		}
	}

	if (success)
	{
		gOcclusionProgram.setup("Occlusion shader", shader_level,
								"interface/occlusionV.glsl",
								"interface/occlusionF.glsl");
		success = gOcclusionProgram.createShader();
	}

	if (success)
	{
		gOcclusionCubeProgram.setup("Occlusion cube shader", shader_level,
									"interface/occlusionCubeV.glsl",
									"interface/occlusionF.glsl");
		success = gOcclusionCubeProgram.createShader();
	}

	if (success)
	{
		gDebugProgram.setup("Debug shader", shader_level,
							"interface/debugV.glsl", "interface/debugF.glsl");
		success = gDebugProgram.createShader();
	}

	if (success)
	{
		gClipProgram.setup("Clip shader", shader_level,
						   "interface/clipV.glsl", "interface/clipF.glsl");
		success = gClipProgram.createShader();
	}

	if (success)
	{
		gDownsampleDepthProgram.setup("Downsample depth shader", shader_level,
									  "interface/downsampleDepthV.glsl",
									  "interface/downsampleDepthF.glsl");
		success = gDownsampleDepthProgram.createShader();
	}

	if (success)
	{
		gBenchmarkProgram.setup("Benchmark shader", shader_level,
								"interface/benchmarkV.glsl",
								"interface/benchmarkF.glsl");
		success = gBenchmarkProgram.createShader();
	}

	if (success)
	{
		gDownsampleDepthRectProgram.setup("Downsample depth rect shader",
										  shader_level,
										  "interface/downsampleDepthV.glsl",
										  "interface/downsampleDepthRectF.glsl");
		success = gDownsampleDepthRectProgram.createShader();
	}

	if (success)
	{
		gAlphaMaskProgram.setup("Alpha mask shader", shader_level,
								"interface/alphamaskV.glsl",
								"interface/alphamaskF.glsl");
		success = gAlphaMaskProgram.createShader();
	}

	if (success)
	{
		llinfos << "Interface shaders loaded at level: " << shader_level
				<< llendl;
	}
	else
	{
		mShaderLevel[SHADER_INTERFACE] = 0;
	}

	return success;
}

bool LLViewerShaderMgr::loadShadersWindLight()
{
	S32 shader_level = mShaderLevel[SHADER_WINDLIGHT];

	if (shader_level < 2)
	{
		gWLSkyProgram.unload();
		gWLCloudProgram.unload();
		gWLSunProgram.unload();
		gWLMoonProgram.unload();
		return true;
	}

	gWLSkyProgram.setup("Windlight sky shader", shader_level,
						"windlight/skyV.glsl", "windlight/skyF.glsl");
	if (gUseNewShaders)
	{
		gWLSkyProgram.mFeatures.calculatesAtmospherics = true;
		gWLSkyProgram.mFeatures.hasTransport = true;
		gWLSkyProgram.mFeatures.hasGamma = true;
		gWLSkyProgram.mFeatures.hasSrgb = true;
	}
	gWLSkyProgram.mShaderGroup = LLGLSLShader::SG_SKY;
	bool success = gWLSkyProgram.createShader();

	if (success)
	{
		gWLCloudProgram.setup("Windlight cloud program", shader_level,
							  "windlight/cloudsV.glsl",
							  "windlight/cloudsF.glsl");
		if (gUseNewShaders)
		{
			gWLCloudProgram.mFeatures.calculatesAtmospherics = true;
			gWLCloudProgram.mFeatures.hasTransport = true;
			gWLCloudProgram.mFeatures.hasGamma = true;
			gWLCloudProgram.mFeatures.hasSrgb = true;
			gWLCloudProgram.addConstant(LLGLSLShader::CONST_CLOUD_MOON_DEPTH);
		}
		gWLCloudProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gWLCloudProgram.createShader();
	}

	if (gUseNewShaders && success)
	{
		gWLSunProgram.setup("Windlight Sun program", shader_level,
							"windlight/sunDiscV.glsl",
							"windlight/sunDiscF.glsl");
		gWLSunProgram.mFeatures.calculatesAtmospherics = true;
		gWLSunProgram.mFeatures.hasTransport = true;
		gWLSunProgram.mFeatures.hasGamma = true;
		gWLSunProgram.mFeatures.hasAtmospherics = true;
		gWLSunProgram.mFeatures.isFullbright = true;
		gWLSunProgram.mFeatures.disableTextureIndex = true;
		gWLSunProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gWLSunProgram.createShader();
	}

	if (gUseNewShaders && success)
	{
		gWLMoonProgram.setup("Windlight Moon program", shader_level,
							 "windlight/moonV.glsl",
							 "windlight/moonF.glsl");
		gWLMoonProgram.mFeatures.calculatesAtmospherics = true;
		gWLMoonProgram.mFeatures.hasTransport = true;
		gWLMoonProgram.mFeatures.hasGamma = true;
		gWLMoonProgram.mFeatures.hasAtmospherics = true;
		gWLMoonProgram.mFeatures.isFullbright = true;
		gWLMoonProgram.mFeatures.disableTextureIndex = true;
		gWLMoonProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		gWLMoonProgram.addConstant(LLGLSLShader::CONST_CLOUD_MOON_DEPTH);
		success = gWLMoonProgram.createShader();
	}

	if (success)
	{
		llinfos << "Windlight shaders loaded at level: " << shader_level
				<< llendl;
	}

	return success;
}

//virtual
void LLViewerShaderMgr::updateShaderUniforms(LLGLSLShader* shader)
{
	LL_TRACY_TIMER(TRC_UPD_SHADER_UNIFORMS);

	if (gUseNewShaders)
	{
		gEnvironment.updateShaderUniforms(shader);
	}
	else
	{
		gWLSkyParamMgr.updateShaderUniforms(shader);
		gWLWaterParamMgr.updateShaderUniforms(shader);
	}
}

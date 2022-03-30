/**
 * @file llviewershadermgr.h
 * @brief Viewer Shader Manager
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_VIEWER_SHADER_MGR_H
#define LL_VIEWER_SHADER_MGR_H

#include "llmaterial.h"
#include "llshadermgr.h"

#define LL_DEFERRED_MULTI_LIGHT_COUNT 16

class LLViewerShaderMgr : public LLShaderMgr
{
protected:
	LOG_CLASS(LLViewerShaderMgr);

private:
	// Use createInstance() and releaseInstance()
	LLViewerShaderMgr();
	~LLViewerShaderMgr() override;

	void init();

public:
	static void createInstance();
	static void releaseInstance();

	void initAttribsAndUniforms() override;
	void setShaders();
	void unloadShaders();
	bool loadBasicShaders();
	bool loadShadersEffects();
	bool loadShadersDeferred();
	bool loadShadersObject();
	bool loadShadersAvatar();
	bool loadShadersEnvironment();
	bool loadShadersWater();
	bool loadShadersInterface();
	bool loadShadersWindLight();

	enum EShaderClass
	{
		SHADER_LIGHTING,
		SHADER_OBJECT,
		SHADER_AVATAR,
		SHADER_ENVIRONMENT,
		SHADER_INTERFACE,
		SHADER_EFFECT,
		SHADER_WINDLIGHT,
		SHADER_WATER,
		SHADER_DEFERRED,
		SHADER_COUNT
	};

	LL_INLINE S32 getShaderLevel(S32 type) const
	{
		return mShaderLevel[type];
	}

	// Simple model of forward iterator
	// http://www.sgi.com/tech/stl/ForwardIterator.html
	class shader_iter
	{
	private:
		friend bool operator==(shader_iter const& a, shader_iter const& b);
		friend bool operator!=(shader_iter const& a, shader_iter const& b);

		typedef std::vector<LLGLSLShader*>::const_iterator base_iter_t;

	public:
		shader_iter() = default;

		LL_INLINE shader_iter(base_iter_t iter)
		:	mIter(iter)
		{
		}

		LL_INLINE LLGLSLShader& operator*() const
		{
			return **mIter;
		}

		LL_INLINE LLGLSLShader* operator->() const
		{
			return *mIter;
		}

		LL_INLINE shader_iter& operator++()
		{
			++mIter;
			return *this;
		}

		LL_INLINE shader_iter operator++(int)
		{
			return mIter++;
		}

	private:
		base_iter_t mIter;
	};

	LL_INLINE shader_iter beginShaders() const	{ return mShaderList.begin(); }
	LL_INLINE shader_iter endShaders() const	{ return mShaderList.end(); }

	LL_INLINE const std::string& getShaderDirPrefix() const override
	{
		return mShaderDirPrefix;
	}

	void updateShaderUniforms(LLGLSLShader* shader) override;

private:
	std::string					mShaderDirPrefix;
	// The list of shaders we need to propagate parameters to.
	std::vector<LLGLSLShader*>	mShaderList;

public:
	std::vector<S32>			mShaderLevel;
	S32							mMaxAvatarShaderLevel;

	static bool					sInitialized;
	static bool					sSkipReload;
};

LL_INLINE bool operator==(LLViewerShaderMgr::shader_iter const& a,
						  LLViewerShaderMgr::shader_iter const& b)
{
	return a.mIter == b.mIter;
}

LL_INLINE bool operator!=(LLViewerShaderMgr::shader_iter const& a,
						  LLViewerShaderMgr::shader_iter const& b)
{
	return a.mIter != b.mIter;
}

extern LLViewerShaderMgr* gViewerShaderMgrp;

extern LLVector4 gShinyOrigin;

// Utility shaders
extern LLGLSLShader gOcclusionProgram;
extern LLGLSLShader gOcclusionCubeProgram;
extern LLGLSLShader gCustomAlphaProgram;
extern LLGLSLShader gGlowCombineProgram;
extern LLGLSLShader gSplatTextureRectProgram;
extern LLGLSLShader gGlowCombineFXAAProgram;
extern LLGLSLShader gDebugProgram;
extern LLGLSLShader gClipProgram;
extern LLGLSLShader gDownsampleDepthProgram;
extern LLGLSLShader gDownsampleDepthRectProgram;
extern LLGLSLShader gAlphaMaskProgram;
extern LLGLSLShader gBenchmarkProgram;
extern LLGLSLShader gOneTextureNoColorProgram;

// Object shaders
extern LLGLSLShader gObjectSimpleProgram;
extern LLGLSLShader gObjectSimpleImpostorProgram;
extern LLGLSLShader gObjectPreviewProgram;
extern LLGLSLShader gObjectSimpleAlphaMaskProgram;
extern LLGLSLShader gObjectSimpleWaterProgram;
extern LLGLSLShader gObjectSimpleWaterAlphaMaskProgram;
extern LLGLSLShader gObjectSimpleNonIndexedProgram;
extern LLGLSLShader gObjectSimpleNonIndexedTexGenProgram;
extern LLGLSLShader gObjectSimpleNonIndexedTexGenWaterProgram;
extern LLGLSLShader gObjectSimpleNonIndexedWaterProgram;
extern LLGLSLShader gObjectAlphaMaskNonIndexedProgram;
extern LLGLSLShader gObjectAlphaMaskNonIndexedWaterProgram;
extern LLGLSLShader gObjectAlphaMaskNoColorProgram;
extern LLGLSLShader gObjectAlphaMaskNoColorWaterProgram;
extern LLGLSLShader gObjectFullbrightProgram;
extern LLGLSLShader gObjectFullbrightWaterProgram;
extern LLGLSLShader gObjectFullbrightNoColorProgram;
extern LLGLSLShader gObjectFullbrightNoColorWaterProgram;
extern LLGLSLShader gObjectEmissiveProgram;
extern LLGLSLShader gObjectEmissiveWaterProgram;
extern LLGLSLShader gObjectFullbrightAlphaMaskProgram;
extern LLGLSLShader gObjectFullbrightWaterAlphaMaskProgram;
extern LLGLSLShader gObjectFullbrightNonIndexedProgram;
extern LLGLSLShader gObjectFullbrightNonIndexedWaterProgram;
extern LLGLSLShader gObjectEmissiveNonIndexedProgram;
extern LLGLSLShader gObjectEmissiveNonIndexedWaterProgram;
extern LLGLSLShader gObjectBumpProgram;
extern LLGLSLShader gTreeProgram;
extern LLGLSLShader gTreeWaterProgram;

extern LLGLSLShader gObjectSimpleLODProgram;
extern LLGLSLShader gObjectFullbrightLODProgram;

extern LLGLSLShader gObjectFullbrightShinyProgram;
extern LLGLSLShader gObjectFullbrightShinyWaterProgram;
extern LLGLSLShader gObjectFullbrightShinyNonIndexedProgram;
extern LLGLSLShader gObjectFullbrightShinyNonIndexedWaterProgram;

extern LLGLSLShader gObjectShinyProgram;
extern LLGLSLShader gObjectShinyWaterProgram;
extern LLGLSLShader gObjectShinyNonIndexedProgram;
extern LLGLSLShader gObjectShinyNonIndexedWaterProgram;

extern LLGLSLShader gSkinnedObjectSimpleProgram;
extern LLGLSLShader gSkinnedObjectFullbrightProgram;
extern LLGLSLShader gSkinnedObjectEmissiveProgram;
extern LLGLSLShader gSkinnedObjectFullbrightShinyProgram;
extern LLGLSLShader gSkinnedObjectShinySimpleProgram;

extern LLGLSLShader gSkinnedObjectSimpleWaterProgram;
extern LLGLSLShader gSkinnedObjectFullbrightWaterProgram;
extern LLGLSLShader gSkinnedObjectEmissiveWaterProgram;
extern LLGLSLShader gSkinnedObjectFullbrightShinyWaterProgram;
extern LLGLSLShader gSkinnedObjectShinySimpleWaterProgram;

// Environment shaders
extern LLGLSLShader gMoonProgram;
extern LLGLSLShader gStarsProgram;
extern LLGLSLShader gTerrainProgram;
extern LLGLSLShader gTerrainWaterProgram;
extern LLGLSLShader gWaterProgram;
extern LLGLSLShader gWaterEdgeProgram;
extern LLGLSLShader gUnderWaterProgram;
extern LLGLSLShader gGlowProgram;
extern LLGLSLShader gGlowExtractProgram;

// Interface shaders
extern LLGLSLShader gHighlightProgram;
extern LLGLSLShader gHighlightNormalProgram;
extern LLGLSLShader gHighlightSpecularProgram;

// Avatar shader handles
extern LLGLSLShader gAvatarProgram;
extern LLGLSLShader gAvatarWaterProgram;
extern LLGLSLShader gAvatarEyeballProgram;
extern LLGLSLShader gAvatarPickProgram;
extern LLGLSLShader gImpostorProgram;

// WindLight shader handles
extern LLGLSLShader gWLSkyProgram;
extern LLGLSLShader gWLCloudProgram;
extern LLGLSLShader gWLSunProgram;
extern LLGLSLShader gWLMoonProgram;

// Post Process Shaders
extern LLGLSLShader gPostColorFilterProgram;
extern LLGLSLShader gPostNightVisionProgram;

// Deferred rendering shaders
extern LLGLSLShader gDeferredImpostorProgram;
extern LLGLSLShader gDeferredWaterProgram;
extern LLGLSLShader gDeferredUnderWaterProgram;
extern LLGLSLShader gDeferredDiffuseProgram;
extern LLGLSLShader gDeferredDiffuseAlphaMaskProgram;
extern LLGLSLShader gDeferredNonIndexedDiffuseAlphaMaskProgram;
extern LLGLSLShader gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram;
extern LLGLSLShader gDeferredNonIndexedDiffuseProgram;
extern LLGLSLShader gDeferredSkinnedDiffuseProgram;
extern LLGLSLShader gDeferredSkinnedBumpProgram;
extern LLGLSLShader gDeferredSkinnedAlphaProgram;
extern LLGLSLShader gDeferredBumpProgram;
extern LLGLSLShader gDeferredTerrainProgram;
extern LLGLSLShader gDeferredTerrainWaterProgram;
extern LLGLSLShader gDeferredTreeProgram;
extern LLGLSLShader gDeferredTreeShadowProgram;
extern LLGLSLShader gDeferredLightProgram;
extern LLGLSLShader gDeferredMultiLightProgram[LL_DEFERRED_MULTI_LIGHT_COUNT];
extern LLGLSLShader gDeferredSpotLightProgram;
extern LLGLSLShader gDeferredMultiSpotLightProgram;
extern LLGLSLShader gDeferredSunProgram;
extern LLGLSLShader gDeferredBlurLightProgram;
extern LLGLSLShader gDeferredAvatarProgram;
extern LLGLSLShader gDeferredSoftenProgram;
extern LLGLSLShader gDeferredSoftenWaterProgram;
extern LLGLSLShader gDeferredShadowProgram;
extern LLGLSLShader gDeferredShadowCubeProgram;
extern LLGLSLShader gDeferredShadowAlphaMaskProgram;
extern LLGLSLShader gDeferredShadowFullbrightAlphaMaskProgram;
extern LLGLSLShader gDeferredPostProgram;
extern LLGLSLShader gDeferredCoFProgram;
extern LLGLSLShader gDeferredDoFCombineProgram;
extern LLGLSLShader gFXAAProgram;
extern LLGLSLShader gDeferredPostNoDoFProgram;
extern LLGLSLShader gDeferredPostGammaCorrectProgram;
extern LLGLSLShader gDeferredAvatarShadowProgram;
extern LLGLSLShader gDeferredAttachmentShadowProgram;
extern LLGLSLShader gDeferredAttachmentAlphaShadowProgram;
extern LLGLSLShader gDeferredAttachmentAlphaMaskShadowProgram;
extern LLGLSLShader gDeferredAvatarAlphaShadowProgram;
extern LLGLSLShader gDeferredAvatarAlphaMaskShadowProgram;
extern LLGLSLShader gDeferredAlphaProgram;
extern LLGLSLShader gDeferredAlphaImpostorProgram;
extern LLGLSLShader gDeferredFullbrightProgram;
extern LLGLSLShader gDeferredFullbrightAlphaMaskProgram;
extern LLGLSLShader gDeferredAlphaWaterProgram;
extern LLGLSLShader gDeferredFullbrightWaterProgram;
extern LLGLSLShader gDeferredFullbrightAlphaMaskWaterProgram;
extern LLGLSLShader gDeferredEmissiveProgram;
extern LLGLSLShader gDeferredAvatarEyesProgram;
extern LLGLSLShader gDeferredAvatarAlphaProgram;
extern LLGLSLShader gDeferredWLSkyProgram;
extern LLGLSLShader gDeferredWLCloudProgram;
extern LLGLSLShader gDeferredWLSunProgram;
extern LLGLSLShader gDeferredWLMoonProgram;
extern LLGLSLShader gDeferredStarProgram;
extern LLGLSLShader gDeferredFullbrightShinyProgram;
extern LLGLSLShader gDeferredSkinnedFullbrightShinyProgram;
extern LLGLSLShader gDeferredSkinnedFullbrightProgram;
extern LLGLSLShader gNormalMapGenProgram;

// Deferred materials shaders
extern LLGLSLShader gDeferredMaterialProgram[LLMaterial::SHADER_COUNT * 2];
extern LLGLSLShader gDeferredMaterialWaterProgram[LLMaterial::SHADER_COUNT * 2];

#endif	// LL_VIEWER_SHADER_MGR_H

/**
 * @file lldrawpoolwater.cpp
 * @brief LLDrawPoolWater class implementation
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

#include <array>

#include "lldrawpoolwater.h"

#include "imageids.h"
#include "llcubemap.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llrender.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llenvironment.h"
#include "llface.h"
#include "llfeaturemanager.h"
#include "llgridmanager.h"			// For gIsInSecondLife
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llsky.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llvowater.h"
#include "llwlwaterparammgr.h"
#include "llworld.h"

static F32 sTime = 0.f;

bool sDeferredRender = false;

bool LLDrawPoolWater::sNeedsReflectionUpdate = true;
bool LLDrawPoolWater::sNeedsTexturesReload = true;
LLColor4 LLDrawPoolWater::sWaterFogColor = LLColor4(0.2f, 0.5f, 0.5f, 0.f);

LLDrawPoolWater::LLDrawPoolWater()
:	LLFacePool(POOL_WATER)
{
}

// NOTE: must be called *after* setTransparentTextures() when gUseNewShaders is
// false (since mOpaqueWaterImagep may be set equal to mWaterImagep[0]).
void LLDrawPoolWater::setOpaqueTexture(const LLUUID& tex_id)
{
	if (mOpaqueWaterImagep && mOpaqueWaterImagep->getID() == tex_id)
	{
		// Nothing to do !
		return;
	}
	if (tex_id == DEFAULT_WATER_OPAQUE || tex_id.isNull())
	{
		mOpaqueWaterImagep = LLViewerFetchedTexture::sOpaqueWaterImagep;
	}
	else
	{
		mOpaqueWaterImagep = LLViewerTextureManager::getFetchedTexture(tex_id);
		mOpaqueWaterImagep->setNoDelete();
	}
	mOpaqueWaterImagep->addTextureStats(1024.f * 1024.f);
}

void LLDrawPoolWater::setTransparentTextures(const LLUUID& tex1_id,
											 const LLUUID& tex2_id)
{
	if (!mWaterImagep[0] || mWaterImagep[0]->getID() != tex1_id)
	{
		if (tex1_id == DEFAULT_WATER_TEXTURE || tex1_id.isNull())
		{
			mWaterImagep[0] = LLViewerFetchedTexture::sWaterImagep;
		}
		else
		{
			mWaterImagep[0] =
				LLViewerTextureManager::getFetchedTexture(tex1_id);
		}
		mWaterImagep[0]->setNoDelete();
		mWaterImagep[0]->addTextureStats(1024.f * 1024.f);
	}

	if (!gUseNewShaders)
	{
		// No such texture for Windlight...
		mWaterImagep[1] = NULL;
		return;
	}

	if (mWaterImagep[1] && mWaterImagep[1]->getID() == tex2_id)
	{
		// Nothing left to do
		return;
	}
	if (tex2_id.notNull())
	{
		mWaterImagep[1] = LLViewerTextureManager::getFetchedTexture(tex2_id);
	}
	else
	{
		// Use the same texture as the first one...
		mWaterImagep[1] = mWaterImagep[0];
	}
	mWaterImagep[1]->setNoDelete();
	mWaterImagep[1]->addTextureStats(1024.f * 1024.f);
}

void LLDrawPoolWater::setNormalMaps(const LLUUID& tex1_id,
									const LLUUID& tex2_id)
{
	if (!mWaterNormp[0] || mWaterNormp[0]->getID() != tex1_id)
	{
		if (tex1_id == DEFAULT_WATER_NORMAL || tex1_id.isNull())
		{
			mWaterNormp[0] = LLViewerFetchedTexture::sWaterNormapMapImagep;
		}
		else
		{
			mWaterNormp[0] =
				LLViewerTextureManager::getFetchedTexture(tex1_id);
		}
		mWaterNormp[0]->setNoDelete();
		mWaterNormp[0]->addTextureStats(1024.f * 1024.f);
	}

	if (!gUseNewShaders)
	{
		// No such texture for Windlight...
		mWaterNormp[1] = NULL;
		return;
	}

	if (mWaterNormp[1] && mWaterNormp[1]->getID() == tex2_id)
	{
		// Nothing left to do
		return;
	}
	if (tex2_id.notNull())
	{
		mWaterNormp[1] = LLViewerTextureManager::getFetchedTexture(tex2_id);
	}
	else
	{
		// Use the same texture as the first one...
		mWaterNormp[1] = mWaterNormp[0];
	}
	mWaterNormp[1]->setNoDelete();
	mWaterNormp[1]->addTextureStats(1024.f * 1024.f);
}

//static
void LLDrawPoolWater::restoreGL()
{
	sNeedsReflectionUpdate = sNeedsTexturesReload = true;
}

void LLDrawPoolWater::prerender()
{
	if (gGLManager.mHasCubeMap && LLCubeMap::sUseCubeMaps)
	{
		mShaderLevel =
			gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_WATER);
	}
	else
	{
		mShaderLevel = 0;
	}

	if (sNeedsTexturesReload)
	{
		sNeedsTexturesReload = false;
		if (gUseNewShaders || LLEnvironment::overrideWindlight())
		{
			const LLSettingsWater::ptr_t& waterp =
				gEnvironment.getCurrentWater();
			if (waterp)
			{
				setTransparentTextures(waterp->getTransparentTextureID(),
									   waterp->getNextTransparentTextureID());
				setOpaqueTexture(waterp->getDefaultOpaqueTextureAssetId());
				setNormalMaps(waterp->getNormalMapID(),
							  waterp->getNextNormalMapID());
			}
		}
		else
		{
			setTransparentTextures(DEFAULT_WATER_TEXTURE, LLUUID::null);
			setOpaqueTexture(DEFAULT_WATER_OPAQUE);
			setNormalMaps(DEFAULT_WATER_NORMAL, LLUUID::null);
		}
	}

	LLVOSky* voskyp = gSky.mVOSkyp;
	if (!voskyp) return;

	if (gUseNewShaders)
	{
		mLightDir = gEnvironment.getLightDirection();
		mLightDir.normalize();
		mIsSunUp = gEnvironment.getIsSunUp();
		const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();
		if (skyp)
		{
			if (mIsSunUp)
			{
				mLightDiffuse = voskyp->getSun().getColorCached();
				mLightColor = skyp->getSunlightColor();
			}
			else if (gEnvironment.getIsMoonUp())
			{
				mLightDiffuse = skyp->getMoonDiffuse();
				mLightColor = skyp->getMoonlightColor();
			}
		}
		mLightDiffuse.normalize();
		mLightExp = mLightDir *
					LLVector3(mLightDir.mV[0], mLightDir.mV[1], 0.f);
		mLightDiffuse *= mLightExp + 0.25f;

		const LLSettingsWater::ptr_t& waterp = gEnvironment.getCurrentWater();
		if (waterp)
		{
			sWaterFogColor = LLColor4(waterp->getWaterFogColor(), 0.f);
		}
	}
	else
	{
		mIsSunUp = gSky.sunUp();
		if (mIsSunUp)
		{
			mLightDir = gSky.getSunDirection();
			mLightDir.normalize();
			mLightDiffuse = voskyp->getSun().getColorCached();
			mLightDiffuse.normalize();
			mLightExp = mLightDir *
						LLVector3(mLightDir.mV[0], mLightDir.mV[1], 0.f);
			mLightDiffuse *= mLightExp + 0.25f;
		}
		else
		{
			mLightDir = gSky.getMoonDirection();
			mLightDir.normalize();
			mLightDiffuse = voskyp->getMoon().getColorCached();
			mLightDiffuse.normalize();
			mLightDiffuse *= 0.5f;
			mLightExp = mLightDir *
						LLVector3(mLightDir.mV[0], mLightDir.mV[1], 0.f);
		}

		// Got rid of modulation by light color since it got a little too green
		// at sunset and SL-57047 (underwater turns black at 08:00)
		sWaterFogColor = gWLWaterParamMgr.getFogColor();
		sWaterFogColor.mV[3] = 0.f;
	}

	mLightExp *= mLightExp;
	mLightExp *= mLightExp;
	mLightExp *= mLightExp;
	mLightExp *= mLightExp;
	mLightExp *= 256.f;
	if (mLightExp < 32.f)
	{
		mLightExp = 32.f;
	}

	mLightDiffuse *= 6.f;
}

S32 LLDrawPoolWater::getNumPasses()
{
	return gViewerCamera.getOrigin().mV[2] < 1024.f ? 1 : 0;
}

void LLDrawPoolWater::beginPostDeferredPass(S32 pass)
{
	beginRenderPass(pass);
	sDeferredRender = true;
}

void LLDrawPoolWater::endPostDeferredPass(S32 pass)
{
	endRenderPass(pass);
	sDeferredRender = false;
}

void LLDrawPoolWater::renderDeferred(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_WATER);
	if (!LLPipeline::sRenderTransparentWater)
	{
		// Render opaque water without use of ALM
		render(pass);
		return;
	}
	sDeferredRender = true;
	renderWater();
	sDeferredRender = false;
}

void LLDrawPoolWater::render(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_WATER);
	if (mDrawFace.empty() || LLDrawable::getCurrentFrame() <= 1)
	{
		return;
	}

	// Do a quick'n dirty depth sort
	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin(),
										end = mDrawFace.end();
		 iter != end; ++iter)
	{
		LLFace* facep = *iter;
		facep->mDistance = -facep->mCenterLocal.mV[2];
	}

	std::sort(mDrawFace.begin(), mDrawFace.end(),
			  LLFace::CompareDistanceGreater());

	if (!LLPipeline::sRenderTransparentWater
//MK
		|| (gRLenabled && gRLInterface.mContainsCamTextures))
//mk
	{
		// Render water for low end hardware
		renderOpaqueLegacyWater();
		return;
	}

	LLGLEnable blend(GL_BLEND);

	if (mShaderLevel > 0)
	{
		renderWater();
		return;
	}

	if (!gGLManager.mHasMultitexture)
	{
		// Ack !  No multitexture !  Bail !
		return;
	}

	LLVOSky* voskyp = gSky.mVOSkyp;
	if (!voskyp)
	{
		return;
	}

	LLFace* refl_face = voskyp->getReflFace();

	gPipeline.disableLights();

	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);

	LLGLDisable cull_face(GL_CULL_FACE);

	// Set up second pass first
	LLTexUnit* unit1 = gGL.getTexUnit(1);
	unit1->activate();
	unit1->enable(LLTexUnit::TT_TEXTURE);
	unit1->bind(mWaterImagep[0]);

	LLTexUnit* unit2 = NULL;
	if (gUseNewShaders)
	{
		unit2 = gGL.getTexUnit(2);
		unit2->activate();
		unit2->enable(LLTexUnit::TT_TEXTURE);
		unit2->bind(mWaterImagep[1]);
	}

	const LLVector3& camera_up = gViewerCamera.getUpAxis();
	F32 up_dot = camera_up * LLVector3::z_axis;

	LLColor4 water_color;
	if (gViewerCamera.cameraUnderWater())
	{
		water_color.set(1.f, 1.f, 1.f, 0.4f);
	}
	else
	{
		water_color.set(1.f, 1.f, 1.f, 0.5f + 0.5f * up_dot);
	}

	gGL.diffuseColor4fv(water_color.mV);

	// Automatically generate texture coords for detail map
	glEnable(GL_TEXTURE_GEN_S); // Texture unit 1
	glEnable(GL_TEXTURE_GEN_T); // Texture unit 1
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

	// Slowly move over time.
	static F32 frame_time = 0.f;
	if (!LLPipeline::sFreezeTime)
	{
		frame_time = gFrameTimeSeconds;
	}
	F32 offset = fmod(frame_time * 2.f, 100.f);
	F32 tp0[4] = { 16.f / 256.f, 0.f, 0.f, offset * 0.01f };
	F32 tp1[4] = { 0.f, 16.f / 256.f, 0.f, offset * 0.01f };
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1);

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->activate();

	glClearStencil(1);
	glClear(GL_STENCIL_BUFFER_BIT);
	glClearStencil(0);
	LLGLEnable gls_stencil(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
	glStencilFunc(GL_ALWAYS, 0, 0xFFFFFFFF);

	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin(),
										end = mDrawFace.end();
		 iter != end; ++iter)
	{
		LLFace* facep = *iter;
		if (!voskyp->isReflFace(facep))
		{
			LLViewerTexture* tex = facep->getTexture();
			if (tex && tex->hasGLTexture())
			{
				unit0->bind(tex);
				facep->renderIndexed();
			}
		}
	}

	// Now, disable texture coord generation on texture state 1
	unit1->activate();
	unit1->unbind(LLTexUnit::TT_TEXTURE);
	unit1->disable();
	glDisable(GL_TEXTURE_GEN_S); // Texture unit 1
	glDisable(GL_TEXTURE_GEN_T); // Texture unit 1

	if (unit2)
	{
		unit2->activate();
		unit2->unbind(LLTexUnit::TT_TEXTURE);
		unit2->disable();
		glDisable(GL_TEXTURE_GEN_S); // Texture unit 2
		glDisable(GL_TEXTURE_GEN_T); // Texture unit 2
	}

	// Disable texture coordinate and color arrays
	unit0->activate();
	unit0->unbind(LLTexUnit::TT_TEXTURE);

	if (voskyp->getCubeMap())
	{
		voskyp->getCubeMap()->enableTexture(0);
		voskyp->getCubeMap()->bind();

		gGL.matrixMode(LLRender::MM_TEXTURE);
		gGL.loadIdentity();
		LLMatrix4 camera_mat = gViewerCamera.getModelview();
		LLMatrix4 camera_rot(camera_mat.getMat3());
		camera_rot.invert();

		gGL.loadMatrix(camera_rot.getF32ptr());

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		LLOverrideFaceColor overrid(this, 1.f, 1.f, 1.f, 0.5f * up_dot);

		for (std::vector<LLFace*>::iterator iter = mDrawFace.begin(),
											end = mDrawFace.end();
			 iter != end; ++iter)
		{
			LLFace* face = *iter;
			if (!voskyp->isReflFace(face) && face->getGeomCount() > 0)
			{
				face->renderIndexed();
			}
		}

		voskyp->getCubeMap()->disableTexture();

		unit0->unbind(LLTexUnit::TT_TEXTURE);
		unit0->enable(LLTexUnit::TT_TEXTURE);
		gGL.matrixMode(LLRender::MM_TEXTURE);
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}

	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	if (refl_face)
	{
		glStencilFunc(GL_NOTEQUAL, 0, 0xFFFFFFFF);
		renderReflection(refl_face);
	}

	stop_glerror();
}

// For low end hardware
void LLDrawPoolWater::renderOpaqueLegacyWater()
{
	LLVOSky* voskyp = gSky.mVOSkyp;
	if (!voskyp)
	{
		return;
	}

	LLGLSLShader* shader = NULL;
	if (LLPipeline::sUnderWaterRender)
	{
		shader = &gObjectSimpleNonIndexedTexGenWaterProgram;
	}
	else
	{
		shader = &gObjectSimpleNonIndexedTexGenProgram;
	}
	shader->bind();

	// Depth sorting and write to depth buffer since this is opaque, we should
	// see nothing behind the water. No blending because of no transparency.
	// And no face culling so that the underside of the water is also opaque.
	LLGLDepthTest gls_depth(GL_TRUE, GL_TRUE);
	LLGLDisable no_cull(GL_CULL_FACE);
	LLGLDisable no_blend(GL_BLEND);

	gPipeline.disableLights();

	// Activate the texture binding and bind one texture since all images will
	// have the same texture
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->activate();
	unit0->enable(LLTexUnit::TT_TEXTURE);
//MK
	if (gRLenabled && gRLInterface.mContainsCamTextures &&
		gRLInterface.mCamTexturesCustom)
	{
		unit0->bind(gRLInterface.mCamTexturesCustom);
	}
	else
//mk
	{
		unit0->bind(mOpaqueWaterImagep);
	}

	// Automatically generate texture coords for water texture
	if (!shader)
	{
		glEnable(GL_TEXTURE_GEN_S); // Texture unit 0
		glEnable(GL_TEXTURE_GEN_T); // Texture unit 0
		glTexGenf(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
		glTexGenf(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	}

	// Use the fact that we know all water faces are the same size to save some
	// computation

	// Slowly move texture coordinates over time so the watter appears to be
	// moving.
	F32 movement_period_secs = 50.f;
	// Slowly move over time.
	static F32 frame_time = 0.f;
	if (!LLPipeline::sFreezeTime)
	{
		frame_time = gFrameTimeSeconds;
	}
	F32 offset = fmod(frame_time, movement_period_secs);

	if (movement_period_secs != 0)
	{
	 	offset /= movement_period_secs;
	}
	else
	{
		offset = 0;
	}

	F32 tp0[4] = { 16.f / 256.f, 0.f, 0.f, offset };
	F32 tp1[4] = { 0.f, 16.f / 256.f, 0.f, offset };

	if (!shader)
	{
		glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0);
		glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1);
	}
	else
	{
		shader->uniform4fv(LLShaderMgr::OBJECT_PLANE_S, 1, tp0);
		shader->uniform4fv(LLShaderMgr::OBJECT_PLANE_T, 1, tp1);
	}

	gGL.diffuseColor3f(1.f, 1.f, 1.f);

	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin(),
										end = mDrawFace.end();
		 iter != end; ++iter)
	{
		LLFace* face = *iter;
		if (!voskyp->isReflFace(face))
		{
			face->renderIndexed();
		}
	}

	if (!shader)
	{
		// Reset the settings back to expected values
		glDisable(GL_TEXTURE_GEN_S); // Texture unit 0
		glDisable(GL_TEXTURE_GEN_T); // Texture unit 0
	}

	unit0->unbind(LLTexUnit::TT_TEXTURE);

	stop_glerror();
}

void LLDrawPoolWater::renderReflection(LLFace* face)
{
	LLVOSky* voskyp = gSky.mVOSkyp;
	if (!voskyp || !face->getGeomCount())
	{
		return;
	}

	S8 dr = voskyp->getDrawRefl();
	if (dr < 0)
	{
		return;
	}

	LLGLSNoFog noFog;

	gGL.getTexUnit(0)->bind(dr == 0 ? voskyp->getSunTex()
									: voskyp->getMoonTex());

	LLOverrideFaceColor override_color(this, face->getFaceColor().mV);
	face->renderIndexed();
}

void LLDrawPoolWater::renderWater()
{
	LLVOSky* voskyp = gSky.mVOSkyp;
	if (!voskyp || !gAgent.getRegion())
	{
		return;
	}

	if (!sDeferredRender)
	{
		gGL.setColorMask(true, true);
	}

	LLGLDisable blend(GL_BLEND);

	static LLCachedControl<bool> fast_edges(gSavedSettings,
											"RenderWaterFastEdges");

	LLGLSLShader* shader;
    LLGLSLShader* edge_shader = NULL;

	F32 eyedepth = gViewerCamera.getOrigin().mV[2] -
				   gAgent.getRegion()->getWaterHeight();
	if (eyedepth < 0.f && LLPipeline::sWaterReflections)
	{
		if (sDeferredRender)
		{
			shader = &gDeferredUnderWaterProgram;
		}
		else
		{
			shader = &gUnderWaterProgram;
		}
	}
	else if (sDeferredRender)
	{
		shader = &gDeferredWaterProgram;
	}
	else
	{
		shader = &gWaterProgram;
		if (gUseNewShaders && !fast_edges)
		{
			edge_shader = &gWaterEdgeProgram;
		}
	}

	if (!gUseNewShaders)
	{
		// Change mWaterNormp[0] if needed
		if (mWaterNormp[0]->getID() != gWLWaterParamMgr.getNormalMapID())
		{
			mWaterNormp[0] =
				LLViewerTextureManager::getFetchedTexture(gWLWaterParamMgr.getNormalMapID(),
														  FTT_DEFAULT, true,
														  LLGLTexture::BOOST_BUMP);
		}
	}

	static LLCachedControl<bool> render_water_mip_normal(gSavedSettings,
														 "RenderWaterMipNormal");
	if (mWaterNormp[0])
	{
		if (render_water_mip_normal)
		{
			mWaterNormp[0]->setFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
		}
		else
		{
			mWaterNormp[0]->setFilteringOption(LLTexUnit::TFO_POINT);
		}
	}
	if (gUseNewShaders && mWaterNormp[1])
	{
		if (render_water_mip_normal)
		{
			mWaterNormp[1]->setFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
		}
		else
		{
			mWaterNormp[1]->setFilteringOption(LLTexUnit::TFO_POINT);
		}
	}

	stop_glerror();

	if (gUseNewShaders)
	{
		renderWaterEE(shader, false, eyedepth);
		if (!fast_edges)
		{
			renderWaterEE(edge_shader ? edge_shader : shader, true, eyedepth);
		}
	}
	else
	{
		renderWaterWL(shader, eyedepth);
	}

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->activate();
	unit0->enable(LLTexUnit::TT_TEXTURE);
	if (!sDeferredRender)
	{
		gGL.setColorMask(true, false);
	}

	stop_glerror();
}

void LLDrawPoolWater::renderWaterWL(LLGLSLShader* shader, F32 eyedepth)
{
	if (sDeferredRender)
	{
		gPipeline.bindDeferredShader(*shader);
	}
	else
	{
		shader->bind();
	}

	if (!LLPipeline::sFreezeTime)
	{
		sTime = (F32)LLFrameTimer::getElapsedSeconds() * 0.5f;
	}

	S32 reftex = shader->enableTexture(LLShaderMgr::WATER_REFTEX);
	if (reftex > -1)
	{
		LLTexUnit* unit = gGL.getTexUnit(reftex);
		unit->activate();
		unit->bind(&gPipeline.mWaterRef);
		gGL.getTexUnit(0)->activate();
	}

	// Bind normal map
	S32 bump_tex = shader->enableTexture(LLShaderMgr::BUMP_MAP);
	gGL.getTexUnit(bump_tex)->bind(mWaterNormp[0]);

	S32 screentex = shader->enableTexture(LLShaderMgr::WATER_SCREENTEX);
	if (screentex > -1)
	{
		shader->uniform4fv(LLShaderMgr::WATER_FOGCOLOR, 1, sWaterFogColor.mV);
		shader->uniform1f(LLShaderMgr::WATER_FOGDENSITY,
						  gWLWaterParamMgr.getFogDensity());
		gPipeline.mWaterDis.bindTexture(0, screentex);
		gGL.getTexUnit(screentex)->bind(&gPipeline.mWaterDis);
	}

	if (mShaderLevel == 1)
	{
		sWaterFogColor.mV[3] = gWLWaterParamMgr.mDensitySliderValue;
		shader->uniform4fv(LLShaderMgr::WATER_FOGCOLOR, 1, sWaterFogColor.mV);
	}

	F32 screen_res[] =
	{
		1.f / gGLViewport[2],
		1.f / gGLViewport[3]
	};
	shader->uniform2fv(LLShaderMgr::DEFERRED_SCREEN_RES, 1, screen_res);

	// NOTE: there is actually no such uniform in the water shaders, so
	// diff_tex is set to -1...
	S32 diff_tex = shader->enableTexture(LLShaderMgr::DIFFUSE_MAP);

	shader->uniform1f(LLShaderMgr::WATER_WATERHEIGHT, eyedepth);
	shader->uniform1f(LLShaderMgr::WATER_TIME, sTime);
	const LLVector3& camera_origin = gViewerCamera.getOrigin();
	shader->uniform3fv(LLShaderMgr::WATER_EYEVEC, 1, camera_origin.mV);
	shader->uniform3fv(LLShaderMgr::WATER_SPECULAR, 1, mLightDiffuse.mV);
	shader->uniform2fv(LLShaderMgr::WATER_WAVE_DIR1, 1,
					   gWLWaterParamMgr.getWave1Dir().mV);
	shader->uniform2fv(LLShaderMgr::WATER_WAVE_DIR2, 1,
					   gWLWaterParamMgr.getWave2Dir().mV);
	shader->uniform3fv(LLShaderMgr::WATER_LIGHT_DIR, 1, mLightDir.mV);

	shader->uniform3fv(LLShaderMgr::WATER_NORM_SCALE, 1,
					   gWLWaterParamMgr.getNormalScale().mV);
	shader->uniform1f(LLShaderMgr::WATER_FRESNEL_SCALE,
					  gWLWaterParamMgr.getFresnelScale());
	shader->uniform1f(LLShaderMgr::WATER_FRESNEL_OFFSET,
					  gWLWaterParamMgr.getFresnelOffset());
	shader->uniform1f(LLShaderMgr::WATER_BLUR_MULTIPLIER,
					  gWLWaterParamMgr.getBlurMultiplier());

	F32 sun_angle = llmax(0.f, mLightDir.mV[2]);
	shader->uniform1f(LLShaderMgr::WATER_SUN_ANGLE, 0.1f + 0.2f * sun_angle);

	LLColor4 water_color;
	const LLVector3& camera_up = gViewerCamera.getUpAxis();
	F32 up_dot = camera_up * LLVector3::z_axis;
	if (gViewerCamera.cameraUnderWater())
	{
		water_color.set(1.f, 1.f, 1.f, 0.4f);
		shader->uniform1f(LLShaderMgr::WATER_REFSCALE,
						  gWLWaterParamMgr.getScaleBelow());
	}
	else
	{
		water_color.set(1.f, 1.f, 1.f, 0.5f + 0.5f * up_dot);
		shader->uniform1f(LLShaderMgr::WATER_REFSCALE,
						  gWLWaterParamMgr.getScaleAbove());
	}

	if (water_color.mV[3] > 0.9f)
	{
		water_color.mV[3] = 0.9f;
	}

#if LL_OPTIMIZED_WATER_OCCLUSIONS
	// *HACK: fix for a render glitch where reflections are not updated often
	// enough while flying up/down without rotating. HB
	static F32 last_altitude = -1.f;
	static F32 last_update = 0.f;
	if (camera_origin.mV[2] != last_altitude &&
		gFrameTimeSeconds - last_update > 0.01f)
	{
		last_altitude = camera_origin.mV[2];
		last_update = gFrameTimeSeconds;
		if (gAgent.getFlying())
		{
			sNeedsReflectionUpdate = true;
		}
	}
#endif

	LLVOSky* voskyp = gSky.mVOSkyp;
	if (voskyp)
	{
		LLGLEnable depth_clamp(gGLManager.mHasDepthClamp ? GL_DEPTH_CLAMP : 0);
		LLGLDisable cullface(GL_CULL_FACE);
		LLTexUnit* unit = diff_tex > -1 ? gGL.getTexUnit(diff_tex) : NULL;
		for (std::vector<LLFace*>::iterator iter = mDrawFace.begin(),
											end = mDrawFace.end();
			iter != end; ++iter)
		{
			LLFace* face = *iter;

			if (voskyp->isReflFace(face))
			{
				continue;
			}

			LLVOWater* water = (LLVOWater*)face->getViewerObject();
			if (!water) continue;

			if (unit)
			{
				unit->bind(face->getTexture());
			}

			bool edge_patch = water->getIsEdgePatch();
#if LL_OPTIMIZED_WATER_OCCLUSIONS
			// If occlusion + water culling enabled, this is set within
			// LLOcclusionCullingGroup::checkOcclusion()
			if (!edge_patch &&
				(!LLPipeline::sRenderWaterCull || !LLPipeline::sUseOcclusion))
#else
			if (!edge_patch)
#endif
			{
				sNeedsReflectionUpdate = true;
			}

			if (!edge_patch || sDeferredRender || water->getUseTexture() ||
				gGLManager.mHasDepthClamp)
			{
				face->renderIndexed();
			}
			else
			{
				LLGLSquashToFarClip far_clip;
				face->renderIndexed();
			}
		}
	}

	shader->disableTexture(LLShaderMgr::ENVIRONMENT_MAP,
						   LLTexUnit::TT_CUBE_MAP);
	shader->disableTexture(LLShaderMgr::WATER_SCREENTEX);
	shader->disableTexture(LLShaderMgr::BUMP_MAP);
	shader->disableTexture(LLShaderMgr::DIFFUSE_MAP);
	shader->disableTexture(LLShaderMgr::WATER_REFTEX);

	if (sDeferredRender)
	{
		gPipeline.unbindDeferredShader(*shader);
	}
	else
	{
		shader->unbind();
	}
	stop_glerror();
}

void LLDrawPoolWater::renderWaterEE(LLGLSLShader* shader, bool edge,
									F32 eyedepth)
{
	const LLSettingsWater::ptr_t& waterp = gEnvironment.getCurrentWater();

	shader->bind();

	if (sDeferredRender &&
		shader->getUniformLocation(LLShaderMgr::DEFERRED_NORM_MATRIX) >= 0)
	{
		LLMatrix4a norm_mat = gGLModelView;
		norm_mat.invert();
		norm_mat.transpose();
		shader->uniformMatrix4fv(LLShaderMgr::DEFERRED_NORM_MATRIX, 1,
								 GL_FALSE, norm_mat.getF32ptr());
	}

	shader->uniform4fv(LLShaderMgr::SPECULAR_COLOR, 1, mLightColor.mV);

	if (!LLPipeline::sFreezeTime)
	{
		sTime = (F32)LLFrameTimer::getElapsedSeconds() * 0.5f;
	}

	S32 reftex = shader->enableTexture(LLShaderMgr::WATER_REFTEX);
	if (reftex > -1)
	{
		LLTexUnit* unit = gGL.getTexUnit(reftex);
		unit->activate();
		unit->bind(&gPipeline.mWaterRef);
		gGL.getTexUnit(0)->activate();
	}

	// Bind normal map
	S32 bump_tex = shader->enableTexture(LLShaderMgr::BUMP_MAP);
	LLTexUnit* unitbump = gGL.getTexUnit(bump_tex);
	unitbump->unbind(LLTexUnit::TT_TEXTURE);
	S32 bump_tex2 = shader->enableTexture(LLShaderMgr::BUMP_MAP2);
	LLTexUnit* unitbump2 = bump_tex2 > -1 ? gGL.getTexUnit(bump_tex2) : NULL;
	if (unitbump2)
	{
		unitbump2->unbind(LLTexUnit::TT_TEXTURE);
	}

	LLViewerTexture* tex_a = mWaterNormp[0];
	LLViewerTexture* tex_b = mWaterNormp[1];

	F32 blend_factor = waterp->getBlendFactor();
	if (tex_a && (!tex_b || tex_a == tex_b))
	{
		unitbump->bind(tex_a);
		blend_factor = 0.f;	// Only one tex provided, no blending
	}
	else if (tex_b && !tex_a)
	{
		unitbump->bind(tex_b);
		blend_factor = 0.f;	// Only one tex provided, no blending
	}
	else if (tex_a != tex_b)
	{
		unitbump->bind(tex_a);
		if (unitbump2)
		{
			unitbump2->bind(tex_b);
		}
	}

	// Bind reflection texture from render target
	S32 screentex = shader->enableTexture(LLShaderMgr::WATER_SCREENTEX);
	// NOTE: there is actually no such uniform in the current water shaders, so
	// diff_tex is set to -1...
	S32 diff_tex = shader->enableTexture(LLShaderMgr::DIFFUSE_MAP);

	// Set uniforms for water rendering
	F32 screen_res[] =
	{
		1.f / gGLViewport[2],
		1.f / gGLViewport[3]
	};
	shader->uniform2fv(LLShaderMgr::DEFERRED_SCREEN_RES, 1, screen_res);
	shader->uniform1f(LLShaderMgr::BLEND_FACTOR, blend_factor);

	LLColor4 fog_color = sWaterFogColor;
	F32 fog_density = waterp->getModifiedWaterFogDensity(eyedepth <= 0.f);
	if (screentex > -1)
	{
		shader->uniform1f(LLShaderMgr::WATER_FOGDENSITY, fog_density);
		gGL.getTexUnit(screentex)->bind(&gPipeline.mWaterDis);
	}
	if (mShaderLevel == 1)
	{
		fog_color.mV[VW] = logf(fog_density) / F_LN2;
	}
	shader->uniform4fv(LLShaderMgr::WATER_FOGCOLOR, 1, fog_color.mV);

	shader->uniform1f(LLShaderMgr::WATER_WATERHEIGHT, eyedepth);
	shader->uniform1f(LLShaderMgr::WATER_TIME, sTime);
	const LLVector3& camera_origin = gViewerCamera.getOrigin();
	shader->uniform3fv(LLShaderMgr::WATER_EYEVEC, 1, camera_origin.mV);
	shader->uniform3fv(LLShaderMgr::WATER_SPECULAR, 1, mLightDiffuse.mV);
	shader->uniform2fv(LLShaderMgr::WATER_WAVE_DIR1, 1,
					   waterp->getWave1Dir().mV);
	shader->uniform2fv(LLShaderMgr::WATER_WAVE_DIR2, 1,
					   waterp->getWave2Dir().mV);
	shader->uniform3fv(LLShaderMgr::WATER_LIGHT_DIR, 1, mLightDir.mV);

	shader->uniform3fv(LLShaderMgr::WATER_NORM_SCALE, 1,
					   waterp->getNormalScale().mV);
	shader->uniform1f(LLShaderMgr::WATER_FRESNEL_SCALE,
					  waterp->getFresnelScale());
	shader->uniform1f(LLShaderMgr::WATER_FRESNEL_OFFSET,
					  waterp->getFresnelOffset());
	shader->uniform1f(LLShaderMgr::WATER_BLUR_MULTIPLIER,
					  waterp->getBlurMultiplier());

	F32 sun_angle = llmax(0.f, mLightDir.mV[2]);
	shader->uniform1f(LLShaderMgr::WATER_SUN_ANGLE, 0.1f + 0.2f * sun_angle);
	shader->uniform1i(LLShaderMgr::SUN_UP_FACTOR, mIsSunUp ? 1 : 0);

	static LLCachedControl<bool> render_fast_edges(gSavedSettings,
												   "RenderWaterFastEdges");
	LLVOSky* voskyp = gSky.mVOSkyp;
	bool fast_edges = render_fast_edges && voskyp != NULL;
	if (!fast_edges)
	{
		shader->uniform1i(LLShaderMgr::WATER_EDGE_FACTOR, edge ? 1 : 0);
	}

	shader->uniform4fv(LLShaderMgr::LIGHTNORM, 1,
					   gEnvironment.getRotatedLightNorm().mV);
	shader->uniform3fv(LLShaderMgr::WL_CAMPOSLOCAL, 1, camera_origin.mV);

	if (gViewerCamera.cameraUnderWater())
	{
		shader->uniform1f(LLShaderMgr::WATER_REFSCALE,
						  waterp->getScaleBelow());
	}
	else
	{
		shader->uniform1f(LLShaderMgr::WATER_REFSCALE,
						  waterp->getScaleAbove());
	}

#if LL_OPTIMIZED_WATER_OCCLUSIONS
	// Fix for a render glitch where reflections are not updated often enough
	// while flying up/down without rotating. HB
	static F32 last_altitude = -1.f;
	static F32 last_update = 0.f;
	if (camera_origin.mV[2] != last_altitude &&
		gFrameTimeSeconds - last_update > 0.01f)
	{
		last_altitude = camera_origin.mV[2];
		last_update = gFrameTimeSeconds;
		if (gAgent.getFlying())
		{
			sNeedsReflectionUpdate = true;
		}
	}
#endif

	LLTexUnit* unit = diff_tex > -1 ? gGL.getTexUnit(diff_tex) : NULL;
	if (fast_edges)	// Use Windlight's method for faster rendering...
	{
		LLGLEnable depth_clamp(gGLManager.mHasDepthClamp ? GL_DEPTH_CLAMP : 0);
		LLGLDisable cullface(GL_CULL_FACE);

		for (std::vector<LLFace*>::iterator iter = mDrawFace.begin(),
											end = mDrawFace.end();
			iter != end; ++iter)
		{
			LLFace* face = *iter;
			if (!face) continue;

			if (voskyp->isReflFace(face))
			{
				continue;
			}

			LLVOWater* water = (LLVOWater*)face->getViewerObject();
			if (!water) continue;

			if (unit)
			{
				unit->bind(face->getTexture());
			}
	
			bool edge_patch = water->getIsEdgePatch();
#if LL_OPTIMIZED_WATER_OCCLUSIONS
			// If occlusion + water culling enabled, this is set within
			// LLOcclusionCullingGroup::checkOcclusion()
			if (!edge_patch &&
				(!LLPipeline::sRenderWaterCull || !LLPipeline::sUseOcclusion))
#else
			if (!edge_patch)
#endif
			{
				sNeedsReflectionUpdate = true;
			}

			if (!edge_patch || sDeferredRender || water->getUseTexture() ||
				gGLManager.mHasDepthClamp)
			{
				face->renderIndexed();
			}
			else if (gGLManager.mHasDepthClamp || sDeferredRender)
			{
				face->renderIndexed();
			}
			else
			{
				LLGLSquashToFarClip far_clip;
				face->renderIndexed();
			}
		}
	}
	else
	{
		LLGLDisable cullface(GL_CULL_FACE);

		for (std::vector<LLFace*>::iterator iter = mDrawFace.begin(),
											end = mDrawFace.end();
			iter != end; ++iter)
		{
			LLFace* face = *iter;
			if (!face) continue;

			LLVOWater* water = (LLVOWater*)face->getViewerObject();
			if (!water) continue;

			if (unit)
			{
				unit->bind(face->getTexture());
			}

			bool edge_patch = water->getIsEdgePatch();
			if (edge)
			{
				if (edge_patch)
				{
					face->renderIndexed();
				}
			}
			else if (!edge_patch)
			{
#if LL_OPTIMIZED_WATER_OCCLUSIONS
				// If occlusion is enabled, this is set within
				// LLOcclusionCullingGroup::checkOcclusion()
				if (!LLPipeline::sUseOcclusion)
#endif
				{
					sNeedsReflectionUpdate = true;
				}
				face->renderIndexed();
			}
		}
	}

	unitbump->unbind(LLTexUnit::TT_TEXTURE);
	if (unitbump2)
	{
		unitbump2->unbind(LLTexUnit::TT_TEXTURE);
	}

	shader->disableTexture(LLShaderMgr::ENVIRONMENT_MAP,
						   LLTexUnit::TT_CUBE_MAP);
	shader->disableTexture(LLShaderMgr::WATER_SCREENTEX);
	shader->disableTexture(LLShaderMgr::BUMP_MAP);
	shader->disableTexture(LLShaderMgr::DIFFUSE_MAP);
	shader->disableTexture(LLShaderMgr::WATER_REFTEX);

	shader->unbind();

	stop_glerror();
}

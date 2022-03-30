/**
 * @file lldrawpoolwlsky.cpp
 * @brief LLDrawPoolWLSky class implementation
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "lldrawpoolwlsky.h"

#include "llfasttimer.h"
#include "llimage.h"

#include "llenvironment.h" 
#include "llenvsettings.h" 
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llvowlsky.h"
#include "llwlskyparammgr.h"

// Uniform names
static LLStaticHashedString sCustomAlphaName("custom_alpha");
static LLStaticHashedString sCamPosLocalName("camPosLocal");

// Static member variables
LLGLSLShader* LLDrawPoolWLSky::sCloudShader = NULL;
LLGLSLShader* LLDrawPoolWLSky::sSkyShader = NULL;
LLGLSLShader* LLDrawPoolWLSky::sSunShader = NULL;
LLGLSLShader* LLDrawPoolWLSky::sMoonShader = NULL;

LLDrawPoolWLSky::LLDrawPoolWLSky()
:	LLDrawPool(POOL_WL_SKY),
	mCamHeightLocal(0.f)
{
	restoreGL();
}

void LLDrawPoolWLSky::beginRenderPass(S32)
{
	if (LLPipeline::sUnderWaterRender)
	{
		sSkyShader = sCloudShader = &gObjectFullbrightNoColorWaterProgram;
		if (gUseNewShaders)
		{
			sSunShader = sMoonShader = &gObjectFullbrightNoColorWaterProgram;
		}
	}
	else
	{
		sSkyShader = &gWLSkyProgram;
		sCloudShader = &gWLCloudProgram;
		if (gUseNewShaders)
		{
			sSunShader = &gWLSunProgram;
			sMoonShader = &gWLMoonProgram;
		}
	}
	if (gUseNewShaders)
	{
		mCurrentSky = gEnvironment.getCurrentSky();
#if LL_VARIABLE_SKY_DOME_SIZE
		mCamHeightLocal = envp->getCamHeight();
#else
		mCamHeightLocal = SKY_DOME_OFFSET * SKY_DOME_RADIUS;
#endif
	}
	else
	{
		// Windlight always used these two constants...
		mCamHeightLocal = SKY_DOME_OFFSET * SKY_DOME_RADIUS;
	}
	mCameraOrigin = gViewerCamera.getOrigin();
}

void LLDrawPoolWLSky::endRenderPass(S32)
{
	if (gUseNewShaders)
	{
		sSkyShader = sCloudShader = sSunShader = sMoonShader = NULL;
		mCurrentSky = NULL;
	}
}

void LLDrawPoolWLSky::beginDeferredPass(S32)
{
	sSkyShader = &gDeferredWLSkyProgram;
	sCloudShader = &gDeferredWLCloudProgram;
	if (gUseNewShaders)
	{
		if (LLPipeline::sUnderWaterRender)
		{
			sSunShader = sMoonShader = &gObjectFullbrightNoColorWaterProgram;
		}
		else
		{
			sSunShader = &gDeferredWLSunProgram;
			sMoonShader = &gDeferredWLMoonProgram;
		}
		mCurrentSky = gEnvironment.getCurrentSky();
#if LL_VARIABLE_SKY_DOME_SIZE
		mCamHeightLocal = envp->getCamHeight();
#else
		mCamHeightLocal = SKY_DOME_OFFSET * SKY_DOME_RADIUS;
#endif
	}
	else
	{
		// Windlight always used these two constants...
		mCamHeightLocal = SKY_DOME_OFFSET * SKY_DOME_RADIUS;
	}
	mCameraOrigin = gViewerCamera.getOrigin();
}

void LLDrawPoolWLSky::endDeferredPass(S32)
{
	if (gUseNewShaders)
	{
		sSkyShader = sCloudShader = sSunShader = sMoonShader = NULL;
		mCurrentSky = NULL;
	}
}

void LLDrawPoolWLSky::renderDome(LLGLSLShader* shader) const
{
	llassert_always(shader != NULL);

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();

	// Chop off translation
	if (LLPipeline::sReflectionRender && mCameraOrigin.mV[2] > 256.f)
	{
		gGL.translatef(mCameraOrigin.mV[0], mCameraOrigin.mV[1],
					   256.f - mCameraOrigin.mV[2] * 0.5f);
	}
	else
	{
		gGL.translatef(mCameraOrigin.mV[0], mCameraOrigin.mV[1],
					   mCameraOrigin.mV[2]);
	}

	// The Windlight sky dome works most conveniently in a coordinate system
	// where Y is up, so permute our basis vectors accordingly.
	constexpr F32 SQRT3INV = 1.f / F_SQRT3;
	static const LLMatrix4a rot = gl_gen_rot(120.f, SQRT3INV, SQRT3INV,
											 SQRT3INV);
	gGL.rotatef(rot);

	gGL.scalef(0.333f, 0.333f, 0.333f);

	gGL.translatef(0.f, -mCamHeightLocal, 0.f);

	// Draw WL Sky
	shader->uniform3f(sCamPosLocalName, 0.f, mCamHeightLocal, 0.f);

	gSky.mVOWLSkyp->drawDome();

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();
}

// Windlight version
void LLDrawPoolWLSky::renderSkyHazeWL() const
{
	if (gPipeline.canUseWindLightShaders() &&
		gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SKY))
	{
		LLGLDisable blend(GL_BLEND);
		sSkyShader->bind();
		// Render the skydome
		renderDome(sSkyShader);
		sSkyShader->unbind();
	}
}

// Extended environment version
void LLDrawPoolWLSky::renderSkyHazeEE() const
{
	if (gPipeline.canUseWindLightShaders() &&
		gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SKY))
	{
		LLGLSPipelineDepthTestSkyBox sky(GL_TRUE, GL_FALSE);
		sSkyShader->bind();
		sSkyShader->uniform1i(LLShaderMgr::SUN_UP_FACTOR, 1);
		sSkyShader->uniform1f(LLShaderMgr::SUN_MOON_GLOW_FACTOR,
							  mCurrentSky->getSunMoonGlowFactor());
		// Render the skydome
		renderDome(sSkyShader);
		sSkyShader->unbind();
	}
}

// Only for extended environment rendering
void LLDrawPoolWLSky::renderSkyHazeDeferred() const
{
	if (!gPipeline.canUseWindLightShaders() ||
		!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SKY))
	{
		return;
	}

	LLGLSPipelineDepthTestSkyBox sky(GL_TRUE, GL_TRUE);

	sSkyShader->bind();

	sSkyShader->bindTexture(LLShaderMgr::RAINBOW_MAP,
							gSky.mVOSkyp->getRainbowTex());

	sSkyShader->bindTexture(LLShaderMgr::HALO_MAP, gSky.mVOSkyp->getHaloTex());

	sSkyShader->uniform1f(LLShaderMgr::ICE_LEVEL,
						  mCurrentSky->getSkyIceLevel());

	bool sun_up = mCurrentSky->getIsSunUp();

	F32 moisture_level, droplet_radius;
	if (sun_up || mCurrentSky->getIsMoonUp())
	{
		moisture_level = mCurrentSky->getSkyMoistureLevel();
		droplet_radius = mCurrentSky->getSkyDropletRadius();
	}
	else
	{
		// Hobble halos and rainbows when there is no light source to generate
		// them
		moisture_level = droplet_radius = 0.f;
	}
	sSkyShader->uniform1f(LLShaderMgr::MOISTURE_LEVEL, moisture_level);
	sSkyShader->uniform1f(LLShaderMgr::DROPLET_RADIUS, droplet_radius);

	sSkyShader->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up ? 1 : 0);

	sSkyShader->uniform1f(LLShaderMgr::SUN_MOON_GLOW_FACTOR,
						  mCurrentSky->getSunMoonGlowFactor());

	// Render the skydome
	renderDome(sSkyShader);

	sSkyShader->unbind();
}

// Windlight version
void LLDrawPoolWLSky::renderSkyCloudsWL() const
{
	if (!gPipeline.canUseWindLightShaders() ||
		!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_CLOUDS))
	{
		return;
	}

	LLViewerTexture* cloud_noise = gSky.mVOSkyp->getCloudNoiseTex();
	if (cloud_noise)
	{
		LLGLEnable blend(GL_BLEND);
		gGL.setSceneBlendType(LLRender::BT_ALPHA);

		gGL.getTexUnit(0)->bind(cloud_noise);

		sCloudShader->bind();

		// Render the skydome
		renderDome(sCloudShader);

		sCloudShader->unbind();
	}
}

// Extended environment version
void LLDrawPoolWLSky::renderSkyCloudsEE() const
{
	if (!gPipeline.canUseWindLightShaders() ||
		!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_CLOUDS))
	{
		return;
	}

	LLViewerTexture* cloud_noise = gSky.mVOSkyp->getCloudNoiseTex();
	LLViewerTexture* cloud_noise_next = gSky.mVOSkyp->getCloudNoiseTexNext();
	if (!cloud_noise && !cloud_noise_next)
	{
		return;
	}

	LLGLSPipelineBlendSkyBox pipeline(GL_TRUE, GL_TRUE);

	sCloudShader->bind();

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	LLTexUnit* unit1 = gGL.getTexUnit(1);

	unit0->unbind(LLTexUnit::TT_TEXTURE);
	unit1->unbind(LLTexUnit::TT_TEXTURE);

	F32 blend_factor = mCurrentSky->getBlendFactor();
	if (cloud_noise && (!cloud_noise_next || cloud_noise == cloud_noise_next))
	{
		sCloudShader->bindTexture(LLShaderMgr::CLOUD_NOISE_MAP, cloud_noise,
								  LLTexUnit::TT_TEXTURE);
		blend_factor = 0.f;
	}
	else if (cloud_noise_next && !cloud_noise)
	{
		sCloudShader->bindTexture(LLShaderMgr::CLOUD_NOISE_MAP,
								  cloud_noise_next, LLTexUnit::TT_TEXTURE);
		blend_factor = 0.f;
	}
	else if (cloud_noise_next != cloud_noise)
	{
		sCloudShader->bindTexture(LLShaderMgr::CLOUD_NOISE_MAP, cloud_noise,
								  LLTexUnit::TT_TEXTURE);
		sCloudShader->bindTexture(LLShaderMgr::CLOUD_NOISE_MAP_NEXT,
								  cloud_noise_next, LLTexUnit::TT_TEXTURE);
	}
	sCloudShader->uniform1f(LLShaderMgr::BLEND_FACTOR, blend_factor);

	sCloudShader->uniform1f(LLShaderMgr::CLOUD_VARIANCE,
						    mCurrentSky->getCloudVariance());

	sCloudShader->uniform1f(LLShaderMgr::SUN_MOON_GLOW_FACTOR,
						    mCurrentSky->getSunMoonGlowFactor());

	// Render the skydome
	renderDome(sCloudShader);

	sCloudShader->unbind();

	unit0->unbind(LLTexUnit::TT_TEXTURE);
	unit1->unbind(LLTexUnit::TT_TEXTURE);
}

// Windlight version
void LLDrawPoolWLSky::renderStarsWL() const
{
	F32 alpha = gWLSkyParamMgr.mCurParams.getStarBrightness();
	// *NOTE: getStarBrightness() returns a number between 2 and 0, so we must
	// divide it by two.
	alpha = llclamp(alpha * 0.5f, 0.f, 1.f);
	if (alpha < 0.01f)
	{
		// There is no point in rendering almost invisible stars...
		return;
	}

	LLGLSPipelineSkyBoxLegacy gls_sky;
	LLGLEnable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	// *NOTE: have to have bound the cloud noise texture already since register
	// combiners blending below requires something to be bound and we might as
	// well only bind once.
	unit0->enable(LLTexUnit::TT_TEXTURE);

	gPipeline.disableLights();

	unit0->bind(gSky.mVOSkyp->getBloomTex());

	gGL.pushMatrix();
	gGL.rotatef(gFrameTimeSeconds * 0.01f, 0.f, 0.f, 1.f);

	gCustomAlphaProgram.bind();
	gCustomAlphaProgram.uniform1f(sCustomAlphaName, alpha);

	gSky.mVOWLSkyp->drawStars();

	gGL.popMatrix();

	gCustomAlphaProgram.unbind();
}

// Fixes stars being stuck at the ground.
#define STARS_FIX 1

// Extended environment version
void LLDrawPoolWLSky::renderStarsEE() const
{
	F32 alpha = llclamp(mCurrentSky->getStarBrightness() / 512.f, 0.f, 1.f);
	if (alpha < 0.01f)
	{
		// There is no point in rendering almost invisible stars...
		return;
	}

	LLGLSPipelineBlendSkyBox gls_skybox(GL_TRUE, GL_FALSE);

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	// *NOTE: have to have bound the cloud noise texture already since register
	// combiners blending below requires something to be bound and we might as
	// well only bind once.
	unit0->enable(LLTexUnit::TT_TEXTURE);

	LLViewerTexture* tex_a = gSky.mVOSkyp->getBloomTex();
	LLViewerTexture* tex_b = gSky.mVOSkyp->getBloomTexNext();
	if (tex_a && (!tex_b || tex_a == tex_b))
	{
		unit0->bind(tex_a);
	}
	else if (tex_b && !tex_a)
	{
		unit0->bind(tex_b);
	}
	else
	{
		unit0->bind(tex_a);
	}

	gGL.pushMatrix();

#if STARS_FIX
	gGL.translatef(mCameraOrigin.mV[0], mCameraOrigin.mV[1],
				   mCameraOrigin.mV[2]);
#endif
	gGL.rotatef(gFrameTimeSeconds * 0.01f, 0.f, 0.f, 1.f);

	gStarsProgram.bind();	// SL-14113 was gCustomAlphaProgram
	gStarsProgram.uniform1f(sCustomAlphaName, alpha);

	gSky.mVOWLSkyp->drawStars();

	unit0->unbind(LLTexUnit::TT_TEXTURE);

	gGL.popMatrix();

	gStarsProgram.unbind();
}

// Only for extended environment rendering
void LLDrawPoolWLSky::renderStarsDeferred() const
{
	F32 star_alpha = mCurrentSky->getStarBrightness() / 512.f;
	if (star_alpha < 0.001f)
	{
		return;	// Stars too dim, nothing to draw !
	}

	LLGLSPipelineBlendSkyBox gls_sky(GL_TRUE, GL_FALSE);

	gGL.setSceneBlendType(LLRender::BT_ADD_WITH_ALPHA);

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	LLTexUnit* unit1 = gGL.getTexUnit(1);

	F32 blend_factor = mCurrentSky->getBlendFactor();
	LLViewerTexture* tex_a = gSky.mVOSkyp->getBloomTex();
	LLViewerTexture* tex_b = gSky.mVOSkyp->getBloomTexNext();
	if (tex_a && (!tex_b || tex_a == tex_b))
	{
		unit0->bind(tex_a);
		unit1->unbind(LLTexUnit::TT_TEXTURE);
		blend_factor = 0.f;
	}
	else if (tex_b && !tex_a)
	{
		unit0->bind(tex_b);
		unit1->unbind(LLTexUnit::TT_TEXTURE);
		blend_factor = 0.f;
	}
	else if (tex_b != tex_a)
	{
		unit0->bind(tex_a);
		unit1->bind(tex_b);
	}

#if STARS_FIX
	gGL.pushMatrix();
	gGL.translatef(mCameraOrigin.mV[0], mCameraOrigin.mV[1],
				   mCameraOrigin.mV[2]);
#endif

	gDeferredStarProgram.bind();

	gDeferredStarProgram.uniform1f(LLShaderMgr::BLEND_FACTOR, blend_factor);

	if (LLPipeline::sReflectionRender)
	{
		star_alpha = 1.f;
	}
	gDeferredStarProgram.uniform1f(sCustomAlphaName, star_alpha);

	F32 start_time = (F32)LLFrameTimer::getElapsedSeconds() * 0.5f;
	gDeferredStarProgram.uniform1f(LLShaderMgr::WATER_TIME, start_time);

	gSky.mVOWLSkyp->drawStars();

	unit0->unbind(LLTexUnit::TT_TEXTURE);
	unit1->unbind(LLTexUnit::TT_TEXTURE);

#if STARS_FIX
	gGL.popMatrix();
#endif

	gDeferredStarProgram.unbind();
}

// Windlight version
void LLDrawPoolWLSky::renderHeavenlyBodiesWL()
{
	LLGLSPipelineSkyBoxLegacy gls_skybox;
	LLGLEnable blend_on(GL_BLEND);
	gPipeline.disableLights();

#if 0	// When we want to re-add a texture sun disc, here is where to do it.
	LLFace* face = gSky.mVOSkyp->mFace[LLVOSky::FACE_SUN];
	if (gSky.mVOSkyp->getSun().getDraw() && face->getGeomCount())
	{
		LLViewerTexture* tex  = face->getTexture();
		gGL.getTexUnit(0)->bind(tex);
		LLColor4 color(gSky.mVOSkyp->getSun().getInterpColor());
		LLFacePool::LLOverrideFaceColor color_override(this, color);
		face->renderIndexed();
	}
#endif

	LLFace* face = gSky.mVOSkyp->mFace[LLVOSky::FACE_MOON];

	if (gSky.mVOSkyp->getMoon().getDraw() && face->getGeomCount())
	{
		// *NOTE: even though we already bound this texture above for the stars
		// register combiners, we bind again here for defensive reasons, since
		// LLImageGL::bind detects that it's a noop, and optimizes it out.
		gGL.getTexUnit(0)->bind(face->getTexture());

		LLColor4 color(gSky.mVOSkyp->getMoon().getInterpColor());
		F32 a = gSky.mVOSkyp->getMoon().getDirection().mV[2];
		if (a > 0.f)
		{
			a = a * a * 4.f;
		}
		color.mV[3] = llclamp(a, 0.f, 1.f);

		bool can_use_shaders = gPipeline.canUseVertexShaders();
		if (can_use_shaders)
		{
			gHighlightProgram.bind();
		}

		LLFacePool::LLOverrideFaceColor color_override(this, color);

		face->renderIndexed();

		if (can_use_shaders)
		{
			gHighlightProgram.unbind();
		}
	}
}

// Extended environment version
void LLDrawPoolWLSky::renderHeavenlyBodiesEE()
{
	// SL-14113 we need moon to write to depth to clip stars behind
	LLGLSPipelineBlendSkyBox gls_skybox(GL_TRUE, GL_TRUE);

	gGL.pushMatrix();

	gGL.translatef(mCameraOrigin.mV[0], mCameraOrigin.mV[1],
				   mCameraOrigin.mV[2]);

	bool can_use_vertex_shaders = gPipeline.canUseVertexShaders();
	bool can_use_windlight_shaders = gPipeline.canUseWindLightShaders();

	F32 blend_factor = mCurrentSky->getBlendFactor();

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	LLTexUnit* unit1 = gGL.getTexUnit(1);

	LLFace* face = gSky.mVOSkyp->mFace[LLVOSky::FACE_SUN];
	if (gSky.mVOSkyp->getSun().getDraw() && face && face->getGeomCount())
	{
		unit0->unbind(LLTexUnit::TT_TEXTURE);
		unit1->unbind(LLTexUnit::TT_TEXTURE);

		LLViewerTexture* tex_a = face->getTexture(LLRender::DIFFUSE_MAP);
		LLViewerTexture* tex_b =
			face->getTexture(LLRender::ALTERNATE_DIFFUSE_MAP);
		if (tex_a || tex_b)
		{
			sSunShader->bind();
			if (tex_a && (!tex_b || tex_a == tex_b))
			{
				sSunShader->bindTexture(LLShaderMgr::DIFFUSE_MAP, tex_a,
										LLTexUnit::TT_TEXTURE);
				blend_factor = 0.f;
			}
			else if (tex_b && !tex_a)
			{
				sSunShader->bindTexture(LLShaderMgr::DIFFUSE_MAP, tex_b,
										LLTexUnit::TT_TEXTURE);
				blend_factor = 0.f;
			}
			else if (tex_b != tex_a)
			{
				sSunShader->bindTexture(LLShaderMgr::DIFFUSE_MAP,
										tex_a, LLTexUnit::TT_TEXTURE);
				sSunShader->bindTexture(LLShaderMgr::ALTERNATE_DIFFUSE_MAP,
										tex_b, LLTexUnit::TT_TEXTURE);
			}
			LLColor4 color(gSky.mVOSkyp->getSun().getInterpColor());
			sSunShader->uniform4fv(LLShaderMgr::DIFFUSE_COLOR, 1, color.mV);
			sSunShader->uniform1f(LLShaderMgr::BLEND_FACTOR, blend_factor);

			face->renderIndexed();

			unit0->unbind(LLTexUnit::TT_TEXTURE);
			unit1->unbind(LLTexUnit::TT_TEXTURE);

			sSunShader->unbind();
		}
	}

	face = gSky.mVOSkyp->mFace[LLVOSky::FACE_MOON];
	if (gSky.mVOSkyp->getMoon().getDraw() && face && face->getGeomCount())
	{
		LLViewerTexture* tex_a = face->getTexture(LLRender::DIFFUSE_MAP);
		LLViewerTexture* tex_b =
			face->getTexture(LLRender::ALTERNATE_DIFFUSE_MAP);

		if (can_use_vertex_shaders && can_use_windlight_shaders &&
			(tex_a || tex_b))
		{
			sMoonShader->bind();
		}
		if (tex_a && (!tex_b || tex_a == tex_b))
		{
			sMoonShader->bindTexture(LLShaderMgr::DIFFUSE_MAP, tex_a,
									 LLTexUnit::TT_TEXTURE);
		}
		else if (tex_b && !tex_a)
		{
			sMoonShader->bindTexture(LLShaderMgr::DIFFUSE_MAP, tex_b,
									 LLTexUnit::TT_TEXTURE);
		}
		else if (tex_b != tex_a)
		{
			sMoonShader->bindTexture(LLShaderMgr::DIFFUSE_MAP, tex_a,
									 LLTexUnit::TT_TEXTURE);
#if 0
			sMoonShader->bindTexture(LLShaderMgr::ALTERNATE_DIFFUSE_MAP, tex_b,
									 LLTexUnit::TT_TEXTURE);
#endif
		}
#if 0
		sMoonShader->uniform1f(LLShaderMgr::BLEND_FACTOR, blend_factor);
#endif
		static LLCachedControl<F32> moonfactor(gSavedSettings,
											   "RenderMoonEEBrightnessFactor");
		F32 factor = llclamp((F32)moonfactor, 1.f, 6.f);
		sMoonShader->uniform1f(LLShaderMgr::MOON_BRIGHTNESS,
							   factor * mCurrentSky->getMoonBrightness());
		sMoonShader->uniform4fv(LLShaderMgr::MOONLIGHT_COLOR, 1,
								gSky.mVOSkyp->getMoon().getColor().mV);
		LLColor4 color(gSky.mVOSkyp->getMoon().getInterpColor());
		sMoonShader->uniform4fv(LLShaderMgr::DIFFUSE_COLOR, 1, color.mV);
		sMoonShader->uniform3fv(LLShaderMgr::DEFERRED_MOON_DIR, 1,
								mCurrentSky->getMoonDirection().mV);

		face->renderIndexed();

		unit0->unbind(LLTexUnit::TT_TEXTURE);
		unit1->unbind(LLTexUnit::TT_TEXTURE);

		sMoonShader->unbind();
	}

	gGL.popMatrix();
}

void LLDrawPoolWLSky::renderDeferred(S32 pass)
{
	if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SKY))
	{
		return;
	}
	LL_FAST_TIMER(FTM_RENDER_WL_SKY);

	if (gUseNewShaders)
	{
		if (!gPipeline.canUseWindLightShaders())
		{
			return;
		}
		if (!mCurrentSky)	// Paranoia
		{
			return;
		}

		gGL.setColorMask(true, false);

		renderSkyHazeDeferred();
		renderHeavenlyBodiesEE();
		renderStarsDeferred();
		renderSkyCloudsEE();

		gGL.setColorMask(true, false);
	}
	else
	{
		LLGLSNoFog disable_fog;
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);
		LLGLDisable clip(GL_CLIP_PLANE0);

		gGL.setColorMask(true, false);

		LLGLSquashToFarClip far_clip;

		renderSkyHazeWL();

		gGL.pushMatrix();

		gGL.translatef(mCameraOrigin.mV[0], mCameraOrigin.mV[1],
					   mCameraOrigin.mV[2]);

		gDeferredStarProgram.bind();
		// *NOTE: have to bind a texture here since register combiners blending
		// in renderStarsWL() requires something to be bound and we might as
		// well only bind the Moon's texture once.
		gGL.getTexUnit(0)->bind(gSky.mVOSkyp->mFace[LLVOSky::FACE_MOON]->getTexture());

		renderHeavenlyBodiesWL();

		renderStarsWL();

		gDeferredStarProgram.unbind();

		gGL.popMatrix();

		renderSkyCloudsWL();

		gGL.setColorMask(true, true);
	}
}

void LLDrawPoolWLSky::render(S32 pass)
{
	if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SKY))
	{
		return;
	}
	LL_FAST_TIMER(FTM_RENDER_WL_SKY);

	if (gUseNewShaders)
	{
		if (!mCurrentSky)	// Paranoia
		{
			return;
		}

		renderSkyHazeEE();
		renderHeavenlyBodiesEE();
		renderStarsEE();
		renderSkyCloudsEE();

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	}
	else
	{
		LLGLSNoFog disableFog;
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);
		LLGLDisable clip(GL_CLIP_PLANE0);

		LLGLSquashToFarClip far_clip;

		renderSkyHazeWL();

		gGL.pushMatrix();

		gGL.translatef(mCameraOrigin.mV[0], mCameraOrigin.mV[1],
					   mCameraOrigin.mV[2]);

		LLTexUnit* unit0 = gGL.getTexUnit(0);

		// *NOTE: have to bind a texture here since register combiners blending
		// in renderStarsWL() requires something to be bound and we might as
		// well only bind the Moon's texture once.
		unit0->bind(gSky.mVOSkyp->mFace[LLVOSky::FACE_MOON]->getTexture());

		renderHeavenlyBodiesWL();

		renderStarsWL();

		gGL.popMatrix();

		renderSkyCloudsWL();

		unit0->unbind(LLTexUnit::TT_TEXTURE);
	}
}

//static
void LLDrawPoolWLSky::cleanupGL()
{
}

//static
void LLDrawPoolWLSky::restoreGL()
{
	if (!gUseNewShaders)
	{
		gWLSkyParamMgr.propagateParameters();
	}
	// We likely need to rebuild our current sky geometry
	if (gSky.mVOWLSkyp.notNull())
	{
		gSky.mVOWLSkyp->updateGeometry(gSky.mVOWLSkyp->mDrawable);
	}
}

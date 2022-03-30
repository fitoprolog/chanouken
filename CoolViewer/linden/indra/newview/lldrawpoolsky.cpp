/**
 * @file lldrawpoolsky.cpp
 * @brief LLDrawPoolSky class implementation
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

#include "lldrawpoolsky.h"

#include "imageids.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llvosky.h"
#include "llworld.h"				// To get water height

LLDrawPoolSky::LLDrawPoolSky()
:	LLFacePool(POOL_SKY),
	mSkyTex(NULL)
{
}

void LLDrawPoolSky::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_ENVIRONMENT);
	gSky.mVOSkyp->updateGeometry(gSky.mVOSkyp->mDrawable);
}

void LLDrawPoolSky::render(S32 pass)
{
	gGL.flush();

	if (mDrawFace.empty())
	{
		return;
	}

	// Do not draw the sky box if we can and are rendering the WL sky dome.
	if (gPipeline.canUseWindLightShaders())
	{
		return;
	}

	// Do not render sky under water (background just gets cleared to fog
	// color)
	if (mShaderLevel > 0 && LLPipeline::sUnderWaterRender)
	{
		return;
	}

	// Just use the UI shader (generic single texture no lighting)
	gOneTextureNoColorProgram.bind();

	LLVector3 origin = gViewerCamera.getOrigin();
	U32 face_count = mDrawFace.size();
	if (gUseNewShaders)
	{
		LLGLSPipelineDepthTestSkyBox gls_skybox(GL_TRUE, GL_FALSE);
		LLGLEnable fog_enable(mShaderLevel < 1 &&
							  gViewerCamera.cameraUnderWater() ? GL_FOG : 0);

		gGL.pushMatrix();
		gGL.translatef(origin.mV[0], origin.mV[1], origin.mV[2]);

		LLVertexBuffer::unbind();
		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);

		for (U32 i = 0; i < face_count; ++i)
		{
			renderSkyFace(i);
		}

		gGL.popMatrix();
	}
	else
	{
		LLGLSPipelineSkyBoxLegacy gls_skybox;
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		LLGLSquashToFarClip far_clip;
		LLGLEnable fog_enable(mShaderLevel < 1 &&
							  gViewerCamera.cameraUnderWater() ? GL_FOG : 0);

		gPipeline.disableLights();

		LLGLDisable clip(GL_CLIP_PLANE0);

		gGL.pushMatrix();
		gGL.translatef(origin.mV[0], origin.mV[1], origin.mV[2]);

		LLVertexBuffer::unbind();
		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);

		face_count = llmin(6U, face_count);
		for (U32 i = 0; i < face_count; ++i)
		{
			renderSkyCubeFace(i);
		}

		gGL.popMatrix();
	}
}

// Extended environment version
void LLDrawPoolSky::renderSkyFace(U8 index)
{
	LLFace* face = mDrawFace[index];
	if (!face || !face->getGeomCount())
	{
		return;
	}

	if (index < LLVOSky::FACE_SUN)			// Sky texture, interpolate
	{
		mSkyTex[index].bindTexture(true);	// Bind the current texture
		face->renderIndexed();
	}
	else if (index == LLVOSky::FACE_MOON)	// Moon
	{
		// SL-14113: write depth for Moon so stars can test if behind it
		LLGLSPipelineDepthTestSkyBox gls_skybox(GL_TRUE, GL_TRUE);
		
		LLGLEnable blend(GL_BLEND);

		LLViewerTexture* tex = face->getTexture(LLRender::DIFFUSE_MAP);
		if (tex)
		{
			gMoonProgram.bind(); // SL-14113 was gOneTextureNoColorProgram
			gGL.getTexUnit(0)->bind(tex);
			face->renderIndexed();
		}
	}
	else									// Heavenly body faces, no interp.
	{
		// Reset to previous
		LLGLSPipelineDepthTestSkyBox gls_skybox(GL_TRUE, GL_FALSE);

		LLGLEnable blend(GL_BLEND);

		LLViewerTexture* tex = face->getTexture(LLRender::DIFFUSE_MAP);
		if (tex)
		{
			gOneTextureNoColorProgram.bind();
			gGL.getTexUnit(0)->bind(tex);
			face->renderIndexed();
		}
	}
}

// Windlight version
void LLDrawPoolSky::renderSkyCubeFace(U8 side)
{
	LLFace& face = *mDrawFace[LLVOSky::FACE_SIDE0 + side];
	if (!face.getGeomCount())
	{
		return;
	}

	llassert(mSkyTex);
	mSkyTex[side].bindTexture(true);

	face.renderIndexed();

	if (LLSkyTex::doInterpolate())
	{
		LLGLEnable blend(GL_BLEND);
		mSkyTex[side].bindTexture(false);
		// Lighting is disabled
		gGL.diffuseColor4f(1.f, 1.f, 1.f, LLSkyTex::getInterpVal());
		face.renderIndexed();
	}
}

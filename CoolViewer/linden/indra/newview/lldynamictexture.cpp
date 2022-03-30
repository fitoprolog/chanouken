/**
 * @file lldynamictexture.cpp
 * @brief Implementation of LLDynamicTexture class
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

#include "llviewerprecompiledheaders.h"

#include "lldynamictexture.h"

#include "llgl.h"
#include "llglslshader.h"
#include "llrender.h"
#include "llvertexbuffer.h"

#include "llpipeline.h"
#include "llstartup.h"			// For getStartupState()
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewertexture.h"
#include "llviewerwindow.h"

// static
LLViewerDynamicTexture::instance_list_t LLViewerDynamicTexture::sInstances[LLViewerDynamicTexture::ORDER_COUNT];
S32 LLViewerDynamicTexture::sNumRenders = 0;

LLViewerDynamicTexture::LLViewerDynamicTexture(S32 width, S32 height,
											   S32 components, EOrder order,
											   bool clamp)
:	LLViewerTexture(width, height, components, false),
	mBoundTarget(NULL),
	mClamp(clamp)
{
	llassert(components >= 1 && components <= 4 &&
			 order >= 0 && order < ORDER_COUNT);

	if (gGLManager.mDebugGPU)
	{
		if (components == 3)
		{
			mComponents = 4;	// Convert to 32 bits.
		}
	}
	generateGLTexture();

	sInstances[order].insert(this);
}

LLViewerDynamicTexture::~LLViewerDynamicTexture()
{
	for (S32 order = 0; order < ORDER_COUNT; ++order)
	{
		// Will fail in all but one case.
		sInstances[order].erase(this);
	}
}

//virtual
S8 LLViewerDynamicTexture::getType() const
{
	return LLViewerTexture::DYNAMIC_TEXTURE;
}

void LLViewerDynamicTexture::generateGLTexture()
{
	LLViewerTexture::generateGLTexture();
	generateGLTexture(-1, 0, 0, false);
}

void LLViewerDynamicTexture::generateGLTexture(S32 internal_fmt,
											   U32 primary_fmt,
											   U32 type_format,
											   bool swap_bytes)
{
	if (mComponents < 1 || mComponents > 4)
	{
		llerrs << "Bad number of components in dynamic texture: "
			   << mComponents << llendl;
	}

	LLPointer<LLImageRaw> raw_image = new LLImageRaw(mFullWidth, mFullHeight,
													 mComponents);
	if (internal_fmt >= 0)
	{
		setExplicitFormat(internal_fmt, primary_fmt, type_format, swap_bytes);
	}
	createGLTexture(0, raw_image, 0, true, LLGLTexture::DYNAMIC_TEX);
	setAddressMode(mClamp ? LLTexUnit::TAM_CLAMP : LLTexUnit::TAM_WRAP);
	mGLTexturep->setGLTextureCreated(false);
}

void LLViewerDynamicTexture::preRender(bool clear_depth)
{
#if LL_USE_DYNAMIC_TEX_FBO
	if (!gGLManager.mIsAMD && gGLManager.mHasFramebufferObject &&
		gPipeline.mBake.isComplete())
#else
	if (gGLManager.mHasFramebufferObject)
#endif
	{
		// Using offscreen render target, just use the bottom left corner
		mOrigin.set(0, 0);
	}
	else
	{
		// Force rendering to on-screen portion of frame buffer
		LLCoordScreen window_pos;
		gWindowp->getPosition(&window_pos);
		// Top left corner
		mOrigin.set(0, gViewerWindowp->getWindowDisplayHeight() - mFullHeight);

		if (window_pos.mX < 0)
		{
			mOrigin.mX = -window_pos.mX;
		}
		if (window_pos.mY < 0)
		{
			mOrigin.mY += window_pos.mY;
			mOrigin.mY = llmax(mOrigin.mY, 0);
		}
	}

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	// Set up camera
	mCamera.setOrigin(gViewerCamera);
	mCamera.setAxes(gViewerCamera);
	mCamera.setAspect(gViewerCamera.getAspect());
	mCamera.setView(gViewerCamera.getView());
	mCamera.setNear(gViewerCamera.getNear());

	glViewport(mOrigin.mX, mOrigin.mY, mFullWidth, mFullHeight);
	if (clear_depth)
	{
		glClear(GL_DEPTH_BUFFER_BIT);
	}
}

void LLViewerDynamicTexture::postRender(bool success)
{
	if (success)
	{
		if (mGLTexturep.isNull() || !mGLTexturep->getHasGLTexture() ||
			mGLTexturep->getDiscardLevel() != 0)
		{
			generateGLTexture();
		}
		mGLTexturep->setSubImageFromFrameBuffer(0, 0, mOrigin.mX, mOrigin.mY,
												mFullWidth, mFullHeight);
	}

	// Restore viewport
	gViewerWindowp->setupViewport();

	// Restore camera
	gViewerCamera.setOrigin(mCamera);
	gViewerCamera.setAxes(mCamera);
	gViewerCamera.setAspect(mCamera.getAspect());
	gViewerCamera.setView(mCamera.getView());
	gViewerCamera.setNear(mCamera.getNear());
}

// Calls update on each dynamic texture. Calls each group in order: "first,"
// then "middle," then "last."
//static
bool LLViewerDynamicTexture::updateAllInstances()
{
	sNumRenders = 0;
	if (gGLManager.mIsDisabled || LLPipeline::sMemAllocationThrottled)
	{
		return true;
	}

#if LL_USE_DYNAMIC_TEX_FBO
	bool use_fbo = !gGLManager.mIsAMD && gGLManager.mHasFramebufferObject &&
				   gPipeline.mBake.isComplete() &&
				   // *HACK: to avoid a flicker of the screen while loading
				   // shaders on login
				   LLStartUp::getStartupState() > STATE_PRECACHE;
	if (use_fbo)
	{
		gPipeline.mBake.bindTarget();
		gPipeline.mBake.clear();
	}
#endif

	LLGLSLShader::bindNoShader();
	LLVertexBuffer::unbind();

	bool result = false;
	bool ret = false;
	for (S32 order = 0; order < ORDER_COUNT; ++order)
	{
		for (instance_list_t::iterator
				iter = LLViewerDynamicTexture::sInstances[order].begin(),
				end = LLViewerDynamicTexture::sInstances[order].end();
			 iter != end; ++iter)
		{
			LLViewerDynamicTexture* tex = *iter;
			if (tex->needsRender())
			{
				glClear(GL_DEPTH_BUFFER_BIT);
				gGL.color4f(1.f, 1.f, 1.f, 1.f);
#if LL_USE_DYNAMIC_TEX_FBO
				gDepthDirty = true;
				tex->mBoundTarget = use_fbo ? &gPipeline.mBake : NULL;
#endif
				tex->preRender();	// Must be called outside of startRender()
				result = false;
				if (tex->render())
				{
					ret = true;
					result = true;
					++sNumRenders;
				}
				gGL.flush();
				LLVertexBuffer::unbind();
#if LL_USE_DYNAMIC_TEX_FBO
				if (use_fbo)
				{
					tex->mBoundTarget = NULL;
				}
#endif
				tex->postRender(result);
			}
		}
	}

#if LL_USE_DYNAMIC_TEX_FBO
	if (use_fbo)
	{
		gPipeline.mBake.flush();
	}
#endif

	gGL.flush();

	return ret;
}

//static
void LLViewerDynamicTexture::destroyGL()
{
	for (S32 order = 0; order < ORDER_COUNT; ++order)
	{
		for (instance_list_t::iterator
				iter = LLViewerDynamicTexture::sInstances[order].begin(),
				end = LLViewerDynamicTexture::sInstances[order].end();
			 iter != end; ++iter)
		{
			LLViewerDynamicTexture* tex = *iter;
			tex->destroyGLTexture();
		}
	}
}

//static
void LLViewerDynamicTexture::restoreGL()
{
	if (gGLManager.mIsDisabled)
	{
		return;
	}

	for (S32 order = 0; order < ORDER_COUNT; ++order)
	{
		for (instance_list_t::iterator
				iter = LLViewerDynamicTexture::sInstances[order].begin(),
				end = LLViewerDynamicTexture::sInstances[order].end();
			 iter != end; ++iter)
		{
			LLViewerDynamicTexture* tex = *iter;
			tex->restoreGLTexture();
		}
	}
}

/**
 * @file llrendertarget.cpp
 * @brief LLRenderTarget implementation
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

#include "linden_common.h"

#include "llrendertarget.h"

#include "llgl.h"
#include "llglslshader.h"		// For gUseNewShaders
#include "llimagegl.h"
#include "llrender.h"

// statics
LLRenderTarget* LLRenderTarget::sBoundTarget = NULL;
U32 LLRenderTarget::sBytesAllocated = 0;
bool LLRenderTarget::sUseFBO = false;
U32 LLRenderTarget::sCurFBO = 0;
U32 LLRenderTarget::sCurResX = 0;
U32 LLRenderTarget::sCurResY = 0;

void check_framebuffer_status()
{
	if (gDebugGL)
	{
		GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
		switch (status)
		{
			case GL_FRAMEBUFFER_COMPLETE:
				break;
			default:
				llwarns << "check_framebuffer_status failed -- " << std::hex
						<< status << std::dec << llendl;
				break;
		}
	}
}

LLRenderTarget::LLRenderTarget()
:	mResX(0),
	mResY(0),
	mFBO(0),
	mPreviousFBO(0),
	mPreviousResX(0),
	mPreviousResY(0),
	mDepth(0),
	mStencil(0),
	mUseDepth(false),
	mRenderDepth(false),
	mUsage(LLTexUnit::TT_TEXTURE)
{
}

LLRenderTarget::~LLRenderTarget()
{
	release();
}

void LLRenderTarget::resize(U32 resx, U32 resy)
{
	// for accounting, get the number of pixels added/subtracted
	S32 pix_diff = resx * resy - mResX * mResY;

	mResX = resx;
	mResY = resy;

	llassert(mInternalFormat.size() == mTex.size());

	U32 internal_type = LLTexUnit::getInternalType(mUsage);
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	for (U32 i = 0, size = mTex.size(); i < size; ++i)
	{
		// Resize color attachments
		unit0->bindManual(mUsage, mTex[i]);
		LLImageGL::setManualImage(internal_type, 0, mInternalFormat[i],
								  mResX, mResY, GL_RGBA, GL_UNSIGNED_BYTE,
								  NULL, false);
		sBytesAllocated += pix_diff * 4;
	}

	if (mDepth)
	{
		// resize depth attachment
		if (mStencil)
		{
			// use render buffers where stencil buffers are in play
			glBindRenderbuffer(GL_RENDERBUFFER, mDepth);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
								  mResX, mResY);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
		}
		else
		{
			unit0->bindManual(mUsage, mDepth);
			LLImageGL::setManualImage(internal_type, 0, GL_DEPTH_COMPONENT24,
									  mResX, mResY, GL_DEPTH_COMPONENT,
									  GL_UNSIGNED_INT, NULL, false);
		}

		sBytesAllocated += pix_diff * 4;
	}
}

bool LLRenderTarget::allocate(U32 resx, U32 resy, U32 color_fmt, bool depth,
							  bool stencil, LLTexUnit::eTextureType usage,
							  bool use_fbo, S32 samples)
{
	resx = llmin(resx, (U32)gGLManager.mGLMaxTextureSize);
	resy = llmin(resy, (U32)gGLManager.mGLMaxTextureSize);

	release();
	stop_glerror();

	mResX = resx;
	mResY = resy;

	mStencil = stencil;
	mUsage = usage;
	mUseDepth = depth;

	if ((sUseFBO || use_fbo) && gGLManager.mHasFramebufferObject)
	{
		if (depth)
		{
			if (!allocateDepth())
			{
				llwarns << "Failed to allocate depth buffer for render target."
						<< llendl;
				return false;
			}
		}

		glGenFramebuffers(1, (GLuint*)&mFBO);

		if (mDepth)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
			if (mStencil)
			{
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
										  GL_RENDERBUFFER, mDepth);
				stop_glerror();
				glFramebufferRenderbuffer(GL_FRAMEBUFFER,
										  GL_STENCIL_ATTACHMENT,
										  GL_RENDERBUFFER, mDepth);
			}
			else
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
									   LLTexUnit::getInternalType(mUsage),
									   mDepth, 0);
			}
			stop_glerror();
			glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
		}
		stop_glerror();
	}

	return addColorAttachment(color_fmt);
}

bool LLRenderTarget::addColorAttachment(U32 color_fmt)
{
	if (color_fmt == 0)
	{
		return true;
	}

	U32 offset = mTex.size();
	if (offset >= 4)
	{
		llwarns << "Too many color attachments !" << llendl;
		return false;
	}
	if (offset > 0 && (!mFBO || !gGLManager.mHasDrawBuffers))
	{
		llwarns << (mFBO ? "No draw buffer available" : "FBO not in use")
				<< ", aborting." << llendl;
		return false;
	}

	U32 tex;
	LLImageGL::generateTextures(1, &tex);
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->bindManual(mUsage, tex);

	stop_glerror();

	U32 internal_type = LLTexUnit::getInternalType(mUsage);
	{
		clear_glerror();
		LLImageGL::setManualImage(internal_type, 0, color_fmt, mResX, mResY,
								  GL_RGBA, GL_UNSIGNED_BYTE, NULL, false);
		if (glGetError() != GL_NO_ERROR)
		{
			llwarns << "Could not allocate color buffer for render target."
					<< llendl;
			return false;
		}
	}

	sBytesAllocated += mResX * mResY * 4;

	stop_glerror();

	if (offset == 0)
	{
		// use bilinear filtering on single texture render targets that aren't
		// multisampled
		unit0->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
		stop_glerror();
	}
	else
	{
		// do not filter data attachments
		unit0->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		stop_glerror();
	}
	if (mUsage != LLTexUnit::TT_RECT_TEXTURE)
	{
		unit0->setTextureAddressMode(LLTexUnit::TAM_MIRROR);
		stop_glerror();
	}
	else
	{
		// ATI does not support mirrored repeat for rectangular textures.
		unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
		stop_glerror();
	}

	if (mFBO)
	{
		stop_glerror();
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + offset,
							   internal_type, tex, 0);
		stop_glerror();

		check_framebuffer_status();

		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
	}

	mTex.push_back(tex);
	mInternalFormat.push_back(color_fmt);

#if !LL_DARWIN
	if (gDebugGL)
	{	// bind and unbind to validate target
		bindTarget();
		flush();
	}
#endif

	return true;
}

bool LLRenderTarget::allocateDepth()
{
	if (mStencil)
	{
		// use render buffers where stencil buffers are in play
		glGenRenderbuffers(1, (GLuint*)&mDepth);
		glBindRenderbuffer(GL_RENDERBUFFER, mDepth);
		stop_glerror();
		clear_glerror();
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
							  mResX, mResY);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	else
	{
		LLImageGL::generateTextures(1, &mDepth);
		LLTexUnit* unit0 = gGL.getTexUnit(0);
		unit0->bindManual(mUsage, mDepth);

		stop_glerror();
		clear_glerror();
		LLImageGL::setManualImage(LLTexUnit::getInternalType(mUsage), 0,
								  GL_DEPTH_COMPONENT24, mResX, mResY,
								  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL,
								  false);
		unit0->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	sBytesAllocated += mResX * mResY * 4;

	if (glGetError() != GL_NO_ERROR)
	{
		llwarns << "Unable to allocate depth buffer for render target."
				<< llendl;
		return false;
	}

	return true;
}

void LLRenderTarget::shareDepthBuffer(LLRenderTarget& target)
{
	if (!mFBO || !target.mFBO)
	{
		llerrs << "Cannot share depth buffer between non FBO render targets."
			   << llendl;
	}

	if (target.mDepth)
	{
		llerrs << "Attempting to override existing depth buffer. Detach existing buffer first."
			   << llendl;
	}

	if (target.mUseDepth)
	{
		llerrs << "Attempting to override existing shared depth buffer. Detach existing buffer first."
			   << llendl;
	}

	if (mDepth)
	{
		stop_glerror();
		glBindFramebuffer(GL_FRAMEBUFFER, target.mFBO);
		stop_glerror();

		if (mStencil)
		{
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
									  GL_RENDERBUFFER, mDepth);
			stop_glerror();
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
									  GL_RENDERBUFFER, mDepth);
			stop_glerror();
			target.mStencil = true;
		}
		else
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
								   LLTexUnit::getInternalType(mUsage),
								   mDepth, 0);
			stop_glerror();
		}

		check_framebuffer_status();

		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);

		target.mUseDepth = true;
	}
}

void LLRenderTarget::release()
{
	if (mDepth)
	{
		if (mStencil)
		{
			glDeleteRenderbuffers(1, (GLuint*)&mDepth);
			stop_glerror();
		}
		else
		{
			if (mFBO)
			{
				// Release before delete.
				glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
									   LLTexUnit::getInternalType(mUsage),
									   0, 0);
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			}
			LLImageGL::deleteTextures(1, &mDepth);
			stop_glerror();
		}
		mDepth = 0;

		sBytesAllocated -= mResX * mResY * 4;
	}
	else if (mUseDepth && mFBO)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

		// Detach shared depth buffer
		if (mStencil)
		{
			// Attached as a renderbuffer
			glFramebufferRenderbuffer(GL_FRAMEBUFFER,
									  GL_STENCIL_ATTACHMENT,
									  GL_RENDERBUFFER, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
									  GL_RENDERBUFFER, 0);
			mStencil = false;
		}
		else
		{
			// Attached as a texture
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
								   LLTexUnit::getInternalType(mUsage), 0, 0);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		mUseDepth = false;
	}

	if (mFBO)
	{
		glDeleteFramebuffers(1, (GLuint*)&mFBO);
		stop_glerror();
		mFBO = 0;
	}

	size_t tsize = mTex.size();
	if (tsize > 0)
	{
		sBytesAllocated -= mResX * mResY * 4 * tsize;
		LLImageGL::deleteTextures(tsize, &mTex[0]);
		mTex.clear();
		mInternalFormat.clear();
	}

	mResX = mResY = 0;
	sBoundTarget = NULL;
}

void LLRenderTarget::bindTarget()
{
	if (mFBO)
	{
		stop_glerror();

		mPreviousFBO = sCurFBO;
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		sCurFBO = mFBO;
		stop_glerror();

		if (gGLManager.mHasDrawBuffers)
		{
			// Setup multiple render targets
			GLenum drawbuffers[] = { GL_COLOR_ATTACHMENT0,
									 GL_COLOR_ATTACHMENT1,
									 GL_COLOR_ATTACHMENT2,
									 GL_COLOR_ATTACHMENT3 };
			glDrawBuffersARB(mTex.size(), drawbuffers);
		}

		if (mTex.empty())
		{
			// No color buffer to draw to
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
		}

		check_framebuffer_status();

		stop_glerror();
	}

	mPreviousResX = sCurResX;
	mPreviousResY = sCurResY;
	glViewport(0, 0, sCurResX = mResX, sCurResY = mResY);
	sBoundTarget = this;
}

void LLRenderTarget::clear(U32 mask_in)
{
	U32 mask = GL_COLOR_BUFFER_BIT;
	if (mUseDepth)
	{
		mask |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
	}
	if (mFBO)
	{
		check_framebuffer_status();
		stop_glerror();
		glClear(mask & mask_in);
		stop_glerror();
	}
	else
	{
		LLGLEnable scissor(GL_SCISSOR_TEST);
		glScissor(0, 0, mResX, mResY);
		stop_glerror();
		glClear(mask & mask_in);
	}
}

U32 LLRenderTarget::getTexture(U32 attachment) const
{
	if (attachment > mTex.size() - 1)
	{
		llerrs << "Invalid attachment index." << llendl;
	}
	if (mTex.empty())
	{
		return 0;
	}
	return mTex[attachment];
}

void LLRenderTarget::bindTexture(U32 index, S32 channel,
								 LLTexUnit::eTextureFilterOptions filter_options)
{
	LLTexUnit* unit = gGL.getTexUnit(channel);
	unit->bindManual(mUsage, getTexture(index));

	if (!gUseNewShaders) return;

	unit->setTextureFilteringOption(filter_options);

	if (index < mInternalFormat.size())
	{
		U32 format = mInternalFormat[index];
		LLTexUnit::eTextureColorSpace space;
		if (format == GL_SRGB || format == GL_SRGB8 ||
			format == GL_SRGB_ALPHA || format == GL_SRGB8_ALPHA8)
		{
			space = LLTexUnit::TCS_SRGB;
		}
		else
		{
			space = LLTexUnit::TCS_LINEAR;
		}
		unit->setTextureColorSpace(space);
	}
}

void LLRenderTarget::flush(bool fetch_depth)
{
	gGL.flush();
	if (!mFBO)
	{
		LLTexUnit* unit0 = gGL.getTexUnit(0);
		unit0->bind(this);
		U32 internal_type = LLTexUnit::getInternalType(mUsage);
		glCopyTexSubImage2D(internal_type, 0, 0, 0, 0, 0, mResX, mResY);

		if (fetch_depth)
		{
			if (!mDepth)
			{
				allocateDepth();
			}
			unit0->bind(this, true);
			glCopyTexSubImage2D(internal_type, 0, 0, 0, 0, 0, mResX, mResY);
		}
		stop_glerror();

		unit0->disable();
	}
	else
	{
		stop_glerror();
		glBindFramebuffer(GL_FRAMEBUFFER, mPreviousFBO);
		sCurFBO = mPreviousFBO;

		if (mPreviousFBO)
		{
			glViewport(0, 0, sCurResX = mPreviousResX, sCurResY = mPreviousResY);
			mPreviousFBO = 0;
		}
		else
		{
			glViewport(gGLViewport[0], gGLViewport[1],
					   sCurResX = gGLViewport[2], sCurResY = gGLViewport[3]);
		}

		stop_glerror();
	}
}

void LLRenderTarget::copyContents(LLRenderTarget& source, S32 srcX0, S32 srcY0,
								  S32 srcX1, S32 srcY1, S32 dstX0, S32 dstY0,
								  S32 dstX1, S32 dstY1, U32 mask, U32 filter)
{
	GLboolean write_depth = mask & GL_DEPTH_BUFFER_BIT ? GL_TRUE : GL_FALSE;

	LLGLDepthTest depth(write_depth, write_depth);

	gGL.flush();
	if (!source.mFBO || !mFBO)
	{
		llwarns << "Cannot copy framebuffer contents for non FBO render targets."
				<< llendl;
		return;
	}

	if (mask == GL_DEPTH_BUFFER_BIT && source.mStencil != mStencil)
	{
		stop_glerror();

		glBindFramebuffer(GL_FRAMEBUFFER, source.mFBO);
		check_framebuffer_status();
		gGL.getTexUnit(0)->bind(this, true);
		stop_glerror();
		glCopyTexSubImage2D(LLTexUnit::getInternalType(mUsage), 0,
							srcX0, srcY0, dstX0, dstY0, dstX1, dstY1);
		stop_glerror();
		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
		stop_glerror();
	}
	else
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, source.mFBO);
		stop_glerror();
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFBO);
		stop_glerror();
		check_framebuffer_status();
		stop_glerror();
		if (gGLManager.mIsAMD && mask & GL_STENCIL_BUFFER_BIT)
		{
			mask &= ~GL_STENCIL_BUFFER_BIT;
			glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
							  dstX1, dstY1, GL_STENCIL_BUFFER_BIT, filter);
		}
		if (mask)
		{
			glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
							  dstX1, dstY1, mask, filter);
		}
		stop_glerror();
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		stop_glerror();
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		stop_glerror();
		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
		stop_glerror();
	}
}

//static
void LLRenderTarget::copyContentsToFramebuffer(LLRenderTarget& source,
											   S32 srcX0, S32 srcY0,
											   S32 srcX1, S32 srcY1,
											   S32 dstX0, S32 dstY0,
											   S32 dstX1, S32 dstY1,
											   U32 mask, U32 filter)
{
	if (!source.mFBO)
	{
		llwarns << "Cannot copy framebuffer contents for non FBO render targets."
				<< llendl;
		return;
 	}

	{
		GLboolean write_depth = mask & GL_DEPTH_BUFFER_BIT ? TRUE : FALSE;

		LLGLDepthTest depth(write_depth, write_depth);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, source.mFBO);
		stop_glerror();
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		stop_glerror();
		check_framebuffer_status();
		stop_glerror();
		if (gGLManager.mIsAMD && mask & GL_STENCIL_BUFFER_BIT)
		{
			mask &= ~GL_STENCIL_BUFFER_BIT;
			glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
							  dstX1, dstY1, GL_STENCIL_BUFFER_BIT, filter);
		}
		if (mask)
		{
			glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
							  dstX1, dstY1, mask, filter);
		}
		stop_glerror();
		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
		stop_glerror();
	}
}

bool LLRenderTarget::isComplete() const
{
	return !mTex.empty() || mDepth;
}

void LLRenderTarget::getViewport(S32* viewport)
{
	viewport[0] = viewport[1] = 0;
	viewport[2] = mResX;
	viewport[3] = mResY;
}

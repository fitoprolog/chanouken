/**
 * @file llgl.h
 * @brief LLGL definition
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

#ifndef LL_LLGL_H
#define LL_LLGL_H

// This file contains various stuff for handling gl extensions and other GL
// related stuff.

#include <list>

#include "llcolor4.h"
#include "llfastmap.h"
#include "llglheaders.h"
#include "llinstancetracker.h"
#include "llmatrix4a.h"
#include "llplane.h"

#define LL_USE_FENCE 0

extern bool gDebugGL;
extern bool gClearARBbuffer;

class LLSD;

// Manage GL extensions...
class LLGLManager
{
protected:
	LOG_CLASS(LLGLManager);

public:
	LLGLManager();

	bool initGL();
	void shutdownGL();

#if LL_WINDOWS
	void initWGL(); // Initializes stupid WGL extensions
#endif

	std::string getRawGLString(); // For sending to simulator

	void getPixelFormat(); // Get the best pixel format

	void printGLInfoString();
	void getGLInfo(LLSD& info);

	void asLLSD(LLSD& info);

private:
	void initExtensions();
	void initGLStates();
	void initGLImages();

public:
	bool mInited;
	bool mIsDisabled;

	// Extensions used by everyone
	S32 mNumTextureUnits;
	S32 mMaxSamples;
	bool mHasMultitexture;
	bool mHasATIMemInfo;
	bool mHasAMDAssociations;
	bool mHasNVXMemInfo;
	bool mHasMipMapGeneration;
	bool mHasCompressedTextures;
	bool mHasFramebufferObject;
	bool mHasBlendFuncSeparate;

	// ARB Extensions
	bool mHasVertexBufferObject;
	bool mHasVertexArrayObject;
	bool mHasSync;
	bool mHasMapBufferRange;
	bool mHasFlushBufferRange;
	bool mHasPBuffer;
	bool mHasOcclusionQuery;
	bool mHasOcclusionQuery2;
	bool mHasTimerQuery;
	bool mHasPointParameters;
	bool mHasDrawBuffers;
	bool mHasDepthClamp;
	bool mUseDepthClamp;
	bool mHasTextureRectangle;
	bool mHasTextureMultisample;
	S32 mNumTextureImageUnits;
	S32 mMaxSampleMaskWords;
	S32 mMaxColorTextureSamples;
	S32 mMaxDepthTextureSamples;
	S32 mMaxIntegerSamples;

	// Other extensions.
	bool mHasAnisotropic;
	bool mHasARBEnvCombine;
	bool mHasCubeMap;
	bool mHasDebugOutput;
	bool mHassRGBTexture;
	bool mHassRGBFramebuffer;
	bool mHasTexturesRGBDecode;
	bool mHasTextureSwizzle;
	bool mHasGpuShader5;

	// Vendor-specific extensions
	bool mIsAMD;
	bool mIsNVIDIA;
	bool mIsIntel;
	bool mIsGF2or4MX;
	bool mIsGFFX;
	bool mATIOffsetVerticalLines;
	bool mATIOldDriver;

#if LL_DARWIN
	// Needed to distinguish problem cards on older Macs that break with
	// materials
	bool mIsMobileGF;
#endif

	// Whether this version of GL is good enough for SL to use
	bool mHasRequirements;

	// Misc extensions
	bool mHasSeparateSpecularColor;
	bool mHasVertexAttribIPointer;

	// Whether this GPU is in the debug list.
	bool mDebugGPU;

	S32 mDriverVersionMajor;
	S32 mDriverVersionMinor;
	S32 mDriverVersionRelease;
	F32 mGLVersion; // e.g = 1.4
	S32 mGLSLVersionMajor;
	S32 mGLSLVersionMinor;

	S32 mVRAM; // VRAM in MB
	S32 mGLMaxVertexRange;
	S32 mGLMaxIndexRange;
	S32 mGLMaxTextureSize;

	std::string mDriverVersionVendorString;
	std::string mGLVersionString;

	// In ALL CAPS
	std::string mGLVendor;
	std::string mGLVendorShort;

	// In ALL CAPS
	std::string mGLRenderer;
};

extern LLGLManager gGLManager;

class LLQuaternion;
class LLMatrix4;

LL_NO_INLINE void log_glerror(const char* file, U32 line, bool crash = false);

#if LL_DEBUG
# define stop_glerror() if (LL_UNLIKELY(gDebugGL)) log_glerror(__FILE__, __LINE__, true)
#else
# define stop_glerror() if (LL_UNLIKELY(gDebugGL)) log_glerror(__FILE__, __LINE__)
#endif

void clear_glerror();

// This is a class for GL state management

/*
	GL STATE MANAGEMENT DESCRIPTION

	LLGLState and its two subclasses, LLGLEnable and LLGLDisable, manage the
	current enable/disable states of the GL to prevent redundant setting of
	state within a render path or the accidental corruption of what state the
	next path expects.

	Essentially, wherever you would call glEnable set a state and then
	subsequently reset it by calling glDisable (or vice versa), make an
	instance of LLGLEnable with the state you want to set, and assume it will
	be restored to its original state when that instance of LLGLEnable is
	destroyed. It is good practice to exploit stack frame controls for optimal
	setting/unsetting and readability of code. There are a collection of helper
	classes below that define groups of enables/disables and that can cause
    multiple states to be set with the creation of one instance.

	Sample usage:

	// Disable lighting for rendering hud objects:

	// INCORRECT USAGE
	LLGLEnable lighting(GL_LIGHTING);
	renderHUD();
	LLGLDisable lighting(GL_LIGHTING);

	// CORRECT USAGE
	{
		LLGLEnable lighting(GL_LIGHTING);
		renderHUD();
	}

	If a state is to be set on a conditional, the following mechanism
	is useful:

	{
		LLGLEnable lighting(light_hud ? GL_LIGHTING : 0);
		renderHUD();
	}

	A LLGLState initialized with a parameter of 0 does nothing.

	LLGLState works by maintaining a map of the current GL states, and ignoring
	redundant enables/disables. If a redundant call is attempted, it becomes a
	noop, otherwise, it is set in the constructor and reset in the destructor.

	For debugging GL state corruption, running with debug enabled will trigger
	asserts if the existing GL state does not match the expected GL state.

*/
class LLGLState
{
protected:
	LOG_CLASS(LLGLState);

public:
	static void initClass();
	static void restoreGL();

	static void resetTextureStates();
	static void dumpStates();
	
	static void check(bool states = true, bool texture_channels = false,
					  const std::string& msg = "", U32 data_mask = 0);

private:
	static void checkStates(const std::string& msg);
	static void checkTextureChannels(const std::string& msg);

protected:
	typedef fast_hmap<U32, GLboolean> state_map_t;
	static state_map_t sStateMap;

public:
	enum { CURRENT_STATE = -2 };
	LLGLState(U32 state, S32 enabled = CURRENT_STATE);
	~LLGLState();

	void setEnabled(S32 enabled);
	LL_INLINE void enable()						{ setEnabled(GL_TRUE); }
	LL_INLINE void disable()					{ setEnabled(GL_FALSE); }

protected:
	U32			mState;
	S32			mWasEnabled;
	S32			mIsEnabled;
};

class LLGLEnable : public LLGLState
{
public:
	LL_INLINE LLGLEnable(U32 state)
	:	LLGLState(state, GL_TRUE)
	{
	}
};

class LLGLDisable : public LLGLState
{
public:
	LL_INLINE LLGLDisable(U32 state)
	:	LLGLState(state, GL_FALSE)
	{
	}
};

/*
  Store and modify projection matrix to create an oblique projection that clips
  to the specified plane. Oblique projections alter values in the depth buffer,
  so this class should not be used mid-renderpass.

  Restores projection matrix on destruction.
  GL_MODELVIEW_MATRIX is active whenever program execution leaves this class.
  Does not stack.
*/

class alignas(16) LLGLUserClipPlane
{
public:
	LLGLUserClipPlane(const LLPlane& plane, const LLMatrix4a& modelview,
					  const LLMatrix4a& projection, bool apply = true);
	~LLGLUserClipPlane();

	void disable();
	void setPlane(F32 a, F32 b, F32 c, F32 d);

private:
	LLMatrix4a	mProjection;
	LLMatrix4a	mModelview;

	bool		mApply;
};

/*
  Modify and load projection matrix to push depth values to far clip plane.

  Restores projection matrix on destruction.
  Saves/restores matrix mode around projection manipulation.
  Does not stack.
*/

class LLGLSquashToFarClip
{
public:
	LLGLSquashToFarClip(U32 layer = 0);
	~LLGLSquashToFarClip();
};

/*
	Interface for objects that need periodic GL updates applied to them.
	Used to synchronize GL updates with GL thread.
*/

class LLGLUpdate
{
public:
	LLGLUpdate()
	:	mInQ(false)
	{
	}

	virtual ~LLGLUpdate()
	{
		if (mInQ)
		{
			std::list<LLGLUpdate*>::iterator end = sGLQ.end();
			std::list<LLGLUpdate*>::iterator iter = std::find(sGLQ.begin(),
															  end,
															  this);
			if (iter != end)
			{
				sGLQ.erase(iter);
			}
		}
	}

	virtual void updateGL() = 0;

public:
	static std::list<LLGLUpdate*> sGLQ;

	bool mInQ;
};

#if LL_USE_FENCE
constexpr U32 FENCE_WAIT_TIME_NANOSECONDS = 1000;  // 1 ms

class LLGLFence
{
public:
	virtual ~LLGLFence() {}

	virtual void placeFence() = 0;
	virtual bool isCompleted() = 0;
	virtual void wait() = 0;
};

class LLGLSyncFence : public LLGLFence
{
public:
	LLGLSyncFence();
	virtual ~LLGLSyncFence();

	void placeFence();
	bool isCompleted();
	void wait();

# ifdef GL_ARB_sync
public:
	GLsync mSync;
# endif
};
#endif	// LL_USE_FENCE

///////////////////////////////////////////////////////////////////////////////
// Formerly in llglstates.h, but since that header was #included by this one...
///////////////////////////////////////////////////////////////////////////////

class LLGLDepthTest
{
	// Enabled by default
public:
	LLGLDepthTest(GLboolean depth_enabled, GLboolean write_enabled = GL_TRUE,
				  U32 depth_func = GL_LEQUAL);

	~LLGLDepthTest();

	void checkState();

public:
	GLboolean			mPrevDepthEnabled;
	U32					mPrevDepthFunc;
	GLboolean			mPrevWriteEnabled;

private:
	static GLboolean	sDepthEnabled;	// defaults to GL_FALSE
	static U32			sDepthFunc;		// defaults to GL_LESS
	static GLboolean	sWriteEnabled;	// defaults to GL_TRUE
};

class LLGLSDefault
{
public:
	LLGLSDefault()
	:	// Enable
		mColorMaterial(GL_COLOR_MATERIAL),
		// Disable
		mAlphaTest(GL_ALPHA_TEST),
		mBlend(GL_BLEND),
		mCullFace(GL_CULL_FACE),
		mDither(GL_DITHER),
		mFog(GL_FOG),
		mLineSmooth(GL_LINE_SMOOTH),
		mLineStipple(GL_LINE_STIPPLE),
		mNormalize(GL_NORMALIZE),
		mPolygonSmooth(GL_POLYGON_SMOOTH),
		mGLMultisample(GL_MULTISAMPLE_ARB)
	{
	}

protected:
	LLGLEnable	mColorMaterial;
	LLGLDisable	mAlphaTest;
	LLGLDisable	mBlend;
	LLGLDisable	mCullFace;
	LLGLDisable	mDither;
	LLGLDisable	mFog;
	LLGLDisable	mLineSmooth;
	LLGLDisable	mLineStipple;
	LLGLDisable	mNormalize;
	LLGLDisable	mPolygonSmooth;
	LLGLDisable	mGLMultisample;
};

class LLGLSObjectSelect
{
public:
	LLGLSObjectSelect()
	:	mBlend(GL_BLEND),
		mFog(GL_FOG),
		mAlphaTest(GL_ALPHA_TEST),
		mCullFace(GL_CULL_FACE)
	{

	}

protected:
	LLGLDisable	mBlend;
	LLGLDisable	mFog;
	LLGLDisable	mAlphaTest;
	LLGLEnable	mCullFace;
};

class LLGLSObjectSelectAlpha
{
protected:
	LLGLEnable mAlphaTest;

public:
	LLGLSObjectSelectAlpha()
	:	mAlphaTest(GL_ALPHA_TEST)
	{
	}
};

class LLGLSUIDefault
{
public:
	LLGLSUIDefault()
	:	mBlend(GL_BLEND),
		mAlphaTest(GL_ALPHA_TEST),
		mCullFace(GL_CULL_FACE),
		mDepthTest(GL_FALSE, GL_TRUE, GL_LEQUAL),
		mMSAA(GL_MULTISAMPLE_ARB)
	{
	}

protected:
	LLGLEnable		mBlend;
	LLGLEnable		mAlphaTest;
	LLGLDisable		mCullFace;
	LLGLDepthTest	mDepthTest;
	LLGLDisable		mMSAA;
};

class LLGLSNoAlphaTest // : public LLGLSUIDefault
{
public:
	LLGLSNoAlphaTest()
	:	mAlphaTest(GL_ALPHA_TEST)
	{
	}

protected:
	LLGLDisable mAlphaTest;
};

class LLGLSFog
{
public:
	LLGLSFog()
	:	mFog(GL_FOG)
	{
	}

protected:
	LLGLEnable mFog;
};

class LLGLSNoFog
{
public:
	LLGLSNoFog()
	:	mFog(GL_FOG)
	{
	}

protected:
	LLGLDisable mFog;
};

class LLGLSPipeline
{
public:
	LLGLSPipeline()
	:	mCullFace(GL_CULL_FACE),
		mDepthTest(GL_TRUE, GL_TRUE, GL_LEQUAL)
	{
	}

protected:
	LLGLEnable		mCullFace;
	LLGLDepthTest	mDepthTest;
};

class LLGLSPipelineAlpha // : public LLGLSPipeline
{
public:
	LLGLSPipelineAlpha()
	:	mBlend(GL_BLEND),
		mAlphaTest(GL_ALPHA_TEST)
	{
	}

protected:
	LLGLEnable mBlend;
	LLGLEnable mAlphaTest;
};

class LLGLSPipelineEmbossBump
{
public:
	LLGLSPipelineEmbossBump()
	:	mFog(GL_FOG)
	{
	}

protected:
	LLGLDisable mFog;
};

class LLGLSPipelineSelection
{
public:
	LLGLSPipelineSelection()
	:	mCullFace(GL_CULL_FACE)
	{
	}

protected:
	LLGLDisable mCullFace;
};

class LLGLSPipelineAvatar
{
public:
	LLGLSPipelineAvatar()
	:	mNormalize(GL_NORMALIZE)
	{
	}

protected:
	LLGLEnable mNormalize;
};

class LLGLSPipelineSkyBoxLegacy
{
public:
	LLGLSPipelineSkyBoxLegacy()
	:	mAlphaTest(GL_ALPHA_TEST),
		mCullFace(GL_CULL_FACE),
		mFog(GL_FOG)
	{
	}

protected:
	LLGLDisable mAlphaTest;
	LLGLDisable mCullFace;
	LLGLDisable mFog;
};

class LLGLSPipelineSkyBox
{
public:
	LLGLSPipelineSkyBox();
	~LLGLSPipelineSkyBox();

protected:
	LLGLDisable			mAlphaTest;
	LLGLDisable			mCullFace;
	LLGLSquashToFarClip	mSquashClip;
};

class LLGLSPipelineDepthTestSkyBox : public LLGLSPipelineSkyBox
{
public:
	LLGLSPipelineDepthTestSkyBox(GLboolean depth_test, GLboolean depth_write);

public:
	LLGLDepthTest mDepth;
};

class LLGLSPipelineBlendSkyBox : public LLGLSPipelineDepthTestSkyBox
{
public:
	LLGLSPipelineBlendSkyBox(GLboolean depth_test, GLboolean depth_write);

public:
	LLGLEnable mBlend;
};

class LLGLSTracker
{
public:
	LLGLSTracker()
	:	mCullFace(GL_CULL_FACE),
		mBlend(GL_BLEND),
		mAlphaTest(GL_ALPHA_TEST)
	{
	}

protected:
	LLGLEnable mCullFace;
	LLGLEnable mBlend;
	LLGLEnable mAlphaTest;
};

class LLGLSSpecular
{
public:
	LLGLSSpecular(const LLColor4& color, F32 shininess)
	{
		mShininess = shininess;
		if (mShininess > 0.f)
		{
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, color.mV);
			S32 shiny = (S32)(shininess * 128.f);
			shiny = llclamp(shiny, 0, 128);
			glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, shiny);
		}
	}

	~LLGLSSpecular()
	{
		if (mShininess > 0.f)
		{
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,
						 LLColor4::transparent.mV);
			glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 0);
		}
	}

public:
	F32 mShininess;
};

///////////////////////////////////////////////////////////////////////////////

const std::string getGLErrorString(U32 error);

// Deal with changing glext.h definitions for newer SDK versions, specifically
// with MAC OSX 10.5 -> 10.6

#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_EXT
#endif

#ifndef GL_STENCIL_ATTACHMENT
#define GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_EXT
#endif

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
#define GL_DRAW_FRAMEBUFFER GL_DRAW_FRAMEBUFFER_EXT
#define GL_READ_FRAMEBUFFER GL_READ_FRAMEBUFFER_EXT
#define GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_EXT
#define GL_FRAMEBUFFER_UNSUPPORTED GL_FRAMEBUFFER_UNSUPPORTED_EXT
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT
#define glGenFramebuffers glGenFramebuffersEXT
#define glBindFramebuffer glBindFramebufferEXT
#define glCheckFramebufferStatus glCheckFramebufferStatusEXT
#define glBlitFramebuffer glBlitFramebufferEXT
#define glDeleteFramebuffers glDeleteFramebuffersEXT
#define glFramebufferRenderbuffer glFramebufferRenderbufferEXT
#define glFramebufferTexture2D glFramebufferTexture2DEXT
#endif

#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER GL_RENDERBUFFER_EXT
#define glGenRenderbuffers glGenRenderbuffersEXT
#define glBindRenderbuffer glBindRenderbufferEXT
#define glRenderbufferStorage glRenderbufferStorageEXT
#define glRenderbufferStorageMultisample glRenderbufferStorageMultisampleEXT
#define glDeleteRenderbuffers glDeleteRenderbuffersEXT
#endif

#ifndef GL_COLOR_ATTACHMENT
#define GL_COLOR_ATTACHMENT GL_COLOR_ATTACHMENT_EXT
#endif

#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#endif

#ifndef GL_COLOR_ATTACHMENT1
#define GL_COLOR_ATTACHMENT1 GL_COLOR_ATTACHMENT1_EXT
#endif

#ifndef GL_COLOR_ATTACHMENT2
#define GL_COLOR_ATTACHMENT2 GL_COLOR_ATTACHMENT2_EXT
#endif

#ifndef GL_COLOR_ATTACHMENT3
#define GL_COLOR_ATTACHMENT3 GL_COLOR_ATTACHMENT3_EXT
#endif

#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_EXT
#endif

#endif // LL_LLGL_H

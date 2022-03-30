/**
 * @file llgl.cpp
 * @brief LLGL implementation
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

// This file sets some global GL parameters, and implements some
// useful functions for GL operations.

#define GLH_EXT_SINGLE_FILE

#include "linden_common.h"

#include "boost/tokenizer.hpp"

#include "llgl.h"

#include "llglslshader.h"
#include "llimagegl.h"
#include "llmath.h"
#include "llquaternion.h"
#include "llrender.h"
#include "llsys.h"
#include "llmatrix4.h"

#if LL_WINDOWS
# include "lldxhardware.h"
#endif

#define MAX_GL_TEXTURE_UNITS 16

bool gDebugGL = false;
// When true, avoids sync-related stalls with NVIDIA GPUs.
// Note: from what I found out during web searches, this optimization seems to
// badly impact ATI GPUs; not sure for Intel... So, it's currently only enabled
// by default for NVIDIA GPUs and users of other GPUs may toggle it via an
// advanced setting. HB
bool gClearARBbuffer = false;

std::list<LLGLUpdate*> LLGLUpdate::sGLQ;

#if (LL_WINDOWS || LL_LINUX)

# if LL_WINDOWS
PFNGLGETSTRINGIPROC glGetStringi = NULL;
# endif

// Vertex blending prototypes
PFNGLWEIGHTPOINTERARBPROC glWeightPointerARB = NULL;
PFNGLVERTEXBLENDARBPROC glVertexBlendARB = NULL;
PFNGLWEIGHTFVARBPROC glWeightfvARB = NULL;

// Vertex buffer object prototypes
PFNGLBINDBUFFERARBPROC glBindBufferARB = NULL;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB = NULL;
PFNGLGENBUFFERSARBPROC glGenBuffersARB = NULL;
PFNGLISBUFFERARBPROC glIsBufferARB = NULL;
PFNGLBUFFERDATAARBPROC glBufferDataARB = NULL;
PFNGLBUFFERSUBDATAARBPROC glBufferSubDataARB = NULL;
PFNGLGETBUFFERSUBDATAARBPROC glGetBufferSubDataARB = NULL;
PFNGLMAPBUFFERARBPROC glMapBufferARB = NULL;
PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB = NULL;
PFNGLGETBUFFERPARAMETERIVARBPROC glGetBufferParameterivARB = NULL;
PFNGLGETBUFFERPOINTERVARBPROC glGetBufferPointervARB = NULL;

// GL_ARB_vertex_array_object
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = NULL;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
PFNGLISVERTEXARRAYPROC glIsVertexArray = NULL;

// GL_ARB_map_buffer_range
PFNGLMAPBUFFERRANGEPROC glMapBufferRange = NULL;
PFNGLFLUSHMAPPEDBUFFERRANGEPROC glFlushMappedBufferRange = NULL;

// GL_ARB_sync
PFNGLFENCESYNCPROC glFenceSync = NULL;
PFNGLISSYNCPROC glIsSync = NULL;
PFNGLDELETESYNCPROC glDeleteSync = NULL;
PFNGLCLIENTWAITSYNCPROC glClientWaitSync = NULL;
PFNGLWAITSYNCPROC glWaitSync = NULL;
PFNGLGETINTEGER64VPROC glGetInteger64v = NULL;
PFNGLGETSYNCIVPROC glGetSynciv = NULL;

// GL_APPLE_flush_buffer_range
PFNGLBUFFERPARAMETERIAPPLEPROC glBufferParameteriAPPLE = NULL;
PFNGLFLUSHMAPPEDBUFFERRANGEAPPLEPROC glFlushMappedBufferRangeAPPLE = NULL;

// GL_ARB_occlusion_query
PFNGLGENQUERIESARBPROC glGenQueriesARB = NULL;
PFNGLDELETEQUERIESARBPROC glDeleteQueriesARB = NULL;
PFNGLISQUERYARBPROC glIsQueryARB = NULL;
PFNGLBEGINQUERYARBPROC glBeginQueryARB = NULL;
PFNGLENDQUERYARBPROC glEndQueryARB = NULL;
PFNGLGETQUERYIVARBPROC glGetQueryivARB = NULL;
PFNGLGETQUERYOBJECTIVARBPROC glGetQueryObjectivARB = NULL;
PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuivARB = NULL;

// GL_ARB_timer_query
PFNGLQUERYCOUNTERPROC glQueryCounter = NULL;
PFNGLGETQUERYOBJECTI64VPROC glGetQueryObjecti64v = NULL;
PFNGLGETQUERYOBJECTUI64VPROC glGetQueryObjectui64v = NULL;

// GL_ARB_point_parameters
PFNGLPOINTPARAMETERFARBPROC glPointParameterfARB = NULL;
PFNGLPOINTPARAMETERFVARBPROC glPointParameterfvARB = NULL;

// GL_ARB_framebuffer_object
PFNGLISRENDERBUFFERPROC glIsRenderbuffer = NULL;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = NULL;
PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers = NULL;
PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers = NULL;
PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = NULL;
PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv = NULL;
PFNGLISFRAMEBUFFERPROC glIsFramebuffer = NULL;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = NULL;
PFNGLFRAMEBUFFERTEXTURE1DPROC glFramebufferTexture1D = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;
PFNGLFRAMEBUFFERTEXTURE3DPROC glFramebufferTexture3D = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = NULL;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv = NULL;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap = NULL;
PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer = NULL;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample = NULL;
PFNGLFRAMEBUFFERTEXTURELAYERPROC glFramebufferTextureLayer = NULL;

// GL_ARB_texture_multisample
PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisample = NULL;
PFNGLTEXIMAGE3DMULTISAMPLEPROC glTexImage3DMultisample = NULL;
PFNGLGETMULTISAMPLEFVPROC glGetMultisamplefv = NULL;
PFNGLSAMPLEMASKIPROC glSampleMaski = NULL;

// GL_ARB_debug_output
PFNGLDEBUGMESSAGECONTROLARBPROC glDebugMessageControlARB = NULL;
PFNGLDEBUGMESSAGEINSERTARBPROC glDebugMessageInsertARB = NULL;
PFNGLDEBUGMESSAGECALLBACKARBPROC glDebugMessageCallbackARB = NULL;
PFNGLGETDEBUGMESSAGELOGARBPROC glGetDebugMessageLogARB = NULL;

// GL_EXT_blend_func_separate
PFNGLBLENDFUNCSEPARATEEXTPROC glBlendFuncSeparateEXT = NULL;

// GL_ARB_draw_buffers
PFNGLDRAWBUFFERSARBPROC glDrawBuffersARB = NULL;

// Shader object prototypes
PFNGLDELETEOBJECTARBPROC glDeleteObjectARB = NULL;
PFNGLGETHANDLEARBPROC glGetHandleARB = NULL;
PFNGLDETACHOBJECTARBPROC glDetachObjectARB = NULL;
PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB = NULL;
PFNGLSHADERSOURCEARBPROC glShaderSourceARB = NULL;
PFNGLCOMPILESHADERARBPROC glCompileShaderARB = NULL;
PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB = NULL;
PFNGLATTACHOBJECTARBPROC glAttachObjectARB = NULL;
PFNGLLINKPROGRAMARBPROC glLinkProgramARB = NULL;
PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB = NULL;
PFNGLVALIDATEPROGRAMARBPROC glValidateProgramARB = NULL;
PFNGLUNIFORM1FARBPROC glUniform1fARB = NULL;
PFNGLUNIFORM2FARBPROC glUniform2fARB = NULL;
PFNGLUNIFORM3FARBPROC glUniform3fARB = NULL;
PFNGLUNIFORM4FARBPROC glUniform4fARB = NULL;
PFNGLUNIFORM1IARBPROC glUniform1iARB = NULL;
PFNGLUNIFORM2IARBPROC glUniform2iARB = NULL;
PFNGLUNIFORM3IARBPROC glUniform3iARB = NULL;
PFNGLUNIFORM4IARBPROC glUniform4iARB = NULL;
PFNGLUNIFORM1FVARBPROC glUniform1fvARB = NULL;
PFNGLUNIFORM2FVARBPROC glUniform2fvARB = NULL;
PFNGLUNIFORM3FVARBPROC glUniform3fvARB = NULL;
PFNGLUNIFORM4FVARBPROC glUniform4fvARB = NULL;
PFNGLUNIFORM1IVARBPROC glUniform1ivARB = NULL;
PFNGLUNIFORM2IVARBPROC glUniform2ivARB = NULL;
PFNGLUNIFORM3IVARBPROC glUniform3ivARB = NULL;
PFNGLUNIFORM4IVARBPROC glUniform4ivARB = NULL;
PFNGLUNIFORMMATRIX2FVARBPROC glUniformMatrix2fvARB = NULL;
PFNGLUNIFORMMATRIX3FVARBPROC glUniformMatrix3fvARB = NULL;
PFNGLUNIFORMMATRIX3X4FVPROC glUniformMatrix3x4fv = NULL;
PFNGLUNIFORMMATRIX4FVARBPROC glUniformMatrix4fvARB = NULL;
PFNGLGETOBJECTPARAMETERFVARBPROC glGetObjectParameterfvARB = NULL;
PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB = NULL;
PFNGLGETINFOLOGARBPROC glGetInfoLogARB = NULL;
PFNGLGETATTACHEDOBJECTSARBPROC glGetAttachedObjectsARB = NULL;
PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB = NULL;
PFNGLGETACTIVEUNIFORMARBPROC glGetActiveUniformARB = NULL;
PFNGLGETUNIFORMFVARBPROC glGetUniformfvARB = NULL;
PFNGLGETUNIFORMIVARBPROC glGetUniformivARB = NULL;
PFNGLGETSHADERSOURCEARBPROC glGetShaderSourceARB = NULL;
PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer = NULL;

# if LL_WINDOWS
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;
# endif

// Vertex shader prototypes
# if LL_LINUX
PFNGLVERTEXATTRIB1DARBPROC glVertexAttrib1dARB = NULL;
PFNGLVERTEXATTRIB1DVARBPROC glVertexAttrib1dvARB = NULL;
PFNGLVERTEXATTRIB1FARBPROC glVertexAttrib1fARB = NULL;
PFNGLVERTEXATTRIB1FVARBPROC glVertexAttrib1fvARB = NULL;
PFNGLVERTEXATTRIB1SARBPROC glVertexAttrib1sARB = NULL;
PFNGLVERTEXATTRIB1SVARBPROC glVertexAttrib1svARB = NULL;
PFNGLVERTEXATTRIB2DARBPROC glVertexAttrib2dARB = NULL;
PFNGLVERTEXATTRIB2DVARBPROC glVertexAttrib2dvARB = NULL;
PFNGLVERTEXATTRIB2FARBPROC glVertexAttrib2fARB = NULL;
PFNGLVERTEXATTRIB2FVARBPROC glVertexAttrib2fvARB = NULL;
PFNGLVERTEXATTRIB2SARBPROC glVertexAttrib2sARB = NULL;
PFNGLVERTEXATTRIB2SVARBPROC glVertexAttrib2svARB = NULL;
PFNGLVERTEXATTRIB3DARBPROC glVertexAttrib3dARB = NULL;
PFNGLVERTEXATTRIB3DVARBPROC glVertexAttrib3dvARB = NULL;
PFNGLVERTEXATTRIB3FARBPROC glVertexAttrib3fARB = NULL;
PFNGLVERTEXATTRIB3FVARBPROC glVertexAttrib3fvARB = NULL;
PFNGLVERTEXATTRIB3SARBPROC glVertexAttrib3sARB = NULL;
PFNGLVERTEXATTRIB3SVARBPROC glVertexAttrib3svARB = NULL;
# endif // LL_LINUX
PFNGLVERTEXATTRIB4NBVARBPROC glVertexAttrib4nbvARB = NULL;
PFNGLVERTEXATTRIB4NIVARBPROC glVertexAttrib4nivARB = NULL;
PFNGLVERTEXATTRIB4NSVARBPROC glVertexAttrib4nsvARB = NULL;
PFNGLVERTEXATTRIB4NUBARBPROC glVertexAttrib4nubARB = NULL;
PFNGLVERTEXATTRIB4NUBVARBPROC glVertexAttrib4nubvARB = NULL;
PFNGLVERTEXATTRIB4NUIVARBPROC glVertexAttrib4nuivARB = NULL;
PFNGLVERTEXATTRIB4NUSVARBPROC glVertexAttrib4nusvARB = NULL;
# if LL_LINUX
PFNGLVERTEXATTRIB4BVARBPROC glVertexAttrib4bvARB = NULL;
PFNGLVERTEXATTRIB4DARBPROC glVertexAttrib4dARB = NULL;
PFNGLVERTEXATTRIB4DVARBPROC glVertexAttrib4dvARB = NULL;
PFNGLVERTEXATTRIB4FARBPROC glVertexAttrib4fARB = NULL;
PFNGLVERTEXATTRIB4FVARBPROC glVertexAttrib4fvARB = NULL;
PFNGLVERTEXATTRIB4IVARBPROC glVertexAttrib4ivARB = NULL;
PFNGLVERTEXATTRIB4SARBPROC glVertexAttrib4sARB = NULL;
PFNGLVERTEXATTRIB4SVARBPROC glVertexAttrib4svARB = NULL;
PFNGLVERTEXATTRIB4UBVARBPROC glVertexAttrib4ubvARB = NULL;
PFNGLVERTEXATTRIB4UIVARBPROC glVertexAttrib4uivARB = NULL;
PFNGLVERTEXATTRIB4USVARBPROC glVertexAttrib4usvARB = NULL;
PFNGLVERTEXATTRIBPOINTERARBPROC glVertexAttribPointerARB = NULL;
PFNGLENABLEVERTEXATTRIBARRAYARBPROC glEnableVertexAttribArrayARB = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArrayARB = NULL;
PFNGLPROGRAMSTRINGARBPROC glProgramStringARB = NULL;
PFNGLBINDPROGRAMARBPROC glBindProgramARB = NULL;
PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB = NULL;
PFNGLGENPROGRAMSARBPROC glGenProgramsARB = NULL;
PFNGLPROGRAMENVPARAMETER4DARBPROC glProgramEnvParameter4dARB = NULL;
PFNGLPROGRAMENVPARAMETER4DVARBPROC glProgramEnvParameter4dvARB = NULL;
PFNGLPROGRAMENVPARAMETER4FARBPROC glProgramEnvParameter4fARB = NULL;
PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fvARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4DARBPROC glProgramLocalParameter4dARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4DVARBPROC glProgramLocalParameter4dvARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC glProgramLocalParameter4fvARB = NULL;
PFNGLGETPROGRAMENVPARAMETERDVARBPROC glGetProgramEnvParameterdvARB = NULL;
PFNGLGETPROGRAMENVPARAMETERFVARBPROC glGetProgramEnvParameterfvARB = NULL;
PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC glGetProgramLocalParameterdvARB = NULL;
PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC glGetProgramLocalParameterfvARB = NULL;
PFNGLGETPROGRAMIVARBPROC glGetProgramivARB = NULL;
PFNGLGETPROGRAMSTRINGARBPROC glGetProgramStringARB = NULL;
PFNGLGETVERTEXATTRIBDVARBPROC glGetVertexAttribdvARB = NULL;
PFNGLGETVERTEXATTRIBFVARBPROC glGetVertexAttribfvARB = NULL;
PFNGLGETVERTEXATTRIBIVARBPROC glGetVertexAttribivARB = NULL;
PFNGLGETVERTEXATTRIBPOINTERVARBPROC glGetVertexAttribPointervARB = NULL;
PFNGLISPROGRAMARBPROC glIsProgramARB = NULL;
# endif // LL_LINUX
PFNGLBINDATTRIBLOCATIONARBPROC glBindAttribLocationARB = NULL;
PFNGLGETACTIVEATTRIBARBPROC glGetActiveAttribARB = NULL;
PFNGLGETATTRIBLOCATIONARBPROC glGetAttribLocationARB = NULL;

# if LL_WINDOWS
PFNWGLGETGPUIDSAMDPROC wglGetGPUIDsAMD = NULL;
PFNWGLGETGPUINFOAMDPROC wglGetGPUInfoAMD = NULL;
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
# endif

# if LL_LINUX_NV_GL_HEADERS
// Linux NVIDIA headers. These define these differently to Mesa's.  ugh.
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB = NULL;
PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements = NULL;
# endif
#endif	// (LL_WINDOWS || LL_LINUX)

// Utility functions

void log_glerror(const char* file, U32 line, bool crash)
{
	static std::string filename;
	GLenum error = glGetError();
	if (LL_UNLIKELY(error))
	{
		filename.assign(file);
		size_t i = filename.find("indra");
		if (i != std::string::npos)
		{
			filename = filename.substr(i);
		}
	}
	while (LL_UNLIKELY(error))
	{
		std::string gl_error_msg = getGLErrorString(error);
		if (crash)
		{
			llerrs << "GL Error: " << gl_error_msg << " (" << error
				   << ") - in file: " << filename << " - at line: "
				   << line << llendl;
		}
		else
		{
			llwarns << "GL Error: " << gl_error_msg << " (" << error
					<< ") - in file: " << filename << " - at line: "
					<< line << llendl;
		}
		error = glGetError();
	}
}

// There are 7 non-zero error flags, one of them being cleared on each call to
// glGetError(). Normally, all error flags should therefore get cleared after
// at most 7 calls to glGetError() and the 8th call should always return 0...
// See: http://www.opengl.org/sdk/docs/man/xhtml/glGetError.xml
#define MAX_LOOPS 8U
void clear_glerror()
{
	U32 counter = MAX_LOOPS;
	if (LL_UNLIKELY(gDebugGL))
	{
		GLenum error;
		while ((error = glGetError()))
		{
			if (--counter == 0)
			{
				llwarns << "glGetError() still returning errors ("
						<< getGLErrorString(error) <<") after "
						<< MAX_LOOPS << " consecutive calls." << llendl;
				break;
			}
			else
			{
				llwarns << "glGetError() returned error: "
						<< getGLErrorString(error) << llendl;
			}
		}
	}
	else
	{
		// Fast code, for when gDebugGL is false
		while (glGetError() && --counter != 0) ;
	}
}

const std::string getGLErrorString(U32 error)
{
	switch (error)
	{
		case GL_NO_ERROR:
			return "GL_NO_ERROR";
			break;

		case GL_INVALID_ENUM:
			return "GL_INVALID_ENUM";
			break;

		case GL_INVALID_VALUE:
			return "GL_INVALID_VALUE";
			break;

		case GL_INVALID_OPERATION:
			return "GL_INVALID_OPERATION";
			break;

		case GL_INVALID_FRAMEBUFFER_OPERATION:
			return "GL_INVALID_FRAMEBUFFER_OPERATION";
			break;

		case GL_OUT_OF_MEMORY:
			return "GL_OUT_OF_MEMORY";
			break;

		case GL_STACK_UNDERFLOW:
			return "GL_STACK_UNDERFLOW";
			break;

		case GL_STACK_OVERFLOW:
			return "GL_STACK_OVERFLOW";
			break;

		default:
			return llformat("Unknown GL error #%d", error);
	}
}

static void parse_gl_version(S32* major, S32* minor, S32* release,
							 std::string* vendor_specific,
							 std::string* version_string)
{
	*major = 0;
	*minor = 0;
	*release = 0;

	// GL_VERSION returns a null-terminated string with the format:
	// <major>.<minor>[.<release>] [<vendor specific>]
	const char* version = (const char*)glGetString(GL_VERSION);
	if (!version)
	{
		vendor_specific->clear();
		return;
	}

	version_string->assign(version);

	std::string ver_copy(version);
	S32 len = (S32)strlen(version);
	S32 i = 0;
	S32 start;
	// Find the major version
	start = i;
	for ( ; i < len; ++i)
	{
		if ('.' == version[i])
		{
			break;
		}
	}
	std::string major_str = ver_copy.substr(start, i - start);
	LLStringUtil::convertToS32(major_str, *major);

	if ('.' == version[i])
	{
		i++;
	}

	// Find the minor version
	start = i;
	for ( ; i < len; ++i)
	{
		if (version[i] == '.' || isspace(version[i]))
		{
			break;
		}
	}
	std::string minor_str = ver_copy.substr(start, i - start);
	LLStringUtil::convertToS32(minor_str, *minor);

	// Find the release number (optional)
	if ('.' == version[i])
	{
		i++;

		start = i;
		for ( ; i < len; ++i)
		{
			if (isspace(version[i]))
			{
				break;
			}
		}

		std::string release_str = ver_copy.substr(start, i - start);
		LLStringUtil::convertToS32(release_str, *release);
	}

	// Skip over any white space
	while (version[i] && isspace(version[i]))
	{
		++i;
	}

	// Copy the vendor-specific string (optional)
	if (version[i])
	{
		vendor_specific->assign(version + i);
	}
}

static void parse_glsl_version(S32& major, S32& minor)
{
	major = minor = 0;

	// GL_SHADING_LANGUAGE_VERSION returns a null-terminated string with the
	// format: <major>.<minor>[.<release>] [<vendor specific>]
	const char* version =
		(const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
	if (!version)
	{
		return;
	}

	std::string ver_copy(version);
	S32 len = (S32)strlen(version);
	S32 i = 0;
	S32 start;
	// Find the major version
	start = i;
	for ( ; i < len; ++i)
	{
		if ( '.' == version[i])
		{
			break;
		}
	}
	std::string major_str = ver_copy.substr(start, i - start);
	LLStringUtil::convertToS32(major_str, major);

	if ('.' == version[i])
	{
		i++;
	}

	// Find the minor version
	start = i;
	for ( ; i < len; ++i)
	{
		if (('.' == version[i]) || isspace(version[i]))
		{
			break;
		}
	}
	std::string minor_str = ver_copy.substr(start, i - start);
	LLStringUtil::convertToS32(minor_str, minor);
}

///////////////////////////////////////////////////////////////////////////////
// LLGLManager class
///////////////////////////////////////////////////////////////////////////////

LLGLManager gGLManager;

LLGLManager::LLGLManager()
:	mInited(false),
	mIsDisabled(false),
	mHasMultitexture(false),
	mHasATIMemInfo(false),
	mHasAMDAssociations(false),
	mHasNVXMemInfo(false),
	mNumTextureUnits(1),
	mHasMipMapGeneration(false),
	mHasCompressedTextures(false),
	mHasFramebufferObject(false),
	mMaxSamples(0),
	mHasBlendFuncSeparate(false),
	mHasSync(false),
	mHasVertexBufferObject(false),
	mHasVertexArrayObject(false),
	mHasMapBufferRange(false),
	mHasFlushBufferRange(false),
	mHasPBuffer(false),
	mNumTextureImageUnits(0),
	mHasOcclusionQuery(false),
	mHasOcclusionQuery2(false),
	mHasTimerQuery(false),
	mHasPointParameters(false),
	mHasDrawBuffers(false),
	mHasTextureRectangle(false),
	mHasTextureMultisample(false),
	mMaxSampleMaskWords(0),
	mMaxColorTextureSamples(0),
	mMaxDepthTextureSamples(0),
	mMaxIntegerSamples(0),

	mHasAnisotropic(false),
	mHasARBEnvCombine(false),
	mHasCubeMap(false),
	mHasDebugOutput(false),
	mHasTextureSwizzle(false),
	mHasGpuShader5(false),

	mUseDepthClamp(false),

	mIsAMD(false),
	mIsNVIDIA(false),
	mIsIntel(false),
	mIsGF2or4MX(false),
	mATIOffsetVerticalLines(false),
	mATIOldDriver(false),

#if LL_DARWIN
	mIsMobileGF(false),
#endif
	mHasRequirements(true),

	mHasSeparateSpecularColor(false),
	mHasVertexAttribIPointer(false),

	mDebugGPU(false),

	mDriverVersionMajor(1),
	mDriverVersionMinor(0),
	mDriverVersionRelease(0),
	mGLVersion(1.f),
	mGLSLVersionMajor(0),
	mGLSLVersionMinor(0),
	mVRAM(0),
	mGLMaxVertexRange(0),
	mGLMaxIndexRange(0)
{
}

//---------------------------------------------------------------------
// Global initialization for GL
//---------------------------------------------------------------------
#if LL_WINDOWS
void LLGLManager::initWGL()
{
	mHasPBuffer = false;
	if (!glh_init_extensions("WGL_ARB_pixel_format"))
	{
		llwarns << "No ARB pixel format extensions" << llendl;
	}

	if (ExtensionExists("WGL_ARB_create_context", gGLHExts.mSysExts))
	{
		GLH_EXT_NAME(wglCreateContextAttribsARB) =
			(PFNWGLCREATECONTEXTATTRIBSARBPROC)GLH_EXT_GET_PROC_ADDRESS("wglCreateContextAttribsARB");
	}
	else
	{
		llwarns << "No ARB create context extensions" << llendl;
	}

	// For retreiving information per AMD adapter, because we cannot trust
	// currently selected/default one when there are multiple
	mHasAMDAssociations = ExtensionExists("WGL_AMD_gpu_association",
										  gGLHExts.mSysExts);
	if (mHasAMDAssociations)
	{
		GLH_EXT_NAME(wglGetGPUIDsAMD) =
			(PFNWGLGETGPUIDSAMDPROC)GLH_EXT_GET_PROC_ADDRESS("wglGetGPUIDsAMD");
		GLH_EXT_NAME(wglGetGPUInfoAMD) =
			(PFNWGLGETGPUINFOAMDPROC)GLH_EXT_GET_PROC_ADDRESS("wglGetGPUInfoAMD");
	}

	if (ExtensionExists("WGL_EXT_swap_control", gGLHExts.mSysExts))
	{
		GLH_EXT_NAME(wglSwapIntervalEXT) =
			(PFNWGLSWAPINTERVALEXTPROC)GLH_EXT_GET_PROC_ADDRESS("wglSwapIntervalEXT");
	}

	if (!glh_init_extensions("WGL_ARB_pbuffer"))
	{
		llwarns << "No ARB WGL PBuffer extensions" << llendl;
	}

	if (!glh_init_extensions("WGL_ARB_render_texture"))
	{
		llwarns << "No ARB WGL render texture extensions" << llendl;
	}

	mHasPBuffer =
		ExtensionExists("WGL_ARB_pbuffer", gGLHExts.mSysExts) &&
		ExtensionExists("WGL_ARB_render_texture", gGLHExts.mSysExts) &&
		ExtensionExists("WGL_ARB_pixel_format", gGLHExts.mSysExts);
}
#endif

// Return false if unable (or unwilling due to old drivers) to init GL
bool LLGLManager::initGL()
{
	if (mInited)
	{
		llerrs << "Calling init on LLGLManager after already initialized !"
			   << llendl;
	}

	stop_glerror();

#if LL_WINDOWS
	if (!glGetStringi)
	{
		glGetStringi =
			(PFNGLGETSTRINGIPROC)GLH_EXT_GET_PROC_ADDRESS("glGetStringi");
	}

	// Reload extensions string (may have changed after using
	// wglCreateContextAttrib)
	if (glGetStringi)
	{
		std::stringstream str;

		GLint count = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &count);
		for (GLint i = 0; i < count; ++i)
		{
			std::string ext =
				ll_safe_string((const char*)glGetStringi(GL_EXTENSIONS, i));
			str << ext << " ";
			LL_DEBUGS("GLExtensions") << ext << llendl;
		}

		PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = 0;
		wglGetExtensionsStringARB =
			(PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
		if (wglGetExtensionsStringARB)
		{
			str << (const char*)wglGetExtensionsStringARB(wglGetCurrentDC());
		}

		free(gGLHExts.mSysExts);
		std::string extensions = str.str();
		gGLHExts.mSysExts = strdup(extensions.c_str());
	}
#endif

	stop_glerror();

	// Extract video card strings and convert to upper case to work around
	// driver-to-driver variation in capitalization.
	mGLVendor = ll_safe_string((const char*)glGetString(GL_VENDOR));
	LLStringUtil::toUpper(mGLVendor);

	mGLRenderer = ll_safe_string((const char*)glGetString(GL_RENDERER));
	LLStringUtil::toUpper(mGLRenderer);

	parse_gl_version(&mDriverVersionMajor, &mDriverVersionMinor,
					 &mDriverVersionRelease, &mDriverVersionVendorString,
					 &mGLVersionString);

	mGLVersion = mDriverVersionMajor + mDriverVersionMinor * 0.1f;

	if (mGLVersion >= 2.f)
	{
		parse_glsl_version(mGLSLVersionMajor, mGLSLVersionMinor);

#if LL_DARWIN
		// Never use GLSL greater than 1.20 on OSX
		if (mGLSLVersionMajor > 1 || mGLSLVersionMinor >= 30)
		{
			mGLSLVersionMajor = 1;
			mGLSLVersionMinor = 20;
		}
#endif
	}

	// We do not support any more fixed GL funcrions, so we need at the minimum
	// support for GLSL v1.10 so to load our basic shaders...
	if (mGLSLVersionMajor < 2 && mGLSLVersionMinor < 10)
	{
		mHasRequirements = false;
	}

	if (mGLVersion >= 2.1f && LLImageGL::sCompressTextures)
	{
		// Use texture compression
		glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);
	}
	else
	{
		// GL version is < 3.0, always disable texture compression
		LLImageGL::sCompressTextures = false;
	}

	// Trailing space necessary to keep "nVidia Corpor_ati_on" cards from being
	// recognized as ATI/AMD.
	// Note: AMD has been pretty good about not breaking this check, do not
	// rename without a good reason.
	if (mGLVendor.substr(0, 4) == "ATI ")
	{
		mGLVendorShort = "AMD";
		mIsAMD = true;

#if LL_WINDOWS
		if (mDriverVersionRelease < 3842)
		{
			mATIOffsetVerticalLines = true;
		}
#endif

#if (LL_WINDOWS || LL_LINUX)
		// Count any pre OpenGL 3.0 implementation as an old driver
		if (mGLVersion < 3.f)
		{
			mATIOldDriver = true;
		}
#endif
	}
	else if (mGLVendor.find("NVIDIA ") != std::string::npos)
	{
		mGLVendorShort = "NVIDIA";
		mIsNVIDIA = true;
		if (mGLRenderer.find("GEFORCE4 MX") != std::string::npos ||
			mGLRenderer.find("GEFORCE2") != std::string::npos ||
			mGLRenderer.find("GEFORCE 2") != std::string::npos ||
			mGLRenderer.find("GEFORCE4 460 GO") != std::string::npos ||
			mGLRenderer.find("GEFORCE4 440 GO") != std::string::npos ||
			mGLRenderer.find("GEFORCE4 420 GO") != std::string::npos)
		{
			mIsGF2or4MX = true;
		}
#if LL_DARWIN
		else if (mGLRenderer.find("9400M") != std::string::npos ||
				 mGLRenderer.find("9600M") != std::string::npos ||
				 mGLRenderer.find("9800M") != std::string::npos)
		{
			mIsMobileGF = true;
		}
#endif
		// Enable the vertex buffer kick-ass optimization for NVIDIA
		gClearARBbuffer = true;
	}
	else if (mGLVendor.find("INTEL") != std::string::npos
#if LL_LINUX
		 // The Mesa-based drivers put this in the Renderer string, not the
		// Vendor string.
		 || mGLRenderer.find("INTEL") != std::string::npos
#endif
		)
	{
		mGLVendorShort = "INTEL";
		mIsIntel = true;
	}
	else
	{
		mGLVendorShort = "MISC";
	}

	// This is called here because it depends on the setting of mIsGF2or4MX,
	// and sets up mHasMultitexture.
	initExtensions();
	stop_glerror();

	S32 old_vram = mVRAM;

#if LL_WINDOWS
	if (mHasAMDAssociations)
	{
		GLuint gl_gpus_count = wglGetGPUIDsAMD(0, 0);
		if (gl_gpus_count > 0)
		{
			GLuint* ids = new GLuint[gl_gpus_count];
			wglGetGPUIDsAMD(gl_gpus_count, ids);

			GLuint mem_mb = 0;
			for (U32 i = 0; i < gl_gpus_count; ++i)
			{
				wglGetGPUInfoAMD(ids[i], WGL_GPU_RAM_AMD, GL_UNSIGNED_INT,
								 sizeof(GLuint), &mem_mb);
				if (mVRAM < mem_mb)
				{
					// Basically pick the best AMD and trust driver/OS to know
					// to switch
					mVRAM = mem_mb;
				}
			}
		}
		if (mVRAM)
		{
			llinfos << "Detected VRAM via AMDAssociations: " << mVRAM
					<< llendl;
		}
	}
#endif

	if (mHasATIMemInfo && mVRAM == 0)
	{
		// Ask GL how much vram is free at startup and attempt to use no more
		// than half of that
		S32 meminfo[4];
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, meminfo);
		mVRAM = meminfo[0] / 1024;
		llinfos << "Detected VRAM via ATIMemInfo: " << mVRAM << llendl;
	}
	else if (mHasNVXMemInfo)
	{
		S32 dedicated_memory;
		glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX,
					  &dedicated_memory);
		mVRAM = dedicated_memory / 1024;
		llinfos << "Detected VRAM via NVXMemInfo: " << mVRAM << llendl;
	}

#if LL_WINDOWS
	if (mVRAM < 256)
	{
		// Something likely went wrong using the above extensions...
		// Try WMI first and fall back to old method (from dxdiag) if all else
		// fails. Function will check all GPUs WMI knows of and will pick up
		// the one with most memory. We need to check all GPUs because system
		// can switch active GPU to weaker one, to preserve power when not
		// under load.
		S32 mem = LLDXHardware::getMBVideoMemoryViaWMI();
		if (mem > 0)
		{
			mVRAM = mem;
			llinfos << "Detected VRAM via WMI: " << mVRAM << llendl;
		}
	}
#endif

	if (mVRAM < 256 && old_vram > 0)
	{
		// Fall back to old method
		mVRAM = old_vram;
	}

	stop_glerror();

	GLint num_tex_image_units;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &num_tex_image_units);
	mNumTextureImageUnits = llmin(num_tex_image_units, 32);

	if (LLRender::sGLCoreProfile)
	{
		mNumTextureUnits = llmin(mNumTextureImageUnits, MAX_GL_TEXTURE_UNITS);
	}
	else if (mHasMultitexture)
	{
		GLint num_tex_units;
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &num_tex_units);
		mNumTextureUnits = llmin(num_tex_units, (GLint)MAX_GL_TEXTURE_UNITS);
		if (mIsIntel)
		{
			mNumTextureUnits = llmin(mNumTextureUnits, 2);
		}
	}
	else
	{
		mHasRequirements = false;

		// We do not support cards that do not support the GL_ARB_multitexture
		// extension
		llwarns << "GL Drivers do not support GL_ARB_multitexture" << llendl;
		return false;
	}

	stop_glerror();

	if (mHasTextureMultisample)
	{
		glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &mMaxColorTextureSamples);
		glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &mMaxDepthTextureSamples);
		glGetIntegerv(GL_MAX_INTEGER_SAMPLES, &mMaxIntegerSamples);
		glGetIntegerv(GL_MAX_SAMPLE_MASK_WORDS, &mMaxSampleMaskWords);
	}

	stop_glerror();

	// *HACK: always disable texture multisample, use FXAA instead
	mHasTextureMultisample = false;
#if LL_WINDOWS
	if (mIsIntel && mGLVersion <= 3.f)
	{
		// Never try to use framebuffer objects on older intel drivers (crashy)
		mHasFramebufferObject = false;
	}
#endif

	if (mHasFramebufferObject)
	{
		glGetIntegerv(GL_MAX_SAMPLES, &mMaxSamples);
	}

	stop_glerror();

	// "MOBILE INTEL(R) 965 EXPRESS CHIP",
	if (mGLRenderer.find("INTEL") != std::string::npos &&
		mGLRenderer.find("965") != std::string::npos)
	{
		mDebugGPU = true;
	}

	initGLStates();
	stop_glerror();

	return true;
}

void LLGLManager::initGLStates()
{
	LLGLState::initClass();
}

void LLGLManager::getGLInfo(LLSD& info)
{
	info["GLInfo"]["GLVendor"] =
		ll_safe_string((const char*)glGetString(GL_VENDOR));
	info["GLInfo"]["GLRenderer"] =
		ll_safe_string((const char*)glGetString(GL_RENDERER));
	info["GLInfo"]["GLVersion"] =
		ll_safe_string((const char*)glGetString(GL_VERSION));

	std::string all_exts = ll_safe_string((const char*)gGLHExts.mSysExts);
	boost::char_separator<char> sep(" ");
	boost::tokenizer<boost::char_separator<char> > tok(all_exts, sep);
	for (boost::tokenizer<boost::char_separator<char> >::iterator
			i = tok.begin(); i != tok.end(); ++i)
	{
		info["GLInfo"]["GLExtensions"].append(*i);
	}
}

void LLGLManager::printGLInfoString()
{
	llinfos << "GL_VENDOR  : "
			<< ll_safe_string((const char*)glGetString(GL_VENDOR))
			<< llendl;
	llinfos << "GL_RENDERER: "
			<< ll_safe_string((const char*)glGetString(GL_RENDERER))
			<< llendl;
	llinfos << "GL_VERSION : "
			<< ll_safe_string((const char*)glGetString(GL_VERSION))
			<< llendl;

	std::string all_exts= ll_safe_string(((const char*)gGLHExts.mSysExts));
	LLStringUtil::replaceChar(all_exts, ' ', '\n');
	LL_DEBUGS("RenderInit") << "GL_EXTENSIONS:\n" << all_exts << LL_ENDL;
}

std::string LLGLManager::getRawGLString()
{
	return ll_safe_string((char*)glGetString(GL_VENDOR)) + " " +
		   ll_safe_string((char*)glGetString(GL_RENDERER));
}

void LLGLManager::asLLSD(LLSD& info)
{
	// Currently these are duplicates of fields in LLViewerStats "system" info
	info["gpu_vendor"] = mGLVendorShort;
	info["gpu_version"] = mDriverVersionVendorString;
	info["opengl_version"] = mGLVersionString;

	info["vram"] = mVRAM;

	// Extensions used by everyone
	info["has_multitexture"] = mHasMultitexture;
	info["num_texture_units"] = mNumTextureUnits;
	info["has_mip_map_generation"] = mHasMipMapGeneration;
	info["has_compressed_textures"] = mHasCompressedTextures;
	info["has_framebuffer_object"] = mHasFramebufferObject;
	info["max_samples"] = mMaxSamples;
	info["has_blend_func_separate"] = mHasBlendFuncSeparate;

	// ARB Extensions
	info["has_vertex_buffer_object"] = mHasVertexBufferObject;
	info["has_vertex_array_object"] = mHasVertexArrayObject;
	info["has_sync"] = mHasSync;
	info["has_map_buffer_range"] = mHasMapBufferRange;
	info["has_flush_buffer_range"] = mHasFlushBufferRange;
	info["has_pbuffer"] = mHasPBuffer;
	info["has_shader_objects"] = std::string("Assumed TRUE");
	info["has_vertex_shader"] = std::string("Assumed TRUE");
	info["has_fragment_shader"] = std::string("Assumed TRUE");
	info["num_texture_image_units"] =  mNumTextureImageUnits;
	info["has_occlusion_query"] = mHasOcclusionQuery;
	info["has_timer_query"] = mHasTimerQuery;
	info["has_occlusion_query2"] = mHasOcclusionQuery2;
	info["has_point_parameters"] = mHasPointParameters;
	info["has_draw_buffers"] = mHasDrawBuffers;
	info["has_depth_clamp"] = mHasDepthClamp;
	info["has_texture_rectangle"] = mHasTextureRectangle;
	info["has_texture_multisample"] = mHasTextureMultisample;
	info["has_transform_feedback"] = false;	// was never used and got removed
	info["max_sample_mask_words"] = mMaxSampleMaskWords;
	info["max_color_texture_samples"] = mMaxColorTextureSamples;
	info["max_depth_texture_samples"] = mMaxDepthTextureSamples;
	info["max_integer_samples"] = mMaxIntegerSamples;

	// Other extensions.
	info["has_anisotropic"] = mHasAnisotropic;
	info["has_arb_env_combine"] = mHasARBEnvCombine;
	info["has_cube_map"] = mHasCubeMap;
	info["has_debug_output"] = mHasDebugOutput;
	info["has_srgb_texture"] = mHassRGBTexture;
	info["has_srgb_framebuffer"] = mHassRGBFramebuffer;
	info["has_texture_srgb_decode"] = mHasTexturesRGBDecode;

	// Vendor-specific extensions
	info["is_intel"] = mIsIntel;
	info["is_nvidia"] = mIsNVIDIA;
	info["has_nvx_mem_info"] = mHasNVXMemInfo;
	info["is_gf2or4mx"] = mIsGF2or4MX;
	info["is_ati"] = mIsAMD;
	info["has_ati_mem_info"] = mHasATIMemInfo;
	info["ati_offset_vertical_lines"] = mATIOffsetVerticalLines;
	info["ati_old_driver"] = mATIOldDriver;

	// Other fields
	info["has_requirements"] = mHasRequirements;
	info["has_separate_specular_color"] = mHasSeparateSpecularColor;
	info["debug_gpu"] = mDebugGPU;
	info["max_vertex_range"] = mGLMaxVertexRange;
	info["max_index_range"] = mGLMaxIndexRange;
	info["max_texture_size"] = mGLMaxTextureSize;
	info["gl_renderer"] = mGLRenderer;
}

void LLGLManager::shutdownGL()
{
	if (mInited)
	{
		glFinish();
		stop_glerror();
		mInited = false;
	}
}

// These are used to turn software blending on. They appear in the Debug/Avatar
// menu presence of vertex skinning/blending or vertex programs will set these
// to false by default.

void LLGLManager::initExtensions()
{
	// Important: gGLHExts.mSysExts is uninitialized until after
	// glh_init_extensions() is called.
	mHasMultitexture = glh_init_extensions("GL_ARB_multitexture");
	// Basic AMD method, also see mHasAMDAssociations:
	mHasATIMemInfo = ExtensionExists("GL_ATI_meminfo", gGLHExts.mSysExts);
	mHasNVXMemInfo = ExtensionExists("GL_NVX_gpu_memory_info", gGLHExts.mSysExts);
	mHasSeparateSpecularColor = glh_init_extensions("GL_EXT_separate_specular_color");
	mHasAnisotropic = glh_init_extensions("GL_EXT_texture_filter_anisotropic");
	glh_init_extensions("GL_ARB_texture_cube_map");
	mHasCubeMap = ExtensionExists("GL_ARB_texture_cube_map", gGLHExts.mSysExts);
	mHasARBEnvCombine = ExtensionExists("GL_ARB_texture_env_combine", gGLHExts.mSysExts);
	mHasCompressedTextures = glh_init_extensions("GL_ARB_texture_compression");
	mHasOcclusionQuery = ExtensionExists("GL_ARB_occlusion_query", gGLHExts.mSysExts);
	mHasOcclusionQuery2 = ExtensionExists("GL_ARB_occlusion_query2", gGLHExts.mSysExts);
	mHasTimerQuery = ExtensionExists("GL_ARB_timer_query", gGLHExts.mSysExts);
	mHasVertexBufferObject = ExtensionExists("GL_ARB_vertex_buffer_object", gGLHExts.mSysExts);
	mHasVertexArrayObject = ExtensionExists("GL_ARB_vertex_array_object", gGLHExts.mSysExts);
	mHasSync = ExtensionExists("GL_ARB_sync", gGLHExts.mSysExts);
	mHasMapBufferRange = ExtensionExists("GL_ARB_map_buffer_range", gGLHExts.mSysExts);
	mHasFlushBufferRange = ExtensionExists("GL_APPLE_flush_buffer_range", gGLHExts.mSysExts);
	mHasDepthClamp = ExtensionExists("GL_ARB_depth_clamp", gGLHExts.mSysExts) ||
					 ExtensionExists("GL_NV_depth_clamp", gGLHExts.mSysExts);
	if (!mHasDepthClamp)
	{
		mUseDepthClamp = false;
	}
	// Mask out FBO support when packed_depth_stencil is not there because we
	// need it for LLRenderTarget. Brad
#ifdef GL_ARB_framebuffer_object
	mHasFramebufferObject = ExtensionExists("GL_ARB_framebuffer_object", gGLHExts.mSysExts);
#else
	mHasFramebufferObject = ExtensionExists("GL_EXT_framebuffer_object", gGLHExts.mSysExts) &&
							ExtensionExists("GL_EXT_framebuffer_blit", gGLHExts.mSysExts) &&
							ExtensionExists("GL_EXT_framebuffer_multisample", gGLHExts.mSysExts) &&
							ExtensionExists("GL_EXT_packed_depth_stencil", gGLHExts.mSysExts);
#endif

#ifdef GL_EXT_texture_sRGB
	mHassRGBTexture = ExtensionExists("GL_EXT_texture_sRGB", gGLHExts.mSysExts);
#endif

#ifdef GL_ARB_framebuffer_sRGB
	mHassRGBFramebuffer = ExtensionExists("GL_ARB_framebuffer_sRGB", gGLHExts.mSysExts);
#else
	mHassRGBFramebuffer = ExtensionExists("GL_EXT_framebuffer_sRGB", gGLHExts.mSysExts);
#endif

#ifdef GL_EXT_texture_sRGB_decode
	mHasTexturesRGBDecode = ExtensionExists("GL_EXT_texture_sRGB_decode",
											gGLHExts.mSysExts);
#else
	mHasTexturesRGBDecode = ExtensionExists("GL_ARB_texture_sRGB_decode",
											gGLHExts.mSysExts);
#endif

	mHasMipMapGeneration = mHasFramebufferObject || mGLVersion >= 1.4f;

	mHasDrawBuffers = ExtensionExists("GL_ARB_draw_buffers", gGLHExts.mSysExts);
	mHasBlendFuncSeparate = ExtensionExists("GL_EXT_blend_func_separate", gGLHExts.mSysExts);
	mHasTextureRectangle = ExtensionExists("GL_ARB_texture_rectangle", gGLHExts.mSysExts);
	mHasTextureMultisample = ExtensionExists("GL_ARB_texture_multisample", gGLHExts.mSysExts);
	mHasDebugOutput = ExtensionExists("GL_ARB_debug_output", gGLHExts.mSysExts);
#if !LL_DARWIN
	mHasPointParameters = ExtensionExists("GL_ARB_point_parameters", gGLHExts.mSysExts);
	mHasVertexAttribIPointer = mGLSLVersionMajor > 1 ||
							   mGLSLVersionMinor >= 30;
#endif

#ifdef GL_ARB_gpu_shader5
	mHasGpuShader5 = ExtensionExists("GL_ARB_gpu_shader5", gGLHExts.mSysExts);
#endif

#ifdef GL_ARB_texture_swizzle
	mHasTextureSwizzle = ExtensionExists("GL_ARB_texture_swizzle",
										 gGLHExts.mSysExts);
#endif

	if (!mHasMultitexture)
	{
		llinfos << "Could not initialize multitexturing" << llendl;
	}
	if (!mHasMipMapGeneration)
	{
		llinfos << "Could not initialize mipmap generation" << llendl;
	}
	if (!mHasARBEnvCombine)
	{
		llinfos << "Could not initialize GL_ARB_texture_env_combine" << llendl;
	}
	if (!mHasSeparateSpecularColor)
	{
		llinfos << "Could not initialize separate specular color" << llendl;
	}
	if (!mHasAnisotropic)
	{
		llinfos << "Could not initialize anisotropic filtering" << llendl;
	}
	if (!mHasCompressedTextures)
	{
		llinfos << "Could not initialize GL_ARB_texture_compression" << llendl;
	}
	if (!mHasOcclusionQuery)
	{
		llinfos << "Could not initialize GL_ARB_occlusion_query" << llendl;
	}
	if (!mHasOcclusionQuery2)
	{
		llinfos << "Could not initialize GL_ARB_occlusion_query2" << llendl;
	}
	if (!mHasPointParameters)
	{
		llinfos << "Could not initialize GL_ARB_point_parameters" << llendl;
	}
	if (!mHasBlendFuncSeparate)
	{
		llinfos << "Could not initialize GL_EXT_blend_func_separate" << llendl;
	}
	if (!mHasDrawBuffers)
	{
		llinfos << "Could not initialize GL_ARB_draw_buffers" << llendl;
	}

	// Misc
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, (GLint*)&mGLMaxVertexRange);
	glGetIntegerv(GL_MAX_ELEMENTS_INDICES, (GLint*)&mGLMaxIndexRange);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint*)&mGLMaxTextureSize);

#if LL_LINUX || LL_WINDOWS
	LL_DEBUGS("RenderInit") << "GL Probe: Getting symbols" << LL_ENDL;
	if (mHasVertexBufferObject)
	{
		glBindBufferARB = (PFNGLBINDBUFFERARBPROC)GLH_EXT_GET_PROC_ADDRESS("glBindBufferARB");
		if (glBindBufferARB)
		{
			glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)GLH_EXT_GET_PROC_ADDRESS("glDeleteBuffersARB");
			glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGenBuffersARB");
			glIsBufferARB = (PFNGLISBUFFERARBPROC)GLH_EXT_GET_PROC_ADDRESS("glIsBufferARB");
			glBufferDataARB = (PFNGLBUFFERDATAARBPROC)GLH_EXT_GET_PROC_ADDRESS("glBufferDataARB");
			glBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC)GLH_EXT_GET_PROC_ADDRESS("glBufferSubDataARB");
			glGetBufferSubDataARB = (PFNGLGETBUFFERSUBDATAARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGetBufferSubDataARB");
			glMapBufferARB = (PFNGLMAPBUFFERARBPROC)GLH_EXT_GET_PROC_ADDRESS("glMapBufferARB");
			glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)GLH_EXT_GET_PROC_ADDRESS("glUnmapBufferARB");
			glGetBufferParameterivARB = (PFNGLGETBUFFERPARAMETERIVARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGetBufferParameterivARB");
			glGetBufferPointervARB = (PFNGLGETBUFFERPOINTERVARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGetBufferPointervARB");
		}
		else
		{
			mHasVertexBufferObject = false;
		}
	}
	if (mHasVertexArrayObject)
	{
		glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) GLH_EXT_GET_PROC_ADDRESS("glBindVertexArray");
		glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteVertexArrays");
		glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) GLH_EXT_GET_PROC_ADDRESS("glGenVertexArrays");
		glIsVertexArray = (PFNGLISVERTEXARRAYPROC) GLH_EXT_GET_PROC_ADDRESS("glIsVertexArray");
	}
	if (mHasSync)
	{
		glFenceSync = (PFNGLFENCESYNCPROC) GLH_EXT_GET_PROC_ADDRESS("glFenceSync");
		glIsSync = (PFNGLISSYNCPROC) GLH_EXT_GET_PROC_ADDRESS("glIsSync");
		glDeleteSync = (PFNGLDELETESYNCPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteSync");
		glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC) GLH_EXT_GET_PROC_ADDRESS("glClientWaitSync");
		glWaitSync = (PFNGLWAITSYNCPROC) GLH_EXT_GET_PROC_ADDRESS("glWaitSync");
		glGetInteger64v = (PFNGLGETINTEGER64VPROC) GLH_EXT_GET_PROC_ADDRESS("glGetInteger64v");
		glGetSynciv = (PFNGLGETSYNCIVPROC) GLH_EXT_GET_PROC_ADDRESS("glGetSynciv");
	}
	if (mHasMapBufferRange)
	{
		glMapBufferRange = (PFNGLMAPBUFFERRANGEPROC) GLH_EXT_GET_PROC_ADDRESS("glMapBufferRange");
		glFlushMappedBufferRange = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC) GLH_EXT_GET_PROC_ADDRESS("glFlushMappedBufferRange");
	}
	if (mHasFramebufferObject)
	{
		llinfos << "FramebufferObject-related procs..." << llendl;
		glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC) GLH_EXT_GET_PROC_ADDRESS("glIsRenderbuffer");
		glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) GLH_EXT_GET_PROC_ADDRESS("glBindRenderbuffer");
		glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteRenderbuffers");
		glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) GLH_EXT_GET_PROC_ADDRESS("glGenRenderbuffers");
		glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) GLH_EXT_GET_PROC_ADDRESS("glRenderbufferStorage");
		glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC) GLH_EXT_GET_PROC_ADDRESS("glGetRenderbufferParameteriv");
		glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC) GLH_EXT_GET_PROC_ADDRESS("glIsFramebuffer");
		glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) GLH_EXT_GET_PROC_ADDRESS("glBindFramebuffer");
		glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteFramebuffers");
		glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) GLH_EXT_GET_PROC_ADDRESS("glGenFramebuffers");
		glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) GLH_EXT_GET_PROC_ADDRESS("glCheckFramebufferStatus");
		glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferTexture1D");
		glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferTexture2D");
		glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferTexture3D");
		glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferRenderbuffer");
		glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) GLH_EXT_GET_PROC_ADDRESS("glGetFramebufferAttachmentParameteriv");
		glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC) GLH_EXT_GET_PROC_ADDRESS("glGenerateMipmap");
		glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC) GLH_EXT_GET_PROC_ADDRESS("glBlitFramebuffer");
		glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) GLH_EXT_GET_PROC_ADDRESS("glRenderbufferStorageMultisample");
		glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferTextureLayer");
	}
	if (mHasDrawBuffers)
	{
		glDrawBuffersARB = (PFNGLDRAWBUFFERSARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDrawBuffersARB");
	}
	if (mHasBlendFuncSeparate)
	{
		glBlendFuncSeparateEXT = (PFNGLBLENDFUNCSEPARATEEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glBlendFuncSeparateEXT");
	}
	if (mHasTextureMultisample)
	{
		glTexImage2DMultisample = (PFNGLTEXIMAGE2DMULTISAMPLEPROC) GLH_EXT_GET_PROC_ADDRESS("glTexImage2DMultisample");
		glTexImage3DMultisample = (PFNGLTEXIMAGE3DMULTISAMPLEPROC) GLH_EXT_GET_PROC_ADDRESS("glTexImage3DMultisample");
		glGetMultisamplefv = (PFNGLGETMULTISAMPLEFVPROC) GLH_EXT_GET_PROC_ADDRESS("glGetMultisamplefv");
		glSampleMaski = (PFNGLSAMPLEMASKIPROC) GLH_EXT_GET_PROC_ADDRESS("glSampleMaski");
	}
	if (mHasDebugOutput)
	{
		glDebugMessageControlARB = (PFNGLDEBUGMESSAGECONTROLARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDebugMessageControlARB");
		glDebugMessageInsertARB = (PFNGLDEBUGMESSAGEINSERTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDebugMessageInsertARB");
		glDebugMessageCallbackARB = (PFNGLDEBUGMESSAGECALLBACKARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDebugMessageCallbackARB");
		glGetDebugMessageLogARB = (PFNGLGETDEBUGMESSAGELOGARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetDebugMessageLogARB");
	}
# if !LL_LINUX || LL_LINUX_NV_GL_HEADERS
	// This is expected to be a static symbol on Linux GL implementations,
	// except if we use the nvidia headers - bah
	glDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC)GLH_EXT_GET_PROC_ADDRESS("glDrawRangeElements");
	if (!glDrawRangeElements)
	{
		mGLMaxVertexRange = 0;
		mGLMaxIndexRange = 0;
	}
# endif // !LL_LINUX || LL_LINUX_NV_GL_HEADERS
# if LL_LINUX_NV_GL_HEADERS
	// nvidia headers are critically different from mesa-esque
 	glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)GLH_EXT_GET_PROC_ADDRESS("glActiveTextureARB");
 	glClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC)GLH_EXT_GET_PROC_ADDRESS("glClientActiveTextureARB");
# endif // LL_LINUX_NV_GL_HEADERS

	if (mHasOcclusionQuery)
	{
		llinfos << "OcclusionQuery-related procs..." << llendl;
		glGenQueriesARB = (PFNGLGENQUERIESARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGenQueriesARB");
		glDeleteQueriesARB = (PFNGLDELETEQUERIESARBPROC)GLH_EXT_GET_PROC_ADDRESS("glDeleteQueriesARB");
		glIsQueryARB = (PFNGLISQUERYARBPROC)GLH_EXT_GET_PROC_ADDRESS("glIsQueryARB");
		glBeginQueryARB = (PFNGLBEGINQUERYARBPROC)GLH_EXT_GET_PROC_ADDRESS("glBeginQueryARB");
		glEndQueryARB = (PFNGLENDQUERYARBPROC)GLH_EXT_GET_PROC_ADDRESS("glEndQueryARB");
		glGetQueryivARB = (PFNGLGETQUERYIVARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGetQueryivARB");
		glGetQueryObjectivARB = (PFNGLGETQUERYOBJECTIVARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGetQueryObjectivARB");
		glGetQueryObjectuivARB = (PFNGLGETQUERYOBJECTUIVARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGetQueryObjectuivARB");
	}
	if (mHasTimerQuery)
	{
		llinfos << "TimerQuery-related procs..." << llendl;
		glQueryCounter = (PFNGLQUERYCOUNTERPROC) GLH_EXT_GET_PROC_ADDRESS("glQueryCounter");
		glGetQueryObjecti64v = (PFNGLGETQUERYOBJECTI64VPROC) GLH_EXT_GET_PROC_ADDRESS("glGetQueryObjecti64v");
		glGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VPROC) GLH_EXT_GET_PROC_ADDRESS("glGetQueryObjectui64v");
	}
	if (mHasPointParameters)
	{
		llinfos << "PointParameters-related procs..." << llendl;
		glPointParameterfARB = (PFNGLPOINTPARAMETERFARBPROC)GLH_EXT_GET_PROC_ADDRESS("glPointParameterfARB");
		glPointParameterfvARB = (PFNGLPOINTPARAMETERFVARBPROC)GLH_EXT_GET_PROC_ADDRESS("glPointParameterfvARB");
	}

	// Assume shader capabilities
	glDeleteObjectARB = (PFNGLDELETEOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteObjectARB");
	glGetHandleARB = (PFNGLGETHANDLEARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetHandleARB");
	glDetachObjectARB = (PFNGLDETACHOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDetachObjectARB");
	glCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glCreateShaderObjectARB");
	glShaderSourceARB = (PFNGLSHADERSOURCEARBPROC) GLH_EXT_GET_PROC_ADDRESS("glShaderSourceARB");
	glCompileShaderARB = (PFNGLCOMPILESHADERARBPROC) GLH_EXT_GET_PROC_ADDRESS("glCompileShaderARB");
	glCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glCreateProgramObjectARB");
	glAttachObjectARB = (PFNGLATTACHOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glAttachObjectARB");
	glLinkProgramARB = (PFNGLLINKPROGRAMARBPROC) GLH_EXT_GET_PROC_ADDRESS("glLinkProgramARB");
	glUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUseProgramObjectARB");
	glValidateProgramARB = (PFNGLVALIDATEPROGRAMARBPROC) GLH_EXT_GET_PROC_ADDRESS("glValidateProgramARB");
	glUniform1fARB = (PFNGLUNIFORM1FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform1fARB");
	glUniform2fARB = (PFNGLUNIFORM2FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform2fARB");
	glUniform3fARB = (PFNGLUNIFORM3FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform3fARB");
	glUniform4fARB = (PFNGLUNIFORM4FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform4fARB");
	glUniform1iARB = (PFNGLUNIFORM1IARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform1iARB");
	glUniform2iARB = (PFNGLUNIFORM2IARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform2iARB");
	glUniform3iARB = (PFNGLUNIFORM3IARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform3iARB");
	glUniform4iARB = (PFNGLUNIFORM4IARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform4iARB");
	glUniform1fvARB = (PFNGLUNIFORM1FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform1fvARB");
	glUniform2fvARB = (PFNGLUNIFORM2FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform2fvARB");
	glUniform3fvARB = (PFNGLUNIFORM3FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform3fvARB");
	glUniform4fvARB = (PFNGLUNIFORM4FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform4fvARB");
	glUniform1ivARB = (PFNGLUNIFORM1IVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform1ivARB");
	glUniform2ivARB = (PFNGLUNIFORM2IVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform2ivARB");
	glUniform3ivARB = (PFNGLUNIFORM3IVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform3ivARB");
	glUniform4ivARB = (PFNGLUNIFORM4IVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform4ivARB");
	glUniformMatrix2fvARB = (PFNGLUNIFORMMATRIX2FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniformMatrix2fvARB");
	glUniformMatrix3fvARB = (PFNGLUNIFORMMATRIX3FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniformMatrix3fvARB");
	glUniformMatrix3x4fv = (PFNGLUNIFORMMATRIX3X4FVPROC) GLH_EXT_GET_PROC_ADDRESS("glUniformMatrix3x4fv");
	glUniformMatrix4fvARB = (PFNGLUNIFORMMATRIX4FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniformMatrix4fvARB");
	glGetObjectParameterfvARB = (PFNGLGETOBJECTPARAMETERFVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetObjectParameterfvARB");
	glGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetObjectParameterivARB");
	glGetInfoLogARB = (PFNGLGETINFOLOGARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetInfoLogARB");
	glGetAttachedObjectsARB = (PFNGLGETATTACHEDOBJECTSARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetAttachedObjectsARB");
	glGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetUniformLocationARB");
	glGetActiveUniformARB = (PFNGLGETACTIVEUNIFORMARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetActiveUniformARB");
	glGetUniformfvARB = (PFNGLGETUNIFORMFVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetUniformfvARB");
	glGetUniformivARB = (PFNGLGETUNIFORMIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetUniformivARB");
	glGetShaderSourceARB = (PFNGLGETSHADERSOURCEARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetShaderSourceARB");

	llinfos << "VertexShader-related procs..." << llendl;
	glGetAttribLocationARB = (PFNGLGETATTRIBLOCATIONARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetAttribLocationARB");
	glBindAttribLocationARB = (PFNGLBINDATTRIBLOCATIONARBPROC) GLH_EXT_GET_PROC_ADDRESS("glBindAttribLocationARB");
	glGetActiveAttribARB = (PFNGLGETACTIVEATTRIBARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetActiveAttribARB");
	glVertexAttrib1dARB = (PFNGLVERTEXATTRIB1DARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib1dARB");
	glVertexAttrib1dvARB = (PFNGLVERTEXATTRIB1DVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib1dvARB");
	glVertexAttrib1fARB = (PFNGLVERTEXATTRIB1FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib1fARB");
	glVertexAttrib1fvARB = (PFNGLVERTEXATTRIB1FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib1fvARB");
	glVertexAttrib1sARB = (PFNGLVERTEXATTRIB1SARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib1sARB");
	glVertexAttrib1svARB = (PFNGLVERTEXATTRIB1SVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib1svARB");
	glVertexAttrib2dARB = (PFNGLVERTEXATTRIB2DARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib2dARB");
	glVertexAttrib2dvARB = (PFNGLVERTEXATTRIB2DVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib2dvARB");
	glVertexAttrib2fARB = (PFNGLVERTEXATTRIB2FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib2fARB");
	glVertexAttrib2fvARB = (PFNGLVERTEXATTRIB2FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib2fvARB");
	glVertexAttrib2sARB = (PFNGLVERTEXATTRIB2SARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib2sARB");
	glVertexAttrib2svARB = (PFNGLVERTEXATTRIB2SVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib2svARB");
	glVertexAttrib3dARB = (PFNGLVERTEXATTRIB3DARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib3dARB");
	glVertexAttrib3dvARB = (PFNGLVERTEXATTRIB3DVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib3dvARB");
	glVertexAttrib3fARB = (PFNGLVERTEXATTRIB3FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib3fARB");
	glVertexAttrib3fvARB = (PFNGLVERTEXATTRIB3FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib3fvARB");
	glVertexAttrib3sARB = (PFNGLVERTEXATTRIB3SARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib3sARB");
	glVertexAttrib3svARB = (PFNGLVERTEXATTRIB3SVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib3svARB");
	glVertexAttrib4nbvARB = (PFNGLVERTEXATTRIB4NBVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nbvARB");
	glVertexAttrib4nivARB = (PFNGLVERTEXATTRIB4NIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nivARB");
	glVertexAttrib4nsvARB = (PFNGLVERTEXATTRIB4NSVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nsvARB");
	glVertexAttrib4nubARB = (PFNGLVERTEXATTRIB4NUBARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nubARB");
	glVertexAttrib4nubvARB = (PFNGLVERTEXATTRIB4NUBVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nubvARB");
	glVertexAttrib4nuivARB = (PFNGLVERTEXATTRIB4NUIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nuivARB");
	glVertexAttrib4nusvARB = (PFNGLVERTEXATTRIB4NUSVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nusvARB");
	glVertexAttrib4bvARB = (PFNGLVERTEXATTRIB4BVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4bvARB");
	glVertexAttrib4dARB = (PFNGLVERTEXATTRIB4DARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4dARB");
	glVertexAttrib4dvARB = (PFNGLVERTEXATTRIB4DVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4dvARB");
	glVertexAttrib4fARB = (PFNGLVERTEXATTRIB4FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4fARB");
	glVertexAttrib4fvARB = (PFNGLVERTEXATTRIB4FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4fvARB");
	glVertexAttrib4ivARB = (PFNGLVERTEXATTRIB4IVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4ivARB");
	glVertexAttrib4sARB = (PFNGLVERTEXATTRIB4SARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4sARB");
	glVertexAttrib4svARB = (PFNGLVERTEXATTRIB4SVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4svARB");
	glVertexAttrib4ubvARB = (PFNGLVERTEXATTRIB4UBVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4ubvARB");
	glVertexAttrib4uivARB = (PFNGLVERTEXATTRIB4UIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4uivARB");
	glVertexAttrib4usvARB = (PFNGLVERTEXATTRIB4USVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4usvARB");
	glVertexAttribPointerARB = (PFNGLVERTEXATTRIBPOINTERARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttribPointerARB");
	glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttribIPointer");
	glEnableVertexAttribArrayARB = (PFNGLENABLEVERTEXATTRIBARRAYARBPROC) GLH_EXT_GET_PROC_ADDRESS("glEnableVertexAttribArrayARB");
	glDisableVertexAttribArrayARB = (PFNGLDISABLEVERTEXATTRIBARRAYARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDisableVertexAttribArrayARB");
	glProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramStringARB");
	glBindProgramARB = (PFNGLBINDPROGRAMARBPROC) GLH_EXT_GET_PROC_ADDRESS("glBindProgramARB");
	glDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteProgramsARB");
	glGenProgramsARB = (PFNGLGENPROGRAMSARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGenProgramsARB");
	glProgramEnvParameter4dARB = (PFNGLPROGRAMENVPARAMETER4DARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramEnvParameter4dARB");
	glProgramEnvParameter4dvARB = (PFNGLPROGRAMENVPARAMETER4DVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramEnvParameter4dvARB");
	glProgramEnvParameter4fARB = (PFNGLPROGRAMENVPARAMETER4FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramEnvParameter4fARB");
	glProgramEnvParameter4fvARB = (PFNGLPROGRAMENVPARAMETER4FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramEnvParameter4fvARB");
	glProgramLocalParameter4dARB = (PFNGLPROGRAMLOCALPARAMETER4DARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramLocalParameter4dARB");
	glProgramLocalParameter4dvARB = (PFNGLPROGRAMLOCALPARAMETER4DVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramLocalParameter4dvARB");
	glProgramLocalParameter4fARB = (PFNGLPROGRAMLOCALPARAMETER4FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramLocalParameter4fARB");
	glProgramLocalParameter4fvARB = (PFNGLPROGRAMLOCALPARAMETER4FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramLocalParameter4fvARB");
	glGetProgramEnvParameterdvARB = (PFNGLGETPROGRAMENVPARAMETERDVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetProgramEnvParameterdvARB");
	glGetProgramEnvParameterfvARB = (PFNGLGETPROGRAMENVPARAMETERFVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetProgramEnvParameterfvARB");
	glGetProgramLocalParameterdvARB = (PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetProgramLocalParameterdvARB");
	glGetProgramLocalParameterfvARB = (PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetProgramLocalParameterfvARB");
	glGetProgramivARB = (PFNGLGETPROGRAMIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetProgramivARB");
	glGetProgramStringARB = (PFNGLGETPROGRAMSTRINGARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetProgramStringARB");
	glGetVertexAttribdvARB = (PFNGLGETVERTEXATTRIBDVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetVertexAttribdvARB");
	glGetVertexAttribfvARB = (PFNGLGETVERTEXATTRIBFVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetVertexAttribfvARB");
	glGetVertexAttribivARB = (PFNGLGETVERTEXATTRIBIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetVertexAttribivARB");
	glGetVertexAttribPointervARB = (PFNGLGETVERTEXATTRIBPOINTERVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glgetVertexAttribPointervARB");
	glIsProgramARB = (PFNGLISPROGRAMARBPROC) GLH_EXT_GET_PROC_ADDRESS("glIsProgramARB");
	LL_DEBUGS("RenderInit") << "GL Probe: got symbols" << LL_ENDL;
#endif	// LL_LINUX || LL_WINDOWS

	mInited = true;
}

///////////////////////////////////////////////////////////////////////////////
// LLGLState class
///////////////////////////////////////////////////////////////////////////////

// Static members
LLGLState::state_map_t LLGLState::sStateMap;

GLboolean LLGLDepthTest::sDepthEnabled = GL_FALSE; // OpenGL default
U32 LLGLDepthTest::sDepthFunc = GL_LESS; // OpenGL default
GLboolean LLGLDepthTest::sWriteEnabled = GL_TRUE; // OpenGL default

//static
void LLGLState::initClass()
{
	sStateMap[GL_DITHER] = GL_TRUE;
#if 0
	sStateMap[GL_TEXTURE_2D] = GL_TRUE;
#endif

	// Make sure multisample defaults to disabled
	sStateMap[GL_MULTISAMPLE_ARB] = GL_FALSE;
	glDisable(GL_MULTISAMPLE_ARB);
}

//static
void LLGLState::restoreGL()
{
	sStateMap.clear();
	initClass();
}

//static
// Really should not be needed, but seems we sometimes do.
void LLGLState::resetTextureStates()
{
	gGL.flush();

	GLint max_tex_units;
	glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &max_tex_units);

	for (S32 j = max_tex_units - 1; j >= 0; --j)
	{
		LLTexUnit* unit = gGL.getTexUnit(j);
		unit->activate();
		glClientActiveTextureARB(GL_TEXTURE0_ARB + j);
		if (j == 0)
		{
			unit->enable(LLTexUnit::TT_TEXTURE);
		}
		else
		{
			unit->disable();
		}
	}
}

void LLGLState::dumpStates()
{
	llinfos << "GL States:";
	for (state_map_t::iterator iter = sStateMap.begin(), end = sStateMap.end();
		 iter != end; ++iter)
	{
		llcont << llformat("\n   0x%04x : %s", (S32)iter->first,
						   iter->second ? "true" : "false");
	}
	llcont << llendl;
}

//static
void LLGLState::check(bool states, bool texture_channels,
					  const std::string& msg, U32 data_mask)
{
	if (!gDebugGL)
	{
		return;
	}

	stop_glerror();

	if (states)
	{
		checkStates(msg);
	}
#if 0
	if (texture_channels)
	{
		checkTextureChannels(msg);
	}
#endif
}

//static
void LLGLState::checkStates(const std::string& msg)
{
	GLint src;
	glGetIntegerv(GL_BLEND_SRC, &src);
	GLint dst;
	glGetIntegerv(GL_BLEND_DST, &dst);

	stop_glerror();

	if (src != GL_SRC_ALPHA || dst != GL_ONE_MINUS_SRC_ALPHA)
	{
		llwarns << "Blend function corrupted: " << std::hex << src
				<< " " << dst << "  " << msg << std::dec << llendl;
	}

	for (state_map_t::iterator iter = sStateMap.begin(), end = sStateMap.end();
		 iter != end; ++iter)
	{
		U32 state = iter->first;
		GLboolean cur_state = iter->second;
		stop_glerror();
		GLboolean gl_state = glIsEnabled(state);
		stop_glerror();
		if (cur_state != gl_state)
		{
			dumpStates();
			llwarns << llformat("LLGLState error. State: 0x%04x", state)
					<< llendl;
		}
	}

	stop_glerror();
}

//static
void LLGLState::checkTextureChannels(const std::string& msg)
{
#if 0
	GLint active_texture;
	glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &active_texture);
	stop_glerror();

	bool error = false;

	if (active_texture == GL_TEXTURE0_ARB)
	{
		GLint tex_env_mode = 0;

		glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &tex_env_mode);
		stop_glerror();

		if (tex_env_mode != GL_MODULATE)
		{
			error = true;
			llwarns << "GL_TEXTURE_ENV_MODE invalid: " << std::hex
					<< tex_env_mode << std::dec << llendl;
		}
	}

	static const char* label[] =
	{
		"GL_TEXTURE_2D",
		"GL_TEXTURE_COORD_ARRAY",
		"GL_TEXTURE_1D",
		"GL_TEXTURE_CUBE_MAP_ARB",
		"GL_TEXTURE_GEN_S",
		"GL_TEXTURE_GEN_T",
		"GL_TEXTURE_GEN_Q",
		"GL_TEXTURE_GEN_R",
		"GL_TEXTURE_RECTANGLE_ARB"
	};

	static GLint value[] =
	{
		GL_TEXTURE_2D,
		GL_TEXTURE_COORD_ARRAY,
		GL_TEXTURE_1D,
		GL_TEXTURE_CUBE_MAP_ARB,
		GL_TEXTURE_GEN_S,
		GL_TEXTURE_GEN_T,
		GL_TEXTURE_GEN_Q,
		GL_TEXTURE_GEN_R,
		GL_TEXTURE_RECTANGLE_ARB
	};

	GLint stack_depth = 0;

	LLMatrix4a mat;
	for (GLint i = 1; i < gGLManager.mNumTextureUnits; ++i)
	{
		gGL.getTexUnit(i)->activate();

		if (i < gGLManager.mNumTextureUnits)
		{
			glClientActiveTextureARB(GL_TEXTURE0_ARB + i);
			stop_glerror();
			glGetIntegerv(GL_TEXTURE_STACK_DEPTH, &stack_depth);
			stop_glerror();

			if (stack_depth != 1)
			{
				error = true;
				llwarns << "Texture matrix stack corrupted." << llendl;
			}

			glGetFloatv(GL_TEXTURE_MATRIX, (GLfloat*)mat.getF32ptr());
			stop_glerror();

			if (!mat.isIdentity())
			{
				error = true;
				llwarns << "Texture matrix in channel " << i << " corrupt."
						<< llendl;
			}

			for (S32 j = (i == 0 ? 1 : 0); j < 9; ++j)
			{
				if (j == 8 && !gGLManager.mHasTextureRectangle ||
					j == 9 && !gGLManager.mHasTextureMultisample)
				{
					continue;
				}

				if (glIsEnabled(value[j]))
				{
					error = true;
					llwarns << "Texture channel " << i << " still has "
							<< label[j] << " enabled." << llendl;
				}
				stop_glerror();
			}

			glGetFloatv(GL_TEXTURE_MATRIX, mat.m);
			stop_glerror();

			if (mat != identity)
			{
				error = true;
				llwarns << "Texture matrix " << i << " is not identity."
						<< llendl;
			}
		}

		{
			GLint tex = 0;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
			stop_glerror();

			if (tex != 0)
			{
				error = true;
				llwarns << "Texture channel " << i << " still has texture "
						<< tex << " bound." << llendl;
			}
		}
	}

	gGL.getTexUnit(0)->activate();
	glClientActiveTextureARB(GL_TEXTURE0_ARB);
	stop_glerror();

	if (error)
	{
		llwarns << "GL texture state corruption detected.  " << msg << llendl;
	}
#endif
}

LLGLState::LLGLState(U32 state, S32 enabled)
:	mState(state),
	mWasEnabled(GL_FALSE),
	mIsEnabled(GL_FALSE)
{
	// Always ignore any state deprecated post GL 3.0
	switch (state)
	{
		case GL_ALPHA_TEST:
		case GL_NORMALIZE:
		case GL_TEXTURE_GEN_R:
		case GL_TEXTURE_GEN_S:
		case GL_TEXTURE_GEN_T:
		case GL_TEXTURE_GEN_Q:
		case GL_LIGHTING:
		case GL_COLOR_MATERIAL:
		case GL_FOG:
		case GL_LINE_STIPPLE:
		case GL_POLYGON_STIPPLE:
			mState = 0;
	}

	if (mState)
	{
		mWasEnabled = sStateMap[state];
#if 0	// Cannot assert on this since queued changes to state are not
		// reflected by glIsEnabled
		llassert(mWasEnabled == glIsEnabled(state));
#endif
		setEnabled(enabled);
	}

	stop_glerror();
}

void LLGLState::setEnabled(S32 enabled)
{
	if (!mState)
	{
		return;
	}
	if (enabled == CURRENT_STATE)
	{
		enabled = sStateMap[mState] == GL_TRUE ? GL_TRUE : GL_FALSE;
	}
	else if (enabled == GL_TRUE && sStateMap[mState] != GL_TRUE)
	{
		gGL.flush();
		glEnable(mState);
		sStateMap[mState] = GL_TRUE;
	}
	else if (enabled == GL_FALSE && sStateMap[mState] != GL_FALSE)
	{
		gGL.flush();
		glDisable(mState);
		sStateMap[mState] = GL_FALSE;
	}
	mIsEnabled = enabled;
}

LLGLState::~LLGLState()
{
	if (mState)
	{
		if (gDebugGL)
		{
			GLboolean state = glIsEnabled(mState);
			if (sStateMap[mState] != state)
			{
				llwarns_once << "Mismatch for state: " << std::hex << mState
							 << std::dec << " - Actual status: " << state
							 << " (should be " << sStateMap[mState] << ")."
							 << llendl;
			}
		}

		if (mIsEnabled != mWasEnabled)
		{
			gGL.flush();
			if (mWasEnabled)
			{
				glEnable(mState);
				sStateMap[mState] = GL_TRUE;
			}
			else
			{
				glDisable(mState);
				sStateMap[mState] = GL_FALSE;
			}
		}
	}
	stop_glerror();
}

///////////////////////////////////////////////////////////////////////////////
// LLGLUserClipPlane class
///////////////////////////////////////////////////////////////////////////////

LLGLUserClipPlane::LLGLUserClipPlane(const LLPlane& p, const LLMatrix4a& mdlv,
									 const LLMatrix4a& proj, bool apply)
:	mApply(apply)
{
	if (apply)
	{
		mModelview = mdlv;
		mProjection = proj;
		setPlane(p[0], p[1], p[2], p[3]);
	}
}

LLGLUserClipPlane::~LLGLUserClipPlane()
{
	disable();
}

void LLGLUserClipPlane::disable()
{
	if (mApply)
	{
		mApply = false;
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
}

void LLGLUserClipPlane::setPlane(F32 a, F32 b, F32 c, F32 d)
{
	LLMatrix4a& p = mProjection;
	LLMatrix4a& m = mModelview;

	LLMatrix4a invtrans_mdlv;
	invtrans_mdlv.setMul(p, m);
	invtrans_mdlv.invert();
	invtrans_mdlv.transpose();

	LLVector4a oplane(a, b, c, d);
	LLVector4a cplane, cplane_splat, cplane_neg;

	invtrans_mdlv.rotate4(oplane, cplane);

	cplane_splat.splat<2>(cplane);
	cplane_splat.setAbs(cplane_splat);
	cplane.div(cplane_splat);
	cplane.sub(LLVector4a(0.f, 0.f, 0.f, 1.f));

	cplane_splat.splat<2>(cplane);
	cplane_neg = cplane;
	cplane_neg.negate();

	cplane.setSelectWithMask(cplane_splat.lessThan(_mm_setzero_ps()),
							 cplane_neg, cplane);

	LLMatrix4a suffix;
	suffix.setIdentity();
	suffix.setColumn<2>(cplane);
	LLMatrix4a new_proj;
	new_proj.setMul(suffix, p);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(new_proj);
	gGL.matrixMode(LLRender::MM_MODELVIEW);
}

///////////////////////////////////////////////////////////////////////////////
// LLGLDepthTest class
///////////////////////////////////////////////////////////////////////////////

LLGLDepthTest::LLGLDepthTest(GLboolean depth_enabled, GLboolean write_enabled,
							 U32 depth_func)
:	mPrevDepthEnabled(sDepthEnabled),
	mPrevDepthFunc(sDepthFunc),
	mPrevWriteEnabled(sWriteEnabled)
{
	stop_glerror();
	checkState();

	if (!depth_enabled)
	{
		// Always disable depth writes if depth testing is disabled. GL spec
		// defines this as a requirement, but some implementations allow depth
		// writes with testing disabled. The proper way to write to depth
		// buffer with testing disabled is to enable testing and use a
		// depth_func of GL_ALWAYS
		write_enabled = GL_FALSE;
	}

	if (depth_enabled != sDepthEnabled)
	{
		gGL.flush();
		if (depth_enabled)
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}
		sDepthEnabled = depth_enabled;
	}
	if (depth_func != sDepthFunc)
	{
		gGL.flush();
		glDepthFunc(depth_func);
		sDepthFunc = depth_func;
	}
	if (write_enabled != sWriteEnabled)
	{
		gGL.flush();
		glDepthMask(write_enabled);
		sWriteEnabled = write_enabled;
	}
	stop_glerror();
}

LLGLDepthTest::~LLGLDepthTest()
{
	checkState();
	if (sDepthEnabled != mPrevDepthEnabled)
	{
		gGL.flush();
		if (mPrevDepthEnabled) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
		sDepthEnabled = mPrevDepthEnabled;
	}
	if (sDepthFunc != mPrevDepthFunc)
	{
		gGL.flush();
		glDepthFunc(mPrevDepthFunc);
		sDepthFunc = mPrevDepthFunc;
	}
	if (sWriteEnabled != mPrevWriteEnabled)
	{
		gGL.flush();
		glDepthMask(mPrevWriteEnabled);
		sWriteEnabled = mPrevWriteEnabled;
	}
	stop_glerror();
}

void LLGLDepthTest::checkState()
{
	if (gDebugGL)
	{
		GLint func = 0;
		GLboolean mask = GL_FALSE;

		glGetIntegerv(GL_DEPTH_FUNC, &func);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);

		if (glIsEnabled(GL_DEPTH_TEST) != sDepthEnabled ||
			sWriteEnabled != mask ||
			(GLint)sDepthFunc != func)
		{
			llwarns << "Unexpected depth testing state." << llendl;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLGLSquashToFarClip class
///////////////////////////////////////////////////////////////////////////////

LLGLSquashToFarClip::LLGLSquashToFarClip(U32 layer)
{
	F32 depth = 0.99999f - 0.0001f * layer;
	LLMatrix4a proj = gGLProjection;
	LLVector4a col = proj.getColumn<3>();
	col.mul(depth);
	proj.setColumn<2>(col);

	U32 last_matrix_mode = gGL.getMatrixMode();
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(proj);
	gGL.matrixMode(last_matrix_mode);
}

LLGLSquashToFarClip::~LLGLSquashToFarClip()
{
	U32 last_matrix_mode = gGL.getMatrixMode();
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(last_matrix_mode);
}

///////////////////////////////////////////////////////////////////////////////
// LLGLSyncFence class
///////////////////////////////////////////////////////////////////////////////

#if LL_USE_FENCE
LLGLSyncFence::LLGLSyncFence()
{
#ifdef GL_ARB_sync
	mSync = 0;
#endif
}

LLGLSyncFence::~LLGLSyncFence()
{
#ifdef GL_ARB_sync
	if (mSync)
	{
		glDeleteSync(mSync);
	}
#endif
}

void LLGLSyncFence::placeFence()
{
#ifdef GL_ARB_sync
	if (mSync)
	{
		glDeleteSync(mSync);
	}
	mSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#endif
}

bool LLGLSyncFence::isCompleted()
{
	bool ret = true;
#ifdef GL_ARB_sync
	if (mSync)
	{
		GLenum status = glClientWaitSync(mSync, 0, 1);
		if (status == GL_TIMEOUT_EXPIRED)
		{
			ret = false;
		}
	}
#endif
	return ret;
}

void LLGLSyncFence::wait()
{
#ifdef GL_ARB_sync
	if (mSync)
	{
		while (glClientWaitSync(mSync, 0, FENCE_WAIT_TIME_NANOSECONDS) == GL_TIMEOUT_EXPIRED)
		{ //track the number of times we've waited here
			static S32 waits = 0;
			++waits;
		}
	}
#endif
}
#endif // LL_USE_FENCE

///////////////////////////////////////////////////////////////////////////////
// LLGLSPipeline*SkyBox classes
///////////////////////////////////////////////////////////////////////////////

LLGLSPipelineSkyBox::LLGLSPipelineSkyBox()
:	mAlphaTest(GL_ALPHA_TEST),
	mCullFace(GL_CULL_FACE),
	mSquashClip()
{
}

LLGLSPipelineSkyBox::~LLGLSPipelineSkyBox()
{
}

LLGLSPipelineDepthTestSkyBox::LLGLSPipelineDepthTestSkyBox(GLboolean depth_test,
														   GLboolean depth_write)
:	LLGLSPipelineSkyBox(),
	mDepth(depth_test, depth_write, GL_LEQUAL)
{
}

LLGLSPipelineBlendSkyBox::LLGLSPipelineBlendSkyBox(GLboolean depth_test,
												   GLboolean depth_write)
:	LLGLSPipelineDepthTestSkyBox(depth_test, depth_write),
	mBlend(GL_BLEND)
{
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
}


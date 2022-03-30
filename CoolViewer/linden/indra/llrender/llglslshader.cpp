/**
 * @file llglslshader.cpp
 * @brief GLSL helper functions and state.
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

#include "linden_common.h"

#include <utility>

#include "llglslshader.h"

#include "llshadermgr.h"
#include "llrender.h"
#include "llvertexbuffer.h"

#if LL_DARWIN
#include "OpenGL/OpenGL.h"
#endif

#if LL_DEBUG
#define UNIFORM_ERRS llerrs
#else
#define UNIFORM_ERRS llwarns_once
#endif

bool gUseNewShaders = false;

GLhandleARB LLGLSLShader::sCurBoundShader = 0;
LLGLSLShader* LLGLSLShader::sCurBoundShaderPtr = NULL;
S32 LLGLSLShader::sIndexedTextureChannels = 0;
bool LLGLSLShader::sProfileEnabled = false;
std::set<LLGLSLShader*> LLGLSLShader::sInstances;
U64 LLGLSLShader::sTotalTimeElapsed = 0;
U32 LLGLSLShader::sTotalTrianglesDrawn = 0;
U64 LLGLSLShader::sTotalSamplesDrawn = 0;
U32 LLGLSLShader::sTotalDrawCalls = 0;

// UI shaders
LLGLSLShader gUIProgram;
LLGLSLShader gSolidColorProgram;

// Shader constants: keep in sync with LLGLSLShader::EShaderConsts !
static const std::string sShaderConstsKey[LLGLSLShader::NUM_SHADER_CONSTS] =
{
	"LL_SHADER_CONST_CLOUD_MOON_DEPTH",
	"LL_SHADER_CONST_STAR_DEPTH"
};
static const std::string sShaderConstsVal[LLGLSLShader::NUM_SHADER_CONSTS] =
{
	"0.99998",	// SHADER_CONST_CLOUD_MOON_DEPTH
	"0.99999"	// SHADER_CONST_STAR_DEPTH
};

//-----------------------------------------------------------------------------
// LLShaderFeatures class
//-----------------------------------------------------------------------------

LLShaderFeatures::LLShaderFeatures()
:	mIndexedTextureChannels(0),
	calculatesLighting(false),
	calculatesAtmospherics(false),
	hasLighting(false),
	isAlphaLighting(false),
	isShiny(false),
	isFullbright(false),
	isSpecular(false),
	hasWaterFog(false),
	hasTransport(false),
	hasSkinning(false),
	hasObjectSkinning(false),
	hasAtmospherics(false),
	hasGamma(false),
	hasSrgb(false),
	encodesNormal(false),
	isDeferred(false),
	hasShadows(false),
	hasAmbientOcclusion(false),
	disableTextureIndex(false),
	hasAlphaMask(false),
	attachNothing(false)
{
}

//-----------------------------------------------------------------------------
// LLGLSLShader class
//-----------------------------------------------------------------------------

//static
void LLGLSLShader::initProfile()
{
	sProfileEnabled = true;
	sTotalTimeElapsed = 0;
	sTotalTrianglesDrawn = 0;
	sTotalSamplesDrawn = 0;
	sTotalDrawCalls = 0;

	for (std::set<LLGLSLShader*>::iterator iter = sInstances.begin(),
										   end = sInstances.end();
		 iter != end; ++iter)
	{
		(*iter)->clearStats();
	}
}

struct LLGLSLShaderCompareTimeElapsed
{
	LL_INLINE bool operator()(const LLGLSLShader* const& lhs,
							  const LLGLSLShader* const& rhs)
	{
		return lhs->mTimeElapsed < rhs->mTimeElapsed;
	}
};

//static
void LLGLSLShader::finishProfile(bool emit_report)
{
	sProfileEnabled = false;

	if (!emit_report)
	{
		return;
	}

	std::vector<LLGLSLShader*> sorted;

	for (std::set<LLGLSLShader*>::iterator iter = sInstances.begin(),
										   end = sInstances.end();
		 iter != end; ++iter)
	{
		sorted.push_back(*iter);
	}

	std::sort(sorted.begin(), sorted.end(), LLGLSLShaderCompareTimeElapsed());

	for (std::vector<LLGLSLShader*>::iterator iter = sorted.begin(),
											  end = sorted.end();
		 iter != end; ++iter)
	{
		(*iter)->dumpStats();
	}

	llinfos << "\nTotal rendering time: "
			<< llformat("%.4f ms", sTotalTimeElapsed / 1000000.f)
			<< "\nTotal samples drawn: "
			<< llformat("%.4f million", sTotalSamplesDrawn / 1000000.f)
			<< "\nTotal triangles drawn: "
			<< llformat("%.3f million", sTotalTrianglesDrawn / 1000000.f)
			<< llendl;
}

void LLGLSLShader::clearStats()
{
	mTrianglesDrawn = 0;
	mTimeElapsed = 0;
	mSamplesDrawn = 0;
	mDrawCalls = 0;
	mTextureStateFetched = false;
	mTextureMagFilter.clear();
	mTextureMinFilter.clear();
}

void LLGLSLShader::dumpStats()
{
	if (mDrawCalls > 0)
	{
		llinfos << "\n=============================================\n"
				<< mName;
		for (U32 i = 0, count = mShaderFiles.size(); i < count; ++i)
		{
			llcont << "\n" << mShaderFiles[i].first;
		}
		for (U32 i = 0, count = mTexture.size(); i < count; ++i)
		{
			S32 idx = mTexture[i];
			if (idx >= 0)
			{
				S32 uniform_idx = getUniformLocation(i);
				llcont << "\n" << mUniformNameMap[uniform_idx]
					   << " - " << std::hex << mTextureMagFilter[i]
					   << "/" << mTextureMinFilter[i] << std::dec;
			}
		}
		llcont << "\n=============================================";

		F32 ms = mTimeElapsed / 1000000.f;
		F32 seconds = ms / 1000.f;

		F32 pct_tris = (F32)mTrianglesDrawn / (F32)sTotalTrianglesDrawn * 100.f;
		F32 tris_sec = (F32)(mTrianglesDrawn / 1000000.0);
		tris_sec /= seconds;

		F32 pct_samples = (F32)((F64)mSamplesDrawn / (F64)sTotalSamplesDrawn) * 100.f;
		F32 samples_sec = (F32)mSamplesDrawn / 1000000000.0;
		samples_sec /= seconds;

		F32 pct_calls = (F32) mDrawCalls / (F32)sTotalDrawCalls * 100.f;
		U32 avg_batch = mTrianglesDrawn / mDrawCalls;

		llcont << "\nTriangles Drawn: " << mTrianglesDrawn <<  " "
			   << llformat("(%.2f pct of total, %.3f million/sec)", pct_tris,
						   tris_sec )
			   << "\nDraw Calls: " << mDrawCalls << " "
			   << llformat("(%.2f pct of total, avg %d tris/call)", pct_calls,
						   avg_batch)
			   << "\nSamplesDrawn: " << mSamplesDrawn << " "
			   << llformat("(%.2f pct of total, %.3f billion/sec)",
						   pct_samples, samples_sec)
			   << "\nTime Elapsed: " << mTimeElapsed << " "
			   << llformat("(%.2f pct of total, %.5f ms)\n",
						   (F32)((F64)mTimeElapsed/(F64)sTotalTimeElapsed) * 100.f,
						   ms)
			   << llendl;
	}
}

void LLGLSLShader::placeProfileQuery()
{
#if !LL_DARWIN
	if (mTimerQuery == 0)
	{
		glGenQueriesARB(1, &mSamplesQuery);
		glGenQueriesARB(1, &mTimerQuery);
	}

	if (!mTextureStateFetched)
	{
		mTextureStateFetched = true;
		mTextureMagFilter.resize(mTexture.size());
		mTextureMinFilter.resize(mTexture.size());

		U32 cur_active = gGL.getCurrentTexUnitIndex();

		for (U32 i = 0, count = mTexture.size(); i < count; ++i)
		{
			S32 idx = mTexture[i];
			if (idx < 0) continue;

			LLTexUnit* unit = gGL.getTexUnit(idx);
			unit->activate();
			U32 mag = 0xFFFFFFFF;
			U32 min = 0xFFFFFFFF;
			U32 type = LLTexUnit::getInternalType(unit->getCurrType());
			glGetTexParameteriv(type, GL_TEXTURE_MAG_FILTER, (GLint*)&mag);
			glGetTexParameteriv(type, GL_TEXTURE_MIN_FILTER, (GLint*)&min);
			mTextureMagFilter[i] = mag;
			mTextureMinFilter[i] = min;
		}

		gGL.getTexUnit(cur_active)->activate();
	}

	glBeginQueryARB(GL_SAMPLES_PASSED, mSamplesQuery);
	glBeginQueryARB(GL_TIME_ELAPSED, mTimerQuery);
#endif
}

void LLGLSLShader::readProfileQuery(U32 count, U32 mode)
{
#if !LL_DARWIN
	glEndQueryARB(GL_TIME_ELAPSED);
	glEndQueryARB(GL_SAMPLES_PASSED);

	GLuint64 time_elapsed = 0;
	glGetQueryObjectui64v(mTimerQuery, GL_QUERY_RESULT, &time_elapsed);

	GLuint64 samples_passed = 0;
	glGetQueryObjectui64v(mSamplesQuery, GL_QUERY_RESULT, &samples_passed);

	sTotalTimeElapsed += time_elapsed;
	mTimeElapsed += time_elapsed;

	sTotalSamplesDrawn += samples_passed;
	mSamplesDrawn += samples_passed;

	U32 tri_count = 0;
	switch (mode)
	{
		case LLRender::TRIANGLES:
			tri_count = count / 3;
			break;

		case LLRender::TRIANGLE_FAN:
			tri_count = count - 2;
			break;

		case LLRender::TRIANGLE_STRIP:
			tri_count = count - 2;
			break;

		default:
			// points lines etc just use primitive count
			tri_count = count;
			break;
	}

	mTrianglesDrawn += tri_count;
	sTotalTrianglesDrawn += tri_count;

	++sTotalDrawCalls;
	++mDrawCalls;
#endif
}

LLGLSLShader::LLGLSLShader()
:	mProgramObject(0),
	mAttributeMask(0),
	mTotalUniformSize(0),
	mActiveTextureChannels(0),
	mShaderLevel(0),
	mShaderGroup(SG_DEFAULT),
	mUniformsDirty(true),
	mTimerQuery(0),
	mSamplesQuery(0)
{
}

void LLGLSLShader::setup(const char* name, S32 level,
						 const char* vertex_shader,
						 const char* fragment_shader)
{
	// NOTE: sadly, vertex shader names do not all end with "V.glsl", and
	// fragment shader names do not all end with "F.glsl", so only check for
	// contradictory naming...
	if (strstr(vertex_shader, "F.glsl"))
	{
		llerrs << "Passing a fragment shader name for the vertex shader: "
			   << vertex_shader << llendl;
	}
	if (strstr(fragment_shader, "V.glsl"))
	{
		llerrs << "Passing a vertex shader name for the fragment shader: "
			   << fragment_shader << llendl;
	}

	mName = name;
	mShaderLevel = level;
	mShaderFiles.clear();
	mShaderFiles.emplace_back(vertex_shader, GL_VERTEX_SHADER_ARB);
	mShaderFiles.emplace_back(fragment_shader, GL_FRAGMENT_SHADER_ARB);

	// Reset everything else to the default values

	mDefines.clear();

	mUniformsDirty = true;

	clearStats();

	mShaderGroup = SG_DEFAULT;
	mActiveTextureChannels = 0;
	mTimerQuery = 0;
	mSamplesQuery = 0;
	mAttributeMask = 0;
	mTotalUniformSize = 0;

	mFeatures.mIndexedTextureChannels = 0;
	mFeatures.calculatesLighting = mFeatures.calculatesAtmospherics =
		mFeatures.hasLighting = mFeatures.isAlphaLighting = mFeatures.isShiny =
		mFeatures.isFullbright = mFeatures.isSpecular = mFeatures.hasWaterFog =
		mFeatures.hasTransport = mFeatures.hasSkinning =
		mFeatures.hasObjectSkinning = mFeatures.hasAtmospherics =
		mFeatures.hasGamma = mFeatures.hasSrgb = mFeatures.encodesNormal =
		mFeatures.isDeferred = mFeatures.hasShadows =
		mFeatures.hasAmbientOcclusion = mFeatures.disableTextureIndex =
		mFeatures.hasAlphaMask = mFeatures.attachNothing = false;
}

void LLGLSLShader::unload()
{
	mShaderFiles.clear();
	mDefines.clear();
	unloadInternal();
}

void LLGLSLShader::unloadInternal()
{
	sInstances.erase(this);

	stop_glerror();
	mAttribute.clear();
	mTexture.clear();
	mUniform.clear();

	if (mProgramObject)
	{
		GLhandleARB obj[1024];
		GLsizei count;

		glGetAttachedObjectsARB(mProgramObject, sizeof(obj) / sizeof(obj[0]),
								&count, obj);
		for (GLsizei i = 0; i < count; ++i)
		{
			glDetachObjectARB(mProgramObject, obj[i]);
			glDeleteObjectARB(obj[i]);
		}

		glDeleteObjectARB(mProgramObject);

		mProgramObject = 0;
	}

	if (mTimerQuery)
	{
		glDeleteQueriesARB(1, &mTimerQuery);
		mTimerQuery = 0;
	}

	if (mSamplesQuery)
	{
		glDeleteQueriesARB(1, &mSamplesQuery);
		mSamplesQuery = 0;
	}

	// *HACK: to make Apple not complain
	glGetError();

	stop_glerror();
}

bool LLGLSLShader::createShader(hash_vector_t* attributes,
								hash_vector_t* uniforms, U32 varying_count,
								const char** varyings)
{
	unloadInternal();

	sInstances.insert(this);

	// Reloading, reset matrix hash values
	for (U32 i = 0; i < LLRender::NUM_MATRIX_MODES; ++i)
	{
		mMatHash[i] = 0xFFFFFFFF;
	}
	mLightHash = 0xFFFFFFFF;

	llassert_always(!mShaderFiles.empty());

	// Create program
	mProgramObject = glCreateProgramObjectARB();
	if (mProgramObject == 0)
	{
		// This should not happen if shader-related extensions, like
		// ARB_vertex_shader, exist.
		llwarns << "Failed to create handle for shader: " << mName << llendl;
		return false;
	}

	bool success = true;

	LLShaderMgr* shadermgr = LLShaderMgr::getInstance();

#if LL_DARWIN
	// Work-around missing mix(vec3,vec3,bvec3)
	mDefines["OLD_SELECT"] = "1";
#endif

	// Compile new source
	GLhandleARB shaderhandle;
	for (files_map_t::iterator it = mShaderFiles.begin();
		 it != mShaderFiles.end(); ++it)
	{
		shaderhandle =
			shadermgr->loadShaderFile(it->first, mShaderLevel, it->second,
									  &mDefines,
									  mFeatures.mIndexedTextureChannels);
		LL_DEBUGS("ShaderLoading") << "SHADER FILE: " << it->first
								   << " mShaderLevel=" << mShaderLevel
								   << LL_ENDL;
		if (shaderhandle)
		{
			attachObject(shaderhandle);
		}
		else
		{
			success = false;
		}
	}

	// Attach existing objects
	if (!shadermgr->attachShaderFeatures(this))
	{
		return false;
	}

	if (gGLManager.mGLSLVersionMajor < 2 && gGLManager.mGLSLVersionMinor < 3)
	{
		// Indexed texture rendering requires GLSL 1.3 or later
		// attachShaderFeatures may have set the number of indexed texture
		// channels, so set to 1 again
		mFeatures.mIndexedTextureChannels =
			llmin(mFeatures.mIndexedTextureChannels, 1);
	}

	// Map attributes and uniforms
	if (success)
	{
		success = mapAttributes(attributes);
	}
	else
	{
		llwarns << "Failed to map attributes for: " << mName << llendl;
	}
	if (success)
	{
		success = mapUniforms(uniforms);
	}
	else
	{
		llwarns << "Failed to map uniforms for: " << mName << llendl;
	}

	if (!success)
	{
		// Try again using a lower shader level;
		if (mShaderLevel > 0)
		{
			llwarns << "Failed to link using shader level "
					<< mShaderLevel << " trying again using shader level "
					<< mShaderLevel - 1 << llendl;
			--mShaderLevel;
			return createShader(attributes, uniforms);
		}
		llwarns << "Failed to link shader: " << mName << llendl;
		return false;
	}

	if (mFeatures.mIndexedTextureChannels > 0)
	{
		// Override texture channels for indexed texture rendering
		bind();
		S32 channel_count = mFeatures.mIndexedTextureChannels;

		for (S32 i = 0; i < channel_count; ++i)
		{
			LLStaticHashedString uniName(llformat("tex%d", i));
			uniform1i(uniName, i);
		}

		// Adjust any texture channels that might have been overwritten
		S32 cur_tex = channel_count;

		for (U32 i = 0, count = mTexture.size(); i < count; ++i)
		{
			if (mTexture[i] > -1 && mTexture[i] < channel_count)
			{
				llassert(cur_tex < gGLManager.mNumTextureImageUnits);
				uniform1i(i, cur_tex);
				mTexture[i] = cur_tex++;
			}
		}
		unbind();
	}

	return success;
}

bool LLGLSLShader::attachVertexObject(const char* object)
{
	// NOTE: sadly, vertex shader names do not all end with "V.glsl", so only
	// check for contradictory naming...
	if (strstr(object, "F.glsl"))
	{
		llerrs << "Passing a vertex shader name for a fragment shader: "
			   << object << llendl;
	}
	LL_DEBUGS("ShaderLoading") << "Attaching: " << object << LL_ENDL;
	LLShaderMgr::map_citer_t it =
		LLShaderMgr::sVertexShaderObjects.find(object);
	if (it != LLShaderMgr::sVertexShaderObjects.end())
	{
		stop_glerror();
		glAttachObjectARB(mProgramObject, it->second);
		stop_glerror();
		return true;
	}

	llwarns << "Attempting to attach shader object that has not been compiled: "
			<< object << llendl;
	return false;
}

bool LLGLSLShader::attachFragmentObject(const char* object)
{
	// NOTE: sadly, fragment shader names do not all end with "F.glsl", so only
	// check for contradictory naming...
	if (strstr(object, "V.glsl"))
	{
		llerrs << "Passing a fragment shader name for a vertex shader: "
			   << object << llendl;
	}
	LL_DEBUGS("ShaderLoading") << "Attaching: " << object << LL_ENDL;
	LLShaderMgr::map_citer_t it =
		LLShaderMgr::sFragmentShaderObjects.find(object);
	if (it != LLShaderMgr::sFragmentShaderObjects.end())
	{
		stop_glerror();
		glAttachObjectARB(mProgramObject, it->second);
		stop_glerror();
		return true;
	}

	llwarns << "Attempting to attach shader object that has not been compiled: "
			<< object << llendl;
	return false;
}

void LLGLSLShader::attachObject(GLhandleARB object)
{
	if (!object)
	{
		llwarns << "Attempting to attach non existing shader object."
				<< llendl;
		return;
	}
	stop_glerror();
	glAttachObjectARB(mProgramObject, object);
	stop_glerror();
}

void LLGLSLShader::attachObjects(GLhandleARB* objects, S32 count)
{
	for (S32 i = 0; i < count; ++i)
	{
		attachObject(objects[i]);
	}
}

bool LLGLSLShader::mapAttributes(const hash_vector_t* attributes)
{
	// Before linking, make sure reserved attributes always have consistent
	// locations
	for (U32 i = 0, count = LLShaderMgr::sReservedAttribs.size();
		 i < count; ++i)
	{
		const char* name = LLShaderMgr::sReservedAttribs[i].c_str();
		glBindAttribLocationARB(mProgramObject, i, (const GLcharARB*)name);
	}

	// Link the program
	bool res = LLShaderMgr::getInstance()->linkProgramObject(mProgramObject,
															 false);

	mAttribute.clear();
	U32 num_attrs = attributes ? attributes->size() : 0;
	mAttribute.resize(LLShaderMgr::sReservedAttribs.size() + num_attrs, -1);

	// Read back channel locations
	if (res)
	{
		mAttributeMask = 0;

		// Read back reserved channels first
		for (U32 i = 0, count = LLShaderMgr::sReservedAttribs.size(); i < count;
			 ++i)
		{
			const char* name = LLShaderMgr::sReservedAttribs[i].c_str();
			S32 index = glGetAttribLocationARB(mProgramObject,
											   (const GLcharARB*)name);
			if (index != -1)
			{
				mAttribute[i] = index;
				mAttributeMask |= 1 << i;
				LL_DEBUGS("ShaderLoading") << "Attribute " << name
										   << " assigned to channel " << index
										   << LL_ENDL;
			}
		}
		if (attributes)
		{
			U32 size = LLShaderMgr::sReservedAttribs.size();
			for (U32 i = 0; i < num_attrs; ++i)
			{
				const char* name = (*attributes)[i].String().c_str();
				S32 index = glGetAttribLocationARB(mProgramObject, name);
				if (index != -1)
				{
					mAttribute[size + i] = index;
					LL_DEBUGS("ShaderLoading") << "Attribute " << name
											   << " assigned to channel "
											   << index << LL_ENDL;
				}
			}
		}
	}

	return res;
}

void LLGLSLShader::mapUniform(S32 index, const hash_vector_t* uniforms)
{
	if (index == -1)
	{
		return;
	}

	GLenum type;
	GLsizei length;
	S32 size = -1;
	char name[1024];
	name[0] = 0;

	glGetActiveUniformARB(mProgramObject, index, 1024, &length, &size, &type,
						  (GLcharARB*)name);
#if !LL_DARWIN
	if (size > 0)
	{
		switch (type)
		{
			case GL_FLOAT_VEC2:			size *= 2; break;
			case GL_FLOAT_VEC3:			size *= 3; break;
			case GL_FLOAT_VEC4:			size *= 4; break;
			case GL_DOUBLE:				size *= 2; break;
			case GL_DOUBLE_VEC2:		size *= 2; break;
			case GL_DOUBLE_VEC3:		size *= 6; break;
			case GL_DOUBLE_VEC4:		size *= 8; break;
			case GL_INT_VEC2:			size *= 2; break;
			case GL_INT_VEC3:			size *= 3; break;
			case GL_INT_VEC4:			size *= 4; break;
			case GL_UNSIGNED_INT_VEC2:	size *= 2; break;
			case GL_UNSIGNED_INT_VEC3:	size *= 3; break;
			case GL_UNSIGNED_INT_VEC4:	size *= 4; break;
			case GL_BOOL_VEC2:			size *= 2; break;
			case GL_BOOL_VEC3:			size *= 3; break;
			case GL_BOOL_VEC4:			size *= 4; break;
			case GL_FLOAT_MAT2:			size *= 4; break;
			case GL_FLOAT_MAT3:			size *= 9; break;
			case GL_FLOAT_MAT4:			size *= 16; break;
			case GL_FLOAT_MAT2x3:		size *= 6; break;
			case GL_FLOAT_MAT2x4:		size *= 8; break;
			case GL_FLOAT_MAT3x2:		size *= 6; break;
			case GL_FLOAT_MAT3x4:		size *= 12; break;
			case GL_FLOAT_MAT4x2:		size *= 8; break;
			case GL_FLOAT_MAT4x3:		size *= 12; break;
			case GL_DOUBLE_MAT2:		size *= 8; break;
			case GL_DOUBLE_MAT3:		size *= 18; break;
			case GL_DOUBLE_MAT4:		size *= 32; break;
			case GL_DOUBLE_MAT2x3:		size *= 12; break;
			case GL_DOUBLE_MAT2x4:		size *= 16; break;
			case GL_DOUBLE_MAT3x2:		size *= 12; break;
			case GL_DOUBLE_MAT3x4:		size *= 24; break;
			case GL_DOUBLE_MAT4x2:		size *= 16; break;
			case GL_DOUBLE_MAT4x3:		size *= 24; break;
		}
		mTotalUniformSize += size;
	}
#endif

	S32 location = glGetUniformLocationARB(mProgramObject, name);
	if (location != -1)
	{
		// Chop off "[0]" so we can always access the first element of an array
		// by the array name
		char* is_array = strstr(name, "[0]");
		if (is_array)
		{
			is_array[0] = 0;
		}

		LLStaticHashedString hashedName(name);
		mUniformNameMap[location] = name;
		mUniformMap[hashedName] = location;
		LL_DEBUGS("ShaderLoading") << "Uniform " << name << " is at location "
								   << location << LL_ENDL;

		// Find the index of this uniform
		U32 size = LLShaderMgr::sReservedUniforms.size();
		for (U32 i = 0; i < size; ++i)
		{
			if (mUniform[i] == -1 &&
				LLShaderMgr::sReservedUniforms[i] == name)
			{
				// Found it
				mUniform[i] = location;
				mTexture[i] = mapUniformTextureChannel(location, type);
				return;
			}
		}

		if (uniforms)
		{
			for (U32 i = 0; i < uniforms->size(); ++i)
			{
				if (mUniform[i + size] == -1 &&
					(*uniforms)[i].String() == name)
				{
					// Found it
					mUniform[i + size] = location;
					mTexture[i + size] = mapUniformTextureChannel(location,
																  type);
					return;
				}
			}
		}
	}
 }

void LLGLSLShader::addConstant(EShaderConsts shader_const)
{
	mDefines[sShaderConstsKey[shader_const]] = sShaderConstsVal[shader_const];
}

void LLGLSLShader::addPermutation(const std::string& name,
								  const std::string& value)
{
	mDefines[name] = value;
}

void LLGLSLShader::removePermutation(const std::string& name)
{
	mDefines[name].erase();
}

S32 LLGLSLShader::mapUniformTextureChannel(S32 location, U32 type)
{
	if ((type >= GL_SAMPLER_1D_ARB && type <= GL_SAMPLER_2D_RECT_SHADOW_ARB) ||
		type == GL_SAMPLER_2D_MULTISAMPLE)
	{
		// This is a texture
		glUniform1iARB(location, mActiveTextureChannels);
		LL_DEBUGS("ShaderLoading") << "Assigned to texture channel "
								   << mActiveTextureChannels << LL_ENDL;
		return mActiveTextureChannels++;
	}
	return -1;
}

bool LLGLSLShader::mapUniforms(const hash_vector_t* uniforms)
{
	mTotalUniformSize = 0;
	mActiveTextureChannels = 0;
	mUniform.clear();
	mUniformMap.clear();
	mUniformNameMap.clear();
	mTexture.clear();
	mValue.clear();
	// Initialize arrays
	U32 num_uniforms = uniforms ? uniforms->size() : 0;
	U32 size = num_uniforms + LLShaderMgr::sReservedUniforms.size();
	mUniform.resize(size, -1);
	mTexture.resize(size, -1);

	bind();

	// Get the number of active uniforms
	GLint active_count;
	glGetObjectParameterivARB(mProgramObject, GL_OBJECT_ACTIVE_UNIFORMS_ARB,
							  &active_count);

	// This is part of code is temporary because as the final result the
	// mapUniform() should be rewritten. But it would need a lot of work to
	// avoid possible regressions.
	// The reason of this code is that SL engine is very sensitive to the fact
	// that "diffuseMap" should appear first as uniform parameter so it gains
	// the 0-"texture channel" index (see mapUniformTextureChannel() and
	// mActiveTextureChannels); it influences which texture matrix will be
	// updated during rendering.
	// The order of indexes of uniform variables is not defined and the GLSL
	// compilers may change it as they see fit, even if the "diffuseMap"
	// appears and is used first in the shader code.
	S32 diffuse_map = -1;
	S32 specular_map = -1;
	S32 bump_map = -1;
	S32 environment_map = -1;
	S32 altdiffuse_map = -1;

	static const char diffmapname[] = "diffuseMap";
	static const char specularmapname[] = "specularMap";
	static const char bumpmapname[] = "bumpMap";
	static const char envmapname[] = "environmentMap";
	static const char altdiffusename[] = "altdiffusename";
	if (glGetUniformLocationARB(mProgramObject, diffmapname) != -1 &&
		(glGetUniformLocationARB(mProgramObject, specularmapname) != -1 ||
		 glGetUniformLocationARB(mProgramObject, bumpmapname) != -1 ||
		 glGetUniformLocationARB(mProgramObject, envmapname) != -1 ||
		 (gUseNewShaders &&
		  glGetUniformLocationARB(mProgramObject, "altDiffuseMap") != -1)))
	{
		char name[1024];
		GLenum type;
		GLsizei length;
		GLint size;
		for (S32 i = 0; i < active_count; ++i)
		{
			name[0] = '\0';
			glGetActiveUniformARB(mProgramObject, i, 1024, &length, &size,
								  &type, (GLcharARB*)name);

			if (diffuse_map == -1 && strcmp(name, diffmapname) == 0)
			{
				diffuse_map = i;
				if (specular_map != -1 && bump_map != -1 &&
					environment_map != -1 &&
					(!gUseNewShaders || altdiffuse_map != -1))
				{
					break;	// We are done !
				}
			}
			else if (specular_map == -1 && strcmp(name, specularmapname) == 0)
			{
				specular_map = i;
				if (diffuse_map != -1 && bump_map != -1 &&
					environment_map != -1 &&
					(!gUseNewShaders || altdiffuse_map != -1))
				{
					break;	// We are done !
				}
			}
			else if (bump_map == -1 && strcmp(name, bumpmapname) == 0)
			{
				bump_map = i;
				if (diffuse_map != -1 && specular_map != -1 &&
					environment_map != -1 &&
					(!gUseNewShaders || altdiffuse_map != -1))
				{
					break;	// We are done !
				}
			}
			else if (environment_map == -1 && strcmp(name, envmapname) == 0)
			{
				environment_map = i;
				if (diffuse_map != -1 && specular_map != -1 &&
					bump_map != -1 &&
					(!gUseNewShaders || altdiffuse_map != -1))
				{
					break;	// We are done !
				}
			}
			else if (gUseNewShaders && altdiffuse_map == -1 &&
					 strcmp(name, altdiffusename) == 0)
			{
				altdiffuse_map = i;
				if (diffuse_map != -1 && specular_map != -1 &&
					bump_map != -1 && environment_map != -1)
				{
					break;	// We are done !
				}
			}
		}
		// Map uniforms in the proper order
		if (diffuse_map != -1)
		{
			mapUniform(diffuse_map, uniforms);
		}
		else
		{
			llwarns << "Diffuse map advertized but not found in program object "
					<< mProgramObject << " !" << llendl;
			llassert(false);
		}
		if (gUseNewShaders && altdiffuse_map != -1)
		{
			mapUniform(altdiffuse_map, uniforms);
		}
		if (specular_map != -1)
		{
			mapUniform(specular_map, uniforms);
		}
		if (bump_map != -1)
		{
			mapUniform(bump_map, uniforms);
		}
		if (environment_map != -1)
		{
			mapUniform(environment_map, uniforms);
		}
	}

	for (S32 i = 0; i < active_count; ++i)
	{
		if (i != specular_map && i != bump_map && i != diffuse_map &&
			i != environment_map && i != altdiffuse_map)
		{
			mapUniform(i, uniforms);
		}
	}

	unbind();

	LL_DEBUGS("ShaderLoading") << "Total Uniform Size: " << mTotalUniformSize
							   << LL_ENDL;

	return true;
}

void LLGLSLShader::bind()
{
	if (sCurBoundShader == mProgramObject)
	{
		if (gDebugGL)
		{
			llwarns_once << "Attempt to re-bind currently bound shader program: "
						 << mName << ". Ignored." << llendl;
		}
		return;
	}

	gGL.flush();
	LLVertexBuffer::unbind();
	glUseProgramObjectARB(mProgramObject);
	sCurBoundShader = mProgramObject;
	sCurBoundShaderPtr = this;
	if (mUniformsDirty)
	{
		LLShaderMgr::getInstance()->updateShaderUniforms(this);
		mUniformsDirty = false;
	}
}

void LLGLSLShader::unbind()
{
	gGL.flush();
	stop_glerror();
	LLVertexBuffer::unbind();
	glUseProgramObjectARB(0);
	sCurBoundShader = 0;
	sCurBoundShaderPtr = NULL;
	stop_glerror();
}

void LLGLSLShader::bindNoShader()
{
	LLVertexBuffer::unbind();
	glUseProgramObjectARB(0);
	sCurBoundShader = 0;
	sCurBoundShaderPtr = NULL;
}

S32 LLGLSLShader::bindTexture(const std::string& uniform, LLGLTexture* texture,
							  LLTexUnit::eTextureType mode,
							  LLTexUnit::eTextureColorSpace colorspace)
{
	return bindTexture(getUniformLocation(uniform), texture, mode, colorspace);
}

S32 LLGLSLShader::bindTexture(S32 uniform, LLGLTexture* texture,
							  LLTexUnit::eTextureType mode,
							  LLTexUnit::eTextureColorSpace colorspace)
{
	if (uniform < 0 || uniform >= (S32)mTexture.size())
	{
		UNIFORM_ERRS << "Uniform out of range: " << uniform << LL_ENDL;
		if (uniform >= 0)
		{
			// Force an update of the uniforms for this shader...
			LLShaderMgr::getInstance()->updateShaderUniforms(this);
		}
		return -1;
	}

	uniform = mTexture[uniform];

	if (uniform > -1)
	{
		LLTexUnit* unit = gGL.getTexUnit(uniform);
		unit->bind(texture, mode);
		unit->setTextureColorSpace(colorspace);
	}

	return uniform;
}

S32 LLGLSLShader::unbindTexture(const std::string& uniform,
								LLTexUnit::eTextureType)
{
	return unbindTexture(getUniformLocation(uniform));
}

S32 LLGLSLShader::unbindTexture(S32 uniform, LLTexUnit::eTextureType mode)
{
	if (uniform < 0 || uniform >= (S32)mTexture.size())
	{
		UNIFORM_ERRS << "Uniform out of range: " << uniform << LL_ENDL;
		return -1;
	}

	uniform = mTexture[uniform];
	if (uniform > -1)
	{
		gGL.getTexUnit(uniform)->unbind(mode);
	}
	return uniform;
}

S32 LLGLSLShader::enableTexture(S32 uniform, LLTexUnit::eTextureType mode,
								LLTexUnit::eTextureColorSpace colorspace)
{
	if (uniform < 0 || uniform >= (S32)mTexture.size())
	{
		UNIFORM_ERRS << "Uniform out of range: " << uniform << LL_ENDL;
		return -1;
	}
	S32 index = mTexture[uniform];
	if (index != -1)
	{
		LLTexUnit* unit = gGL.getTexUnit(index);		
		unit->activate();
		unit->enable(mode);
		unit->setTextureColorSpace(colorspace);
	}
	return index;
}

S32 LLGLSLShader::disableTexture(S32 uniform, LLTexUnit::eTextureType mode,
								 LLTexUnit::eTextureColorSpace colorspace)
{
	if (uniform < 0 || uniform >= (S32)mTexture.size())
	{
		UNIFORM_ERRS << "Uniform out of range: " << uniform << LL_ENDL;
		return -1;
	}

	S32 index = mTexture[uniform];
	if (index == -1)
	{
		return -1;
	}

	LLTexUnit* unit = gGL.getTexUnit(index);
	if (unit->getCurrType() != LLTexUnit::TT_NONE)
	{
		if (gDebugGL && unit->getCurrType() != mode &&
			unit->getCurColorSpace() != colorspace)
		{
			llwarns_once << "Texture channel " << index
						 << " texture type corrupted." << llendl;
		}
		unit->disable();
	}
	return index;
}

void LLGLSLShader::uniform1i(U32 index, S32 x)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(mUniform[index]);
			if (iter == mValue.end() || iter->second.mV[0] != x)
			{
				glUniform1iARB(mUniform[index], x);
				mValue[mUniform[index]] = LLVector4(x, 0.f, 0.f, 0.f);
			}
		}
	}
}

void LLGLSLShader::uniform1f(U32 index, F32 x)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(mUniform[index]);
			if (iter == mValue.end() || iter->second.mV[0] != x)
			{
				glUniform1fARB(mUniform[index], x);
				mValue[mUniform[index]] = LLVector4(x, 0.f, 0.f, 0.f);
			}
		}
	}
}

void LLGLSLShader::uniform2f(U32 index, F32 x, F32 y)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(x, y, 0.f, 0.f);
			if (iter == mValue.end() || iter->second != vec)
			{
				glUniform2fARB(mUniform[index], x, y);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform3f(U32 index, F32 x, F32 y, F32 z)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(x, y, z, 0.f);
			if (iter == mValue.end() || iter->second != vec)
			{
				glUniform3fARB(mUniform[index], x, y, z);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform4f(U32 index, F32 x, F32 y, F32 z, F32 w)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(x, y, z, w);
			if (iter == mValue.end() || iter->second != vec)
			{
				glUniform4fARB(mUniform[index], x, y, z, w);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform1iv(U32 index, U32 count, const S32* v)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(v[0], 0.f, 0.f, 0.f);
			if (count != 1 || iter == mValue.end() || iter->second != vec)
			{
				glUniform1ivARB(mUniform[index], count, v);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform1fv(U32 index, U32 count, const F32* v)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(v[0], 0.f, 0.f, 0.f);
			if (count != 1 || iter == mValue.end() || iter->second != vec)
			{
				glUniform1fvARB(mUniform[index], count, v);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform2fv(U32 index, U32 count, const F32* v)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(v[0], v[1], 0.f, 0.f);
			if (count != 1 || iter == mValue.end() || iter->second != vec)
			{
				glUniform2fvARB(mUniform[index], count, v);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform3fv(U32 index, U32 count, const F32* v)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(v[0], v[1], v[2], 0.f);
			if (count != 1 || iter == mValue.end() || iter->second != vec)
			{
				glUniform3fvARB(mUniform[index], count, v);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform4fv(U32 index, U32 count, const F32* v)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			uniform_value_map_t::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(v[0],v[1],v[2],v[3]);
			if (count != 1 || iter == mValue.end() || iter->second != vec)
			{
				glUniform4fvARB(mUniform[index], count, v);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniformMatrix2fv(U32 index, U32 count, GLboolean transpose,
									const F32* v)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			glUniformMatrix2fvARB(mUniform[index], count, transpose, v);
		}
	}
}

void LLGLSLShader::uniformMatrix3fv(U32 index, U32 count, GLboolean transpose,
									const F32* v)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			glUniformMatrix3fvARB(mUniform[index], count, transpose, v);
		}
	}
}

void LLGLSLShader::uniformMatrix3x4fv(U32 index, U32 count,
									  GLboolean transpose, const F32* v)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			glUniformMatrix3x4fv(mUniform[index], count, transpose, v);
		}
	}
}

void LLGLSLShader::uniformMatrix4fv(U32 index, U32 count, GLboolean transpose,
									const F32* v)
{
	if (mProgramObject)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			glUniformMatrix4fvARB(mUniform[index], count, transpose, v);
		}
	}
}

S32 LLGLSLShader::getUniformLocation(const LLStaticHashedString& uniform)
{
	if (!mProgramObject)
	{
		return -1;
	}

	LLStaticStringTable<S32>::iterator iter = mUniformMap.find(uniform);
	if (iter == mUniformMap.end())
	{
		return -1;
	}

	if (gDebugGL)
	{
		stop_glerror();
		if (iter->second != glGetUniformLocationARB(mProgramObject,
													uniform.String().c_str()))
		{
			llwarns_once << "Uniform does not match: "
						 << uniform.String().c_str() << llendl;
		}
	}

	return iter->second;
}

S32 LLGLSLShader::getUniformLocation(U32 index)
{
	if (mProgramObject && index < mUniform.size())
	{
		return mUniform[index];
	}
	return -1;
}

S32 LLGLSLShader::getAttribLocation(U32 attrib)
{
	return attrib < mAttribute.size() ? mAttribute[attrib] : -1;
}

void LLGLSLShader::uniform1i(const LLStaticHashedString& uniform, S32 v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(v, 0.f, 0.f, 0.f);
		if (iter == mValue.end() || iter->second != vec)
		{
			glUniform1iARB(location, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform2i(const LLStaticHashedString& uniform, S32 i, S32 j)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(i, j, 0.f, 0.f);
		if (iter == mValue.end() || iter->second != vec)
		{
			glUniform2iARB(location, i, j);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform1f(const LLStaticHashedString& uniform, F32 v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(v, 0.f, 0.f, 0.f);
		if (iter == mValue.end() || iter->second != vec)
		{
			glUniform1fARB(location, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform2f(const LLStaticHashedString& uniform, F32 x, F32 y)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(x, y, 0.f, 0.f);
		if (iter == mValue.end() || iter->second != vec)
		{
			glUniform2fARB(location, x,y);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform3f(const LLStaticHashedString& uniform, F32 x,
							 F32 y, F32 z)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(x, y, z, 0.f);
		if (iter == mValue.end() || iter->second != vec)
		{
			glUniform3fARB(location, x, y, z);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform1fv(const LLStaticHashedString& uniform, U32 count,
							  const F32* v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(v[0], 0.f, 0.f, 0.f);
		if (count != 1 || iter == mValue.end() || iter->second != vec)
		{
			glUniform1fvARB(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform2fv(const LLStaticHashedString& uniform, U32 count,
							  const F32* v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(v[0], v[1], 0.f, 0.f);
		if (count != 1 || iter == mValue.end() || iter->second != vec)
		{
			glUniform2fvARB(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform3fv(const LLStaticHashedString& uniform, U32 count,
							  const F32* v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		uniform_value_map_t::iterator iter = mValue.find(location);
		LLVector4 vec(v[0], v[1], v[2], 0.f);
		if (count != 1 || iter == mValue.end() || iter->second != vec)
		{
			glUniform3fvARB(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform4fv(const LLStaticHashedString& uniform, U32 count,
							  const F32* v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		LLVector4 vec(v);
		uniform_value_map_t::iterator iter = mValue.find(location);
		if (count != 1 || iter == mValue.end() || iter->second != vec)
		{
			stop_glerror();
			glUniform4fvARB(location, count, v);
			stop_glerror();
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniformMatrix4fv(const LLStaticHashedString& uniform,
									U32 count, GLboolean transpose,
									const F32* v)
{
	S32 location = getUniformLocation(uniform);
	if (location >= 0)
	{
		stop_glerror();
		glUniformMatrix4fvARB(location, count, transpose, v);
		stop_glerror();
	}
}

void LLGLSLShader::vertexAttrib4f(U32 index, F32 x, F32 y, F32 z, F32 w)
{
	if (mAttribute[index] > 0)
	{
		glVertexAttrib4fARB(mAttribute[index], x, y, z, w);
	}
}

void LLGLSLShader::vertexAttrib4fv(U32 index, F32* v)
{
	if (mAttribute[index] > 0)
	{
		glVertexAttrib4fvARB(mAttribute[index], v);
	}
}

void LLGLSLShader::setMinimumAlpha(F32 minimum)
{
	gGL.flush();
	uniform1f(LLShaderMgr::MINIMUM_ALPHA, minimum);
}

//-----------------------------------------------------------------------------
// LLShaderUniforms class
//-----------------------------------------------------------------------------

void LLShaderUniforms::apply(LLGLSLShader* shader)
{
	if (!mActive)
	{
		return;
	}
	for (U32 i = 0, count = mIntegers.size(); i < count; ++i)
	{
		const IntSetting& uniform = mIntegers[i];
		shader->uniform1i(uniform.mUniform, uniform.mValue);
	}
	for (U32 i = 0, count = mFloats.size(); i < count; ++i)
	{
		const FloatSetting& uniform = mFloats[i];
		shader->uniform1f(uniform.mUniform, uniform.mValue);
	}
	for (U32 i = 0, count = mVectors.size(); i < count; ++i)
	{
		const VectorSetting& uniform = mVectors[i];
		shader->uniform4fv(uniform.mUniform, 1, uniform.mValue.mV);
	}
	for (U32 i = 0, count = mVector3s.size(); i < count; ++i)
	{
		const Vector3Setting& uniform = mVector3s[i];
		shader->uniform3fv(uniform.mUniform, 1, uniform.mValue.mV);
	}
}

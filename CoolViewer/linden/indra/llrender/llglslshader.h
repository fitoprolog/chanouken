/**
 * @file llglslshader.h
 * @brief GLSL shader wrappers
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

#ifndef LL_LLGLSLSHADER_H
#define LL_LLGLSLSHADER_H

#include <vector>

#include "llgl.h"
#include "llrender.h"
#include "llstaticstringtable.h"

class LLShaderFeatures
{
public:
	LLShaderFeatures();

public:
	S32 mIndexedTextureChannels;

	bool calculatesLighting;
	bool calculatesAtmospherics;
	// Implies no transport (it is possible to have neither though):
	bool hasLighting;
	// Indicates lighting shaders need not be linked in (lighting performed
	// directly in alpha shader to match deferred lighting functions):
	bool isAlphaLighting;
	bool isShiny;
	bool isFullbright;		// Implies no lighting
	bool isSpecular;
	bool hasWaterFog;		// Implies no gamma
	// Implies no lighting (it is possible to have neither though):
	bool hasTransport;
	bool hasSkinning;
	bool hasObjectSkinning;
	bool hasAtmospherics;
	bool hasGamma;
	bool hasSrgb;
	bool encodesNormal;
	bool isDeferred;
	bool hasShadows;
	bool hasAmbientOcclusion;
	bool disableTextureIndex;
	bool hasAlphaMask;
	bool attachNothing;
};

class LLGLSLShader
{
protected:
	LOG_CLASS(LLGLSLShader);

public:
	enum
	{
		SG_DEFAULT = 0,	// Not sky or water specific
		SG_SKY,
		SG_WATER,
		SG_ANY,
		SG_COUNT
	};

	enum EShaderConsts
	{
		CONST_CLOUD_MOON_DEPTH,
		CONST_STAR_DEPTH,
		NUM_SHADER_CONSTS
	};

	LLGLSLShader();

	static void initProfile();
	static void finishProfile(bool emit_report = true);

	LL_INLINE static void startProfile()
	{
		if (sProfileEnabled && sCurBoundShaderPtr)
		{
			sCurBoundShaderPtr->placeProfileQuery();
		}
	}

	LL_INLINE static void stopProfile(U32 count, U32 mode)
	{
		if (sProfileEnabled && sCurBoundShaderPtr)
		{
			sCurBoundShaderPtr->readProfileQuery(count, mode);
		}
	}

	void clearStats();
	void dumpStats();
	void placeProfileQuery();
	void readProfileQuery(U32 count, U32 mode);

	void setup(const char* name, S32 level, const char* vertex_shader,
			   const char* fragment_shader);

	void unload();

	typedef std::vector<LLStaticHashedString> hash_vector_t;
	bool createShader(hash_vector_t* attributes = NULL,
					  hash_vector_t* uniforms = NULL,
					  U32 varying_count = 0, const char** varyings = NULL);

	bool attachVertexObject(const char* object);
	bool attachFragmentObject(const char* object);
	void attachObject(GLhandleARB object);
	void attachObjects(GLhandleARB* objects = NULL, S32 count = 0);
	bool mapAttributes(const hash_vector_t* attributes);
	bool mapUniforms(const hash_vector_t* uniforms);
	void mapUniform(S32 index, const hash_vector_t* uniforms);

	void uniform1i(U32 index, S32 i);
	void uniform1f(U32 index, F32 v);
	void uniform2f(U32 index, F32 x, F32 y);
	void uniform3f(U32 index, F32 x, F32 y, F32 z);
	void uniform4f(U32 index, F32 x, F32 y, F32 z, F32 w);
	void uniform1iv(U32 index, U32 count, const S32* i);
	void uniform1fv(U32 index, U32 count, const F32* v);
	void uniform2fv(U32 index, U32 count, const F32* v);
	void uniform3fv(U32 index, U32 count, const F32* v);
	void uniform4fv(U32 index, U32 count, const F32* v);

	void uniformMatrix2fv(U32 index, U32 count, GLboolean transpose,
						  const F32* v);
	void uniformMatrix3fv(U32 index, U32 count, GLboolean transpose,
						  const F32* v);
	void uniformMatrix3x4fv(U32 index, U32 count, GLboolean transpose,
							const F32* v);
	void uniformMatrix4fv(U32 index, U32 count, GLboolean transpose,
						  const F32* v);

	void uniform1i(const LLStaticHashedString& uniform, S32 i);
	void uniform2i(const LLStaticHashedString& uniform, S32 i, S32 j);
	void uniform1f(const LLStaticHashedString& uniform, F32 v);
	void uniform2f(const LLStaticHashedString& uniform, F32 x, F32 y);
	void uniform3f(const LLStaticHashedString& uniform, F32 x, F32 y,
				   F32 z);
	void uniform4f(const LLStaticHashedString& uniform, F32 x, F32 y, F32 z,
				    F32 w);
	void uniform1iv(const LLStaticHashedString& uniform, U32 count,
					const S32* i);
	void uniform1fv(const LLStaticHashedString& uniform, U32 count,
					const F32* v);
	void uniform2fv(const LLStaticHashedString& uniform, U32 count,
					const F32* v);
	void uniform3fv(const LLStaticHashedString& uniform, U32 count,
					const F32* v);
	void uniform4fv(const LLStaticHashedString& uniform, U32 count,
					const F32* v);

	void uniformMatrix4fv(const LLStaticHashedString& uniform, U32 count,
						  GLboolean transpose, const F32* v);

	void setMinimumAlpha(F32 minimum);

	void vertexAttrib4f(U32 index, F32 x, F32 y, F32 z, F32 w);
	void vertexAttrib4fv(U32 index, F32* v);

	S32 getUniformLocation(const LLStaticHashedString& uniform);
	S32 getUniformLocation(U32 index);

	S32 getAttribLocation(U32 attrib);
	S32 mapUniformTextureChannel(S32 location, U32 type);

	void addConstant(EShaderConsts shader_const);
	void addPermutation(const std::string&, const std::string& value);
	void removePermutation(const std::string& name);

	// Enable/disable texture channel for specified uniform. If given texture
	// uniform is active in the shader, the corresponding channel will be
	// active upon return. Returns channel texture is enabled in from [0-MAX).
	S32 enableTexture(S32 uniform,
					  LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE,
					  LLTexUnit::eTextureColorSpace s = LLTexUnit::TCS_LINEAR);
	S32 disableTexture(S32 uniform,
					   LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE,
					   LLTexUnit::eTextureColorSpace s = LLTexUnit::TCS_LINEAR);

	// bindTexture returns the texture unit we've bound the texture to. You can
	// reuse the return value to unbind a texture when required.
	S32 bindTexture(const std::string& uniform, LLGLTexture* texture,
					LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE,
					LLTexUnit::eTextureColorSpace sps = LLTexUnit::TCS_LINEAR);
	S32 bindTexture(S32 uniform, LLGLTexture* texture,
					LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE,
					LLTexUnit::eTextureColorSpace spc = LLTexUnit::TCS_LINEAR);
	S32 unbindTexture(const std::string& uniform,
					  LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE);
	S32 unbindTexture(S32 uniform,
					  LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE);

	void bind();
	void unbind();

	// Unbinds any previously bound shader by explicitly binding no shader.
	static void bindNoShader();

private:
	void unloadInternal();

public:
	U32								mMatHash[LLRender::NUM_MATRIX_MODES];
	U32								mLightHash;

	GLhandleARB						mProgramObject;

	// Lookup table of attribute enum to attribute channel
	std::vector<S32>				mAttribute;

	// Mask of which reserved attributes are set (lines up with
	// LLVertexBuffer::getTypeMask())
	U32 							mAttributeMask;

	S32								mTotalUniformSize;
	S32								mActiveTextureChannels;
	S32								mShaderLevel;
	S32								mShaderGroup;
	LLShaderFeatures				mFeatures;

	std::string 					mName;

	// Lookup map of uniform name to uniform location
	LLStaticStringTable<S32>		mUniformMap;

	// Lookup table of uniform enum to uniform location
	std::vector<S32>				mUniform;

	// Lookup map of uniform location to uniform name
	typedef fast_hmap<S32, std::string> uniforms_map_t;
	uniforms_map_t					mUniformNameMap;

	// Lookup map of uniform location to last known value
	typedef fast_hmap<S32, LLVector4> uniform_value_map_t;
	uniform_value_map_t				mValue;

	typedef std::vector<std::pair<std::string, U32> > files_map_t;
	files_map_t						mShaderFiles;

	typedef fast_hmap<std::string, std::string> defines_map_t;
	defines_map_t					mDefines;

	// Statistics for profiling shader performance
	U32								mTimerQuery;
	U32								mSamplesQuery;
	U64								mTimeElapsed;
	U32								mTrianglesDrawn;
	U64								mSamplesDrawn;
	U32								mDrawCalls;

	std::vector<S32>				mTexture;
	std::vector<U32>				mTextureMagFilter;
	std::vector<U32>				mTextureMinFilter;
	bool							mTextureStateFetched;

	bool							mUniformsDirty;

	static std::set<LLGLSLShader*>	sInstances;
	static LLGLSLShader*			sCurBoundShaderPtr;
	static S32						sIndexedTextureChannels;
	static GLhandleARB				sCurBoundShader;
	static bool						sProfileEnabled;

	// Statistcis for profiling shader performance
	static U64						sTotalTimeElapsed;
	static U32						sTotalTrianglesDrawn;
	static U64						sTotalSamplesDrawn;
	static U32						sTotalDrawCalls;
};

class LLShaderUniforms
{
public:
	LL_INLINE LLShaderUniforms()
	:	mActive(false)
	{
	}

	LL_INLINE void clear()
	{
		mIntegers.resize(0);
		mFloats.resize(0);
		mVectors.resize(0);
		mVector3s.resize(0);
		mActive = false;
	}

	LL_INLINE void uniform1i(S32 index, S32 value)
	{
		mIntegers.push_back({ index, value });
		mActive = true;
	}

	LL_INLINE void uniform1f(S32 index, F32 value)
	{
		mFloats.push_back({ index, value });
		mActive = true;
	}

	LL_INLINE void uniform4fv(S32 index, const LLVector4& value)
	{
		mVectors.push_back({ index, value });
		mActive = true;
	}

	LL_INLINE void uniform4fv(S32 index, const F32* value)
	{
		mVectors.push_back({ index, LLVector4(value) });
		mActive = true;
	}

	LL_INLINE void uniform3fv(S32 index, const LLVector3& value)
	{
		mVector3s.push_back({ index, value });
		mActive = true;
	}

	void apply(LLGLSLShader* shader);

private:
	template<typename T>
	struct UniformSetting
	{
		S32	mUniform;
		T	mValue;
	};

	typedef UniformSetting<S32> IntSetting;
	typedef UniformSetting<F32> FloatSetting;
	typedef UniformSetting<LLVector4> VectorSetting;
	typedef UniformSetting<LLVector3> Vector3Setting;

	std::vector<IntSetting>		mIntegers;
	std::vector<FloatSetting>	mFloats;
	std::vector<VectorSetting>	mVectors;
	std::vector<Vector3Setting>	mVector3s;

	bool						mActive;
};

// UI shader
extern LLGLSLShader gUIProgram;

// Output vec4(color.rgb, color.a * tex0[tc0].a)
extern LLGLSLShader	gSolidColorProgram;

// Alpha mask shader (declared here so llappearance can access properly)
extern LLGLSLShader gAlphaMaskProgram;

// Set to true when using the new extended environment shaders (and renderer)
extern bool gUseNewShaders;

#endif

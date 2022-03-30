/**
 * @file llrender.h
 * @brief LLRender definition
 *
 * This class acts as a wrapper for OpenGL calls.
 * The goal of this class is to minimize the number of api calls due to legacy
 * rendering code, to define an interface for a multiple rendering API
 * abstraction of the UI rendering, and to abstract out direct rendering calls
 * in a way that is cleaner and easier to maintain.
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

#ifndef LL_LLGLRENDER_H
#define LL_LLGLRENDER_H

#include "llcolor4u.h"
#include "llglheaders.h"
#include "llmatrix4a.h"
#include "llpointer.h"
#include "llpreprocessor.h"
#include "llrect.h"
#include "llstrider.h"
#include "llvector2.h"
#include "llvector3.h"
#include "llvector4.h"

class LLCubeMap;
class LLImageGL;
class LLMatrix4a;
class LLRenderTarget;
class LLGLTexture;
class LLVertexBuffer;

#define LL_MATRIX_STACK_DEPTH 32
// Not enabled for now (EE deferred shaders not ready).
#define LL_USE_SRGB_DECODE 0

constexpr U32 LL_NUM_TEXTURE_LAYERS = 32;
constexpr U32 LL_NUM_LIGHT_UNITS = 8;

class LLTexUnit
{
	friend class LLRender;

protected:
	LOG_CLASS(LLTexUnit);

public:
	typedef enum
	{
		TT_TEXTURE = 0,			// Standard 2D Texture
		TT_RECT_TEXTURE,		// Non power of 2 texture
		TT_CUBE_MAP,			// 6-sided cube map texture
		TT_MULTISAMPLE_TEXTURE,	// See GL_ARB_texture_multisample
		TT_TEXTURE_3D,			// Standard 3D Texture
		TT_NONE 				// No texture type is currently enabled
	} eTextureType;

	typedef enum
	{
		TAM_WRAP = 0,		// Standard 2D Texture
		TAM_MIRROR,			// Non power of 2 texture
		TAM_CLAMP 			// No texture type is currently enabled
	} eTextureAddressMode;

	typedef enum
	{	// Note: If mipmapping or anisotropic are not enabled or supported it
		// should fall back gracefully
		TFO_POINT = 0,		// Equal to: min=point, mag=point, mip=none.
		TFO_BILINEAR,		// Equal to: min=linear, mag=linear, mip=point.
		TFO_TRILINEAR,		// Equal to: min=linear, mag=linear, mip=linear.
		TFO_ANISOTROPIC		// Equal to: min=anisotropic, max=anisotropic, mip=linear.
	} eTextureFilterOptions;

	typedef enum
	{
		TBS_PREV_COLOR = 0,			// Color from the previous texture stage
		TBS_PREV_ALPHA,
		TBS_ONE_MINUS_PREV_COLOR,
		TBS_ONE_MINUS_PREV_ALPHA,
		TBS_TEX_COLOR,				// Color from the texture bound to this stage
		TBS_TEX_ALPHA,
		TBS_ONE_MINUS_TEX_COLOR,
		TBS_ONE_MINUS_TEX_ALPHA,
		TBS_VERT_COLOR,				// The vertex color currently set
		TBS_VERT_ALPHA,
		TBS_ONE_MINUS_VERT_COLOR,
		TBS_ONE_MINUS_VERT_ALPHA,
		TBS_CONST_COLOR,			// The constant color value currently set
		TBS_CONST_ALPHA,
		TBS_ONE_MINUS_CONST_COLOR,
		TBS_ONE_MINUS_CONST_ALPHA
	} eTextureBlendSrc;

	typedef enum
	{
		TCS_LINEAR = 0,
		TCS_SRGB
	} eTextureColorSpace;

	LLTexUnit(S32 index);

	// Refreshes renderer state of the texture unit to the cached values.
	// Needed when the render context has changed and invalidated the
	// current state
	void refreshState();

	// returns the index of this texture unit
	LL_INLINE S32 getIndex() const					{ return mIndex; }

	// Sets this tex unit to be the currently active one
	void activate();

	// Enables this texture unit for the given texture type
	// (automatically disables any previously enabled texture type)
	void enable(eTextureType type);

	// Disables the current texture unit
	void disable();

	// Binds the LLImageGL to this texture unit (automatically enables the
	// unit for the LLImageGL's texture type)
	bool bind(LLImageGL* texture, bool force_bind = false);
	bool bind(LLGLTexture* texture, bool force_bind = false);

	// Binds a cubemap to this texture unit (automatically enables the
	// texture unit for cubemaps)
	bool bind(LLCubeMap* cube_map);

	// Binds a render target to this texture unit (automatically enables the
	// texture unit for the RT's texture type)
	bool bind(LLRenderTarget* render_target, bool bind_depth = false);

	// Manually binds a texture to the texture unit (automatically enables the
	// tex unit for the given texture type)
	bool bindManual(eTextureType type, U32 texture, bool has_mips = false);

	// Unbinds the currently bound texture of the given type
	// (only if there's a texture of the given type currently bound)
	void unbind(eTextureType type);

	// Sets the addressing mode used to sample the texture.
	// Warning: this stays set for the bound texture forever; make sure you
	// want to permanently change the address mode for the bound texture.
	void setTextureAddressMode(eTextureAddressMode mode);

	// Sets the filtering options used to sample the texture.
	// Warning: this stays set for the bound texture forever; make sure you
	// want to permanently change the filtering for the bound texture.
	void setTextureFilteringOption(LLTexUnit::eTextureFilterOptions option);

	LL_INLINE U32 getCurrTexture()					{ return mCurrTexture; }

	LL_INLINE eTextureType getCurrType()			{ return mCurrTexType; }

	LL_INLINE void setHasMipMaps(bool has_mips)		{ mHasMipMaps = has_mips; }

	void setTextureColorSpace(eTextureColorSpace space);
	LL_INLINE eTextureColorSpace getCurColorSpace()	{ return mTexColorSpace; }

	static U32 getInternalType(eTextureType type);

protected:
	void debugTextureUnit();
	void setColorScale(S32 scale);
	void setAlphaScale(S32 scale);
	S32 getTextureSource(eTextureBlendSrc src);
	S32 getTextureSourceType(eTextureBlendSrc src, bool is_alpha = false);

public:
	static U32			sWhiteTexture;

protected:
	const S32			mIndex;
	U32					mCurrTexture;
	eTextureType		mCurrTexType;
	eTextureColorSpace	mTexColorSpace;
	S32					mCurrColorScale;
	S32					mCurrAlphaScale;
	bool				mHasMipMaps;
};

class LLLightState
{
	friend class LLRender;

public:
	LLLightState(S32 index);

	void enable();
	void disable();
	void setDiffuse(const LLColor4& diffuse);
	void setDiffuseB(const LLColor4& diffuse);
	void setAmbient(const LLColor4& ambient);
	void setSpecular(const LLColor4& specular);
	void setPosition(const LLVector4& position);
	void setConstantAttenuation(F32 atten);
	void setLinearAttenuation(F32 atten);
	void setQuadraticAttenuation(F32 atten);
	void setSpotExponent(F32 exponent);
	void setSpotCutoff(F32 cutoff);
	void setSpotDirection(const LLVector3& direction);
	void setSunPrimary(bool b);

protected:
	S32			mIndex;

	LLColor4	mDiffuse;
	LLColor4	mDiffuseB;
	LLColor4	mAmbient;
	LLColor4	mSpecular;
	LLVector4	mPosition;
	LLVector3	mSpotDirection;

	F32			mConstantAtten;
	F32			mLinearAtten;
	F32			mQuadraticAtten;

	F32			mSpotExponent;
	F32			mSpotCutoff;

	bool		mSunIsPrimary;

	bool		mEnabled;
};

class LLRender
{
	friend class LLLightState;
	friend class LLTexUnit;

protected:
	LOG_CLASS(LLRender);

public:
	enum eTexIndex
	{
		DIFFUSE_MAP = 0,
		ALTERNATE_DIFFUSE_MAP = 1,
		NORMAL_MAP = 1,
		SPECULAR_MAP = 2,
		NUM_TEXTURE_CHANNELS = 3,
	};

	enum eVolumeTexIndex
	{
		LIGHT_TEX = 0,
		SCULPT_TEX,
		NUM_VOLUME_TEXTURE_CHANNELS,
	};

	typedef enum
	{
		TRIANGLES = 0,
		TRIANGLE_STRIP,
		TRIANGLE_FAN,
		POINTS,
		LINES,
		LINE_STRIP,
		LINE_LOOP,
		NUM_MODES
	} eGeomModes;

	typedef enum
	{
		CF_NEVER = 0,
		CF_ALWAYS,
		CF_LESS,
		CF_LESS_EQUAL,
		CF_EQUAL,
		CF_NOT_EQUAL,
		CF_GREATER_EQUAL,
		CF_GREATER,
		CF_DEFAULT
	}  eCompareFunc;

	typedef enum
	{
		BT_ALPHA = 0,
		BT_ADD,
		BT_ADD_WITH_ALPHA,	// Additive blend modulated by the fragment's alpha.
		BT_MULT,
		BT_MULT_ALPHA,
		BT_MULT_X2,
		BT_REPLACE
	} eBlendType;

	typedef enum
	{
		BF_ONE = 0,
		BF_ZERO,
		BF_DEST_COLOR,
		BF_SOURCE_COLOR,
		BF_ONE_MINUS_DEST_COLOR,
		BF_ONE_MINUS_SOURCE_COLOR,
		BF_DEST_ALPHA,
		BF_SOURCE_ALPHA,
		BF_ONE_MINUS_DEST_ALPHA,
		BF_ONE_MINUS_SOURCE_ALPHA,

		BF_UNDEF
	} eBlendFactor;

	typedef enum
	{
		MM_MODELVIEW = 0,
		MM_PROJECTION,
		MM_TEXTURE0,
		MM_TEXTURE1,
		MM_TEXTURE2,
		MM_TEXTURE3,
		NUM_MATRIX_MODES,
		MM_TEXTURE
	} eMatrixMode;

	LLRender();
	~LLRender();

	void init();
	void shutdown();

	// Refreshes renderer state to the cached values. Needed when the render
	// context has changed and invalidated the current state.
	void refreshState();

	void translatef(F32 x, F32 y, F32 z);

	void scalef(F32 x, F32 y, F32 z);

	void rotatef(F32 a, F32 x, F32 y, F32 z);
	// Requires the generation of a transform matrix involving sine/cosine. If
	// rotating by a constant value, use gl_gen_rot(), store the result in a
	// static variable, and pass it to rotatef().
	void rotatef(const LLMatrix4a& rot);

	void ortho(F32 left, F32 right, F32 bottom, F32 top, F32 znear, F32 zfar);

	bool projectf(const LLVector3& object, const LLMatrix4a& modelview,
				  const LLMatrix4a& projection, const LLRect& viewport,
				  LLVector3& window_coord);
	bool unprojectf(const LLVector3& window_coord, const LLMatrix4a& modelview,
					const LLMatrix4a& projection, const LLRect& viewport,
					LLVector3& object);

	void pushMatrix();
	void popMatrix();
	void loadMatrix(const F32* m);
	void loadMatrix(const LLMatrix4a& m);
	void loadIdentity();
	void multMatrix(const F32* m);
	void multMatrix(const LLMatrix4a& m);
	void matrixMode(U32 mode);
	U32 getMatrixMode();

	const LLMatrix4a& getModelviewMatrix();

	void syncMatrices();
	void syncLightState();

	void translateUI(F32 x, F32 y, F32 z);
	void scaleUI(F32 x, F32 y, F32 z);
	void pushUIMatrix();
	void popUIMatrix();
	void loadUIIdentity();
	LLVector3 getUITranslation();
	LLVector3 getUIScale();

	void flush();

	void begin(U32 mode);
	void end(bool force_flush = false);

	void vertex3f(F32 x, F32 y, F32 z);

	LL_INLINE void vertex2i(S32 x, S32 y)
	{
		vertex3f((F32)x, (F32)y, 0.f);
	}

	LL_INLINE void vertex2f(F32 x, F32 y)
	{
		vertex3f(x, y, 0.f);
	}

	LL_INLINE void vertex2fv(const F32* v)
	{
		vertex3f(v[0], v[1], 0.f);
	}

	LL_INLINE void vertex3fv(const F32* v)
	{
		vertex3f(v[0], v[1], v[2]);
	}

	LL_INLINE void texCoord2i(S32 x, S32 y)
	{
		mTexcoordsp[mCount].set((F32)x, (F32)y);
	}

	LL_INLINE void texCoord2f(F32 x, F32 y)
	{
		mTexcoordsp[mCount].set(x, y);
	}

	LL_INLINE void texCoord2fv(const F32* tc)
	{
		texCoord2f(tc[0], tc[1]);
	}

	void color4ub(U8 r, U8 g, U8 b, U8 a);

	LL_INLINE void color4ubv(const U8* c)
	{
		color4ub(c[0], c[1], c[2], c[3]);
	}

	LL_INLINE void color4f(F32 r, F32 g, F32 b, F32 a)
	{
		color4ub((U8)(llclamp(r, 0.f, 1.f) * 255),
				 (U8)(llclamp(g, 0.f, 1.f) * 255),
				 (U8)(llclamp(b, 0.f, 1.f) * 255),
				 (U8)(llclamp(a, 0.f, 1.f) * 255));
	}

	LL_INLINE void color4fv(const F32* c)
	{
		color4f(c[0], c[1], c[2], c[3]);
	}

	LL_INLINE void color3f(F32 r, F32 g, F32 b)
	{
		color4f(r, g, b, 1);
	}

	LL_INLINE void color3fv(const F32* c)
	{
		color4f(c[0], c[1], c[2], 1);
	}

	void diffuseColor3f(F32 r, F32 g, F32 b);
	void diffuseColor3fv(const F32* c);
	void diffuseColor4f(F32 r, F32 g, F32 b, F32 a);
	void diffuseColor4fv(const F32* c);
	void diffuseColor4ubv(const U8* c);
	void diffuseColor4ub(U8 r, U8 g, U8 b, U8 a);

	void lineWidth(F32 width);

	void vertexBatchPreTransformed(LLVector3* verts, S32 vert_count);
	void vertexBatchPreTransformed(LLVector3* verts, LLVector2* uvs,
								   S32 vert_count);
	void vertexBatchPreTransformed(LLVector3* verts, LLVector2* uvs,
								   LLColor4U*, S32 vert_count);

	void setColorMask(bool write_color, bool write_alpha);
	void setColorMask(bool write_red, bool write_green, bool write_blue,
					  bool write_alpha);
	void setSceneBlendType(eBlendType type);

	// Applies blend func to both color and alpha
	void blendFunc(eBlendFactor sfactor, eBlendFactor dfactor);
	// Applies separate blend functions to color and alpha
	void blendFunc(eBlendFactor color_sfactor, eBlendFactor color_dfactor,
				   eBlendFactor alpha_sfactor, eBlendFactor alpha_dfactor);

	LLLightState* getLight(U32 index);
	void setAmbientLightColor(const LLColor4& color);

	LLTexUnit* getTexUnit(U32 index);

	LL_INLINE U32 getCurrentTexUnitIndex() const	{ return mCurrTextureUnitIndex; }

	bool verifyTexUnitActive(U32 unit_to_verify);

	void debugTexUnits();

	void clearErrors();

public:
	static U32					sUICalls;
	static U32					sUIVerts;
	static bool					sGLCoreProfile;

private:
	alignas(16) LLMatrix4a		mMatrix[NUM_MATRIX_MODES][LL_MATRIX_STACK_DEPTH];
	U32							mMatIdx[NUM_MATRIX_MODES];
	U32							mMatHash[NUM_MATRIX_MODES];
	U32							mCurMatHash[NUM_MATRIX_MODES];
	U32							mMatrixMode;
	U32							mLightHash;
	LLColor4					mAmbientLightColor;

	F32							mMaxAnisotropy;

	U32							mCount;
	U32							mMode;
	U32							mCurrTextureUnitIndex;

	eBlendFactor				mCurrBlendColorSFactor;
	eBlendFactor				mCurrBlendColorDFactor;
	eBlendFactor				mCurrBlendAlphaSFactor;
	eBlendFactor				mCurrBlendAlphaDFactor;

	LLPointer<LLVertexBuffer>	mBuffer;
	LLStrider<LLVector3>		mVerticesp;
	LLStrider<LLVector2>		mTexcoordsp;
	LLStrider<LLColor4U>		mColorsp;
	LLTexUnit*					mDummyTexUnit;

	std::vector<LLTexUnit*>		mTexUnits;
	std::vector<LLLightState*>	mLightState;

	std::vector<LLVector3>		mUIOffset;
	std::vector<LLVector3>		mUIScale;

	bool						mCurrColorMask[4];
	bool						mDirty;

	static U32					sDummyVAO;
};

extern thread_local LLRender gGL;
extern LLMatrix4a gGLModelView;
extern LLMatrix4a gGLLastModelView;
extern LLMatrix4a gGLProjection;
extern LLMatrix4a gGLLastProjection;
extern S32 gGLViewport[4];

// This rotation matrix moves the default OpenGL reference frame
// (-Z at, Y up) to Cory's favorite reference frame (X at, Z up)
static const F32 OGL_TO_CFR_ROTATION[16] =
{
	 0.f,  0.f, -1.f,  0.f,	// -Z becomes X
	-1.f,  0.f,  0.f,  0.f,	// -X becomes Y
	 0.f,  1.f,  0.f,  0.f,	//  Y becomes Z
	 0.f,  0.f,  0.f,  1.f
};
// Same thing, as an LLMatrix4a...
static const LLMatrix4a OGL_TO_CFR_ROT4A(
	LLVector4a( 0.f,  0.f, -1.f,  0.f),	// -Z becomes X
	LLVector4a(-1.f,  0.f,  0.f,  0.f),	// -X becomes Y
	LLVector4a( 0.f,  1.f,  0.f,  0.f),	//  Y becomes Z
	LLVector4a( 0.f,  0.f,  0.f,  1.f)
);

// Functions commonly used for OpenGL matrices transformations

LLMatrix4a gl_gen_rot(F32 a, const LLVector4a& axis);

LL_INLINE LLMatrix4a gl_gen_rot(F32 a, F32 x, F32 y, F32 z)
{
	return gl_gen_rot(a, LLVector4a(x, y, z));
}

LLMatrix4a gl_ortho(F32 left, F32 right, F32 bottom, F32 top, F32 znear,
					   F32 zfar);
LLMatrix4a gl_perspective(F32 fovy, F32 aspect, F32 znear, F32 zfar);

#endif

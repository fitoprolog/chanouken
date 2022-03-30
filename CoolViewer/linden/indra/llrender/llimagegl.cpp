/**
 * @file llimagegl.cpp
 * @brief Generic GL image handler
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

#include "llimagegl.h"

#include "llglslshader.h"
#include "llimage.h"
#include "llmath.h"
#include "hbtracy.h"

#define FIX_MASKS 1

// Which power of 2 is i ?  Assumes i is a power of 2 > 0
U32 wpo2(U32 i);	// defined in llvertexbuffer.cpp

// Static members
U32 LLImageGL::sUniqueCount = 0;
U32 LLImageGL::sBindCount = 0;
S64 LLImageGL::sGlobalTextureMemoryInBytes = 0;
S64 LLImageGL::sBoundTextureMemoryInBytes = 0;
S64 LLImageGL::sCurBoundTextureMemory = 0;
S32 LLImageGL::sCount = 0;
bool LLImageGL::sGlobalUseAnisotropic = false;
bool LLImageGL::sPreserveDiscard = false;
bool LLImageGL::sCompressTextures = false;
U32 LLImageGL::sCompressThreshold = 262144U;
F32 LLImageGL::sLastFrameTime = 0.f;
LLImageGL* LLImageGL::sDefaultGLImagep = NULL;
LLImageGL::glimage_list_t LLImageGL::sImageList;

constexpr S8 INVALID_OFFSET = -99;

// Helper function used to check the size of a texture image. So dim should be
// a positive number
static bool check_power_of_two(S32 dim)
{
	if (dim < 0)
	{
		return false;
	}
	if (!dim)	// 0 is a power-of-two number
	{
		return true;
	}
	return !(dim & (dim - 1));
}

//*****************************************************************************
//End for texture auditing use only
//*****************************************************************************

bool is_little_endian()
{
	S32 a = 0x12345678;
    U8* c = (U8*)(&a);
	return *c == 0x78;
}

//static
S32 LLImageGL::dataFormatBits(S32 dataformat)
{
	switch (dataformat)
	{
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:			return 4;
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:	return 4;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:			return 8;
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:	return 8;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:			return 8;
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:	return 8;
		case GL_LUMINANCE:								return 8;
		case GL_ALPHA:									return 8;
		case GL_COLOR_INDEX:							return 8;
		case GL_LUMINANCE_ALPHA:						return 16;
		case GL_RED:									return 8;
		case GL_RG:										return 16;
		case GL_RGB:									return 24;
		case GL_SRGB:									return 24;
		case GL_RGB8:									return 24;
		case GL_RGBA:									return 32;
		case GL_SRGB_ALPHA:								return 32;
		// Used for QuickTime media textures on the Mac:
		case GL_BGRA:									return 32;

		default:
			llerrs << "Unknown format: " << dataformat << llendl;
			return 0;
	}
}

//static
S32 LLImageGL::dataFormatBytes(S32 dataformat, S32 width, S32 height)
{
	if (dataformat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
		dataformat == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT ||
		dataformat == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT ||
		dataformat == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT ||
		dataformat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ||
		dataformat == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
	{
		if (width < 4)
		{
			width = 4;
		}
		if (height < 4)
		{
			height = 4;
		}
	}
	S32 bytes = (width * height * dataFormatBits(dataformat) + 7) >> 3;
	return (bytes + 3) &~ 3;	// aligned
}

//static
S32 LLImageGL::dataFormatComponents(S32 dataformat)
{
	switch (dataformat)
	{
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:			return 3;
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:	return 3;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:			return 4;
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:	return 4;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:			return 4;
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:	return 4;
		case GL_LUMINANCE:								return 1;
		case GL_ALPHA:									return 1;
		case GL_COLOR_INDEX:							return 1;
		case GL_LUMINANCE_ALPHA:						return 2;
		case GL_RED:									return 1;
		case GL_RG:										return 2;
		case GL_RGB:									return 3;
		case GL_SRGB:									return 3;
		case GL_RGBA:									return 4;
		case GL_SRGB_ALPHA:								return 4;
		// Used for QuickTime media textures on the Mac
		case GL_BGRA:									return 4;

		default:
			llerrs << "Unknown format: " << dataformat << llendl;
			return 0;
	}
}

//static
void LLImageGL::updateStats(F32 current_time)
{
	sLastFrameTime = current_time;
	sBoundTextureMemoryInBytes = sCurBoundTextureMemory;
	sCurBoundTextureMemory = 0;
}

//static
S32 LLImageGL::updateBoundTexMem(S32 mem, S32 ncomponents, S32 category)
{
	LLImageGL::sCurBoundTextureMemory += mem;
	return LLImageGL::sCurBoundTextureMemory;
}

//static
void LLImageGL::destroyGL(bool save_state)
{
	for (S32 stage = 0; stage < gGLManager.mNumTextureUnits; ++stage)
	{
		gGL.getTexUnit(stage)->unbind(LLTexUnit::TT_TEXTURE);
	}

	for (glimage_list_t::iterator iter = sImageList.begin(),
								  end = sImageList.end();
		 iter != end; ++iter)
	{
		LLImageGL* glimage = *iter;
		if (glimage->mTexName)
		{
			if (save_state && glimage->isGLTextureCreated() &&
				glimage->mComponents)
			{
				glimage->mSaveData = new LLImageRaw;
				if (!glimage->readBackRaw(glimage->mCurrentDiscardLevel,
												 			  // necessary
										  glimage->mSaveData, false))
				{
					glimage->mSaveData = NULL;
				}
			}

			glimage->destroyGLTexture();
			stop_glerror();
		}
	}
}

//static
void LLImageGL::restoreGL()
{
	for (glimage_list_t::iterator iter = sImageList.begin(),
								  end = sImageList.end();
		 iter != end; ++iter)
	{
		LLImageGL* glimage = *iter;
		if (glimage->getTexName())
		{
			llwarns << "Tex name is not 0." << llendl;
		}
		if (glimage->mSaveData.notNull())
		{
			if (glimage->getComponents() &&
				glimage->mSaveData->getComponents())
			{
				glimage->createGLTexture(glimage->mCurrentDiscardLevel,
										 glimage->mSaveData, 0, true,
										 glimage->getCategory());
				stop_glerror();
			}
		}

		glimage->mSaveData = NULL; // deletes data
	}
}

//static
void LLImageGL::dirtyTexOptions()
{
	for (glimage_list_t::iterator iter = sImageList.begin(),
								  end = sImageList.end();
		 iter != end; ++iter)
	{
		LLImageGL* glimage = *iter;
		glimage->mTexOptionsDirty = true;
	}
}

//----------------------------------------------------------------------------
// For server side use only.

//static
bool LLImageGL::create(LLPointer<LLImageGL>& dest, bool usemipmaps)
{
	dest = new LLImageGL(usemipmaps);
	return true;
}

// For server side use only.
bool LLImageGL::create(LLPointer<LLImageGL>& dest, U32 width, U32 height,
					   U8 components, bool usemipmaps)
{
	dest = new LLImageGL(width, height, components, usemipmaps);
	return true;
}

// For server side use only.
bool LLImageGL::create(LLPointer<LLImageGL>& dest, const LLImageRaw* imageraw,
					   bool usemipmaps)
{
	dest = new LLImageGL(imageraw, usemipmaps);
	return true;
}

//----------------------------------------------------------------------------

LLImageGL::LLImageGL(bool usemipmaps)
:	mSaveData(NULL),
	mExternalTexture(false)
{
	init(usemipmaps);
	setSize(0, 0, 0);
	sImageList.insert(this);
	++sCount;
}

LLImageGL::LLImageGL(U32 width, U32 height, U8 components, bool usemipmaps)
:	mSaveData(NULL),
	mExternalTexture(false)
{
	llassert(components <= 4);
	init(usemipmaps);
	setSize(width, height, components);
	sImageList.insert(this);
	++sCount;
}

LLImageGL::LLImageGL(const LLImageRaw* imageraw, bool usemipmaps)
:	mSaveData(NULL),
	mExternalTexture(false)
{
	init(usemipmaps);
	setSize(0, 0, 0);
	sImageList.insert(this);
	++sCount;

	createGLTexture(0, imageraw);
}

LLImageGL::LLImageGL(U32 tex_name, U32 components, U32 target,
					 S32 format_internal, U32 format_primary,
					 U32 format_type, LLTexUnit::eTextureAddressMode mode)
:	mSaveData(NULL),
	mExternalTexture(true)
{
	init(false);
	// Must be done after init() to override its defaults...
	mTexName = tex_name;
	mComponents = components;
	mTarget = target;
	mFormatInternal = format_internal;
	mFormatPrimary = format_primary;
	mFormatType = format_type;
	mAddressMode = mode;
}

LLImageGL::~LLImageGL()
{
	if (!mExternalTexture)
	{
		sImageList.erase(this);
		--sCount;
		cleanup();
	}
}

void LLImageGL::init(bool usemipmaps)
{
	mTextureMemory = 0;
	mLastBindTime = 0.f;

	mPickMask = NULL;
	mPickMaskWidth = 0;
	mPickMaskHeight = 0;
	mUseMipMaps = usemipmaps;
	mHasExplicitFormat = false;
	mAutoGenMips = false;

	mIsMask = false;
#if FIX_MASKS
	mNeedsAlphaAndPickMask = false;
	mAlphaOffset = INVALID_OFFSET;
#else
	mNeedsAlphaAndPickMask = true;
	mAlphaOffset = 0;
#endif
	mAlphaStride = 0;

	mGLTextureCreated = false;
	mTexName = 0;
	mWidth = 0;
	mHeight = 0;
	mCurrentDiscardLevel = -1;

	mAllowCompression = true;

	mTarget	= GL_TEXTURE_2D;
	mBindTarget	= LLTexUnit::TT_TEXTURE;
	mHasMipMaps	= false;
	mMipLevels = -1;

	mIsResident	= 0;

	mComponents	= 0;
	mMaxDiscardLevel = MAX_DISCARD_LEVEL;

	mTexOptionsDirty = true;
	mAddressMode = LLTexUnit::TAM_WRAP;
	mFilterOption = LLTexUnit::TFO_ANISOTROPIC;

	mFormatInternal = -1;
	mFormatPrimary = 0;
	mFormatType = GL_UNSIGNED_BYTE;
	mFormatSwapBytes = false;

#ifdef DEBUG_MISS
	mMissed = false;
#endif

	mCategory = -1;
}

void LLImageGL::cleanup()
{
	if (!gGLManager.mIsDisabled)
	{
		destroyGLTexture();
	}
	freePickMask();
	mSaveData = NULL; // deletes data
}

//static
bool LLImageGL::checkSize(S32 width, S32 height)
{
	return check_power_of_two(width) && check_power_of_two(height);
}

bool LLImageGL::setSize(S32 width, S32 height, S32 ncomponents,
						S32 discard_level)
{
	if (width != mWidth || height != mHeight || ncomponents != mComponents)
	{
		// Check if dimensions are a power of two
		if (!checkSize(width, height))
		{
			llwarns <<"Texture has non power of two dimension: " << width
					<< "x" << height  << ". Aborted." << llendl;
			return false;
		}

		if (mTexName)
		{
#if 0
 			llwarns << "Setting size with existing mTexName = " << mTexName
					<< llendl;
#endif
			destroyGLTexture();
		}

		// pickmask validity depends on old image size, delete it
		freePickMask();

		mWidth = width;
		mHeight = height;
		mComponents = ncomponents;
		if (ncomponents > 0)
		{
			mMaxDiscardLevel = 0;
			while (width > 1 && height > 1 &&
				   mMaxDiscardLevel < MAX_DISCARD_LEVEL)
			{
				++mMaxDiscardLevel;
				width >>= 1;
				height >>= 1;
			}

			if (!sPreserveDiscard && discard_level > 0)
			{
				mMaxDiscardLevel = llmax(mMaxDiscardLevel, (S8)discard_level);
			}
		}
		else
		{
			mMaxDiscardLevel = MAX_DISCARD_LEVEL;
		}
	}

	return true;
}

// virtual
void LLImageGL::dump()
{
	llinfos << "mMaxDiscardLevel = " << S32(mMaxDiscardLevel)
			<< " - mLastBindTime = " << mLastBindTime
			<< " - mTarget = " << S32(mTarget)
			<< " - mBindTarget = " << S32(mBindTarget)
			<< " - mUseMipMaps = " << S32(mUseMipMaps)
			<< " - mHasMipMaps = " << S32(mHasMipMaps)
			<< " - mCurrentDiscardLevel = " << S32(mCurrentDiscardLevel)
			<< " - mFormatInternal = " << S32(mFormatInternal)
			<< " - mFormatPrimary = " << S32(mFormatPrimary)
			<< " - mFormatType = " << S32(mFormatType)
			<< " - mFormatSwapBytes = " << S32(mFormatSwapBytes)
			<< " - mHasExplicitFormat = " << S32(mHasExplicitFormat)
#if DEBUG_MISS
			<< " - mMissed = " << mMissed
#endif
			<< " - mTextureMemory = " << mTextureMemory
			<< " - mTexNames = " << mTexName
			<< " - mIsResident = " << S32(mIsResident)
			<< llendl;
}

void LLImageGL::forceUpdateBindStats() const
{
	mLastBindTime = sLastFrameTime;
}

bool LLImageGL::updateBindStats(S32 tex_mem) const
{
	if (mTexName != 0)
	{
#ifdef DEBUG_MISS
		mMissed = ! getIsResident(true);
#endif
		++sBindCount;
		if (mLastBindTime != sLastFrameTime)
		{
			// We have not accounted for this texture yet this frame
			++sUniqueCount;
			updateBoundTexMem(tex_mem, mComponents, mCategory);
			mLastBindTime = sLastFrameTime;

			return true;
		}
	}
	return false;
}

F32 LLImageGL::getTimePassedSinceLastBound()
{
	return sLastFrameTime - mLastBindTime;
}

void LLImageGL::setExplicitFormat(S32 internal_format, U32 primary_format,
								  U32 type_format, bool swap_bytes)
{
	// Notes:
	//  - Must be called before createTexture()
	//  - It is up to the caller to ensure that the format matches the number
	//    of components.
	mHasExplicitFormat = true;
	mFormatInternal = internal_format;
	mFormatPrimary = primary_format;
	mFormatType = type_format == 0 ? GL_UNSIGNED_BYTE : type_format;
	mFormatSwapBytes = swap_bytes;

	calcAlphaChannelOffsetAndStride();
}

void LLImageGL::setImage(const LLImageRaw* imageraw)
{
	llassert(imageraw->getWidth() == getWidth(mCurrentDiscardLevel) &&
			 imageraw->getHeight() == getHeight(mCurrentDiscardLevel) &&
			 imageraw->getComponents() == getComponents());
	const U8* rawdata = imageraw->getData();
	setImage(rawdata, false);
}

bool LLImageGL::setImage(const U8* data_in, bool data_hasmips)
{
	LL_TRACY_TIMER(TRC_SET_IMAGE);
	bool is_compressed =
		mFormatPrimary == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
		mFormatPrimary == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT ||
		mFormatPrimary == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT ||
		mFormatPrimary == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT ||
		mFormatPrimary == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ||
		mFormatPrimary == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;

	if (mUseMipMaps)
	{
		// Set has mip maps to true before binding image so tex parameters get
		// set properly
		gGL.getTexUnit(0)->unbind(mBindTarget);
		mHasMipMaps = true;
		mTexOptionsDirty = true;
		setFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
	}
	else
	{
		mHasMipMaps = false;
	}

	gGL.getTexUnit(0)->bind(this);

	if (mUseMipMaps)
	{
		if (data_hasmips)
		{
			// NOTE: data_in points to largest image; smaller images are stored
			// BEFORE the largest image
			for (S32 d = mCurrentDiscardLevel; d <= mMaxDiscardLevel; ++d)
			{
				S32 w = getWidth(d);
				S32 h = getHeight(d);
				S32 gl_level = d - mCurrentDiscardLevel;

				mMipLevels = llmax(mMipLevels, gl_level);

				if (d > mCurrentDiscardLevel)
				{
					// See above comment
					data_in -= dataFormatBytes(mFormatPrimary, w, h);
				}
				if (is_compressed)
				{
 					S32 tex_size = dataFormatBytes(mFormatPrimary, w, h);
					glCompressedTexImage2DARB(mTarget, gl_level,
											  mFormatPrimary, w, h, 0,
											  tex_size, (GLvoid*)data_in);
					stop_glerror();
				}
				else
				{
					if (mFormatSwapBytes)
					{
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
						stop_glerror();
					}

					setManualImage(mTarget, gl_level, mFormatInternal, w, h,
								   mFormatPrimary, GL_UNSIGNED_BYTE,
								   (GLvoid*)data_in, mAllowCompression);
					if (gl_level == 0)
					{
						analyzeAlpha(data_in, w, h);
					}
					updatePickMask(w, h, data_in);

					if (mFormatSwapBytes)
					{
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
						stop_glerror();
					}
				}
			}
		}
		else if (!is_compressed)
		{
			if (mAutoGenMips)
			{
				if (mFormatSwapBytes)
				{
					glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
					stop_glerror();
				}

				S32 w = getWidth(mCurrentDiscardLevel);
				S32 h = getHeight(mCurrentDiscardLevel);

				mMipLevels = wpo2(llmax(w, h));

				// Use legacy mipmap generation mode only when core profile is
				// not enabled (to avoid deprecation warnings).
				if (!LLRender::sGLCoreProfile)
				{
					glTexParameteri(mTarget, GL_GENERATE_MIPMAP, GL_TRUE);
				}

				setManualImage(mTarget, 0, mFormatInternal, w, h,
							   mFormatPrimary, mFormatType, data_in,
							   mAllowCompression);
				analyzeAlpha(data_in, w, h);
				updatePickMask(w, h, data_in);

				if (mFormatSwapBytes)
				{
					glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
					stop_glerror();
				}

				if (LLRender::sGLCoreProfile)
				{
					glGenerateMipmap(mTarget);
					stop_glerror();
				}
			}
			else
			{
				// Create mips by hand
				// ~4x faster than gluBuild2DMipmaps
				S32 width = getWidth(mCurrentDiscardLevel);
				S32 height = getHeight(mCurrentDiscardLevel);
				S32 nummips = mMaxDiscardLevel - mCurrentDiscardLevel + 1;
				S32 w = width, h = height;
				U8* prev_mip_data = NULL;
				U8* cur_mip_data = NULL;

				mMipLevels = nummips;

				for (S32 m = 0; m < nummips; ++m)
				{
					if (m == 0)
					{
						cur_mip_data = const_cast<U8*>(data_in);
					}
					else
					{
						S32 bytes = w * h * mComponents;
						llassert(prev_mip_data);

						U8* new_data = new (std::nothrow) U8[bytes];
						if (!new_data)
						{
							if (prev_mip_data)
							{
								delete[] prev_mip_data;
							}
							if (cur_mip_data)
							{
								delete[] cur_mip_data;
							}
							mGLTextureCreated = false;
							return false;
						}

						LLImageBase::generateMip(prev_mip_data, new_data, w, h,
												 mComponents);
						cur_mip_data = new_data;
					}

					if (w > 0 && h > 0 && cur_mip_data)
					{
						if (mFormatSwapBytes)
						{
							glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
							stop_glerror();
						}

						setManualImage(mTarget, m, mFormatInternal, w, h,
									   mFormatPrimary, mFormatType,
									   cur_mip_data, mAllowCompression);
						if (m == 0)
						{
							analyzeAlpha(data_in, w, h);
						}
						if (m == 0)
						{
							updatePickMask(w, h, cur_mip_data);
						}

						if (mFormatSwapBytes)
						{
							glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
							stop_glerror();
						}
					}
					else
					{
						llassert(false);
					}

					if (prev_mip_data && prev_mip_data != data_in)
					{
						delete[] prev_mip_data;
					}
					prev_mip_data = cur_mip_data;
					w >>= 1;
					h >>= 1;
				}
				if (prev_mip_data && prev_mip_data != data_in)
				{
					delete[] prev_mip_data;
					prev_mip_data = NULL;
				}
			}
		}
		else
		{
			llerrs << "Compressed Image has mipmaps but data does not (can not auto generate compressed mips)"
				   << llendl;
		}
	}
	else
	{
		mMipLevels = 0;
		S32 w = getWidth();
		S32 h = getHeight();
		if (is_compressed)
		{
			S32 tex_size = dataFormatBytes(mFormatPrimary, w, h);
			glCompressedTexImage2DARB(mTarget, 0, mFormatPrimary, w, h, 0,
									  tex_size, (GLvoid*)data_in);
			stop_glerror();
		}
		else
		{
			if (mFormatSwapBytes)
			{
				glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
				stop_glerror();
			}

			setManualImage(mTarget, 0, mFormatInternal, w, h, mFormatPrimary,
						   mFormatType, (GLvoid*)data_in, mAllowCompression);
			analyzeAlpha(data_in, w, h);

			updatePickMask(w, h, data_in);

			if (mFormatSwapBytes)
			{
				glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
				stop_glerror();
			}
		}
	}
	mGLTextureCreated = true;
	return true;
}

bool LLImageGL::setSubImage(const U8* datap, S32 data_width, S32 data_height,
							S32 x_pos, S32 y_pos, S32 width, S32 height,
							bool force_fast_update)
{
	if (!width || !height)
	{
		return true;
	}
	if (!datap || mTexName == 0)
	{
		// *TODO: Re-add warning ?  Ran into thread locking issues ? - DK
		return false;
	}

	// *HACK: allow the caller to explicitly force the fast path (i.e. using
	// glTexSubImage2D here instead of calling setImage) even when updating the
	// full texture.
	if (!force_fast_update && x_pos == 0 && y_pos == 0 &&
		data_width == width && data_height == height &&
		width == getWidth() && height == getHeight())
	{
		setImage(datap, false);
	}
	else
	{
		if (mUseMipMaps)
		{
			dump();
			llerrs << "setSubImage called with mipmapped image (not supported)"
				   << llendl;
		}
		llassert_always(mCurrentDiscardLevel == 0);
		llassert_always(x_pos >= 0 && y_pos >= 0);

		if (x_pos + width > getWidth() || y_pos + height > getHeight())
		{
			dump();
			llerrs << "Subimage not wholly in target image !"
				   << " x_pos " << x_pos
				   << " y_pos " << y_pos
				   << " width " << width
				   << " height " << height
				   << " getWidth() " << getWidth()
				   << " getHeight() " << getHeight()
				   << llendl;
		}

		if (x_pos + width > data_width || y_pos + height > data_height)
		{
			dump();
			llerrs << "Subimage not wholly in source image !"
				   << " x_pos " << x_pos
				   << " y_pos " << y_pos
				   << " width " << width
				   << " height " << height
				   << " source_width " << data_width
				   << " source_height " << data_height
				   << llendl;
		}

		glPixelStorei(GL_UNPACK_ROW_LENGTH, data_width);
		stop_glerror();

		if (mFormatSwapBytes)
		{
			glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
			stop_glerror();
		}

		datap += (y_pos * data_width + x_pos) * getComponents();
		// Update the GL texture
		LLTexUnit* unit0 = gGL.getTexUnit(0);
		bool res = unit0->bindManual(mBindTarget, mTexName);
		if (!res)
		{
			llerrs << "gGL.getTexUnit(0)->bindManual() failed" << llendl;
		}

		glTexSubImage2D(mTarget, 0, x_pos, y_pos,
						width, height, mFormatPrimary, mFormatType, datap);
		unit0->disable();
		stop_glerror();

		if (mFormatSwapBytes)
		{
			glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
			stop_glerror();
		}

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		stop_glerror();
		mGLTextureCreated = true;
	}
	return true;
}

bool LLImageGL::setSubImage(const LLImageRaw* imageraw, S32 x_pos, S32 y_pos,
							S32 width, S32 height, bool force_fast_update)
{
	return setSubImage(imageraw->getData(), imageraw->getWidth(),
					   imageraw->getHeight(), x_pos, y_pos, width, height,
					   force_fast_update);
}

// Copy sub image from frame buffer
bool LLImageGL::setSubImageFromFrameBuffer(S32 fb_x, S32 fb_y, S32 x_pos,
										   S32 y_pos, S32 width, S32 height)
{
	if (gGL.getTexUnit(0)->bind(this, true))
	{
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, fb_x, fb_y, x_pos, y_pos, width,
							height);
		stop_glerror();
		mGLTextureCreated = true;
		return true;
	}
	return false;
}

//static
void LLImageGL::generateTextures(S32 num_textures, U32* textures)
{
	LL_TRACY_TIMER(TRC_GENERATE_TEXTURES);
	glGenTextures(num_textures, textures);
}

//static
void LLImageGL::deleteTextures(S32 num_textures, U32* textures)
{
	if (gGLManager.mInited)
	{
		glDeleteTextures(num_textures, textures);
	}
}

//static
void LLImageGL::setManualImage(U32 target, S32 miplevel, S32 intformat,
							   S32 width, S32 height, U32 pixformat,
							   U32 pixtype, const void* pixels,
							   bool allow_compression)
{
	LL_TRACY_TIMER(TRC_SET_MANUAL_IMAGE);

	U32* scratch = NULL;
	const U32 pixels_count = (U32)(width * height);

	if (LLRender::sGLCoreProfile)
	{
#ifdef GL_ARB_texture_swizzle
		if (gGLManager.mHasTextureSwizzle)
		{
			if (pixformat == GL_ALPHA)
			{
				// GL_ALPHA is deprecated, convert to RGBA
				const GLint mask[] = { GL_ZERO, GL_ZERO, GL_ZERO, GL_RED };
				glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, mask);
				pixformat = GL_RED;
				intformat = GL_R8;
			}

			if (pixformat == GL_LUMINANCE)
			{
				// GL_LUMINANCE is deprecated, convert to RGBA
				const GLint mask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
				glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, mask);
				pixformat = GL_RED;
				intformat = GL_R8;
			}

			if (pixformat == GL_LUMINANCE_ALPHA)
			{
				// GL_LUMINANCE_ALPHA is deprecated, convert to RGBA
				const GLint mask[] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
				glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, mask);
				pixformat = GL_RG;
				intformat = GL_RG8;
			}
		}
		else
#endif
		{
			if (pixformat == GL_ALPHA && pixtype == GL_UNSIGNED_BYTE)
			{
				// GL_ALPHA is deprecated, convert to RGBA
				scratch = new U32[pixels_count];
				for (U32 i = 0; i < pixels_count; ++i)
				{
					U8* pix = (U8*)&scratch[i];
					pix[0] = pix[1] = pix[2] = 0;
					pix[3] = ((U8*)pixels)[i];
				}
				pixformat = GL_RGBA;
				intformat = GL_RGBA8;
				pixels = scratch;
			}

			if (pixformat == GL_LUMINANCE_ALPHA && pixtype == GL_UNSIGNED_BYTE)
			{
				// GL_LUMINANCE_ALPHA is deprecated, convert to RGBA
				scratch = new U32[pixels_count];
				for (U32 i = 0; i < pixels_count; ++i)
				{
					U8 lum = ((U8*)pixels)[2 * i];
					U8 alpha = ((U8*)pixels)[2 * i + 1];
					U8* pix = (U8*)&scratch[i];
					pix[0] = pix[1] = pix[2] = lum;
					pix[3] = alpha;
				}
				pixformat = GL_RGBA;
				intformat = GL_RGBA8;
				pixels = scratch;
			}

			if (pixformat == GL_LUMINANCE && pixtype == GL_UNSIGNED_BYTE)
			{
				// GL_LUMINANCE is deprecated, convert to RGBA
				scratch = new U32[pixels_count];
				for (U32 i = 0; i < pixels_count; ++i)
				{
					U8 lum = ((U8*)pixels)[i];
					U8* pix = (U8*)&scratch[i];
					pix[0] = pix[1] = pix[2] = lum;
					pix[3] = 255;
				}
				pixformat = GL_RGBA;
				intformat = GL_RGB8;
				pixels = scratch;
			}
		}
	}

	if (allow_compression && sCompressTextures &&
		pixels_count > sCompressThreshold)
	{
		switch (intformat)
		{
			case GL_RED:
			case GL_R8:
				intformat = GL_COMPRESSED_RED;
				break;

			case GL_RG:
			case GL_RG8:
				intformat = GL_COMPRESSED_RG;
				break;

			case GL_RGB:
			case GL_RGB8:
				intformat = GL_COMPRESSED_RGB;
				break;

			case GL_SRGB:
			case GL_SRGB8:
				intformat = GL_COMPRESSED_SRGB;
				break;

			case GL_RGBA:
			case GL_RGBA8:
				intformat = GL_COMPRESSED_RGBA;
				break;

			case GL_SRGB_ALPHA:
			case GL_SRGB8_ALPHA8:
				intformat = GL_COMPRESSED_SRGB_ALPHA;
				break;

			case GL_LUMINANCE:
			case GL_LUMINANCE8:
				intformat = GL_COMPRESSED_LUMINANCE;
				break;

			case GL_LUMINANCE_ALPHA:
			case GL_LUMINANCE8_ALPHA8:
				intformat = GL_COMPRESSED_LUMINANCE_ALPHA;
				break;

			case GL_ALPHA:
			case GL_ALPHA8:
				intformat = GL_COMPRESSED_ALPHA;
				break;

			default:
				llwarns_once << "Could not compress format: " << std::hex
							 << intformat << std::dec << llendl;
		}
	}

	glTexImage2D(target, miplevel, intformat, width, height, 0, pixformat,
				 pixtype, pixels);
	stop_glerror();

	if (scratch)
	{
		delete[] scratch;
	}
}

// Create an empty GL texture: just create a texture name. The texture is
// associated with some image by calling glTexImage outside LLImageGL.
bool LLImageGL::createGLTexture()
{
	LL_TRACY_TIMER(TRC_CREATE_GL_TEXTURE1);
	if (gGLManager.mIsDisabled)
	{
		llwarns << "Trying to create a texture while GL is disabled !"
				<< llendl;
		return false;
	}

	// Do not save this texture when gl is destroyed.
	mGLTextureCreated = false;

	llassert(gGLManager.mInited);
	stop_glerror();

	if (mTexName)
	{
		deleteTextures(1, reinterpret_cast<GLuint*>(&mTexName));
	}

	LLImageGL::generateTextures(1, &mTexName);
	stop_glerror();
	if (!mTexName)
	{
		llwarns << "Failed to make an empty texture" << llendl;
		return false;
	}

	return true;
}

bool LLImageGL::createGLTexture(S32 discard_level, const LLImageRaw* imageraw,
								S32 usename, bool to_create, S32 category)
{
	LL_TRACY_TIMER(TRC_CREATE_GL_TEXTURE2);
	if (gGLManager.mIsDisabled)
	{
		llwarns << "Trying to create a texture while GL is disabled !"
				<< llendl;
		return false;
	}

	if (!imageraw || !imageraw->getData())
	{
		return false;
	}

	mGLTextureCreated = false;
	llassert(gGLManager.mInited);

	if (discard_level < 0)
	{
		llassert(mCurrentDiscardLevel >= 0);
		discard_level = mCurrentDiscardLevel;
	}
	if (sPreserveDiscard)
	{
		discard_level = llclamp(discard_level, 0, (S32)mMaxDiscardLevel);
	}

	// Actual image width/height = raw image width/height * 2^discard_level
	S32 raw_w = imageraw->getWidth();
	S32 raw_h = imageraw->getHeight();
	S32 w = raw_w << discard_level;
	S32 h = raw_h << discard_level;

	// setSize may call destroyGLTexture if the size does not match
	if (!setSize(w, h, imageraw->getComponents(), discard_level))
	{
		return false;
	}

	if (mHasExplicitFormat &&
		((mFormatPrimary == GL_RGBA && mComponents < 4) ||
		 (mFormatPrimary == GL_RGB && mComponents < 3)))
	{
		llwarns << "Incorrect format: " << std::hex << mFormatPrimary
				<< std::dec << " - Number of components: " << (U32)mComponents
				<< llendl;
		mHasExplicitFormat = false;
	}

	if (!mHasExplicitFormat)
	{
		switch (mComponents)
		{
			case 1:
				// Use luminance alpha (for fonts)
				mFormatInternal = GL_LUMINANCE8;
				mFormatPrimary = GL_LUMINANCE;
				mFormatType = GL_UNSIGNED_BYTE;
				break;

			case 2:
				// Use luminance alpha (for fonts)
				mFormatInternal = GL_LUMINANCE8_ALPHA8;
				mFormatPrimary = GL_LUMINANCE_ALPHA;
				mFormatType = GL_UNSIGNED_BYTE;
				break;

			case 3:
#if LL_USE_SRGB_DECODE
				if (gUseNewShaders && gGLManager.mHasTexturesRGBDecode)
				{
					mFormatInternal = GL_SRGB8;
				}
				else
#endif
				{
					mFormatInternal = GL_RGB8;
				}
				mFormatPrimary = GL_RGB;
				mFormatType = GL_UNSIGNED_BYTE;
				break;

			case 4:
#if LL_USE_SRGB_DECODE
				if (gUseNewShaders && gGLManager.mHasTexturesRGBDecode)
				{
					mFormatInternal = GL_SRGB8_ALPHA8;
				}
				else
#endif
				{
					mFormatInternal = GL_RGBA8;
				}
				mFormatPrimary = GL_RGBA;
				mFormatType = GL_UNSIGNED_BYTE;
				break;

			default:
				llwarns << "Bad number of components for texture: "
						<< (U32)getComponents() << LL_ENDL;
				to_create = false;
		}

		calcAlphaChannelOffsetAndStride();
	}

	if (!to_create) // Do not create a GL texture
	{
		destroyGLTexture();
		mCurrentDiscardLevel = discard_level;
		mLastBindTime = sLastFrameTime;
		return true;
	}

	setCategory(category);
 	const U8* rawdata = imageraw->getData();
	return createGLTexture(discard_level, rawdata, false, usename);
}

bool LLImageGL::createGLTexture(S32 discard_level, const U8* data_in,
								bool data_hasmips, S32 usename)
{
	LL_TRACY_TIMER(TRC_CREATE_GL_TEXTURE3);
	llassert(data_in);
	stop_glerror();

	if (discard_level < 0)
	{
		llassert(mCurrentDiscardLevel >= 0);
		discard_level = mCurrentDiscardLevel;
	}
	discard_level = llclamp(discard_level, 0, (S32)mMaxDiscardLevel);

	if (mTexName != 0 && discard_level == mCurrentDiscardLevel)
	{
		// This will only be true if the size has not changed
		return setImage(data_in, data_hasmips);
	}

	U32 old_name = mTexName;

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	if (usename != 0)
	{
		mTexName = usename;
	}
	else
	{
		generateTextures(1, &mTexName);
		unit0->bind(this);
		U32 type = LLTexUnit::getInternalType(mBindTarget);
		glTexParameteri(type, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(type, GL_TEXTURE_MAX_LEVEL,
						mMaxDiscardLevel - discard_level);
	}
	if (!mTexName)
	{
		llwarns << "Failed to make texture" << llendl;
		if (old_name)
		{
			sGlobalTextureMemoryInBytes -= mTextureMemory;
			deleteTextures(1, &old_name);
			stop_glerror();
		}
		return false;
	}

	if (mUseMipMaps)
	{
		mAutoGenMips = gGLManager.mHasMipMapGeneration;
#if LL_DARWIN
		// On the Mac GF2 and GF4MX drivers, auto mipmap generation does not
		// work right with alpha-only textures.
		if (gGLManager.mIsGF2or4MX && mFormatInternal == GL_ALPHA8 &&
			mFormatPrimary == GL_ALPHA)
		{
			mAutoGenMips = false;
		}
#endif
	}

	mCurrentDiscardLevel = discard_level;

	if (!setImage(data_in, data_hasmips))
	{
		stop_glerror();
		return false;
	}

	// Set texture options to our defaults.
	unit0->setHasMipMaps(mHasMipMaps);
	unit0->setTextureAddressMode(mAddressMode);
	unit0->setTextureFilteringOption(mFilterOption);

	// Things will break if we do not unbind after creation
	unit0->unbind(mBindTarget);
	stop_glerror();

	if (old_name)
	{
		sGlobalTextureMemoryInBytes -= mTextureMemory;
		deleteTextures(1, &old_name);
		stop_glerror();
	}

	mTextureMemory = getMipBytes(discard_level);
	sGlobalTextureMemoryInBytes += mTextureMemory;

	// Mark this as bound at this point, so we do not throw it out immediately
	mLastBindTime = sLastFrameTime;
	return true;
}

bool LLImageGL::readBackRaw(S32 discard_level, LLImageRaw* imageraw,
							bool compressed_ok) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}

	if (mTexName == 0 || discard_level < mCurrentDiscardLevel ||
		discard_level > mMaxDiscardLevel)
	{
		return false;
	}

	S32 gl_discard = discard_level - mCurrentDiscardLevel;

	// Explicitly unbind texture
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->unbind(mBindTarget);
	if (!unit0->bindManual(mBindTarget, mTexName))
	{
		return false;
	}

	GLint glwidth = 0;
	glGetTexLevelParameteriv(mTarget, gl_discard, GL_TEXTURE_WIDTH, &glwidth);
	if (glwidth == 0)
	{
		// No mip data smaller than current discard level
		return false;
	}

	S32 width = getWidth(discard_level);
	S32 height = getHeight(discard_level);
	S32 ncomponents = getComponents();
	if (ncomponents == 0)
	{
		return false;
	}
	if (width < (S32)glwidth)
	{
		llwarns << "Texture size is smaller than it should be: width: "
				<< width << " - glwidth: " << glwidth << " - mWidth: "
				<< mWidth << " - mCurrentDiscardLevel: "
				<< (S32)mCurrentDiscardLevel << " - discard_level: "
				<< (S32)discard_level << llendl;
		return false;
	}

	if (width <= 0 || width > 2048 || height <= 0 || height > 2048 ||
		ncomponents < 1 || ncomponents > 4)
	{
		llerrs << "Bogus size/components: " << width << "x" << height << "x"
			   << ncomponents << llendl;
	}

	GLint is_compressed = 0;
	if (compressed_ok)
	{
		glGetTexLevelParameteriv(mTarget, is_compressed, GL_TEXTURE_COMPRESSED,
								 &is_compressed);
	}

	GLenum error;
	while ((error = glGetError()) != GL_NO_ERROR)
	{
		llwarns << "GL Error happens before reading back texture. Error code: "
				<< error << llendl;
	}

	if (is_compressed)
	{
		GLint glbytes;
		glGetTexLevelParameteriv(mTarget, gl_discard,
								 GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &glbytes);
		if (!imageraw->allocateDataSize(width, height, ncomponents, glbytes))
		{
			llwarns << "Memory allocation failed for reading back texture. Size is: "
					<< glbytes << " - width: " << width << " - height: "
					<< height << " - components: " << ncomponents << llendl;
			return false;
		}

		glGetCompressedTexImageARB(mTarget, gl_discard,
								   (GLvoid*)imageraw->getData());
	}
	else
	{
		if (!imageraw->allocateDataSize(width, height, ncomponents))
		{
			llwarns << "Memory allocation failed for reading back texture: width: "
					<< width << " - height: " << height << " - components: "
					<< ncomponents << llendl;
			return false;
		}

		glGetTexImage(GL_TEXTURE_2D, gl_discard, mFormatPrimary, mFormatType,
					  (GLvoid*)(imageraw->getData()));
	}

	if ((error = glGetError()) != GL_NO_ERROR)
	{
		llwarns << "GL Error happens after reading back texture. Error code: "
				<< error << llendl;
		imageraw->deleteData();

		while ((error = glGetError()) != GL_NO_ERROR)
		{
			llwarns << "GL Error happens after reading back texture. Error code: "
					<< error << llendl;
		}

		return false;
	}

	return true;
}

void LLImageGL::destroyGLTexture()
{
	if (mTexName != 0)
	{
		if (mTextureMemory)
		{
			sGlobalTextureMemoryInBytes -= mTextureMemory;
			mTextureMemory = 0;
		}

		deleteTextures(1, &mTexName);
		mCurrentDiscardLevel = -1; // invalidate mCurrentDiscardLevel.
		mTexName = 0;
		mGLTextureCreated = false;
	}
}

// Force to invalidate the gl texture, most likely a sculpty texture
void LLImageGL::forceToInvalidateGLTexture()
{
	if (mTexName != 0)
	{
		destroyGLTexture();
	}
	else
	{
		mCurrentDiscardLevel = -1 ; // invalidate mCurrentDiscardLevel.
	}
}

void LLImageGL::setAddressMode(LLTexUnit::eTextureAddressMode mode)
{
	if (mAddressMode != mode)
	{
		mTexOptionsDirty = true;
		mAddressMode = mode;
	}

	LLTexUnit* unit = gGL.getTexUnit(gGL.getCurrentTexUnitIndex());
	if (unit->getCurrTexture() == mTexName)
	{
		unit->setTextureAddressMode(mode);
		mTexOptionsDirty = false;
	}
}

void LLImageGL::setFilteringOption(LLTexUnit::eTextureFilterOptions option)
{
	if (mFilterOption != option)
	{
		mTexOptionsDirty = true;
		mFilterOption = option;
	}

	if (mTexName == 0)
	{
		return;
	}

	LLTexUnit* unit = gGL.getTexUnit(gGL.getCurrentTexUnitIndex());
	if (unit->getCurrTexture() == mTexName)
	{
		unit->setTextureFilteringOption(option);
		mTexOptionsDirty = false;
		stop_glerror();
	}
}

bool LLImageGL::getIsResident(bool test_now)
{
	if (test_now)
	{
		if (mTexName != 0)
		{
			glAreTexturesResident(1, (GLuint*)&mTexName, &mIsResident);
		}
		else
		{
			mIsResident = false;
		}
	}

	return mIsResident;
}

S32 LLImageGL::getHeight(S32 discard_level) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	S32 height = mHeight >> discard_level;
	if (height < 1) height = 1;
	return height;
}

S32 LLImageGL::getWidth(S32 discard_level) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	S32 width = mWidth >> discard_level;
	if (width < 1) width = 1;
	return width;
}

S32 LLImageGL::getBytes(S32 discard_level) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	S32 w = mWidth >> discard_level;
	S32 h = mHeight >> discard_level;
	if (w == 0) w = 1;
	if (h == 0) h = 1;
	return dataFormatBytes(mFormatPrimary, w, h);
}

S32 LLImageGL::getMipBytes(S32 discard_level) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	S32 w = mWidth >> discard_level;
	S32 h = mHeight >> discard_level;
	S32 res = dataFormatBytes(mFormatPrimary, w, h);
	if (mUseMipMaps)
	{
		while (w > 1 && h > 1)
		{
			w >>= 1; if (w == 0) w = 1;
			h >>= 1; if (h == 0) h = 1;
			res += dataFormatBytes(mFormatPrimary, w, h);
		}
	}
	return res;
}

void LLImageGL::setTarget(U32 target, LLTexUnit::eTextureType bind_target)
{
	mTarget = target;
	mBindTarget = bind_target;
}

void LLImageGL::setNeedsAlphaAndPickMask(bool need_mask)
{
	if (mNeedsAlphaAndPickMask != need_mask)
	{
		mNeedsAlphaAndPickMask = need_mask;

		if (mNeedsAlphaAndPickMask)
		{
			mAlphaOffset = 0;
		}
		else // Do not need alpha mask
		{
			mAlphaOffset = INVALID_OFFSET;
			mIsMask = false;
		}
	}
}

void LLImageGL::calcAlphaChannelOffsetAndStride()
{
	if (mAlphaOffset == INVALID_OFFSET) // Do not need alpha mask
	{
		return;
	}

	mAlphaStride = -1;
	switch (mFormatPrimary)
	{
		case GL_LUMINANCE:
		case GL_ALPHA:
			mAlphaStride = 1;
			break;
		case GL_LUMINANCE_ALPHA:
			mAlphaStride = 2;
			break;
		case GL_RED:
		case GL_RGB:
		case GL_SRGB:
			mNeedsAlphaAndPickMask = mIsMask = false;
#if FIX_MASKS
			mAlphaOffset = INVALID_OFFSET;
#endif
			return; // No alpha channel.
		case GL_RGBA:
		case GL_SRGB_ALPHA:
			mAlphaStride = 4;
			break;
		case GL_BGRA_EXT:
			mAlphaStride = 4;
			break;
		default:
			break;
	}

	mAlphaOffset = -1;
	if (mFormatType == GL_UNSIGNED_BYTE)
	{
		mAlphaOffset = mAlphaStride - 1;
	}
	else if (is_little_endian())
	{
		if (mFormatType == GL_UNSIGNED_INT_8_8_8_8)
		{
			mAlphaOffset = 0;
		}
		else if (mFormatType == GL_UNSIGNED_INT_8_8_8_8_REV)
		{
			mAlphaOffset = 3;
		}
	}
	else // Big endian
	{
		if (mFormatType == GL_UNSIGNED_INT_8_8_8_8)
		{
			mAlphaOffset = 3;
		}
		else if (mFormatType == GL_UNSIGNED_INT_8_8_8_8_REV)
		{
			mAlphaOffset = 0;
		}
	}

	if (mAlphaStride < 1 ||					// Unsupported format
		mAlphaOffset < 0 ||					// Unsupported type
		(mFormatPrimary == GL_BGRA_EXT &&
		 mFormatType != GL_UNSIGNED_BYTE))	// Unknown situation
	{
		llwarns << "Cannot analyze alpha for image with format type "
				<< std::hex << mFormatType << std::dec << llendl;

		mNeedsAlphaAndPickMask = mIsMask = false;
#if FIX_MASKS
		mAlphaOffset = INVALID_OFFSET;
#endif
	}
}

void LLImageGL::analyzeAlpha(const void* data_in, U32 w, U32 h)
{
	if (!mNeedsAlphaAndPickMask)
	{
		return;
	}

	U32 length = w * h;
	U32 alphatotal = 0;

	U32 sample[16];
	memset(sample, 0, sizeof(U32) * 16);

	// Generate histogram of quantized alpha. Also add-in the histogram of a
	// 2x2 box-sampled version. The idea is this will mid-skew the data (and
	// thus increase the chances of not being used as a mask) from high-
	// frequency alpha maps which suffer the worst from aliasing when used as
	// alpha masks.
	if (w >= 2 && h >= 2)
	{
		llassert(w % 2 == 0);
		llassert(h % 2 == 0);
		const GLubyte* rowstart = (const GLubyte*)data_in + mAlphaOffset;
		for (U32 y = 0; y < h; y += 2)
		{
			const GLubyte* current = rowstart;
			for (U32 x = 0; x < w; x += 2)
			{
				const U32 s1 = current[0];
				alphatotal += s1;
				const U32 s2 = current[w * mAlphaStride];
				alphatotal += s2;
				current += mAlphaStride;
				const U32 s3 = current[0];
				alphatotal += s3;
				const U32 s4 = current[w * mAlphaStride];
				alphatotal += s4;
				current += mAlphaStride;

				++sample[s1 / 16];
				++sample[s2 / 16];
				++sample[s3 / 16];
				++sample[s4 / 16];

				const U32 asum = (s1 + s2 + s3 + s4);
				alphatotal += asum;
				sample[asum / 64] += 4;
			}


			rowstart += 2 * w * mAlphaStride;
		}
		length *= 2; // we sampled everything twice, essentially
	}
	else
	{
		const GLubyte* current = ((const GLubyte*)data_in) + mAlphaOffset;
		for (U32 i = 0; i < length; ++i)
		{
			const U32 s1 = *current;
			alphatotal += s1;
			++sample[s1 / 16];
			current += mAlphaStride;
		}
	}

	// If more than 1/16th of alpha samples are mid-range, this shouldn't be
	// treated as a 1-bit mask. Also, if all of the alpha samples are clumped
	// on one half of the range (but not at an absolute extreme), then consider
	// this to be an intentional effect and don't treat as a mask.

	U32 midrangetotal = 0;
	for (U32 i = 2; i < 13; ++i)
	{
		midrangetotal += sample[i];
	}
	U32 lowerhalftotal = 0;
	for (U32 i = 0; i < 8; ++i)
	{
		lowerhalftotal += sample[i];
	}
	U32 upperhalftotal = 0;
	for (U32 i = 8; i < 16; ++i)
	{
		upperhalftotal += sample[i];
	}

	if (midrangetotal > length / 48 ||	// Lots of midrange, or
		// All close to transparent but not all totally transparent, or
	    (lowerhalftotal == length && alphatotal != 0) ||
		// All close to opaque but not all totally opaque
	    (upperhalftotal == length && alphatotal != 255 * length))
	{
		mIsMask = false; // Not suitable for masking
	}
	else
	{
		mIsMask = true;
	}
}

U32 LLImageGL::createPickMask(S32 width, S32 height)
{
	U32 pick_width = width / 2 + 1;
	U32 pick_height = height / 2 + 1;

	U32 size = pick_width * pick_height;
	size = (size + 7) / 8; // pixelcount-to-bits
	mPickMask = new (std::nothrow) U8[size];
	if (mPickMask)
	{
		mPickMaskWidth = pick_width - 1;
		mPickMaskHeight = pick_height - 1;
		memset(mPickMask, 0, sizeof(U8) * size);
	}
	else
	{
		mPickMaskWidth = 0;
		mPickMaskHeight = 0;
		size = 0;
	}

	return size;
}

void LLImageGL::freePickMask()
{
	// mPickMask validity depends on old image size, delete it
	if (mPickMask)
	{
		delete[] mPickMask;
		mPickMask = NULL;
	}
	mPickMaskWidth = mPickMaskHeight = 0;
}

void LLImageGL::updatePickMask(S32 width, S32 height, const U8* data_in)
{
	if (!mNeedsAlphaAndPickMask)
	{
		return;
	}

	freePickMask();

	if (mFormatType != GL_UNSIGNED_BYTE ||
		(mFormatPrimary != GL_RGBA && mFormatPrimary != GL_SRGB_ALPHA))
	{
		// Cannot generate a pick mask for this texture
		return;
	}

#if LL_DEBUG
	const U32 size = createPickMask(width, height);
#else
	createPickMask(width, height);
#endif

	U32 pick_bit = 0;
	for (S32 y = 0; y < height; y += 2)
	{
		for (S32 x = 0; x < width; x += 2)
		{
			U8 alpha = data_in[(y * width + x) * 4 + 3];

			if (alpha > 32)
			{
				U32 pick_idx = pick_bit / 8;
				U32 pick_offset = pick_bit % 8;
				llassert(pick_idx < size);

				mPickMask[pick_idx] |= 1 << pick_offset;
			}

			++pick_bit;
		}
	}
}

bool LLImageGL::getMask(const LLVector2 &tc)
{
	bool res = true;

	if (mPickMask)
	{
		F32 u, v;
		if (LL_LIKELY(tc.isFinite()))
		{
			u = tc.mV[0] - floorf(tc.mV[0]);
			v = tc.mV[1] - floorf(tc.mV[1]);
		}
		else
		{
			llwarns_once << "Non-finite u/v in mask pick !" << llendl;
			u = v = 0.f;
			// removing assert per EXT-4388
			//llassert(false);
		}

		if (LL_UNLIKELY(u < 0.f || u > 1.f || v < 0.f || v > 1.f))
		{
			llwarns_once << "u/v out of range in image mask pick !" << llendl;
			u = v = 0.f;
			// removing assert per EXT-4388
			//llassert(false);
		}

		S32 x = llfloor(u * mPickMaskWidth);
		S32 y = llfloor(v * mPickMaskHeight);

		if (LL_UNLIKELY(x > mPickMaskWidth))
		{
			llwarns_once << "Width overrun on pick mask read !" << llendl;
			x = llmax((U16)0, mPickMaskWidth);
		}
		if (LL_UNLIKELY(y > mPickMaskHeight))
		{
			llwarns_once << "Height overrun on pick mask read !" << llendl;
			y = llmax((U16)0, mPickMaskHeight);
		}

		S32 idx = y * mPickMaskWidth + x;
		S32 offset = idx % 8;

		res = (mPickMask[idx / 8] & (1 << offset)) != 0;
	}

	return res;
}

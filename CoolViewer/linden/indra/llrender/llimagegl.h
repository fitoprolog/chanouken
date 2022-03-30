/**
 * @file llimagegl.h
 * @brief Object for managing images and their textures
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


#ifndef LL_LLIMAGEGL_H
#define LL_LLIMAGEGL_H

#include "llimage.h"	// For allocate_texture_mem() and free_texture_mem()
#include "llrefcount.h"
#include "llrender.h"

class LLImageGL : public LLRefCount
{
	friend class LLTexUnit;

protected:
	LOG_CLASS(LLImageGL);

	virtual ~LLImageGL();

	void analyzeAlpha(const void* data_in, U32 w, U32 h);
	void calcAlphaChannelOffsetAndStride();

public:
#if LL_JEMALLOC
	LL_INLINE void* operator new(size_t size)
	{
		return allocate_texture_mem(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return allocate_texture_mem(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		free_texture_mem(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		free_texture_mem(ptr);
	}
#endif

	// These 2 functions replace glGenTextures() and glDeleteTextures()
	static void generateTextures(S32 num_textures, U32* textures);
	static void deleteTextures(S32 num_textures, U32* textures);

	// Size calculation
	static S32 dataFormatBits(S32 dataformat);
	static S32 dataFormatBytes(S32 dataformat, S32 width, S32 height);
	static S32 dataFormatComponents(S32 dataformat);

	bool updateBindStats(S32 tex_mem) const;
	F32 getTimePassedSinceLastBound();
	void forceUpdateBindStats() const;

	// Needs to be called every frame
	static void updateStats(F32 current_time);

	// Save off / restore GL textures
	static void destroyGL(bool save_state = true);
	static void restoreGL();
	static void dirtyTexOptions();

	// Sometimes called externally for textures not using LLImageGL (should
	// go away...)
	static S32 updateBoundTexMem(S32 mem, S32 ncomponents, S32 category);

	static bool checkSize(S32 width, S32 height);

	// For server side use only. Not currently necessary for LLImageGL, but
	// required in some derived classes, so include for compatability
	static bool create(LLPointer<LLImageGL>& dest, bool usemipmaps = true);
	static bool create(LLPointer<LLImageGL>& dest, U32 width, U32 height,
					   U8 components, bool usemipmaps = true);
	static bool create(LLPointer<LLImageGL>& dest, const LLImageRaw* imageraw,
					   bool usemipmaps = true);

	LLImageGL(bool usemipmaps = true);
	LLImageGL(U32 width, U32 height, U8 components, bool usemipmaps = true);
	LLImageGL(const LLImageRaw* imageraw, bool usemipmaps = true);
	// For wrapping textures created via GL elsewhere with our API only. Use
	// with caution.
	LLImageGL(U32 tex_name, U32 components, U32 target, S32 format_internal,
			  U32 format_primary, U32 format_type,
			  LLTexUnit::eTextureAddressMode mode);

	virtual void dump();	// debugging info to llinfos

	bool setSize(S32 width, S32 height, S32 ncomponents,
				 S32 discard_level = -1);
	LL_INLINE void setComponents(S32 ncomponents)			{ mComponents = (S8)ncomponents; }
	LL_INLINE void setAllowCompression(bool allow)			{ mAllowCompression = allow; }

	static void setManualImage(U32 target, S32 miplevel, S32 intformat,
							   S32 width, S32 height,
							   U32 pixformat, U32 pixtype,
							   const void* pixels,
							   bool allow_compression = true);

	bool createGLTexture();
	bool createGLTexture(S32 discard_level, const LLImageRaw* imageraw,
						 S32 usename = 0, bool to_create = true,
						 S32 category = 0);
	bool createGLTexture(S32 discard_level, const U8* data,
						 bool data_hasmips = false, S32 usename = 0);
	void setImage(const LLImageRaw* imageraw);
	bool setImage(const U8* data_in, bool data_hasmips = false);
	bool setSubImage(const LLImageRaw* imageraw, S32 x_pos, S32 y_pos,
					 S32 width, S32 height, bool force_fast_update = false);
	bool setSubImage(const U8* datap, S32 data_width, S32 data_height,
					 S32 x_pos, S32 y_pos, S32 width, S32 height,
					 bool force_fast_update = false);
	bool setSubImageFromFrameBuffer(S32 fb_x, S32 fb_y, S32 x_pos, S32 y_pos,
									S32 width, S32 height);

	// Read back a raw image for this discard level, if it exists
	bool readBackRaw(S32 discard_level, LLImageRaw* imageraw,
					 bool compressed_ok) const;
	void destroyGLTexture();
	void forceToInvalidateGLTexture();

	void setExplicitFormat(S32 internal_format, U32 primary_format,
						   U32 type_format = 0, bool swap_bytes = false);
	LL_INLINE void setComponents(S8 ncomponents)			{ mComponents = ncomponents; }

	LL_INLINE S32 getDiscardLevel() const					{ return mCurrentDiscardLevel; }
	LL_INLINE S32 getMaxDiscardLevel() const				{ return mMaxDiscardLevel; }

	LL_INLINE S32 getCurrentWidth() const					{ return mWidth;}
	LL_INLINE S32 getCurrentHeight() const					{ return mHeight;}
	S32 getWidth(S32 discard_level = -1) const;
	S32 getHeight(S32 discard_level = -1) const;
	LL_INLINE U8 getComponents() const						{ return mComponents; }
	S32 getBytes(S32 discard_level = -1) const;
	S32 getMipBytes(S32 discard_level = -1) const;

	LL_INLINE bool getBoundRecently() const
	{
		constexpr F32 MIN_TEXTURE_LIFETIME = 10.f;
		return sLastFrameTime - mLastBindTime < MIN_TEXTURE_LIFETIME;
	}

	LL_INLINE bool isJustBound() const						{ return sLastFrameTime - mLastBindTime < 0.5f; }

	LL_INLINE bool hasExplicitFormat() const				{ return mHasExplicitFormat; }
	LL_INLINE U32 getPrimaryFormat() const					{ return mFormatPrimary; }
	LL_INLINE U32 getFormatType() const						{ return mFormatType; }

	LL_INLINE bool getHasGLTexture() const					{ return mTexName != 0; }
	LL_INLINE U32 getTexName() const						{ return mTexName; }
	LL_INLINE void setTexName(U32 name)						{ mTexName = name; }

	LL_INLINE bool getIsAlphaMask() const					{ return mIsMask; }

	bool getIsResident(bool test_now = false); // not const

	void setTarget(U32 target, LLTexUnit::eTextureType bind_target);

	LL_INLINE LLTexUnit::eTextureType getTarget() const		{ return mBindTarget; }
	LL_INLINE bool isGLTextureCreated() const				{ return mGLTextureCreated; }
	LL_INLINE void setGLTextureCreated(bool initialized)	{ mGLTextureCreated = initialized; }

	LL_INLINE bool getUseMipMaps() const					{ return mUseMipMaps; }
	LL_INLINE void setUseMipMaps(bool usemips)				{ mUseMipMaps = usemips; }

	void updatePickMask(S32 width, S32 height, const U8* data_in);
	bool getMask(const LLVector2& tc);

	// Sets the addressing mode used to sample the texture (such as wrapping,
	// mirrored wrapping, and clamp).
	// Note: this actually gets set the next time the texture is bound.
	void setAddressMode(LLTexUnit::eTextureAddressMode mode);

	LL_INLINE LLTexUnit::eTextureAddressMode getAddressMode() const
	{
		return mAddressMode;
	}

	// Sets the filtering options used to sample the texture (such as point
	// sampling, bilinear interpolation, mipmapping and anisotropic filtering).
	// Note: this actually gets set the next time the texture is bound.
	void setFilteringOption(LLTexUnit::eTextureFilterOptions option);

	LL_INLINE LLTexUnit::eTextureFilterOptions getFilteringOption() const
	{
		return mFilterOption;
	}

	LL_INLINE U32 getTexTarget()const						{ return mTarget; }

	void init(bool usemipmaps);

	 // Clean up the LLImageGL so it can be reinitialized. Be careful when
	// using this in derived class destructors
	virtual void cleanup();

	void setNeedsAlphaAndPickMask(bool need_mask);

	LL_INLINE void	setCategory(S32 category)				{ mCategory = category; }
	LL_INLINE S32  	getCategory() const						{ return mCategory; }

#if DEBUG_MISS
	LL_INLINE bool getMissed() const						{ return mMissed; }
#else
	LL_INLINE bool getMissed() const						{ return false; }
#endif

private:
	U32 createPickMask(S32 width, S32 height);
	void freePickMask();

private:
	LLPointer<LLImageRaw> mSaveData;	// used for destroyGL/restoreGL

	// Downsampled bitmap approximation of alpha channel. NULL if no alpha
	// channel:
	U8*			mPickMask;

	U16			mPickMaskWidth;
	U16			mPickMaskHeight;

	U16			mWidth;
	U16			mHeight;

	S32			mCategory;

	U32			mTexName;

	bool		mUseMipMaps;

	// If false (default), GL format is f(mComponents):
	bool		mHasExplicitFormat;

	bool		mAutoGenMips;

	bool		mIsMask;
	bool		mNeedsAlphaAndPickMask;
	S8			mAlphaStride;
	S8			mAlphaOffset;

	bool		mGLTextureCreated;
	S8			mCurrentDiscardLevel;

	bool		mAllowCompression;

protected:
	bool		mHasMipMaps;

	bool		mTexOptionsDirty;

	// If true, use glPixelStorei(GL_UNPACK_SWAP_BYTES, 1)
	bool		mFormatSwapBytes;

	bool		mExternalTexture;

	GLboolean	mIsResident;

	S8			mComponents;
	S8			mMaxDiscardLevel;

	// Normally GL_TEXTURE2D, sometimes something else (ex. cube maps):
	U32			mTarget;

	// Normally TT_TEXTURE, sometimes something else (ex. cube maps)
	LLTexUnit::eTextureType mBindTarget;

	S32			mMipLevels;

	// Defaults to TAM_WRAP
	LLTexUnit::eTextureAddressMode mAddressMode;
	// Defaults to TFO_ANISOTROPIC
	LLTexUnit::eTextureFilterOptions mFilterOption;

	S32			mFormatInternal;	// = GL internalformat
	U32			mFormatPrimary;		// = GL format (pixel data format)
	U32			mFormatType;

public:
	// Various GL/Rendering options
	S32						mTextureMemory;

	// Last time this was bound, by discard level
	mutable F32				mLastBindTime;

#if DEBUG_MISS
	bool					mMissed	// Missed on last bind ?
#endif

	static S32				sCount;
	static F32				sLastFrameTime;
	static LLImageGL*		sDefaultGLImagep;

	// Global memory statistics

	// Tracks main memory texmem
	static S64				sGlobalTextureMemoryInBytes;
	// Tracks bound texmem for last completed frame;
	static S64				sBoundTextureMemoryInBytes;
	// Tracks bound texmem for current frame
	static S64				sCurBoundTextureMemory;
	// Tracks number of texture binds for current frame
	static U32				sBindCount;
	// Tracks number of unique texture binds for current frame
	static U32				sUniqueCount;
	static bool				sGlobalUseAnisotropic;

	// This flag *must* be set to true before stopping GL and can only be reset
	// to false again once GL is restarted (else, GL textures may get recreated
	// while GL is stopped, which leads to a crash !).
	static bool				sPreserveDiscard;

	// GL textures compression
	static bool				sCompressTextures;
	static U32				sCompressThreshold;

private:
	typedef fast_hset<LLImageGL*> glimage_list_t;
	static glimage_list_t	sImageList;
};

#endif // LL_LLIMAGEGL_H

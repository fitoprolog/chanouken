/**
 * @file llgltexture.h
 * @brief Object for managing OpenGL textures
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#ifndef LL_GL_TEXTURE_H
#define LL_GL_TEXTURE_H

#include "llgl.h"
#include "llrefcount.h"
#include "llrender.h"

class LLFontGL;
class LLImageGL;
class LLImageRaw;
class LLViewerFetchedTexture;

// This is the parent for the LLViewerTexture class. Via its virtual methods,
// the LLViewerTexture class can be reached from llrender.

class LLGLTexture : public virtual LLRefCount
{
	friend class LLFontGL;
	friend class LLTexUnit;

protected:
	LOG_CLASS(LLGLTexture);

public:
	enum
	{
		MAX_IMAGE_SIZE_DEFAULT = 1024,
		INVALID_DISCARD_LEVEL = 0x7fff
	};

	enum EBoostLevel
	{
		BOOST_NONE = 0,
		BOOST_ALM, 				// == NONE when ALM is on, max discard when off
		BOOST_AVATAR_BAKED,
		BOOST_AVATAR,
		BOOST_CLOUDS,
		BOOST_SCULPTED,

		BOOST_HIGH = 10,
		BOOST_BUMP,
		BOOST_TERRAIN,			// Has to be high prio for minimap/low detail
		BOOST_SELECTED,
		BOOST_AVATAR_BAKED_SELF,
		BOOST_AVATAR_SELF,		// Needed for baking avatar
		// Textures higher than this need to be downloaded at the required
		// resolution without delay.
		BOOST_SUPER_HIGH,
		BOOST_HUD,
		BOOST_ICON,
		BOOST_UI,
		BOOST_PREVIEW,
		BOOST_MAP,
		BOOST_MAP_VISIBLE,
		BOOST_MAX_LEVEL,

		// Other texture Categories
		LOCAL = BOOST_MAX_LEVEL,
		AVATAR_SCRATCH_TEX,
		DYNAMIC_TEX,
		MEDIA,
		OTHER,
		MAX_GL_IMAGE_CATEGORY
	};

	typedef enum
	{
		// Removed from memory
		DELETED = 0,
		// Ready to be removed from memory
		DELETION_CANDIDATE,
		// Not be used for the last certain period (i.e., 30 seconds).
		INACTIVE,
		// Just being used, can become inactive if not being used for a certain
		// time (10 seconds).
		ACTIVE,
		// Stays in memory, cannot be removed.
		NO_DELETE = 99
	} LLGLTextureState;

	static S32 getTotalNumOfCategories();
	static S32 getIndexFromCategory(S32 category);
	static S32 getCategoryFromIndex(S32 index);

protected:
	~LLGLTexture() override;

public:
	LLGLTexture(bool usemipmaps = true);
	LLGLTexture(const LLImageRaw* raw, bool usemipmaps);
	LLGLTexture(U32 width, U32 height, U8 components, bool usemipmaps);

	// Logs debug info
	virtual void dump();

	virtual const LLUUID& getID() const = 0;

	void setBoostLevel(S32 level);
	LL_INLINE S32 getBoostLevel()						{ return mBoostLevel; }

	LL_INLINE S32 getFullWidth() const					{ return mFullWidth; }
	LL_INLINE S32 getFullHeight() const					{ return mFullHeight; }

	void generateGLTexture();
	void destroyGLTexture();

	LL_INLINE virtual LLViewerFetchedTexture* asFetched()
	{
		return NULL;
	}

	void forceActive();
	void setNoDelete();

	LL_INLINE void dontDiscard()
	{
		mDontDiscard = true;
		mTextureState = NO_DELETE;
	}

	LL_INLINE bool getDontDiscard() const				{ return mDontDiscard; }

	//-------------------------------------------------------------------------
	// Methods to access LLImageGL
	//-------------------------------------------------------------------------

	bool hasGLTexture() const;
	U32 getTexName() const;
	bool createGLTexture();
	bool createGLTexture(S32 discard_level, const LLImageRaw* imageraw,
						 S32 usename = 0, bool to_create = true,
						 S32 category = LLGLTexture::OTHER);

	void setFilteringOption(LLTexUnit::eTextureFilterOptions option);

	void setExplicitFormat(S32 internal_format, U32 primary_format,
						   U32 type_format = 0, bool swap_bytes = false);

	void setAddressMode(LLTexUnit::eTextureAddressMode mode);

	bool setSubImage(const LLImageRaw* imageraw, S32 x_pos, S32 y_pos,
					 S32 width, S32 height);
	bool setSubImage(const U8* datap, S32 data_width, S32 data_height,
					 S32 x_pos, S32 y_pos, S32 width, S32 height);

	void setGLTextureCreated (bool initialized);
	void setCategory(S32 category);
	// For forcing with externally created textures only:
	void setTexName(U32 name);
	void setTarget(U32 target, LLTexUnit::eTextureType bind_target);

	LLTexUnit::eTextureAddressMode getAddressMode() const;
	S32 getMaxDiscardLevel() const;
	S32 getDiscardLevel() const;
	S8 getComponents() const;
	bool getBoundRecently() const;
	S32 getTextureMemory() const;
	U32 getPrimaryFormat() const;
	bool getIsAlphaMask() const;
	LLTexUnit::eTextureType getTarget() const;
	bool getMask(const LLVector2 &tc);
	F32 getTimePassedSinceLastBound();
	bool getMissed() const;
	bool isJustBound()const;
	void forceUpdateBindStats() const;

	LL_INLINE LLGLTextureState getTextureState() const	{ return mTextureState; }

	//-------------------------------------------------------------------------
	// Virtual interface to access LLViewerTexture
	//-------------------------------------------------------------------------

	virtual void setActive();
	virtual S32 getWidth(S32 discard_level = -1) const;
	virtual S32 getHeight(S32 discard_level = -1) const;

	virtual S8 getType() const = 0;
	virtual void setKnownDrawSize(S32 width, S32 height) = 0;
	virtual bool bindDefaultImage(S32 stage = 0) = 0;
	virtual void forceImmediateUpdate() = 0;

private:
	void cleanup();
	void init();

protected:
	void setTexelsPerImage();

	// NOTE: do not make this method public.
	LLImageGL* getGLImage() const;

protected:
	LLPointer<LLImageGL>	mGLTexturep;

	LLGLTextureState		mTextureState;

	S32						mBoostLevel;	// enum describing priority level
	S32						mFullWidth;
	S32						mFullHeight;
	S32						mTexelsPerImage;
	S8						mComponents;

	bool					mUseMipMaps;

	// Set to true to keep full resolution version of this image (for UI, etc)
	bool					mDontDiscard;

	mutable bool			mNeedsGLTexture;
};

#endif // LL_GL_TEXTURE_H

/**
 * @file llgltexture.cpp
 * @brief OpenGL texture implementation
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

#include "linden_common.h"

#include "llgltexture.h"

#include "llimagegl.h"

//static
S32 LLGLTexture::getTotalNumOfCategories()
{
	return MAX_GL_IMAGE_CATEGORY - BOOST_HIGH + BOOST_SCULPTED + 2;
}

//static
//index starts from zero.
S32 LLGLTexture::getIndexFromCategory(S32 category)
{
	return category < BOOST_HIGH ? category
								 : category - BOOST_HIGH + BOOST_SCULPTED + 1;
}

//static
S32 LLGLTexture::getCategoryFromIndex(S32 index)
{
	return index < BOOST_HIGH ? index
							  : index + BOOST_HIGH - BOOST_SCULPTED - 1;
}

LLGLTexture::LLGLTexture(bool usemipmaps)
{
	init();
	mUseMipMaps = usemipmaps;
}

LLGLTexture::LLGLTexture(U32 width, U32 height, U8 components, bool usemipmaps)
{
	init();
	mFullWidth = width;
	mFullHeight = height;
	mUseMipMaps = usemipmaps;
	mComponents = components;
	setTexelsPerImage();
}

LLGLTexture::LLGLTexture(const LLImageRaw* raw, bool usemipmaps)
{
	init();
	mUseMipMaps = usemipmaps;
	// Create an empty image of the specified size and width
	mGLTexturep = new LLImageGL(raw, usemipmaps);
}

LLGLTexture::~LLGLTexture()
{
	cleanup();
}

void LLGLTexture::init()
{
	mBoostLevel = LLGLTexture::BOOST_NONE;

	mFullWidth = 0;
	mFullHeight = 0;
	mTexelsPerImage = 0;
	mUseMipMaps = false;
	mComponents = 0;

	mTextureState = NO_DELETE;
	mDontDiscard = false;
	mNeedsGLTexture = false;
}

void LLGLTexture::cleanup()
{
	if (mGLTexturep)
	{
		mGLTexturep->cleanup();
	}
}

// virtual
void LLGLTexture::dump()
{
	if (mGLTexturep)
	{
		mGLTexturep->dump();
	}
}

void LLGLTexture::setBoostLevel(S32 level)
{
	if (mBoostLevel != level)
	{
		mBoostLevel = level;
		if (mBoostLevel != LLGLTexture::BOOST_NONE)
		{
			setNoDelete();
		}
	}
}

void LLGLTexture::forceActive()
{
	mTextureState = ACTIVE;
}

//virtual
void LLGLTexture::setActive()
{
	if (mTextureState != NO_DELETE)
	{
		mTextureState = ACTIVE;
	}
}

// Set the texture to stay in memory
void LLGLTexture::setNoDelete()
{
	mTextureState = NO_DELETE;
}

void LLGLTexture::generateGLTexture()
{
	if (mGLTexturep.isNull())
	{
		mGLTexturep = new LLImageGL(mFullWidth, mFullHeight, mComponents,
									mUseMipMaps);
	}
}

LLImageGL* LLGLTexture::getGLImage() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep;
}

bool LLGLTexture::createGLTexture()
{
	if (mGLTexturep.isNull())
	{
		generateGLTexture();
	}

	return mGLTexturep->createGLTexture();
}

bool LLGLTexture::createGLTexture(S32 discard_level, const LLImageRaw* rawimg,
								  S32 usename, bool to_create, S32 category)
{
	llassert(mGLTexturep.notNull());

	bool ret = mGLTexturep->createGLTexture(discard_level, rawimg, usename,
											to_create, category);
	if (ret)
	{
		mFullWidth = mGLTexturep->getCurrentWidth();
		mFullHeight = mGLTexturep->getCurrentHeight();
		mComponents = mGLTexturep->getComponents();
		setTexelsPerImage();
	}

	return ret;
}

void LLGLTexture::setExplicitFormat(S32 internal_format, U32 primary_format,
									U32 type_format, bool swap_bytes)
{
	llassert(mGLTexturep.notNull());

	mGLTexturep->setExplicitFormat(internal_format, primary_format,
								   type_format, swap_bytes);
}

void LLGLTexture::setAddressMode(LLTexUnit::eTextureAddressMode mode)
{
	llassert(mGLTexturep.notNull());
	mGLTexturep->setAddressMode(mode);
}

void LLGLTexture::setFilteringOption(LLTexUnit::eTextureFilterOptions option)
{
	llassert(mGLTexturep.notNull());
	mGLTexturep->setFilteringOption(option);
}

//virtual
S32	LLGLTexture::getWidth(S32 discard_level) const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getWidth(discard_level);
}

//virtual
S32	LLGLTexture::getHeight(S32 discard_level) const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getHeight(discard_level);
}

S32 LLGLTexture::getMaxDiscardLevel() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getMaxDiscardLevel();
}
S32 LLGLTexture::getDiscardLevel() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getDiscardLevel();
}
S8 LLGLTexture::getComponents() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getComponents();
}

U32 LLGLTexture::getTexName() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getTexName();
}

bool LLGLTexture::hasGLTexture() const
{
	return mGLTexturep.notNull() && mGLTexturep->getHasGLTexture();
}

bool LLGLTexture::getBoundRecently() const
{
	return mGLTexturep.notNull() && mGLTexturep->getBoundRecently();
}

LLTexUnit::eTextureType LLGLTexture::getTarget() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getTarget();
}

bool LLGLTexture::setSubImage(const LLImageRaw* rawimg, S32 x_pos, S32 y_pos,
							  S32 width, S32 height)
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->setSubImage(rawimg, x_pos, y_pos, width, height);
}

bool LLGLTexture::setSubImage(const U8* datap, S32 data_width, S32 data_height,
							  S32 x_pos, S32 y_pos, S32 width, S32 height)
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->setSubImage(datap, data_width, data_height,
									x_pos, y_pos, width, height);
}

void LLGLTexture::setGLTextureCreated (bool initialized)
{
	llassert(mGLTexturep.notNull());
	mGLTexturep->setGLTextureCreated (initialized);
}

void LLGLTexture::setCategory(S32 category)
{
	llassert(mGLTexturep.notNull());
	mGLTexturep->setCategory(category);
}

void LLGLTexture::setTexName(U32 name)
{
	llassert(mGLTexturep.notNull());
	mGLTexturep->setTexName(name);
}

void LLGLTexture::setTarget(U32 target, LLTexUnit::eTextureType bind_target)
{
	llassert(mGLTexturep.notNull());
	mGLTexturep->setTarget(target, bind_target);
}

LLTexUnit::eTextureAddressMode LLGLTexture::getAddressMode() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getAddressMode();
}

S32 LLGLTexture::getTextureMemory() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->mTextureMemory;
}

U32 LLGLTexture::getPrimaryFormat() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getPrimaryFormat();
}

bool LLGLTexture::getIsAlphaMask() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getIsAlphaMask();
}

bool LLGLTexture::getMask(const LLVector2 &tc)
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getMask(tc);
}

F32 LLGLTexture::getTimePassedSinceLastBound()
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getTimePassedSinceLastBound();
}

bool LLGLTexture::getMissed() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->getMissed();
}

bool LLGLTexture::isJustBound() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->isJustBound();
}

void LLGLTexture::forceUpdateBindStats() const
{
	llassert(mGLTexturep.notNull());
	return mGLTexturep->forceUpdateBindStats();
}

void LLGLTexture::destroyGLTexture()
{
	if (mGLTexturep.notNull() && mGLTexturep->getHasGLTexture())
	{
		mGLTexturep->destroyGLTexture();
		mTextureState = DELETED;
	}
}

void LLGLTexture::setTexelsPerImage()
{
	S32 fullwidth = llmin(mFullWidth, (S32)MAX_IMAGE_SIZE_DEFAULT);
	S32 fullheight = llmin(mFullHeight, (S32)MAX_IMAGE_SIZE_DEFAULT);
	mTexelsPerImage = fullwidth * fullheight;
}

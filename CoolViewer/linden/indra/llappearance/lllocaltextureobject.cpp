/**
 * @file lllocaltextureobject.cpp
 * @brief LLLocalTextureObject class implementation
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#include "lllocaltextureobject.h"

#include "llgltexture.h"
#include "llimage.h"
#include "llrender.h"
#include "lltexlayer.h"
#include "llwearable.h"

LLLocalTextureObject::LLLocalTextureObject()
:	mIsBakedReady(false),
	mDiscard(MAX_DISCARD_LEVEL + 1),
	mImage(NULL)
{
}

LLLocalTextureObject::LLLocalTextureObject(LLGLTexture* image,
										   const LLUUID& id)
:	mIsBakedReady(false),
	mDiscard(MAX_DISCARD_LEVEL + 1),
	mImage(image)
{
	gGL.getTexUnit(0)->bind(image);
	mID = id;
}

LLLocalTextureObject::LLLocalTextureObject(const LLLocalTextureObject& lto)
:	mImage(lto.mImage),
	mID(lto.mID),
	mIsBakedReady(lto.mIsBakedReady),
	mDiscard(lto.mDiscard)
{
	U32 num_layers = lto.getNumTexLayers();
	mTexLayers.reserve(num_layers);
	for (U32 index = 0; index < num_layers; ++index)
	{
		LLTexLayer* original_layer = lto.getTexLayer(index);
		if (!original_layer)
		{
			llerrs << "could not clone Local Texture Object: unable to extract texlayer !"
				   << llendl;
			continue;
		}

		LLTexLayer* new_layer = new LLTexLayer(*original_layer);
		new_layer->setLTO(this);
		mTexLayers.push_back(new_layer);
	}
}

LLGLTexture* LLLocalTextureObject::getImage() const
{
	return mImage;
}

LLTexLayer* LLLocalTextureObject::getTexLayer(U32 index) const
{
	if (index >= getNumTexLayers())
	{
		return NULL;
	}

	return mTexLayers[index];
}

LLTexLayer* LLLocalTextureObject::getTexLayer(const std::string& name)
{
	for (tex_layer_vec_t::iterator iter = mTexLayers.begin(),
								   end = mTexLayers.end();
		 iter != end; ++iter)
	{
		LLTexLayer* layer = *iter;
		if (layer && layer->getName().compare(name) == 0)
		{
			return layer;
		}
	}

	return NULL;
}

U32 LLLocalTextureObject::getNumTexLayers() const
{
	return mTexLayers.size();
}

LLUUID LLLocalTextureObject::getID() const
{
	return mID;
}

S32 LLLocalTextureObject::getDiscard() const
{
	return mDiscard;
}

bool LLLocalTextureObject::getBakedReady() const
{
	return mIsBakedReady;
}

void LLLocalTextureObject::setImage(LLGLTexture* new_image)
{
	mImage = new_image;
}

bool LLLocalTextureObject::setTexLayer(LLTexLayer* new_tex_layer, U32 index)
{
	if (index >= getNumTexLayers())
	{
		return false;
	}

	if (new_tex_layer == NULL)
	{
		return removeTexLayer(index);
	}

	LLTexLayer* layer = new LLTexLayer(*new_tex_layer);
	layer->setLTO(this);

	if (mTexLayers[index])
	{
		delete mTexLayers[index];
	}
	mTexLayers[index] = layer;

	return true;
}

bool LLLocalTextureObject::addTexLayer(LLTexLayer* new_tex_layer,
									   LLWearable* wearable)
{
	if (new_tex_layer == NULL)
	{
		return false;
	}

	LLTexLayer* layer = new LLTexLayer(*new_tex_layer, wearable);
	layer->setLTO(this);
	mTexLayers.push_back(layer);
	return true;
}

bool LLLocalTextureObject::addTexLayer(LLTexLayerTemplate* new_tex_layer,
									   LLWearable* wearable)
{
	if (new_tex_layer == NULL)
	{
		return false;
	}

	LLTexLayer* layer = new LLTexLayer(*new_tex_layer, this, wearable);
	layer->setLTO(this);
	mTexLayers.push_back(layer);
	return true;
}

bool LLLocalTextureObject::removeTexLayer(U32 index)
{
	if (index >= getNumTexLayers())
	{
		return false;
	}
	tex_layer_vec_t::iterator iter = mTexLayers.begin();
	iter += index;

	if (*iter)
	{
		delete *iter;
	}
	mTexLayers.erase(iter);
	return true;
}

void LLLocalTextureObject::setID(LLUUID new_id)
{
	mID = new_id;
}

void LLLocalTextureObject::setDiscard(S32 new_discard)
{
	mDiscard = new_discard;
}

void LLLocalTextureObject::setBakedReady(bool ready)
{
	mIsBakedReady = ready;
}

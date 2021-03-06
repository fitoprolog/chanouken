/**
 * @file lllocaltextureobject.h
 * @brief LLLocalTextureObject class header file
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

#ifndef LL_LOCALTEXTUREOBJECT_H
#define LL_LOCALTEXTUREOBJECT_H

#include "llpointer.h"
#include "llgltexture.h"

class LLTexLayer;
class LLTexLayerTemplate;
class LLUUID;
class LLWearable;

// Stores all relevant information for a single texture assumed to have
// ownership of all objects referred to - will delete objects when being
// replaced or if object is destroyed.
class LLLocalTextureObject
{
public:
	LLLocalTextureObject();
	LLLocalTextureObject(LLGLTexture* image, const LLUUID& id);
	LLLocalTextureObject(const LLLocalTextureObject& lto);

	LLGLTexture* getImage() const;
	LLTexLayer* getTexLayer(U32 index) const;
	LLTexLayer* getTexLayer(const std::string& name);
	U32 getNumTexLayers() const;
	LLUUID getID() const;
	S32 getDiscard() const;
	bool getBakedReady() const;

	void setImage(LLGLTexture* new_image);
	bool setTexLayer(LLTexLayer* new_tex_layer, U32 index);
	bool addTexLayer(LLTexLayer* new_tex_layer, LLWearable* wearable);
	bool addTexLayer(LLTexLayerTemplate* new_tex_layer, LLWearable* wearable);
	bool removeTexLayer(U32 index);

	void setID(LLUUID new_id);
	void setDiscard(S32 new_discard);
	void setBakedReady(bool ready);

private:
	LLPointer<LLGLTexture>	mImage;
	LLUUID					mID;
	S32						mDiscard;
	bool					mIsBakedReady;

	// NOTE: LLLocalTextureObject should be the exclusive owner of mTexEntry
	// and mTexLayer; using shared pointers here only for smart assignment &
	// cleanup. Do NOT create new shared pointers to these objects, or keep
	// pointers to them around
	typedef std::vector<LLTexLayer*> tex_layer_vec_t;
	tex_layer_vec_t			mTexLayers;
};

#endif // LL_LOCALTEXTUREOBJECT_H

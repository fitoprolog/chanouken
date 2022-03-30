/**
 * @file lllocalbitmaps.cpp
 * @author Vaalith Jinn, code cleanup by Henri Beauchamp
 * @brief Local Bitmaps source
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 *
 * Copyright (c) 2011, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include <time.h>

#include "boost/filesystem.hpp"

#include "lllocalbitmaps.h"

#include "imageids.h"
#include "lldir.h"
#include "llimagebmp.h"
#include "llimagejpeg.h"
#include "llimagepng.h"
#include "llimagetga.h"
#include "lllocaltextureobject.h"
#include "llnotifications.h"

#include "llagentwearables.h"
#include "llface.h"
#include "llmaterialmgr.h"
#include "llviewerobjectlist.h"
#include "llviewertexturelist.h"
#include "llviewerwearable.h"
#include "llvoavatarself.h"
#include "llvovolume.h"

/*=======================================*/
/*  Formal declarations, constants, etc. */
/*=======================================*/

std::list<LLLocalBitmap*>	LLLocalBitmap::sBitmapList;
S32							LLLocalBitmap::sBitmapsListVersion = 0;
LLLocalBitmapTimer			LLLocalBitmap::sTimer;
bool						LLLocalBitmap::sNeedsRebake = false;

constexpr F32 LL_LOCAL_TIMER_HEARTBEAT = 3.f;
constexpr S32 LL_LOCAL_UPDATE_RETRIES  = 5;

/*=======================================*/
/*  LLLocalBitmap: unit class            */
/*=======================================*/

LLLocalBitmap::LLLocalBitmap(std::string filename)
:	mFilename(filename),
	mShortName(gDirUtilp->getBaseFileName(filename, true)),
	mValid(false),
	mLastModified(),
	mLinkStatus(LS_ON),
	mUpdateRetries(LL_LOCAL_UPDATE_RETRIES)
{
	mTrackingID.generate();

	/* extension */
	std::string temp_exten = gDirUtilp->getExtension(mFilename);
	if (temp_exten == "bmp")
	{
		mExtension = ET_IMG_BMP;
	}
	else if (temp_exten == "tga")
	{
		mExtension = ET_IMG_TGA;
	}
	else if (temp_exten == "jpg" || temp_exten == "jpeg")
	{
		mExtension = ET_IMG_JPG;
	}
	else if (temp_exten == "png")
	{
		mExtension = ET_IMG_PNG;
	}
	else
	{
		llwarns << "File of no valid extension given, local bitmap creation aborted. Filename: "
				<< mFilename << llendl;
		return; // no valid extension.
	}

	// Next phase of unit creation is nearly the same as an update cycle. We
	// are running updateSelf as a special case with the optional UT_FIRSTUSE
	// which omits the parts associated with removing the outdated texture.
	mValid = updateSelf(UT_FIRSTUSE);
}

LLLocalBitmap::~LLLocalBitmap()
{
	// Replace IDs with defaults
	if (mValid && isAgentAvatarValid())
	{
		replaceIDs(mWorldID, IMG_DEFAULT);
		LLLocalBitmap::doRebake();
	}

	// Delete self from gimagelist
	LLViewerFetchedTexture* image = gTextureList.findImage(mWorldID);
	if (image)
	{
		gTextureList.deleteImage(image);
		image->unref();
	}
}

bool LLLocalBitmap::updateSelf(EUpdateType optional_firstupdate)
{
	bool updated = false;

	if (mLinkStatus == LS_ON)
	{
		// Verifying that the file exists
		if (LLFile::exists(mFilename))
		{
			// Verifying that the file has indeed been modified
#if LL_WINDOWS
			boost::filesystem::path p(ll_convert_string_to_wide(mFilename));
#else
			boost::filesystem::path p(mFilename);
#endif
			const std::time_t temp_time = boost::filesystem::last_write_time(p);
			LLSD new_last_modified = asctime(localtime(&temp_time));

			if (mLastModified.asString() != new_last_modified.asString())
			{
				// Loading the image file and decoding it; here is a critical
				// point which, if fails, invalidates the whole update (or unit
				// creation) process.
				LLPointer<LLImageRaw> raw_image = new LLImageRaw();
				if (decodeBitmap(raw_image))
				{
					// Decode is successful, we can safely proceed.
					LLUUID old_id;
					if (optional_firstupdate != UT_FIRSTUSE &&
						mWorldID.notNull())
					{
						old_id = mWorldID;
					}
					mWorldID.generate();
					mLastModified = new_last_modified;

					LLPointer<LLViewerFetchedTexture> texture;
					texture = new LLViewerFetchedTexture("file://" + mFilename,
														 FTT_LOCAL_FILE,
														 mWorldID, true);
					texture->createGLTexture(0, raw_image);
					texture->setCachedRawImage(0, raw_image);
					texture->ref();
					gTextureList.addImage(texture);

					if (optional_firstupdate != UT_FIRSTUSE)
					{
						// seek out everything old_id uses and replace it with
						// mWorldID
						replaceIDs(old_id, mWorldID);

						// remove old_id from gimagelist
						LLViewerFetchedTexture* image;
						image = gTextureList.findImage(old_id);
						if (image)
						{
							gTextureList.deleteImage(image);
							image->unref();
						}
						else
						{
							llwarns_once << "Could not find texture for id: "
										 << old_id << llendl;
						}
					}

					mUpdateRetries = LL_LOCAL_UPDATE_RETRIES;
					updated = true;
				}
				else
				{
					// If decoding failed, we get here and it will attempt to
					// decode it in the next cycles until mUpdateRetries runs
					// out. this is done because some software lock the bitmap
					// while writing to it
					if (mUpdateRetries)
					{
						--mUpdateRetries;
					}
					else
					{
						LLSD notif_args;
						notif_args["FNAME"] = mFilename;
						notif_args["NRETRIES"] = LL_LOCAL_UPDATE_RETRIES;
						gNotifications.add("LocalBitmapsUpdateFailedFinal",
										   notif_args);
						mLinkStatus = LS_BROKEN;
					}
				}
			}

		}
		else
		{
			LLSD notif_args;
			notif_args["FNAME"] = mFilename;
			gNotifications.add("LocalBitmapsUpdateFileNotFound", notif_args);
			mLinkStatus = LS_BROKEN;
		}
	}

	return updated;
}

bool LLLocalBitmap::decodeBitmap(LLPointer<LLImageRaw> rawimg)
{
	bool decode_successful = false;

	switch (mExtension)
	{
		case ET_IMG_BMP:
		{
			LLPointer<LLImageBMP> bmp_image = new LLImageBMP;
			if (bmp_image->load(mFilename) && bmp_image->decode(rawimg))
			{
				rawimg->biasedScaleToPowerOfTwo(LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT);
				decode_successful = true;
			}
			break;
		}

		case ET_IMG_TGA:
		{
			LLPointer<LLImageTGA> tga_image = new LLImageTGA;
			if (tga_image->load(mFilename) && tga_image->decode(rawimg) &&
				(tga_image->getComponents() == 3 ||
				 tga_image->getComponents() == 4))
			{
				rawimg->biasedScaleToPowerOfTwo(LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT);
				decode_successful = true;
			}
			break;
		}

		case ET_IMG_JPG:
		{
			LLPointer<LLImageJPEG> jpeg_image = new LLImageJPEG;
			if (jpeg_image->load(mFilename) &&
				jpeg_image->decode(rawimg))
			{
				rawimg->biasedScaleToPowerOfTwo(LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT);
				decode_successful = true;
			}
			break;
		}

		case ET_IMG_PNG:
		{
			LLPointer<LLImagePNG> png_image = new LLImagePNG;
			if (png_image->load(mFilename) &&
				png_image->decode(rawimg))
			{
				rawimg->biasedScaleToPowerOfTwo(LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT);
				decode_successful = true;
			}
			break;
		}

		default:
		{
			// separating this into -several- llwarns calls because in the
			// extremely unlikely case that this happens, accessing mFilename
			// and any other object properties might very well crash the
			// viewer. Getting here should be impossible, or there's been a
			// pretty serious bug.

			llwarns << "During a decode attempt, the following local bitmap had no properly assigned extension: "
					<< mFilename
					<< ". Disabling further update attempts for this file."
					<< llendl;
			mLinkStatus = LS_BROKEN;
		}
	}

	return decode_successful;
}

void LLLocalBitmap::replaceIDs(const LLUUID& old_id, LLUUID new_id)
{
	// checking for misuse.
	if (old_id == new_id)
	{
		llinfos << "An attempt was made to replace a texture with itself (matching UUIDs): "
				<< old_id.asString() << llendl;
		return;
	}

	// Processing updates per channel; makes the process scalable. The only
	// actual difference is in SetTE* call i.e. SetTETexture, SetTENormal, etc.
	updateUserPrims(old_id, new_id, LLRender::DIFFUSE_MAP);
	updateUserPrims(old_id, new_id, LLRender::NORMAL_MAP);
	updateUserPrims(old_id, new_id, LLRender::SPECULAR_MAP);

	updateUserVolumes(old_id, new_id, LLRender::LIGHT_TEX);
	// Is not there supposed to be an IMG_DEFAULT_SCULPT or something ?
	updateUserVolumes(old_id, new_id, LLRender::SCULPT_TEX);

	// default safeguard image for layers
	if (new_id == IMG_DEFAULT)
	{
		new_id = IMG_DEFAULT_AVATAR;
	}

	// It does not actually update all of those, it merely checks if any of
	// them contains the referenced ID and if so, updates.
	updateUserLayers(old_id, new_id, LLWearableType::WT_ALPHA);
	updateUserLayers(old_id, new_id, LLWearableType::WT_EYES);
	updateUserLayers(old_id, new_id, LLWearableType::WT_GLOVES);
	updateUserLayers(old_id, new_id, LLWearableType::WT_JACKET);
	updateUserLayers(old_id, new_id, LLWearableType::WT_PANTS);
	updateUserLayers(old_id, new_id, LLWearableType::WT_SHIRT);
	updateUserLayers(old_id, new_id, LLWearableType::WT_SHOES);
	updateUserLayers(old_id, new_id, LLWearableType::WT_SKIN);
	updateUserLayers(old_id, new_id, LLWearableType::WT_SKIRT);
	updateUserLayers(old_id, new_id, LLWearableType::WT_SOCKS);
	updateUserLayers(old_id, new_id, LLWearableType::WT_TATTOO);
	updateUserLayers(old_id, new_id, LLWearableType::WT_UNIVERSAL);
	updateUserLayers(old_id, new_id, LLWearableType::WT_UNDERPANTS);
	updateUserLayers(old_id, new_id, LLWearableType::WT_UNDERSHIRT);
}

// This function sorts the faces from a getFaceList[getNumFaces] into a list of
// objects in order to prevent multiple sendTEUpdate calls per object during
// updateUserPrims.
std::vector<LLViewerObject*> LLLocalBitmap::prepUpdateObjects(const LLUUID& old_id,
															  U32 channel)
{
	std::vector<LLViewerObject*> obj_list;
	LLViewerFetchedTexture* old_texture = gTextureList.findImage(old_id);
	if (!old_texture)
	{
		llwarns_once << "Could not find texture for id: " << old_id << llendl;
		return obj_list;
	}

	for (U32 i = 0, count = old_texture->getNumFaces(channel); i < count; ++i)
	{
		// getting an object from a face
		LLFace* facep = (*old_texture->getFaceList(channel))[i];
		if (facep)
		{
			LLViewerObject* objectp = facep->getViewerObject();
			if (objectp)
			{
				// We have an object, we'll take its UUID and compare it to
				// whatever we already have in the returnable object list. If
				// there is a match, we do not add it, to prevent duplicates.
				LLUUID mainlist_obj_id = objectp->getID();
				bool add_object = true;

				// Look for duplicates
				for (std::vector<LLViewerObject*>::iterator
						iter = obj_list.begin(), end = obj_list.end();
					 iter != end; ++iter)
				{
					LLViewerObject* obj = *iter;
					if (obj && obj->getID() == mainlist_obj_id)
					{
						add_object = false; // duplicate found.
						break;
					}
				}

				if (add_object)
				{
					obj_list.push_back(objectp);
				}
			}
		}
	}

	return obj_list;
}

void LLLocalBitmap::updateUserPrims(const LLUUID& old_id, const LLUUID& new_id,
									U32 channel)
{
	std::vector<LLViewerObject*> objectlist = prepUpdateObjects(old_id,
																channel);

	for (std::vector<LLViewerObject*>::iterator iter = objectlist.begin(),
												end = objectlist.end();
		 iter != end; ++iter)
	{
		LLViewerObject* object = *iter;
		if (object)
		{
			bool update_tex = false;
			bool update_mat = false;
			for (U8 i = 0, count = object->getNumFaces(); i < count; ++i)
			{
				if (object->mDrawable)
				{
					LLFace* face = object->mDrawable->getFace(i);
					if (face && face->getTexture(channel) &&
						face->getTexture(channel)->getID() == old_id)
					{
						switch (channel)
						{
							case LLRender::DIFFUSE_MAP:
							{
								object->setTETexture(i, new_id);
								update_tex = true;
								break;
							}

							case LLRender::NORMAL_MAP:
							{
								object->setTENormalMap(i, new_id);
								update_mat = update_tex = true;
								break;
							}

							case LLRender::SPECULAR_MAP:
							{
								object->setTESpecularMap(i, new_id);
								update_mat = update_tex = true;
								break;
							}
						}
					}
				}
			}

			if (update_tex)
			{
				object->sendTEUpdate();
			}
			if (update_mat)
			{
				object->mDrawable->getVOVolume()->faceMappingChanged();
			}
		}
	}
}

void LLLocalBitmap::updateUserVolumes(const LLUUID& old_id,
									  const LLUUID& new_id, U32 channel)
{
	LLViewerFetchedTexture* old_texture = gTextureList.findImage(old_id);
	if (!old_texture)
	{
		llwarns_once << "Could not find texture for id: " << old_id << llendl;
		return;
	}
	if (channel != LLRender::LIGHT_TEX && channel != LLRender::SCULPT_TEX)
	{
		llwarns_once << "Bad texture channel: " << channel << llendl;
		llassert(false);
		return;
	}

	for (U32 i = 0, count = old_texture->getNumVolumes(channel); i < count;
		 ++i)
	{
		LLVOVolume* vovol = (*old_texture->getVolumeList(channel))[i];
		if (!vovol) continue;	// Paranoia

		if (channel == LLRender::LIGHT_TEX)
		{
			if (vovol->getLightTextureID() == old_id)
			{
				vovol->setLightTextureID(new_id);
			}
		}
		else
		{
			LLViewerObject* object = (LLViewerObject*)vovol;
			if (object && object->isSculpted() && object->getVolume() &&
				object->getVolume()->getParams().getSculptID() == old_id)
			{
				const LLSculptParams* params = object->getSculptParams();
				if (!params) continue;

				LLSculptParams new_params(*params);
				new_params.setSculptTexture(new_id, params->getSculptType());
				object->setParameterEntry(LLNetworkData::PARAMS_SCULPT,
										  new_params, true);
			}
		}
	}
}

void LLLocalBitmap::updateUserLayers(const LLUUID& old_id,
									 const LLUUID& new_id,
									 LLWearableType::EType type)
{
	LLViewerWearable* wearable;
	LLAvatarAppearanceDefines::EBakedTextureIndex baked_texind;
	LLAvatarAppearanceDefines::ETextureIndex reg_texind;
	for (U32 i = 0, count = gAgentWearables.getWearableCount(type); i < count;
		 ++i)
	{
		wearable = gAgentWearables.getViewerWearable(type, i);
		if (wearable)
		{
			std::vector<LLLocalTextureObject*> texture_list = wearable->getLocalTextureListSeq();
			for (std::vector<LLLocalTextureObject*>::iterator
					iter = texture_list.begin(), end = texture_list.end();
				 iter != end; ++iter)
			{
				LLLocalTextureObject* lto = *iter;
				if (lto && lto->getID() == old_id)
				{
					// Can't keep that as static const, gives errors, so
					// leaving this var here:
					U32 lti = 0;
					baked_texind = lto->getTexLayer(lti)->getTexLayerSet()->getBakedTexIndex();

					reg_texind = getTexIndex(type, baked_texind);
					if (reg_texind != LLAvatarAppearanceDefines::TEX_NUM_INDICES)
					{
						U32 index;
						if (gAgentWearables.getWearableIndex(wearable, index))
						{
							gAgentAvatarp->setLocalTexture(reg_texind,
														   gTextureList.getImage(new_id),
														   false, index);
							gAgentAvatarp->wearableUpdated(type, false);

							// Telling the manager to rebake once update cycle is
							// fully done:
							LLLocalBitmap::setNeedsRebake();
						}
					}
				}
			}
		}
	}
}

LLAvatarAppearanceDefines::ETextureIndex LLLocalBitmap::getTexIndex(LLWearableType::EType type,
																	LLAvatarAppearanceDefines::EBakedTextureIndex baked_texind)
{
	// Using TEX_NUM_INDICES as a default/fail return
	LLAvatarAppearanceDefines::ETextureIndex result = LLAvatarAppearanceDefines::TEX_NUM_INDICES;

	switch (type)
	{
		case LLWearableType::WT_ALPHA:
		{
			switch (baked_texind)
			{
				case LLAvatarAppearanceDefines::BAKED_EYES:
					result = LLAvatarAppearanceDefines::TEX_EYES_ALPHA;
					break;

				case LLAvatarAppearanceDefines::BAKED_HAIR:
					result = LLAvatarAppearanceDefines::TEX_HAIR_ALPHA;
					break;

				case LLAvatarAppearanceDefines::BAKED_HEAD:
					result = LLAvatarAppearanceDefines::TEX_HEAD_ALPHA;
					break;

				case LLAvatarAppearanceDefines::BAKED_LOWER:
					result = LLAvatarAppearanceDefines::TEX_LOWER_ALPHA;
					break;

				case LLAvatarAppearanceDefines::BAKED_UPPER:
					result = LLAvatarAppearanceDefines::TEX_UPPER_ALPHA;
					break;

				default:
					break;
			}
			break;
		}

		case LLWearableType::WT_EYES:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_EYES)
			{
				result = LLAvatarAppearanceDefines::TEX_EYES_IRIS;
			}

			break;
		}

		case LLWearableType::WT_GLOVES:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_UPPER)
			{
				result = LLAvatarAppearanceDefines::TEX_UPPER_GLOVES;
			}

			break;
		}

		case LLWearableType::WT_JACKET:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_LOWER)
			{
				result = LLAvatarAppearanceDefines::TEX_LOWER_JACKET;
			}
			else if (baked_texind == LLAvatarAppearanceDefines::BAKED_UPPER)
			{
				result = LLAvatarAppearanceDefines::TEX_UPPER_JACKET;
			}

			break;
		}

		case LLWearableType::WT_PANTS:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_LOWER)
			{
				result = LLAvatarAppearanceDefines::TEX_LOWER_PANTS;
			}

			break;
		}

		case LLWearableType::WT_SHIRT:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_UPPER)
			{
				result = LLAvatarAppearanceDefines::TEX_UPPER_SHIRT;
			}

			break;
		}

		case LLWearableType::WT_SHOES:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_LOWER)
			{
				result = LLAvatarAppearanceDefines::TEX_LOWER_SHOES;
			}

			break;
		}

		case LLWearableType::WT_SKIN:
		{
			switch (baked_texind)
			{
				case LLAvatarAppearanceDefines::BAKED_HEAD:
					result = LLAvatarAppearanceDefines::TEX_HEAD_BODYPAINT;
					break;

				case LLAvatarAppearanceDefines::BAKED_LOWER:
					result = LLAvatarAppearanceDefines::TEX_LOWER_BODYPAINT;
					break;

				case LLAvatarAppearanceDefines::BAKED_UPPER:
					result = LLAvatarAppearanceDefines::TEX_UPPER_BODYPAINT;
					break;

				default:
					break;
			}
			break;
		}

		case LLWearableType::WT_SKIRT:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_SKIRT)
			{
				result = LLAvatarAppearanceDefines::TEX_SKIRT;
			}

			break;
		}

		case LLWearableType::WT_SOCKS:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_LOWER)
			{
				result = LLAvatarAppearanceDefines::TEX_LOWER_SOCKS;
			}

			break;
		}

		case LLWearableType::WT_TATTOO:
		{
			switch (baked_texind)
			{
				case LLAvatarAppearanceDefines::BAKED_HEAD:
					result = LLAvatarAppearanceDefines::TEX_HEAD_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_LOWER:
					result = LLAvatarAppearanceDefines::TEX_LOWER_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_UPPER:
					result = LLAvatarAppearanceDefines::TEX_UPPER_TATTOO;
					break;

				default:
					break;
			}
			break;
		}

		case LLWearableType::WT_UNIVERSAL:
		{
			switch (baked_texind)
			{
				case LLAvatarAppearanceDefines::BAKED_HEAD:
					result = LLAvatarAppearanceDefines::TEX_HEAD_UNIVERSAL_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_UPPER:
					result = LLAvatarAppearanceDefines::TEX_UPPER_UNIVERSAL_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_LOWER:
					result = LLAvatarAppearanceDefines::TEX_LOWER_UNIVERSAL_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_HAIR:
					result = LLAvatarAppearanceDefines::TEX_HAIR_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_EYES:
					result = LLAvatarAppearanceDefines::TEX_EYES_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_LEFT_ARM:
					result = LLAvatarAppearanceDefines::TEX_LEFT_ARM_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_LEFT_LEG:
					result = LLAvatarAppearanceDefines::TEX_LEFT_LEG_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_SKIRT:
					result = LLAvatarAppearanceDefines::TEX_SKIRT_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_AUX1:
					result = LLAvatarAppearanceDefines::TEX_AUX1_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_AUX2:
					result = LLAvatarAppearanceDefines::TEX_AUX2_TATTOO;
					break;

				case LLAvatarAppearanceDefines::BAKED_AUX3:
					result = LLAvatarAppearanceDefines::TEX_AUX3_TATTOO;
					break;

				default:
					break;
			}
			break;
		}

		case LLWearableType::WT_UNDERPANTS:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_LOWER)
			{
				result = LLAvatarAppearanceDefines::TEX_LOWER_UNDERPANTS;
			}

			break;
		}

		case LLWearableType::WT_UNDERSHIRT:
		{
			if (baked_texind == LLAvatarAppearanceDefines::BAKED_UPPER)
			{
				result = LLAvatarAppearanceDefines::TEX_UPPER_UNDERSHIRT;
			}

			break;
		}

		default:
		{
			llwarns << "Unknown wearable type: " << (S32)type
				    << " - Baked texture index: " << (S32)baked_texind
					<< " - Filename: " << mFilename
					<< " - TrackingID: " << mTrackingID
					<< " - InworldID: " << mWorldID << llendl;
		}
	}

	return result;
}

//static
void LLLocalBitmap::addUnitsCallback(HBFileSelector::ELoadFilter type,
									std::deque<std::string>& files, void*)
{
	bool updated = false;

	while (!files.empty())
	{
		std::string filename = files.front();
		files.pop_front();

		if (!filename.empty())
		{
			sTimer.stopTimer();

			LLLocalBitmap* unit = new LLLocalBitmap(filename);
			if (unit->getValid())
			{
				sBitmapList.push_back(unit);
				updated = true;
			}
			else
			{
				LLSD notif_args;
				notif_args["FNAME"] = filename;
				gNotifications.add("LocalBitmapsVerifyFail", notif_args);
				delete unit;
			}

			sTimer.startTimer();
		}
	}

	if (updated)
	{
		++sBitmapsListVersion;
	}
}

//static
void LLLocalBitmap::cleanupClass()
{
	std::for_each(sBitmapList.begin(), sBitmapList.end(), DeletePointer());
	sBitmapList.clear();
}

//static
void LLLocalBitmap::addUnits()
{
	HBFileSelector::loadFiles(HBFileSelector::FFLOAD_IMAGE, addUnitsCallback);
}

//static
void LLLocalBitmap::delUnit(LLUUID& tracking_id)
{
	if (!sBitmapList.empty())
	{
		// Find which ones we want deleted and make a separate list
		std::vector<LLLocalBitmap*> to_delete;
		for (local_list_iter iter = sBitmapList.begin(),
							 end = sBitmapList.end();
			 iter != end; ++iter)
		{
			LLLocalBitmap* unit = *iter;
			if (unit->getTrackingID() == tracking_id)
			{
				to_delete.push_back(unit);
			}
		}

		// Delete the corresponding bitmaps
		bool updated = false;
		for (std::vector<LLLocalBitmap*>::iterator iter = to_delete.begin(),
												   end = to_delete.end();
			 iter != end; ++iter)
		{
			LLLocalBitmap* unit = *iter;
			sBitmapList.remove(unit);
			delete unit;
			updated = true;
		}

		if (updated)
		{
			++sBitmapsListVersion;
		}
	}
}

//static
LLUUID LLLocalBitmap::getWorldID(const LLUUID& tracking_id)
{
	for (local_list_iter iter = sBitmapList.begin(),
						 end = sBitmapList.end();
		 iter != end; ++iter)
	{
		LLLocalBitmap* unit = *iter;
		if (unit && unit->getTrackingID() == tracking_id)
		{
			return unit->getWorldID();
		}
	}

	return LLUUID::null;
}

//static
bool LLLocalBitmap::isLocal(const LLUUID& world_id)
{
	for (local_list_iter iter = sBitmapList.begin(),
						 end = sBitmapList.end();
		 iter != end; ++iter)
	{
		LLLocalBitmap* unit = *iter;
		if (unit && unit->getWorldID() == world_id)
		{
			return true;
		}
	}

	return false;
}

//static
std::string LLLocalBitmap::getFilename(const LLUUID& tracking_id)
{
	for (local_list_iter iter = sBitmapList.begin(), end = sBitmapList.end();
		 iter != end; ++iter)
	{
		LLLocalBitmap* unit = *iter;
		if (unit && unit->getTrackingID() == tracking_id)
		{
			return unit->getFilename();
		}
	}

	return LLStringUtil::null;
}

//static
void LLLocalBitmap::doUpdates()
{
	// Preventing theoretical overlap in cases of huge number of loaded images.
	sTimer.stopTimer();
	sNeedsRebake = false;

	for (local_list_iter iter = sBitmapList.begin(), end = sBitmapList.end();
		 iter != end; ++iter)
	{
		(*iter)->updateSelf();
	}

	doRebake();
	sTimer.startTimer();
}

//static
void LLLocalBitmap::setNeedsRebake()
{
	sNeedsRebake = true;
}

// Separated that from doUpdates to insure a rebake can be called separately
// during deletion
//static
void LLLocalBitmap::doRebake()
{
	if (sNeedsRebake)
	{
		gAgentAvatarp->forceBakeAllTextures(true);
		sNeedsRebake = false;
	}
}

/*=======================================*/
/*  LLLocalBitmapTimer: timer class      */
/*=======================================*/
LLLocalBitmapTimer::LLLocalBitmapTimer()
:	LLEventTimer(LL_LOCAL_TIMER_HEARTBEAT)
{
}

void LLLocalBitmapTimer::startTimer()
{
	mEventTimer.start();
}

void LLLocalBitmapTimer::stopTimer()
{
	mEventTimer.stop();
}

bool LLLocalBitmapTimer::isRunning()
{
	return mEventTimer.getStarted();
}

bool LLLocalBitmapTimer::tick()
{
	LLLocalBitmap::doUpdates();
	return false;
}

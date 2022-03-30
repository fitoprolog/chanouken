/**
 * @file llviewerwearable.cpp
 * @brief LLViewerWearable class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llviewerwearable.h"

#include "imageids.h"
#include "llassetstorage.h"
#include "lldir.h"
#include "lllocaltextureobject.h"
#include "llmd5.h"
#include "llnotifications.h"
#include "llvisualparam.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llfloatercustomize.h"
#include "llinventorymodel.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewertexlayer.h"
#include "llviewertexturelist.h"
#include "llvoavatarpuppet.h"
#include "llvoavatarself.h"

using namespace LLAvatarAppearanceDefines;

// static variables
std::map<ETextureIndex, LLUUID> LLViewerWearable::sCachedTextures;
bool LLWearableSaveData::sResetCOFTimer = false;
U32 LLWearableSaveData::sSavedWearableCount = 0;

// Support classes

LLWearableSaveData::LLWearableSaveData(LLWearableType::EType type)
:	mType(type)
{
	mResetCOFTimer = sResetCOFTimer;
	if (mResetCOFTimer)
	{
		gAppearanceMgr.resetCOFUpdateTimer();
		++sSavedWearableCount;
	}
}

LLWearableSaveData::~LLWearableSaveData()
{
	if (mResetCOFTimer && sSavedWearableCount > 0)
	{
		gAppearanceMgr.resetCOFUpdateTimer();
		--sSavedWearableCount;
	}
}

class LLOverrideBakedTextureUpdate
{
public:
	LLOverrideBakedTextureUpdate(bool temp_state)
	{
		U32 num_bakes = (U32)LLAvatarAppearanceDefines::BAKED_NUM_INDICES;
		for (U32 index = 0; index < num_bakes; ++index)
		{
			composite_enabled[index] = gAgentAvatarp->isCompositeUpdateEnabled(index);
		}
		gAgentAvatarp->setCompositeUpdatesEnabled(temp_state);
	}

	~LLOverrideBakedTextureUpdate()
	{
		U32 num_bakes = (U32)LLAvatarAppearanceDefines::BAKED_NUM_INDICES;
		for (U32 index = 0; index < num_bakes; ++index)
		{
			gAgentAvatarp->setCompositeUpdatesEnabled(index,
													  composite_enabled[index]);
		}
	}

private:
	bool composite_enabled[LLAvatarAppearanceDefines::BAKED_NUM_INDICES];
};

// Helper function
static std::string asset_id_to_filename(const LLUUID& asset_id)
{
	std::string fname = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
													   asset_id.asString());
	return fname + ".wbl";
}

LLViewerWearable::LLViewerWearable(const LLTransactionID& transaction_id)
:	LLWearable(),
	mTransactionID(transaction_id),
	mVolatile(false)
{
	mType = LLWearableType::WT_INVALID;
	mAssetID = mTransactionID.makeAssetID(gAgent.getSecureSessionID());
}

LLViewerWearable::LLViewerWearable(const LLAssetID& asset_id)
:	LLWearable(),
	mAssetID(asset_id),
	mVolatile(false)
{
	mType = LLWearableType::WT_INVALID;
	mTransactionID.setNull();
}

//virtual
LLWearable::EImportResult LLViewerWearable::importStream(std::istream& input_stream,
														 LLAvatarAppearance* avatarp)
{
	// Suppress texlayerset updates while wearables are being imported.
	// Layersets will be updated when the wearables are "worn", not loaded.
	// Note state will be restored when this object is destroyed.
	LLOverrideBakedTextureUpdate stop_bakes(false);

	LLWearable::EImportResult result = LLWearable::importStream(input_stream,
																avatarp);
	if (result == LLWearable::FAILURE)
	{
		return result;
	}

	if (result == LLWearable::BAD_HEADER)
	{
		// Should not really log the asset id for security reasons, but we need
		// it in this case.
		llwarns << "Bad Wearable asset header: " << mAssetID << llendl;
		return result;
	}

	LLStringUtil::truncate(mName, DB_INV_ITEM_NAME_STR_LEN);
	LLStringUtil::truncate(mDescription, DB_INV_ITEM_DESC_STR_LEN);

	return result;
}

// Avatar parameter and texture definitions can change over time. This function
// returns true if parameters or textures have been added or removed since this
// wearable was created.
bool LLViewerWearable::isOldVersion() const
{
	if (!isAgentAvatarValid())
	{
		return false;
	}

	if (LLWearable::sCurrentDefinitionVersion < mDefinitionVersion)
	{
		llwarns << "Wearable asset has newer version (" << mDefinitionVersion
				<< ") than XML (" << LLWearable::sCurrentDefinitionVersion
				<< ")" << llendl;
		llassert(0);
	}

	if (LLWearable::sCurrentDefinitionVersion != mDefinitionVersion)
	{
		return true;
	}

	S32 param_count = 0;
	for (LLViewerVisualParam* param = (LLViewerVisualParam*)gAgentAvatarp->getFirstVisualParam();
		 param; param = (LLViewerVisualParam*)gAgentAvatarp->getNextVisualParam())
	{
		if (param->getWearableType() == mType && param->isTweakable())
		{
			++param_count;
			if (!mVisualParamIndexMap.count(param->getID()))
			{
				return true;
			}
		}
	}
	if (param_count != (S32)mVisualParamIndexMap.size())
	{
		return true;
	}

	S32 te_count = 0;
	for (S32 te = 0; te < TEX_NUM_INDICES; ++te)
	{
		if (LLAvatarAppearanceDictionary::getTEWearableType((ETextureIndex)te) == mType)
		{
			++te_count;
			if (!mTEMap.count(te))
			{
				return true;
			}
		}
	}

	return te_count != (S32)mTEMap.size();
}

// Avatar parameter and texture definitions can change over time.
// * If parameters or textures have been REMOVED since the wearable was
//   created, they're just ignored, so we consider the wearable clean even
//   though isOldVersion() will return true.
// * If parameters or textures have been ADDED since the wearable was created,
//   they are taken to have default values, so we consider the wearable clean
//   only if those values are the same as the defaults.
bool LLViewerWearable::isDirty() const
{
	if (!isAgentAvatarValid())
	{
		return false;
	}

	for (LLViewerVisualParam* param = (LLViewerVisualParam*)gAgentAvatarp->getFirstVisualParam();
		 param;
		 param = (LLViewerVisualParam*)gAgentAvatarp->getNextVisualParam())
	{
		if (param->getWearableType() == mType && param->isTweakable() &&
			!param->getCrossWearable())
		{
			F32 current_weight = getVisualParamWeight(param->getID());
			current_weight = llclamp(current_weight, param->getMinWeight(),
									 param->getMaxWeight());
			F32 saved_weight = get_if_there(mSavedVisualParamMap,
											param->getID(),
											param->getDefaultWeight());
			saved_weight = llclamp(saved_weight, param->getMinWeight(),
								   param->getMaxWeight());

			U8 a = F32_to_U8(saved_weight, param->getMinWeight(),
							 param->getMaxWeight());
			U8 b = F32_to_U8(current_weight, param->getMinWeight(),
							 param->getMaxWeight());
			if (a != b)
			{
				return true;
			}
		}
	}

	te_map_t::const_iterator te_map_end = mTEMap.end();
	te_map_t::const_iterator saved_map_end = mSavedTEMap.end();
	for (S32 te = 0; te < TEX_NUM_INDICES; ++te)
	{
		if (LLAvatarAppearanceDictionary::getTEWearableType((ETextureIndex)te) == mType)
		{
			te_map_t::const_iterator current_iter = mTEMap.find(te);
			if (current_iter != te_map_end)
			{
 				const LLUUID& current_image_id = current_iter->second->getID();
				te_map_t::const_iterator saved_iter = mSavedTEMap.find(te);
				if (saved_iter != saved_map_end)
				{
					const LLUUID& saved_image_id = saved_iter->second->getID();
					if (saved_image_id != current_image_id)
					{
						// saved vs current images are different, wearable is
						// dirty
						return true;
					}
				}
				else
				{
					// image found in current image list but not saved image
					// list
					return true;
				}
			}
		}
	}

	return false;
}

void LLViewerWearable::setParamsToDefaults()
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	for (LLVisualParam* param = gAgentAvatarp->getFirstVisualParam(); param;
		 param = gAgentAvatarp->getNextVisualParam())
	{
		if (((LLViewerVisualParam*)param)->getWearableType() == mType &&
			param->isTweakable())
		{
			setVisualParamWeight(param->getID(), param->getDefaultWeight(),
								 false);
		}
	}
}

void LLViewerWearable::setTexturesToDefaults()
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	te_map_t::const_iterator te_map_end = mTEMap.end();
	for (S32 te = 0; te < TEX_NUM_INDICES; ++te)
	{
		if (LLAvatarAppearanceDictionary::getTEWearableType((ETextureIndex)te) == mType)
		{
			LLUUID id = getDefaultTextureImageID((ETextureIndex)te);
			LLViewerFetchedTexture* image = LLViewerTextureManager::getFetchedTexture(id);
			if (mTEMap.find(te) == te_map_end)
			{
				mTEMap[te] = new LLLocalTextureObject(image, id);
				createLayers(te, gAgentAvatarp);
			}
			else
			{
				// Local Texture Object already created, just set image and
				// UUID
				LLLocalTextureObject* lto = mTEMap[te];
				lto->setID(id);
				lto->setImage(image);
			}
		}
	}
}

//virtual
LLUUID LLViewerWearable::getDefaultTextureImageID(ETextureIndex index)
{
	std::map<ETextureIndex, LLUUID>::iterator it = sCachedTextures.find(index);
	if (it != sCachedTextures.end())
	{
		return it->second;
	}

	LLUUID tex_id = IMG_DEFAULT_AVATAR;
	const LLAvatarAppearanceDictionary::TextureEntry* dict =
		gAvatarAppDictp->getTexture(index);
	if (dict)
	{
		const std::string& default_image = dict->mDefaultImageName;
		if (!default_image.empty())
		{
			tex_id = LLUUID(gSavedSettings.getString(default_image.c_str()));
		}
	}

	// Cache this texture id for future usage
	sCachedTextures[index] = tex_id;

	return tex_id;
}

// Updates the user's avatar's appearance
//virtual
void LLViewerWearable::writeToAvatar(LLAvatarAppearance* avatarp)
{
	if (!avatarp)
	{
		return;
	}

	LLVOAvatarSelf* self_av = NULL;
	if (avatarp == (LLAvatarAppearance*)gAgentAvatarp)
	{
		if (!gAgentAvatarp->isValid())
		{
			return;
		}
		self_av = gAgentAvatarp;
#if 0
		// FIXME: kludgy way to avoid overwriting avatar state from wearables.
		// Ideally would avoid calling this func in the first place.
		if (self_av->isUsingServerBakes() &&
			!self_av->isUsingLocalAppearance())
		{
			return;
		}
#endif
	}
#if LL_ANIMESH_VPARAMS
	else if (!avatarp->isPuppetAvatar() || avatarp->isDead())
#else
	else
#endif
	{
		return;
	}

	ESex old_sex = avatarp->getSex();

	LLWearable::writeToAvatar(avatarp);

#if LL_ANIMESH_VPARAMS
	if (!self_av)
	{
		return;
	}
#endif

	// Pull texture entries
	te_map_t::const_iterator te_map_end = mTEMap.end();
	for (S32 te = 0; te < TEX_NUM_INDICES; ++te)
	{
		if (LLAvatarAppearanceDictionary::getTEWearableType((ETextureIndex)te) == mType)
		{
			te_map_t::const_iterator iter = mTEMap.find(te);
			LLUUID image_id;
			if (iter != te_map_end)
			{
				image_id = iter->second->getID();
			}
			else
			{
				image_id = getDefaultTextureImageID((ETextureIndex)te);
			}
			LLViewerTexture* image =
				LLViewerTextureManager::getFetchedTexture(image_id,
														  FTT_DEFAULT,
														  true,
														  LLGLTexture::BOOST_NONE,
														  LLViewerTexture::LOD_TEXTURE);
			// MULTI-WEARABLE: assume index 0 will be used when writing to
			// avatar. TODO: eliminate the need for this.
			self_av->setLocalTextureTE(te, image, 0);
		}
	}

	ESex new_sex = gAgentAvatarp->getSex();
	if (old_sex != new_sex)
	{
		self_av->updateSexDependentLayerSets(false);
	}
}

// Updates the user's avatar's appearance, replacing this wearables' parameters
// and textures with default values.
//static
void LLViewerWearable::removeFromAvatar(LLWearableType::EType type,
										bool upload_bake)
{
	if (!isAgentAvatarValid())
	{
		return;
	}
	// You cannot just remove body parts.
	if (type == LLWearableType::WT_SHAPE || type == LLWearableType::WT_SKIN ||
		type == LLWearableType::WT_HAIR || type == LLWearableType::WT_EYES)
	{
		return;
	}

	// Pull params
	for (LLVisualParam* param = gAgentAvatarp->getFirstVisualParam(); param;
		 param = gAgentAvatarp->getNextVisualParam())
	{
		if (((LLViewerVisualParam*)param)->getWearableType() == type &&
			param->isTweakable())
		{
			S32 param_id = param->getID();
			gAgentAvatarp->setVisualParamWeight(param_id,
												param->getDefaultWeight(),
												upload_bake);
		}
	}

	if (gFloaterCustomizep)
	{
		gFloaterCustomizep->updateWearableType(type, NULL);
	}

	gAgentAvatarp->updateVisualParams();
	gAgentAvatarp->wearableUpdated(type, false);
}

// Does not copy mAssetID.
// Definition version is current: removes obsolete enties and creates default
// values for new ones.
void LLViewerWearable::copyDataFrom(const LLViewerWearable* src)
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	mDefinitionVersion = LLWearable::sCurrentDefinitionVersion;

	mName = src->mName;
	mDescription = src->mDescription;
	mPermissions = src->mPermissions;
	mSaleInfo = src->mSaleInfo;

	setType(src->mType, gAgentAvatarp);

	mSavedVisualParamMap.clear();
	// Deep copy of mSavedVisualParamMap (copies only those params that are
	// current, filling in defaults where needed)
	for (LLViewerVisualParam* param = (LLViewerVisualParam*)gAgentAvatarp->getFirstVisualParam();
		param;
		param = (LLViewerVisualParam*)gAgentAvatarp->getNextVisualParam())
	{
		if (param->getWearableType() == mType)
		{
			S32 id = param->getID();
			F32 weight = src->getVisualParamWeight(id);
			mSavedVisualParamMap[id] = weight;
		}
	}

	destroyTextures();
	// Deep copy of mTEMap (copies only those tes that are current, filling in
	// defaults where needed)
	LLUUID image_id;
	te_map_t::const_iterator te_map_end = src->mTEMap.end();
	for (S32 te = 0; te < TEX_NUM_INDICES; ++te)
	{
		if (LLAvatarAppearanceDictionary::getTEWearableType((ETextureIndex)te) == mType)
		{
			LLViewerFetchedTexture* image = NULL;
			te_map_t::const_iterator iter = src->mTEMap.find(te);
			if (iter != te_map_end)
			{
				const LLLocalTextureObject* lto = src->getLocalTextureObject(te);
				if (!lto)	// Paranoia
				{
					llwarns << "NULL local texture object for texture entry "
							<< te << llendl;
					continue;
				}
				image_id = lto->getID();

				LLGLTexture* gltex = lto->getImage();
				if (!gltex)	// Paranoia
				{
					llwarns << "NULL local texture for " << image_id
							<< llendl;
					continue;
				}

				image = gltex->asFetched();
				if (!image)	// Paranoia
				{
					llwarns << "NULL fetched texture for " << image_id
							<< llendl;
					continue;
				}

				mTEMap[te] = new LLLocalTextureObject(image, image_id);
				mSavedTEMap[te] = new LLLocalTextureObject(image, image_id);
				mTEMap[te]->setBakedReady(lto->getBakedReady());
				mTEMap[te]->setDiscard(lto->getDiscard());
			}
			else
			{
				image_id = getDefaultTextureImageID((ETextureIndex)te);
				image = LLViewerTextureManager::getFetchedTexture(image_id);
				if (!image)
				{
					llwarns << "NULL fetched texture for " << image_id
							<< llendl;
					continue;
				}
				mTEMap[te] = new LLLocalTextureObject(image, image_id);
				mSavedTEMap[te] = new LLLocalTextureObject(image, image_id);
			}
			createLayers(te, gAgentAvatarp);
		}
	}

	// Probably reduntant, but ensure that the newly created wearable is not
	// dirty by setting current value of params in new wearable to be the same
	// as the saved values (which were loaded from src at param->cloneParam(this))
	revertValuesWithoutUpdate();
}

void LLViewerWearable::setItemID(const LLUUID& item_id)
{
	mItemID = item_id;
}

void LLViewerWearable::revertValues()
{
	LLWearable::revertValues();

	if (gFloaterCustomizep)
	{
		LLFloaterCustomize::setCurrentWearableType(mType);
		gFloaterCustomizep->updateScrollingPanelUI();
	}
}

void LLViewerWearable::saveValues()
{
	LLWearable::saveValues();

	if (gFloaterCustomizep)
	{
		LLFloaterCustomize::setCurrentWearableType(mType);
		gFloaterCustomizep->updateScrollingPanelUI();
	}
}

//virtual
void LLViewerWearable::setUpdated() const
{
	gInventory.addChangedMask(LLInventoryObserver::LABEL, getItemID());
}

void LLViewerWearable::refreshName()
{
	LLUUID item_id = getItemID();
	LLInventoryItem* item = gInventory.getItem(item_id);
	if (item)
	{
		mName = item->getName();
	}
}

// virtual
void LLViewerWearable::addToBakedTextureHash(LLMD5& hash) const
{
	LLUUID asset_id = getAssetID();
	hash.update((const unsigned char*)asset_id.mData, UUID_BYTES);
}

void LLViewerWearable::saveNewAsset() const
{
	const std::string filename = asset_id_to_filename(mAssetID);
	if (!exportFile(filename))
	{
		llwarns << "Unable to save '" << mName << "' to wearable file."
				<< llendl;
		LLSD args;
		args["NAME"] = mName;
		gNotifications.add("CannotSaveWearableOutOfSpace", args);
		return;
	}

	// save it out to database
	if (gAssetStoragep)
	{
	 	LLWearableSaveData* data = new LLWearableSaveData(mType);
		gAssetStoragep->storeAssetData(filename, mTransactionID,
									   getAssetType(), onSaveNewAssetComplete,
									   (void*)data);
	}
}

// StoreAssetData callback (fixed)
//static
void LLViewerWearable::onSaveNewAssetComplete(const LLUUID& new_asset_id,
											  void* userdata, S32 status,
											  LLExtStat ext_status)
{
	LLWearableSaveData* data = (LLWearableSaveData*)userdata;
	const std::string& type_name = LLWearableType::getTypeName(data->mType);
	if (status == 0)
	{
		// Success
		llinfos << "Saved wearable " << type_name << llendl;
	}
	else
	{
		std::string buffer = llformat("Unable to save %s to central asset store.",
									  type_name.c_str());
		llwarns << buffer << " Status: " << status << llendl;
		LLSD args;
		args["NAME"] = type_name;
		gNotifications.add("CannotSaveToAssetStore", args);
	}

	// Delete temp file
	const std::string src_filename = asset_id_to_filename(new_asset_id);
	LLFile::remove(src_filename);

	// Delete the context data
	delete data;
}

std::ostream& operator<<(std::ostream& s, const LLViewerWearable& w)
{
	s << "wearable " << LLWearableType::getTypeName(w.mType) << "\n";
	s << "    Name: " << w.mName << "\n";
	s << "    Desc: " << w.mDescription << "\n";
	//w.mPermissions
	//w.mSaleInfo

	s << "    Params:" << "\n";
	for (LLWearable::visual_param_index_map_t::const_iterator
			iter = w.mVisualParamIndexMap.begin(),
			end = w.mVisualParamIndexMap.end();
		 iter != end; ++iter)
	{
		S32 param_id = iter->first;
		LLVisualParam* wearable_param = iter->second;
		F32 param_weight = wearable_param->getWeight();
		s << "        " << param_id << " " << param_weight << "\n";
	}

	s << "    Textures:" << "\n";
	for (LLViewerWearable::te_map_t::const_iterator iter = w.mTEMap.begin(),
													end = w.mTEMap.end();
		 iter != end; ++iter)
	{
		S32 te = iter->first;
		const LLUUID& image_id = iter->second->getID();
		s << "        " << te << " " << image_id << "\n";
	}
	return s;
}

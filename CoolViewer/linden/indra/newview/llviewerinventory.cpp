/**
 * @file llviewerinventory.cpp
 * @brief Implementation of the viewer side inventory objects.
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

#include "llviewerinventory.h"

#include "llcallbacklist.h"
#include "llcorehttputil.h"
#include "llnotifications.h"
#include "llsdutil.h"
#include "llmessage.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "llaisapi.h"
#include "llappearancemgr.h"
#include "llcommandhandler.h"
#include "llfloaterinventory.h"
#include "llfolderview.h"
#include "llgesturemgr.h"
#include "llinventorybridge.h"
#include "llinventorymodel.h"
#include "llinventorymodelfetch.h"
#include "llmarketplacefunctions.h"
#include "llviewercontrol.h"
#include "llviewerfoldertype.h"
#include "llpreviewgesture.h"
#include "llviewermessage.h"			// For open_inventory_offer()
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"

// Do-nothing ops for use in callbacks:

void no_op_inventory_func(const LLUUID&)
{
}

void no_op_llsd_func(const LLSD&)
{
}

void no_op()
{
}

#if 1	// *TODO: LLInventoryCallback should be deprecated to conform to the
		// new boost::bind/coroutine model. This is temporary/transition code.
void doInventoryCb(LLPointer<LLInventoryCallback> cb, LLUUID id)
{
	if (cb.notNull())
	{
		cb->fire(id);
	}
}
#endif

// Command handler

class LLInventoryHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLInventoryHandler() : LLCommandHandler("inventory", UNTRUSTED_THROTTLE) {}

	bool handle(const LLSD& params, const LLSD& query_map,
				LLMediaCtrl* web)
	{
		if (params.size() < 1)
		{
			return false;
		}

		// Support secondlife:///app/inventory/show
		if (params[0].asString() == "show")
		{
			LLFloaterInventory::showAgentInventory();
			return true;
		}

		// Otherwise, we need a UUID and a verb...
		if (params.size() < 2)
		{
			return false;
		}
		LLUUID inventory_id;
		if (!inventory_id.set(params[0], false))
		{
			return false;
		}

		const std::string verb = params[1].asString();
		if (verb == "select")
		{
			uuid_vec_t items_to_open;
			items_to_open.emplace_back(inventory_id);
			// inventory_handler is just a stub, because we don't know from who
			// this offer
			open_inventory_offer(items_to_open, "inventory_handler");
			return true;
		}

		return false;
	}
};
LLInventoryHandler gInventoryHandler;

//----------------------------------------------------------------------------
/// Class LLViewerInventoryItem
///----------------------------------------------------------------------------

LLViewerInventoryItem::LLViewerInventoryItem(const LLUUID& uuid,
											 const LLUUID& parent_uuid,
											 const LLPermissions& perm,
											 const LLUUID& asset_uuid,
											 LLAssetType::EType type,
											 LLInventoryType::EType inv_type,
											 const std::string& name,
											 const std::string& desc,
											 const LLSaleInfo& sale_info,
											 U32 flags,
											 time_t creation_date_utc)
:	LLInventoryItem(uuid, parent_uuid, perm, asset_uuid, type, inv_type,
					name, desc, sale_info, flags, creation_date_utc),
	mIsComplete(true)
{
}

LLViewerInventoryItem::LLViewerInventoryItem(const LLUUID& item_id,
											 const LLUUID& parent_id,
											 const std::string& name,
											 LLInventoryType::EType inv_type)
:	LLInventoryItem(),
	mIsComplete(false)
{
	mUUID = item_id;
	mParentUUID = parent_id;
	mInventoryType = inv_type;
	mName = name;
}

LLViewerInventoryItem::LLViewerInventoryItem()
:	LLInventoryItem(),
	mIsComplete(false)
{
}

LLViewerInventoryItem::LLViewerInventoryItem(const LLViewerInventoryItem* other)
:	LLInventoryItem()
{
	copyViewerItem(other);
	if (!mIsComplete)
	{
		llwarns << "Copy constructor for incomplete item: " << mUUID << llendl;
	}
}

LLViewerInventoryItem::LLViewerInventoryItem(const LLInventoryItem* other)
:	LLInventoryItem(other),
	mIsComplete(true)
{
}

void LLViewerInventoryItem::copyViewerItem(const LLViewerInventoryItem* other)
{
	LLInventoryItem::copyItem(other);
	mIsComplete = other->mIsComplete;
	mTransactionID = other->mTransactionID;
}

//virtual
void LLViewerInventoryItem::copyItem(const LLInventoryItem* other)
{
	LLInventoryItem::copyItem(other);
	mIsComplete = true;
	mTransactionID.setNull();
}

void LLViewerInventoryItem::cloneViewerItem(LLPointer<LLViewerInventoryItem>& newitem) const
{
	newitem = new LLViewerInventoryItem(this);
	if (newitem.notNull())
	{
		LLUUID item_id;
		item_id.generate();
		newitem->setUUID(item_id);
	}
}

void LLViewerInventoryItem::updateServer(bool is_new) const
{
	if (!mIsComplete)
	{
		llwarns << "Incomplete item" << llendl;
	 	gNotifications.add("IncompleteInventoryItem");
		return;
	}
	if (gAgentID != mPermissions.getOwner())
	{
		// *FIX: deal with this better.
		llwarns << "Unowned item:\n" << ll_pretty_print_sd(this->asLLSD())
				<< llendl;
		return;
	}

	if (AISAPI::isAvailable())
	{
		LL_DEBUGS("Inventory") << "Updating item via AIS: " << mUUID
							   << LL_ENDL;
		LLSD updates = asLLSD();
		// Replace asset_id and/or shadow_id with transaction_id (hash_id)
		if (updates.has("asset_id"))
		{
			updates.erase("asset_id");
			if (mTransactionID.notNull())
			{
				updates["hash_id"] = mTransactionID;
			}
		}
		if (updates.has("shadow_id"))
		{
			updates.erase("shadow_id");
			if (mTransactionID.notNull())
			{
				updates["hash_id"] = mTransactionID;
			}
		}
		AISAPI::completion_t cr = boost::bind(&doInventoryCb,
											  (LLPointer<LLInventoryCallback>)NULL,
											  _1);
		AISAPI::UpdateItem(mUUID, updates, cr);
	}
	else
	{
		LLInventoryModel::LLCategoryUpdate up(mParentUUID, is_new ? 1 : 0);
		gInventory.accountForUpdate(up);

		LL_DEBUGS("Inventory") << "Updating item: " << mUUID << LL_ENDL;
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_UpdateInventoryItem);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->addUUIDFast(_PREHASH_TransactionID, mTransactionID);
		msg->nextBlockFast(_PREHASH_InventoryData);
		msg->addU32Fast(_PREHASH_CallbackID, 0);
		packMessage(msg);
		gAgent.sendReliableMessage();
	}
}

void LLViewerInventoryItem::fetchFromServer() const
{
	if (mIsComplete)
	{
		// This should not happen
		llwarns << "Request to fetch complete item " << mUUID << llendl;
		return;
	}

	const std::string& url = mPermissions.getOwner() != gAgentID ?
							 gAgent.getRegionCapability("FetchLib2") :
							 gAgent.getRegionCapability("FetchInventory2");
	if (url.empty())
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage("FetchInventory");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgentID);
		msg->addUUID("SessionID", gAgentSessionID);
		msg->nextBlock("InventoryData");
		msg->addUUID("OwnerID", mPermissions.getOwner());
		msg->addUUID("ItemID", mUUID);
		gAgent.sendReliableMessage();
	}
	else
	{
		LLSD body;
		body["agent_id"] = gAgentID;
		body["items"][0]["owner_id"] = mPermissions.getOwner();
		body["items"][0]["item_id"] = mUUID;
		LLCore::HttpHandler::ptr_t
			handler(new LLInventoryModel::FetchItemHttpHandler(body));
		gInventory.requestPost(true, url, body, handler, "Inventory Item");
	}
}

//virtual
bool LLViewerInventoryItem::unpackMessage(const LLSD item)
{
	bool rv = LLInventoryItem::fromLLSD(item);
	mIsComplete = true;
	return rv;
}

//virtual
bool LLViewerInventoryItem::unpackMessage(LLMessageSystem* msg,
										  const char* block,
										  S32 block_num)
{
	bool rv = LLInventoryItem::unpackMessage(msg, block, block_num);
	mIsComplete = true;
	return rv;
}

void LLViewerInventoryItem::setTransactionID(const LLTransactionID& transaction_id)
{
	mTransactionID = transaction_id;
}

//virtual
void LLViewerInventoryItem::packMessage(LLMessageSystem* msg) const
{
	llinfos << "UDP Rez/UpdateObject of UUID " << mUUID << " - parent = "
			<< mParentUUID << " - type = " << mType << " - transaction = "
			<< mTransactionID << llendl;
	msg->addUUIDFast(_PREHASH_ItemID, mUUID);
	msg->addUUIDFast(_PREHASH_FolderID, mParentUUID);
	mPermissions.packMessage(msg);
	msg->addUUIDFast(_PREHASH_TransactionID, mTransactionID);
	S8 type = static_cast<S8>(mType);
	msg->addS8Fast(_PREHASH_Type, type);
	type = static_cast<S8>(mInventoryType);
	msg->addS8Fast(_PREHASH_InvType, type);
	msg->addU32Fast(_PREHASH_Flags, mFlags);
	mSaleInfo.packMessage(msg);
	msg->addStringFast(_PREHASH_Name, mName);
	msg->addStringFast(_PREHASH_Description, mDescription);
	msg->addS32Fast(_PREHASH_CreationDate, mCreationDate);
	U32 crc = getCRC32();
	msg->addU32Fast(_PREHASH_CRC, crc);
}

//virtual
bool LLViewerInventoryItem::importLegacyStream(std::istream& input_stream)
{
	bool rv = LLInventoryItem::importLegacyStream(input_stream);
	mIsComplete = true;
	return rv;
}

//virtual
void LLViewerInventoryItem::updateParentOnServer(bool restamp) const
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_MoveInventoryItem);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addBoolFast(_PREHASH_Stamp, restamp);
	msg->nextBlockFast(_PREHASH_InventoryData);
	msg->addUUIDFast(_PREHASH_ItemID, mUUID);
	msg->addUUIDFast(_PREHASH_FolderID, mParentUUID);
	msg->addString("NewName", NULL);
	gAgent.sendReliableMessage();
}

///----------------------------------------------------------------------------
/// Class LLViewerInventoryCategory
///----------------------------------------------------------------------------

LLViewerInventoryCategory::LLViewerInventoryCategory(const LLUUID& uuid,
													 const LLUUID& parent_uuid,
													 LLFolderType::EType pref,
													 const std::string& name,
													 const LLUUID& owner_id)
:	LLInventoryCategory(uuid, parent_uuid, pref, name),
	mOwnerID(owner_id),
	mVersion(LLViewerInventoryCategory::VERSION_UNKNOWN),
	mDescendentCount(LLViewerInventoryCategory::DESCENDENT_COUNT_UNKNOWN)
{
	mDescendentsRequested.reset();
}

LLViewerInventoryCategory::LLViewerInventoryCategory(const LLUUID& owner_id)
:	mOwnerID(owner_id),
	mVersion(LLViewerInventoryCategory::VERSION_UNKNOWN),
	mDescendentCount(LLViewerInventoryCategory::DESCENDENT_COUNT_UNKNOWN)
{
	mDescendentsRequested.reset();
}

LLViewerInventoryCategory::LLViewerInventoryCategory(const LLViewerInventoryCategory* other)
{
	copyViewerCategory(other);
}

void LLViewerInventoryCategory::copyViewerCategory(const LLViewerInventoryCategory* other)
{
	copyCategory(other);
	mOwnerID = other->mOwnerID;
	mVersion = other->mVersion;
	mDescendentCount = other->mDescendentCount;
	mDescendentsRequested = other->mDescendentsRequested;
}

void LLViewerInventoryCategory::packMessage(LLMessageSystem* msg) const
{
	msg->addUUIDFast(_PREHASH_FolderID, mUUID);
	msg->addUUIDFast(_PREHASH_ParentID, mParentUUID);
	S8 type = static_cast<S8>(mPreferredType);
	msg->addS8Fast(_PREHASH_Type, type);
	msg->addStringFast(_PREHASH_Name, mName);
}

void LLViewerInventoryCategory::updateParentOnServer(bool restamp) const
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_MoveInventoryFolder);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	msg->addBool("Stamp", restamp);
	msg->nextBlockFast(_PREHASH_InventoryData);
	msg->addUUIDFast(_PREHASH_FolderID, mUUID);
	msg->addUUIDFast(_PREHASH_ParentID, mParentUUID);
	gAgent.sendReliableMessage();
}

// Communicate changes with the server.
void LLViewerInventoryCategory::updateServer(bool is_new) const
{
	if (LLFolderType::lookupIsProtectedType(mPreferredType))
	{
		gNotifications.add("CannotModifyProtectedCategories");
		return;
	}

	if (AISAPI::isAvailable())
	{
		LL_DEBUGS("Inventory") << "Updating category via AIS: " << mUUID
							   << LL_ENDL;
		LLSD new_llsd = asLLSD();
		AISAPI::completion_t cr = boost::bind(&doInventoryCb,
											  (LLPointer<LLInventoryCallback>)NULL,
											  _1);
		AISAPI::UpdateCategory(mUUID, new_llsd, cr);
	}
	else
	{
		LLInventoryModel::LLCategoryUpdate up(mParentUUID, is_new ? 1 : 0);
		gInventory.accountForUpdate(up);

		LL_DEBUGS("Inventory") << "Updating category: " << mUUID
							   << LL_ENDL;
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_UpdateInventoryFolder);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_FolderData);
		packMessage(gMessageSystemp);
		gAgent.sendReliableMessage();
	}
}

bool LLViewerInventoryCategory::fetch()
{
	static LLViewerRegion* last_region = NULL;

	if (mVersion == VERSION_UNKNOWN &&
		// Expired check prevents multiple downloads:
		mDescendentsRequested.hasExpired())
	{
		LL_DEBUGS("InventoryFetch") << "Fetching category children: " << mName
									<< ", UUID: " << mUUID << LL_ENDL;
		constexpr F32 FETCH_TIMER_EXPIRY = 10.f;
		mDescendentsRequested.reset();
		mDescendentsRequested.setTimerExpirySec(FETCH_TIMER_EXPIRY);

		// bitfield
		// 1 = by date
		// 2 = folders by date
		// Need to mask off anything but the first bit.
		// This comes from LLInventoryFilter from llfolderview.h
		U32 sort_order = gSavedSettings.getU32("InventorySortOrder") & 0x1;

		const std::string& url =
			gAgent.getRegionCapability("FetchInventoryDescendents2");
		if (!url.empty()) // Capability found. Build up LLSD and use it.
		{
			if (last_region != gAgent.getRegion())	// Do not spam the log file.
			{
				llinfos << "Using capability \"FetchInventoryDescendents2\" for inventory fetch."
						<< llendl;
				last_region = gAgent.getRegion();
			}
			LLInventoryModelFetch::getInstance()->start(mUUID);
		}
		else
		{
			// Deprecated, but if we do not have a capability, use the old
			// system.
			if (last_region != gAgent.getRegion())	// Do not spam the log file.
			{
				llinfos << "Inventory fetch capabilities not found. Using deprecated UDP message."
						<< llendl;
				last_region = gAgent.getRegion();
			}
			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessage("FetchInventoryDescendents");
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID", gAgentID);
			msg->addUUID("SessionID", gAgentSessionID);
			msg->nextBlock("InventoryData");
			msg->addUUID("FolderID", mUUID);
			msg->addUUID("OwnerID", mOwnerID);

			msg->addS32("SortOrder", sort_order);
			msg->addBool("FetchFolders", false);
			msg->addBool("FetchItems", true);
			gAgent.sendReliableMessage();
		}

		return true;
	}

	return false;
}

bool LLViewerInventoryCategory::isProtected() const
{
	LLFolderType::EType cat_type = getPreferredType();

	// If not a protected type, do not bother !
	if (!LLFolderType::lookupIsProtectedType(cat_type))
	{
		return false;
	}

	// If the folder does not bear the default name for its preferred type, it
	// is not protected.
	if (getName() != LLViewerFolderType::lookupNewCategoryName(cat_type))
	{
		return false;
	}

	// If the folder is not at the root of the inventory, it is not protected.
	LLViewerInventoryCategory* cat = gInventory.getCategory(getParentUUID());
	if (cat && cat->getUUID() != gInventory.getRootFolderID())
	{
		return false;
	}

	// Folder is indeed protected !
	return true;	
}

S32 LLViewerInventoryCategory::getViewerDescendentCount() const
{
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(getUUID(), cats, items);
	S32 descendents_actual = 0;
	if (cats && items)
	{
		descendents_actual = cats->size() + items->size();
	}
	return descendents_actual;
}

LLSD LLViewerInventoryCategory::exportLLSD() const
{
	LLSD cat_data;
	cat_data["cat_id"] = mUUID;
	cat_data["parent_id"] = mParentUUID;
	cat_data["type"] = LLAssetType::lookup(mType);
	cat_data["pref_type"] = LLFolderType::lookup(mPreferredType);
	cat_data["name"] = mName;
	cat_data["owner_id"] = mOwnerID;
	cat_data["version"] = mVersion;
	return cat_data;
}

bool LLViewerInventoryCategory::importLLSD(const LLSD& cat_data)
{
	if (cat_data.has("cat_id"))
	{
		mUUID = cat_data["cat_id"].asUUID();
	}
	if (cat_data.has("parent_id"))
	{
		mParentUUID = cat_data["parent_id"].asUUID();
	}
	if (cat_data.has("type"))
	{
		mType = LLAssetType::lookup(cat_data["type"].asString());
	}
	if (cat_data.has("pref_type"))
	{
		mPreferredType =
			LLFolderType::lookup(cat_data["pref_type"].asString());
	}
	if (cat_data.has("name"))
	{
		mName = cat_data["name"].asString();
		LLStringUtil::replaceNonstandardASCII(mName, ' ');
		LLStringUtil::replaceChar(mName, '|', ' ');
	}
	if (cat_data.has("owner_id"))
	{
		mOwnerID = cat_data["owner_id"].asUUID();
	}
	if (cat_data.has("version"))
	{
		mVersion = cat_data["version"].asInteger();
	}
	return true;
}

bool LLViewerInventoryCategory::acceptItem(LLInventoryItem* inv_item)
{
	if (!inv_item)
	{
		return false;
	}

	// Only stock folders have limitation on which item they will accept
	bool accept = true;
	if (getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
	{
		// If the item is copyable (i.e. non stock) do not accept the drop in a
		// stock folder
		if (inv_item->getPermissions().allowOperationBy(PERM_COPY, gAgentID,
														gAgent.getGroupID()))
		{
			accept = false;
		}
		else
		{
			LLInventoryModel::cat_array_t* cat_array;
			LLInventoryModel::item_array_t* item_array;
			gInventory.getDirectDescendentsOf(getUUID(), cat_array, item_array);
			// Destination stock folder must be empty OR types of incoming and
			// existing items must be identical and have the same permissions
			accept = (!item_array->size() ||
					  ((item_array->at(0)->getInventoryType() == inv_item->getInventoryType()) &&
					   (item_array->at(0)->getPermissions().getMaskNextOwner() == inv_item->getPermissions().getMaskNextOwner())));
		}
	}
	return accept;
}

//virtual
bool LLViewerInventoryCategory::unpackMessage(const LLSD& category)
{
#if 0
	bool rv = LLInventoryCategory::fromLLSD(category);
	localizeName();
	return rv;
#else
	return LLInventoryCategory::fromLLSD(category);
#endif
}

//virtual
void LLViewerInventoryCategory::unpackMessage(LLMessageSystem* msg,
											  const char* block, S32 block_num)
{
	LLInventoryCategory::unpackMessage(msg, block, block_num);
#if 0
	localizeName();
#endif
}

//-----------------------------------------------------------------------------
// LLInventoryCallbackManager
//-----------------------------------------------------------------------------

LLInventoryCallbackManager* LLInventoryCallbackManager::sInstance = NULL;

LLInventoryCallbackManager::LLInventoryCallbackManager()
:	mLastCallback(0)
{
	if (sInstance)
	{
		llwarns << "Unexpected multiple instances" << llendl;
		return;
	}
	sInstance = this;
}

LLInventoryCallbackManager::~LLInventoryCallbackManager()
{
	if (sInstance != this)
	{
		llwarns << "Unexpected multiple instances" << llendl;
		return;
	}
	sInstance = NULL;
}

//static
void LLInventoryCallbackManager::destroyClass()
{
	if (sInstance)
	{
		for (callback_map_t::iterator it = sInstance->mMap.begin(),
									  end = sInstance->mMap.end();
			 it != end; ++it)
		{
			// drop LLPointer reference to callback
			it->second = NULL;
		}
		sInstance->mMap.clear();
	}
}

U32 LLInventoryCallbackManager::registerCB(LLPointer<LLInventoryCallback> cb)
{
	if (cb.isNull())
	{
		return 0;
	}

	if (!++mLastCallback)
	{
		++mLastCallback;
	}

	mMap[mLastCallback] = cb;
	return mLastCallback;
}

void LLInventoryCallbackManager::fire(U32 callback_id, const LLUUID& item_id)
{
	if (callback_id && item_id.notNull())
	{
		callback_map_t::iterator i = mMap.find(callback_id);
		if (i != mMap.end())
		{
			i->second->fire(item_id);
			mMap.erase(i);
		}
	}
}

LLInventoryCallbackManager gInventoryCallbacks;

//-----------------------------------------------------------------------------
// Other callbacks
//-----------------------------------------------------------------------------

void ActivateGestureCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.notNull())
	{
		gGestureManager.activateGesture(inv_item);
	}
}

class CreateGestureCallback : public LLInventoryCallback
{
public:
	void fire(const LLUUID& inv_item);
};

void CreateGestureCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.isNull())
	{
		return;
	}

	gGestureManager.activateGesture(inv_item);

	LLViewerInventoryItem* item = gInventory.getItem(inv_item);
	if (!item) return;

	LLPermissions perm = item->getPermissions();
	perm.setGroupBits(gAgentID, gAgent.getGroupID(),
					  gSavedSettings.getBool("ShareWithGroup"),
					  PERM_MODIFY | PERM_MOVE | PERM_COPY);
	perm.setEveryoneBits(gAgentID, gAgent.getGroupID(),
						 gSavedSettings.getBool("EveryoneCopy"),
						 PERM_COPY);
	if (perm != item->getPermissions() && item->isFinished())
	{
		item->setPermissions(perm);
		item->updateServer(false);
	}

	gInventory.updateItem(item);
	gInventory.notifyObservers();

	if (!LLPreview::show(inv_item, false))
	{
		LLPreviewGesture* preview;
		preview = LLPreviewGesture::show(std::string("Gesture: ") + item->getName(),
										 inv_item, LLUUID::null);
		// Force to be entirely onscreen.
		gFloaterViewp->adjustToFitScreen(preview);
	}
}

class CreateItemCallback : public LLInventoryCallback
{
public:
	void fire(const LLUUID& inv_item);
};

void CreateItemCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.isNull())
	{
		return;
	}

	LLViewerInventoryItem* item = gInventory.getItem(inv_item);
	if (!item || item->getIsLinkType())
	{
		return;
	}

	bool share_with_group = gSavedSettings.getBool("ShareWithGroup") &&
							(item->getType() != LLAssetType::AT_LSL_TEXT ||
							 !gSavedSettings.getBool("NoModScripts"));
	bool everyone_copy = gSavedSettings.getBool("EveryoneCopy");
	if (!share_with_group && !everyone_copy)
	{
		// Do not bother...
		return;
	}

	LLPermissions perm = item->getPermissions();
	perm.setGroupBits(gAgentID, gAgent.getGroupID(), share_with_group,
					  PERM_MODIFY | PERM_MOVE | PERM_COPY);
	perm.setEveryoneBits(gAgentID, gAgent.getGroupID(), everyone_copy,
						 PERM_COPY);
	if (item->isFinished())
	{
		item->setPermissions(perm);
		item->updateServer(false);
		gInventory.updateItem(item);
		gInventory.notifyObservers();
	}
}

void create_new_item(const std::string& name, const LLUUID& parent_id,
					 LLAssetType::EType asset_type,
					 LLInventoryType::EType inv_type,
					 U32 next_owner_perm, std::string desc)
{
	if (desc.empty())
	{
		LLAssetType::generateDescriptionFor(asset_type, desc);
	}

	if (next_owner_perm == 0)
	{
		next_owner_perm = PERM_MOVE | PERM_TRANSFER;
	}

	LLPointer<LLInventoryCallback> cb = NULL;
	if (inv_type == LLInventoryType::IT_GESTURE)
	{
		cb = new CreateGestureCallback();
	}
	else
	{
		cb = new CreateItemCallback();
	}
	create_inventory_item(parent_id, LLTransactionID::tnull, name, desc,
						  asset_type, inv_type, NO_INV_SUBTYPE,
						  next_owner_perm, cb);
}

void create_inventory_item(const LLUUID& parent,
						   const LLTransactionID& transaction_id,
						   const std::string& name, const std::string& desc,
						   LLAssetType::EType asset_type,
						   LLInventoryType::EType inv_type, U8 sub_type,
						   U32 next_owner_perm,
						   LLPointer<LLInventoryCallback> cb)
{
	LL_DEBUGS("Inventory") << "Creating item: " << name << LL_ENDL;
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_CreateInventoryItem);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_InventoryBlock);
	msg->addU32Fast(_PREHASH_CallbackID, gInventoryCallbacks.registerCB(cb));
	msg->addUUIDFast(_PREHASH_FolderID, parent);
	msg->addUUIDFast(_PREHASH_TransactionID, transaction_id);
	msg->addU32Fast(_PREHASH_NextOwnerMask, next_owner_perm);
	msg->addS8Fast(_PREHASH_Type, (S8)asset_type);
	msg->addS8Fast(_PREHASH_InvType, (S8)inv_type);
	msg->addU8Fast(_PREHASH_WearableType, sub_type);
	msg->addStringFast(_PREHASH_Name, name);
	msg->addStringFast(_PREHASH_Description, desc);
	gAgent.sendReliableMessage();
}

void copy_inventory_item(const LLUUID& current_owner, const LLUUID& item_id,
						 const LLUUID& parent_id, const std::string& new_name,
						 LLPointer<LLInventoryCallback> cb)
{
	LL_DEBUGS("Inventory") << "Copying item: " << item_id
						   << "- as new item: " << new_name << LL_ENDL;
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_CopyInventoryItem);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_InventoryData);
	msg->addU32Fast(_PREHASH_CallbackID, gInventoryCallbacks.registerCB(cb));
	msg->addUUIDFast(_PREHASH_OldAgentID, current_owner);
	msg->addUUIDFast(_PREHASH_OldItemID, item_id);
	msg->addUUIDFast(_PREHASH_NewFolderID, parent_id);
	msg->addStringFast(_PREHASH_NewName, new_name);
	gAgent.sendReliableMessage();
}

// Counts the number of items (not folders) in the descending hierarchy
S32 count_descendants_items(const LLUUID& cat_id)
{
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat_id, cat_array, item_array);

	S32 count = item_array->size();

	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator
			iter = cat_array_copy.begin(), end = cat_array_copy.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryCategory* category = *iter;
		if (category)
		{
			count += count_descendants_items(category->getUUID());
		}
	}

	return count;
}

void update_folder_cb(const LLUUID& folder_id)
{
	LLViewerInventoryCategory* cat = gInventory.getCategory(folder_id);
	if (cat)
	{
		gInventory.updateCategory(cat);
		gInventory.notifyObservers();
	}
}

void copy_inventory_category(LLInventoryModel* model,
							 LLViewerInventoryCategory* cat,
							 const LLUUID& parent_id,
							 const LLUUID& root_copy_id,
							 bool move_no_copy_items)
{
	if (!model || !cat) return;

	LLMarketplaceData* marketdata = NULL;
	if (LLMarketplace::contains(cat->getUUID()))
	{
		marketdata = LLMarketplaceData::getInstance();
	}

	// Create the initial folder
	LLUUID new_cat_uuid = gInventory.createNewCategory(parent_id,
													   LLFolderType::FT_NONE,
													   cat->getName());
	model->notifyObservers();

	// We need to exclude the initial root of the copy to avoid recursively
	// copying the copy, etc...
	LLUUID root_id = root_copy_id.isNull() ? new_cat_uuid : root_copy_id;

	// Get the content of the folder
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat->getUUID(), cat_array, item_array);

	// If root_copy_id is null, tell the marketplace model we'll be waiting for
	// new items to be copied over for this folder
	if (marketdata && root_copy_id.isNull())
	{
		S32 count = count_descendants_items(cat->getUUID());
		marketdata->setValidationWaiting(root_id, count);
	}

	// Copy all the items
	LLInventoryModel::item_array_t item_array_copy = *item_array;
	for (LLInventoryModel::item_array_t::iterator
			iter = item_array_copy.begin(), end = item_array_copy.end();
		 iter != end; ++iter)
	{
		LLInventoryItem* item = *iter;
		if (!item) continue;	// Paranoia

		if (!item->getPermissions().allowOperationBy(PERM_COPY, gAgentID,
													 gAgent.getGroupID()))
		{
			// If the item is nocopy, we do nothing or, optionally, move it
			if (move_no_copy_items)
			{
				// Reparent the item
				LLViewerInventoryItem* vitem = (LLViewerInventoryItem*)item;
				gInventory.changeItemParent(vitem, new_cat_uuid, true);
			}
			if (marketdata)
			{
				// Decrement the count in root_id since that one item won't be
				// copied over
				marketdata->decrementValidationWaiting(root_id);
			}
		}
		else
		{
			LLPointer<LLInventoryCallback> cb;
			cb = new LLBoostFuncInventoryCallback(boost::bind(update_folder_cb,
															  new_cat_uuid));
			copy_inventory_item(item->getPermissions().getOwner(),
								item->getUUID(), new_cat_uuid,
								LLStringUtil::null, cb);
		}
	}

	// Copy all the folders
	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator
			iter = cat_array_copy.begin(), end = cat_array_copy.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryCategory* category = *iter;
		if (category && category->getUUID() != root_id)
		{
			copy_inventory_category(model, category, new_cat_uuid, root_id,
									move_no_copy_items);
		}
	}
}

void link_inventory_object(const LLUUID& parent_id,
						   LLConstPointer<LLInventoryObject> baseobj,
						   LLPointer<LLInventoryCallback> cb)
{
	if (baseobj)
	{
		LLInventoryObject::const_object_list_t obj_array;
		obj_array.emplace_back(baseobj);
		link_inventory_array(parent_id, obj_array, cb);
	}
	else
	{
		llwarns << "Attempt to link to non-existent object inside category: "
				<< parent_id << llendl;
	}
}

void link_inventory_object(const LLUUID& parent_id, const LLUUID& id,
						   LLPointer<LLInventoryCallback> cb)
{
	LLConstPointer<LLInventoryObject> baseobj = gInventory.getObject(id);
	link_inventory_object(parent_id, baseobj, cb);
}

void do_link_objects(const LLUUID& parent_id, LLSD& links,
					 LLPointer<LLInventoryCallback> cb)
{
	LL_DEBUGS("Inventory") << "Creating links:\n" << ll_pretty_print_sd(links)
						   << LL_ENDL;
	if (AISAPI::isAvailable())
	{
		LLSD new_inventory = LLSD::emptyMap();
		new_inventory["links"] = links;
		AISAPI::completion_t cr = boost::bind(&doInventoryCb, cb, _1);
		AISAPI::CreateInventory(parent_id, new_inventory, cr);
	}
	else
	{
		LLMessageSystem* msg = gMessageSystemp;
		for (LLSD::array_iterator iter = links.beginArray(),
								  end = links.endArray();
			 iter != end; ++iter)
		{
			msg->newMessageFast(_PREHASH_LinkInventoryItem);
			msg->nextBlock(_PREHASH_AgentData);
			{
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			}
			msg->nextBlock(_PREHASH_InventoryBlock);
			{
				LLSD link = *iter;
				msg->addU32Fast(_PREHASH_CallbackID,
								gInventoryCallbacks.registerCB(cb));
				msg->addUUIDFast(_PREHASH_FolderID, parent_id);
				msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null);
				msg->addUUIDFast(_PREHASH_OldItemID,
								 link["linked_id"].asUUID());
				msg->addS8Fast(_PREHASH_Type, link["type"].asInteger());
				msg->addS8Fast(_PREHASH_InvType, link["inv_type"].asInteger());
				msg->addStringFast(_PREHASH_Name, link["name"].asString());
				msg->addStringFast(_PREHASH_Description,
								   link["desc"].asString());
			}
			gAgent.sendReliableMessage();
		}
	}
}

// Create links to all listed inventory objects.
void link_inventory_array(const LLUUID& parent_id,
						  LLInventoryObject::const_object_list_t& baseobj_array,
						  LLPointer<LLInventoryCallback> cb)
{
	LLSD links = LLSD::emptyArray();
	for (LLInventoryObject::const_object_list_t::const_iterator
			it = baseobj_array.begin(), end = baseobj_array.end();
		 it != end; ++it)
	{
		const LLInventoryObject* baseobj = *it;
		if (!baseobj)
		{
			llwarns << "Attempt to link to unknown object inside category: "
					<< parent_id << llendl;
			continue;
		}
#if 0	// This is actually cared for below... and thus possible !
		if (baseobj->getIsLinkType())
		{
			llwarns << "Attempt to create a link to link object: "
					<< baseobj->getUUID() << llendl;
			continue;
		}
#endif
		if (!LLAssetType::lookupCanLink(baseobj->getType()))
		{
			// Fail if item can be found but is of a type that can't be linked.
			// Arguably should fail if the item can't be found too, but that
			// could be a larger behavioral change.
			llwarns << "Attempt to link an unlinkable object, type = "
					<< baseobj->getActualType() << ", id = "
					<< baseobj->getUUID() << llendl;
			continue;
		}

		LLInventoryType::EType inv_type = LLInventoryType::IT_NONE;
		LLAssetType::EType asset_type = LLAssetType::AT_NONE;
		std::string new_desc;
		LLUUID linkee_id;
		if (baseobj->asInventoryCategory())
		{
			inv_type = LLInventoryType::IT_CATEGORY;
			asset_type = LLAssetType::AT_LINK_FOLDER;
			linkee_id = baseobj->getUUID();
		}
		else
		{
			const LLViewerInventoryItem* baseitem =
				baseobj->asViewerInventoryItem();
			if (baseitem)
			{
				inv_type = baseitem->getInventoryType();
				new_desc = baseitem->getActualDescription();
				switch (baseitem->getActualType())
				{
					case LLAssetType::AT_LINK:
					case LLAssetType::AT_LINK_FOLDER:
						linkee_id = baseobj->getLinkedUUID();
						asset_type = baseitem->getActualType();
						break;

					default:
						linkee_id = baseobj->getUUID();
						asset_type = LLAssetType::AT_LINK;
				}
			}
			else
			{
				llwarns << "Could not convert object into an item or category: "
						<< baseobj->getUUID() << llendl;
				continue;
			}
		}

		LLSD link = LLSD::emptyMap();
		link["linked_id"] = linkee_id;
		link["type"] = (S8)asset_type;
		link["inv_type"] = (S8)inv_type;
		link["name"] = baseobj->getName();
		link["desc"] = new_desc;
		links.append(link);

		LL_DEBUGS("Inventory") << "Linking object '" << baseobj->getName()
							   << "' (" << baseobj->getUUID()
							   << ") into category: " << parent_id << LL_ENDL;
	}

	do_link_objects(parent_id, links, cb);
}

void link_inventory_item(const LLUUID& item_id, const LLUUID& parent_id,
						 const std::string& new_description,
						 const LLAssetType::EType asset_type,
						 LLPointer<LLInventoryCallback> cb)
{
	const LLInventoryObject* baseobj = gInventory.getObject(item_id);
	if (!baseobj)
	{
		llwarns << "attempt to link to unknown item, linked-to-item's itemID "
				<< item_id << llendl;
		return;
	}
	if (baseobj->getIsLinkType())
	{
		llwarns << "attempt to create a link to a link, linked-to-item's itemID "
				<< item_id << llendl;
		return;
	}

	if (!LLAssetType::lookupCanLink(baseobj->getType()))
	{
		// Fail if item can be found but is of a type that can't be linked.
		// Arguably should fail if the item can't be found too, but that could
		// be a larger behavioral change.
		llwarns << "attempt to link an unlinkable item, type = "
				<< baseobj->getActualType() << llendl;
		return;
	}

	LLUUID transaction_id;
	LLInventoryType::EType inv_type = LLInventoryType::IT_NONE;
	if (baseobj->asInventoryCategory())
	{
		inv_type = LLInventoryType::IT_CATEGORY;
	}
	else
	{
		const LLViewerInventoryItem* baseitem =
			baseobj->asViewerInventoryItem();
		if (baseitem)
		{
			inv_type = baseitem->getInventoryType();
		}
	}

	LLSD link = LLSD::emptyMap();
	link["linked_id"] = item_id;
	link["type"] = (S8)asset_type;
	link["inv_type"] = (S8)inv_type;
	link["name"] = baseobj->getName(); // Links cannot be given arbitrary names
	link["desc"] = new_description;
	LLSD links = LLSD::emptyArray();
	links.append(link);

	do_link_objects(parent_id, links, cb);
}

void move_inventory_item(const LLUUID& item_id, const LLUUID& parent_id,
						 const std::string& new_name,
						 LLPointer<LLInventoryCallback> cb)
{
	LLViewerInventoryItem* item = gInventory.getItem(item_id);
	if (!item)
	{
		llwarns << "Attempt to move an unknown item: " << item_id << llendl;
		return;
	}

	LLUUID curcat_id = item->getParentUUID();
	std::string cur_name = item->getName();
	LL_DEBUGS("Inventory") << "Moving item: " << item_id << " - name: "
						   << cur_name << " - new name: " << new_name
						   << " - from category: " << curcat_id
						   << " - to category: " << parent_id << LL_ENDL;

	// First step: change the name if needed.
	if (new_name != cur_name)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_MoveInventoryItem);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->addBoolFast(_PREHASH_Stamp, false);
		msg->nextBlockFast(_PREHASH_InventoryData);
		msg->addUUIDFast(_PREHASH_ItemID, item_id);
		msg->addUUIDFast(_PREHASH_FolderID, curcat_id);
		msg->addStringFast(_PREHASH_NewName, new_name);
		gAgent.sendReliableMessage();
	}

	// Second step: change the category if needed.
	if (parent_id != curcat_id)
	{
		gInventory.changeItemParent(item, parent_id, false);
	}

	gInventory.notifyObservers();

	if (cb.notNull())
	{
		// *HACK: There is no callback for MoveInventoryItem... Emulate one.
		constexpr F32 CALLBACK_DELAY = 3.f; // In seconds
		U32 callback_id = gInventoryCallbacks.registerCB(cb);
		doAfterInterval(boost::bind(&LLInventoryCallbackManager::fire,
									&gInventoryCallbacks,
									callback_id, item_id),
						CALLBACK_DELAY);
	}
}

// Should call this with an update_item that's been copied and modified from an
// original source item, rather than modifying the source item directly.
void update_inventory_item(LLViewerInventoryItem* update_item,
						   LLPointer<LLInventoryCallback> cb)
{
	if (!update_item)
	{
		llwarns << "NULL update_item parameter passed !" << llendl;
		llassert(false);
		return;
	}

	const LLUUID& item_id = update_item->getUUID();
	LLPointer<LLViewerInventoryItem> obj = gInventory.getItem(item_id);
	if (obj.notNull())
	{
		if (AISAPI::isAvailable())
		{
			LL_DEBUGS("Inventory") << "Updating item via AIS: " << item_id
								   << "- name: "
								   << update_item->getName() << LL_ENDL;
			LLSD updates = update_item->asLLSD();
			// Replace asset_id and/or shadow_id with transaction_id (hash_id)
			if (updates.has("asset_id"))
			{
				updates.erase("asset_id");
				if (update_item->getTransactionID().notNull())
				{
					updates["hash_id"] = update_item->getTransactionID();
				}
			}
			if (updates.has("shadow_id"))
			{
				updates.erase("shadow_id");
				if (update_item->getTransactionID().notNull())
				{
					updates["hash_id"] = update_item->getTransactionID();
				}
			}
			AISAPI::completion_t cr = boost::bind(&doInventoryCb, cb, _1);
			AISAPI::UpdateItem(item_id, updates, cr);
		}
		else
		{
			LL_DEBUGS("Inventory") << "Updating item: " << item_id
								   << "- name: "
								   << update_item->getName() << LL_ENDL;
			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessageFast(_PREHASH_UpdateInventoryItem);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->addUUIDFast(_PREHASH_TransactionID,
							 update_item->getTransactionID());
			msg->nextBlockFast(_PREHASH_InventoryData);
			msg->addU32Fast(_PREHASH_CallbackID, 0);
			update_item->packMessage(msg);
			gAgent.sendReliableMessage();

			LLInventoryModel::LLCategoryUpdate up(update_item->getParentUUID(),
												  0);
			gInventory.accountForUpdate(up);
			gInventory.updateItem(update_item);
			if (cb)
			{
				cb->fire(item_id);
			}
		}
	}
	else
	{
		llwarns << "Call done for invalid item: " << item_id << llendl;
	}
}

// Note this only supports updating an existing item. Goes through AISv3 code
// path where available. Not all uses of item->updateServer() can easily be
// switched to this paradigm.
void update_inventory_item(const LLUUID& item_id, const LLSD& updates,
						   LLPointer<LLInventoryCallback> cb)
{
	LLPointer<LLViewerInventoryItem> obj = gInventory.getItem(item_id);
	if (obj.notNull())
	{
		if (AISAPI::isAvailable())
		{
			LL_DEBUGS("Inventory") << "Updating item via AIS: " << item_id
								   << "- name: " << obj->getName() << LL_ENDL;
			AISAPI::completion_t cr = boost::bind(&doInventoryCb, cb, _1);
			AISAPI::UpdateItem(item_id, updates, cr);
		}
		else
		{
			LL_DEBUGS("Inventory") << "Updating item: " << item_id
								   << "- name: " << obj->getName() << LL_ENDL;
			LLPointer<LLViewerInventoryItem> new_item(new LLViewerInventoryItem);
			new_item->copyViewerItem(obj);
			new_item->fromLLSD(updates, false);

			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessageFast(_PREHASH_UpdateInventoryItem);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->addUUIDFast(_PREHASH_TransactionID,
							 new_item->getTransactionID());
			msg->nextBlockFast(_PREHASH_InventoryData);
			msg->addU32Fast(_PREHASH_CallbackID, 0);
			new_item->packMessage(msg);
			gAgent.sendReliableMessage();

			LLInventoryModel::LLCategoryUpdate up(new_item->getParentUUID(),
												  0);
			gInventory.accountForUpdate(up);
			gInventory.updateItem(new_item);
			if (cb)
			{
				cb->fire(item_id);
			}
		}
	}
	else
	{
		llwarns << "Call done for invalid item: " << item_id << llendl;
	}
}

void update_inventory_category(const LLUUID& cat_id, const LLSD& updates,
							   LLPointer<LLInventoryCallback> cb)
{
	LLPointer<LLViewerInventoryCategory> obj = gInventory.getCategory(cat_id);
	if (obj.notNull())
	{
		if (LLFolderType::lookupIsProtectedType(obj->getPreferredType()))
		{
			gNotifications.add("CannotModifyProtectedCategories");
			return;
		}

		LLPointer<LLViewerInventoryCategory> new_cat =
			new LLViewerInventoryCategory(obj);
		new_cat->fromLLSD(updates);
		if (AISAPI::isAvailable())
		{
			LL_DEBUGS("Inventory") << "Updating category via AIS: " << cat_id
								   << " - name: " << obj->getName() << LL_ENDL;
			LLSD new_llsd = new_cat->asLLSD();
			AISAPI::completion_t cr = boost::bind(&doInventoryCb, cb, _1);
			AISAPI::UpdateCategory(cat_id, new_llsd, cr);
		}
		else
		{
			LL_DEBUGS("Inventory") << "Updating category: " << cat_id
								   << " - name: " << obj->getName() << LL_ENDL;
			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessageFast(_PREHASH_UpdateInventoryFolder);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_FolderData);
			new_cat->packMessage(msg);
			gAgent.sendReliableMessage();

			LLInventoryModel::LLCategoryUpdate up(new_cat->getParentUUID(), 0);
			gInventory.accountForUpdate(up);
			gInventory.updateCategory(new_cat);
			if (cb)
			{
				cb->fire(cat_id);
			}
		}
	}
	else
	{
		llwarns << "Call done for invalid category: " << cat_id << llendl;
	}
}

void rename_category(LLInventoryModel* model, const LLUUID& cat_id,
					 const std::string& new_name)
{
	if (model)
	{
		LLViewerInventoryCategory* cat = model->getCategory(cat_id);
		if (cat && get_is_category_renameable(model, cat_id) &&
			cat->getName() != new_name)
		{
			LLSD updates;
			updates["name"] = new_name;
			update_inventory_category(cat_id, updates, NULL);
		}
	}
}

bool get_is_category_renameable(const LLInventoryModel* model,
								const LLUUID& id)
{
	if (model)
	{
		LLViewerInventoryCategory* cat = model->getCategory(id);
		if (cat &&
			!LLFolderType::lookupIsProtectedType(cat->getPreferredType()) &&
			cat->getOwnerID() == gAgentID)
		{
			return true;
		}
	}
	return false;
}

void remove_inventory_items(LLInventoryObject::object_list_t& items_to_kill,
							LLPointer<LLInventoryCallback> cb)
{
	for (LLInventoryObject::object_list_t::iterator it = items_to_kill.begin(),
													end = items_to_kill.end();
		 it != end; ++it)
	{
		remove_inventory_item(*it, cb);
	}
}

void remove_inventory_item(const LLUUID& item_id,
						   LLPointer<LLInventoryCallback> cb)
{
	LLPointer<LLInventoryObject> obj = gInventory.getItem(item_id);
	if (obj.notNull())
	{
		LL_DEBUGS("Inventory") << " Removing item, id: " << item_id
							   << " - name " << obj->getName() << LL_ENDL;

		remove_inventory_item(obj, cb);
	}
	else
	{
		llwarns << "Call done for invalid item: " << item_id << llendl;
	}
}

void remove_inventory_item(LLPointer<LLInventoryObject> obj,
						   LLPointer<LLInventoryCallback> cb)
{
	if (obj.notNull())
	{
		const LLUUID& item_id = obj->getUUID();
		LL_DEBUGS("Inventory") << " Removing item, id: " << item_id
							   << " - name " << obj->getName() << LL_ENDL;

		// Hide any preview
		LLPreview::hide(item_id, true);

		if (AISAPI::isAvailable())
		{
			AISAPI::completion_t cr = boost::bind(&doInventoryCb, cb, _1);
			AISAPI::RemoveItem(item_id, cr);
		}
		else
		{
			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessageFast(_PREHASH_RemoveInventoryItem);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_InventoryData);
			msg->addUUIDFast(_PREHASH_ItemID, item_id);
			gAgent.sendReliableMessage();

			// Update inventory and call callback immediately since the UDP
			// message-based system has no callback mechanism.
			gInventory.onObjectDeletedFromServer(item_id);
			if (cb)
			{
				cb->fire(item_id);
			}
		}
	}
	else
	{
		// *TODO: Clean up callback?
		llwarns << "Call done for invalid or non-existent item." << llendl;
	}
}

class LLRemoveCategoryOnDestroy final : public LLInventoryCallback
{
public:
	LLRemoveCategoryOnDestroy(const LLUUID& cat_id,
							  LLPointer<LLInventoryCallback> cb)
	:	mID(cat_id),
		mCB(cb)
	{
	}

	~LLRemoveCategoryOnDestroy() override
	{
		LLInventoryModel::EHasChildren children;
		children = gInventory.categoryHasChildren(mID);
		if (children != LLInventoryModel::CHILDREN_NO)
		{
			llwarns << "Descendents removal failed; cannot remove category: "
					<< mID << llendl;
		}
		else
		{
			remove_inventory_category(mID, mCB);
		}
	}

	void fire(const LLUUID& item_id) override	{}

private:
	LLUUID mID;
	LLPointer<LLInventoryCallback> mCB;
};

void remove_inventory_category(const LLUUID& cat_id,
							   LLPointer<LLInventoryCallback> cb,
							   bool check_protected)
{
	LLPointer<LLViewerInventoryCategory> obj = gInventory.getCategory(cat_id);
	if (obj.notNull())
	{
		LL_DEBUGS("Inventory") << "Removing category id: " << cat_id
							   << " - name " << obj->getName() << LL_ENDL;
		if (!gInventory.isCategoryComplete(cat_id))
		{
			llwarns << "Not purging the incompletely downloaded folder: "
					<< cat_id << llendl;
			return;
		}
		if (check_protected && obj->isProtected())
		{
			gNotifications.add("CannotRemoveProtectedCategories");
			return;
		}

		if (AISAPI::isAvailable())
		{
			AISAPI::completion_t cr = boost::bind(&doInventoryCb, cb, _1);
			AISAPI::RemoveCategory(cat_id, cr);
		}
		else
		{
			// RemoveInventoryFolder does not remove children, so must clear
			// descendents first.
			LLInventoryModel::EHasChildren children;
			children = gInventory.categoryHasChildren(cat_id);
			if (children != LLInventoryModel::CHILDREN_NO)
			{
				LL_DEBUGS("Inventory") << "Purging descendents first..."
									   << LL_ENDL;
				LLPointer<LLInventoryCallback> wrap_cb =
					new LLRemoveCategoryOnDestroy(cat_id, cb);
				purge_descendents_of(cat_id, wrap_cb);
				return;
			}

			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessageFast(_PREHASH_RemoveInventoryFolder);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_FolderData);
			msg->addUUIDFast(_PREHASH_FolderID, cat_id);
			gAgent.sendReliableMessage();

			// Update inventory and call callback immediately since the UDP
			// message-based system has no callback mechanism.
			gInventory.onObjectDeletedFromServer(cat_id);
			if (cb)
			{
				cb->fire(cat_id);
			}
		}
	}
	else
	{
		llwarns << "Call done for invalid or non-existent category: " << cat_id
				<< llendl;
	}
}

void remove_inventory_object(const LLUUID& object_id,
							 LLPointer<LLInventoryCallback> cb)
{
	if (gInventory.getCategory(object_id))
	{
		remove_inventory_category(object_id, cb);
	}
	else
	{
		remove_inventory_item(object_id, cb);
	}
}

void remove_folder_contents(const LLUUID& category,
							LLPointer<LLInventoryCallback> cb)
{
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	gInventory.collectDescendents(category, cats, items,
								  LLInventoryModel::EXCLUDE_TRASH);
	for (S32 i = 0, count = items.size(); i < count; ++i)
	{
		LLViewerInventoryItem* item = items[i];
		if (item && item->getIsLinkType())
		{
			remove_inventory_item(item->getUUID(), cb);
		}
	}
}

void slam_inventory_folder(const LLUUID& folder_id, const LLSD& contents,
						   LLPointer<LLInventoryCallback> cb)
{
	if (AISAPI::isAvailable())
	{
		LL_DEBUGS("Inventory") << "using AISv3 to slam folder, id: "
							   << folder_id << " - New contents: "
							   << ll_pretty_print_sd(contents) << LL_ENDL;

		AISAPI::completion_t cr = boost::bind(&doInventoryCb, cb, _1);
		AISAPI::SlamFolder(folder_id, contents, cr);
	}
	else
	{
		LL_DEBUGS("Inventory") << "using item-by-item calls to slam folder, id: "
							   << folder_id << " - New contents: "
							   << ll_pretty_print_sd(contents) << LL_ENDL;
		remove_folder_contents(folder_id, cb);
		for (LLSD::array_const_iterator it = contents.beginArray(),
										end = contents.endArray();
			 it != end; ++it)
		{
			LLViewerInventoryItem* item = new LLViewerInventoryItem;
			if (item)	// Paranoia (guard against out of memory)
			{
				const LLSD& item_contents = *it;
				item->fromLLSD(item_contents);
				link_inventory_object(folder_id, item, cb);
			}
		}
	}
}

void purge_descendents_of(const LLUUID& id, LLPointer<LLInventoryCallback> cb)
{
#if 0	// MAINT-3319: the cached number of descendents is not always reliable
	if (gInventory.categoryHasChildren(id) == LLInventoryModel::CHILDREN_NO)
	{
		LL_DEBUGS("Inventory") << "No descendents to purge for " << id
							   << LL_ENDL;
		return;
	}
#endif
	LLPointer<LLViewerInventoryCategory> cat = gInventory.getCategory(id);
	if (cat.notNull())
	{
		if (!gInventory.isCategoryComplete(id))
		{
			llwarns << "Not purging the incompletely downloaded folder: "
					<< id << llendl;
			return;
		}
		if (AISAPI::isAvailable())
		{
			AISAPI::completion_t cr = boost::bind(&doInventoryCb, cb, _1);
			AISAPI::PurgeDescendents(id, cr);
		}
		else
		{
			// Send it upstream
			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessage("PurgeInventoryDescendents");
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID", gAgentID);
			msg->addUUID("SessionID", gAgentSessionID);
			msg->nextBlock("InventoryData");
			msg->addUUID("FolderID", id);
			gAgent.sendReliableMessage();

			// Update inventory and call callback immediately since the UDP
			// message-based system has no callback mechanism.
			gInventory.onDescendentsPurgedFromServer(id);
			if (cb)
			{
				cb->fire(id);
			}
		}
	}
}

void copy_inventory_from_notecard(const LLUUID& object_id,
								  const LLUUID& notecard_inv_id,
								  const LLInventoryItem* src,
								  U32 callback_id)
{
	if (!src)
	{
		llwarns << "Null pointer to item was passed for object_id "
				<< object_id
				<< " and notecard_inv_id " << notecard_inv_id << llendl;
		return;
	}

	LLViewerRegion* regionp = NULL;
	LLViewerObject* vo = NULL;
	if (object_id.notNull() && (vo = gObjectList.findObject(object_id)))
	{
		regionp = vo->getRegion();
	}

	// Fallback to the agents region if for some reason the object is not found
	// in the viewer.
	if (!regionp)
	{
		regionp = gAgent.getRegion();
	}

	if (!regionp)
	{
		llwarns << "Cannot find region from object_id " << object_id
				<< " or agent" << llendl;
		return;
	}

	const std::string& url =
		regionp->getCapability("CopyInventoryFromNotecard");
	if (url.empty())
	{
		llwarns << "There is no 'CopyInventoryFromNotecard' capability for region: "
				<< regionp->getIdentity() << llendl;
		return;
	}

	LLSD body;
	body["notecard-id"] = notecard_inv_id;
	body["object-id"] = object_id;
	body["item-id"] = src->getUUID();
	body["folder-id"] =
		gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(src->getType()));
	body["callback-id"] = (LLSD::Integer)callback_id;

	LLCoreHttpUtil::HttpCoroutineAdapter::messageHttpPost(url, body,
														  "Notecard coppied.",
														  "Failed to copy notecard");
}

//virtual
LLAssetType::EType LLViewerInventoryItem::getType() const
{
	if (const LLViewerInventoryItem* linked_item = getLinkedItem())
	{
		return linked_item->getType();
	}
	if (const LLViewerInventoryCategory* linked_category = getLinkedCategory())
	{
		return linked_category->getType();
	}
	return LLInventoryItem::getType();
}

//virtual
const LLUUID& LLViewerInventoryItem::getAssetUUID() const
{
	if (const LLViewerInventoryItem* linked_item = getLinkedItem())
	{
		return linked_item->getAssetUUID();
	}

	return LLInventoryItem::getAssetUUID();
}

//virtual
const std::string& LLViewerInventoryItem::getName() const
{
	if (const LLViewerInventoryItem* linked_item = getLinkedItem())
	{
		return linked_item->getName();
	}
	if (const LLViewerInventoryCategory* linked_category = getLinkedCategory())
	{
		return linked_category->getName();
	}

	return  LLInventoryItem::getName();
}

//virtual
const LLPermissions& LLViewerInventoryItem::getPermissions() const
{
	// Use the actual permissions of the symlink, not its parent.
	return LLInventoryItem::getPermissions();
}

//virtual
const LLUUID& LLViewerInventoryItem::getCreatorUUID() const
{
	if (const LLViewerInventoryItem* linked_item = getLinkedItem())
	{
		return linked_item->getCreatorUUID();
	}

	return LLInventoryItem::getCreatorUUID();
}

//virtual
const std::string& LLViewerInventoryItem::getDescription() const
{
	if (const LLViewerInventoryItem* linked_item = getLinkedItem())
	{
		return linked_item->getDescription();
	}

	return LLInventoryItem::getDescription();
}

//virtual
const LLSaleInfo& LLViewerInventoryItem::getSaleInfo() const
{
	if (const LLViewerInventoryItem* linked_item = getLinkedItem())
	{
		return linked_item->getSaleInfo();
	}

	return LLInventoryItem::getSaleInfo();
}

//virtual
LLInventoryType::EType LLViewerInventoryItem::getInventoryType() const
{
	if (const LLViewerInventoryItem* linked_item = getLinkedItem())
	{
		return linked_item->getInventoryType();
	}

	// Categories do not have types. If this item is an AT_FOLDER_LINK, treat
	// it as a category.
	if (getLinkedCategory())
	{
		return LLInventoryType::IT_CATEGORY;
	}

	return LLInventoryItem::getInventoryType();
}

//virtual
U32 LLViewerInventoryItem::getFlags() const
{
	if (const LLViewerInventoryItem* linked_item = getLinkedItem())
	{
		return linked_item->getFlags();
	}
	return LLInventoryItem::getFlags();
}

//virtual
S32 LLViewerInventoryItem::getSubType() const
{
	return getFlags() & LLInventoryItem::II_FLAGS_SUBTYPE_MASK;
}

//virtual
bool LLViewerInventoryItem::isWearableType() const
{
	return getInventoryType() == LLInventoryType::IT_WEARABLE;
}

//virtual
LLWearableType::EType LLViewerInventoryItem::getWearableType() const
{
	if (!isWearableType())
	{
		return LLWearableType::WT_INVALID;
	}
	return LLWearableType::inventoryFlagsToWearableType(getFlags());
}

//virtual
bool LLViewerInventoryItem::isSettingsType() const
{
	return getInventoryType() == LLInventoryType::IT_SETTINGS;
}

//virtual
LLSettingsType::EType LLViewerInventoryItem::getSettingsType() const
{
	if (!isSettingsType())
	{
		return LLSettingsType::ST_NONE;
	}
	return LLSettingsType::fromInventoryFlags(getFlags());
}

// This returns true if the item that this item points to doesn't exist in
// memory (i.e. LLInventoryModel). The base item might still be in the database
// but just not loaded yet.
bool LLViewerInventoryItem::getIsBrokenLink() const
{
	// If the item's type resolves to be a link, that means either:
	// A. It was not able to perform indirection, i.e. the baseobj does not
	// exist in memory.
	// B. It is pointing to another link, which is illegal.
	return LLAssetType::lookupIsLinkType(getType());
}

LLViewerInventoryItem* LLViewerInventoryItem::getLinkedItem() const
{
	if (mType == LLAssetType::AT_LINK)
	{
		LLViewerInventoryItem* linked_item = gInventory.getItem(mAssetUUID);
		if (linked_item && linked_item->getIsLinkType())
		{
			llwarns << "Warning: Accessing link to link" << llendl;
			return NULL;
		}
		return linked_item;
	}
	return NULL;
}

LLViewerInventoryCategory* LLViewerInventoryItem::getLinkedCategory() const
{
	if (mType == LLAssetType::AT_LINK_FOLDER)
	{
		return gInventory.getCategory(mAssetUUID);
	}
	return NULL;
}

bool get_is_item_worn(const LLUUID& id, bool include_gestures)
{
	const LLViewerInventoryItem* item = gInventory.getItem(id);
	if (!item)
	{
		return false;
	}

	switch (item->getType())
	{
		case LLAssetType::AT_OBJECT:
		{
			if (isAgentAvatarValid() &&
				gAgentAvatarp->isWearingAttachment(item->getLinkedUUID()))
			{
				return true;
			}
			break;
		}
		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_CLOTHING:
		{
			if (gAgentWearables.isWearingItem(item->getLinkedUUID()))
			{
				return true;
			}
			break;
		}
		case LLAssetType::AT_GESTURE:
		{
			if (include_gestures &&
				gGestureManager.isGestureActive(item->getLinkedUUID()))
			{
				return true;
			}
			break;
		}
		default:
			break;
	}
	return false;
}

S32 get_folder_levels(LLInventoryCategory* inv_cat)
{
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(inv_cat->getUUID(), cats, items);

	S32 max_child_levels = 0;

	for (S32 i = 0, count = cats->size(); i < count; ++i)
	{
		LLInventoryCategory* category = cats->at(i);
		max_child_levels = llmax(max_child_levels,
								 get_folder_levels(category));
	}

	return max_child_levels + 1;
}

S32 get_folder_path_length(const LLUUID& ancestor_id,
						   const LLUUID& descendant_id)
{
	if (ancestor_id == descendant_id)
	{
		return 0;
	}

	S32 depth = 0;
	LLInventoryCategory* category = gInventory.getCategory(descendant_id);
	while (category)
	{
		const LLUUID& parent_id = category->getParentUUID();
		if (parent_id.isNull())
		{
			break;
		}

		++depth;

		if (parent_id == ancestor_id)
		{
			return depth;
		}

		category = gInventory.getCategory(parent_id);
	}

	llwarns << "Couldn't trace a path from the descendant to the ancestor"
			<< llendl;
	return -1;
}

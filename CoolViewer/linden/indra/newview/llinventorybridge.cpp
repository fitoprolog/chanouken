/**
 * @file llinventorybridge.cpp
 * @brief Implementation of the Inventory-Folder-View-Bridge classes.
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

#include "llviewerprecompiledheaders.h"

#include <utility> // for std::pair<>

#include "llinventorybridge.h"

#include "llaudioengine.h"
#include "llevent.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotifications.h"
#include "llradiogroup.h"
#include "llscrollcontainer.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llenvironment.h"
#include "llfloaterchat.h"
#include "llfloatercustomize.h"
#include "hbfloatereditenvsettings.h"
#include "llfloaterinventory.h"
#include "llfloatermarketplace.h"
#include "llfloateropenobject.h"
#include "llfloaterproperties.h"
#include "llfloaterworldmap.h"
#include "llgesturemgr.h"
#include "llinventoryactions.h"
#include "llinventoryclipboard.h"
#include "llinventoryicon.h"
#include "llinventorymodelfetch.h"
#include "lllandmarklist.h"
#include "llmarketplacefunctions.h"
#include "llpreviewanim.h"
#include "llpreviewgesture.h"
#include "llpreviewscript.h"
#include "llpreviewsound.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltooldraganddrop.h"
#include "hbviewerautomation.h"
#include "llviewercontrol.h"
#include "llviewerfoldertype.h"
#include "llviewerinventory.h"
#include "llviewermenu.h"						// For handle_object_edit()
#include "llviewermessage.h"					// For send_sound_trigger()
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#if LL_RESTORE_TO_WORLD
# include "llviewerregion.h"
#endif
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llwearablelist.h"

using namespace LLOldEvents;

// Helper functions

static std::string safe_inv_type_lookup(LLInventoryType::EType inv_type)
{
	std::string type = LLInventoryType::lookupHumanReadable(inv_type);
	if (!type.empty())
	{
		return type;
	}
	return "<" + LLTrans::getString("invalid") + ">";
}

#if LL_RESTORE_TO_WORLD
static bool restore_to_world_callback(const LLSD& notification,
									  const LLSD& response, LLItemBridge* self)
{
	if (self && LLNotification::getSelectedOption(notification, response) == 0)
	{
		self->restoreToWorld();
	}
	return false;
}
#endif

void set_menu_entries_state(LLMenuGL& menu,
							const std::vector<std::string>& entries_to_show,
							const std::vector<std::string>& disabled_entries)
{
	const LLView::child_list_t* list = menu.getChildList();
	for (LLView::child_list_t::const_iterator it = list->begin(),
											  end = list->end();
		 it != end; ++it)
	{
		std::string name = (*it)->getName();

		// Descend into split menus:
		LLMenuItemBranchGL* branchp = dynamic_cast<LLMenuItemBranchGL*>(*it);
		if (branchp && name == "More")
		{
			set_menu_entries_state(*branchp->getBranch(), entries_to_show,
								   disabled_entries);
		}

		bool found = false;
		for (S32 i = 0, count = entries_to_show.size(); i < count; ++i)
		{
			if (entries_to_show[i] == name)
			{
				found = true;
				break;
			}
		}
		if (found)
		{
			for (S32 i = 0, count = disabled_entries.size(); i < count; ++i)
			{
				if (disabled_entries[i] == name)
				{
					(*it)->setEnabled(false);
					break;
				}
			}
		}
		else
		{
			(*it)->setVisible(false);
		}
	}
}

//-----------------------------------------------------------------------------
// Class LLInventoryWearObserver
//
// Observer for "copy and wear" operation to support knowing when the all of
// the contents has been added to inventory.
//-----------------------------------------------------------------------------

class LLInventoryCopyAndWearObserver final : public LLInventoryObserver
{
public:
	LLInventoryCopyAndWearObserver(const LLUUID& cat_id, S32 count,
								   bool folder_added = false)
	:	mCatID(cat_id),
		mContentsCount(count),
		mFolderAdded(folder_added)
	{
	}

	~LLInventoryCopyAndWearObserver() override {}

	void changed(U32 mask) override;

protected:
	LLUUID mCatID;
	S32    mContentsCount;
	bool   mFolderAdded;
};

void LLInventoryCopyAndWearObserver::changed(U32 mask)
{
	if ((mask & LLInventoryObserver::ADD) != 0)
	{
		if (!mFolderAdded)
		{
			const uuid_list_t& changed_items = gInventory.getChangedIDs();
			for (uuid_list_t::const_iterator it = changed_items.begin(),
											 end = changed_items.end();
				 it != end; ++it)
			{
				if (*it == mCatID)
				{
					mFolderAdded = true;
					break;
				}
			}
		}

		if (mFolderAdded)
		{
			LLViewerInventoryCategory* category = gInventory.getCategory(mCatID);
			if (!category)
			{
				llwarns << "Couldn't find category: " << mCatID  << llendl;
			}
			else if (category->getDescendentCount() == mContentsCount)
			{
				gInventory.removeObserver(this);
				gAppearanceMgr.wearInventoryCategory(category, false, true);
				delete this;
			}
		}
	}
}

typedef std::pair<LLUUID, LLUUID> two_uuids_t;
typedef std::list<two_uuids_t> two_uuids_list_t;
typedef std::pair<LLUUID, two_uuids_list_t> uuid_move_list_t;

struct LLMoveInv
{
	LLUUID				mObjectID;
	LLUUID				mCategoryID;
	two_uuids_list_t	mMoveList;
	void				(*mCallback)(S32, void*);
	void*				mUserData;
};

static bool move_task_inventory_callback(const LLSD& notification,
										 const LLSD& response,
										 LLMoveInv* move_inv)
{
	LLViewerObject* object = gObjectList.findObject(move_inv->mObjectID);
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (object && move_inv && option == 0)
	{
		LLFloaterOpenObject::LLCatAndWear* cat_and_wear =
			(LLFloaterOpenObject::LLCatAndWear*)move_inv->mUserData;
		if (cat_and_wear && cat_and_wear->mWear
#if 0
			&& !cat_and_wear->mFolderResponded
#endif
			)
		{
			LLInventoryObject::object_list_t inventory_objects;
			object->getInventoryContents(inventory_objects);
			// Subtract one for containing folder
			S32 contents_count = inventory_objects.size() - 1;

			LLInventoryCopyAndWearObserver* inv_observer =
				new LLInventoryCopyAndWearObserver(cat_and_wear->mCatID,
												   contents_count,
												   cat_and_wear->mFolderResponded);
			gInventory.addObserver(inv_observer);
		}

		for (two_uuids_list_t::iterator it = move_inv->mMoveList.begin(),
										end = move_inv->mMoveList.end();
			 it != end; ++it)
		{
			object->moveInventory(it->first, it->second);
		}

		// Update the UI.
		dialog_refresh_all();
	}

	if (move_inv->mCallback)
	{
		move_inv->mCallback(option, move_inv->mUserData);
	}
	delete move_inv;

	return false;
}

static void warn_move_inventory(LLViewerObject* object, LLMoveInv* move_inv)
{
	std::string dialog =
		object->flagScripted() ? "MoveInventoryFromScriptedObject"
							   : "MoveInventoryFromObject";
	gNotifications.add(dialog, LLSD(), LLSD(),
					   boost::bind(move_task_inventory_callback, _1, _2,
								   move_inv));
}

// Move/copy all inventory items from the Contents folder of an in-world
// object to the agent's inventory, inside a given category.
bool move_inv_category_world_to_agent(const LLUUID& object_id,
									  const LLUUID& category_id, bool drop,
									  void (*callback)(S32, void*),
									  void* user_data)
{
	// Make sure the object exists. If we allowed dragging from anonymous
	// objects, it would be possible to bypass permissions.
	// content category has same ID as object itself
	LLViewerObject* object = gObjectList.findObject(object_id);
	if (!object)
	{
		llinfos << "Object not found for drop." << llendl;
		return false;
	}

	// This folder is coming from an object, as there is only one folder in an
	// object, the root, we need to collect the entire contents and handle them
	// as a group
	LLInventoryObject::object_list_t inventory_objects;
	object->getInventoryContents(inventory_objects);

	if (inventory_objects.empty())
	{
		llinfos << "Object contents not found for drop." << llendl;
		return false;
	}

	bool accept = true;
	bool is_move = false;

	// Coming from a task. Need to figure out if the person can move/copy this
	// item.
	LLInventoryObject::object_list_t::iterator it = inventory_objects.begin();
	LLInventoryObject::object_list_t::iterator end = inventory_objects.end();
	for ( ; it != end; ++it)
	{
		LLInventoryItem* item = dynamic_cast<LLInventoryItem*>(it->get());
		if (!item)
		{
			llwarns << "Invalid inventory item for drop" << llendl;
			continue;
		}
		LLPermissions perm(item->getPermissions());
		if (perm.allowCopyBy(gAgentID, gAgent.getGroupID()) &&
			perm.allowTransferTo(gAgentID))	/* || gAgent.isGodlike()) */
		{
			accept = true;
		}
		else if (object->permYouOwner())
		{
			// If the object cannot be copied, but the object the inventory is
			// owned by the agent, then the item can be moved from the task to
			// agent inventory.
			is_move = accept = true;
		}
		else
		{
			accept = false;
			break;
		}
	}

	if (drop && accept)
	{
		it = inventory_objects.begin();
		LLMoveInv* move_inv = new LLMoveInv;
		move_inv->mObjectID = object_id;
		move_inv->mCategoryID = category_id;
		move_inv->mCallback = callback;
		move_inv->mUserData = user_data;

		for ( ; it != end; ++it)
		{
			move_inv->mMoveList.emplace_back(category_id, (*it)->getUUID());
		}

		if (is_move)
		{
			// Callback called from within here.
			warn_move_inventory(object, move_inv);
		}
		else
		{
			LLNotification::Params params("MoveInventoryFromObject");
			params.functor(boost::bind(move_task_inventory_callback,
									   _1, _2, move_inv));
			gNotifications.forceResponse(params, 0);
		}
	}
	return accept;
}

///////////////////////////////////////////////////////////////////////////////
// LLInvFVBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLInvFVBridge::getName() const
{
	LLInventoryObject* obj = getInventoryObject();
	return obj ? obj->getName() : LLStringUtil::null;
}

// Can it be destroyed (or moved to trash) ?
//virtual
bool LLInvFVBridge::isItemRemovable()
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return false;
	}

	const LLViewerInventoryItem* item = model->getItem(mUUID);
	if (item && item->getIsLinkType())
	{
		return true;
	}

	return model->isObjectDescendentOf(mUUID, gInventory.getRootFolderID());
}

// Can it be moved to another folder ?
//virtual
bool LLInvFVBridge::isItemMovable()
{
	bool can_move = false;
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (model)
	{
		can_move = model->isObjectDescendentOf(mUUID,
											   gInventory.getRootFolderID());
	}
	return can_move;
}

// *TODO: make sure this does the right thing
//virtual
void LLInvFVBridge::showProperties()
{
	if (!LLFloaterProperties::show(mUUID, LLUUID::null))
	{
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PropertiesRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLFloaterProperties* floater =
			new LLFloaterProperties("item properties", rect,
									"Inventory item properties",
									mUUID, LLUUID::null);
		// Keep onscreen
		gFloaterViewp->adjustToFitScreen(floater);
	}
}

//virtual
void LLInvFVBridge::removeBatch(std::vector<LLFolderViewEventListener*>& batch)
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return;

	// Deactivate gestures and close settings editors when moving them into the
	// Trash.
	S32 count = batch.size();
	for (S32 i = 0; i < count; ++i)
	{
		LLInvFVBridge* bridge = (LLInvFVBridge*)batch[i];
		if (!bridge) continue;	// Paranoia
		if (bridge->isItemRemovable())
		{
			LLViewerInventoryItem* item = model->getItem(bridge->getUUID());
			if (item && item->getType() == LLAssetType::AT_GESTURE)
			{
				gGestureManager.deactivateGesture(item->getUUID());
			}
		}
		else if (!bridge->isMultiPreviewAllowed())
		{
			LLViewerInventoryItem* item = model->getItem(bridge->getUUID());
			if (item && item->getType() == LLAssetType::AT_SETTINGS)
			{
				HBFloaterEditEnvSettings::destroy(item->getUUID());
			}
		}
	}
	for (S32 i = 0; i < count; ++i)
	{
		LLInvFVBridge* bridge = (LLInvFVBridge*)batch[i];
		if (!bridge || !bridge->isItemRemovable()) continue;
		LLViewerInventoryCategory* cat = model->getCategory(bridge->getUUID());
		if (cat)
		{
			LLInventoryModel::cat_array_t descendent_categories;
			LLInventoryModel::item_array_t descendent_items;
			model->collectDescendents(cat->getUUID(), descendent_categories,
									  descendent_items, false);
			for (S32 j = 0, count2 = descendent_items.size(); j < count2; ++j)
			{
				LLViewerInventoryItem* item = descendent_items[j];
				if (!item) continue;	// Paranoia
				if (item->getType() == LLAssetType::AT_GESTURE)
				{
					gGestureManager.deactivateGesture(item->getUUID());
				}
				else if (item->getType() == LLAssetType::AT_SETTINGS)
				{
					HBFloaterEditEnvSettings::destroy(item->getUUID());
				}
			}
		}
	}

	removeBatchNoCheck(batch);
}

void LLInvFVBridge::removeBatchNoCheck(std::vector<LLFolderViewEventListener*>& batch)
{
	// This method moves a bunch of items and folders to the trash. As per
	// design guidelines for the inventory model, the message is built and the
	// accounting is performed first. After all of that, we call
	// LLInventoryModel::moveObject() to move everything
	// around.
	LLInvFVBridge* bridge;
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return;
	const LLUUID& trash_id = model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
	LLViewerInventoryItem* item = NULL;
	LLViewerInventoryCategory* cat = NULL;
	uuid_vec_t move_ids;
	LLInventoryModel::update_map_t update;
	bool start_new_message = true;
	S32 count = batch.size();
	LLMessageSystem* msg = gMessageSystemp;
	for (S32 i = 0; i < count; ++i)
	{
		bridge = (LLInvFVBridge*)batch[i];
		if (!bridge || !bridge->isItemRemovable()) continue;

		item = model->getItem(bridge->getUUID());
		if (item)
		{
			if (item->getParentUUID() == trash_id) continue;

			move_ids.emplace_back(item->getUUID());
			LLPreview::hide(item->getUUID());
			if (item->getType() == LLAssetType::AT_SETTINGS)
			{
				HBFloaterEditEnvSettings::destroy(item->getUUID());
			}
			--update[item->getParentUUID()];
			++update[trash_id];
			if (start_new_message)
			{
				start_new_message = false;
				msg->newMessageFast(_PREHASH_MoveInventoryItem);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->addBoolFast(_PREHASH_Stamp, true);
			}
			msg->nextBlockFast(_PREHASH_InventoryData);
			msg->addUUIDFast(_PREHASH_ItemID, item->getUUID());
			msg->addUUIDFast(_PREHASH_FolderID, trash_id);
			msg->addString("NewName", NULL);
			if (msg->isSendFullFast(_PREHASH_InventoryData))
			{
				start_new_message = true;
				gAgent.sendReliableMessage();
				model->accountForUpdate(update);
				update.clear();
			}
		}
	}
	if (!start_new_message)
	{
		start_new_message = true;
		gAgent.sendReliableMessage();
		model->accountForUpdate(update);
		update.clear();
	}
	for (S32 i = 0; i < count; ++i)
	{
		bridge = (LLInvFVBridge*)batch[i];
		if (!bridge || !bridge->isItemRemovable()) continue;
		cat = model->getCategory(bridge->getUUID());
		if (cat)
		{
			if (cat->getParentUUID() == trash_id) continue;
			move_ids.emplace_back(cat->getUUID());
			--update[cat->getParentUUID()];
			++update[trash_id];
			if (start_new_message)
			{
				start_new_message = false;
				msg->newMessageFast(_PREHASH_MoveInventoryFolder);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->addBool("Stamp", true);
			}
			msg->nextBlockFast(_PREHASH_InventoryData);
			msg->addUUIDFast(_PREHASH_FolderID, cat->getUUID());
			msg->addUUIDFast(_PREHASH_ParentID, trash_id);
			if (msg->isSendFullFast(_PREHASH_InventoryData))
			{
				start_new_message = true;
				gAgent.sendReliableMessage();
				model->accountForUpdate(update);
				update.clear();
			}
		}
	}
	if (!start_new_message)
	{
		gAgent.sendReliableMessage();
		model->accountForUpdate(update);
	}

	// Move everything.
	for (uuid_vec_t::iterator it = move_ids.begin(), end = move_ids.end();
		 it != end; ++it)
	{
		model->moveObject(*it, trash_id);
		LLViewerInventoryItem* item = model->getItem(*it);
		if (item)
		{
			model->updateItem(item);
		}
	}

	// Notify inventory observers.
	model->notifyObservers();
}

//virtual
bool LLInvFVBridge::isClipboardPasteable() const
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		model = &gInventory;
	}
	if (!isAgentInventory() || !gInventoryClipBoard.hasContents())
	{
		return false;
	}

	const LLUUID& agent_id = gAgentID;
	uuid_vec_t objects;
	gInventoryClipBoard.retrieve(objects);
	S32 count = objects.size();
	for (S32 i = 0; i < count; ++i)
	{
		const LLUUID& object_id = objects[i];

		const LLViewerInventoryCategory* cat = model->getCategory(object_id);
		if (cat)
		{
			if (cat->getPreferredType() != LLFolderType::FT_NONE)
			{
				// Do not allow to copy any special folder
				return false;
			}

			LLFolderBridge cat_br(mInventoryPanel, object_id);
			if (!cat_br.isItemCopyable())
			{
				return false;
			}
		}
		else
		{
			const LLViewerInventoryItem* item = model->getItem(object_id);
			if (!item || !item->getPermissions().allowCopyBy(agent_id))
			{
				return false;
			}
		}
	}
//MK
	if (gRLenabled)
	{
		// Do not allow if either the destination folder or the source folder
		// is locked
		LLViewerInventoryCategory* current_cat = model->getCategory(mUUID);
		if (current_cat)
		{
			for (S32 i = objects.size() - 1; i >= 0; --i)
			{
				const LLUUID& obj_id = objects[i];
				if (gRLInterface.isFolderLocked(current_cat) ||
					gRLInterface.isFolderLocked(model->getCategory(model->getObject(obj_id)->getParentUUID())))
				{
					return false;
				}
			}
			gInventoryClipBoard.retrieveCuts(objects);
			count = objects.size();
			for (S32 i = objects.size() - 1; i >= 0; --i)
			{
				const LLUUID& obj_id = objects[i];
				if (gRLInterface.isFolderLocked(current_cat) ||
					gRLInterface.isFolderLocked(model->getCategory(model->getObject(obj_id)->getParentUUID())))
				{
					return false;
				}
			}
		}
	}
//mk
	return true;
}

//virtual
bool LLInvFVBridge::isClipboardPasteableAsLink() const
{
	if (!gInventoryClipBoard.hasCopiedContents() ||
		!isAgentInventory())
	{
		return false;
	}
	const LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return false;
	}

	uuid_vec_t objects;
	gInventoryClipBoard.retrieve(objects);
	S32 count = objects.size();
	for (S32 i = 0; i < count; ++i)
	{
		const LLUUID& object_id = objects[i];

		const LLViewerInventoryItem* item = model->getItem(object_id);
		if (item && !LLAssetType::lookupCanLink(item->getActualType()))
		{
			return false;
		}

		const LLViewerInventoryCategory* cat = model->getCategory(object_id);
#if 0	// We do not support pasting folders as links (it is useless anyway...)
		if (cat && cat->isProtected())
#else
		if (cat)
#endif
		{
			return false;
		}
	}
//MK
	if (gRLenabled)
	{
		// Do not allow if either the destination folder or the source folder
		// is locked
		LLViewerInventoryCategory* current_cat = model->getCategory(mUUID);
		if (current_cat)
		{
			for (S32 i = objects.size() - 1; i >= 0; --i)
			{
				const LLUUID& obj_id = objects[i];
				const LLUUID& parent_id =
					model->getObject(obj_id)->getParentUUID();
				if (gRLInterface.isFolderLocked(current_cat) ||
					gRLInterface.isFolderLocked(model->getCategory(parent_id)))
				{
					return false;
				}
			}
		}
	}
//mk
	return true;
}

//virtual
bool LLInvFVBridge::cutToClipboard() const
{
	gInventoryClipBoard.addCut(mUUID);
	return true;
}

// Generic method for commonly-used entries
void LLInvFVBridge::getClipboardEntries(bool show_asset_id,
										std::vector<std::string>& items,
										std::vector<std::string>& disabled_items,
										U32 flags)
{
	bool not_first_selected_item = (flags & FIRST_SELECTED_ITEM) == 0;
	bool agent_inventory = isAgentInventory();

	const LLInventoryObject* obj = getInventoryObject();
	if (obj)
	{
		bool need_separator = false;
		if (obj->getIsLinkType())
		{
			items.emplace_back("Find Original");
			if (isLinkedObjectMissing())
			{
				disabled_items.emplace_back("Find Original");
			}
			need_separator = true;
		}
		else
		{
			if (agent_inventory)
			{
				items.emplace_back("Rename");
				if (not_first_selected_item || !isItemRenameable())
				{
					disabled_items.emplace_back("Rename");
				}
				need_separator = true;
			}

			if (show_asset_id)
			{
				items.emplace_back("Copy Asset UUID");
				if (not_first_selected_item ||
					!(isItemPermissive() || gAgent.isGodlike()))
				{
					disabled_items.emplace_back("Copy Asset UUID");
				}
				need_separator = true;
			}

		}

		if (need_separator)
		{
			items.emplace_back("Copy Separator");
		}
		items.emplace_back("Copy");
		if (!isItemCopyable())
		{
			disabled_items.emplace_back("Copy");
		}
	}

	if (agent_inventory)
	{
		items.emplace_back("Cut");
		if (!isItemMovable())
		{
			disabled_items.emplace_back("Cut");
		}
	}

	if (!isInCOF() && agent_inventory)
	{
		items.emplace_back("Paste");
		if (not_first_selected_item || !isClipboardPasteable())
		{
			disabled_items.emplace_back("Paste");
		}

		if (!isInMarketplace())
		{
			items.emplace_back("Paste As Link");
			if (not_first_selected_item || !isClipboardPasteableAsLink())
			{
				disabled_items.emplace_back("Paste As Link");
			}
		}
	}

	if (agent_inventory)
	{
		items.emplace_back("Paste Separator");

		items.emplace_back("Delete");
		if (!isItemRemovable())
		{
			disabled_items.emplace_back("Delete");
		}
	}
}

//virtual
void LLInvFVBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;
	if (isInTrash())
	{
		const LLInventoryObject* obj = getInventoryObject();
		if (obj && obj->getIsLinkType())
		{
			items.emplace_back("Find Original");
			if (isLinkedObjectMissing())
			{
				disabled_items.emplace_back("Find Original");
			}
		}
		items.emplace_back("Purge Item");
		if (!isItemRemovable())
		{
			disabled_items.emplace_back("Purge Item");
		}
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Open");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);
	}

	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
bool LLInvFVBridge::startDrag(EDragAndDropType* type, LLUUID* id) const
{
	const LLInventoryObject* obj = getInventoryObject();
	if (obj)
	{
		*type = LLAssetType::lookupDragAndDropType(obj->getActualType());
		if (*type == DAD_NONE)
		{
			return false;
		}

		*id = obj->getUUID();

		if (*type == DAD_CATEGORY)
		{
			LLInventoryModelFetch::getInstance()->start(obj->getUUID());
		}

		return true;
	}

	return false;
}

LLInventoryObject* LLInvFVBridge::getInventoryObject() const
{
	LLInventoryObject* obj = NULL;
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (model)
	{
		obj = (LLInventoryObject*)model->getObject(mUUID);
	}
	return obj;
}

bool LLInvFVBridge::isInTrash() const
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return false;
	const LLUUID& trash_id =
		model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
	return model->isObjectDescendentOf(mUUID, trash_id);
}

bool LLInvFVBridge::isInLostAndFound() const
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return false;
	const LLUUID& laf_id =
		model->findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);
	return model->isObjectDescendentOf(mUUID, laf_id);
}

bool LLInvFVBridge::isInCOF() const
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return false;
	const LLUUID& cof_id = gAppearanceMgr.getCOF();
	return cof_id.notNull() && model->isObjectDescendentOf(mUUID, cof_id);
}

bool LLInvFVBridge::isInMarketplace() const
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return false;
	const LLUUID& market_id = LLMarketplace::getMPL();
	return market_id.notNull() &&
		   model->isObjectDescendentOf(mUUID, market_id);
}

bool LLInvFVBridge::isLinkedObjectInTrash() const
{
	if (isInTrash()) return true;

	const LLInventoryObject* obj = getInventoryObject();
	if (obj && obj->getIsLinkType())
	{
		LLInventoryModel* model = mInventoryPanel->getModel();
		if (!model) return false;
		const LLUUID& trash_id =
			model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
		return model->isObjectDescendentOf(obj->getLinkedUUID(), trash_id);
	}
	return false;
}

bool LLInvFVBridge::isLinkedObjectMissing() const
{
	const LLInventoryObject* obj = getInventoryObject();
	return !obj ||
		   (obj->getIsLinkType() &&
			LLAssetType::lookupIsLinkType(obj->getType()));
}

bool LLInvFVBridge::isAgentInventory() const
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return false;
	if (gInventory.getRootFolderID() == mUUID) return true;
	return model->isObjectDescendentOf(mUUID, gInventory.getRootFolderID());
}

//static
void LLInvFVBridge::changeItemParent(LLInventoryModel* model,
									 LLViewerInventoryItem* item,
									 const LLUUID& new_parent,
									 bool restamp)
{
	if (model && item && item->getParentUUID() != new_parent)
	{
//MK
		if (gRLenabled)
		{
			LLViewerInventoryCategory* cat_parent =
				model->getCategory(item->getParentUUID());
			LLViewerInventoryCategory* cat_new_parent =
				model->getCategory(new_parent);
			// We can move this category if we are moving it from a non shared
			// folder to another one, even if both folders are locked
			if ((gRLInterface.isUnderRlvShare(cat_parent) ||
				 gRLInterface.isUnderRlvShare(cat_new_parent)) &&
				(gRLInterface.isFolderLocked(cat_parent) ||
				 gRLInterface.isFolderLocked(cat_new_parent)))
			{
				return;
			}
		}
//mk
		model->changeItemParent(item, new_parent, restamp);
	}
}

//static
void LLInvFVBridge::changeCategoryParent(LLInventoryModel* model,
										 LLViewerInventoryCategory* cat,
										 const LLUUID& new_parent,
										 bool restamp)
{
	if (model && cat &&
		!model->isObjectDescendentOf(new_parent, cat->getUUID()))
	{
//MK
		if (gRLenabled)
		{
			LLViewerInventoryCategory* cat_new_parent =
				model->getCategory(new_parent);
			// We can move this category if we are moving it from a non shared
			// folder to another one, even if both folders are locked
			if ((gRLInterface.isUnderRlvShare(cat) ||
				 gRLInterface.isUnderRlvShare(cat_new_parent)) &&
				(gRLInterface.isFolderLocked(cat) ||
				 gRLInterface.isFolderLocked(cat_new_parent)))
			{
				return;
			}
		}
//mk
		model->changeCategoryParent(cat, new_parent, restamp);
	}
}

//static
LLInvFVBridge* LLInvFVBridge::createBridge(LLAssetType::EType asset_type,
										   LLAssetType::EType actual_asset_type,
										   LLInventoryType::EType inv_type,
										   LLInventoryPanel* inventory,
										   const LLUUID& uuid,
										   U32 flags)
{
	static LLUUID last_uuid;
	bool warn = false;
	S32 sub_type = -1;
	LLInvFVBridge* self = NULL;

	switch (asset_type)
	{
	case LLAssetType::AT_TEXTURE:
		if (inv_type != LLInventoryType::IT_TEXTURE &&
			inv_type != LLInventoryType::IT_SNAPSHOT)
		{
			warn = true;
		}
		self = new LLTextureBridge(inventory, uuid, inv_type);
		break;

	case LLAssetType::AT_SOUND:
		if (inv_type != LLInventoryType::IT_SOUND)
		{
			warn = true;
		}
		self = new LLSoundBridge(inventory, uuid);
		break;

	case LLAssetType::AT_LANDMARK:
		if (inv_type != LLInventoryType::IT_LANDMARK)
		{
			warn = true;
		}
		self = new LLLandmarkBridge(inventory, uuid, flags);
		break;

	case LLAssetType::AT_CALLINGCARD:
		if (inv_type != LLInventoryType::IT_CALLINGCARD)
		{
			warn = true;
		}
		self = new LLCallingCardBridge(inventory, uuid);
		break;

	case LLAssetType::AT_SCRIPT:
		if (inv_type != LLInventoryType::IT_LSL)
		{
			warn = true;
		}
		self = new LLScriptBridge(inventory, uuid);
		break;

	case LLAssetType::AT_OBJECT:
		if (inv_type != LLInventoryType::IT_OBJECT &&
			inv_type != LLInventoryType::IT_ATTACHMENT)
		{
			warn = true;
		}
		self = new LLObjectBridge(inventory, uuid, inv_type, flags);
		break;

	case LLAssetType::AT_NOTECARD:
		if (inv_type != LLInventoryType::IT_NOTECARD)
		{
			warn = true;
		}
		self = new LLNotecardBridge(inventory, uuid);
		break;

	case LLAssetType::AT_ANIMATION:
		if (inv_type != LLInventoryType::IT_ANIMATION)
		{
			warn = true;
		}
		self = new LLAnimationBridge(inventory, uuid);
		break;

	case LLAssetType::AT_GESTURE:
		if (inv_type != LLInventoryType::IT_GESTURE)
		{
			warn = true;
		}
		self = new LLGestureBridge(inventory, uuid);
		break;

	case LLAssetType::AT_LSL_TEXT:
		if (inv_type != LLInventoryType::IT_LSL)
		{
			warn = true;
		}
		self = new LLLSLTextBridge(inventory, uuid);
		break;

	case LLAssetType::AT_CLOTHING:
	case LLAssetType::AT_BODYPART:
		sub_type = flags & LLInventoryItem::II_FLAGS_SUBTYPE_MASK;
		if (inv_type != LLInventoryType::IT_WEARABLE)
		{
			warn = true;
		}
		self = new LLWearableBridge(inventory, uuid, asset_type, inv_type,
									(LLWearableType::EType)sub_type);
		break;

	case LLAssetType::AT_CATEGORY:
		if (actual_asset_type == LLAssetType::AT_LINK_FOLDER)
		{
			// Create a link folder handler instead.
			self = new LLLinkFolderBridge(inventory, uuid);
			break;
		}
		self = new LLFolderBridge(inventory, uuid);
		break;

	case LLAssetType::AT_LINK:
	case LLAssetType::AT_LINK_FOLDER:
		// Only should happen for broken links.
		self = new LLLinkItemBridge(inventory, uuid);
		break;

#if LL_MESH_ASSET_SUPPORT
	case LLAssetType::AT_MESH:
		if (inv_type != LLInventoryType::IT_MESH)
		{
			warn = true;
		}
		self = new LLMeshBridge(inventory, uuid);
		break;
#endif

	case LLAssetType::AT_SETTINGS:
		sub_type = flags & LLInventoryItem::II_FLAGS_SUBTYPE_MASK;
		if (inv_type != LLInventoryType::IT_SETTINGS)
		{
			warn = true;
		}
		self = new LLSettingsBridge(inventory, uuid, sub_type);
		break;

	default:
		llwarns_once << "Unhandled asset type: " << (S32)asset_type << llendl;
	}

	if (warn && uuid != last_uuid)
	{
		last_uuid = uuid;
		llwarns << LLAssetType::lookup(asset_type)
				<< " asset has inventory type "
				<< safe_inv_type_lookup(inv_type)
				<< " on uuid " << uuid << llendl;
	}

	if (self)
	{
		self->mInvType = inv_type;
		self->mSubType = sub_type;
	}

	return self;
}

void LLInvFVBridge::purgeItem(LLInventoryModel* model, const LLUUID& uuid)
{
	LLInventoryObject* obj = model ? model->getObject(uuid) : NULL;
	if (obj && uuid.notNull())
	{
		remove_inventory_object(uuid);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLItemBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
void LLItemBridge::performAction(LLFolderView* folder, LLInventoryModel* model,
								 const std::string& action)
{
	if (action == "goto")
	{
		gotoItem(folder);
	}
	else if (action == "open")
	{
		openItem();
	}
	else if (action == "properties")
	{
		showProperties();
	}
	else if (action == "purge")
	{
		purgeItem(model, mUUID);
	}
#if LL_RESTORE_TO_WORLD
	else if (action == "restoreToWorld")
	{
		gNotifications.add("ObjectRestoreToWorld", LLSD(), LLSD(),
						   boost::bind(restore_to_world_callback, _1, _2,
									   this));
	}
#endif
	else if (action == "restore")
	{
		restoreItem();
	}
	else if (action == "copy_uuid")
	{
		// Single item only
		LLViewerInventoryItem* item = model->getItem(mUUID);
		if (!item || !gWindowp) return;

		LLUUID asset_id = item->getAssetUUID();
		gWindowp->copyTextToClipboard(utf8str_to_wstring(asset_id.asString()));
	}
	else if (action == "paste_link")
	{
		// Single item only
		LLViewerInventoryItem* itemp = model->getItem(mUUID);
		if (!itemp) return;

		LLFolderViewItem* vitemp = folder->getItemByID(itemp->getParentUUID());
		if (vitemp)
		{
			vitemp->getListener()->pasteLinkFromClipboard();
		}
	}
	else if (action == "marketplace_edit_listing")
	{
		LLMarketplace::editListing(mUUID);
	}
}

//virtual
void LLItemBridge::selectItem()
{
	LLViewerInventoryItem* item = getItem();
	if (item && !item->isFinished())
	{
		item->fetchFromServer();
	}
}

//virtual
void LLItemBridge::restoreItem()
{
	LLViewerInventoryItem* item = getItem();
	if (item)
	{
		LLInventoryModel* model = mInventoryPanel->getModel();
		const LLUUID& new_parent =
			model->findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(item->getType()));
		// do not restamp on restore.
		changeItemParent(model, item, new_parent, false);
	}
}

#if LL_RESTORE_TO_WORLD
//virtual
void LLItemBridge::restoreToWorld()
{
	if (!gAgent.getRegion()) return;

	LLViewerInventoryItem* itemp = getItem();
	if (itemp)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage("RezRestoreToWorld");
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

		msg->nextBlockFast(_PREHASH_InventoryData);
		itemp->packMessage(msg);
		msg->sendReliable(gAgent.getRegionHost());
	}

	// Similar functionality to the drag and drop rez logic
	bool remove_from_inventory = false;

	// Remove local inventory copy, sim will deal with permissions and removing
	// the item from the actual inventory if its a no-copy etc
	if (!itemp->getPermissions().allowCopyBy(gAgentID))
	{
		remove_from_inventory = true;
	}

	// Check if it is in the trash (again similar to the normal rez logic).
	const LLUUID& trash_id =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH);
	if (gInventory.isObjectDescendentOf(itemp->getUUID(), trash_id))
	{
		remove_from_inventory = true;
	}

	if (remove_from_inventory)
	{
		gInventory.deleteObject(itemp->getUUID());
		gInventory.notifyObservers();
	}
}
#endif

//virtual
void LLItemBridge::gotoItem(LLFolderView* folder)
{
	LLInventoryObject* obj = getInventoryObject();
	if (obj && obj->getIsLinkType())
	{
		LLFloaterInventory* floater = LLFloaterInventory::getActiveFloater();
		if (floater)
		{
			floater->getPanel()->setSelection(obj->getLinkedUUID(),
											  TAKE_FOCUS_NO);
		}
	}
}

//virtual
LLUIImagePtr LLItemBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLInventoryType::ICONNAME_OBJECT);
}

//virtual
PermissionMask LLItemBridge::getPermissionMask() const
{
	PermissionMask perm_mask = 0;
	LLViewerInventoryItem* item = getItem();
	if (item)
	{
		if (item->getPermissions().allowCopyBy(gAgentID))
		{
			perm_mask |= PERM_COPY;
		}
		if (item->getPermissions().allowModifyBy(gAgentID))
		{
			perm_mask |= PERM_MODIFY;
		}
		if (item->getPermissions().allowOperationBy(PERM_TRANSFER, gAgentID))
		{
			perm_mask |= PERM_TRANSFER;
		}
	}
	return perm_mask;
}

//virtual
const std::string& LLItemBridge::getDisplayName() const
{
	if (mDisplayName.empty())
	{
		buildDisplayName(getItem(), mDisplayName);
	}
	return mDisplayName;
}

//virtual
void LLItemBridge::buildDisplayName(LLInventoryItem* item, std::string& name)
{
	if (item)
	{
		name = item->getName();
	}
	else
	{
		name.clear();
	}
}

//virtual
std::string LLItemBridge::getLabelSuffix() const
{
	static const std::string link = " (" + LLTrans::getString("link") + ")";
	static const std::string broken = " (" + LLTrans::getString("brokenlink") +
									  ")";
	static const std::string nocopy = " (" + LLTrans::getString("nocopy") + ")";
	static const std::string nomod = " (" + LLTrans::getString("nomod") + ")";
	static const std::string noxfr = " (" + LLTrans::getString("noxfr") + ")";

	std::string suffix;
	LLViewerInventoryItem* item = getItem();
	if (item)
	{
		if (LLAssetType::lookupIsLinkType(item->getType()))
		{
			return broken;
		}
		if (item->getIsLinkType())
		{
			return link;
		}
		// It is a bit confusing to list permissions for calling cards.
		if (item->getType() != LLAssetType::AT_CALLINGCARD &&
			item->getPermissions().getOwner() == gAgentID)
		{
			if (!item->getPermissions().allowCopyBy(gAgentID))
			{
				suffix += nocopy;
			}
			if (!item->getPermissions().allowModifyBy(gAgentID))
			{
				suffix += nomod;
			}
			if (!item->getPermissions().allowOperationBy(PERM_TRANSFER,
														 gAgentID))
			{
				suffix += noxfr;
			}
		}
	}
	return suffix;
}

//virtual
time_t LLItemBridge::getCreationDate() const
{
	LLViewerInventoryItem* item = getItem();
	return item ? item->getCreationDate() : 0;
}

//virtual
bool LLItemBridge::isItemRenameable() const
{
	LLViewerInventoryItem* item = getItem();
	return item && item->getPermissions().allowModifyBy(gAgentID);
}

//virtual
bool LLItemBridge::renameItem(const std::string& new_name)
{
	if (!isItemRenameable()) return false;

	LLPreview::rename(mUUID, getPrefix() + new_name);
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return false;

	LLViewerInventoryItem* item = getItem();
	if (item && item->getName() != new_name)
	{
		LLSD updates;
		updates["name"] = new_name;
		update_inventory_item(item->getUUID(), updates);
	}

	// Return false because we either notified observers (and therefore
	// rebuilt) or we didn't update.
	return false;
}

//virtual
bool LLItemBridge::removeItem()
{
	if (!isItemRemovable())
	{
		return false;
	}
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return false;
	}

	const LLUUID& trash_id =
		model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
	LLViewerInventoryItem* item = getItem();
	// If item is not already in trash
	if (item && !model->isObjectDescendentOf(mUUID, trash_id))
	{
		// Move to trash, and restamp
		changeItemParent(model, item, trash_id, true);
		// Delete was successful
		return true;
	}

	// Tried to delete already item in trash (should purge ?)
	return false;
}

//virtual
bool LLItemBridge::isItemCopyable() const
{
	LLViewerInventoryItem* item = getItem();
	// All non-links can be copied (at least as a link), and non-broken links
	// can get their linked object copied too (at least as a link as well).
	return item && (!item->getIsLinkType() || !isLinkedObjectMissing());
}

//virtual
bool LLItemBridge::copyToClipboard() const
{
	if (isItemCopyable())
	{
		LLViewerInventoryItem* item = getItem();
		if (!item || (item->getIsLinkType() && isLinkedObjectMissing()))
		{
			// No item (paranoia, should not happen), or broken link: abort !
			return false;
		}
		gInventoryClipBoard.add(item->getLinkedUUID());
		return true;
	}
	return false;
}

//virtual
LLViewerInventoryItem* LLItemBridge::getItem() const
{
	LLViewerInventoryItem* item = NULL;
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (model)
	{
		item = model->getItem(mUUID);
	}
	return item;
}

//virtual
bool LLItemBridge::isItemPermissive() const
{
	LLViewerInventoryItem* item = getItem();
	if (item)
	{
		U32 mask = item->getPermissions().getMaskBase();
		if ((mask & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED)
		{
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// LLFolderBridge
///////////////////////////////////////////////////////////////////////////////

LLFolderBridge* LLFolderBridge::sSelf = NULL;

//virtual
LLFolderBridge::~LLFolderBridge()
{
	if (sSelf == this)
	{
		sSelf = NULL;
	}
}

// Can it be moved to another folder ?
//virtual
bool LLFolderBridge::isItemMovable()
{
	bool can_move = false;
	LLInventoryObject* obj = getInventoryObject();
	if (obj)
	{
		LLViewerInventoryCategory* cat = (LLViewerInventoryCategory*)obj;
		can_move = !cat->isProtected();
	}
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (model && can_move)
	{
		can_move = model->isObjectDescendentOf(mUUID,
											   model->getRootFolderID());
	}
	return can_move;
}

// Can it be destroyed (or moved to trash) ?
//virtual
bool LLFolderBridge::isItemRemovable()
{
	const LLUUID& root_id = gInventory.getRootFolderID();
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model || !model->isObjectDescendentOf(mUUID, root_id))
	{
		return false;
	}

	LLViewerInventoryCategory* category = model->getCategory(mUUID);
	if (!isAgentAvatarValid() || !category)
	{
		return false;
	}

	if (category->isProtected())
	{
		return false;
	}

	LLInventoryModel::cat_array_t	descendent_categories;
	LLInventoryModel::item_array_t	descendent_items;
	model->collectDescendents(mUUID, descendent_categories,
							  descendent_items, false);

	for (S32 i = 0, count = descendent_items.size(); i < count; ++i)
	{
		LLViewerInventoryItem* item = descendent_items[i];
		if (item && !item->getIsLinkType() &&
			get_is_item_worn(item->getUUID(), false))
		{
			return false;
		}
	}

	if (isInMarketplace() &&
		(!LLMarketplaceData::getInstance()->isSLMDataFetched() ||
		 LLMarketplace::isFolderActive(mUUID)))
	{
		return false;
	}

	return true;
}

//virtual
bool LLFolderBridge::isUpToDate() const
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return false;

	LLViewerInventoryCategory* cat = model->getCategory(mUUID);
	return cat &&
		   cat->getVersion() != LLViewerInventoryCategory::VERSION_UNKNOWN;
}

//virtual
bool LLFolderBridge::isItemCopyable() const
{
	if (getPreferredType() != LLFolderType::FT_NONE)
	{
		// Do not allow to copy any special folder
		return false;
	}

	// Get the content of the folder
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(mUUID, cat_array, item_array);

	// Check the items
	LLInventoryModel::item_array_t item_array_copy = *item_array;
	for (LLInventoryModel::item_array_t::iterator
			iter = item_array_copy.begin(), end = item_array_copy.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryItem* item = *iter;
		if (item)	// Paranoia
		{
			LLItemBridge item_br(mInventoryPanel, item->getUUID());
			if (!item_br.isItemCopyable())
			{
				return false;
			}
		}
	}

	// Recurse through the sub-folders
	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator
			iter = cat_array_copy.begin(), end = cat_array_copy.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryCategory* cat = *iter;
		if (cat)	// Paranoia
		{
			LLFolderBridge cat_br(mInventoryPanel, cat->getUUID());
			if (!cat_br.isItemCopyable())
			{
				return false;
			}
		}
	}

	return true;
}

//virtual
bool LLFolderBridge::copyToClipboard() const
{
	if (isItemCopyable())
	{
		LLViewerInventoryCategory* cat = getCategory();
		if (!cat || (cat->getIsLinkType() && isLinkedObjectMissing()))
		{
			// No category (paranoia, should not happen), or broken link
			return false;
		}
		gInventoryClipBoard.add(cat->getLinkedUUID());
		return true;
	}
	return false;
}

//virtual
bool LLFolderBridge::isClipboardPasteableAsLink() const
{
#if 0	// We do not support pasting folders as links (it is useless anyway...)
	// Check normal paste-as-link permissions
	if (!LLInvFVBridge::isClipboardPasteableAsLink())
	{
		return false;
	}

	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return false;
	}

	const LLViewerInventoryCategory* current_cat = getCategory();
	if (current_cat)
	{
		const LLUUID& current_cat_id = current_cat->getUUID();
		uuid_vec_t objects;
		gInventoryClipBoard.retrieve(objects);
		S32 count = objects.size();
		for (S32 i = 0; i < count; ++i)
		{
			const LLUUID& obj_id = objects[i];
			const LLViewerInventoryCategory* cat = model->getCategory(obj_id);
			if (cat)
			{
				const LLUUID& cat_id = cat->getUUID();
				// Don't allow recursive pasting
				if (cat_id == current_cat_id ||
					model->isObjectDescendentOf(current_cat_id, cat_id))
				{
					return false;
				}
			}
		}
	}

	return true;
#else
	// Check normal paste-as-link permissions
	return LLInvFVBridge::isClipboardPasteableAsLink();
#endif
}

bool LLFolderBridge::dragCategoryIntoFolder(LLInventoryCategory* inv_cat,
											bool drop,
											std::string& tooltip_msg)
{
	// This should never happen, but if an inventory item is incorrectly
	// parented, the UI will get confused and pass in a NULL.
	if (!inv_cat)
	{
		return false;
	}

	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model || !isAgentAvatarValid() || !isAgentInventory() || isInCOF())
	{
		return false;
	}

	const LLUUID& cat_id = inv_cat->getUUID();
	const LLUUID& from_folder_uuid = inv_cat->getParentUUID();

	if (mUUID == cat_id ||							// Can't move into itself
		mUUID == from_folder_uuid ||				// That would change nothing
		model->isObjectDescendentOf(mUUID, cat_id))	// Avoid circularity
	{
		return false;
	}

	const LLUUID& market_id = LLMarketplace::getMPL();
	bool move_is_into_market = model->isObjectDescendentOf(mUUID, market_id);
	bool move_is_from_market = model->isObjectDescendentOf(cat_id, market_id);
	bool move_is_into_trash = isInTrash();

	bool accept = false;
	LLInventoryModel::cat_array_t	descendent_categories;
	LLInventoryModel::item_array_t	descendent_items;

	// Check to make sure source is agent inventory, and is represented there.
	LLToolDragAndDrop* dadtoolp = LLToolDragAndDrop::getInstance();
	LLToolDragAndDrop::ESource source = dadtoolp->getSource();
	bool is_agent_inventory = model->getCategory(cat_id) != NULL &&
							  source == LLToolDragAndDrop::SOURCE_AGENT;
	if (is_agent_inventory)
	{
		bool movable = !((LLViewerInventoryCategory*)inv_cat)->isProtected();
		if (movable &&
			getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
		{
			// Cannot move a folder into a stock folder
			movable = false;
		}

		// Is the destination the trash ?
		if (movable && move_is_into_trash)
		{
			for (S32 i = 0, count = descendent_items.size(); i < count; ++i)
			{
				LLViewerInventoryItem* item = descendent_items[i];
				if (!item || item->getIsLinkType())
				{
					// Inventory links can always be destroyed
					continue;
				}
				if (get_is_item_worn(item->getUUID(), false))
				{
					// It is generally movable, but not into the trash !
					movable = false;
					break;
				}
			}
		}

		LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
		if (movable && move_is_from_market &&
			marketdata->getActivationState(cat_id))
		{
			movable = false;
			if (!tooltip_msg.empty())
			{
				tooltip_msg += ' ';
			}
			tooltip_msg = LLTrans::getString("TooltipOutboxDragActive");
		}

		if (movable && move_is_into_market)
		{
			const LLViewerInventoryCategory* master_folder;
			master_folder = model->getFirstDescendantOf(market_id, mUUID);

			LLViewerInventoryCategory* dest_folder = getCategory();
			S32 bundle_size = 1;
			if (!drop)
			{
				bundle_size = dadtoolp->getCargoCount();
			}
			std::string error_msg;
			movable = LLMarketplace::canMoveFolderInto(master_folder,
														  dest_folder,
														  (LLViewerInventoryCategory*)inv_cat,
														  error_msg,
														  bundle_size);
		}

		accept = movable;

		if (accept && !drop && (move_is_from_market || move_is_into_market))
		{
			if (move_is_from_market)
			{
				if (marketdata->isInActiveFolder(cat_id) ||
					marketdata->isListedAndActive(cat_id))
				{
					if (!tooltip_msg.empty())
					{
						tooltip_msg += ' ';
					}
					if (marketdata->isListed(cat_id) ||
						marketdata->isVersionFolder(cat_id))
					{
						// Moving the active version folder or listing folder
						// itself outside the Marketplace Listings would unlist
						// the listing
						tooltip_msg += LLTrans::getString("TipMerchantUnlist");
					}
					else
					{
						tooltip_msg += LLTrans::getString("TipMerchantActiveChange");
					}
				}
				else if (marketdata->isVersionFolder(cat_id))
				{
					// Moving the version folder from its location would
					// deactivate it.
					if (!tooltip_msg.empty())
					{
						tooltip_msg += ' ';
					}
					tooltip_msg += LLTrans::getString("TipMerchantClearVersion");
				}
				else if (marketdata->isListed(cat_id))
				{
					// Moving a whole listing folder folder would result in
					// archival of SLM data.
					if (!tooltip_msg.empty())
					{
						tooltip_msg += ' ';
					}
					tooltip_msg += LLTrans::getString("TipListingDelete");
				}
			}
			else // move_is_into_market
			{
				if (marketdata->isInActiveFolder(mUUID))
				{
					// Moving something in an active listed listing would
					// modify it.
					if (!tooltip_msg.empty())
					{
						tooltip_msg += ' ';
					}
					tooltip_msg += LLTrans::getString("TipMerchantActiveChange");
				}
				if (!move_is_from_market)
				{
					if (!tooltip_msg.empty())
					{
						tooltip_msg += ' ';
					}
					tooltip_msg += LLTrans::getString("TipMerchantMoveInventory");
				}
			}
		}

		if (accept && drop)
		{
 			// Look for any gestures and deactivate them
			if (move_is_into_trash)
			{
				for (S32 i = 0, count = descendent_items.size();
					 i < count; ++i)
				{
					LLViewerInventoryItem* item = descendent_items[i];
					if (item && item->getType() == LLAssetType::AT_GESTURE &&
						gGestureManager.isGestureActive(item->getUUID()))
					{
						gGestureManager.deactivateGesture(item->getUUID());
					}
				}
			}

			if (move_is_into_market)
			{
				LLMarketplace::moveFolderInto((LLViewerInventoryCategory*)inv_cat,
											  mUUID);
			}
			else
			{
				// Reparent the folder and restamp children if it's moving
				// into trash.
				changeCategoryParent(model,
									 (LLViewerInventoryCategory*)inv_cat,
									 mUUID, move_is_into_trash);
			}
			if (move_is_from_market)
			{
				LLMarketplace::updateMovedFrom(from_folder_uuid, cat_id);
			}
		}
	}
	else if (LLToolDragAndDrop::SOURCE_WORLD == source)
	{
		if (move_is_into_market)
		{
			accept = false;
		}
		else
		{
			// Content category has same ID as object itself
			accept = move_inv_category_world_to_agent(cat_id, mUUID, drop);
		}
	}

	if (accept && drop && move_is_into_trash)
	{
		model->checkTrashOverflow();
	}

	return accept;
}

bool LLFindWearables::operator()(LLInventoryCategory*, LLInventoryItem* item)
{
	if (!item)
	{
		return false;
	}
	LLAssetType::EType type = item->getType();
	return type == LLAssetType::AT_CLOTHING ||
		   type == LLAssetType::AT_BODYPART;
}

// Used by LLFolderBridge as callback for directory recursion.
class LLRightClickInventoryFetchObserver final
:	public LLInventoryFetchObserver
{
public:
	LLRightClickInventoryFetchObserver()
	:	mCopyItems(false)
	{
	}

	LLRightClickInventoryFetchObserver(const LLUUID& cat_id, bool copy_items)
	:	mCatID(cat_id),
		mCopyItems(copy_items)
	{
	}

	void done() override
	{
		// We have downloaded all the items, so repaint the dialog
		LLFolderBridge::staticFolderOptionsMenu();

		gInventory.removeObserver(this);
		delete this;
	}

protected:
	LLUUID mCatID;
	bool mCopyItems;
};

// Used by LLFolderBridge as callback for directory recursion.
class LLRightClickInventoryFetchDescendentsObserver final
:	public LLInventoryFetchDescendentsObserver
{
public:
	LLRightClickInventoryFetchDescendentsObserver(bool copy_items)
	:	mCopyItems(copy_items)
	{
	}

	~LLRightClickInventoryFetchDescendentsObserver() override
	{
	}

	void done() override;

protected:
	bool mCopyItems;
};

//virtual
void LLRightClickInventoryFetchDescendentsObserver::done()
{
	// Avoid passing a NULL-ref as mCompleteFolders.front() down to
	// gInventory.collectDescendents()
	if (mCompleteFolders.empty())
	{
		llwarns << "Empty mCompleteFolders" << llendl;
		gInventory.removeObserver(this);
		delete this;
		return;
	}

	// What we do here is get the complete information on the items in the
	// library, and set up an observer that will wait for that to happen.
	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	gInventory.collectDescendents(mCompleteFolders.front(), cat_array,
								  item_array, LLInventoryModel::EXCLUDE_TRASH);
	LLRightClickInventoryFetchObserver* outfit =
		new LLRightClickInventoryFetchObserver(mCompleteFolders.front(),
											   mCopyItems);
	LLInventoryFetchObserver::item_ref_t ids;
	for (S32 i = 0, count = item_array.size(); i < count; ++i)
	{
		ids.emplace_back(item_array[i]->getUUID());
	}

	// Clean up, and remove this as an observer since the call to the outfit
	// could notify observers and throw us into an infinite loop.
	gInventory.removeObserver(this);
	delete this;

	// Do the fetch
	outfit->fetchItems(ids);
	// Not interested in waiting and this will be right 99% of the time:
	outfit->done();
}

//virtual
void LLFolderBridge::performAction(LLFolderView* folder,
								   LLInventoryModel* model,
								   const std::string& action)
{
	if (action == "open")
	{
		openItem();
	}
	else if (action == "paste_link")
	{
		pasteLinkFromClipboard();
	}
	else if (action == "properties")
	{
		showProperties();
	}
	else if (action == "replaceoutfit")
	{
		modifyOutfit(false, false);
	}
	else if (action == "addtooutfit")
	{
		modifyOutfit(true, false);
	}
	else if (action == "wearitems")
	{
		modifyOutfit(true, true);
	}
	else if (action == "removefromoutfit")
	{
		LLInventoryModel* model = mInventoryPanel->getModel();
		if (!model) return;
		LLViewerInventoryCategory* cat = getCategory();
		if (!cat) return;
//MK
		if (gRLenabled && !gRLInterface.canDetachCategory(cat))
		{
			return;
		}
//mk
		gAppearanceMgr.removeInventoryCategoryFromAvatar(cat);
	}
	else if (action == "updatelinks")
	{
		gAppearanceMgr.updateClothingOrderingInfo(mUUID);
		gNotifications.add("ReorderingWearablesLinks");
	}
	else if (action == "purge")
	{
		if (model->isCategoryComplete(mUUID))
		{
			purgeItem(model, mUUID);
		}
		else
		{
			llwarns << "Not purging the incompletely downloaded folder: "
					<< mUUID << llendl;
		}
	}
	else if (action == "restore")
	{
		restoreItem();
	}
	else if (action == "marketplace_connect")
	{
		LLMarketplace::checkMerchantStatus();
	}
	else if (action == "marketplace_list")
	{
		LLMarketplace::listFolder(mUUID);
	}
	else if (action == "marketplace_unlist")
	{
		LLMarketplace::listFolder(mUUID, false);
	}
	else if (action == "marketplace_activate")
	{
		LLMarketplace::activateFolder(mUUID);
	}
	else if (action == "marketplace_deactivate")
	{
		LLMarketplace::activateFolder(mUUID, false);
	}
	else if (action == "marketplace_get_listing")
	{
		LLMarketplace::getListing(mUUID);
	}
	else if (action == "marketplace_create_listing")
	{
		LLMarketplace::createListing(mUUID);
	}
	else if (action == "marketplace_associate_listing")
	{
		LLFloaterAssociateListing::show(mUUID);
	}
	else if (action == "marketplace_disassociate_listing")
	{
		LLMarketplace::clearListing(mUUID);
	}
	else if (action == "marketplace_check_listing")
	{
		LLFloaterMarketplaceValidation::show(mUUID);
	}
	else if (action == "marketplace_edit_listing")
	{
		LLMarketplace::editListing(mUUID);
	}
}

//virtual
void LLFolderBridge::openItem()
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (model)
	{
		model->fetchDescendentsOf(mUUID);
	}
}

//virtual
bool LLFolderBridge::isItemRenameable() const
{
	LLViewerInventoryCategory* cat = getCategory();
//MK
	if (gRLenabled && gRLInterface.isUnderRlvShare(cat) &&
		gRLInterface.isFolderLocked(cat))
	{
		return false;
	}
//mk
	return cat && cat->getOwnerID() == gAgentID &&
		   !LLFolderType::lookupIsProtectedType(cat->getPreferredType());
}

//virtual
void LLFolderBridge::restoreItem()
{
	LLViewerInventoryCategory* cat = getCategory();
	if (cat)
	{
		LLInventoryModel* model = mInventoryPanel->getModel();
		LLFolderType::EType type =
			LLFolderType::assetTypeToFolderType(cat->getType());
		const LLUUID& new_parent = model->findCategoryUUIDForType(type);
		// false to avoid restamping children on restore
		changeCategoryParent(model, cat, new_parent, false);
	}
}

//virtual
LLFolderType::EType LLFolderBridge::getPreferredType() const
{
	LLFolderType::EType preferred_type = LLFolderType::FT_NONE;
	LLViewerInventoryCategory* cat = getCategory();
	if (cat)
	{
		preferred_type = cat->getPreferredType();
	}
	return preferred_type;
}

// Icons for folders are based on the preferred type
//virtual
LLUIImagePtr LLFolderBridge::getIcon() const
{
	LLFolderType::EType preferred_type = LLFolderType::FT_NONE;
	LLViewerInventoryCategory* cat = getCategory();
	if (cat)
	{
		preferred_type = cat->getPreferredType();
	}
	if (preferred_type == LLFolderType::FT_NONE &&
		LLMarketplace::depthNesting(mUUID) == 2)
	{
		preferred_type = LLFolderType::FT_MARKETPLACE_VERSION;
	}
	return LLViewerFolderType::lookupIcon(preferred_type);
}

//virtual
std::string LLFolderBridge::getLabelSuffix() const
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (model)
	{
		const LLUUID& market_id = LLMarketplace::getMPL();
		if (market_id.notNull())
		{
			if (mUUID == market_id)
			{
				return LLMarketplace::rootFolderLabelSuffix();
			}
			else if (model->isObjectDescendentOf(mUUID, market_id))
			{
				return LLMarketplace::folderLabelSuffix(mUUID);
			}
		}
	}
	return LLStringUtil::null;
}

//virtual
LLFontGL::StyleFlags LLFolderBridge::getLabelStyle() const
{
	S32 font = LLFontGL::NORMAL;

	if (isInMarketplace() && LLMarketplace::isFolderActive(mUUID))
	{
		font |= LLFontGL::BOLD;
	}

	return (LLFontGL::StyleFlags)font;
}

//virtual
bool LLFolderBridge::renameItem(const std::string& new_name)
{
	if (!isItemRenameable()) return false;

	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return false;

	rename_category(model, mUUID, new_name);

	// Return false because we either notified observers (and therefore
	// rebuilt) or we didn't update.
	return false;
}

//virtual
bool LLFolderBridge::removeItem()
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model || !isItemRemovable())
	{
		return false;
	}

	// Move it to the trash
	model->removeCategory(mUUID);

	return true;
}

//virtual
void LLFolderBridge::pasteFromClipboard()
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model || !isClipboardPasteable() || isInTrash() || isInCOF())
	{
		return;
	}

	bool move_is_into_market = isInMarketplace();

	bool is_cut = false;	// Copy mode in force
	uuid_vec_t objects;
	// Retrieve copied objects, if any
	gInventoryClipBoard.retrieve(objects);
	S32 count = objects.size();
	if (!count)
	{
		// Retrieve cut objects, then, if any...
		gInventoryClipBoard.retrieveCuts(objects);
		count = objects.size();
		if (!count)
		{
			return; // nothing to do !
		}
		is_cut = true;	// Cut mode in force
	}

	LLViewerInventoryCategory* dest_folder = getCategory();
	if (move_is_into_market)
	{
		std::string error_msg;
		const LLUUID& root_id = LLMarketplace::getMPL();
		const LLViewerInventoryCategory* master_folder;
		master_folder = model->getFirstDescendantOf(root_id, mUUID);

		for (S32 i = 0; i < count; ++i)
		{
			const LLUUID& object_id = objects[i];
			LLViewerInventoryItem* item = model->getItem(object_id);
			LLViewerInventoryCategory* cat = model->getCategory(object_id);
			if (item &&
				!LLMarketplace::canMoveItemInto(master_folder, dest_folder,
												item, error_msg, count - i,
												move_is_into_market, true))
			{
				break;
			}
			if (cat &&
				!LLMarketplace::canMoveFolderInto(master_folder,
												 dest_folder, cat,
												 error_msg, count - i,
												 move_is_into_market, true))
			{
				break;
			}
		}
		if (!error_msg.empty())
		{
			LLSD subs;
			subs["[ERROR_CODE]"] = error_msg;
			gNotifications.add("MerchantPasteFailed", subs);
			return;
		}
	}
	else
	{
		// Check that all items can be moved into that folder: for the
		// moment, only stock folder mismatch is checked
		bool dest_is_stock =
			dest_folder->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK;
		for (S32 i = 0; i < count; ++i)
		{
			const LLUUID& object_id = objects[i];
			LLViewerInventoryItem* item = model->getItem(object_id);
			LLViewerInventoryCategory* cat = model->getCategory(object_id);
			if ((cat && dest_is_stock) ||
				(item && !dest_folder->acceptItem(item)))
			{
				LLSD subs;
				subs["[ERROR_CODE]"] =
					LLTrans::getString("TooltipOutboxMixedStock");
				gNotifications.add("StockPasteFailed", subs);
				return;
			}
		}
	}

	for (S32 i = 0; i < count; ++i)
	{
		const LLUUID& object_id = objects[i];
		LLViewerInventoryItem* item = model->getItem(object_id);
		LLViewerInventoryCategory* cat = model->getCategory(object_id);
		if (cat)
		{
			if (is_cut)
			{
				LLMarketplace::clearListing(object_id);
				if (move_is_into_market)
				{
					LLMarketplace::moveFolderInto(cat, mUUID);
				}
				else if (mUUID != object_id && mUUID != cat->getParentUUID() &&
						 !model->isObjectDescendentOf(mUUID, object_id))
				{
					changeCategoryParent(model, cat, mUUID, false);
				}
			}
			else
			{
				if (move_is_into_market)
				{
					LLMarketplace::moveFolderInto(cat, mUUID, true);
				}
				else
				{
					copy_inventory_category(model, cat, mUUID);
				}
			}
		}
		else if (item)
		{
			if (is_cut)
			{
				if (move_is_into_market)
				{
					if (!LLMarketplace::moveItemInto(item, mUUID))
					{
						// Stop pasting into the marketplace as soon as we get
						// an error
						break;
					}
				}
				else if (mUUID != item->getParentUUID())
				{
					changeItemParent(model, item, mUUID, false);
				}
			}
			else
			{
				if (move_is_into_market)
				{
					if (!LLMarketplace::moveItemInto(item, mUUID, true))
					{
						// Stop pasting into the marketplace as soon as we get
						// an error
						break;
					}
				}
				else
				{
					copy_inventory_item(item->getPermissions().getOwner(),
										item->getUUID(), mUUID);
				}
			}
		}
	}

	model->notifyObservers();
}

//virtual
void LLFolderBridge::pasteLinkFromClipboard()
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model || isInTrash() || isInCOF() || isInMarketplace())
	{
		return;
	}

	// This description should only show if the object cannot find its baseobj:
	const std::string description = "Broken link";
	uuid_vec_t objects;
	gInventoryClipBoard.retrieve(objects);
	for (uuid_vec_t::const_iterator iter = objects.begin(),
									end = objects.end();
		 iter != end; ++iter)
	{
		const LLUUID& object_id = *iter;
		if (LLViewerInventoryItem* item = model->getItem(object_id))
		{
			link_inventory_item(item->getLinkedUUID(), mUUID, description,
								LLAssetType::AT_LINK);
		}
	}
}

//static
void LLFolderBridge::staticFolderOptionsMenu()
{
	if (sSelf)
	{
		sSelf->folderOptionsMenu();
	}
}

void LLFolderBridge::folderOptionsMenu(U32 flags)
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return;

	const LLUUID& trash_id =
		model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
	const LLUUID& laf_id =
		model->findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);
	const LLUUID& cc_id =
		model->findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
	if (laf_id == mUUID)
	{
		// This is the lost+found folder.
		mItems.emplace_back("Empty Lost And Found");
		LLViewerInventoryCategory* laf = getCategory();
		LLInventoryModel::cat_array_t* cat_array;
		LLInventoryModel::item_array_t* item_array;
		model->getDirectDescendentsOf(mUUID, cat_array, item_array);
		// Enable "Empty Lost And Found" menu item only when there is something
		// to act upon. Also do not enable menu if folder is not fully fetched.
		if ((item_array->size() == 0 && cat_array->size() == 0) || !laf ||
			laf->getVersion() == LLViewerInventoryCategory::VERSION_UNKNOWN ||
			!model->isCategoryComplete(mUUID))
		{
			mDisabledItems.emplace_back("Empty Lost And Found");
		}
	}
	else if (trash_id == mUUID)
	{
		// This is the trash.
		mItems.emplace_back("Empty Trash");
		LLViewerInventoryCategory* trash = getCategory();
		LLInventoryModel::cat_array_t* cat_array;
		LLInventoryModel::item_array_t* item_array;
		model->getDirectDescendentsOf(mUUID, cat_array, item_array);
		// Enable "Empty Trash" menu item only when there is something to act
		// upon. Also do not enable menu if folder is not fully fetched.
		if ((item_array->size() == 0 && cat_array->size() == 0) || !trash ||
			trash->getVersion() == LLViewerInventoryCategory::VERSION_UNKNOWN ||
			!model->isCategoryComplete(mUUID))
		{
			mDisabledItems.emplace_back("Empty Trash");
		}
	}
	else if (model->isObjectDescendentOf(mUUID, trash_id))
	{
		// This is a folder in the trash.
		mItems.clear(); // clear any items that used to exist
		const LLInventoryObject* obj = getInventoryObject();
		if (obj && obj->getIsLinkType())
		{
			mItems.emplace_back("Find Original");
			if (isLinkedObjectMissing())
			{
				mDisabledItems.emplace_back("Find Original");
			}
		}
		mItems.emplace_back("Purge Item");
		if (!isItemRemovable())
		{
			mDisabledItems.emplace_back("Purge Item");
		}

		mItems.emplace_back("Restore Item");
	}
	else if (isInMarketplace())
	{
		// Allow to use the clipboard actions
		getClipboardEntries(false, mItems, mDisabledItems, flags);

		if (isInMarketplace())
		{
			LLMarketplace::inventoryContextMenu(this, mUUID, flags, mItems,
												mDisabledItems);
		}
	}
	else
	{
		bool agent_inventory = isAgentInventory();
		const LLUUID& cof_id = gAppearanceMgr.getCOF();
		 // Do not allow creating in library neither in COF
		if (mUUID != cof_id)
		{
			if (agent_inventory)
			{
				mItems.emplace_back("New Folder");
				mItems.emplace_back("New Script");
				mItems.emplace_back("New Note");
				mItems.emplace_back("New Gesture");
				mItems.emplace_back("New Clothes");
				mItems.emplace_back("New Body Parts");
				if (mUUID == cc_id)
				{
					mItems.emplace_back("New Calling Card");
				}
				mItems.emplace_back("New Settings");
				if (!LLEnvironment::isInventoryEnabled())
				{
					mDisabledItems.emplace_back("New Settings");
				}
				mItems.emplace_back("Upload Prefs Separator");
				mItems.emplace_back("Upload Prefs");
			}

			getClipboardEntries(false, mItems, mDisabledItems, flags);
		}
		else if (cof_id == mUUID && LLFolderType::getCanDeleteCOF())
		{
			// Allow to delete the COF when not in use
			mItems.emplace_back("Delete");
		}

		if (!mCallingCards)
		{
			LLIsType is_callingcard(LLAssetType::AT_CALLINGCARD);
			mCallingCards = checkFolderForContentsOfType(model,
														 is_callingcard);
		}
		if (mCallingCards)
		{
			mItems.emplace_back("Calling Card Separator");
			mItems.emplace_back("Conference Chat Folder");
#if 0
			mItems.emplace_back("IM All Contacts In Folder");
#endif
		}

		if (!mWearables)
		{
			LLFindWearables is_wearable;
			LLIsType is_object(LLAssetType::AT_OBJECT);
			LLIsType is_gesture(LLAssetType::AT_GESTURE);
			mWearables = checkFolderForContentsOfType(model, is_wearable) ||
						 checkFolderForContentsOfType(model, is_object) ||
						 checkFolderForContentsOfType(model, is_gesture);
		}
		if (cof_id != mUUID && mWearables)
		{
			mItems.emplace_back("Folder Wearables Separator");
			mItems.emplace_back("Add To Outfit");
			mItems.emplace_back("Wear Items");
			mItems.emplace_back("Replace Outfit");
			if (agent_inventory)
			{
				mItems.emplace_back("Take Off Items");
				mItems.emplace_back("Update Links");
			}
//MK
			if (gRLenabled)
			{
				if (gRLInterface.mContainsDetach &&
					(!gSavedSettings.getBool("RestrainedLoveAllowWear") ||
					 gRLInterface.mContainsDefaultwear))
				{
					mDisabledItems.emplace_back("Add To Outfit");
					mDisabledItems.emplace_back("Wear Items");
					mDisabledItems.emplace_back("Replace Outfit");
					if (agent_inventory)
					{
						mDisabledItems.emplace_back("Take Off Items");
					}
				}
				else
				{
					LLViewerInventoryCategory* cat = model->getCategory(mUUID);
					if (cat && !gRLInterface.canAttachCategory(cat))
					{
						mDisabledItems.emplace_back("Add To Outfit");
						mDisabledItems.emplace_back("Wear Items");
						mDisabledItems.emplace_back("Replace Outfit");
					}
					if (cat && agent_inventory &&
						!gRLInterface.canDetachCategory(cat))
					{
						mDisabledItems.emplace_back("Take Off Items");
					}
				}
			}
//mk
		}
	}

	set_menu_entries_state(*mMenu, mItems, mDisabledItems);
}

bool LLFolderBridge::checkFolderForContentsOfType(LLInventoryModel* model,
												  LLInventoryCollectFunctor& is_type)
{
	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	model->collectDescendentsIf(mUUID, cat_array, item_array,
								LLInventoryModel::EXCLUDE_TRASH, is_type);
	return item_array.size() > 0;
}

//virtual
void LLFolderBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return;

	LLViewerInventoryCategory* category = model->getCategory(mUUID);
	if (!category) return;

	mItems.clear();
	mDisabledItems.clear();
	mCallingCards = mWearables = false;
	mMenu = &menu;
	folderOptionsMenu(flags);

	sSelf = this;
	LLRightClickInventoryFetchDescendentsObserver* fetch =
		new LLRightClickInventoryFetchDescendentsObserver(false);
	LLInventoryFetchDescendentsObserver::folder_ref_t folders;
	folders.emplace_back(category->getUUID());
	fetch->fetchDescendents(folders);
	if (fetch->isFinished())
	{
		// Everything is already here - call done.
		fetch->done();
	}
	else
	{
		// It is all on its way: add an observer, and the inventory will call
		// done for us when everything is here.
		model->addObserver(fetch);
	}
}

//virtual
bool LLFolderBridge::hasChildren() const
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	return model &&
		   model->categoryHasChildren(mUUID) != LLInventoryModel::CHILDREN_NO;
}

//virtual
bool LLFolderBridge::dragOrDrop(MASK mask, bool drop,
								EDragAndDropType cargo_type,
								void* cargo_data,
								std::string& tooltip_msg)
{
	bool accept = false;
	switch (cargo_type)
	{
		case DAD_TEXTURE:
		case DAD_SOUND:
		case DAD_CALLINGCARD:
		case DAD_LANDMARK:
		case DAD_SCRIPT:
		case DAD_OBJECT:
		case DAD_NOTECARD:
		case DAD_CLOTHING:
		case DAD_BODYPART:
		case DAD_ANIMATION:
		case DAD_GESTURE:
#if LL_MESH_ASSET_SUPPORT
		case DAD_MESH:
#endif
		case DAD_SETTINGS:
		case DAD_LINK:
			accept = dragItemIntoFolder((LLInventoryItem*)cargo_data, drop,
										tooltip_msg);
			break;

		case DAD_CATEGORY:
			accept = dragCategoryIntoFolder((LLInventoryCategory*)cargo_data,
											drop, tooltip_msg);
			break;

		default:
			break;
	}

	return accept;
}

LLViewerInventoryCategory* LLFolderBridge::getCategory() const
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	return model ? model->getCategory(mUUID) : NULL;
}

//static
void LLFolderBridge::pasteClipboard(void* user_data)
{
	LLFolderBridge* self = (LLFolderBridge*)user_data;
	if (self)
	{
		self->pasteFromClipboard();
	}
}

//static
void LLFolderBridge::createNewCategory(void* user_data)
{
	LLFolderBridge* self = (LLFolderBridge*)user_data;
	if (!self) return;

	LLInventoryPanel* panel = self->mInventoryPanel;

	LLInventoryModel* model = panel->getModel();
	if (!model) return;

	LLUUID id = model->createNewCategory(self->getUUID(),
										 LLFolderType::FT_NONE,
										 LLStringUtil::null);
	model->notifyObservers();

	// At this point, the self has probably been deleted, but the view is
	// still there.
	panel->setSelection(id, TAKE_FOCUS_YES);
}

// Separate method so can be called by global menu as well as right-click menu
void LLFolderBridge::modifyOutfit(bool append, bool replace)
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return;
	LLViewerInventoryCategory* cat = getCategory();
	if (!cat) return;
//MK
	if (gRLenabled && !gRLInterface.canAttachCategory(cat))
	{
		return;
	}
//mk
	if (!isAgentInventory())
	{
		// If in library, copy then add to/replace outfit
		if (!append &&
//MK
			(!gRLenabled || gRLInterface.canDetachCategory(cat)))
//mk
		{
			LLAgentWearables::userRemoveAllAttachments();
			LLAgentWearables::userRemoveAllClothes();
		}
		LLPointer<LLInventoryCategory> category =
			new LLInventoryCategory(mUUID, LLUUID::null,
									LLFolderType::FT_CLOTHING,
									"Quick Appearance");
		gAppearanceMgr.wearInventoryCategory(category, true, !replace);
	}
	else
	{
		gAppearanceMgr.wearInventoryCategoryOnAvatar(cat, append, replace);
	}
}

bool LLFolderBridge::dragItemIntoFolder(LLInventoryItem* inv_item, bool drop,
										std::string& tooltip_msg)
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!inv_item || !model || !isAgentInventory() || !isAgentAvatarValid() ||
		isInCOF())
	{
		return false;
	}

	const LLUUID& market_id = LLMarketplace::getMPL();
	const LLUUID& from_folder_uuid = inv_item->getParentUUID();

	bool move_is_into_market = model->isObjectDescendentOf(mUUID, market_id);
	bool move_is_from_market = model->isObjectDescendentOf(inv_item->getUUID(),
														   market_id);
	bool move_is_into_trash = isInTrash();

	bool accept = false;
	LLViewerObject* object = NULL;
	LLToolDragAndDrop* dadtoolp = LLToolDragAndDrop::getInstance();
	LLToolDragAndDrop::ESource source = dadtoolp->getSource();
	if (source == LLToolDragAndDrop::SOURCE_AGENT)
	{
		bool movable = true;
		if (inv_item->getActualType() == LLAssetType::AT_CATEGORY)
		{
			movable = !((LLViewerInventoryCategory*)inv_item)->isProtected();
		}

		if (movable && move_is_into_trash)
		{
			movable = inv_item->getIsLinkType() ||
					  !get_is_item_worn(inv_item->getUUID(), false);
		}

		if (move_is_into_market && !move_is_from_market)
		{
			const LLViewerInventoryCategory* master_folder;
			master_folder = model->getFirstDescendantOf(market_id, mUUID);

			S32 count = dadtoolp->getCargoCount() - dadtoolp->getCargoIndex();
            LLViewerInventoryCategory* dest_folder = getCategory();
			accept = LLMarketplace::canMoveItemInto(master_folder, dest_folder,
													(LLViewerInventoryItem*)inv_item,
													tooltip_msg, count,
													move_is_into_market);
		}
		else
		{
			accept = movable && mUUID != from_folder_uuid;
		}

		// Check that the folder can accept this item based on folder/item type
		// compatibility (e.g. stock folder compatibility)
		if (accept)
		{
			LLViewerInventoryCategory* dest_folder = getCategory();
			accept = dest_folder->acceptItem(inv_item);
		}

		if (accept && !drop && (move_is_into_market || move_is_from_market))
		{
			LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
			if ((move_is_from_market &&
				 (marketdata->isInActiveFolder(inv_item->getUUID()) ||
				  marketdata->isListedAndActive(inv_item->getUUID()))) ||
				(move_is_into_market && marketdata->isInActiveFolder(mUUID)))
			{
				if (!tooltip_msg.empty())
				{
					tooltip_msg += ' ';
				}
				tooltip_msg += LLTrans::getString("TipMerchantActiveChange");
			}
			if (move_is_into_market && !move_is_from_market)
			{
				if (!tooltip_msg.empty())
				{
					tooltip_msg += ' ';
				}
				tooltip_msg += LLTrans::getString("TipMerchantMoveInventory");
			}
		}

		if (accept && drop)
		{
			if (move_is_into_trash &&
				inv_item->getType() == LLAssetType::AT_GESTURE &&
				gGestureManager.isGestureActive(inv_item->getUUID()))
			{
				gGestureManager.deactivateGesture(inv_item->getUUID());
			}
			// If an item is being dragged between windows, unselect everything
			// in the active window so that we don't follow the selection to
			// its new location (which is very annoying).
			if (LLFloaterInventory::getActiveFloater())
			{
				LLInventoryPanel* active_floater =
					LLFloaterInventory::getActiveFloater()->getPanel();
				if (active_floater && mInventoryPanel != active_floater)
				{
					active_floater->unSelectAll();
				}
			}

			if (move_is_into_market)
			{
				LLMarketplace::moveItemInto((LLViewerInventoryItem*)inv_item,
											mUUID);
			}
			else
			{
				changeItemParent(model, (LLViewerInventoryItem*)inv_item,
								 mUUID, move_is_into_trash);
			}
#if 0
			if (move_is_from_market)
			{
				LLMarketplace::updateMovedFrom(from_folder_uuid);
			}
#endif
		}
	}
	else if (source == LLToolDragAndDrop::SOURCE_WORLD)
	{
		// Make sure the object exists. If we allowed dragging from anonymous
		// objects, it would be possible to bypass permissions.
		object = gObjectList.findObject(inv_item->getParentUUID());
		if (!object)
		{
			llinfos << "Object not found for drop." << llendl;
			return false;
		}

		// Coming from a task. Need to figure out if the person can move/copy
		// this item.
		LLPermissions perm(inv_item->getPermissions());
		bool is_move = false;
		if (perm.allowCopyBy(gAgentID, gAgent.getGroupID()) &&
			perm.allowTransferTo(gAgentID))	//   || gAgent.isGodlike())
		{
			accept = true;
		}
		else if (object->permYouOwner())
		{
			// If the object cannot be copied, but the object the inventory is
			// owned by the agent, then the item can be moved from the task to
			// agent inventory.
			is_move = accept = true;
		}
		if (move_is_into_market)
		{
			accept = false;
		}
		if (drop && accept)
		{
			LLMoveInv* move_inv = new LLMoveInv;
			move_inv->mObjectID = inv_item->getParentUUID();
			move_inv->mMoveList.emplace_back(mUUID, inv_item->getUUID());
			move_inv->mCallback = NULL;
			move_inv->mUserData = NULL;
			if (is_move)
			{
				warn_move_inventory(object, move_inv);
			}
			else
			{
				LLNotification::Params params("MoveInventoryFromObject");
				params.functor(boost::bind(move_task_inventory_callback,
										   _1, _2, move_inv));
				gNotifications.forceResponse(params, 0);
			}
		}

	}
	else if (source == LLToolDragAndDrop::SOURCE_NOTECARD)
	{
		accept = !move_is_into_market;
		if (accept && inv_item->getActualType() == LLAssetType::AT_SETTINGS)
		{
			accept = LLEnvironment::isInventoryEnabled();
		}
		if (accept && drop)
		{
			copy_inventory_from_notecard(dadtoolp->getObjectID(),
										 dadtoolp->getSourceID(), inv_item);
		}
	}
	else if (source == LLToolDragAndDrop::SOURCE_LIBRARY)
	{
		LLViewerInventoryItem* item = (LLViewerInventoryItem*)inv_item;
		if (item && item->isFinished())
		{
			accept = !move_is_into_market;
			if (accept && drop)
			{
				copy_inventory_item(inv_item->getPermissions().getOwner(),
									inv_item->getUUID(), mUUID);
			}
		}
	}
	else
	{
		llwarns << "Unhandled drag source" << llendl;
	}

	if (accept && drop && move_is_into_trash)
	{
		model->checkTrashOverflow();
	}

	return accept;
}

///////////////////////////////////////////////////////////////////////////////
// LLScriptBridge (DEPRECATED)
///////////////////////////////////////////////////////////////////////////////

//virtual
LLUIImagePtr LLScriptBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_SCRIPT,
									LLInventoryType::IT_LSL, 0, false);
}

///////////////////////////////////////////////////////////////////////////////
// LLTextureBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLTextureBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Texture") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLTextureBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_TEXTURE, mInvType, 0,
									false);
}

//virtual
void LLTextureBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	if (item)
	{
		open_texture(mUUID, getPrefix() + item->getName(), false);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLSoundBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLSoundBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Sound") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLSoundBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_SOUND,
									LLInventoryType::IT_SOUND, 0, false);
}

//virtual
void LLSoundBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	if (item)
	{
		openSoundPreview((void*)this);
	}
}

//virtual
void LLSoundBridge::previewItem()
{
	LLViewerInventoryItem* item = getItem();
	if (!item) return;

	U32 action = gSavedSettings.getU32("DoubleClickInventorySoundAction");
	if (action == 0)
	{
		openSoundPreview((void*)this);
	}
	else if (action == 1 && gAudiop)
	{
		// Play the sound locally.
		LLVector3d lpos_global = gAgent.getPositionGlobal();
		gAudiop->triggerSound(item->getAssetUUID(), gAgentID, 1.0,
							  LLAudioEngine::AUDIO_TYPE_UI, lpos_global);
	}
	else if (action == 2)
	{
		// Play the sound in-world.
		send_sound_trigger(item->getAssetUUID(), 1.0);
	}
}

//static
void LLSoundBridge::openSoundPreview(void* userdata)
{
	LLSoundBridge* self = (LLSoundBridge*)userdata;
	if (self && !LLPreview::show(self->mUUID))
	{
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewSoundRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLPreviewSound* preview;
		preview = new LLPreviewSound("preview sound", rect,
									 self->getPrefix() + self->getName(),
									 self->mUUID);
		preview->setFocus(true);
		// Keep entirely onscreen.
		gFloaterViewp->adjustToFitScreen(preview);
	}
}

//virtual
void LLSoundBridge::performAction(LLFolderView* folder,
								  LLInventoryModel* model,
								  const std::string& action)
{
	if (action == "playworld")
	{
		LLViewerInventoryItem* item = getItem();
		if (item)
		{
			send_sound_trigger(item->getAssetUUID(), 1.0);
		}
	}
	else if (action == "playlocal")
	{
		LLViewerInventoryItem* item = getItem();
		if (item && gAudiop)
		{
			// Play the sound locally.
			LLVector3d lpos_global = gAgent.getPositionGlobal();
			gAudiop->triggerSound(item->getAssetUUID(), gAgentID, 1.0,
								  LLAudioEngine::AUDIO_TYPE_UI, lpos_global);
		}
	}
	else
	{
		LLItemBridge::performAction(folder, model, action);
	}
}

//virtual
void LLSoundBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Sound Open");
		items.emplace_back("Sound Play1");
		items.emplace_back("Sound Play2");
		items.emplace_back("Sound Separator");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);
	}

	set_menu_entries_state(menu, items, disabled_items);
}

///////////////////////////////////////////////////////////////////////////////
// LLLandmarkBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLLandmarkBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Landmark") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLLandmarkBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_LANDMARK,
									LLInventoryType::IT_LANDMARK, mVisited,
									false);
}

//virtual
void LLLandmarkBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Landmark Open");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);
	}

	items.emplace_back("Landmark Separator");
	items.emplace_back("About Landmark");
	items.emplace_back("Show on Map");

	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
void LLLandmarkBridge::performAction(LLFolderView* folder,
									 LLInventoryModel* model,
									 const std::string& action)
{
	if (action == "teleport")
	{
		LLViewerInventoryItem* item = getItem();
		if (item)
		{
			gAgent.teleportViaLandmark(item->getAssetUUID());

			// We now automatically track the landmark you're teleporting to
			// because you'll probably arrive at a telehub instead
			if (gFloaterWorldMapp)
			{
				// remember this must be the item UUID, not the asset UUID
				gFloaterWorldMapp->trackLandmark(item->getUUID());
			}
		}
	}
	else if (action == "about")
	{
		LLViewerInventoryItem* item = getItem();
		if (item)
		{
			std::string title = "  " + getPrefix() + item->getName();
			open_landmark(item, title, false);
		}
	}
	else if (action == "show_on_map")
	{
		LLViewerInventoryItem* item = getItem();
		if (item)
		{
			const LLUUID& asset_id = item->getAssetUUID();
			if (asset_id.isNull()) return;	// Paranoia
			LLLandmark* lm;
			lm = gLandmarkList.getAsset(asset_id,
										boost::bind(&LLLandmarkBridge::showOnMap,
													this, _1));
			if (lm)
			{
				showOnMap(lm);
			}
		}
	}
	else
	{
		LLItemBridge::performAction(folder, model, action);
	}
}

//virtual
void LLLandmarkBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	if (item)
	{
		// Double-clicking a landmark immediately teleports, but warns you the
		// first time.
		LLSD payload;
		payload["asset_id"] = item->getAssetUUID();
		payload["item_id"] = item->getUUID();
		gNotifications.add("TeleportFromLandmark", LLSD(), payload);
	}
}

void LLLandmarkBridge::showOnMap(LLLandmark* landmark)
{
	LLVector3d pos;
	if (landmark && landmark->getGlobalPos(pos) && gFloaterWorldMapp)
	{
		if (!pos.isExactlyZero())
		{
			gFloaterWorldMapp->trackLocation(pos);
			LLFloaterWorldMap::show(NULL, true);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLCallingCardBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
void LLCallingCardObserver::changed(U32 mask)
{
	mBridgep->refreshFolderViewItem();
}

//virtual
const std::string& LLCallingCardBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Calling card") +
									  ": ";
	return prefix;
}

LLCallingCardBridge::LLCallingCardBridge(LLInventoryPanel* inventory,
										 const LLUUID& uuid)
:	LLItemBridge(inventory, uuid)
{
	mObserver = new LLCallingCardObserver(this);
	gAvatarTracker.addObserver(mObserver);
}

//virtual
LLCallingCardBridge::~LLCallingCardBridge()
{
	gAvatarTracker.removeObserver(mObserver);
	delete mObserver;
}

void LLCallingCardBridge::refreshFolderViewItem()
{
	LLFolderViewItem* itemp =
		mInventoryPanel->getRootFolder()->getItemByID(mUUID);
	if (itemp)
	{
		itemp->refresh();
	}
}

//virtual
void LLCallingCardBridge::performAction(LLFolderView* folder,
										LLInventoryModel* model,
										const std::string& action)
{
	if (action == "begin_im")
	{
		LLViewerInventoryItem* item = getItem();
		const LLUUID& id = item ? item->getCreatorUUID() : LLUUID::null;
		if (id.notNull() && id != gAgentID)
		{
			LLAvatarActions::startIM(id);
		}
	}
	else if (action == "lure")
	{
		LLViewerInventoryItem* item = getItem();
		const LLUUID& id = item ? item->getCreatorUUID() : LLUUID::null;
		if (id.notNull() && id != gAgentID)
		{
			LLAvatarActions::offerTeleport(id);
		}
	}
	else if (action == "request_teleport")
	{
		LLViewerInventoryItem* item = getItem();
		const LLUUID& id = item ? item->getCreatorUUID() : LLUUID::null;
		if (id.notNull() && id != gAgentID)
		{
			LLAvatarActions::teleportRequest(id);
		}
	}
	else
	{
		LLItemBridge::performAction(folder, model, action);
	}
}

//virtual
LLUIImagePtr LLCallingCardBridge::getIcon() const
{
	bool online = false;
	LLViewerInventoryItem* item = getItem();
	const LLUUID& id = item ? item->getCreatorUUID() : LLUUID::null;
	if (id.notNull() && id != gAgentID)
	{
		online = gAvatarTracker.isBuddyOnline(id);
	}
	return LLInventoryIcon::getIcon(LLAssetType::AT_CALLINGCARD,
									LLInventoryType::IT_CALLINGCARD,
									online, false);
}

//virtual
std::string LLCallingCardBridge::getLabelSuffix() const
{
	static const std::string online = " (" + LLTrans::getString("online") +
									  ")";
	LLViewerInventoryItem* item = getItem();
	const LLUUID& id = item ? item->getCreatorUUID() : LLUUID::null;
	if (id.notNull() && id != gAgentID && gAvatarTracker.isBuddyOnline(id))
	{
		return LLItemBridge::getLabelSuffix() + online;
	}
	else
	{
		return LLItemBridge::getLabelSuffix();
	}
}

//virtual
void LLCallingCardBridge::openItem()
{
	open_callingcard((LLViewerInventoryItem*)getItem());
}

//virtual
void LLCallingCardBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Open");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);

		LLViewerInventoryItem* item = getItem();
		bool good_card = item && item->getCreatorUUID().notNull() &&
						 item->getCreatorUUID() != gAgentID;
		bool user_online = false;
		if (item)
		{
			user_online = gAvatarTracker.isBuddyOnline(item->getCreatorUUID());
		}
		items.emplace_back("Send Instant Message Separator");
		items.emplace_back("Send Instant Message");
		items.emplace_back("Offer Teleport...");
		items.emplace_back("Request Teleport...");
		items.emplace_back("Conference Chat");

		if (!good_card)
		{
			disabled_items.emplace_back("Send Instant Message");
		}
		if (!good_card || !user_online)
		{
			disabled_items.emplace_back("Offer Teleport...");
			disabled_items.emplace_back("Request Teleport...");
			disabled_items.emplace_back("Conference Chat");
		}
	}
	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
bool LLCallingCardBridge::dragOrDrop(MASK mask, bool drop,
									 EDragAndDropType cargo_type,
									 void* cargo_data,
									 std::string& tooltip_msg)
{
	LLViewerInventoryItem* item = getItem();
	if (item && cargo_data)
	{
		// Check the type
		switch (cargo_type)
		{
			case DAD_TEXTURE:
			case DAD_SOUND:
			case DAD_LANDMARK:
			case DAD_SCRIPT:
			case DAD_CLOTHING:
			case DAD_OBJECT:
			case DAD_NOTECARD:
			case DAD_BODYPART:
			case DAD_ANIMATION:
			case DAD_GESTURE:
#if LL_MESH_ASSET_SUPPORT
			case DAD_MESH:
#endif
			case DAD_SETTINGS:
			{
				LLInventoryItem* inv_item = (LLInventoryItem*)cargo_data;
				const LLPermissions& perm = inv_item->getPermissions();
				if (gInventory.getItem(inv_item->getUUID()) &&
					perm.allowOperationBy(PERM_TRANSFER, gAgentID))
				{
					if (drop)
					{
						LLToolDragAndDrop::giveInventory(item->getCreatorUUID(),
														 inv_item);
					}
					return true;
				}
				else
				{
					// It is not in the user's inventory (it is probably in an
					// object's contents), so disallow dragging it here. You
					// cannot give something you do not yet have.
					return false;
				}
				break;
			}

			case DAD_CATEGORY:
			{
				LLInventoryCategory* inv_cat = (LLInventoryCategory*)cargo_data;
				if (gInventory.getCategory(inv_cat->getUUID()))
				{
					if (drop)
					{
						LLToolDragAndDrop::giveInventoryCategory(item->getCreatorUUID(),
																 inv_cat);
					}
					return true;
				}
				else
				{
					// It is not in the user's inventory (it is probably in an
					// object's contents), so disallow dragging it here. You
					// cannot give something you do not yet have.
					return false;
				}
				break;
			}

			default:
				break;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// LLNotecardBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLNotecardBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Note") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLNotecardBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_NOTECARD,
									LLInventoryType::IT_NOTECARD, 0, false);
}

//virtual
void LLNotecardBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	if (item)
	{
		open_notecard(item, getPrefix() + item->getName(), LLUUID::null,
					  false);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLGestureBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLGestureBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Gesture") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLGestureBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_GESTURE,
									LLInventoryType::IT_GESTURE, 0, false);
}

//virtual
LLFontGL::StyleFlags LLGestureBridge::getLabelStyle() const
{
	S32 font = LLFontGL::NORMAL;

	if (gGestureManager.isGestureActive(mUUID))
	{
		font |= LLFontGL::BOLD;
	}

	const LLViewerInventoryItem* item = getItem();
	if (item && item->getIsLinkType())
	{
		font |= LLFontGL::ITALIC;
	}

	return (LLFontGL::StyleFlags)font;
}

//virtual
std::string LLGestureBridge::getLabelSuffix() const
{
	static const std::string active = " (" + LLTrans::getString("active") +
									  ")";
	if (gGestureManager.isGestureActive(mUUID))
	{
		return LLItemBridge::getLabelSuffix() + active;
	}
	else
	{
		return LLItemBridge::getLabelSuffix();
	}
}

//virtual
void LLGestureBridge::performAction(LLFolderView* folder,
									LLInventoryModel* model,
									const std::string& action)
{
	if (action == "activate")
	{
		gGestureManager.activateGesture(mUUID);

		LLViewerInventoryItem* item = gInventory.getItem(mUUID);
		if (!item) return;

		// Since we just changed the suffix to indicate (active)
		// the server doesn't need to know, just the viewer.
		gInventory.updateItem(item);
		gInventory.notifyObservers();
	}
	else if (action == "deactivate")
	{
		gGestureManager.deactivateGesture(mUUID);

		LLViewerInventoryItem* item = gInventory.getItem(mUUID);
		if (!item) return;

		// Since we just changed the suffix to indicate (active)
		// the server doesn't need to know, just the viewer.
		gInventory.updateItem(item);
		gInventory.notifyObservers();
	}
	else
	{
		LLItemBridge::performAction(folder, model, action);
	}
}

//virtual
void LLGestureBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	// See if we can bring an existing preview to the front
	if (item && !LLPreview::show(mUUID))
	{
		LLUUID item_id = mUUID;
		std::string title = getPrefix() + item->getName();
		LLUUID object_id;

		// TODO: save the rectangle
		LLPreviewGesture* preview;
		preview = LLPreviewGesture::show(title, item_id, object_id);
		preview->setFocus(true);

		// Force to be entirely onscreen.
		gFloaterViewp->adjustToFitScreen(preview);
	}
}

//virtual
bool LLGestureBridge::removeItem()
{
	// Force close the preview window, if it exists
	gGestureManager.deactivateGesture(mUUID);
	return LLItemBridge::removeItem();
}

//virtual
void LLGestureBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;
	if (isInTrash())
	{
		const LLInventoryObject* obj = getInventoryObject();
		if (obj && obj->getIsLinkType())
		{
			items.emplace_back("Find Original");
			if (isLinkedObjectMissing())
			{
				disabled_items.emplace_back("Find Original");
			}
		}
		items.emplace_back("Purge Item");
		if (!isItemRemovable())
		{
			disabled_items.emplace_back("Purge Item");
		}

		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Gesture Open");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);

		if (!isInMarketplace())
		{
			items.emplace_back("Gesture Separator");
			items.emplace_back("Activate");
			items.emplace_back("Deactivate");
		}
	}
	set_menu_entries_state(menu, items, disabled_items);
}

///////////////////////////////////////////////////////////////////////////////
// LLAnimationBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLAnimationBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Animation") + ": ";
	return prefix;
}

LLUIImagePtr LLAnimationBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_ANIMATION,
									LLInventoryType::IT_ANIMATION, 0, false);
}

//virtual
void LLAnimationBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Animation Open");
		items.emplace_back("Animation Play");
		items.emplace_back("Animation Audition");
		items.emplace_back("Animation Separator");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);
	}

	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
void LLAnimationBridge::performAction(LLFolderView* folder,
									  LLInventoryModel* model,
									  const std::string& action)
{
	S32 activate = 0;

	if (action == "playworld" || action == "playlocal")
	{
		activate = action == "playworld" ? 1 : 2;

		// See if we can bring an existing preview to the front
		if (!LLPreview::show(mUUID))
		{
			// There is not one, so make a new preview
			LLViewerInventoryItem* item = getItem();
			if (item)
			{
				S32 left, top;
				gFloaterViewp->getNewFloaterPosition(&left, &top);
				LLRect rect = gSavedSettings.getRect("PreviewAnimRect");
				rect.translate(left - rect.mLeft, top - rect.mTop);
				LLPreviewAnim* preview =
					new LLPreviewAnim("preview anim", rect,
									  getPrefix() + item->getName(), mUUID,
									  activate);
				// Force to be entirely onscreen.
				gFloaterViewp->adjustToFitScreen(preview);
			}
		}
	}
	else
	{
		LLItemBridge::performAction(folder, model, action);
	}
}

//virtual
void LLAnimationBridge::openItem()
{
	// See if we can bring an existing preview to the front
	if (!LLPreview::show(mUUID))
	{
		// There is not one, so make a new preview
		LLViewerInventoryItem* item = getItem();
		if (item)
		{
			S32 left, top;
			gFloaterViewp->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("PreviewAnimRect");
			rect.translate(left - rect.mLeft, top - rect.mTop);
			LLPreviewAnim* preview =
				new LLPreviewAnim("preview anim", rect,
								  getPrefix() + item->getName(), mUUID, 0);
			preview->setFocus(true);
			// Force to be entirely onscreen.
			gFloaterViewp->adjustToFitScreen(preview);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLObjectBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLObjectBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Object") + ": ";
	return prefix;
}

//virtual
bool LLObjectBridge::isItemRemovable()
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return false;
	}

	const LLInventoryObject* obj = model->getItem(mUUID);
	if (obj && obj->getIsLinkType())
	{
		return true;
	}

	if (!isAgentAvatarValid() || gAgentAvatarp->isWearingAttachment(mUUID))
	{
		return false;
	}

	return LLInvFVBridge::isItemRemovable();
}

//virtual
LLUIImagePtr LLObjectBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_OBJECT, mInvType,
									mAttachPt, mIsMultiObject);
}

//virtual
void LLObjectBridge::performAction(LLFolderView* folder,
								   LLInventoryModel* model,
								   const std::string& action)
{
	if (action == "attach" || action == "attach_add")
	{
		bool replace = action == "attach"; // Replace if "Wear"ing.
		LLUUID object_id = gInventory.getLinkedItemID(mUUID);
		LLViewerInventoryItem* item = gInventory.getItem(object_id);
		if (item &&
			gInventory.isObjectDescendentOf(object_id,
											gInventory.getRootFolderID()))
		{
//MK
			if (gRLenabled && gRLInterface.canAttach(item))
			{
				LLViewerJointAttachment* attachmentp = NULL;
				// If it is a no-mod item, the containing folder has priority
				// to decide where to wear it
				if (!item->getPermissions().allowModifyBy(gAgentID))
				{
					attachmentp =
						gRLInterface.findAttachmentPointFromParentName(item);
					if (attachmentp)
					{
						gAppearanceMgr.rezAttachment(item, attachmentp,
													 replace);
					}
					else
					{
						// But the name itself could also have the information
						// => check
						attachmentp =
							gRLInterface.findAttachmentPointFromName(item->getName());
						if (attachmentp)
						{
							gAppearanceMgr.rezAttachment(item, attachmentp,
														 replace);
						}
						else if (!gRLInterface.mContainsDefaultwear &&
								 gSavedSettings.getBool("RestrainedLoveAllowWear"))
						{
							gAppearanceMgr.rezAttachment(item, NULL, replace);
						}
					}
				}
				else
				{
					// This is a mod item, wear it according to its name
					attachmentp =
						gRLInterface.findAttachmentPointFromName(item->getName());
					if (attachmentp)
					{
						gAppearanceMgr.rezAttachment(item, attachmentp,
													 replace);
					}
					else if (!gRLInterface.mContainsDefaultwear &&
							 gSavedSettings.getBool("RestrainedLoveAllowWear"))
					{
						gAppearanceMgr.rezAttachment(item, NULL, replace);
					}
				}
			}
			else
//mk
			{
				gAppearanceMgr.rezAttachment(item, NULL, replace);
			}
		}
		else if (item && item->isFinished())
		{
			// must be in the inventory library. Copy it to our inventory
			// and put it on right away.
			LLPointer<LLInventoryCallback> cb =
				new LLRezAttachmentCallback(NULL, replace);
			copy_inventory_item(item->getPermissions().getOwner(),
								item->getUUID(), LLUUID::null, std::string(),
								cb);
		}
		else if (item)
		{
			// *TODO: We should fetch the item details, and then do
			// the operation above.
			gNotifications.add("CannotWearInfoNotComplete");
		}
		gFocusMgr.setKeyboardFocus(NULL);
	}
	else if (action == "detach")
	{
		LLViewerInventoryItem* item = gInventory.getItem(mUUID);
		if (item)
		{
			LLVOAvatarSelf::detachAttachmentIntoInventory(item->getLinkedUUID());
		}
	}
	else if (action == "edit" || action == "inspect")
	{
		LLViewerInventoryItem* item = gInventory.getItem(mUUID);
		if (item && isAgentAvatarValid())
		{
			LLViewerObject* vobj =
				gAgentAvatarp->getWornAttachment(item->getLinkedUUID());
			if (vobj)
			{
				gSelectMgr.deselectAll();
				gSelectMgr.selectObjectAndFamily(vobj);
				if (action == "edit")
				{
					handle_object_edit();
				}
				else
				{
					handle_object_inspect();
				}
			}
		}
	}
	else
	{
		LLItemBridge::performAction(folder, model, action);
	}
}

//virtual
void LLObjectBridge::openItem()
{
	if (isAgentAvatarValid() && !isInMarketplace())
	{
		if (gAgentAvatarp->isWearingAttachment(mUUID))
		{
//MK
			if (gRLenabled &&
				!gRLInterface.canDetach(gAgentAvatarp->getWornAttachment(mUUID)))
			{
				return;
			}
//mk
			performAction(NULL, NULL, "detach");
		}
		else
		{
			performAction(NULL, NULL, "attach");
		}
	}
}

//virtual
LLFontGL::StyleFlags LLObjectBridge::getLabelStyle() const
{
	S32 font = LLFontGL::NORMAL;

	if (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(mUUID))
	{
		font |= LLFontGL::BOLD;
	}

	const LLViewerInventoryItem* item = getItem();
	if (item && item->getIsLinkType())
	{
		font |= LLFontGL::ITALIC;
	}

	return (LLFontGL::StyleFlags)font;
}

//virtual
std::string LLObjectBridge::getLabelSuffix() const
{
	static const std::string wornon = " (" + LLTrans::getString("wornon") +
									  " ";
	std::string suffix = LLItemBridge::getLabelSuffix();
	if (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(mUUID))
	{
		suffix += wornon;
		suffix += gAgentAvatarp->getAttachedPointName(mUUID, true) + ")";
	}
	return suffix;
}

//virtual
void LLObjectBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return;
	}
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;
	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		if (!isItemRemovable())
		{
			disabled_items.emplace_back("Purge Item");
		}

		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Properties");
#if LL_RESTORE_TO_WORLD
		if (isInLostAndFound())
		{
			items.emplace_back("Restore to Last Position");
		}
#endif

		getClipboardEntries(true, items, disabled_items, flags);

		LLViewerInventoryItem* item = getItem();
		if (item && !isInMarketplace())
		{
			if (!isAgentAvatarValid())
			{
				return;
			}

			if (gAgentAvatarp->isWearingAttachment(mUUID))
			{
				items.emplace_back("Attach Separator");
				items.emplace_back("Detach From Yourself");
				items.emplace_back("Edit");
				items.emplace_back("Inspect");
				bool disable_edit = (flags & FIRST_SELECTED_ITEM) == 0;
				bool disable_inspect = disable_edit;
//MK
				if (gRLenabled)
				{
					if (gRLInterface.mContainsRez ||
						gRLInterface.mContainsEdit)
					{
						disable_edit = true;
					}
					if (gRLInterface.mContainsShownames ||
						gRLInterface.mContainsShownametags)
					{
						disable_inspect = true;
					}
					if (!gRLInterface.canDetach(gAgentAvatarp->getWornAttachment(mUUID)))
					{
						disabled_items.emplace_back("Detach From Yourself");
					}
				}
//mk
				if (disable_edit)
				{
					disabled_items.emplace_back("Edit");
				}
				if (disable_inspect)
				{
					disabled_items.emplace_back("Inspect");
				}
			}
			else if (isAgentInventory())
			{
				items.emplace_back("Attach Separator");
				items.emplace_back("Object Wear");
				items.emplace_back("Object Add");
				if (!gAgentAvatarp->canAttachMoreObjects())
				{
					disabled_items.emplace_back("Object Add");
				}
				items.emplace_back("Attach To");
				items.emplace_back("Attach To HUD");
//MK
				if (gRLenabled && gRLInterface.mContainsDetach &&
					(gRLInterface.mContainsDefaultwear ||
					 !gSavedSettings.getBool("RestrainedLoveAllowWear")) &&
					 !gRLInterface.findAttachmentPointFromName(item->getName()) &&
					 !gRLInterface.findAttachmentPointFromParentName(item))
				{
					disabled_items.emplace_back("Object Wear");
				}
//mk

				LLMenuGL* attach_menu;
				attach_menu = menu.getChildMenuByName("Attach To", true);
				LLMenuGL* attach_hud_menu;
				attach_hud_menu = menu.getChildMenuByName("Attach To HUD",
														  true);
				if (attach_menu && attach_menu->getChildCount() == 0 &&
					attach_hud_menu && attach_hud_menu->getChildCount() == 0)
				{
					std::string name;
					for (LLVOAvatar::attachment_map_t::iterator
							iter = gAgentAvatarp->mAttachmentPoints.begin(),
							end = gAgentAvatarp->mAttachmentPoints.end();
						 iter != end; ++iter)
					{
						LLViewerJointAttachment* attachment = iter->second;
						if (!attachment) continue;	// Paranoia

						name = LLTrans::getString(attachment->getName());
						LLMenuItemCallGL* new_item =
							new LLMenuItemCallGL(name, NULL, NULL,
												 &attach_label,
												 (void*)attachment);
						if (attachment->getIsHUDAttachment())
						{
							attach_hud_menu->append(new_item);
						}
						else
						{
							attach_menu->append(new_item);
						}
						LLSimpleListener* callback =
							mInventoryPanel->getListenerByName("Inventory.AttachObject");
						if (callback)
						{
							new_item->addListener(callback, "on_click",
												  LLSD(name));
						}
					}
				}
//MK
				if (gRLenabled && !gRLInterface.canAttach(item))
				{
					disabled_items.emplace_back("Object Wear");
					disabled_items.emplace_back("Object Add");
					disabled_items.emplace_back("Attach To");
					disabled_items.emplace_back("Attach To HUD");
				}
//mk
			}
		}
	}
	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
bool LLObjectBridge::renameItem(const std::string& new_name)
{
	if (!isItemRenameable())
	{
		return false;
	}

	LLPreview::rename(mUUID, getPrefix() + new_name);

	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return false;
	}

	LLViewerInventoryItem* item = getItem();
	if (item && item->getName() != new_name)
	{
		LLPointer<LLViewerInventoryItem> new_item;
		new_item = new LLViewerInventoryItem(item);
		new_item->rename(new_name);
		buildDisplayName(new_item, mDisplayName);
		new_item->updateServer(false);
		model->updateItem(new_item);
		model->notifyObservers();

		if (isAgentAvatarValid())
		{
			LLViewerObject* obj;
			obj = gAgentAvatarp->getWornAttachment(item->getUUID());
			if (obj)
			{
				gSelectMgr.deselectAll();
				gSelectMgr.addAsIndividual(obj, SELECT_ALL_TES, false);
				gSelectMgr.selectionSetObjectName(new_name);
				gSelectMgr.deselectAll();
			}
		}
	}

	// Return false because we either notified observers (and therefore
	// rebuilt) or we did not update.
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// LLLSLTextBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLLSLTextBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Script") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLLSLTextBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_SCRIPT,
									LLInventoryType::IT_LSL, 0, false);
}

//virtual
void LLLSLTextBridge::openItem()
{
//MK
	if (gRLenabled && gRLInterface.mContainsViewscript)
	{
		return;
	}
//mk
	// See if we can bring an exiting preview to the front
	if (!LLPreview::show(mUUID))
	{
		LLViewerInventoryItem* item = getItem();
		if (item)
		{
			// There is not one, so make a new preview
			S32 left, top;
			gFloaterViewp->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("PreviewScriptRect");
			rect.translate(left - rect.mLeft, top - rect.mTop);

			LLPreviewScript* preview =
				new LLPreviewScript("preview lsl text", rect,
									getPrefix() + item->getName(), mUUID);
			preview->setFocus(true);
			// Keep on screen
			gFloaterViewp->adjustToFitScreen(preview);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLWearableBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
bool LLWearableBridge::renameItem(const std::string& new_name)
{
	if (gAgentWearables.isWearingItem(mUUID))
	{
		gAgentWearables.setWearableName(mUUID, new_name);
	}
	return LLItemBridge::renameItem(new_name);
}

//virtual
bool LLWearableBridge::isItemRemovable()
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return false;
	}

	const LLViewerInventoryItem* item = model->getItem(mUUID);
	if (item && item->getIsLinkType())
	{
		return true;
	}

	if (gAgentWearables.isWearingItem(mUUID))
	{
		return false;
	}

	return LLInvFVBridge::isItemRemovable();
}

//virtual
LLFontGL::StyleFlags LLWearableBridge::getLabelStyle() const
{
	S32 font = LLFontGL::NORMAL;

	if (gAgentWearables.isWearingItem(mUUID))
	{
		font |= LLFontGL::BOLD;
	}

	const LLViewerInventoryItem* item = getItem();
	if (item && item->getIsLinkType())
	{
		font |= LLFontGL::ITALIC;
	}

	return (LLFontGL::StyleFlags)font;
}

std::string LLWearableBridge::getLabelSuffix() const
{
	static const std::string worn = " (" + LLTrans::getString("worn") + ")";
	if (gAgentWearables.isWearingItem(mUUID))
	{
		return LLItemBridge::getLabelSuffix() + worn;
	}
	else
	{
		return LLItemBridge::getLabelSuffix();
	}
}

//virtual
LLUIImagePtr LLWearableBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(mAssetType, mInvType, mWearableType,
									false);
}

//virtual
void LLWearableBridge::performAction(LLFolderView* folder,
									 LLInventoryModel* model,
									 const std::string& action)
{
	bool agent_inventory = isAgentInventory();
	if (action == "wear")
	{
		if (agent_inventory)
		{
			wearOnAvatar();
		}
	}
	else if (action == "wear_add")
	{
		if (agent_inventory)
		{
			wearOnAvatar(false);
		}
	}
	else if (action == "edit")
	{
		if (agent_inventory)
		{
			editOnAvatar();
		}
	}
	else if (action == "take_off")
	{
		if (isAgentAvatarValid() && gAgentWearables.isWearingItem(mUUID))
		{
			LLViewerInventoryItem* item = getItem();
			if (item
//MK
				&& (!gRLenabled || gRLInterface.canUnwear(item)))
//mk
			{
				LLWearableList* wlist = LLWearableList::getInstance();
				wlist->getAsset(item->getAssetUUID(), item->getName(),
								gAgentAvatarp, item->getType(),
								LLWearableBridge::onRemoveFromAvatarArrived,
								new OnRemoveStruct(item->getLinkedUUID()));
			}
		}
	}
	else
	{
		LLItemBridge::performAction(folder, model, action);
	}
}

//virtual
void LLWearableBridge::openItem()
{
	if (isInTrash())
	{
		gNotifications.add("CannotWearTrash");
	}
	else if (gAgentWearables.isWearingItem(mUUID))
	{
		performAction(NULL, NULL, "take_off");
	}
	else if (isAgentInventory())
	{
		if (!isInMarketplace())
		{
			performAction(NULL, NULL, "wear");
		}
	}
	else
	{
		// Must be in the inventory library. Copy it to our inventory and put
		// it on right away.
		LLViewerInventoryItem* item = getItem();
		if (item && item->isFinished())
		{
			LLPointer<LLInventoryCallback> cb = new LLWearOnAvatarCallback();
			copy_inventory_item(item->getPermissions().getOwner(),
								item->getUUID(), LLUUID::null, std::string(),
								cb);
		}
		else if (item)
		{
			// *TODO: We should fetch the item details, and then do
			// the operation above.
			gNotifications.add("CannotWearInfoNotComplete");
		}
	}
}

//virtual
void LLWearableBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;
	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		if (!isItemRemovable())
		{
			disabled_items.emplace_back("Purge Item");
		}

		items.emplace_back("Restore Item");
	}
	else if (isInMarketplace())
	{
		items.emplace_back("Properties");
		getClipboardEntries(true, items, disabled_items, flags);
	}
	else
	{
		// FWIW, it looks like SUPPRESS_OPEN_ITEM is not set anywhere
		bool no_open = ((flags & SUPPRESS_OPEN_ITEM) == SUPPRESS_OPEN_ITEM);

		// If we have clothing, do not add "Open" as it is the same action as
		// "Wear"   SL-18976
		LLViewerInventoryItem* item = getItem();
		if (!no_open && item)
		{
			no_open = item->getType() == LLAssetType::AT_CLOTHING ||
					  item->getType() == LLAssetType::AT_BODYPART;
		}
		if (!no_open)
		{
			items.emplace_back("Open");
		}

		bool wearing = gAgentWearables.isWearingItem(mUUID);
		bool agent_inventory = isAgentInventory();
		// Allow to wear only non-library items in SSB-enabled sims
		if (wearing || agent_inventory)
		{
			if (wearing)
			{
				items.emplace_back("Edit");
				if (!agent_inventory || (flags & FIRST_SELECTED_ITEM) == 0)
				{
					disabled_items.emplace_back("Edit");
				}
			}
			else
			{
				items.emplace_back("Wearable Wear");
//MK
				if (gRLenabled && !gRLInterface.canWear(item))
				{
					disabled_items.emplace_back("Wearable Wear");
				}
//mk
			}
			if (item && item->getType() == LLAssetType::AT_CLOTHING)
			{
				if (wearing)
				{
					items.emplace_back("Take Off");
//MK
					if (gRLenabled && !gRLInterface.canUnwear(item))
					{
						disabled_items.emplace_back("Take Off");
					}
//mk
				}
				else
				{
					items.emplace_back("Wearable Add");
//MK
					if (gRLenabled && !gRLInterface.canWear(item))
					{
						disabled_items.emplace_back("Wearable Add");
					}
//mk
				}
			}

			items.emplace_back("Wearable Separator");
		}

		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);

	}
	set_menu_entries_state(menu, items, disabled_items);
}

// Called from menus
//static
bool LLWearableBridge::canWearOnAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	return self && self->isAgentInventory() &&
		   !gAgentWearables.isWearingItem(self->mUUID);
}

// Called from menus
//static
void LLWearableBridge::onWearOnAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	if (self)
	{
		self->wearOnAvatar();
	}
}

void LLWearableBridge::wearOnAvatar(bool replace)
{
	// Do not wear anything until initial wearables are loaded; could destroy
	// clothing items.
	if (!gAgentWearables.areWearablesLoaded())
	{
		gNotifications.add("CanNotChangeAppearanceUntilLoaded");
		return;
	}

	LLViewerInventoryItem* item = getItem();
	if (item)
	{
		gAppearanceMgr.wearItemOnAvatar(item->getLinkedUUID(), replace);
	}
}

//static
void LLWearableBridge::onWearOnAvatarArrived(LLViewerWearable* wearable,
											 void* userdata)
{
	OnWearStruct* on_wear_struct = (OnWearStruct*)userdata;
	const LLUUID& item_id = on_wear_struct->mUUID;
	bool replace = on_wear_struct->mReplace;

	if (wearable)
	{
		LLViewerInventoryItem* item = gInventory.getItem(item_id);
		if (item)
		{
			if (item->getAssetUUID() == wearable->getAssetID())
			{
//MK
				bool old_restore = gRLInterface.mRestoringOutfit;
				gRLInterface.mRestoringOutfit =
					gAppearanceMgr.isRestoringInitialOutfit();
//mk
				gAgentWearables.setWearableItem(item, wearable, !replace);
//MK
				gRLInterface.mRestoringOutfit = old_restore;
//mk
				gInventory.notifyObservers();
			}
			else
			{
				llinfos << "By the time wearable asset arrived, its inv item already pointed to a different asset."
						<< llendl;
			}
		}
	}
	delete on_wear_struct;
}

//static
bool LLWearableBridge::canEditOnAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	return self && gAgentWearables.isWearingItem(self->mUUID);
}

//static
void LLWearableBridge::onEditOnAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	if (self)
	{
		self->editOnAvatar();
	}
}

// *TODO: implement v3's way and allow wear & edit
void LLWearableBridge::editOnAvatar()
{
	const LLUUID& linked_id = gInventory.getLinkedItemID(mUUID);
	LLViewerWearable* wearable =
		gAgentWearables.getWearableFromItemID(linked_id);
	if (wearable)
	{
		// Set the tab to the right wearable.
		LLFloaterCustomize::setCurrentWearableType(wearable->getType());

		if (gAgent.getCameraMode() != CAMERA_MODE_CUSTOMIZE_AVATAR)
		{
			// Start Avatar Customization
			gAgent.changeCameraToCustomizeAvatar();
		}
	}
}

//static
bool LLWearableBridge::canRemoveFromAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	return self && self->mAssetType != LLAssetType::AT_BODYPART &&
		   gAgentWearables.isWearingItem(self->mUUID);
}

//static
void LLWearableBridge::onRemoveFromAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	if (self && isAgentAvatarValid() &&
		gAgentWearables.isWearingItem(self->mUUID))
	{
		LLViewerInventoryItem* item = self->getItem();
		if (item)
		{
			LLWearableList* wlist = LLWearableList::getInstance();
			wlist->getAsset(item->getAssetUUID(), item->getName(),
							gAgentAvatarp, item->getType(),
							onRemoveFromAvatarArrived,
							new OnRemoveStruct(LLUUID(self->mUUID)));
		}
	}
}

//static
void LLWearableBridge::onRemoveFromAvatarArrived(LLViewerWearable* wearable,
												 void* userdata)
{
	OnRemoveStruct* on_remove_struct = (OnRemoveStruct*)userdata;
	if (!on_remove_struct) return;	// Paranoia

	const LLUUID& item_id =
		gInventory.getLinkedItemID(on_remove_struct->mUUID);
	if (wearable)
	{
		if (get_is_item_worn(item_id))
		{
			LLWearableType::EType type = wearable->getType();
			U32 index;
			if (gAgentWearables.getWearableIndex(wearable, index))
			{
				gAgentWearables.userRemoveWearable(type, index);
				gInventory.notifyObservers();
			}
		}
	}

	delete on_remove_struct;
}

///////////////////////////////////////////////////////////////////////////////
// LLLinkItemBridge
///////////////////////////////////////////////////////////////////////////////
// For broken links

//virtual
const std::string& LLLinkItemBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Link") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLLinkItemBridge::getIcon() const
{
	if (LLViewerInventoryItem* item = getItem())
	{
		// Low byte of inventory flags
		U32 attachment_point = (item->getFlags() & 0xff);

		bool is_multi = (LLInventoryItem::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS &
						 item->getFlags()) != 0;

		return LLInventoryIcon::getIcon(item->getActualType(),
										item->getInventoryType(),
										attachment_point, is_multi);
	}
	return LLInventoryIcon::getIcon(LLAssetType::AT_LINK,
									LLInventoryType::IT_NONE, 0, false);
}

//virtual
void LLLinkItemBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	items.emplace_back("Find Original");
	disabled_items.emplace_back("Find Original");

	if (isInTrash())
	{
		disabled_items.emplace_back("Find Original");
		if (isLinkedObjectMissing())
		{
			disabled_items.emplace_back("Find Original");
		}
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Properties");
		items.emplace_back("Find Original");
		if (isLinkedObjectMissing())
		{
			disabled_items.emplace_back("Find Original");
		}
		items.emplace_back("Delete");
	}
	set_menu_entries_state(menu, items, disabled_items);
}

#if LL_MESH_ASSET_SUPPORT
///////////////////////////////////////////////////////////////////////////////
// LLMeshBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLMeshBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Mesh") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLMeshBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_MESH,
									LLInventoryType::IT_MESH, 0, false);
}

//virtual
void LLMeshBridge::openItem()
{
}

//virtual
void LLMeshBridge::previewItem()
{
}

//virtual
void LLMeshBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);
	}

	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
void LLMeshBridge::performAction(LLFolderView* folder, LLInventoryModel* model,
								 const std::string& action)
{
	LLItemBridge::performAction(folder, model, action);
}
#endif

///////////////////////////////////////////////////////////////////////////////
// LLSettingsBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLSettingsBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Settings") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLSettingsBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_SETTINGS,
									LLInventoryType::IT_SETTINGS,
									mSettingsType, false);
}

//virtual
LLFontGL::StyleFlags LLSettingsBridge::getLabelStyle() const
{
	S32 font = LLFontGL::NORMAL;

#if	0 // *TODO: use bold font when settings active
	if ()
	{
		font |= LLFontGL::BOLD;
	}
#endif

	const LLViewerInventoryItem* item = getItem();
	if (item && item->getIsLinkType())
	{
		font |= LLFontGL::ITALIC;
	}

	return (LLFontGL::StyleFlags)font;
}

//virtual
void LLSettingsBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	if (item && item->getPermissions().getOwner() == gAgentID)
	{
		HBFloaterEditEnvSettings* floaterp =
			HBFloaterEditEnvSettings::show(mUUID);
		if (floaterp)
		{
			floaterp->setEditContextInventory();
		}
	}
	else
	{
		gNotifications.add("NoEditFromLibrary");
	}
}

//virtual
void LLSettingsBridge::previewItem()
{
	openItem();
}

//virtual
void LLSettingsBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Settings Open");
		items.emplace_back("Properties");
		getClipboardEntries(true, items, disabled_items, flags);
	}

	if (LLEnvironment::isInventoryEnabled())
	{
		items.emplace_back("Setings Separator");
		items.emplace_back("Apply Local");
		items.emplace_back("Apply Parcel");
		if (!LLEnvironment::canAgentUpdateParcelEnvironment())
		{
			disabled_items.emplace_back("Apply Parcel");
		}
		items.emplace_back("Apply Region");
		if (!LLEnvironment::canAgentUpdateRegionEnvironment())
		{
			disabled_items.emplace_back("Apply Region");
		}
	}

	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
void LLSettingsBridge::performAction(LLFolderView* folder,
									 LLInventoryModel* model,
									 const std::string& action)
{
	if (action == "apply_settings_local")
	{
		LLViewerInventoryItem* item = getItem();
		if (item)
		{
			LLUUID asset_id = item->getAssetUUID();
			gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, asset_id,
										LLEnvironment::TRANSITION_INSTANT);
			gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
												LLEnvironment::TRANSITION_INSTANT);
			if (gAutomationp)
			{
				LLSettingsType::EType type = item->getSettingsType();
				const std::string& name = item->getName();
				if (type == LLSettingsType::ST_SKY)
				{
					gAutomationp->onWindlightChange(name, "", "");
				}
				else if (type == LLSettingsType::ST_WATER)
				{
					gAutomationp->onWindlightChange("", name, "");
				}
				else if (type == LLSettingsType::ST_DAYCYCLE)
				{
					gAutomationp->onWindlightChange("", "", name);
				}
			}
		}
	}
	else if (action == "apply_settings_parcel")
	{
		LLViewerInventoryItem* item = getItem();
		if (!item) return;

		LLUUID asset_id = item->getAssetUUID();
		std::string name = item->getName();
		LLParcel* parcelp = gViewerParcelMgr.getSelectedOrAgentParcel();
		if (!parcelp)
		{
			llwarns << "Could not find any selected or agent parcel. Aborted."
					<< llendl;
			return;
		}

		if (!LLEnvironment::canAgentUpdateParcelEnvironment(parcelp))
		{
			gNotifications.add("WLParcelApplyFail");
			return;
		}

		S32 parcel_id = parcelp->getLocalID();
		LL_DEBUGS("Environment") << "Applying environment settings asset Id "
								 << asset_id << " to parcel " << parcel_id
								 << LL_ENDL;

		U32 flags = 0;
		const LLPermissions& perms = item->getPermissions();
		if (!perms.allowOperationBy(PERM_MODIFY, gAgentID))
		{
			flags |= LLSettingsBase::FLAG_NOMOD;
		}
		if (!perms.allowOperationBy(PERM_TRANSFER, gAgentID))
		{
			flags |= LLSettingsBase::FLAG_NOTRANS;
		}

		gEnvironment.updateParcel(parcel_id, asset_id, name,
								  LLEnvironment::NO_TRACK, -1, -1, flags);
		gEnvironment.setSharedEnvironment();
	}
	else if (action == "apply_settings_region")
	{
		LLViewerInventoryItem* item = getItem();
		if (!item) return;

		if (!LLEnvironment::canAgentUpdateRegionEnvironment())
		{
			LLSD args;
			args["FAIL_REASON"] = LLTrans::getString("no_permission");
			gNotifications.add("WLRegionApplyFail", args);
			return;
		}

		U32 flags = 0;
		const LLPermissions& perms = item->getPermissions();
		if (!perms.allowOperationBy(PERM_MODIFY, gAgentID))
		{
			flags |= LLSettingsBase::FLAG_NOMOD;
		}
		if (!perms.allowOperationBy(PERM_TRANSFER, gAgentID))
		{
			flags |= LLSettingsBase::FLAG_NOTRANS;
		}

		gEnvironment.updateRegion(item->getAssetUUID(), item->getName(),
								  LLEnvironment::NO_TRACK, -1, -1, flags);
	}
	else if (action == "open")
	{
		openItem();
	}
	else
	{
		LLItemBridge::performAction(folder, model, action);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLLinkFolderBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLLinkFolderBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Link") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLLinkFolderBridge::getIcon() const
{
	return LLUI::getUIImage("inv_link_folder.tga");
}

//virtual
void LLLinkFolderBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	items.emplace_back("Find Original");
	if (isLinkedObjectMissing())
	{
		disabled_items.emplace_back("Find Original");
	}
	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Delete");
	}
	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
void LLLinkFolderBridge::performAction(LLFolderView* folder,
									   LLInventoryModel* model,
									   const std::string& action)
{
	if (action == "goto")
	{
		gotoItem(folder);
		return;
	}
	LLItemBridge::performAction(folder, model, action);
}

//virtual
void LLLinkFolderBridge::gotoItem(LLFolderView* folder)
{
	const LLUUID& cat_uuid = getFolderID();
	if (cat_uuid.notNull())
	{
		if (LLFolderViewItem* base_folder = folder->getItemByID(cat_uuid))
		{
			if (LLInventoryModel* model = mInventoryPanel->getModel())
			{
				model->fetchDescendentsOf(cat_uuid);
			}
			base_folder->setOpen(true);
			folder->setSelectionFromRoot(base_folder, true);
			folder->scrollToShowSelection();
		}
	}
}

const LLUUID& LLLinkFolderBridge::getFolderID() const
{
	if (LLViewerInventoryItem* link_item = getItem())
	{
		const LLViewerInventoryCategory* cat = link_item->getLinkedCategory();
		if (cat)
		{
			const LLUUID& cat_uuid = cat->getUUID();
			return cat_uuid;
		}
	}
	return LLUUID::null;
}

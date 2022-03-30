/**
 * @file llfloaterproperties.cpp
 * @brief A floater which shows an inventory item's properties.
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

#include "llfloaterproperties.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "llradiogroup.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llfloateravatarinfo.h"
#include "llfloatergroupinfo.h"
#include "llgridmanager.h"
#include "llinventorymodel.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewermessage.h"		// For formatted_time()
#include "llviewerobjectlist.h"
#include "llviewerregion.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLPropertiesObserver
//
// helper class to watch the inventory.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Ugh. This cannot be a singleton because it needs to remove itself from the 
// inventory observer list when destroyed, which could happen after gInventory
// has already been destroyed if a singleton. Instead, do our own ref counting
// and create / destroy it as needed
class LLPropertiesObserver final : public LLInventoryObserver
{
public:
	LLPropertiesObserver()
	{
		gInventory.addObserver(this);
	}

	~LLPropertiesObserver() override
	{
		gInventory.removeObserver(this);
	}

	void changed(U32 mask) override
	{
		// If there is a change we are interested in...
		if ((mask & (LLInventoryObserver::LABEL |
					 LLInventoryObserver::INTERNAL |
					 LLInventoryObserver::REMOVE)) != 0)
		{
			LLFloaterProperties::dirtyAll();
		}
	}
};

///----------------------------------------------------------------------------
/// Class LLFloaterProperties
///----------------------------------------------------------------------------

//static
LLFloaterProperties::instance_map_t LLFloaterProperties::sInstances;
LLPropertiesObserver* LLFloaterProperties::sPropertiesObserver = NULL;
S32 LLFloaterProperties::sPropertiesObserverCount = 0;

//static
LLFloaterProperties* LLFloaterProperties::find(const LLUUID& item_id,
											   const LLUUID& object_id)
{
	// For simplicity's sake, we key the properties window with a single uuid.
	// However, the items are keyed by item and object (obj == null -> agent
	// inventory). So, we xor the two ids, and use that as a lookup key.
	instance_map_t::iterator it = sInstances.find(item_id ^ object_id);
	return it != sInstances.end() ? it->second : NULL;
}

//static
LLFloaterProperties* LLFloaterProperties::show(const LLUUID& item_id,
											   const LLUUID& object_id)
{
	LLFloaterProperties* instance = find(item_id, object_id);
	if (instance)
	{
		if (LLFloater::getFloaterHost() &&
			LLFloater::getFloaterHost() != instance->getHost())
		{
			// this properties window is being opened in a new context
			// needs to be rehosted
			LLFloater::getFloaterHost()->addFloater(instance, true);
		}

		instance->refresh();
		instance->open();
	}
	return instance;
}

void LLFloaterProperties::dirtyAll()
{
	for (instance_map_t::iterator it = sInstances.begin(),
								  end = sInstances.end();
		 it != end; ++it)
	{
		it->second->dirty();
	}
}

// Default constructor
LLFloaterProperties::LLFloaterProperties(const std::string& name,
										 const LLRect& rect,
										 const std::string& title,
										 const LLUUID& item_id,
										 const LLUUID& object_id)
:	LLFloater(name, rect, title),
	mItemID(item_id),
	mObjectID(object_id),
	mDirty(true)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_inventory_item_properties.xml");

	if (!sPropertiesObserver)
	{
		sPropertiesObserver = new LLPropertiesObserver;
	}
	++sPropertiesObserverCount;

	// Add the object to the static structure
	LLUUID key = mItemID ^ mObjectID;
	sInstances.emplace(key, this);
}

bool LLFloaterProperties::postBuild()
{
	// Item name & description
	childSetPrevalidate("LabelItemName",
						LLLineEditor::prevalidatePrintableNotPipe);
	childSetCommitCallback("LabelItemName", onCommitName, this);

	childSetPrevalidate("LabelItemDesc",
						LLLineEditor::prevalidatePrintableNotPipe);
	childSetCommitCallback("LabelItemDesc", onCommitDescription, this);

	// Creator information
	childSetAction("BtnCreator", onClickCreator, this);

	// Owner information
	childSetAction("BtnOwner", onClickOwner, this);

	// Last owner information
	childSetAction("BtnLastOwner", onClickLastOwner, this);

	// Permissions

	// Group permissions
	childSetCommitCallback("CheckGroupCopy", onCommitPermissions, this);
	childSetCommitCallback("CheckGroupMod", onCommitPermissions, this);
	childSetCommitCallback("CheckGroupMove", onCommitPermissions, this);

	// Everyone's permissions
	childSetCommitCallback("CheckEveryoneCopy", onCommitPermissions, this);
	childSetCommitCallback("CheckEveryoneMove", onCommitPermissions, this);
	childSetCommitCallback("CheckEveryoneExport", onCommitPermissions, this);

	// Next owner permissions
	childSetCommitCallback("CheckNextOwnerModify", onCommitPermissions, this);
	childSetCommitCallback("CheckNextOwnerCopy", onCommitPermissions, this);
	childSetCommitCallback("CheckNextOwnerTransfer", onCommitPermissions, this);

	// Mark for sale or not, and sale info
	childSetCommitCallback("CheckPurchase", onCommitSaleInfo, this);
	childSetCommitCallback("RadioSaleType", onCommitSaleType, this);

	// "Price" label for edit
	childSetCommitCallback("EditPrice", onCommitSaleInfo, this);

	// The UI has been built, now fill in all the values
	refresh();

	return true;
}

// Destroys the object
LLFloaterProperties::~LLFloaterProperties()
{
	// clean up the static data.
	instance_map_t::iterator it = sInstances.find(mItemID ^ mObjectID);
	if (it != sInstances.end())
	{
		sInstances.erase(it);
	}

	if (--sPropertiesObserverCount <= 0)
	{
		delete sPropertiesObserver;
		sPropertiesObserver = NULL;
	}
}

void LLFloaterProperties::refresh()
{
	LLInventoryItem* item = findItem();
	if (item)
	{
		refreshFromItem(item);
	}
	else
	{
		// RN: it is possible that the container object is in the middle of an
		// inventory refresh causing findItem() to fail, so just temporarily
		// disable everything

		mDirty = true;

		static const char* enable_names[] =
		{
			"LabelItemName",
			"LabelItemDesc",
			"LabelCreatorName",
			"BtnCreator",
			"LabelOwnerName",
			"BtnOwner",
			"LabelLastOwnerName",
			"BtnLastOwner",
			"CheckOwnerModify",
			"CheckOwnerCopy",
			"CheckOwnerTransfer",
			"CheckOwnerExport",
			"CheckGroupCopy",
			"CheckGroupMod",
			"CheckGroupMove",
			"CheckEveryoneCopy",
			"CheckEveryoneMove",
			"CheckEveryoneExport",
			"CheckNextOwnerModify",
			"CheckNextOwnerCopy",
			"CheckNextOwnerTransfer",
			"CheckPurchase",
			"RadioSaleType",
			"EditPrice"
		};
		constexpr size_t enable_names_count = LL_ARRAY_SIZE(enable_names);

		for (size_t t = 0; t < enable_names_count; ++t)
		{
			childSetEnabled(enable_names[t], false);
		}

		static const char* hide_names[] =
		{
			"BaseMaskDebug",
			"OwnerMaskDebug",
			"GroupMaskDebug",
			"EveryoneMaskDebug",
			"NextMaskDebug"
		};
		constexpr size_t hide_names_count = LL_ARRAY_SIZE(hide_names);

		for (size_t t = 0; t < hide_names_count; ++t)
		{
			childSetVisible(hide_names[t], false);
		}
	}
}

void LLFloaterProperties::draw()
{
	if (mDirty)
	{
		// RN: clear dirty first because refresh can set dirty to true
		mDirty = false;
		refresh();
	}

	LLFloater::draw();
}

void LLFloaterProperties::refreshFromItem(LLInventoryItem* item)
{
	////////////////////////
	// PERMISSIONS LOOKUP //
	////////////////////////

	// Do not enable the UI for incomplete items.
	LLViewerInventoryItem* i = (LLViewerInventoryItem*)item;
	bool is_complete = i->isFinished();
	bool is_link = i->getIsLinkType();
	bool is_object = !is_link && item->getType() == LLAssetType::AT_OBJECT;
	bool no_restrict = LLInventoryType::cannotRestrictPermissions(i->getInventoryType());

	const LLPermissions& perm = item->getPermissions();
	bool can_agent_manipulate = gAgent.allowOperation(PERM_OWNER, perm,
													  GP_OBJECT_MANIPULATE);
	bool can_agent_sell = !no_restrict &&
						  gAgent.allowOperation(PERM_OWNER, perm,
												GP_OBJECT_SET_SALE);

	// You need permission to modify the object to modify an inventory item in
	// it.
	LLViewerObject* object = NULL;
	if (mObjectID.notNull())
	{
		object = gObjectList.findObject(mObjectID);
	}
	bool is_obj_modify = !object || object->permOwnerModify();

	//////////////////////
	// ITEM NAME & DESC //
	//////////////////////
	bool is_modifiable = is_obj_modify && is_complete &&
						 gAgent.allowOperation(PERM_MODIFY, perm,
											   GP_OBJECT_MANIPULATE);

	childSetEnabled("LabelItemNameTitle", true);
	childSetEnabled("LabelItemName",
					is_modifiable &&
					// Don't allow to rename calling cards
					i->getInventoryType() != LLInventoryType::IT_CALLINGCARD);
	childSetText("LabelItemName", item->getName());
	childSetEnabled("LabelItemDescTitle", true);
	childSetEnabled("LabelItemDesc", is_modifiable);
	childSetVisible("IconLocked", !is_modifiable);
	childSetText("LabelItemDesc", item->getDescription());
	if (is_link)
	{
		LL_DEBUGS("Properties") << "Link description for: " << item->getName()
								<< " : " << item->getActualDescription()
								<< LL_ENDL;
	}

	//////////////////
	// CREATOR NAME //
	//////////////////
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!gCacheNamep || !regionp) return;

	if (item->getCreatorUUID().notNull())
	{
		childSetEnabled("BtnCreator", true);
		childSetEnabled("LabelCreatorTitle", true);
		childSetEnabled("LabelCreatorName", true);
		std::string name;
		gCacheNamep->getFullName(item->getCreatorUUID(), name);
		childSetText("LabelCreatorName", name);
	}
	else
	{
		childSetEnabled("BtnCreator", false);
		childSetEnabled("LabelCreatorTitle", false);
		childSetEnabled("LabelCreatorName", false);
		childSetText("LabelCreatorName", getString("unknown"));
	}

	////////////////
	// OWNER NAME //
	////////////////
	if (perm.isOwned())
	{
		childSetEnabled("BtnOwner", true);
		childSetEnabled("LabelOwnerTitle", true);
		childSetEnabled("LabelOwnerName", true);
		std::string name;
		if (perm.isGroupOwned())
		{
			gCacheNamep->getGroupName(perm.getGroup(), name);
		}
		else
		{
			gCacheNamep->getFullName(perm.getOwner(), name);
//MK
			if (gRLenabled &&
				(gRLInterface.mContainsShownames ||
				 gRLInterface.mContainsShownametags))
			{
				name = gRLInterface.getDummyName(name);
			}
//mk
		}
		childSetText("LabelOwnerName", name);
	}
	else
	{
		childSetEnabled("BtnOwner", false);
		childSetEnabled("LabelOwnerTitle", false);
		childSetEnabled("LabelOwnerName", false);
		childSetText("LabelOwnerName", getString("public"));
	}

	/////////////////////
	// LAST OWNER NAME //
	/////////////////////
	if (perm.getLastOwner().notNull())
	{
		childSetEnabled("BtnLastOwner", true);
		childSetEnabled("LabelLastOwnerTitle", true);
		childSetEnabled("LabelLastOwnerName", true);
		std::string name;
		gCacheNamep->getFullName(perm.getLastOwner(), name);
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			name = gRLInterface.getDummyName(name);
		}
//mk
		childSetText("LabelLastOwnerName", name);
	}
	else
	{
		childSetEnabled("BtnLastOwner", false);
		childSetEnabled("LabelLastOwnerTitle", false);
		childSetEnabled("LabelLastOwnerName", false);
		childSetText("LabelLastOwnerName", getString("unknown"));
	}

	//////////////////
	// ACQUIRE DATE //
	//////////////////
	time_t time_utc = item->getCreationDate();
	std::string timestr;
	if (time_utc == 0)
	{
		timestr = getString("unknown");
	}
	else
	{
		timestr = formatted_time(time_utc);
	}
	childSetText("LabelAcquiredDate", timestr);

	///////////////////////
	// OWNER PERMISSIONS //
	///////////////////////
	if (can_agent_manipulate)
	{
		childSetText("OwnerLabel", getString("you_can"));
	}
	else
	{
		childSetText("OwnerLabel", getString("owner_can"));
	}

	U32 base_mask = perm.getMaskBase();
	U32 owner_mask = perm.getMaskOwner();
	U32 group_mask = perm.getMaskGroup();
	U32 everyone_mask = perm.getMaskEveryone();
	U32 next_owner_mask = perm.getMaskNextOwner();

	childSetEnabled("OwnerLabel", true);
	childSetEnabled("CheckOwnerModify", false);
	childSetValue("CheckOwnerModify", (owner_mask & PERM_MODIFY) != 0);
	childSetEnabled("CheckOwnerCopy", false);
	childSetValue("CheckOwnerCopy", (owner_mask & PERM_COPY) != 0);
	childSetEnabled("CheckOwnerTransfer", false);
	childSetValue("CheckOwnerTransfer", (owner_mask & PERM_TRANSFER) != 0);

	bool export_support = regionp->isOSExportPermSupported();
	// You can never change this yourself !
	childSetEnabled("CheckOwnerExport", false);
	childSetValue("CheckOwnerExport",
				  export_support && (owner_mask & PERM_EXPORT) != 0);
	childSetVisible("CheckOwnerExport", export_support);

	///////////////////////
	// DEBUG PERMISSIONS //
	///////////////////////
	static LLCachedControl<bool> debug_permissions(gSavedSettings,
												   "DebugPermissions");
	if (debug_permissions)
	{
		bool slam_perm = false;
		bool overwrite_group = false;
		bool overwrite_everyone = false;
		if (is_object)
		{
			U32 flags = item->getFlags();
			slam_perm = (flags & LLInventoryItem::II_FLAGS_OBJECT_SLAM_PERM) != 0;
			overwrite_everyone = (flags & LLInventoryItem::II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE) != 0;
			overwrite_group = (flags & LLInventoryItem::II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP) != 0;
		}

		std::string perm_string;

		perm_string = "B: ";
		perm_string += mask_to_string(base_mask, export_support);
		childSetText("BaseMaskDebug", perm_string);
		childSetVisible("BaseMaskDebug", true);

		perm_string = "O: ";
		perm_string += mask_to_string(owner_mask, export_support);
		childSetText("OwnerMaskDebug",perm_string);
		childSetVisible("OwnerMaskDebug", true);

		perm_string = "G";
		perm_string += overwrite_group ? "*: " : ": ";
		perm_string += mask_to_string(group_mask);
		childSetText("GroupMaskDebug", perm_string);
		childSetVisible("GroupMaskDebug", true);

		perm_string = "E";
		perm_string += overwrite_everyone ? "*: " : ": ";
		perm_string += mask_to_string(everyone_mask, export_support);
		childSetText("EveryoneMaskDebug", perm_string);
		childSetVisible("EveryoneMaskDebug", true);

		perm_string = "N";
		perm_string += slam_perm ? "*: " : ": ";
		perm_string += mask_to_string(next_owner_mask, export_support);
		childSetText("NextMaskDebug", perm_string);
		childSetVisible("NextMaskDebug", true);
	}
	else
	{
		childSetVisible("BaseMaskDebug", false);
		childSetVisible("OwnerMaskDebug", false);
		childSetVisible("GroupMaskDebug", false);
		childSetVisible("EveryoneMaskDebug", false);
		childSetVisible("NextMaskDebug", false);
	}

	/////////////
	// SHARING //
	/////////////

	// Check for ability to change values.
	if (!is_link && is_obj_modify && can_agent_manipulate)
	{
		bool can_share = (owner_mask & PERM_COPY) != 0 &&
						 (owner_mask & PERM_TRANSFER) != 0;
		childSetEnabled("GroupLabel", true);
		childSetEnabled("CheckGroupCopy", can_share && !no_restrict);
		childSetEnabled("CheckGroupMod",
						can_share && !no_restrict &&
						(owner_mask & PERM_MODIFY) != 0);
		childSetEnabled("CheckGroupMove", is_object && !no_restrict);
		childSetEnabled("EveryoneLabel", true);
		childSetEnabled("CheckEveryoneCopy", can_share && !no_restrict);
		childSetEnabled("CheckEveryoneMove", is_object && !no_restrict);
		childSetEnabled("CheckEveryoneExport",
						export_support && !no_restrict &&
						item->getCreatorUUID() == gAgentID &&
						can_set_export(base_mask, owner_mask,
									   next_owner_mask));
	}
	else
	{
		childSetEnabled("GroupLabel", false);
		childSetEnabled("CheckGroupCopy", false);
		childSetEnabled("CheckGroupMod", false);
		childSetEnabled("CheckGroupMove", false);
		childSetEnabled("EveryoneLabel", false);
		childSetEnabled("CheckEveryoneCopy", false);
		childSetEnabled("CheckEveryoneMove", false);
		childSetEnabled("CheckEveryoneExport", false);
	}
	childSetVisible("CheckGroupMove", is_object);
	childSetVisible("CheckEveryoneMove", is_object);
	childSetVisible("CheckEveryoneExport", export_support);

	// Set values.
	childSetValue("CheckGroupCopy", (group_mask & PERM_COPY) != 0);
	childSetValue("CheckGroupMod", (group_mask & PERM_MODIFY) != 0);
	childSetValue("CheckGroupMove", (group_mask & PERM_MOVE) != 0);

	childSetValue("CheckEveryoneCopy", (everyone_mask & PERM_COPY) != 0);
	childSetValue("CheckEveryoneMove", (everyone_mask & PERM_MOVE) != 0);
	childSetValue("CheckEveryoneExport",
				  export_support && (everyone_mask & PERM_EXPORT) != 0);

	///////////////
	// SALE INFO //
	///////////////

	const LLSaleInfo& sale_info = item->getSaleInfo();
	bool is_for_sale = sale_info.isForSale();
	// Check for ability to change values.
	if (is_obj_modify && can_agent_sell &&
		gAgent.allowOperation(PERM_TRANSFER, perm, GP_OBJECT_MANIPULATE))
	{
		childSetEnabled("CheckPurchase", is_complete);

		// Next owner perms can't be changed if export is set
		bool no_export = (everyone_mask & PERM_EXPORT) == 0;

		childSetEnabled("NextOwnerLabel", no_export && !no_restrict);
		childSetEnabled("CheckNextOwnerModify",
						no_export && !no_restrict &&
						(base_mask & PERM_MODIFY) != 0);
		childSetEnabled("CheckNextOwnerCopy",
						no_export && !no_restrict &&
						(base_mask & PERM_COPY) != 0);
		childSetEnabled("CheckNextOwnerTransfer",
						no_export && !no_restrict &&
						(next_owner_mask & PERM_COPY) != 0);

		childSetEnabled("RadioSaleType", is_complete && is_for_sale);
		childSetEnabled("TextPrice", is_complete && is_for_sale);
		childSetEnabled("EditPrice", is_complete && is_for_sale);
	}
	else
	{
		childSetEnabled("CheckPurchase", false);

		childSetEnabled("NextOwnerLabel", false);
		childSetEnabled("CheckNextOwnerModify", false);
		childSetEnabled("CheckNextOwnerCopy", false);
		childSetEnabled("CheckNextOwnerTransfer", false);

		childSetEnabled("RadioSaleType", false);
		childSetEnabled("TextPrice", false);
		childSetEnabled("EditPrice", false);
	}

	// Set values.
	childSetValue("CheckPurchase", is_for_sale);
	childSetValue("CheckNextOwnerModify",
				  LLSD((next_owner_mask & PERM_MODIFY) != 0));
	childSetValue("CheckNextOwnerCopy",
				  LLSD((next_owner_mask & PERM_COPY) != 0));
	childSetValue("CheckNextOwnerTransfer",
				  LLSD((next_owner_mask & PERM_TRANSFER) != 0));

	LLRadioGroup* radioSaleType = getChild<LLRadioGroup>("RadioSaleType");
	if (is_for_sale)
	{
		childSetEnabled("contents", is_object);
		radioSaleType->setSelectedIndex((S32)sale_info.getSaleType() - 1);
		S32 numerical_price;
		numerical_price = sale_info.getSalePrice();
		childSetText("EditPrice", llformat("%d", numerical_price));
	}
	else
	{
		radioSaleType->setSelectedIndex(-1);
		childSetText("EditPrice", llformat("%d", 0));
	}
}

//static
void LLFloaterProperties::onClickCreator(void* data)
{
	LLFloaterProperties* self = (LLFloaterProperties*)data;
	if (!self) return;

	LLInventoryItem* item = self->findItem();
	if (item && item->getCreatorUUID().notNull())
	{
		LLFloaterAvatarInfo::showFromObject(item->getCreatorUUID());
	}
}

//static
void LLFloaterProperties::onClickOwner(void* data)
{
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		return;
	}
//mk
	LLFloaterProperties* self = (LLFloaterProperties*)data;
	if (!self) return;

	LLInventoryItem* item = self->findItem();
	if (!item) return;

	if (item->getPermissions().isGroupOwned())
	{
		LLFloaterGroupInfo::showFromUUID(item->getPermissions().getGroup());
	}
	else if (item->getPermissions().getOwner().notNull())
	{
		LLFloaterAvatarInfo::showFromObject(item->getPermissions().getOwner());
	}
}

//static
void LLFloaterProperties::onClickLastOwner(void* data)
{
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		return;
	}
//mk
	LLFloaterProperties* self = (LLFloaterProperties*)data;
	if (!self) return;

	LLInventoryItem* item = self->findItem();
	if (item && item->getPermissions().getOwner().notNull())
	{
		LLFloaterAvatarInfo::showFromObject(item->getPermissions().getLastOwner());
	}
}

//static
void LLFloaterProperties::onCommitName(LLUICtrl* ctrl, void* data)
{
	LLFloaterProperties* self = (LLFloaterProperties*)data;
	if (!self)
	{
		return;
	}
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)self->findItem();
	if (!item)
	{
		return;
	}
	LLLineEditor* labelItemName = self->getChild<LLLineEditor>("LabelItemName");

	if (labelItemName && item->getName() != labelItemName->getText() &&
		gAgent.allowOperation(PERM_MODIFY, item->getPermissions(),
							  GP_OBJECT_MANIPULATE))
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->rename(labelItemName->getText());
		if (self->mObjectID.isNull())
		{
			new_item->updateServer(false);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
		else
		{
			LLViewerObject* object = gObjectList.findObject(self->mObjectID);
			if (object)
			{
				object->updateInventory(new_item, TASK_INVENTORY_ITEM_KEY,
										false);
			}
		}
	}
}

//static
void LLFloaterProperties::onCommitDescription(LLUICtrl* ctrl, void* data)
{
	LLFloaterProperties* self = (LLFloaterProperties*)data;
	if (!self) return;
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)self->findItem();
	if (!item) return;

	LLLineEditor* labelItemDesc = self->getChild<LLLineEditor>("LabelItemDesc");
	if (!labelItemDesc)
	{
		return;
	}
	if (item->getDescription() != labelItemDesc->getText() &&
	    gAgent.allowOperation(PERM_MODIFY, item->getPermissions(),
							  GP_OBJECT_MANIPULATE))
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);

		new_item->setDescription(labelItemDesc->getText());
		if (self->mObjectID.isNull())
		{
			new_item->updateServer(false);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
		else
		{
			LLViewerObject* object = gObjectList.findObject(self->mObjectID);
			if (object)
			{
				object->updateInventory(new_item, TASK_INVENTORY_ITEM_KEY,
										false);
			}
		}
	}
}

//static
void LLFloaterProperties::onCommitPermissions(LLUICtrl* ctrl, void* data)
{
	LLFloaterProperties* self = (LLFloaterProperties*)data;
	if (!self) return;
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)self->findItem();
	if (!item) return;
	LLPermissions perm(item->getPermissions());

	LLCheckBoxCtrl* check = self->getChild<LLCheckBoxCtrl>("CheckGroupMod");
	perm.setGroupBits(gAgentID, gAgent.getGroupID(), check->get(),
					  PERM_MODIFY);

	check = self->getChild<LLCheckBoxCtrl>("CheckGroupCopy");
	perm.setGroupBits(gAgentID, gAgent.getGroupID(), check->get(), PERM_COPY);

	check = self->getChild<LLCheckBoxCtrl>("CheckGroupMove");
	// Don't attempt to change this permission when not supported (not an
	// object, i.e. not rezzable)...
	if (check->getVisible())
	{
		perm.setGroupBits(gAgentID, gAgent.getGroupID(), check->get(),
						  PERM_MOVE);
	}

	check = self->getChild<LLCheckBoxCtrl>("CheckEveryoneCopy");
	perm.setEveryoneBits(gAgentID, gAgent.getGroupID(), check->get(),
						 PERM_COPY);

	check = self->getChild<LLCheckBoxCtrl>("CheckEveryoneMove");
	// Don't attempt to change this permission when not supported (not an
	// object, i.e. not rezzable)...
	if (check->getVisible())
	{
		perm.setEveryoneBits(gAgentID, gAgent.getGroupID(), check->get(),
							 PERM_MOVE);
	}

	check = self->getChild<LLCheckBoxCtrl>("CheckEveryoneExport");
	// Don't attempt to change this permission when not supported...
	if (check->getVisible())
	{
		perm.setEveryoneBits(gAgentID, gAgent.getGroupID(), check->get(),
							 PERM_EXPORT);
	}

	check = self->getChild<LLCheckBoxCtrl>("CheckNextOwnerModify");
	perm.setNextOwnerBits(gAgentID, gAgent.getGroupID(), check->get(),
						  PERM_MODIFY);

	check = self->getChild<LLCheckBoxCtrl>("CheckNextOwnerCopy");
	perm.setNextOwnerBits(gAgentID, gAgent.getGroupID(), check->get(),
						  PERM_COPY);

	check = self->getChild<LLCheckBoxCtrl>("CheckNextOwnerTransfer");
	perm.setNextOwnerBits(gAgentID, gAgent.getGroupID(), check->get(),
						  PERM_TRANSFER);

	if (perm != item->getPermissions() && item->isFinished())
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->setPermissions(perm);
		U32 flags = new_item->getFlags();

		// Object permissions
		if (item->getType() == LLAssetType::AT_OBJECT)
		{
			// If next owner permissions have changed then set the slam
			// permissions flag so that they are applied on rez.
			if (perm.getMaskNextOwner() != item->getPermissions().getMaskNextOwner())
			{
				flags |= LLInventoryItem::II_FLAGS_OBJECT_SLAM_PERM;
			}
			// If everyone permissions have changed then set the overwrite
			// everyone permissions flag so they are applied on rez.
			if (perm.getMaskEveryone() != item->getPermissions().getMaskEveryone())
			{
				flags |= LLInventoryItem::II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE;
			}
			// If group permissions have changed then set the overwrite group
			// permissions flag so they are applied on rez.
			if (perm.getMaskGroup() != item->getPermissions().getMaskGroup())
			{
				flags |= LLInventoryItem::II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP;
			}
		}

		new_item->setFlags(flags);

		if (self->mObjectID.isNull())
		{
			new_item->updateServer(false);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
		else
		{
			LLViewerObject* object = gObjectList.findObject(self->mObjectID);
			if (object)
			{
				object->updateInventory(new_item, TASK_INVENTORY_ITEM_KEY,
										false);
			}
		}
	}
	else
	{
		// need to make sure we don't just follow the click
		self->refresh();
	}
}

//static
void LLFloaterProperties::onCommitSaleInfo(LLUICtrl* ctrl, void* data)
{
	LLFloaterProperties* self = (LLFloaterProperties*)data;
	if (!self) return;
	self->updateSaleInfo();
}

//static
void LLFloaterProperties::onCommitSaleType(LLUICtrl* ctrl, void* data)
{
	LLFloaterProperties* self = (LLFloaterProperties*)data;
	if (!self) return;
	self->updateSaleInfo();
}

void LLFloaterProperties::updateSaleInfo()
{
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)findItem();
	if (!item) return;
	LLSaleInfo sale_info(item->getSaleInfo());
	if (!gAgent.allowOperation(PERM_TRANSFER, item->getPermissions(),
							   GP_OBJECT_SET_SALE))
	{
		childSetValue("CheckPurchase", false);
	}

	if (childGetValue("CheckPurchase").asBoolean())
	{
		// turn on sale info
		LLSaleInfo::EForSale sale_type = LLSaleInfo::FS_COPY;

		LLRadioGroup* RadioSaleType = getChild<LLRadioGroup>("RadioSaleType");
		if (RadioSaleType)
		{
			switch (RadioSaleType->getSelectedIndex())
			{
			case 0:
				sale_type = LLSaleInfo::FS_ORIGINAL;
				break;
			case 2:
				sale_type = LLSaleInfo::FS_CONTENTS;
				break;
			case 1:
			default:
				sale_type = LLSaleInfo::FS_COPY;
				break;
			}
		}

		if (sale_type == LLSaleInfo::FS_COPY &&
			!gAgent.allowOperation(PERM_COPY, item->getPermissions(),
								   GP_OBJECT_SET_SALE))
		{
			sale_type = LLSaleInfo::FS_ORIGINAL;
		}

		LLLineEditor* EditPrice = getChild<LLLineEditor>("EditPrice");

		S32 price = -1;
		if (EditPrice)
		{
			price = atoi(EditPrice->getText().c_str());
		}
		// Invalid data - turn off the sale
		if (price < 0)
		{
			sale_type = LLSaleInfo::FS_NOT;
			price = 0;
		}

		sale_info.setSaleType(sale_type);
		sale_info.setSalePrice(price);
	}
	else
	{
		sale_info.setSaleType(LLSaleInfo::FS_NOT);
	}
	if (sale_info != item->getSaleInfo() && item->isFinished())
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);

		// Force an update on the sale price at rez
		if (item->getType() == LLAssetType::AT_OBJECT)
		{
			U32 flags = new_item->getFlags();
			flags |= LLInventoryItem::II_FLAGS_OBJECT_SLAM_SALE;
			new_item->setFlags(flags);
		}

		new_item->setSaleInfo(sale_info);
		if (mObjectID.isNull())
		{
			// This is in the agent's inventory.
			new_item->updateServer(false);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
		else
		{
			// This is in an object's contents.
			LLViewerObject* object = gObjectList.findObject(mObjectID);
			if (object)
			{
				object->updateInventory(new_item, TASK_INVENTORY_ITEM_KEY,
										false);
			}
		}
	}
	else
	{
		// need to make sure we don't just follow the click
		refresh();
	}
}

LLInventoryItem* LLFloaterProperties::findItem() const
{
	LLInventoryItem* item = NULL;
	if (mObjectID.isNull())
	{
		// it is in agent inventory
		item = gInventory.getItem(mItemID);
	}
	else
	{
		LLViewerObject* object = gObjectList.findObject(mObjectID);
		if (object)
		{
			item = (LLInventoryItem*)object->getInventoryObject(mItemID);
		}
	}
	return item;
}

void LLFloaterProperties::closeByID(const LLUUID& item_id,
									const LLUUID& object_id)
{
	LLFloaterProperties* floaterp = find(item_id, object_id);

	if (floaterp)
	{
		floaterp->close();
	}
}

///----------------------------------------------------------------------------
/// LLMultiProperties
///----------------------------------------------------------------------------

LLMultiProperties::LLMultiProperties(const LLRect& rect)
:	LLMultiFloater(std::string("Properties"), rect)
{
}

/**
 * @file llinventoryactions.cpp
 * @brief Implementation of the actions associated with menu items.
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

#include "llinventoryactions.h"

#include "llnotifications.h"
#include "lltrans.h"

#include "llagent.h"
#include "llappearancemgr.h"
#include "llavatartracker.h"
#include "llenvironment.h"
#include "llenvsettings.h"
#include "llfloateravatarinfo.h"
#include "hbfloatermakenewoutfit.h"
#include "llfloaterinventory.h"
#include "llfloaterperms.h"
#include "llfloaterproperties.h"
#include "llfloaterworldmap.h"
#include "llfolderview.h"
#include "llimmgr.h"
#include "llinventorybridge.h"
#include "llinventoryclipboard.h"
#include "llpanelinventory.h"
#include "llpreview.h"
#include "llpreviewlandmark.h"
#include "llpreviewnotecard.h"
#include "llpreviewtexture.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llwearablelist.h"

using namespace LLOldEvents;

const std::string NEW_LSL_NAME = "New Script";
const std::string NEW_NOTECARD_NAME = "New Note";
const std::string NEW_GESTURE_NAME = "New Gesture";

typedef LLMemberListener<LLPanelInventory> object_inventory_listener_t;
typedef LLMemberListener<LLFloaterInventory> inventory_listener_t;
typedef LLMemberListener<LLInventoryPanel> inventory_panel_listener_t;

bool doToSelected(LLFolderView* folder, std::string action)
{
	LLInventoryModel* model = &gInventory;
	if (action == "rename")
	{
		folder->startRenamingSelectedItem();
		return true;
	}
	if (action == "delete")
	{
		folder->removeSelectedItems();
		model->checkTrashOverflow();
		return true;
	}
	if (action == "copy")
	{
		folder->copy();
		return true;
	}
	if (action == "cut")
	{
		folder->cut();
		return true;
	}
	if (action == "paste")
	{
		folder->paste();
		return true;
	}

	uuid_list_t selected_items;
	folder->getSelectionList(selected_items);

	LLMultiPreview* multi_previewp = NULL;
	LLMultiProperties* multi_propertiesp = NULL;
	{
		LLHostFloater host;
		if (selected_items.size() > 1)
		{
			if (action == "task_open" || action == "open")
			{
				bool open_multi_preview = true;
				for (uuid_list_t::iterator it = selected_items.begin(),
										   end = selected_items.end();
					 it != end; ++it)
				{
					LLFolderViewItem* folder_item = folder->getItemByID(*it);
					if (folder_item)
					{
						LLInvFVBridge* bridge =
							(LLInvFVBridge*)folder_item->getListener();
						if (bridge && !bridge->isMultiPreviewAllowed())
						{
							open_multi_preview = false;
							break;
						}
					}
				}
				if (open_multi_preview)
				{
					S32 left, top;
					gFloaterViewp->getNewFloaterPosition(&left, &top);

					multi_previewp = new LLMultiPreview(LLRect(left, top,
														left + 300,
														top - 100));
					gFloaterViewp->addChild(multi_previewp);

					host.set(multi_previewp);
				}
			}
			else if (action == "task_properties" || action == "properties")
			{
				S32 left, top;
				gFloaterViewp->getNewFloaterPosition(&left, &top);

				multi_propertiesp = new LLMultiProperties(LLRect(left, top,
																 left + 100,
																 top - 100));
				gFloaterViewp->addChild(multi_propertiesp);

				host.set(multi_propertiesp);
			}
		}

		for (uuid_list_t::iterator it = selected_items.begin(),
								   end = selected_items.end();
			 it != end; ++it)
		{
			LLFolderViewItem* folder_item = folder->getItemByID(*it);
			if (folder_item)
			{
				LLInvFVBridge* bridge =
					(LLInvFVBridge*)folder_item->getListener();
				if (bridge)
				{
					bridge->performAction(folder, model, action);
				}
			}
		}
	}

	if (multi_previewp)
	{
		multi_previewp->open();
	}
	else if (multi_propertiesp)
	{
		multi_propertiesp->open();
	}

	return true;
}

class LLDoToSelectedPanel : public object_inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		std::string action = userdata.asString();
		LLPanelInventory* panel = mPtr;
		LLFolderView* folder = panel->getRootFolder();
		if (!folder) return true;

		return doToSelected(folder, action);
	}
};

class LLDoToSelectedFloater : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		std::string action = userdata.asString();
		LLInventoryPanel* panel = mPtr->getPanel();
		LLFolderView* folder = panel->getRootFolder();
		if (!folder) return true;

		return doToSelected(folder, action);
	}
};

class LLDoToSelected : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		std::string action = userdata.asString();
		LLInventoryPanel* panel = mPtr;
		LLFolderView* folder = panel->getRootFolder();
		if (!folder) return true;

		return doToSelected(folder, action);
	}
};

class LLNewWindow : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryPanel* panelp = mPtr->getActivePanel();
		if (!panelp) return true;	// Paranoia

		LLRect rect(gSavedSettings.getRect("FloaterInventoryRect"));
		S32 left = 0 , top = 0;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		rect.setLeftTopAndSize(left, top, rect.getWidth(), rect.getHeight());
		LLFloaterInventory* floaterp =
			new LLFloaterInventory("Inventory", rect, panelp->getModel());
		floaterp->getActivePanel()->setFilterTypes(panelp->getFilterTypes());
		floaterp->getActivePanel()->setFilterSubString(panelp->getFilterSubString());
		floaterp->open();
		// Force on screen
		gFloaterViewp->adjustToFitScreen(floaterp);

		return true;
	}
};

class LLShowFilters : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		mPtr->toggleFindOptions();
		return true;
	}
};

class LLResetFilter : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		if (!mPtr->getActivePanel()) return true;	// Paranoia

		LLFloaterInventoryFilters* filters = mPtr->getInvFilters();
		mPtr->getActivePanel()->getFilter()->resetDefault();
		if (filters)
		{
			filters->updateElementsFromFilter();
		}

		mPtr->setFilterTextFromFilter();
		return true;
	}
};

class LLCloseAllFolders : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		mPtr->closeAllFolders();
		return true;
	}
};

class LLCloseAllFoldersFloater : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		if (mPtr->getPanel())	// Paranoia
		{
			mPtr->getPanel()->closeAllFolders();
		}
		return true;
	}
};

class LLEmptyTrash : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* model = mPtr->getModel();
		if (!model) return true;

		const LLUUID& trash_id =
			model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
		if (!model->isCategoryComplete(trash_id))
		{
			llwarns << "Not purging the incompletely downloaded Trash folder"
					<< llendl;
			return true;
		}

		gNotifications.add("ConfirmEmptyTrash", LLSD(), LLSD(),
						   boost::bind(&LLEmptyTrash::cb_empty_trash,
									   this, _1, _2));
		return true;
	}

	bool cb_empty_trash(const LLSD& notification, const LLSD& response)
	{
		if (LLNotification::getSelectedOption(notification, response) == 0)
		{
			LLInventoryModel* model = mPtr->getModel();
			if (model)
			{
				const LLUUID& trash_id =
					model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
				purge_descendents_of(trash_id);
				model->notifyObservers();
			}
		}
		return false;
	}
};

class LLEmptyLostAndFound : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD&)
	{
		LLInventoryModel* model = mPtr->getModel();
		if (!model) return true;

		const LLUUID& laf_id =
			model->findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);
		if (!model->isCategoryComplete(laf_id))
		{
			llwarns << "Not purging the incompletely downloaded Lost and found folder"
					<< llendl;
			return true;
		}

		gNotifications.add("ConfirmEmptyLostAndFound", LLSD(), LLSD(),
						   boost::bind(&LLEmptyLostAndFound::cb_purge_laf,
									  this, _1, _2));
		return true;
	}

	bool cb_purge_laf(const LLSD& notification, const LLSD& response)
	{
		if (LLNotification::getSelectedOption(notification, response) == 0)
		{
			LLInventoryModel* model = mPtr->getModel();
			if (!model) return false;

			const LLUUID& laf_id =
				model->findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);
			purge_descendents_of(laf_id);
			model->notifyObservers();
		}
		return false;
	}
};

class LLHideEmptySystemFolders : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool hide = !gSavedSettings.getBool("HideEmptySystemFolders");
		gSavedSettings.setBool("HideEmptySystemFolders", hide);
		// Force a new filtering
		mPtr->getActivePanel()->getFilter()->setModified();
		return true;
	}
};

class LLHideMarketplaceFolder : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool hide = !gSavedSettings.getBool("HideMarketplaceFolder");
		gSavedSettings.setBool("HideMarketplaceFolder", hide);
		// Force a new filtering
		mPtr->getActivePanel()->getFilter()->setModified();
		return true;
	}
};

class LLHideCurrentOutfitFolder : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool hide = !gSavedSettings.getBool("HideCurrentOutfitFolder");
		gSavedSettings.setBool("HideCurrentOutfitFolder", hide);
		// Force a new filtering
		mPtr->getActivePanel()->getFilter()->setModified();
		return true;
	}
};

class LLCheckSystemFolders : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel::checkSystemFolders();
		return true;
	}
};

class LLMakeNewOutfit : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		HBFloaterMakeNewOutfit::showInstance();
		return true;
	}
};

class LLEmptyTrashFloater : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD&)
	{
		LLInventoryModel* model = mPtr->getPanel()->getModel();
		if (!model) return true;

		const LLUUID& trash_id =
			model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
		purge_descendents_of(trash_id);
		model->notifyObservers();

		return true;
	}
};

#if 0	// Wear on create is not implemented/used in the Cool VL Viewer
static void create_wearable(LLWearableType::EType type,
							const LLUUID& parent_id, bool wear = false)
#else
static void create_wearable(LLWearableType::EType type,
							const LLUUID& parent_id)
#endif
{
	if (type == LLWearableType::WT_INVALID ||
		type == LLWearableType::WT_NONE ||
		!isAgentAvatarValid())
	{
		return;
	}

	if (type == LLWearableType::WT_UNIVERSAL)
	{
		LLViewerRegion* regionp = gAgent.getRegion();
		if (!regionp || !regionp->bakesOnMeshEnabled())
		{
			llwarns << "Cannot create Universal wearable type in this region"
					<< llendl;
			return;
		}
	}

	LLViewerWearable* wearable =
		LLWearableList::getInstance()->createNewWearable(type, gAgentAvatarp);
	LLAssetType::EType asset_type = wearable->getAssetType();
	LLInventoryType::EType inv_type = LLInventoryType::IT_WEARABLE;
#if 0	// Not implemented/used in the Cool VL Viewer
	LLPointer<LLInventoryCallback> cb = wear ? new LLWearAndEditCallback : NULL;
#else
	LLPointer<LLInventoryCallback> cb = NULL;
#endif

	LLUUID folder_id;
	if (parent_id.notNull())
	{
		folder_id = parent_id;
	}
	else
	{
		LLFolderType::EType folder_type =
			LLFolderType::assetTypeToFolderType(asset_type);
		folder_id = gInventory.findCategoryUUIDForType(folder_type);
	}

	create_inventory_item(folder_id, wearable->getTransactionID(),
						  wearable->getName(), wearable->getDescription(),
						  asset_type, inv_type, (U8)wearable->getType(),
						  wearable->getPermissions().getMaskNextOwner(), cb);
}

static void do_create(LLInventoryModel* model, LLInventoryPanel* ptr,
					  const std::string& type, LLFolderBridge* self = NULL)
{
	if (type == "category")
	{
		LLUUID category;
		if (self)
		{
			category = model->createNewCategory(self->getUUID(),
												LLFolderType::FT_NONE,
												LLStringUtil::null);
		}
		else
		{
			category = model->createNewCategory(gInventory.getRootFolderID(),
												LLFolderType::FT_NONE,
												LLStringUtil::null);
		}
		model->notifyObservers();
		ptr->setSelection(category, true);
	}
	else if (type == "lsl")
	{
		U32 perms = PERM_MOVE | LLFloaterPerms::getNextOwnerPerms();
		if (gSavedSettings.getBool("NoModScripts"))
		{
			perms = perms & ~PERM_MODIFY;
		}
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : model->findCategoryUUIDForType(LLFolderType::FT_LSL_TEXT);
		create_new_item(NEW_LSL_NAME, parent_id, LLAssetType::AT_LSL_TEXT,
						LLInventoryType::IT_LSL, perms);
	}
	else if (type == "notecard")
	{
		U32 perms = PERM_MOVE | LLFloaterPerms::getNextOwnerPerms();
		if (gSavedSettings.getBool("FullPermNotecards"))
		{
			perms = PERM_ALL;
		}
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : model->findCategoryUUIDForType(LLFolderType::FT_NOTECARD);
		create_new_item(NEW_NOTECARD_NAME, parent_id, LLAssetType::AT_NOTECARD,
						LLInventoryType::IT_NOTECARD, perms);
	}
	else if (type == "gesture")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : model->findCategoryUUIDForType(LLFolderType::FT_GESTURE);
		create_new_item(NEW_GESTURE_NAME, parent_id, LLAssetType::AT_GESTURE,
						LLInventoryType::IT_GESTURE,
						PERM_MOVE | LLFloaterPerms::getNextOwnerPerms());
	}
	else if (type == "callingcard")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
		std::string name;
		gAgent.getName(name);
		create_new_item(name, parent_id, LLAssetType::AT_CALLINGCARD,
						LLInventoryType::IT_CALLINGCARD,
						PERM_ALL & ~PERM_MODIFY, gAgentID.asString());
	}
	else if (type == "shirt")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_SHIRT, parent_id);
	}
	else if (type == "pants")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_PANTS, parent_id);
	}
	else if (type == "shoes")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_SHOES, parent_id);
	}
	else if (type == "socks")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_SOCKS, parent_id);
	}
	else if (type == "jacket")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_JACKET, parent_id);
	}
	else if (type == "skirt")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_SKIRT, parent_id);
	}
	else if (type == "gloves")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_GLOVES, parent_id);
	}
	else if (type == "undershirt")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_UNDERSHIRT, parent_id);
	}
	else if (type == "underpants")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_UNDERPANTS, parent_id);
	}
	else if (type == "alpha")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_ALPHA, parent_id);
	}
	else if (type == "tattoo")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_TATTOO, parent_id);
	}
	else if (type == "universal")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_UNIVERSAL, parent_id);
	}
	else if (type == "physics")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_PHYSICS, parent_id);
	}
	else if (type == "shape")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_BODYPART);
		create_wearable(LLWearableType::WT_SHAPE, parent_id);
	}
	else if (type == "skin")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_BODYPART);
		create_wearable(LLWearableType::WT_SKIN, parent_id);
	}
	else if (type == "hair")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_BODYPART);
		create_wearable(LLWearableType::WT_HAIR, parent_id);
	}
	else if (type == "eyes")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_BODYPART);
		create_wearable(LLWearableType::WT_EYES, parent_id);
	}
	else if (type == "sky")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS);
		LLEnvSettingsBase::createNewInventoryItem(LLSettingsType::ST_SKY,
												  parent_id);
	}
	else if (type == "water")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS);
		LLEnvSettingsBase::createNewInventoryItem(LLSettingsType::ST_WATER,
												  parent_id);
	}
	else if (type == "day")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS);
		LLEnvSettingsBase::createNewInventoryItem(LLSettingsType::ST_DAYCYCLE,
												  parent_id);
	}

	ptr->getRootFolder()->setNeedsAutoRename(true);
}

class LLDoCreate : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* model = mPtr->getModel();
		if (model)
		{
			do_create(model, mPtr, userdata.asString(), LLFolderBridge::sSelf);
		}
		return true;
	}
};

class LLFileUploadLocation : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* model = mPtr->getModel();
		if (!model) return true;

		std::string setting_name = userdata.asString();
		LLControlVariable* control =
			gSavedPerAccountSettings.getControl(setting_name.c_str());
		if (control)
		{
			control->setValue(LLFolderBridge::sSelf->getUUID().asString());
		}
		return true;
	}
};

class LLDoCreateFloater : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* model = mPtr->getPanel()->getModel();
		if (model)
		{
			do_create(model, mPtr->getPanel(), userdata.asString());
		}
		return true;
	}
};

class LLSetSortBy : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		std::string sort_field = userdata.asString();
		U32 order = mPtr->getActivePanel()->getSortOrder();
		if (sort_field == "name")
		{
			order &= ~LLInventoryFilter::SO_DATE;
		}
		else if (sort_field == "date")
		{
			order |= LLInventoryFilter::SO_DATE;
		}
		else if (sort_field == "foldersalwaysbyname")
		{
			if (order & LLInventoryFilter::SO_FOLDERS_BY_NAME)
			{
				order &= ~LLInventoryFilter::SO_FOLDERS_BY_NAME;
			}
			else
			{
				order |= LLInventoryFilter::SO_FOLDERS_BY_NAME;
			}
		}
		else if (sort_field == "systemfolderstotop")
		{
			if (order & LLInventoryFilter::SO_SYSTEM_FOLDERS_TO_TOP)
			{
				order &= ~LLInventoryFilter::SO_SYSTEM_FOLDERS_TO_TOP;
			}
			else
			{
				order |= LLInventoryFilter::SO_SYSTEM_FOLDERS_TO_TOP;
			}
		}
		mPtr->getActivePanel()->setSortOrder(order);
		mPtr->updateSortControls();

		return true;
	}
};

class LLSetSearchType : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		std::string toggle = userdata.asString();
		U32 flags = mPtr->getActivePanel()->getRootFolder()->toggleSearchType(toggle);
		mPtr->getControl("Inventory.SearchName")->setValue((flags & 1) != 0);
		mPtr->getControl("Inventory.SearchDesc")->setValue((flags & 2) != 0);
		mPtr->getControl("Inventory.SearchCreator")->setValue((flags & 4) != 0);
		return true;
	}
};

class LLBeginIMSession : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryPanel* panel = mPtr;
		LLInventoryModel* model = panel->getModel();
		if (!model) return true;

		uuid_list_t selected_items;
		panel->getRootFolder()->getSelectionList(selected_items);

		std::string name;
		static S32 session_num = 1;

		uuid_vec_t members;
		EInstantMessage type = IM_SESSION_CONFERENCE_START;

		for (uuid_list_t::const_iterator iter = selected_items.begin(),
										 end = selected_items.end();
			 iter != end; ++iter)
		{
			LLUUID item = *iter;

			LLFolderViewItem* folder_item =
				panel->getRootFolder()->getItemByID(item);
			if (folder_item)
			{
				LLFolderViewEventListener* fve_listener =
					folder_item->getListener();
				if (fve_listener &&
					fve_listener->getInventoryType() ==
						LLInventoryType::IT_CATEGORY)
				{

					LLFolderBridge* bridge =
						(LLFolderBridge*)folder_item->getListener();
					if (!bridge) return true;

					LLViewerInventoryCategory* cat = bridge->getCategory();
					if (!cat) return true;

					name = cat->getName();
					LLUniqueBuddyCollector is_buddy;
					LLInventoryModel::cat_array_t cat_array;
					LLInventoryModel::item_array_t item_array;
					model->collectDescendentsIf(bridge->getUUID(),
												cat_array,
												item_array,
												LLInventoryModel::EXCLUDE_TRASH,
												is_buddy);
					S32 count = item_array.size();
					if (count > 0)
					{
						// Create the session
						gIMMgrp->setFloaterOpen(true);

						LLAvatarTracker& at = gAvatarTracker;
						LLUUID id;
						for (S32 i = 0; i < count; ++i)
						{
							id = item_array[i]->getCreatorUUID();
							if (at.isBuddyOnline(id))
							{
								members.emplace_back(id);
							}
						}
					}
				}
				else
				{
					LLFolderViewItem* folder_item =
						panel->getRootFolder()->getItemByID(item);
					if (!folder_item) return true;

					LLInvFVBridge* listenerp =
						(LLInvFVBridge*)folder_item->getListener();
					if (listenerp &&
						listenerp->getInventoryType() ==
							LLInventoryType::IT_CALLINGCARD)
					{
						LLInventoryItem* inv_item =
							gInventory.getItem(listenerp->getUUID());
						if (inv_item)
						{
							LLUUID id = inv_item->getCreatorUUID();
							if (gAvatarTracker.isBuddyOnline(id))
							{
								members.emplace_back(id);
							}
						}
					}
				}
			}
		}

		// The session_id is randomly generated UUID which will be replaced
		// later with a server side generated number

		if (name.empty())
		{
			name = llformat("Session %d", session_num++);
		}

		gIMMgrp->addSession(name, type, members[0], members);

		return true;
	}
};

class LLAttachObject : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryPanel* panel = mPtr;
		LLFolderView* folder = panel->getRootFolder();
		if (!folder || !isAgentAvatarValid()) return true;

		uuid_list_t selected_items;
		folder->getSelectionList(selected_items);
		LLUUID id = *selected_items.begin();

		std::string joint_name = userdata.asString();
		LLViewerJointAttachment* attachmentp = NULL;
		for (LLVOAvatar::attachment_map_t::iterator
				iter = gAgentAvatarp->mAttachmentPoints.begin();
			 iter != gAgentAvatarp->mAttachmentPoints.end(); )
		{
			LLVOAvatar::attachment_map_t::iterator curiter = iter++;
			LLViewerJointAttachment* attachment = curiter->second;
			std::string name = LLTrans::getString(attachment->getName());
			if (name == joint_name)
			{
				attachmentp = attachment;
				break;
			}
		}
		if (attachmentp == NULL)
		{
			return true;
		}

		LLViewerInventoryItem* item;
		item = (LLViewerInventoryItem*)gInventory.getItem(id);
		if (item &&
			gInventory.isObjectDescendentOf(id, gInventory.getRootFolderID()))
		{
			gAppearanceMgr.rezAttachment(item, attachmentp);
		}
		else if (item && item->isFinished())
		{
			// Must be in library. copy it to our inventory and put it on.
			LLPointer<LLInventoryCallback> cb =
				new LLRezAttachmentCallback(attachmentp);
			copy_inventory_item(item->getPermissions().getOwner(),
								item->getUUID(), LLUUID::null, std::string(),
								cb);
		}
		gFocusMgr.setKeyboardFocus(NULL);

		return true;
	}
};

class LLEnableUniversal : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLViewerRegion* regionp = gAgent.getRegion();
		bool enable = regionp && regionp->bakesOnMeshEnabled();
		mPtr->findControl(userdata["control"].asString())->setValue(enable);
		return true;
	}
};

class LLEnableSettings : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool enable = LLEnvironment::isInventoryEnabled();
		mPtr->findControl(userdata["control"].asString())->setValue(enable);
		return true;
	}
};

void init_object_inventory_panel_actions(LLPanelInventory* panel)
{
	(new LLDoToSelectedPanel())->registerListener(panel, "Inventory.DoToSelected");
}

void init_inventory_actions(LLFloaterInventory* floater)
{
	(new LLDoToSelectedFloater())->registerListener(floater,
													"Inventory.DoToSelected");
	(new LLCloseAllFoldersFloater())->registerListener(floater,
													   "Inventory.CloseAllFolders");
	(new LLHideEmptySystemFolders())->registerListener(floater,
													   "Inventory.HideEmptySystemFolders");
	(new LLHideMarketplaceFolder())->registerListener(floater,
													  "Inventory.HideMarketplaceFolder");
	(new LLHideCurrentOutfitFolder())->registerListener(floater,
														"Inventory.HideCurrentOutfitFolder");
	(new LLCheckSystemFolders())->registerListener(floater,
												   "Inventory.CheckSystemFolders");
	(new LLMakeNewOutfit())->registerListener(floater,
											  "Inventory.MakeNewOutfit");
	(new LLEmptyTrashFloater())->registerListener(floater,
												  "Inventory.EmptyTrash");
	(new LLDoCreateFloater())->registerListener(floater,
												"Inventory.DoCreate");

	(new LLNewWindow())->registerListener(floater, "Inventory.NewWindow");
	(new LLShowFilters())->registerListener(floater, "Inventory.ShowFilters");
	(new LLResetFilter())->registerListener(floater, "Inventory.ResetFilter");
	(new LLSetSortBy())->registerListener(floater, "Inventory.SetSortBy");
	(new LLSetSearchType())->registerListener(floater,
											  "Inventory.SetSearchType");

	(new LLEnableUniversal())->registerListener(floater,
												"Inventory.EnableUniversal");
	(new LLEnableSettings())->registerListener(floater,
											   "Inventory.EnableSettings");
}

void init_inventory_panel_actions(LLInventoryPanel* panel)
{
	(new LLDoToSelected())->registerListener(panel, "Inventory.DoToSelected");
	(new LLAttachObject())->registerListener(panel, "Inventory.AttachObject");
	(new LLCloseAllFolders())->registerListener(panel,
												"Inventory.CloseAllFolders");
	(new LLEmptyTrash())->registerListener(panel, "Inventory.EmptyTrash");
	(new LLEmptyLostAndFound())->registerListener(panel,
												  "Inventory.EmptyLostAndFound");
	(new LLDoCreate())->registerListener(panel, "Inventory.DoCreate");
	(new LLFileUploadLocation())->registerListener(panel,
												   "Inventory.FileUploadLocation");
	(new LLBeginIMSession())->registerListener(panel,
											   "Inventory.BeginIMSession");
}

void open_notecard(LLViewerInventoryItem* inv_item, const std::string& title,
				   const LLUUID& object_id, bool show_keep_discard,
				   const LLUUID& source_id, bool take_focus)
{
//MK
	if (gRLenabled && gRLInterface.contains("viewnote"))
	{
		return;
	}
//mk
	// See if we can bring an existing preview to the front
	if (inv_item && !LLPreview::show(inv_item->getUUID(), take_focus))
	{
		// There is none, so make a new preview
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("NotecardEditorRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLPreviewNotecard* preview =
			new LLPreviewNotecard("preview notecard", rect, title,
								  inv_item->getUUID(), object_id,
								  inv_item->getAssetUUID(), show_keep_discard,
								  inv_item);
		preview->setSourceID(source_id);
		if (take_focus)
		{
			preview->setFocus(true);
		}
		// Force to be entirely on screen.
		gFloaterViewp->adjustToFitScreen(preview);
#if 0
		if (source_id.notNull())
		{
			// Look for existing tabbed view for content from same source
			LLPreview* existing_preview;
			existing_preview = LLPreview::getPreviewForSource(source_id);
			if (existing_preview)
			{
				// Found existing preview from this source is it already hosted
				// in a multi-preview window ?
				LLMultiPreview* preview_hostp;
				preview_hostp = (LLMultiPreview*)existing_preview->getHost();
				if (!preview_hostp)
				{
					// Create a new multipreview if it does not exist
					preview_hostp = new LLMultiPreview(existing_preview->getRect());
					preview_hostp->addFloater(existing_preview);
				}
				// Add this preview to existing host
				preview_hostp->addFloater(preview);
			}
		}
#endif
	}
}

void open_landmark(LLViewerInventoryItem* inv_item, const std::string& title,
				   bool show_keep_discard, const LLUUID& source_id,
				   bool take_focus)
{
	// See if we can bring an exiting preview to the front
	if (inv_item && !LLPreview::show(inv_item->getUUID(), take_focus))
	{
		// There is none, so make a new preview
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewLandmarkRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);

		LLPreviewLandmark* preview =
			new LLPreviewLandmark(title, rect, title, inv_item->getUUID(),
								  show_keep_discard, inv_item);
		preview->setSourceID(source_id);
		if (take_focus)
		{
			preview->setFocus(true);
		}
		// Keep on screen
		gFloaterViewp->adjustToFitScreen(preview);
	}
}

static bool open_landmark_callback(const LLSD& notification,
								   const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	LLUUID asset_id = notification["payload"]["asset_id"].asUUID();
	LLUUID item_id = notification["payload"]["item_id"].asUUID();
	if (option == 0)
	{
		gAgent.teleportViaLandmark(asset_id);

		// We now automatically track the landmark you're teleporting to
		// because you will probably arrive at a fixed TP point instead.
		if (gFloaterWorldMapp)
		{
			// Remember this is the item UUID not the asset UUID
			gFloaterWorldMapp->trackLandmark(item_id);
		}
	}

	return false;
}
static LLNotificationFunctorRegistration open_landmark_callback_reg("TeleportFromLandmark",
																	open_landmark_callback);

void open_texture(const LLUUID& item_id, const std::string& title,
				  bool show_keep_discard, const LLUUID& source_id,
				  bool take_focus)
{
//MK
	if (gRLenabled && gRLInterface.contains("viewtexture"))
	{
		return;
	}
//mk
	// See if we can bring an exiting preview to the front
	if (!LLPreview::show(item_id, take_focus))
	{
		// There is none, so make a new preview
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);

		LLPreviewTexture* preview =
			new LLPreviewTexture("preview texture", rect, title, item_id,
								 LLUUID::null, show_keep_discard);
		preview->setSourceID(source_id);
		if (take_focus)
		{
			preview->setFocus(true);
		}
		gFloaterViewp->adjustToFitScreen(preview);
	}
}

void open_callingcard(LLViewerInventoryItem* item)
{
	LLUUID id = item ? item->getCreatorUUID() : LLUUID::null;
	if (id.notNull())
	{
		if (id == gAgentID)
		{
			// If the calling card was created by us, then it is most probably
			// a v2 viewer force-re-created calling card (if only LL could mind
			// their own ass and let users the choice of what should be kept or
			// not in their inventory !... It would also avoid such invalid
			// calling cards). Try to extract the target avatar UUID from its
			// description, if any.
			std::string desc = item->getActualDescription();
			if (LLUUID::validate(desc))
			{
				id.set(desc);
			}
		}
		bool online = id == gAgentID || gAvatarTracker.isBuddyOnline(id);
		LLFloaterAvatarInfo::showFromFriend(id, online);
	}
}

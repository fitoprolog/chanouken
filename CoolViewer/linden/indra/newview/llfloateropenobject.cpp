/** 
 * @file llfloateropenobject.cpp
 * @brief LLFloaterOpenObject class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

/*
 * Shows the contents of an object.
 * A floater wrapper for llpanelinventory
 */

#include "llviewerprecompiledheaders.h"

#include "llfloateropenobject.h"

#include "llnotifications.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloaterinventory.h"
#include "llinventorybridge.h"
#include "llpanelinventory.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"

//static
void* LLFloaterOpenObject::createPanelInventory(void* data)
{
	LLFloaterOpenObject* self = (LLFloaterOpenObject*)data;
	self->mPanelInventory = new LLPanelInventory("Object Contents", LLRect());
	return self->mPanelInventory;
}

LLFloaterOpenObject::LLFloaterOpenObject(const LLSD&)
:	mPanelInventory(NULL),
	mDirty(true)
{
	LLCallbackMap::map_t factory_map;
	factory_map["object_contents"] = LLCallbackMap(createPanelInventory, this);
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_openobject.xml",
												 &factory_map);
}

//virtual
bool LLFloaterOpenObject::postBuild()
{
	childSetAction("copy_to_inventory_button", onClickMoveToInventory, this);

	if (gSavedSettings.getBool("EnableCopyAndWear"))
	{
		childSetAction("copy_and_wear_button", onClickMoveAndWear, this);
	}
	else
	{
		childSetVisible("copy_and_wear_button", false);
	}

	// *Note: probably do not want to translate this
	childSetTextArg("object_name", "[DESC]", std::string("Object"));

	center();

	return true;
}

//virtual
void LLFloaterOpenObject::refresh()
{
	mPanelInventory->refresh();

	LLSelectNode* node = mObjectSelection->getFirstRootNode();
	if (node)
	{
		std::string name = node->mName;
		childSetTextArg("object_name", "[DESC]", name);
	}
}

//virtual
void LLFloaterOpenObject::draw()
{
	if (mDirty)
	{
		refresh();
		mDirty = false;
	}
	LLFloater::draw();
}

void LLFloaterOpenObject::moveToInventory(bool wear)
{
	if (mObjectSelection->getRootObjectCount() != 1)
	{
		gNotifications.add("OnlyCopyContentsOfSingleItem");
		return;
	}

	LLSelectNode* node = mObjectSelection->getFirstRootNode();
	if (!node) return;

	LLViewerObject* object = node->getObject();
	if (!object) return;

	LLUUID object_id = object->getID();
	std::string name = node->mName;

	// Either create a sub-folder of clothing, or of the root folder.
	LLUUID parent_cat_id;
	if (wear)
	{
		parent_cat_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
	}
	else
	{
		parent_cat_id = gInventory.getRootFolderID();
	}

	inventory_func_type func = boost::bind(LLFloaterOpenObject::callbackCreateInventoryCategory,
										   _1, object_id, wear);
	LLUUID cat_id = gInventory.createNewCategory(parent_cat_id,
												 LLFolderType::FT_NONE,
												 name, func);

	// If we get a null category ID, we are using a capability in
	// createNewCategory and we will handle the following in the
	// callbackCreateInventoryCategory routine.
	if (cat_id.notNull())
	{
		LLCatAndWear* data = new LLCatAndWear;
		data->mCatID = cat_id;
		data->mWear = wear;
		data->mFolderResponded = false;

		// Copy and/or move the items into the newly created folder.
		// Ignore any "you're going to break this item" messages.
		bool success = move_inv_category_world_to_agent(object_id, cat_id,
														true,
														callbackMoveInventory, 
														(void*)data);
		if (!success)
		{
			delete data;
			data = NULL;

			gNotifications.add("OpenObjectCannotCopy");
		}
	}
}

//static
void LLFloaterOpenObject::dirty()
{
	LLFloaterOpenObject* self = findInstance();
	if (self)
	{
		self->mDirty = true;
	}
}

//static
void LLFloaterOpenObject::show()
{
	LLObjectSelectionHandle object_selection = gSelectMgr.getSelection();
	if (object_selection->getRootObjectCount() != 1)
	{
		gNotifications.add("UnableToViewContentsMoreThanOne");
		return;
	}
//MK
	if (gRLenabled && gRLInterface.mContainsEdit)
	{
		LLViewerObject* objp = gSelectMgr.getSelection()->getPrimaryObject();
		if (objp && !gRLInterface.canEdit(objp))
		{
			return;
		}
	}
//mk

	LLFloaterOpenObject* self = getInstance(); // Create new instance if needed
	self->open();
	self->setFocus(true);
	self->mObjectSelection = gSelectMgr.getEditSelection();
}

//static
void LLFloaterOpenObject::callbackCreateInventoryCategory(const LLUUID& cat_id,
														  LLUUID object_id,
														  bool wear)
{
	LLCatAndWear* wear_data = new LLCatAndWear;
	wear_data->mCatID = cat_id;
	wear_data->mWear = wear;
	wear_data->mFolderResponded = true;

 	// Copy and/or move the items into the newly created folder.
 	// Ignore any "you're going to break this item" messages.
 	if (!move_inv_category_world_to_agent(object_id, cat_id, true,
 										  callbackMoveInventory,
										  (void*)wear_data))
 	{
		delete wear_data;
		wear_data = NULL;

 		gNotifications.add("OpenObjectCannotCopy");
 	}
}

//static
void LLFloaterOpenObject::callbackMoveInventory(S32 result, void* data)
{
	LLCatAndWear* cat = (LLCatAndWear*)data;

	if (result == 0)
	{
		LLFloaterInventory::showAgentInventory();
		LLFloaterInventory* inv = LLFloaterInventory::getActiveFloater();
		if (inv)
		{
			inv->getPanel()->setSelection(cat->mCatID, TAKE_FOCUS_NO);
		}
	}

	delete cat;
}

//static
void LLFloaterOpenObject::onClickMoveToInventory(void* data)
{
	LLFloaterOpenObject* self = (LLFloaterOpenObject*)data;
	if (self)
	{
		self->moveToInventory(false);
		self->close();
	}
}

//static
void LLFloaterOpenObject::onClickMoveAndWear(void* data)
{
	LLFloaterOpenObject* self = (LLFloaterOpenObject*)data;
	if (self)
	{
//MK
		if (gRLenabled && gRLInterface.mContainsDetach)
		{
			self->moveToInventory(false);
		}
		else
		{
			self->moveToInventory(true);
		}
//mk
		self->close();
	}
}

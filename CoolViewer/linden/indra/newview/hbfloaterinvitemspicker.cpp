/**
 * @file hbfloaterinvitemspicker.cpp
 * @brief Generic inventory items picker.
 * Also replaces LL's environment settings picker.
 *
 * $LicenseInfo:firstyear=2019&license=viewergpl$
 *
 * Copyright (c) 2019, Henri Beauchamp
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

#include <deque>

#include "hbfloaterinvitemspicker.h"

#include "llbutton.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llinventorymodel.h"

HBFloaterInvItemsPicker::HBFloaterInvItemsPicker(LLView* owner,
												 callback_t cb, void* userdata)
:	mCallback(cb),
	mCallbackUserdata(userdata),
	mHasParentFloater(false),
	mAutoClose(true)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_inv_items_picker.xml");
	// Note: at this point postBuild() has been called and returned.
	LLView* parentp = owner;
	// Search for our owner's parent floater and register as dependent of
	// it if found.
	while (parentp)
	{
		LLFloater* floaterp = parentp->asFloater();
		if (floaterp)
		{
			mHasParentFloater = true;
			floaterp->addDependentFloater(this);
			break;
		}
		parentp = parentp->getParent();
	}
	if (!parentp)
	{
		// Place ourselves in a smart way, like preview floaters...
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		translate(left - getRect().mLeft, top - getRect().mTop);
		gFloaterViewp->adjustToFitScreen(this);
	}
}

//virtual
HBFloaterInvItemsPicker::~HBFloaterInvItemsPicker()
{
	gFocusMgr.releaseFocusIfNeeded(this);
}

//virtual
bool HBFloaterInvItemsPicker::postBuild()
{
	mSelectBtn = getChild<LLButton>("select_btn");
	mSelectBtn->setClickedCallback(onBtnSelect, this);
	mSelectBtn->setEnabled(false);

	childSetAction("close_btn", onBtnClose, this);

	mSearchEditor = getChild<LLSearchEditor>("search_editor");
	mSearchEditor->setSearchCallback(onSearchEdit, this);

	mInventoryPanel = getChild<LLInventoryPanel>("inventory_panel");
	mInventoryPanel->setFollowsAll();
	mInventoryPanel->setShowFolderState(LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
	mInventoryPanel->setSelectCallback(onInventorySelectionChange, this);

	setAllowMultiple(false);

	return true;
}

//virtual
void HBFloaterInvItemsPicker::onFocusLost()
{
	// NOTE: never auto-close when loosing focus if not parented
	if (mAutoClose && mHasParentFloater)
	{
		close();
	}
	else
	{
		LLFloater::onFocusLost();
	}
}

void HBFloaterInvItemsPicker::setAllowMultiple(bool allow_multiple)
{
	mInventoryPanel->setAllowMultiSelect(allow_multiple);
}

void HBFloaterInvItemsPicker::setExcludeLibrary(bool exclude)
{
	mInventoryPanel->setFilterHideLibrary(exclude);
}

void HBFloaterInvItemsPicker::setAssetType(LLAssetType::EType type,
										   S32 sub_type)
{
	U32 filter = 1 << LLInventoryType::defaultForAssetType(type);
	mInventoryPanel->setFilterTypes(filter);
	mInventoryPanel->setFilterSubType(sub_type);
	mInventoryPanel->openDefaultFolderForType(type);
	// Set the floater title according to the type of asset we want to pick
	std::string type_name = LLAssetType::lookupHumanReadable(type);
	LLUIString title = getString("title");
	LLSD args;
	args["ASSETTYPE"] = LLTrans::getString(type_name);
	title.setArgs(args);
	setTitle(title);
}

//static
void HBFloaterInvItemsPicker::onBtnSelect(void* userdata)
{
	HBFloaterInvItemsPicker* self = (HBFloaterInvItemsPicker*)userdata;
	if (!self) return;

	self->mCallback(self->mSelectedInvNames, self->mSelectedInvIDs,
					self->mCallbackUserdata);

	self->mInventoryPanel->setSelection(LLUUID::null, false);

	if (self->mAutoClose)
	{
		self->mAutoClose = false;
		self->close();
	}
}

//static
void HBFloaterInvItemsPicker::onBtnClose(void* userdata)
{
	HBFloaterInvItemsPicker* self = (HBFloaterInvItemsPicker*)userdata;
	if (self)
	{
		self->close();
	}
}

void HBFloaterInvItemsPicker::onSearchEdit(const std::string& search_str,
										   void* userdata)
{
	HBFloaterInvItemsPicker* self = (HBFloaterInvItemsPicker*)userdata;
	if (!self) return;	// Paranoia

	if (search_str.empty())
	{
		if (self->mInventoryPanel->getFilterSubString().empty())
		{
			// Current and new filters are empty: nothing to do !
			return;
		}

		self->mSavedFolderState.setApply(true);
		self->mInventoryPanel->getRootFolder()->applyFunctorRecursively(self->mSavedFolderState);
		// Add folder with current item to the list of previously opened
		// folders
		LLOpenFoldersWithSelection opener;
		self->mInventoryPanel->getRootFolder()->applyFunctorRecursively(opener);
		self->mInventoryPanel->getRootFolder()->scrollToShowSelection();

	}
	else if (self->mInventoryPanel->getFilterSubString().empty())
	{
		// User just typed the first letter in the search editor; save existing
		// folder open state.
		if (!self->mInventoryPanel->getRootFolder()->isFilterModified())
		{
			self->mSavedFolderState.setApply(false);
			self->mInventoryPanel->getRootFolder()->applyFunctorRecursively(self->mSavedFolderState);
		}
	}

	std::string uc_search_str = search_str;
	LLStringUtil::toUpper(uc_search_str);
	self->mInventoryPanel->setFilterSubString(uc_search_str);
}

//static
void HBFloaterInvItemsPicker::onInventorySelectionChange(LLFolderView* folderp,
														 bool, void* userdata)
{
	HBFloaterInvItemsPicker* self = (HBFloaterInvItemsPicker*)userdata;
	if (!self || !folderp)	// Paranoia
	{
		return;
	}

	self->mSelectedInvIDs.clear();
	self->mSelectedInvNames.clear();

	const LLFolderView::selected_items_t& items = folderp->getSelectedItems();
	for (std::deque<LLFolderViewItem*>::const_iterator it = items.begin(),
													   end = items.end();
		 it != end; ++it)
	{
		LLFolderViewEventListener* listenerp = (*it)->getListener();
		if (!listenerp) continue;	// Paranoia
		LLInventoryType::EType type = listenerp->getInventoryType();
		if (type != LLInventoryType::IT_CATEGORY &&
			type != LLInventoryType::IT_ROOT_CATEGORY)
		{
			LLInventoryItem* item = gInventory.getItem(listenerp->getUUID());
			if (item)	// Paranoia
			{
				self->mSelectedInvIDs.emplace_back(item->getUUID());
				self->mSelectedInvNames.emplace_back(listenerp->getName());
			}
		}
	}

	self->mSelectBtn->setEnabled(!self->mSelectedInvIDs.empty());
}

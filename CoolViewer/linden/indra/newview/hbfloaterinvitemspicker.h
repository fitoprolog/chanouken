/**
 * @file hbfloaterinventorypicker.h
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

#ifndef LL_HBFLOATERINVENTORYPICKER_H
#define LL_HBFLOATERINVENTORYPICKER_H

#include "llfloater.h"

#include "llfolderview.h"

class LLButton;
class LLFolderView;
class LLInventoryPanel;
class LLSearchEditor;

class HBFloaterInvItemsPicker final : public LLFloater
{
protected:
	LOG_CLASS(HBFloaterInvItemsPicker);

public:
	typedef void(*callback_t)(const std::vector<std::string>&,
							  const uuid_vec_t&, void*);

	// Call this to select one or several inventory items. The callback
	// function will be passed the selected inventory name(s) and UUID(s), if
	// any.
	// The inventory picker floater will automatically become dependent on the
	// parent floater of 'owner', if there is one (and if owner is not NULL, of
	// course), else it will stay independent.
	HBFloaterInvItemsPicker(LLView* owner, callback_t cb, void* userdata);

	~HBFloaterInvItemsPicker() override;

	// Use this method to restrict the inventory items asset type (and possibly
	// sub-type, such as for wearables and environment settings). Showing all
	// items types is the default behaviour when the floater is created.
	void setAssetType(LLAssetType::EType type, S32 sub_type = -1);

	// Use this method to (dis)allow multiple inventory items selection. Single
	// item selection is the default behaviour when the floater is created.
	void setAllowMultiple(bool allow_multiple = true);

	// Use this method to exclude the Library from the list of selectable items
	// (when the floater is created, the default behaviour is to show the
	// library).
	void setExcludeLibrary(bool exclude = true);

	// When 'auto_close' is true, the picker will auto-close when parented and
	// loosing focus (thus cancelling the picking action) and when the "Select"
	// button is pressed (else "Select" just invokes the callback). Auto-close
	// is the default behaviour when the floater is created.
	void setAutoClose(bool auto_close = true);

private:
	bool postBuild() override;
	void onFocusLost() override;

	static void onBtnSelect(void* userdata);
	static void onBtnClose(void* userdata);
	static void onSearchEdit(const std::string& search_string, void* userdata);
	static void onInventorySelectionChange(LLFolderView* folderp, bool,
										   void* userdata);

private:
	LLButton*					mSelectBtn;
	LLInventoryPanel*			mInventoryPanel;
	LLSearchEditor*				mSearchEditor;

	LLSaveFolderState			mSavedFolderState;

	uuid_vec_t					mSelectedInvIDs;
	std::vector<std::string>	mSelectedInvNames;

	void						(*mCallback)(const std::vector<std::string>& name,
											 const uuid_vec_t& id,
											 void* userdata);
	void*						mCallbackUserdata;

	bool						mHasParentFloater;
	bool						mAutoClose;
};

#endif

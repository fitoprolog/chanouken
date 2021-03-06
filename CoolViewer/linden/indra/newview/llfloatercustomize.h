/**
 * @file llfloatercustomize.h
 * @brief The customize avatar floater, triggered by "Appearance..."
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

#ifndef LL_LLFLOATERCUSTOMIZE_H
#define LL_LLFLOATERCUSTOMIZE_H

#include <map>

#include "llfloater.h"

#include "llviewerwearable.h"

class LLInventoryObserver;
class LLJoint;
class LLPanelEditWearable;
class LLScrollingPanelList;
class LLViewerJointMesh;
class LLViewerVisualParam;
class LLVisualParamReset;

class LLFloaterCustomize final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterCustomize);

public:
	typedef std::pair<bool, LLViewerVisualParam*> editable_param;
	typedef std::map<F32, editable_param> param_map;

public:
	LLFloaterCustomize();
	~LLFloaterCustomize() override;

	// Inherited methods from LLFloater (and above)
	bool postBuild() override;
	void onClose(bool app_quitting) override;
	void draw() override;
	void open() override;
	bool isDirty() const override;

	static bool isVisible();

	// New methods
	void clearScrollingPanelList();
	void generateVisualParamHints(LLPanelEditWearable* panel,
								  LLViewerJointMesh* joint_mesh,
								  param_map& params, LLWearable* wearable,
								  bool use_hints, LLJoint* jointp);

	void updateScrollingPanelList(bool allow_modify);

	void setWearable(LLWearableType::EType type, LLViewerWearable* wearable,
					 U32 perm_mask, bool is_complete);

	void askToSaveIfDirty(void(*next_step_callback)(bool, void*), void* data);

	void switchToDefaultSubpart();

	static void	 updateAvatarHeightDisplay();

	static void setCurrentWearableType(LLWearableType::EType type);

	LL_INLINE static LLWearableType::EType getCurrentWearableType()
	{
		return sCurrentWearableType;
	}

	LL_INLINE LLPanelEditWearable* getCurrentWearablePanel()
	{
		return mWearablePanelList[sCurrentWearableType];
	}

	void fetchInventory();
	void updateInventoryUI();
	void updateScrollingPanelUI();
	void updateWearableType(LLWearableType::EType type, LLViewerWearable* w);

private:
	void initWearablePanels();
	void initScrollingPanelList();

	// Callbacks
	static void onBtnOk(void* userdata);
	static void onBtnMakeOutfit(void* userdata);

	static void onTabChanged(void* userdata, bool from_click);
	static void onTabPrecommit(void* userdata, bool from_click);
	bool onSaveDialog(const LLSD& notification, const LLSD& response);
	static void onCommitChangeTab(bool proceed, void* userdata);

	static void* createWearablePanel(void* userdata);

private:
	LLPanelEditWearable*			mWearablePanelList[LLWearableType::WT_COUNT];

	LLScrollingPanelList*			mScrollingPanelList;
	LLVisualParamReset*				mResetParams;

	LLInventoryObserver*			mInventoryObserver;

	void							(*mNextStepAfterSaveCallback)(bool proceed,
																  void* userdata);

	void*							mNextStepAfterSaveUserdata;

	static LLWearableType::EType	sCurrentWearableType;
};

extern LLFloaterCustomize* gFloaterCustomizep;

#endif  // LL_LLFLOATERCUSTOMIZE_H

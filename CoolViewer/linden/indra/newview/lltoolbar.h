/**
 * @file lltoolbar.h
 * @brief Large friendly buttons at bottom of screen.
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

#ifndef LL_LLTOOLBAR_H
#define LL_LLTOOLBAR_H

#include "llpanel.h"

#include "llframetimer.h"

constexpr S32 TOOL_BAR_HEIGHT = 20;

class LLButton;
#if LL_DARWIN
class LLFakeResizeHandle;
#endif // LL_DARWIN

class LLToolBar final : public LLPanel
{
public:
	LLToolBar(const LLRect& rect);
	~LLToolBar() override;

	bool postBuild() override;

	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type, void* cargo_data,
						   EAcceptance* accept, std::string& tooltip) override;

	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

	static void toggle();
	static bool isVisible();

	// Move buttons to appropriate locations based on rect.
	void layoutButtons();

	// Per-frame refresh call
	void refresh() override;

	// Callbacks
	static void onClickChat(void* data);
	static void onClickIM(void*);
	static void onClickFriends(void* data);
	static void onClickGroups(void* data);
	static void onClickFly(void*);
	static void onClickSnapshot(void* data);
	static void onClickSearch(void* data);
	static void onClickBuild(void* data);
	static void onClickRadar(void* data);
	static void onClickMiniMap(void* data);
	static void onClickMap(void* data);
	static void onClickInventory(void* data);

	static F32 sInventoryAutoOpenTime;

private:
	LLButton*				mChatButton;
	LLButton*				mIMButton;
	LLButton*				mFriendsButton;
	LLButton*				mGroupsButton;
	LLButton*				mFlyButton;
	LLButton*				mSnapshotButton;
	LLButton*				mSearchButton;
	LLButton*				mBuildButton;
	LLButton*				mRadarButton;
	LLButton*				mMiniMapButton;
	LLButton*				mMapButton;
	LLButton*				mInventoryButton;
	LLFrameTimer			mInventoryAutoOpenTimer;
#if LL_DARWIN
	LLFakeResizeHandle*		mResizeHandle;
#endif // LL_DARWIN

	bool					mInventoryAutoOpen;
};

extern LLToolBar* gToolBarp;

#endif

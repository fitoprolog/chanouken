/**
 * @file llfloatergesture.h
 * @brief Read-only list of gestures from your inventory.
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

#ifndef LL_LLFLOATERGESTURE_H
#define LL_LLFLOATERGESTURE_H

#include "llfloater.h"

class LLFloaterGestureObserver;
class LLScrollListCtrl;

class LLFloaterGesture final : public LLFloater,
							   public LLFloaterSingleton<LLFloaterGesture>
{
	friend class LLUISingleton<LLFloaterGesture, VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLFloaterGesture);

	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	LLFloaterGesture(const LLSD&);
	~LLFloaterGesture() override;

	bool postBuild() override;

public:
	static void refreshAll();

private:
	// Reads from the gesture manager's list of active gestures and puts them
	// in this list.
	void buildGestureList();

	static void onCommitList(LLUICtrl* ctrl, void* data);
	static void onClickEdit(void* data);
	static void onClickPlay(void* data);
	static void onClickNew(void* data);
#if 0	// Not used
	static void onClickInventory(void* data);
#endif

private:
	LLFloaterGestureObserver*	mObserver;
	LLScrollListCtrl*			mGesturesList;
};

#endif

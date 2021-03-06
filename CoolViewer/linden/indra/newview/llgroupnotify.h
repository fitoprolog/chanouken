/**
 * @file llgroupnotify.h
 * @brief Non-blocking notification that does not take keyboard focus.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LL_LLGROUPNOTIFY_H
#define LL_LLGROUPNOTIFY_H

#include "llinitdestroyclass.h"
#include "llnotifications.h"
#include "llpanel.h"
#include "lltimer.h"

class LLButton;
class LLOfferInfo;

// NotifyBox - for notifications that require a response from the user.
class LLGroupNotifyBox final : public LLPanel,
							   public LLInitClass<LLGroupNotifyBox>
{
protected:
	LOG_CLASS(LLGroupNotifyBox);

public:
	void close();

	static void initClass();
	static void destroyClass();
	static bool onNewNotification(const LLSD& notification);

	static LL_INLINE S32 getGroupNotifyBoxCount()		{ return sGroupNotifyBoxCount; }

protected:
	// Non-transient messages. You can specify non-default button layouts (like
	// one for script dialogs) by passing various numbers in for "layout".
	LLGroupNotifyBox(const std::string& subject, const std::string& message,
					 const std::string& from_name, const LLUUID& group_id,
					 const LLUUID& group_insign, const std::string& group_name,
					 const LLDate& time_stamp, bool has_inventory,
					 const std::string& inv_name, const LLSD& inventory_offer);

	~LLGroupNotifyBox() override;

	// A right click on the notification sends it to the back of the pile
	bool handleRightMouseDown(S32, S32, MASK) override;

	// Animate as sliding onto the screen.
	void draw() override;

	void moveToBack();

	// Returns the rect, relative to gNotifyView, where this notify box should
	// be placed.
	static LLRect getGroupNotifyRect();

	// Internal handler for button being clicked
	static void onClickOk(void* data);
	static void onClickGroupInfo(void* data);
	static void onClickSaveInventory(void* data);

	// For "next" button
	static void onClickNext(void* data);

private:
	LLButton*		mNextBtn;
	LLButton*		mSaveInventoryBtn;

	LLOfferInfo*	mInventoryOffer;

	LLUUID			mGroupID;

	// Time since this notification was displayed. This is a LLTimer not a
	// frame timer because I am concerned that I could be out-of-sync by one
	// frame in the animation.
	LLTimer			mTimer;

	bool				mHasInventory;
	bool				mAnimating;				// Are we sliding onscreen ?

	static S32			sGroupNotifyBoxCount;
};

constexpr S32 GROUP_LAYOUT_DEFAULT = 0;
constexpr S32 GROUP_LAYOUT_SCRIPT_DIALOG = 1;

#endif

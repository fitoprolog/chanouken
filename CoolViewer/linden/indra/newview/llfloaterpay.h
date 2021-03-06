/**
 * @file llfloaterpay.h
 * @brief LLFloaterPay class header file
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

#ifndef LL_LLFLOATERPAY_H
#define LL_LLFLOATERPAY_H

#include "llfloater.h"
#include "lllslconstants.h"
#include "llsafehandle.h"
#include "lluuid.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterPay
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLButton;
class LLLineEditor;
class LLMessageSystem;
class LLObjectSelection;
class LLTextBox;
class LLViewerRegion;
struct LLGiveMoneyInfo;

typedef void (*money_callback)(const LLUUID&, LLViewerRegion*, S32, bool, S32,
							   const std::string&);

class LLFloaterPay : public LLFloater
{
public:
	LLFloaterPay(const std::string& name, money_callback callback,
				 const LLUUID& target_id, bool target_is_object);
	virtual ~LLFloaterPay();

	// Pay into an in-world object, which will trigger scripts and eventually
	// transfer the L$ to the resident or group that owns the object. Object
	// must be selected. Must pass object id, because it may be a child of the
	// selected object.
	static void payViaObject(money_callback callback, const LLUUID& object_id);

	// Pay an avatar or group directly, not via an object in the world.
	// Scripts are not notified, L$ can be direcly transferred.
	static void payDirectly(money_callback callback, const LLUUID& target_id,
							bool is_group);

private:
	bool give(S32 amount);

	static void onCancel(void* data);
	static void onKeystroke(LLLineEditor*, void* data);
	static void onGive(void* data);
	static void processPayPriceReply(LLMessageSystem* msg, void** userdata);
	static void onCacheOwnerName(const LLUUID& owner_id,
								 const std::string& full_name, bool is_group,
								 LLFloaterPay* self);
	void finishPayUI(const LLUUID& target_id, bool is_group);

protected:
	LLUUID							mTargetUUID;

	LLGiveMoneyInfo*				mQuickPayInfo[MAX_PAY_BUTTONS];
	LLButton*						mQuickPayButton[MAX_PAY_BUTTONS];
	LLTextBox*						mObjectNameText;
	LLLineEditor*					mPayMessageText;

	LLSafeHandle<LLObjectSelection>	mObjectSelection;

	money_callback					mCallback;
	std::vector<LLGiveMoneyInfo*>	mCallbackData;

	bool							mTargetIsObject;
	bool							mTargetIsGroup;
	bool							mHaveName;

	static S32						sLastAmount;

private:
	static std::set<LLFloaterPay*>	sInstances;
};

#endif // LL_LLFLOATERPAY_H

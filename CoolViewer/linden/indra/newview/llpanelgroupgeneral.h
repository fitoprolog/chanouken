/**
 * @file llpanelgroupgeneral.h
 * @brief General information about a group.
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

#ifndef LL_LLPANELGROUPGENERAL_H
#define LL_LLPANELGROUPGENERAL_H

#include "llpanelgroup.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLNameBox;
class LLNameListCtrl;
class LLSpinCtrl;
class LLTextBox;
class LLTextureCtrl;
class LLTextEditor;
class LLTimer;

class LLPanelGroupGeneral : public LLPanelGroupTab
{
protected:
	LOG_CLASS(LLPanelGroupGeneral);

public:
	LLPanelGroupGeneral(const std::string& name, const LLUUID& group_id);

	// LLPanelGroupTab
	static void* createTab(void* data);
	void activate() override;
	bool needsApply(std::string& mesg) override;
	bool apply(std::string& mesg) override;
	void cancel() override;
	bool createGroupCallback(const LLSD& notification, const LLSD& response);

	void update(LLGroupChange gc) override;

	bool postBuild() override;

	void draw() override;

private:
	static void onFocusEdit(LLFocusableElement* ctrl, void* data);
	static void onCommitAny(LLUICtrl* ctrl, void* data);
	static void onCommitUserOnly(LLUICtrl* ctrl, void* data);
	static void onCommitTitle(LLUICtrl* ctrl, void* data);
	static void onCommitEnrollment(LLUICtrl* ctrl, void* data);
	static void onClickJoin(void* userdata);
	static void onClickInfo(void* userdata);
	static void onReceiveNotices(LLUICtrl* ctrl, void* data);
	static void openProfile(void* data);

    static bool joinDlgCB(const LLSD& notification, const LLSD& response);

	void updateMembers();
	void updateChanged();
	bool confirmMatureApply(const LLSD& notification, const LLSD& response);

private:
	LLTimer			mUpdateTimer;
	F32				mUpdateInterval;
	bool			mSkipNextUpdate;
	bool			mPendingMemberUpdate;
	bool			mChanged;
	bool			mFirstUse;
	std::string		mIncompleteMemberDataStr;
	LLUUID			mDefaultIconID;

	// Group information (include any updates in updateChanged)
	LLLineEditor*	mGroupNameEditor;
	LLTextBox*		mGroupName;
	LLNameBox*		mFounderName;
	LLTextureCtrl*	mInsignia;
	LLTextEditor*	mEditCharter;
	LLButton*		mBtnJoinGroup;
	LLButton*		mBtnInfo;

	LLNameListCtrl*	mListVisibleMembers;

	// Options (include any updates in updateChanged)
	LLCheckBoxCtrl*	mCtrlShowInGroupList;
	LLCheckBoxCtrl*	mCtrlOpenEnrollment;
	LLCheckBoxCtrl*	mCtrlEnrollmentFee;
	LLSpinCtrl*		mSpinEnrollmentFee;
	LLCheckBoxCtrl*	mCtrlReceiveNotices;
	LLCheckBoxCtrl*	mCtrlReceiveChat;
	LLCheckBoxCtrl*	mCtrlListGroup;
	LLTextBox*		mActiveTitleLabel;
	LLComboBox*		mComboActiveTitle;
	LLComboBox*		mComboMature;

	LLGroupMgrGroupData::member_list_t::iterator mMemberProgress;
};

#endif

/**
 * @file llpanelavatar.h
 * @brief LLPanelAvatar and related class definitions
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

#ifndef LL_LLPANELAVATAR_H
#define LL_LLPANELAVATAR_H

#include "llavatarnamecache.h"
#include "llpanel.h"
#include "lluuid.h"
#include "llvector3d.h"

#include "llmediactrl.h"

class LLButton;
class LLCheckBoxCtrl;
class LLDropTarget;
class LLFlyoutButton;
class LLLineEditor;
class LLMessageSystem;
class LLPanelAvatar;
class LLScrollListCtrl;
class LLTabContainer;
class LLTextBox;
class LLTextEditor;
class LLTextureCtrl;
class LLUICtrl;

enum EOnlineStatus
{
	ONLINE_STATUS_NO	= 0,
	ONLINE_STATUS_YES	= 1
};

// Base class for all sub-tabs inside the avatar profile. Many of these panels
// need to keep track of the parent panel (to get the avatar id) and only
// request data from the database when they are first drawn. JC
class LLPanelAvatarTab : public LLPanel
{
public:
	LLPanelAvatarTab(const std::string& name,
					 const LLRect& rect,
					 LLPanelAvatar* panel_avatar);

	// Calls refresh() once per frame when panel is visible
	void draw() override;

	LL_INLINE LLPanelAvatar* getPanelAvatar() const	{ return mPanelAvatar; }

	LL_INLINE void setDataRequested(bool b)			{ mDataRequested = b; }
	LL_INLINE bool isDataRequested() const			{ return mDataRequested; }

	// If the data for this tab has not yet been requested, send the request.
	// Used by tabs that are filled in only when they are first displayed.
	// 'method' is one of "avatarnotesrequest", "avatarpicksrequest",
	// or "avatarclassifiedsrequest"
	void sendAvatarProfileRequestIfNeeded(const std::string& method);

private:
	LLPanelAvatar*	mPanelAvatar;
	bool			mDataRequested;
};

class LLPanelAvatarFirstLife final : public LLPanelAvatarTab
{
public:
	LLPanelAvatarFirstLife(const std::string& name,
						   const LLRect& rect,
						   LLPanelAvatar* panel_avatar);

	bool postBuild() override;

	void enableControls(bool own_avatar);

public:
	LLTextureCtrl*	m1stLifePicture;
	LLTextEditor*	mAbout1stLifeText;
};

class LLPanelAvatarSecondLife final : public LLPanelAvatarTab
{
public:
	LLPanelAvatarSecondLife(const std::string& name,
							const LLRect& rect,
							LLPanelAvatar* panel_avatar);

	bool postBuild() override;
	void refresh() override;

	static void onClickFriends(void* userdata);
	static void onDoubleClickGroup(void* userdata);
	static void onClickShowInSearchHelp(void* userdata);
	static void onClickPartnerHelp(void* userdata);
	static bool onClickPartnerHelpLoadURL(const LLSD& notification,
										  const LLSD& response);
	static void onClickPartnerInfo(void* userdata);

	// Clear out the controls anticipating new network data.
	void clearControls();
	void enableControls(bool own_avatar);
	void updateOnlineText(bool online, bool have_calling_card);
	void updatePartnerName();

	LL_INLINE void setPartnerID(const LLUUID& id)	{ mPartnerID = id; }

public:
	LLTextureCtrl*		m2ndLifePicture;

	LLLineEditor*		mBornText;
	LLTextBox*			mOnlineText;
	LLTextBox*			mAccountInfoText;

	LLScrollListCtrl*	mGroupsListCtrl;

	LLTextEditor*		mAbout2ndLifeText;
	LLCheckBoxCtrl*		mShowInSearchCheck;
	LLButton*			mShowInSearchHelpButton;

	LLButton*			mFindOnMapButton;
	LLButton*			mOfferTPButton;
	LLButton*			mRequestTPButton;
	LLButton*			mAddFriendButton;
	LLButton*			mPayButton;
	LLButton*			mIMButton;
	LLButton*			mMuteButton;

private:
	LLUUID				mPartnerID;
};

// WARNING !  The order of the inheritance here matters !  Do not change. - KLW
class LLPanelAvatarWeb final : public LLPanelAvatarTab,
							   public LLViewerMediaObserver
{
public:
	LLPanelAvatarWeb(const std::string& name,
					 const LLRect& rect,
					 LLPanelAvatar* panel_avatar);

	bool postBuild() override;

	void refresh() override;

	void enableControls(bool own_avatar);

	void setWebURL(std::string url);
	LL_INLINE const std::string& getWebURL() const	{ return mHome; }

	void load(const std::string& url);

	static void onURLKeystroke(LLLineEditor* editor, void* data);
	static void onCommitLoad(LLUICtrl* ctrl, void* data);
	static void onCommitSLWebProfile(LLUICtrl* ctrl, void* data);
	static void onCommitURL(LLUICtrl* ctrl, void* data);
	static void onClickWebProfileHelp(void*);

	// Inherited from LLViewerMediaObserver
	void handleMediaEvent(LLPluginClassMedia* self, EMediaEvent evt) override;

private:
	bool			mCanEditURL;
	std::string		mHome;
	std::string		mNavigateTo;
	LLFlyoutButton* mWebProfileBtn;
	LLMediaCtrl*	mWebBrowser;
};

class LLPanelAvatarAdvanced final : public LLPanelAvatarTab
{
public:
	LLPanelAvatarAdvanced(const std::string& name,
						  const LLRect& rect,
						  LLPanelAvatar* panel_avatar);

	bool postBuild() override;

	void enableControls(bool own_avatar);
	void setWantSkills(U32 want_to_mask, const std::string& want_to_text,
					   U32 skills_mask, const std::string& skills_text,
					   const std::string& languages_text);
	void getWantSkills(U32* want_to_mask, std::string& want_to_text,
					   U32* skills_mask, std::string& skills_text,
					   std::string& languages_text);

private:
	S32				mWantToCount;
	S32				mSkillsCount;
	LLCheckBoxCtrl*	mWantToCheck[8];
	LLLineEditor*	mWantToEdit;
	LLCheckBoxCtrl*	mSkillsCheck[8];
	LLLineEditor*	mSkillsEdit;
};

class LLPanelAvatarNotes final : public LLPanelAvatarTab
{
public:
	LLPanelAvatarNotes(const std::string& name, const LLRect& rect,
					   LLPanelAvatar* panel_avatar);

	bool postBuild() override;

	void refresh() override;

	void clearControls();

	static void onCommitNotes(LLUICtrl* field, void* userdata);
};

class LLPanelAvatarClassified final : public LLPanelAvatarTab
{
public:
	LLPanelAvatarClassified(const std::string& name,
							const LLRect& rect,
							LLPanelAvatar* panel_avatar);

	bool postBuild() override;

	void refresh() override;

	// If can close, return true. If cannot close, pop save/discard dialog
	// and return false.
	bool canClose();

	void apply();

	bool titleIsValid();

	// Delete all the classified sub-panels from the tab container
	void deleteClassifiedPanels();

	// Unpack the outline of classified for this avatar (count, names, but not
	// actual data).
	void processAvatarClassifiedReply(LLMessageSystem* msg, void**);

private:
	static void onClickNew(void* data);
	static void onClickDelete(void* data);

	bool callbackDelete(const LLSD& notification, const LLSD& response);
	bool callbackNew(const LLSD& notification, const LLSD& response);

private:
	LLTabContainer*	mClassifiedTab;
	LLButton*		mButtonNew;
	LLButton*		mButtonDelete;
	LLTextBox*		mLoadingText;
};

class LLPanelAvatarPicks final : public LLPanelAvatarTab
{
public:
	LLPanelAvatarPicks(const std::string& name,
					   const LLRect& rect,
					   LLPanelAvatar* panel_avatar);

	bool postBuild() override;

	void refresh() override;

	// Delete all the pick sub-panels from the tab container
	void deletePickPanels();

	// Unpack the outline of picks for this avatar (count, names, but not
	// actual data).
	void processAvatarPicksReply(LLMessageSystem* msg, void**);
	void processAvatarClassifiedReply(LLMessageSystem* msg, void**);

private:
	static void onClickNew(void* data);
	static void onClickDelete(void* data);

	bool callbackDelete(const LLSD& notification, const LLSD& response);

private:
	LLTabContainer*	mPicksTab;
	LLButton*		mButtonNew;
	LLButton*		mButtonDelete;
	LLTextBox*		mLoadingText;
};

class LLPanelAvatar final : public LLPanel
{
public:
	LLPanelAvatar(const std::string& name, const LLRect& rect,
				  bool allow_edit);
	~LLPanelAvatar() override;

	bool postBuild() override;

	// If can close, return true. If cannot close, pop save/discard dialog
	// and return false.
	bool canClose();

	// Fill in the avatar ID and handle some field fill-in, as well as
	// button enablement.
	// Pass one of the ONLINE_STATUS_foo constants above.
	void setAvatarID(const LLUUID& avatar_id, const std::string& name,
					 EOnlineStatus online_status);

	void setOnlineStatus(EOnlineStatus online_status);

	LL_INLINE const LLUUID& getAvatarID() const		{ return mAvatarID; }

	LL_INLINE const std::string& getAvatarUserName() const
	{
		return mAvatarUserName;
	}

	void resetGroupList();

	void sendAvatarStatisticsRequest();

	void sendAvatarPropertiesRequest();
	void sendAvatarPropertiesUpdate();

	void sendAvatarNotesRequest();
	void sendAvatarNotesUpdate();

	void sendAvatarPicksRequest();

	void selectTab(S32 tabnum);
	void selectTabByName(std::string tab_name);

	LL_INLINE bool haveData() const					{ return mHaveProperties; }
	LL_INLINE bool isEditable() const				{ return mAllowEdit; }

	static void processAvatarPropertiesReply(LLMessageSystem* msg, void**);
	static void processAvatarInterestsReply(LLMessageSystem* msg, void**);
	static void processAvatarGroupsReply(LLMessageSystem* msg, void**);
	static void processAvatarNotesReply(LLMessageSystem* msg, void**);
	static void processAvatarPicksReply(LLMessageSystem* msg, void**);
	static void processAvatarClassifiedReply(LLMessageSystem* msg, void**);

	static void onClickTrack(void* userdata);
	static void onClickIM(void* userdata);
	static void onClickOfferTeleport(void* userdata);
	static void onClickRequestTeleport(void* userdata);
	static void onClickPay(void* userdata);
	static void onClickAddFriend(void* userdata);
	static void onClickOK(void* userdata);
	static void onClickCancel(void* userdata);
	static void onClickKick(void* userdata);
	static void onClickFreeze(void* userdata);
	static void onClickUnfreeze(void* userdata);
	static void onClickCSR(void* userdata);
	static void onClickMute(void* userdata);

private:
	void enableOKIfReady();

	static bool finishKick(const LLSD& notification, const LLSD& response);
	static bool finishFreeze(const LLSD& notification, const LLSD& response);
	static bool finishUnfreeze(const LLSD& notification, const LLSD& response);

	static void showProfileCallback(S32 option, void* userdata);
	static void completeNameCallback(const LLUUID& agent_id,
									 const LLAvatarName& avatar_name,
									 void* userdata);

	static void* createPanelAvatar(void* data);
	static void* createFloaterAvatarInfo(void* data);
	static void* createPanelAvatarSecondLife(void* data);
	static void* createPanelAvatarWeb(void* data);
	static void* createPanelAvatarInterests(void* data);
	static void* createPanelAvatarPicks(void* data);
	static void* createPanelAvatarClassified(void* data);
	static void* createPanelAvatarFirstLife(void* data);
	static void* createPanelAvatarNotes(void* data);

public:
	LLPanelAvatarSecondLife*	mPanelSecondLife;
	LLPanelAvatarAdvanced*		mPanelAdvanced;
	LLPanelAvatarClassified*	mPanelClassified;
	LLPanelAvatarPicks*			mPanelPicks;
	LLPanelAvatarNotes*			mPanelNotes;
	LLPanelAvatarFirstLife*		mPanelFirstLife;
	LLPanelAvatarWeb*			mPanelWeb;

	LLDropTarget* 				mDropTarget;

	// Teen users are not allowed to see or enter data into the first life
	// page, or their own about/interests text entry fields.
	static bool sAllowFirstLife;

	// Tool tip and text strings, defined in panel_avatar.xml
	static std::string	sLoading;			// public

private:
	static std::string	sClickToEnlarge;
	static std::string	sShowOnMapNonFriend;
	static std::string	sShowOnMapFriendOffline;
	static std::string	sShowOnMapFriendOnline;
	static std::string	sTeleportGod;
	static std::string	sTeleportPrelude;
	static std::string	sTeleportNormal;

	LLUUID				mAvatarID;			// Which avatar's window is this ?
	std::string			mAvatarUserName;	// Known avatar user name
	bool				mIsFriend;			// Are we friends?
	bool				mHaveProperties;
	// only update note if data received from database and note is changed from
	// database version
	bool				mHaveNotes;
	std::string			mLastNotes;

	LLTabContainer*		mTab;
	bool				mAllowEdit;

	typedef std::list<LLPanelAvatar*> panel_list_t;
	static panel_list_t	sInstances;

	LLButton*			mOKButton;
	LLButton*			mCancelButton;
	LLButton*			mKickButton;
	LLButton*			mFreezeButton;
	LLButton*			mUnfreezeButton;
	LLButton*			mCSRButton;
};

// helper funcs
void add_left_label(LLPanel* panel, const std::string& name, S32 y);

#endif // LL_LLPANELAVATAR_H

/**
 * @file llpanelavatar.cpp
 * @brief LLPanelAvatar and related class implementations
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

#include "llviewerprecompiledheaders.h"

#include "llpanelavatar.h"

#include "llcachename.h"
#include "llcheckboxctrl.h"
#include "llclassifiedflags.h"
#include "llcombobox.h"
#include "lleconomy.h"
#include "llfloater.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llnameeditor.h"
#include "llpluginclassmedia.h"
#include "llscrolllistctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexturectrl.h"
#include "lluictrlfactory.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llavatartracker.h"
#include "llfloateravatarinfo.h"
#include "llfloaterfriends.h"
#include "llfloatergroupinfo.h"
#include "llfloatermediabrowser.h"
#include "llfloatermute.h"
#include "llfloaterworldmap.h"
#include "llgridmanager.h"			// For gIsInSecondLife
#include "llinventorymodel.h"
#include "llmutelist.h"
#include "llpanelclassified.h"
#include "llpanelpick.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llstatusbar.h"
#include "lltooldraganddrop.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"		// For send_generic_message()
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewertexteditor.h"
#include "llvoavatar.h"
#include "llweb.h"

// Some constants:

// For Flags in AvatarPropertiesReply
constexpr U32 AVATAR_ALLOW_PUBLISH	= 0x1 << 0;	// whether profile is externally visible or not
#if 0	// Not used here
constexpr U32 AVATAR_MATURE_PUBLISH	= 0x1 << 1;	// profile is "mature"
#endif
constexpr U32 AVATAR_IDENTIFIED		= 0x1 << 2;	// whether avatar has provided payment info
constexpr U32 AVATAR_TRANSACTED		= 0x1 << 3;	// whether avatar has actively used payment info
constexpr U32 AVATAR_ONLINE			= 0x1 << 4; // the online status of this avatar, if known.
constexpr U32 AVATAR_AGEVERIFIED	= 0x1 << 5;	// whether avatar has been age-verified

// Statics
std::list<LLPanelAvatar*> LLPanelAvatar::sInstances;
bool LLPanelAvatar::sAllowFirstLife = false;

std::string LLPanelAvatar::sLoading;
std::string LLPanelAvatar::sClickToEnlarge;
std::string LLPanelAvatar::sShowOnMapNonFriend;
std::string LLPanelAvatar::sShowOnMapFriendOffline;
std::string LLPanelAvatar::sShowOnMapFriendOnline;
std::string LLPanelAvatar::sTeleportGod;
std::string LLPanelAvatar::sTeleportPrelude;
std::string LLPanelAvatar::sTeleportNormal;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLDropTarget
//
// This handy class is a simple way to drop something on another
// view. It handles drop events, always setting itself to the size of
// its parent.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLDropTarget : public LLView
{
protected:
	LOG_CLASS(LLDropTarget);

public:
	LLDropTarget(const std::string& name, const LLRect& rect,
				 const LLUUID& agent_id);

	void doDrop(EDragAndDropType cargo_type, void* cargo_data);

	// LLView functionality
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type, void* cargo_data,
						   EAcceptance* accept, std::string& tooltip) override;

	void setAgentID(const LLUUID &agent_id)		{ mAgentID = agent_id; }

protected:
	LLUUID mAgentID;
};

LLDropTarget::LLDropTarget(const std::string& name,
						   const LLRect& rect,
						   const LLUUID& agent_id)
:	LLView(name, rect, false, FOLLOWS_ALL),
	mAgentID(agent_id)
{
}

void LLDropTarget::doDrop(EDragAndDropType cargo_type, void* cargo_data)
{
	llinfos << "No operation." << llendl;
}

bool LLDropTarget::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
									 EDragAndDropType cargo_type,
									 void* cargo_data,
									 EAcceptance* accept,
									 std::string& tooltip_msg)
{
	if (getParent())
	{
		LLToolDragAndDrop::handleGiveDragAndDrop(mAgentID, LLUUID::null, drop,
												 cargo_type, cargo_data, accept);

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// LLPanelAvatarTab class
//-----------------------------------------------------------------------------

LLPanelAvatarTab::LLPanelAvatarTab(const std::string& name, const LLRect& rect,
								   LLPanelAvatar* panel_avatar)
:	LLPanel(name, rect),
	mPanelAvatar(panel_avatar),
	mDataRequested(false)
{
}

//virtual
void LLPanelAvatarTab::draw()
{
	refresh();
	LLPanel::draw();
}

void LLPanelAvatarTab::sendAvatarProfileRequestIfNeeded(const std::string& method)
{
	if (!mDataRequested)
	{
		std::vector<std::string> strings;
		strings.emplace_back(mPanelAvatar->getAvatarID().asString());
		send_generic_message(method, strings);
		mDataRequested = true;
	}
}

//-----------------------------------------------------------------------------
// LLPanelAvatarSecondLife class
//-----------------------------------------------------------------------------

LLPanelAvatarSecondLife::LLPanelAvatarSecondLife(const std::string& name,
												 const LLRect& rect,
												 LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar),
	mPartnerID()
{
}

bool LLPanelAvatarSecondLife::postBuild()
{
	mBornText = getChild<LLLineEditor>("born");
	mBornText->setEnabled(false);

	mAccountInfoText = getChild<LLTextBox>("acct");

	childSetEnabled("partner_edit", false);
	childSetAction("partner_help", onClickPartnerHelp, this);
	childSetAction("partner_info", onClickPartnerInfo, this);
	childSetEnabled("partner_info", mPartnerID.notNull());

	childSetVisible("About:", LLPanelAvatar::sAllowFirstLife);
	childSetVisible("(500 chars)", LLPanelAvatar::sAllowFirstLife);

	mAbout2ndLifeText = getChild<LLTextEditor>("about");
	mAbout2ndLifeText->setVisible(LLPanelAvatar::sAllowFirstLife);

	mShowInSearchCheck = getChild<LLCheckBoxCtrl>("show_in_search_chk");
	mShowInSearchCheck->setVisible(LLPanelAvatar::sAllowFirstLife);

	mShowInSearchHelpButton = getChild<LLButton>("show_in_search_help_btn");
	mShowInSearchHelpButton->setClickedCallback(onClickShowInSearchHelp, this);
	mShowInSearchHelpButton->setVisible(LLPanelAvatar::sAllowFirstLife);

	mOnlineText = getChild<LLTextBox>("online_yes");
	mOnlineText->setVisible(false);

	mFindOnMapButton = getChild<LLButton>("find_on_map_btn");
	mFindOnMapButton->setClickedCallback(LLPanelAvatar::onClickTrack,
										 getPanelAvatar());

	mOfferTPButton = getChild<LLButton>("offer_tp_btn");
	mOfferTPButton->setClickedCallback(LLPanelAvatar::onClickOfferTeleport,
									   getPanelAvatar());

	mRequestTPButton = getChild<LLButton>("request_tp_btn");
	mRequestTPButton->setClickedCallback(LLPanelAvatar::onClickRequestTeleport,
										 getPanelAvatar());

	mAddFriendButton = getChild<LLButton>("add_friend_btn");
	mAddFriendButton->setClickedCallback(LLPanelAvatar::onClickAddFriend,
										 getPanelAvatar());

	mPayButton = getChild<LLButton>("pay_btn");
	mPayButton->setClickedCallback(LLPanelAvatar::onClickPay,
								   getPanelAvatar());

	mIMButton = getChild<LLButton>("im_btn");
	mIMButton->setClickedCallback(LLPanelAvatar::onClickIM, getPanelAvatar());

	mMuteButton = getChild<LLButton>("mute_btn");
	mMuteButton->setClickedCallback(LLPanelAvatar::onClickMute,
									getPanelAvatar());

	mGroupsListCtrl = getChild<LLScrollListCtrl>("groups");
	mGroupsListCtrl->setDoubleClickCallback(onDoubleClickGroup);
	mGroupsListCtrl->setCallbackUserData(this);

	m2ndLifePicture = getChild<LLTextureCtrl>("img");
	m2ndLifePicture->setFallbackImageName("default_profile_picture.j2c");

	enableControls(getPanelAvatar()->getAvatarID() == gAgentID);

	return true;
}

void LLPanelAvatarSecondLife::refresh()
{
	updatePartnerName();
}

void LLPanelAvatarSecondLife::updatePartnerName()
{
	if (mPartnerID.notNull())
	{
		std::string first, last;
		if (gCacheNamep && gCacheNamep->getName(mPartnerID, first, last))
		{
			childSetTextArg("partner_edit", "[FIRST]", first);
			childSetTextArg("partner_edit", "[LAST]", last);
		}
		childSetEnabled("partner_info", true);
	}
}

// Empty the data out of the controls, since we have to wait for new data off
// the network.
void LLPanelAvatarSecondLife::clearControls()
{
	if (m2ndLifePicture)
	{
		m2ndLifePicture->setImageAssetID(LLUUID::null);
	}
	mAbout2ndLifeText->setValue("");
	mBornText->setValue("");
	mAccountInfoText->setValue("");

	childSetTextArg("partner_edit", "[FIRST]", LLStringUtil::null);
	childSetTextArg("partner_edit", "[LAST]", LLStringUtil::null);

	mPartnerID.setNull();

	if (mGroupsListCtrl)
	{
		mGroupsListCtrl->deleteAllItems();
	}
}

void LLPanelAvatarSecondLife::enableControls(bool own_avatar)
{
	m2ndLifePicture->setEnabled(own_avatar);
	mAbout2ndLifeText->setEnabled(own_avatar);
	mShowInSearchCheck->setVisible(own_avatar);
	mShowInSearchCheck->setEnabled(own_avatar);
	mShowInSearchHelpButton->setVisible(own_avatar);
	mShowInSearchHelpButton->setEnabled(own_avatar);
}

//static
void LLPanelAvatarSecondLife::onDoubleClickGroup(void* data)
{
	LLPanelAvatarSecondLife* self = (LLPanelAvatarSecondLife*)data;
	if (!self) return;

	LLScrollListCtrl* group_list = self->mGroupsListCtrl;
	if (group_list)
	{
		LLScrollListItem* item = group_list->getFirstSelected();

		if (item && item->getUUID().notNull())
		{
			llinfos << "Show group info " << item->getUUID() << llendl;

			LLFloaterGroupInfo::showFromUUID(item->getUUID());
		}
	}
}

//static
void LLPanelAvatarSecondLife::onClickShowInSearchHelp(void*)
{
	gNotifications.add("ClickPublishHelpAvatar");
}

//static
void LLPanelAvatarSecondLife::onClickPartnerHelp(void*)
{
	gNotifications.add("ClickPartnerHelpAvatar", LLSD(), LLSD(),
					   onClickPartnerHelpLoadURL);
}

//static
bool LLPanelAvatarSecondLife::onClickPartnerHelpLoadURL(const LLSD& notification,
														const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLWeb::loadURL("http://secondlife.com/partner");
	}
	return false;
}

//static
void LLPanelAvatarSecondLife::onClickPartnerInfo(void *data)
{
	LLPanelAvatarSecondLife* self = (LLPanelAvatarSecondLife*) data;
	if (self && self->mPartnerID.notNull())
	{
		LLFloaterAvatarInfo::showFromProfile(self->mPartnerID,
											 self->getScreenRect());
	}
}

//-----------------------------------------------------------------------------
// LLPanelAvatarFirstLife class
//-----------------------------------------------------------------------------

LLPanelAvatarFirstLife::LLPanelAvatarFirstLife(const std::string& name,
											   const LLRect& rect,
											   LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar)
{
}

bool LLPanelAvatarFirstLife::postBuild()
{
	m1stLifePicture = getChild<LLTextureCtrl>("img");
	m1stLifePicture->setFallbackImageName("default_profile_picture.j2c");

	mAbout1stLifeText = getChild<LLTextEditor>("about");

	enableControls(getPanelAvatar()->getAvatarID() == gAgentID);

	return true;
}

void LLPanelAvatarFirstLife::enableControls(bool own_avatar)
{
	m1stLifePicture->setEnabled(own_avatar);
	mAbout1stLifeText->setEnabled(own_avatar);
}

//-----------------------------------------------------------------------------
// LLPanelAvatarWeb class
//-----------------------------------------------------------------------------

LLPanelAvatarWeb::LLPanelAvatarWeb(const std::string& name,
								   const LLRect& rect,
								   LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar)
{
}

bool LLPanelAvatarWeb::postBuild()
{
	childSetKeystrokeCallback("url_edit", onURLKeystroke, this);
	childSetCommitCallback("load", onCommitLoad, this);

	mWebProfileBtn = getChild<LLFlyoutButton>("sl_web_profile");
	mWebProfileBtn->setCommitCallback(onCommitSLWebProfile);
	mWebProfileBtn->setCallbackUserData(this);
	bool enabled = !LLFloaterAvatarInfo::getProfileURL("").empty();
	mWebProfileBtn->setEnabled(enabled);

	childSetAction("web_profile_help", onClickWebProfileHelp, this);

	childSetCommitCallback("url_edit", onCommitURL, this);

	childSetControlName("auto_load", "AutoLoadWebProfiles");

	mWebBrowser = getChild<LLMediaCtrl>("profile_html");
	mWebBrowser->addObserver(this);

	return true;
}

void LLPanelAvatarWeb::refresh()
{
	if (!mNavigateTo.empty())
	{
		llinfos << "Loading " << mNavigateTo << llendl;
		mWebBrowser->navigateTo(mNavigateTo);
		mNavigateTo.clear();
	}
	std::string user_name = getPanelAvatar()->getAvatarUserName();
	bool enabled = !LLFloaterAvatarInfo::getProfileURL(user_name).empty();
	mWebProfileBtn->setEnabled(enabled);
}

void LLPanelAvatarWeb::enableControls(bool own_avatar)
{
	mCanEditURL = own_avatar;
	childSetEnabled("url_edit", own_avatar);
}

void LLPanelAvatarWeb::setWebURL(std::string url)
{
	bool changed_url = mHome != url;

	mHome = url;
	bool have_url = !mHome.empty();

	childSetText("url_edit", mHome);
	childSetEnabled("load", have_url);

	if (have_url && gSavedSettings.getBool("AutoLoadWebProfiles"))
	{
		if (changed_url)
		{
			load(mHome);
		}
	}
	else
	{
		childSetVisible("profile_html", false);
		childSetVisible("status_text", false);
	}
}

//static
void LLPanelAvatarWeb::onCommitURL(LLUICtrl* ctrl, void* data)
{
	LLPanelAvatarWeb* self = (LLPanelAvatarWeb*)data;
	if (self)
	{
		self->mHome = self->childGetText("url_edit");
		self->load(self->childGetText("url_edit"));
	}
}

//static
void LLPanelAvatarWeb::onClickWebProfileHelp(void*)
{
	gNotifications.add("ClickWebProfileHelpAvatar");
}

void LLPanelAvatarWeb::load(const std::string& url)
{
	bool have_url = !url.empty();

	childSetVisible("profile_html", have_url);
	childSetVisible("status_text", have_url);
	childSetText("status_text", LLStringUtil::null);

	if (have_url)
	{
		if (mCanEditURL)
		{
			childSetEnabled("url_edit", false);
		}
		childSetText("url_edit", LLPanelAvatar::sLoading);

		mNavigateTo = url;
	}
}

//static
void LLPanelAvatarWeb::onURLKeystroke(LLLineEditor* editor, void* data)
{
	LLPanelAvatarWeb* self = (LLPanelAvatarWeb*)data;
	if (self && editor)
	{
		const std::string& url = editor->getText();
		self->childSetEnabled("load", !url.empty());
	}
}

//static
void LLPanelAvatarWeb::onCommitLoad(LLUICtrl* ctrl, void* data)
{
	LLPanelAvatarWeb* self = (LLPanelAvatarWeb*)data;
	if (!self || !ctrl) return;

	std::string valstr = ctrl->getValue().asString();
	std::string urlstr = self->childGetText("url_edit");
	if (valstr == "builtin")
	{
		 // Open in built-in browser
		if (!self->mHome.empty())
		{
			LLFloaterMediaBrowser::showInstance(urlstr);
		}
	}
	else if (valstr == "open")
	{
		 // Open in user's external browser
		if (!urlstr.empty())
		{
			LLWeb::loadURLExternal(urlstr);
		}
	}
	else if (valstr == "home")
	{
		 // Reload profile owner's home page
		if (!self->mHome.empty())
		{
			self->mWebBrowser->setTrusted(false);
			self->load(self->mHome);
		}
	}
	else if (!urlstr.empty())
	{
		// Load url string into browser panel
		self->mWebBrowser->setTrusted(false);
		self->load(urlstr);
	}
}

//static
void LLPanelAvatarWeb::onCommitSLWebProfile(LLUICtrl* ctrl, void * data)
{
	LLPanelAvatarWeb* self = (LLPanelAvatarWeb*)data;
	if (!self || !ctrl) return;

	std::string user_name = self->getPanelAvatar()->getAvatarUserName();
	if (user_name.empty()) return;

	std::string urlstr = LLFloaterAvatarInfo::getProfileURL(user_name);
	if (urlstr.empty()) return;

	std::string valstr = ctrl->getValue().asString();
	if (valstr == "sl_builtin")
	{
		 // Open in a trusted built-in browser
		LLFloaterMediaBrowser::showInstance(urlstr, true);
	}
	else if (valstr == "sl_open")
	{
		 // Open in user's external browser
		LLWeb::loadURLExternal(urlstr);
	}
	else
	{
		// Load SL's web-based avatar profile (trusted)
		self->mWebBrowser->setTrusted(true);
		self->load(urlstr);
	}
}

void LLPanelAvatarWeb::handleMediaEvent(LLPluginClassMedia* self,
										EMediaEvent event)
{
	switch (event)
	{
		case MEDIA_EVENT_STATUS_TEXT_CHANGED:
			childSetText("status_text", self->getStatusText());
			break;
		case MEDIA_EVENT_LOCATION_CHANGED:
			childSetText("url_edit", self->getLocation());
			enableControls(mCanEditURL);
			break;
		default:
			// Having a default case makes the compiler happy.
			break;
	}
}

//-----------------------------------------------------------------------------
// LLPanelAvatarAdvanced class
//-----------------------------------------------------------------------------

LLPanelAvatarAdvanced::LLPanelAvatarAdvanced(const std::string& name,
											 const LLRect& rect,
											 LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar),
	mWantToCount(0),
	mSkillsCount(0),
	mWantToEdit(NULL),
	mSkillsEdit(NULL)
{
}

bool LLPanelAvatarAdvanced::postBuild()
{
	const size_t want_to_check_size = LL_ARRAY_SIZE(mWantToCheck);
	for (size_t i = 0; i < want_to_check_size; ++i)
	{
		mWantToCheck[i] = NULL;
	}

	const size_t skills_check_size = LL_ARRAY_SIZE(mSkillsCheck);
	for (size_t i = 0; i < skills_check_size; ++i)
	{
		mSkillsCheck[i] = NULL;
	}

	mWantToCount = want_to_check_size < 8 ? want_to_check_size : 8;
	for (S32 i = 0; i < mWantToCount; ++i)
	{
		std::string ctlname = llformat("chk%d", i);
		mWantToCheck[i] = getChild<LLCheckBoxCtrl>(ctlname.c_str());
	}

	mSkillsCount = skills_check_size < 6 ? skills_check_size : 6;
	for (S32 i = 0; i < mSkillsCount; ++i)
	{
		// Find the Skills checkboxes and save off their controls
		std::string ctlname = llformat("schk%d", i);
		mSkillsCheck[i] = getChild<LLCheckBoxCtrl>(ctlname.c_str());
	}

	mWantToEdit = getChild<LLLineEditor>("want_to_edit");
	mSkillsEdit = getChild<LLLineEditor>("skills_edit");
	childSetVisible("skills_edit", LLPanelAvatar::sAllowFirstLife);
	childSetVisible("want_to_edit", LLPanelAvatar::sAllowFirstLife);

	return true;
}

void LLPanelAvatarAdvanced::enableControls(bool own_avatar)
{
	for (S32 i = 0; i < mWantToCount; ++i)
	{
		if (mWantToCheck[i])
		{
			mWantToCheck[i]->setEnabled(own_avatar);
		}
	}
	for (S32 i = 0; i < mSkillsCount; ++i)
	{
		if (mSkillsCheck[i])
		{
			mSkillsCheck[i]->setEnabled(own_avatar);
		}
	}

	if (mWantToEdit && mSkillsEdit)
	{
		mWantToEdit->setEnabled(own_avatar);
		mSkillsEdit->setEnabled(own_avatar);
	}
	childSetEnabled("languages_edit", own_avatar);
}

void LLPanelAvatarAdvanced::setWantSkills(U32 want_to_mask,
										  const std::string& want_to_text,
										  U32 skills_mask,
										  const std::string& skills_text,
										  const std::string& languages_text)
{
	for (S32 i = 0; i < mWantToCount; ++i)
	{
		mWantToCheck[i]->set(want_to_mask & (1 << i));
	}
	for (S32 i = 0; i < mSkillsCount; ++i)
	{
		mSkillsCheck[i]->set(skills_mask & (1 << i));
	}
	if (mWantToEdit && mSkillsEdit)
	{
		mWantToEdit->setText(want_to_text);
		mSkillsEdit->setText(skills_text);
	}

	childSetText("languages_edit", languages_text);
}

void LLPanelAvatarAdvanced::getWantSkills(U32* want_to_mask,
										  std::string& want_to_text,
										  U32* skills_mask,
										  std::string& skills_text,
										  std::string& languages_text)
{
	if (want_to_mask)
	{
		*want_to_mask = 0;
		for (S32 t = 0; t < mWantToCount; ++t)
		{
			if (mWantToCheck[t]->get())
			{
				*want_to_mask |= 1 << t;
			}
		}
	}
	if (skills_mask)
	{
		*skills_mask = 0;
		for (S32 t = 0; t < mSkillsCount; ++t)
		{
			if (mSkillsCheck[t]->get())
			{
				*skills_mask |= 1 << t;
			}
		}
	}
	if (mWantToEdit)
	{
		want_to_text = mWantToEdit->getText();
	}

	if (mSkillsEdit)
	{
		skills_text = mSkillsEdit->getText();
	}

	languages_text = childGetText("languages_edit");
}

//-----------------------------------------------------------------------------
// LLPanelAvatarNotes class
//-----------------------------------------------------------------------------

LLPanelAvatarNotes::LLPanelAvatarNotes(const std::string& name,
									   const LLRect& rect,
									   LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar)
{
}

bool LLPanelAvatarNotes::postBuild()
{
	LLTextEditor* editor = getChild<LLTextEditor>("notes edit");
	editor->setCommitCallback(onCommitNotes);
	editor->setCallbackUserData(this);
	editor->setCommitOnFocusLost(true);

	return true;
}

void LLPanelAvatarNotes::refresh()
{
	sendAvatarProfileRequestIfNeeded("avatarnotesrequest");
}

void LLPanelAvatarNotes::clearControls()
{
	LLTextEditor* editor = getChild<LLTextEditor>("notes edit");
	editor->setEnabled(false);
	editor->setValue(LLSD(LLPanelAvatar::sLoading));
}

//static
void LLPanelAvatarNotes::onCommitNotes(LLUICtrl*, void* userdata)
{
	LLPanelAvatarNotes* self = (LLPanelAvatarNotes*)userdata;
	if (self)
	{
		self->getPanelAvatar()->sendAvatarNotesUpdate();
	}
}

//-----------------------------------------------------------------------------
// LLPanelAvatarClassified class
//-----------------------------------------------------------------------------

LLPanelAvatarClassified::LLPanelAvatarClassified(const std::string& name,
												 const LLRect& rect,
												 LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar),
	mClassifiedTab(NULL),
	mButtonNew(NULL),
	mButtonDelete(NULL),
	mLoadingText(NULL)
{
}

bool LLPanelAvatarClassified::postBuild()
{
	mClassifiedTab = getChild<LLTabContainer>("classified tab");

	mButtonNew = getChild<LLButton>("New...");
	mButtonNew->setClickedCallback(onClickNew, this);

	mButtonDelete = getChild<LLButton>("Delete...");
	mButtonDelete->setClickedCallback(onClickDelete, this);

	mLoadingText = getChild<LLTextBox>("loading_text");

	return true;
}

void LLPanelAvatarClassified::refresh()
{
	bool self = getPanelAvatar()->getAvatarID() == gAgentID;

	S32 tab_count = mClassifiedTab ? mClassifiedTab->getTabCount() : 0;

	bool allow_new = tab_count < MAX_CLASSIFIEDS;
	bool allow_delete = tab_count > 0;
	bool show_help = tab_count == 0;

	// *HACK: Do not allow making new classifieds from inside the directory.
	// The logic for save/don't save when closing is too hairy, and the
	// directory is conceptually read-only. JC
	bool in_directory = false;
	LLView* view = this;
	while (view)
	{
		if (view->getName() == "directory")
		{
			in_directory = true;
			break;
		}
		view = view->getParent();
	}
	if (mButtonNew)
	{
		mButtonNew->setEnabled(self && !in_directory && allow_new);
		mButtonNew->setVisible(!in_directory);
	}
	if (mButtonDelete)
	{
		mButtonDelete->setEnabled(self && !in_directory && allow_delete);
		mButtonDelete->setVisible(!in_directory);
	}
	if (mClassifiedTab)
	{
		mClassifiedTab->setVisible(!show_help);
	}

	sendAvatarProfileRequestIfNeeded("avatarclassifiedsrequest");
}

bool LLPanelAvatarClassified::canClose()
{
	if (!mClassifiedTab) return true;

	for (S32 i = 0; i < mClassifiedTab->getTabCount(); ++i)
	{
		LLPanelClassified* panel =
			(LLPanelClassified*)mClassifiedTab->getPanelByIndex(i);
		if (panel && !panel->canClose())
		{
			return false;
		}
	}
	return true;
}

bool LLPanelAvatarClassified::titleIsValid()
{
	if (mClassifiedTab)
	{
		LLPanelClassified* panel =
			(LLPanelClassified*)mClassifiedTab->getCurrentPanel();
		if (panel && !panel->titleIsValid())
		{
			return false;
		}
	}

	return true;
}

void LLPanelAvatarClassified::apply()
{
	if (!mClassifiedTab) return;

	for (S32 i = 0; i < mClassifiedTab->getTabCount(); ++i)
	{
		LLPanelClassified* panel =
			(LLPanelClassified*)mClassifiedTab->getPanelByIndex(i);
		if (panel)
		{
			panel->apply();
		}
	}
}

void LLPanelAvatarClassified::deleteClassifiedPanels()
{
	if (mClassifiedTab)
	{
		mClassifiedTab->deleteAllTabs();
	}
	if (mButtonNew)
	{
		mButtonNew->setVisible(false);
	}
	if (mButtonDelete)
	{
		mButtonDelete->setVisible(false);
	}
	if (mLoadingText)
	{
		mLoadingText->setVisible(true);
	}
}

void LLPanelAvatarClassified::processAvatarClassifiedReply(LLMessageSystem* msg,
														   void**)
{
	// Note: we do not remove old panels. We need to be able to process
	// multiple packets for people who have lots of classifieds. JC

	LLUUID id;
	std::string classified_name;
	S32 block_count = msg->getNumberOfBlocksFast(_PREHASH_Data);
	for (S32 i = 0; i < block_count; ++i)
	{
		msg->getUUIDFast(_PREHASH_Data, _PREHASH_ClassifiedID, id, i);
		msg->getStringFast(_PREHASH_Data, _PREHASH_Name, classified_name, i);

		LLPanelClassified* panel_classified = new LLPanelClassified(false,
																	false);
		panel_classified->setClassifiedID(id);

		// This will request data from the server when the pick is first drawn
		panel_classified->markForServerRequest();

		// The button should automatically truncate long names for us
		if (mClassifiedTab)
		{
			mClassifiedTab->addTabPanel(panel_classified, classified_name);
		}
	}

	// Make sure somebody is highlighted. This works even if there are no tabs
	// in the container.
	if (mClassifiedTab)
	{
		mClassifiedTab->selectFirstTab();
	}

	if (mButtonNew)
	{
		mButtonNew->setVisible(true);
	}
	if (mButtonDelete)
	{
		mButtonDelete->setVisible(true);
	}
	if (mLoadingText)
	{
		mLoadingText->setVisible(false);
	}
}

// Create a new classified panel. It will automatically handle generating its
// own id when it is time to save.
//static
void LLPanelAvatarClassified::onClickNew(void* data)
{
	LLPanelAvatarClassified* self = (LLPanelAvatarClassified*)data;
	if (!self) return;

//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		return;
	}
//mk

	gNotifications.add("AddClassified", LLSD(), LLSD(),
					   boost::bind(&LLPanelAvatarClassified::callbackNew, self,
								   _1, _2));
}

bool LLPanelAvatarClassified::callbackNew(const LLSD& notification,
										  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLPanelClassified* panel_classified = new LLPanelClassified(false,
																	false);
		panel_classified->initNewClassified();
		if (mClassifiedTab)
		{
			mClassifiedTab->addTabPanel(panel_classified,
										panel_classified->getClassifiedName());
			mClassifiedTab->selectLastTab();
		}
	}

	return false;
}

//static
void LLPanelAvatarClassified::onClickDelete(void* data)
{
	LLPanelAvatarClassified* self = (LLPanelAvatarClassified*)data;
	if (!self) return;

	LLPanelClassified* panel_classified = NULL;
	if (self->mClassifiedTab)
	{
		panel_classified = (LLPanelClassified*)self->mClassifiedTab->getCurrentPanel();
	}
	if (!panel_classified) return;

	LLSD args;
	args["NAME"] = panel_classified->getClassifiedName();
	gNotifications.add("DeleteClassified", args, LLSD(),
					   boost::bind(&LLPanelAvatarClassified::callbackDelete,
								   self, _1, _2));
}

bool LLPanelAvatarClassified::callbackDelete(const LLSD& notification,
											 const LLSD& response)
{
	LLPanelClassified* panel_classified = NULL;
	if (mClassifiedTab)
	{
		panel_classified = (LLPanelClassified*)mClassifiedTab->getCurrentPanel();
	}
	if (!panel_classified) return false;

	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_ClassifiedDelete);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_Data);
		msg->addUUIDFast(_PREHASH_ClassifiedID,
						 panel_classified->getClassifiedID());
		gAgent.sendReliableMessage();

		if (mClassifiedTab)
		{
			mClassifiedTab->removeTabPanel(panel_classified);
		}

		delete panel_classified;
	}

	return false;
}

//-----------------------------------------------------------------------------
// LLPanelAvatarPicks class
//-----------------------------------------------------------------------------

LLPanelAvatarPicks::LLPanelAvatarPicks(const std::string& name,
									   const LLRect& rect,
									   LLPanelAvatar* panel_avatar)
:	LLPanelAvatarTab(name, rect, panel_avatar),
	mPicksTab(NULL),
	mButtonNew(NULL),
	mButtonDelete(NULL),
	mLoadingText(NULL)
{
}

bool LLPanelAvatarPicks::postBuild()
{
	mPicksTab = getChild<LLTabContainer>("picks tab");

	mButtonNew = getChild<LLButton>("New...");
	mButtonNew->setClickedCallback(onClickNew, this);

	mButtonDelete = getChild<LLButton>("Delete...");
	mButtonDelete->setClickedCallback(onClickDelete, this);

	mLoadingText = getChild<LLTextBox>("loading_text");

	return true;
}

void LLPanelAvatarPicks::refresh()
{
	bool is_self = getPanelAvatar()->getAvatarID() == gAgentID;
	bool editable = getPanelAvatar()->isEditable();
	S32 tab_count = mPicksTab ? mPicksTab->getTabCount() : 0;
	if (mButtonNew)
	{
		S32 max_picks = LLEconomy::getInstance()->getPicksLimit();
		mButtonNew->setEnabled(is_self && tab_count < max_picks);
		mButtonNew->setVisible(is_self && editable);
	}
	if (mButtonDelete)
	{
		mButtonDelete->setEnabled(is_self && tab_count > 0);
		mButtonDelete->setVisible(is_self && editable);
	}

	sendAvatarProfileRequestIfNeeded("avatarpicksrequest");
}

void LLPanelAvatarPicks::deletePickPanels()
{
	if (mPicksTab)
	{
		mPicksTab->deleteAllTabs();
	}
	if (mButtonNew)
	{
		mButtonNew->setVisible(false);
	}
	if (mButtonDelete)
	{
		mButtonDelete->setVisible(false);
	}
	if (mLoadingText)
	{
		mLoadingText->setVisible(true);
	}
}

void LLPanelAvatarPicks::processAvatarPicksReply(LLMessageSystem* msg, void**)
{
	// Clear out all the old panels. We will replace them with the correct
	// number of new panels.
	deletePickPanels();

	// The database needs to know for which user to look up picks.
	LLUUID avatar_id = getPanelAvatar()->getAvatarID();

	LLUUID pick_id;
	std::string pick_name;
	S32 block_count = msg->getNumberOfBlocks("Data");
	for (S32 block = 0; block < block_count; ++block)
	{
		msg->getUUID("Data", "PickID", pick_id, block);
		msg->getString("Data", "PickName", pick_name, block);

		LLPanelPick* panel_pick = new LLPanelPick(false);
		panel_pick->setPickID(pick_id, avatar_id);

		// This will request data from the server when the pick is first drawn
		panel_pick->markForServerRequest();

		// The button should automatically truncate long names for us
		if (mPicksTab)
		{
			mPicksTab->addTabPanel(panel_pick, pick_name);
		}
	}

	// Make sure somebody is highlighted. This works even if there are no tabs
	// in the container.
	if (mPicksTab)
	{
		mPicksTab->selectFirstTab();
	}

	if (mButtonNew)
	{
		mButtonNew->setVisible(true);
	}
	if (mButtonDelete)
	{
		mButtonDelete->setVisible(true);
	}
	if (mLoadingText)
	{
		mLoadingText->setVisible(false);
	}
}

// Create a new pick panel. It will automatically handle generating its own id
// when it is time to save.
//static
void LLPanelAvatarPicks::onClickNew(void* data)
{
	LLPanelAvatarPicks* self = (LLPanelAvatarPicks*)data;
	if (!self) return;

//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		return;
	}
//mk

	LLPanelPick* panel_pick = new LLPanelPick(false);
	panel_pick->initNewPick();

	if (self->mPicksTab)
	{
		self->mPicksTab->addTabPanel(panel_pick, panel_pick->getPickName());
		self->mPicksTab->selectLastTab();
	}
}

//static
void LLPanelAvatarPicks::onClickDelete(void* data)
{
	LLPanelAvatarPicks* self = (LLPanelAvatarPicks*)data;
	if (!self) return;

	LLPanelPick* panel_pick = NULL;
	if (self->mPicksTab)
	{
		panel_pick = (LLPanelPick*)self->mPicksTab->getCurrentPanel();
	}
	if (!panel_pick) return;

	LLSD args;
	args["PICK"] = panel_pick->getPickName();

	gNotifications.add("DeleteAvatarPick", args, LLSD(),
					   boost::bind(&LLPanelAvatarPicks::callbackDelete, self,
								   _1, _2));
}

//static
bool LLPanelAvatarPicks::callbackDelete(const LLSD& notification,
										const LLSD& response)
{
	LLPanelPick* panel_pick = NULL;
	if (mPicksTab)
	{
		panel_pick = (LLPanelPick*)mPicksTab->getCurrentPanel();
	}
	if (!panel_pick) return false;

	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLMessageSystem* msg = gMessageSystemp;
		// If the viewer has a hacked god-mode, then this call will fail
		if (gAgent.isGodlike())
		{
			msg->newMessage("PickGodDelete");
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID", gAgentID);
			msg->addUUID("SessionID", gAgentSessionID);
			msg->nextBlock("Data");
			msg->addUUID("PickID", panel_pick->getPickID());
			// *HACK: we need to send the pick's creator id to accomplish the
			// delete, and we do not use the query id for anything. JC
			msg->addUUID("QueryID", panel_pick->getPickCreatorID());
		}
		else
		{
			msg->newMessage("PickDelete");
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID", gAgentID);
			msg->addUUID("SessionID", gAgentSessionID);
			msg->nextBlock("Data");
			msg->addUUID("PickID", panel_pick->getPickID());
		}
		gAgent.sendReliableMessage();

		if (mPicksTab)
		{
			mPicksTab->removeTabPanel(panel_pick);
		}

		delete panel_pick;
	}

	return false;
}

//-----------------------------------------------------------------------------
// LLPanelAvatar class
//-----------------------------------------------------------------------------

LLPanelAvatar::LLPanelAvatar(const std::string& name, const LLRect& rect,
							 bool allow_edit)
:	LLPanel(name, rect, false),
	mPanelSecondLife(NULL),
	mPanelAdvanced(NULL),
	mPanelClassified(NULL),
	mPanelPicks(NULL),
	mPanelNotes(NULL),
	mPanelFirstLife(NULL),
	mPanelWeb(NULL),
	mDropTarget(NULL),
	mHaveProperties(false),
	mHaveNotes(false),
	mAllowEdit(allow_edit)
{
	sInstances.push_back(this);

	LLCallbackMap::map_t factory_map;

	factory_map["2nd Life"] = LLCallbackMap(createPanelAvatarSecondLife, this);
	factory_map["WebProfile"] = LLCallbackMap(createPanelAvatarWeb, this);
	factory_map["Interests"] = LLCallbackMap(createPanelAvatarInterests, this);
	factory_map["Picks"] = LLCallbackMap(createPanelAvatarPicks, this);
	factory_map["Classified"] = LLCallbackMap(createPanelAvatarClassified, this);
	factory_map["1st Life"] = LLCallbackMap(createPanelAvatarFirstLife, this);
	factory_map["My Notes"] = LLCallbackMap(createPanelAvatarNotes, this);

	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_avatar.xml",
											   &factory_map);

	selectTab(0);
}

bool LLPanelAvatar::postBuild()
{
	if (sLoading.empty())
	{
		sLoading = getString("loading");
		sClickToEnlarge = getString("click_to_enlarge");
		sShowOnMapNonFriend = getString("ShowOnMapNonFriend");
		sShowOnMapFriendOffline = getString("ShowOnMapFriendOffline");
		sShowOnMapFriendOnline = getString("ShowOnMapFriendOnline");
		sTeleportGod = getString("TeleportGod");
		sTeleportPrelude = getString("TeleportPrelude");
		sTeleportNormal = getString("TeleportNormal");
	}

	mTab = getChild<LLTabContainer>("tab");

	mOKButton = getChild<LLButton>("OK");
	mOKButton->setClickedCallback(onClickOK, this);

	mCancelButton = getChild<LLButton>("Cancel");
	mCancelButton->setClickedCallback(onClickCancel, this);

	mKickButton = getChild<LLButton>("kick_btn");
	mKickButton->setClickedCallback(onClickKick, this);
	mKickButton->setVisible(false);
	mKickButton->setEnabled(false);

	mFreezeButton = getChild<LLButton>("freeze_btn");
	mFreezeButton->setClickedCallback(onClickFreeze, this);
	mFreezeButton->setVisible(false);
	mFreezeButton->setEnabled(false);

	mUnfreezeButton = getChild<LLButton>("unfreeze_btn");
	mUnfreezeButton->setClickedCallback(onClickUnfreeze, this);
	mUnfreezeButton->setVisible(false);
	mUnfreezeButton->setEnabled(false);

	mCSRButton = getChild<LLButton>("csr_btn");
	mCSRButton->setClickedCallback(onClickCSR, this);
	mCSRButton->setVisible(false);
	mCSRButton->setEnabled(false);

	if (!sAllowFirstLife)
	{
		LLPanel* panel = mTab->getPanelByName("1st Life");
		if (panel)
		{
			mTab->removeTabPanel(panel);
		}

		panel = mTab->getPanelByName("WebProfile");
		if (panel)
		{
			mTab->removeTabPanel(panel);
		}
	}

	return true;
}

LLPanelAvatar::~LLPanelAvatar()
{
	sInstances.remove(this);
}

bool LLPanelAvatar::canClose()
{
	return mPanelClassified && mPanelClassified->canClose();
}

void LLPanelAvatar::setOnlineStatus(EOnlineStatus online_status)
{
	if (!mPanelSecondLife) return;

	bool online = online_status == ONLINE_STATUS_YES;
	// Online status NO could be because they are hidden. If they are a friend,
	// we may know the truth !
	if (!online && mIsFriend && gAvatarTracker.isBuddyOnline(mAvatarID))
	{
		online = true;
	}

	mPanelSecondLife->mOnlineText->setVisible(online);

	// Since setOnlineStatus gets called after setAvatarID, we need to make
	// sure that "Offer Teleport" does not get set to true again for yourself
	if (mAvatarID != gAgentID)
	{
		mPanelSecondLife->mOfferTPButton->setVisible(true);
		mPanelSecondLife->mRequestTPButton->setVisible(true);
	}

	if (gAgent.isGodlike())
	{
		mPanelSecondLife->mOfferTPButton->setEnabled(true);
		mPanelSecondLife->mOfferTPButton->setToolTip(sTeleportGod);
	}
	else if (gAgent.inPrelude())
	{
		mPanelSecondLife->mOfferTPButton->setEnabled(false);
		mPanelSecondLife->mOfferTPButton->setToolTip(sTeleportPrelude);
	}
	else
	{
		mPanelSecondLife->mOfferTPButton->setEnabled(true);
		mPanelSecondLife->mOfferTPButton->setToolTip(sTeleportNormal);
	}

	if (!mIsFriend)
	{
		mPanelSecondLife->mFindOnMapButton->setToolTip(sShowOnMapNonFriend);
	}
	else if (!online)
	{
		mPanelSecondLife->mFindOnMapButton->setToolTip(sShowOnMapFriendOffline);
	}
	else
	{
		mPanelSecondLife->mFindOnMapButton->setToolTip(sShowOnMapFriendOnline);
	}
}

void LLPanelAvatar::setAvatarID(const LLUUID& avatar_id,
								const std::string& name,
								EOnlineStatus online_status)
{
	if (avatar_id.isNull()) return;

	mAvatarID = avatar_id;

	// Determine if they are a friend
	mIsFriend = LLAvatarTracker::isAgentFriend(mAvatarID);

	// setOnlineStatus() uses mIsFriend
	setOnlineStatus(online_status);

	bool own_avatar = mAvatarID == gAgentID;

	mPanelSecondLife->enableControls(own_avatar && mAllowEdit);
	mPanelWeb->enableControls(own_avatar && mAllowEdit);
	mPanelAdvanced->enableControls(own_avatar && mAllowEdit);

	// Teens do not have this.
	if (sAllowFirstLife)
	{
		mPanelFirstLife->enableControls(own_avatar && mAllowEdit);
	}

	LLView* target_view = getChild<LLView>("drop_target_rect");
	if (target_view)
	{
		if (mDropTarget)
		{
			delete mDropTarget;
		}
		mDropTarget = new LLDropTarget("drop target", target_view->getRect(),
									   mAvatarID);
		addChild(mDropTarget);
		mDropTarget->setAgentID(mAvatarID);
	}

	LLNameEditor* name_edit = getChild<LLNameEditor>("name");
	std::string avname = name;
	if (name_edit)
	{
		if (name.empty())
		{
			name_edit->setNameID(avatar_id, false);
		}
		else
		{
			name_edit->setText(avname);
		}
		childSetVisible("name", true);
	}
	LLNameEditor* complete_name_edit = getChild<LLNameEditor>("complete_name");
	if (complete_name_edit)
	{
		if (LLAvatarNameCache::useDisplayNames())
		{
			LLAvatarName avatar_name;
			if (LLAvatarNameCache::get(avatar_id, &avatar_name))
			{
				// Always show "Display Name [Legacy Name]" for security reasons
				avname = avatar_name.getNames();
				mAvatarUserName = avatar_name.mUsername;
			}
			else
			{
				avname = name_edit->getText();
				LLAvatarNameCache::get(avatar_id,
									   boost::bind(&LLPanelAvatar::completeNameCallback,
												   _1, _2, this));
			}
			complete_name_edit->setText(avname);
			childSetVisible("name", false);
			childSetVisible("complete_name", true);
		}
		else
		{
			childSetVisible("complete_name", false);
		}
	}
	// We can't set a tooltip on a text input box ("name" or "complete_name"),
	// so we set it on the profile picture... This can be helpful with very
	// long names (which would *appear* truncated in the textbox; even if it's
	// still possible to pan through the name with the mouse).
	LLTextureCtrl* image_ctrl = mPanelSecondLife->m2ndLifePicture;
	if (image_ctrl)
	{
		std::string tooltip = avname;
		if (!own_avatar)
		{
			tooltip += "\n" + sClickToEnlarge;
		}
		image_ctrl->setToolTip(tooltip);
	}

#if 0
	bool avatar_changed = avatar_id != mAvatarID;
 	if (avatar_changed)
#endif
	{
		// While we're waiting for data off the network, clear out the old data
		mPanelSecondLife->clearControls();

		mPanelPicks->deletePickPanels();
		mPanelPicks->setDataRequested(false);

		mPanelClassified->deleteClassifiedPanels();
		mPanelClassified->setDataRequested(false);

		mHaveNotes = false;
		mLastNotes.clear();
		mPanelNotes->clearControls();
		mPanelNotes->setDataRequested(false);

		// Request just the first two pages of data. The picks, classifieds and
		// notes will be requested when that panel is made visible. JC
		sendAvatarPropertiesRequest();

		if (own_avatar)
		{
			if (mAllowEdit)
			{
				// OK button disabled until properties data arrives
				mOKButton->setVisible(true);
				mOKButton->setEnabled(false);
				mCancelButton->setVisible(true);
				mCancelButton->setEnabled(true);
			}
			else
			{
				mOKButton->setVisible(false);
				mOKButton->setEnabled(false);
				mCancelButton->setVisible(false);
				mCancelButton->setEnabled(false);
			}
			mPanelSecondLife->mFindOnMapButton->setVisible(false);
			mPanelSecondLife->mFindOnMapButton->setEnabled(false);
			mPanelSecondLife->mOfferTPButton->setVisible(false);
			mPanelSecondLife->mOfferTPButton->setEnabled(false);
			mPanelSecondLife->mRequestTPButton->setVisible(false);
			mPanelSecondLife->mRequestTPButton->setEnabled(false);
			mPanelSecondLife->mAddFriendButton->setVisible(false);
			mPanelSecondLife->mAddFriendButton->setEnabled(false);
			mPanelSecondLife->mPayButton->setVisible(false);
			mPanelSecondLife->mPayButton->setEnabled(false);
			mPanelSecondLife->mIMButton->setVisible(false);
			mPanelSecondLife->mIMButton->setEnabled(false);
			mPanelSecondLife->mMuteButton->setVisible(false);
			mPanelSecondLife->mMuteButton->setEnabled(false);
			childSetVisible("drop target", false);
			childSetEnabled("drop target", false);
		}
		else
		{
			mOKButton->setVisible(false);
			mOKButton->setEnabled(false);

			mCancelButton->setVisible(false);
			mCancelButton->setEnabled(false);

			mPanelSecondLife->mFindOnMapButton->setVisible(true);
			// Note: we don't always know online status, so always allow gods
			// to try to track
			bool can_map = LLAvatarTracker::isAgentMappable(mAvatarID);
			mPanelSecondLife->mFindOnMapButton->setEnabled(can_map ||
														   gAgent.isGodlike());
			if (!mIsFriend)
			{
				mPanelSecondLife->mFindOnMapButton->setToolTip(sShowOnMapNonFriend);
			}
			else if (ONLINE_STATUS_YES != online_status)
			{
				mPanelSecondLife->mFindOnMapButton->setToolTip(sShowOnMapFriendOffline);
			}
			else
			{
				mPanelSecondLife->mFindOnMapButton->setToolTip(sShowOnMapFriendOnline);
			}

			mPanelSecondLife->mAddFriendButton->setVisible(true);
			mPanelSecondLife->mAddFriendButton->setEnabled(!mIsFriend);

			mPanelSecondLife->mPayButton->setVisible(true);
			mPanelSecondLife->mPayButton->setEnabled(false);
			mPanelSecondLife->mIMButton->setVisible(true);
			mPanelSecondLife->mIMButton->setEnabled(false);
			mPanelSecondLife->mMuteButton->setVisible(true);
			mPanelSecondLife->mMuteButton->setEnabled(false);
			childSetVisible("drop target", true);
			childSetEnabled("drop target", false);
		}
		LLNameEditor* avatar_key = getChild<LLNameEditor>("avatar_key");
		if (avatar_key)
		{
			avatar_key->setText(avatar_id.asString());
		}
	}

	bool is_god = gAgent.isGodlike();

	mKickButton->setVisible(is_god);
	mKickButton->setEnabled(is_god);
	mFreezeButton->setVisible(is_god);
	mFreezeButton->setEnabled(is_god);
	mUnfreezeButton->setVisible(is_god);
	mUnfreezeButton->setEnabled(is_god);
	mCSRButton->setVisible(is_god);
	mCSRButton->setEnabled(is_god && gIsInSecondLife);
}

void LLPanelAvatar::completeNameCallback(const LLUUID& agent_id,
										 const LLAvatarName& avatar_name,
										 void* userdata)
{
	if (!LLAvatarNameCache::useDisplayNames())
	{
		return;
	}
	// look up all panels which have this avatar
	for (panel_list_t::iterator iter = sInstances.begin(); iter != sInstances.end(); ++iter)
	{
		LLPanelAvatar* self = *iter;
		if (self->mAvatarID == agent_id)
		{
			LLLineEditor* complete_name_edit = self->getChild<LLLineEditor>("complete_name");
			if (complete_name_edit)
			{
				self->mAvatarUserName = avatar_name.mUsername;
				// Always show "Display Name [Legacy Name]" for security reasons
				std::string avname = avatar_name.getNames();
				complete_name_edit->setText(avname);
				if (self->mPanelSecondLife)
				{
					LLTextureCtrl* image_ctrl = self->mPanelSecondLife->m2ndLifePicture;
					if (image_ctrl)
					{
						std::string tooltip = avname;
						if (agent_id != gAgentID)
						{
							tooltip += "\n" + sClickToEnlarge;
						}
						image_ctrl->setToolTip(tooltip);
					}
				}
			}
		}
	}
}

void LLPanelAvatar::resetGroupList()
{
	// only get these updates asynchronously via the group floater, which works
	// on the agent only
	if (mAvatarID != gAgentID)
	{
		return;
	}

	if (mPanelSecondLife)
	{
		LLScrollListCtrl* group_list = mPanelSecondLife->mGroupsListCtrl;
		if (group_list)
		{
			group_list->deleteAllItems();

			for (S32 i = 0, count = gAgent.mGroups.size(); i < count; ++i)
			{
				LLGroupData group_data = gAgent.mGroups[i];

				std::string group_string;
#if 0			// Show group title ?  DUMMY_POWER for Don Grep
				if (group_data.mOfficer)
				{
					group_string = "Officer of ";
				}
				else
				{
					group_string = "Member of ";
				}
#endif
				group_string += group_data.mName;

				LLSD row;
				row["id"] = group_data.mID;
				row["columns"][0]["value"] = group_string;
				row["columns"][0]["font"] = "SANSSERIF_SMALL";
				row["columns"][0]["width"] = 0;
				group_list->addElement(row);
			}
			group_list->sortByColumnIndex(0, true);
		}
	}
}

//static
void LLPanelAvatar::onClickIM(void* userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*) userdata;
	if (self)
	{
		LLAvatarActions::startIM(self->mAvatarID);
	}
}

//static
void LLPanelAvatar::onClickTrack(void* userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*) userdata;
	if (self && gFloaterWorldMapp)
	{
		std::string name;
		LLNameEditor* nameedit = self->mPanelSecondLife->getChild<LLNameEditor>("name");
		if (nameedit) name = nameedit->getText();
		gFloaterWorldMapp->trackAvatar(self->mAvatarID, name);
		LLFloaterWorldMap::show(NULL, true);
	}
}

//static
void LLPanelAvatar::onClickAddFriend(void* userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*) userdata;
	if (!self) return;

	LLNameEditor* name_edit = self->mPanelSecondLife->getChild<LLNameEditor>("name");
	if (name_edit)
	{
		LLAvatarActions::requestFriendshipDialog(self->getAvatarID(),
												 name_edit->getText());
	}
}

void LLPanelAvatar::onClickMute(void *userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*) userdata;
	if (!self) return;

	LLUUID agent_id = self->getAvatarID();
	LLNameEditor* name_edit = self->mPanelSecondLife->getChild<LLNameEditor>("name");
	if (name_edit)
	{
		std::string agent_name = name_edit->getText();

		if (LLMuteList::isMuted(agent_id))
		{
			LLFloaterMute::selectMute(agent_id);
		}
		else
		{
			LLMute mute(agent_id, agent_name, LLMute::AGENT);
			if (LLMuteList::add(mute))
			{
				LLFloaterMute::selectMute(mute.mID);
			}
		}
	}
}

//static
void LLPanelAvatar::onClickOfferTeleport(void *userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*)userdata;
	if (self)
	{
		LLAvatarActions::offerTeleport(self->mAvatarID);
	}
}

//static
void LLPanelAvatar::onClickRequestTeleport(void *userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*)userdata;
	if (self)
	{
		LLAvatarActions::teleportRequest(self->mAvatarID);
	}
}

//static
void LLPanelAvatar::onClickPay(void *userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*) userdata;
	if (self)
	{
		LLAvatarActions::pay(self->mAvatarID);
	}
}

//static
void LLPanelAvatar::onClickOK(void *userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*)userdata;

	// JC: Only save the data if we actually got the original properties.
	// Otherwise we might save blanks into the database.
	if (self && self->mHaveProperties)
	{
		self->sendAvatarPropertiesUpdate();

		LLTabContainer* tabs = self->getChild<LLTabContainer>("tab");
		if (tabs->getCurrentPanel() != self->mPanelClassified)
		{
			self->mPanelClassified->apply();

			LLFloaterAvatarInfo* infop = LLFloaterAvatarInfo::getInstance(self->mAvatarID);
			if (infop)
			{
				infop->close();
			}
		}
		else
		{
			if (self->mPanelClassified->titleIsValid())
			{
				self->mPanelClassified->apply();

				LLFloaterAvatarInfo* infop = LLFloaterAvatarInfo::getInstance(self->mAvatarID);
				if (infop)
				{
					infop->close();
				}
			}
		}
	}
}

//static
void LLPanelAvatar::onClickCancel(void *userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*)userdata;
	if (self)
	{
		LLFloaterAvatarInfo *infop;
		if ((infop = LLFloaterAvatarInfo::getInstance(self->mAvatarID)))
		{
			infop->close();
		}
		else
		{
			// We're in the Search directory and are cancelling an edit to our
			// own profile, so reset.
			self->sendAvatarPropertiesRequest();
		}
	}
}

void LLPanelAvatar::sendAvatarPropertiesRequest()
{
	//lldebugs << "LLPanelAvatar::sendAvatarPropertiesRequest()" << llendl;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_AvatarPropertiesRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_AvatarID, mAvatarID);
	gAgent.sendReliableMessage();
}

void LLPanelAvatar::sendAvatarNotesUpdate()
{
	std::string notes = mPanelNotes->childGetValue("notes edit").asString();

	if (!mHaveNotes || notes == mLastNotes || notes == sLoading)
	{
		// no notes from server and no user updates
		return;
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("AvatarNotesUpdate");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock("Data");
	msg->addUUID("TargetID", mAvatarID);
	msg->addString("Notes", notes);

	gAgent.sendReliableMessage();
}

//static
void LLPanelAvatar::processAvatarPropertiesReply(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;	// your id
	LLUUID avatar_id;	// target of this panel
	LLUUID image_id;
	LLUUID fl_image_id;
	LLUUID partner_id;
	std::string	about_text;
	std::string	fl_about_text;
	std::string	born_on;
	std::string	profile_url;
    bool allow_publish = false;
    bool identified = false;
    bool transacted = false;
    bool age_verified = false;
    bool online = false;
	S32 charter_member_size = 0;
	U32 flags = 0x0;

	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AvatarID, avatar_id);

	for (panel_list_t::iterator iter = sInstances.begin();
		 iter != sInstances.end(); ++iter)
	{
		LLPanelAvatar* self = *iter;
		if (self->mAvatarID != avatar_id)
		{
			continue;
		}
		if (self->mPanelSecondLife)
		{
			self->mPanelSecondLife->mIMButton->setEnabled(true);
			self->mPanelSecondLife->mPayButton->setEnabled(true);
			self->mPanelSecondLife->mMuteButton->setEnabled(true);

			self->childSetEnabled("drop target", true);
		}

		self->mHaveProperties = true;
		self->enableOKIfReady();

		msg->getUUIDFast(_PREHASH_PropertiesData, _PREHASH_ImageID, image_id);
		msg->getUUIDFast(_PREHASH_PropertiesData, _PREHASH_FLImageID, fl_image_id);
		msg->getUUIDFast(_PREHASH_PropertiesData, _PREHASH_PartnerID, partner_id);
		msg->getStringFast(_PREHASH_PropertiesData, _PREHASH_AboutText, about_text);
		msg->getStringFast(_PREHASH_PropertiesData, _PREHASH_FLAboutText, fl_about_text);
		msg->getStringFast(_PREHASH_PropertiesData, _PREHASH_BornOn, born_on);
		msg->getString("PropertiesData", "ProfileURL", profile_url);
		msg->getU32Fast(_PREHASH_PropertiesData, _PREHASH_Flags, flags);

		tm t;
		if (sscanf(born_on.c_str(), "%u/%u/%u",
				   &t.tm_mon, &t.tm_mday, &t.tm_year) == 3 && t.tm_year > 1900)
		{
			t.tm_year -= 1900;
			t.tm_mon--;
			t.tm_hour = t.tm_min = t.tm_sec = 0;
			timeStructToFormattedString(&t,
										gSavedSettings.getString("ShortDateFormat"),
										born_on);
		}

		identified = (flags & AVATAR_IDENTIFIED) != 0;
		transacted = (flags & AVATAR_TRANSACTED) != 0;
		allow_publish = (flags & AVATAR_ALLOW_PUBLISH) != 0;
		online = (flags & AVATAR_ONLINE) != 0;
		// Not currently reported by server for privacy considerations
		age_verified = (flags & AVATAR_AGEVERIFIED) != 0;

		U8 caption_index = 0;
		std::string caption_text;
		charter_member_size = msg->getSize("PropertiesData", "CharterMember");
		if (charter_member_size == 1)
		{
			msg->getBinaryData("PropertiesData", "CharterMember",
							   &caption_index, 1);
		}
		else if (charter_member_size > 1)
		{
			msg->getString("PropertiesData", "CharterMember", caption_text);
		}

		if (caption_text.empty())
		{
			LLStringUtil::format_map_t args;
			caption_text = self->mPanelSecondLife->getString("CaptionTextAcctInfo");

			static const char* ACCT_TYPE[] =
			{
				"AcctTypeResident",
				"AcctTypeTrial",
				"AcctTypeCharterMember",
				"AcctTypeEmployee"
			};
			constexpr S32 ACCT_TYPE_SIZE = LL_ARRAY_SIZE(ACCT_TYPE);
			caption_index = llclamp(caption_index,
									(U8)0, (U8)(ACCT_TYPE_SIZE - 1));
			args["[ACCTTYPE]"] =
				self->mPanelSecondLife->getString(ACCT_TYPE[caption_index]);

			std::string payment_text = " ";
			constexpr S32 DEFAULT_CAPTION_LINDEN_INDEX = 3;
			if (caption_index != DEFAULT_CAPTION_LINDEN_INDEX)
			{
				if (transacted)
				{
					payment_text = "PaymentInfoUsed";
				}
				else if (identified)
				{
					payment_text = "PaymentInfoOnFile";
				}
				else
				{
					payment_text = "NoPaymentInfoOnFile";
				}
				args["[PAYMENTINFO]"] =
					self->mPanelSecondLife->getString(payment_text);
				std::string age_text = age_verified ? "AgeVerified"
													: "NotAgeVerified";
#if 0			// Do not display age verification status at this time
				args["[[AGEVERIFICATION]]"] =
					self->mPanelSecondLife->getString(age_text);
#else
				args["[AGEVERIFICATION]"] = " ";
#endif
			}
			else
			{
				args["[PAYMENTINFO]"] = " ";
				args["[AGEVERIFICATION]"] = " ";
			}
			LLStringUtil::format(caption_text, args);
		}

		self->mPanelSecondLife->mAccountInfoText->setValue(caption_text);
		self->mPanelSecondLife->mBornText->setValue(born_on);

		EOnlineStatus online_status = online ? ONLINE_STATUS_YES
											 : ONLINE_STATUS_NO;

		self->setOnlineStatus(online_status);

		self->mPanelWeb->setWebURL(profile_url);

		LLTextureCtrl* image_ctrl = self->mPanelSecondLife->m2ndLifePicture;
		if (image_ctrl)
		{
			image_ctrl->setImageAssetID(image_id);
		}

		LLTextEditor* about_ctrl = self->mPanelSecondLife->mAbout2ndLifeText;
		about_ctrl->clear();
		about_ctrl->setParseHTML(true);
		if (self->mAvatarID == gAgentID)
		{
			about_ctrl->setText(about_text);
		}
		else
		{
			about_ctrl->appendColoredText(about_text, false, false,
										  about_ctrl->getReadOnlyFgColor());
		}

		self->mPanelSecondLife->setPartnerID(partner_id);
		self->mPanelSecondLife->updatePartnerName();

		if (sAllowFirstLife)	// Teens do not get this
		{
			about_ctrl = self->mPanelFirstLife->mAbout1stLifeText;
			about_ctrl->clear();
			about_ctrl->setParseHTML(true);
			if (self->mAvatarID == gAgentID)
			{
				about_ctrl->setText(fl_about_text);
			}
			else
			{
				about_ctrl->appendColoredText(fl_about_text, false, false,
											  about_ctrl->getReadOnlyFgColor());
			}

			LLTextureCtrl* image_ctrl = self->mPanelFirstLife->m1stLifePicture;
			if (image_ctrl)
			{
				image_ctrl->setImageAssetID(fl_image_id);
				if (avatar_id == gAgentID || fl_image_id.isNull())
				{
					image_ctrl->setToolTip(std::string(""));
				}
				else
				{
					image_ctrl->setToolTip(sClickToEnlarge);
				}
			}

			self->mPanelSecondLife->mShowInSearchCheck->setValue(allow_publish);
		}
	}
}

//static
void LLPanelAvatar::processAvatarInterestsReply(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;	// Our own agent Id
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID avatar_id;	// Target of this panel
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AvatarID, avatar_id);

	U32 want_to_mask, skills_mask;
	std::string	want_to_text, skills_text, languages_text;
	for (panel_list_t::iterator iter = sInstances.begin();
		 iter != sInstances.end(); ++iter)
	{
		LLPanelAvatar* self = *iter;
		if (self->mAvatarID == avatar_id)
		{
			msg->getU32Fast(_PREHASH_PropertiesData, _PREHASH_WantToMask,
							want_to_mask);
			msg->getStringFast(_PREHASH_PropertiesData, _PREHASH_WantToText,
							   want_to_text);
			msg->getU32Fast(_PREHASH_PropertiesData, _PREHASH_SkillsMask,
							skills_mask);
			msg->getStringFast(_PREHASH_PropertiesData, _PREHASH_SkillsText,
							   skills_text);
			msg->getString(_PREHASH_PropertiesData, "LanguagesText",
						   languages_text);

			self->mPanelAdvanced->setWantSkills(want_to_mask, want_to_text,
												skills_mask, skills_text,
												languages_text);
		}
	}
}

// Separate function because the groups list can be very long, almost
// filling a packet. JC
//static
void LLPanelAvatar::processAvatarGroupsReply(LLMessageSystem* msg, void**)
{
	LL_DEBUGS("GroupPanel") << "Groups packet size: " << msg->getReceiveSize()
							<< LL_ENDL;

	LLUUID agent_id;		// your id
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID avatar_id;		// target of this panel
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AvatarID, avatar_id);

	for (panel_list_t::iterator iter = sInstances.begin();
		 iter != sInstances.end(); ++iter)
	{
		LLPanelAvatar* self = *iter;
		if (self->mAvatarID != avatar_id || !self->mPanelSecondLife)
		{
			continue;
		}

		LLScrollListCtrl* group_list = self->mPanelSecondLife->mGroupsListCtrl;
		if (!group_list)
		{
			continue;
		}
#if 0
		group_list->deleteAllItems();
#endif

		S32 group_count = msg->getNumberOfBlocksFast(_PREHASH_GroupData);
		if (group_count == 0)
		{
			// *TODO: Translate
			group_list->addCommentText("None");
		}
		else
		{
			std::string	group_title, group_name, group_string;
			LLUUID group_id, group_insignia_id;
			U64 group_powers;
			for (S32 i = 0; i < group_count; ++i)
			{
				msg->getU64(_PREHASH_GroupData, "GroupPowers",
							group_powers, i);
				msg->getStringFast(_PREHASH_GroupData, _PREHASH_GroupTitle,
								   group_title, i);
				msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID,
								 group_id, i);
				msg->getStringFast(_PREHASH_GroupData, _PREHASH_GroupName,
								   group_name, i);
				msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupInsigniaID,
								 group_insignia_id, i);

				if (group_id.notNull())
				{
					group_string = group_name;
				}
				else
				{
					group_string.clear();
				}

				// Is this really necessary ?  Remove existing entry if it
				// exists. *TODO: clear the whole list when a request for data
				// is made.
				S32 index = group_list->getItemIndex(group_id);
				if (index >= 0)
				{
					group_list->deleteSingleItem(index);
				}

				LLSD row;
				row["id"] = group_id;
				row["columns"][0]["value"] = group_string;
				row["columns"][0]["font"] = "SANSSERIF_SMALL";
				group_list->addElement(row);
			}
		}
		group_list->sortByColumnIndex(0, true);
	}
}

// Do not enable the OK button until you actually have the data. Otherwise you
// will write blanks back into the database.
void LLPanelAvatar::enableOKIfReady()
{
	mOKButton->setEnabled(mHaveProperties && mOKButton->getVisible());
}

void LLPanelAvatar::sendAvatarPropertiesUpdate()
{
	llinfos << "Sending avatarinfo update" << llendl;

	bool allow_publish = sAllowFirstLife &&
						 mPanelSecondLife->mShowInSearchCheck->getValue();

	U32 want_to_mask = 0x0;
	U32 skills_mask = 0x0;
	std::string want_to_text, skills_text, languages_text;
	mPanelAdvanced->getWantSkills(&want_to_mask, want_to_text, &skills_mask,
								  skills_text, languages_text);

	LLUUID first_life_image_id;
	std::string first_life_about_text;
	if (sAllowFirstLife)
	{
		first_life_about_text =
			mPanelFirstLife->mAbout1stLifeText->getValue().asString();
		LLTextureCtrl* image_ctrl = mPanelFirstLife->m1stLifePicture;
		if (image_ctrl)
		{
			first_life_image_id = image_ctrl->getImageAssetID();
		}
	}

	std::string about_text =
		mPanelSecondLife->mAbout2ndLifeText->getValue().asString();

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_AvatarPropertiesUpdate);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_PropertiesData);

	LLTextureCtrl* image_ctrl = mPanelSecondLife->m2ndLifePicture;
	if (image_ctrl)
	{
		msg->addUUIDFast(_PREHASH_ImageID, image_ctrl->getImageAssetID());
	}
	else
	{
		msg->addUUIDFast(_PREHASH_ImageID, LLUUID::null);
	}
#if 0
	msg->addUUIDFast(_PREHASH_ImageID,
					 mPanelSecondLife->mimage_ctrl->getImageAssetID());
#endif
	msg->addUUIDFast(_PREHASH_FLImageID, first_life_image_id);
	msg->addStringFast(_PREHASH_AboutText, about_text);
	msg->addStringFast(_PREHASH_FLAboutText, first_life_about_text);

	msg->addBool("AllowPublish", allow_publish);
	msg->addBool("MaturePublish", false); // A profile should never be mature
	msg->addString("ProfileURL", mPanelWeb->getWebURL());
	gAgent.sendReliableMessage();

	msg->newMessage("AvatarInterestsUpdate");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_PropertiesData);
	msg->addU32Fast(_PREHASH_WantToMask, want_to_mask);
	msg->addStringFast(_PREHASH_WantToText, want_to_text);
	msg->addU32Fast(_PREHASH_SkillsMask, skills_mask);
	msg->addStringFast(_PREHASH_SkillsText, skills_text);
	msg->addString("LanguagesText", languages_text);
	gAgent.sendReliableMessage();
}

void LLPanelAvatar::selectTab(S32 tabnum)
{
	if (mTab)
	{
		mTab->selectTab(tabnum);
	}
}

void LLPanelAvatar::selectTabByName(std::string tab_name)
{
	if (mTab)
	{
		if (tab_name.empty())
		{
			mTab->selectFirstTab();
		}
		else
		{
			mTab->selectTabByName(tab_name);
		}
	}
}

void LLPanelAvatar::processAvatarNotesReply(LLMessageSystem* msg, void**)
{
	// Extract the agent id
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);

	LLUUID target_id;
	msg->getUUID("Data", "TargetID", target_id);

	// Look up all panels which have this avatar
	for (panel_list_t::iterator iter = sInstances.begin(),
								end = sInstances.end();
		 iter != end; ++iter)
	{
		LLPanelAvatar* self = *iter;
		if (self->mAvatarID != target_id)
		{
			continue;
		}

		std::string text;
		msg->getString("Data", "Notes", text);
		self->childSetValue("notes edit", text);
		self->childSetEnabled("notes edit", true);
		self->mHaveNotes = true;
		self->mLastNotes = text;
	}
}

void LLPanelAvatar::processAvatarClassifiedReply(LLMessageSystem* msg,
												 void** userdata)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID target_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_TargetID, target_id);

	// Look up all panels which have this avatar target
	for (panel_list_t::iterator iter = sInstances.begin(),
								end = sInstances.end();
		 iter != end; ++iter)
	{
		LLPanelAvatar* self = *iter;
		if (self->mAvatarID != target_id)
		{
			continue;
		}

		self->mPanelClassified->processAvatarClassifiedReply(msg, userdata);
	}
}

void LLPanelAvatar::processAvatarPicksReply(LLMessageSystem* msg,
											void** userdata)
{
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	LLUUID target_id;
	msg->getUUID("AgentData", "TargetID", target_id);

	// Look up all panels which have this avatar target
	for (panel_list_t::iterator iter = sInstances.begin(),
								end = sInstances.end();
		 iter != end; ++iter)
	{
		LLPanelAvatar* self = *iter;
		if (self->mAvatarID != target_id)
		{
			continue;
		}

		self->mPanelPicks->processAvatarPicksReply(msg, userdata);
	}
}

//static
void LLPanelAvatar::onClickKick(void* userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*)userdata;
	if (!self) return;

	S32 left, top;
	gFloaterViewp->getNewFloaterPosition(&left, &top);
	LLRect rect(left, top, left + 400, top - 300);

	LLSD payload;
	payload["avatar_id"] = self->mAvatarID;
	gNotifications.add("KickUser", LLSD(), payload, finishKick);
}

//static
bool LLPanelAvatar::finishKick(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLUUID avatar_id = notification["payload"]["avatar_id"].asUUID();
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_GodKickUser);
		msg->nextBlockFast(_PREHASH_UserInfo);
		msg->addUUIDFast(_PREHASH_GodID, gAgentID);
		msg->addUUIDFast(_PREHASH_GodSessionID, gAgentSessionID);
		msg->addUUIDFast(_PREHASH_AgentID, avatar_id);
		msg->addU32("KickFlags", KICK_FLAGS_DEFAULT);
		msg->addStringFast(_PREHASH_Reason, response["message"].asString());
		gAgent.sendReliableMessage();
	}

	return false;
}

//static
void LLPanelAvatar::onClickFreeze(void* userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*) userdata;
	if (self)
	{
		LLSD payload;
		payload["avatar_id"] = self->mAvatarID;
		gNotifications.add("FreezeUser", LLSD(), payload, finishFreeze);
	}
}

//static
bool LLPanelAvatar::finishFreeze(const LLSD& notification,
								 const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLUUID avatar_id = notification["payload"]["avatar_id"].asUUID();
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_GodKickUser);
		msg->nextBlockFast(_PREHASH_UserInfo);
		msg->addUUIDFast(_PREHASH_GodID, gAgentID);
		msg->addUUIDFast(_PREHASH_GodSessionID, gAgentSessionID);
		msg->addUUIDFast(_PREHASH_AgentID, avatar_id);
		msg->addU32("KickFlags", KICK_FLAGS_FREEZE);
		msg->addStringFast(_PREHASH_Reason, response["message"].asString());
		gAgent.sendReliableMessage();
	}

	return false;
}

//static
void LLPanelAvatar::onClickUnfreeze(void* userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*) userdata;
	if (self)
	{
		LLSD payload;
		payload["avatar_id"] = self->mAvatarID;
		gNotifications.add("UnFreezeUser", LLSD(), payload, finishUnfreeze);
	}
}

//static
bool LLPanelAvatar::finishUnfreeze(const LLSD& notification,
								   const LLSD& response)
{
	std::string text = response["message"].asString();
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLUUID avatar_id = notification["payload"]["avatar_id"].asUUID();
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_GodKickUser);
		msg->nextBlockFast(_PREHASH_UserInfo);
		msg->addUUIDFast(_PREHASH_GodID, gAgentID);
		msg->addUUIDFast(_PREHASH_GodSessionID, gAgentSessionID);
		msg->addUUIDFast(_PREHASH_AgentID, avatar_id);
		msg->addU32("KickFlags", KICK_FLAGS_UNFREEZE);
		msg->addStringFast(_PREHASH_Reason, text);
		gAgent.sendReliableMessage();
	}

	return false;
}

//static
void LLPanelAvatar::onClickCSR(void* userdata)
{
	LLPanelAvatar* self = (LLPanelAvatar*)userdata;
	if (!self) return;

	LLNameEditor* name_edit = self->getChild<LLNameEditor>("name");
	if (!name_edit) return;

	std::string name = name_edit->getText();
	if (name.empty()) return;

	std::string url = "http://csr.lindenlab.com/agent/";

	// Slow and stupid, but it is late
	S32 len = name.length();
	for (S32 i = 0; i < len; ++i)
	{
		if (name[i] == ' ')
		{
			url += "%20";
		}
		else
		{
			url += name[i];
		}
	}

	LLWeb::loadURL(url);
}

void* LLPanelAvatar::createPanelAvatarSecondLife(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelSecondLife = new LLPanelAvatarSecondLife("2nd Life", LLRect(),
														 self);
	return self->mPanelSecondLife;
}

void* LLPanelAvatar::createPanelAvatarWeb(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelWeb = new LLPanelAvatarWeb("Web", LLRect(), self);
	return self->mPanelWeb;
}

void* LLPanelAvatar::createPanelAvatarInterests(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelAdvanced = new LLPanelAvatarAdvanced("Interests", LLRect(),
													 self);
	return self->mPanelAdvanced;
}

void* LLPanelAvatar::createPanelAvatarPicks(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelPicks = new LLPanelAvatarPicks("Picks", LLRect(), self);
	return self->mPanelPicks;
}

void* LLPanelAvatar::createPanelAvatarClassified(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelClassified = new LLPanelAvatarClassified("Classified",
														 LLRect(), self);
	return self->mPanelClassified;
}

void* LLPanelAvatar::createPanelAvatarFirstLife(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelFirstLife = new LLPanelAvatarFirstLife("1st Life", LLRect(),
													   self);
	return self->mPanelFirstLife;
}

void* LLPanelAvatar::createPanelAvatarNotes(void* data)
{
	LLPanelAvatar* self = (LLPanelAvatar*)data;
	self->mPanelNotes = new LLPanelAvatarNotes("My Notes", LLRect(), self);
	return self->mPanelNotes;
}

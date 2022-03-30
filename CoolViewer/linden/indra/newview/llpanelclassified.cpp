/**
 * @file llpanelclassified.cpp
 * @brief LLPanelClassified class implementation
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

// Display of a classified used both for the global view in the
// Find directory, and also for each individual user's classified in their
// profile.

#include "llviewerprecompiledheaders.h"

#include "llpanelclassified.h"

#include "llalertdialog.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llclassifiedflags.h"
#include "llcombobox.h"
#include "llcorehttputil.h"
#include "lldir.h"
#include "lldispatcher.h"
#include "lllineeditor.h"
#include "llparcel.h"
#include "llsdserialize.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llappviewer.h"			// For abortQuit()
#include "llcommandhandler.h"		// For classified page click tracking
#include "llfloateravatarinfo.h"
#include "llfloaterclassified.h"
#include "llfloaterworldmap.h"
#include "lltexturectrl.h"
#include "llurldispatcher.h"		// For classified detail click teleports
#include "llviewercontrol.h"
#include "llviewermessage.h"		// For gGenericDispatcher
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexteditor.h"
#include "llworldmap.h"

constexpr S32 MINIMUM_PRICE_FOR_LISTING = 50;	// L$
constexpr S32 MATURE_CONTENT = 1;
constexpr S32 PG_CONTENT = 2;
constexpr S32 DECLINE_TO_STATE = 0;

///////////////////////////////////////////////////////////////////////////////
// LLClassifiedInfo static class
///////////////////////////////////////////////////////////////////////////////

LLClassifiedInfo::map_t LLClassifiedInfo::sCategories;

//static
void LLClassifiedInfo::loadCategories(const LLSD& options)
{
	std::string name;
	for (LLSD::array_const_iterator it = options.beginArray(),
									end = options.endArray();
		 it != end; ++it)
	{
		const LLSD& entry = *it;
		if (entry.has("name") && entry.has("category_id"))
		{
			U32 id = entry["category_id"].asInteger();
			sCategories[id] = entry["category_name"].asString();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelClassified class
///////////////////////////////////////////////////////////////////////////////

//static
std::list<LLPanelClassified*> LLPanelClassified::sInstances;

// "classifiedclickthrough"
// strings[0] = classified_id
// strings[1] = teleport_clicks
// strings[2] = map_clicks
// strings[3] = profile_clicks
class LLDispatchClassifiedClickThrough final : public LLDispatchHandler
{
public:
	bool operator()(const LLDispatcher* dispatcher, const std::string& key,
					const LLUUID& invoice, const sparam_t& strings) override
	{
		if (strings.size() != 4)
		{
			return false;
		}
		LLUUID classified_id(strings[0]);
		S32 teleport_clicks = atoi(strings[1].c_str());
		S32 map_clicks = atoi(strings[2].c_str());
		S32 profile_clicks = atoi(strings[3].c_str());
		LLPanelClassified::setClickThrough(classified_id, teleport_clicks,
										   map_clicks, profile_clicks, false);
		return true;
	}
};
static LLDispatchClassifiedClickThrough sClassifiedClickThrough;

#if 0	// Re-expose this if we need to have classified HTML detail pages.
// We need to count classified teleport clicks from the search HTML detail
// pages, so we need have a teleport that also sends a click count message.
class LLClassifiedTeleportHandler : public LLCommandHandler
{
protected:
	LOG_CLASS(LLClassifiedTeleportHandler);

public:
	// don't allow from external browsers because it moves you immediately
	LLClassifiedTeleportHandler()
	:	LLCommandHandler("classifiedteleport", UNTRUSTED_BLOCK) {}

	bool handle(const LLSD& tokens, const LLSD& queryMap)
	{
		// Need at least classified id and region name, so 2 params
		if (tokens.size() < 2) return false;
		LLUUID classified_id = tokens[0].asUUID();
		if (classified_id.isNull()) return false;
		// *HACK: construct a SLURL to do the teleport
		std::string url("secondlife:///app/teleport/");
		// skip the uuid we took off above, rebuild URL
		// separated by slashes.
		for (S32 i = 1; i < tokens.size(); ++i)
		{
			url += tokens[i].asString();
			url += "/";
		}
		llinfos << "Classified teleport to " << url << llendl;
		// *TODO: separately track old search, sidebar, and new search
		// Right now detail HTML pages count as new search.
		constexpr bool from_search = true;
		LLPanelClassified::sendClassifiedClickMessage(classified_id,
													 "teleport", from_search);
		// Invoke teleport
		LLMediaCtrl* web = NULL;
		constexpr bool trusted_browser = true;
		return LLURLDispatcher::dispatch(url, web, trusted_browser);
	}
};
// Creating the object registers with the dispatcher.
LLClassifiedTeleportHandler gClassifiedTeleportHandler;
#endif

LLPanelClassified::LLPanelClassified(bool in_finder, bool from_search)
:	LLPanel(std::string("Classified Panel")),
	mInFinder(in_finder),
	mFromSearch(from_search),
	mDirty(false),
	mForceClose(false),
	mLocationChanged(false),
	mClassifiedID(),
	mCreatorID(),
	mPriceForListing(0),
	mDataRequested(false),
	mPaidFor(false),
	mPosGlobal(),
	mSnapshotCtrl(NULL),
	mNameEditor(NULL),
	mDescEditor(NULL),
	mLocationEditor(NULL),
	mCategoryCombo(NULL),
	mMatureCombo(NULL),
	mAutoRenewCheck(NULL),
	mUpdateBtn(NULL),
	mTeleportBtn(NULL),
	mMapBtn(NULL),
	mProfileBtn(NULL),
	mInfoText(NULL),
	mSetBtn(NULL),
	mClickThroughText(NULL),
	mTeleportClicksOld(0),
	mMapClicksOld(0),
	mProfileClicksOld(0),
	mTeleportClicksNew(0),
	mMapClicksNew(0),
	mProfileClicksNew(0)
{
    sInstances.push_back(this);

	std::string file = mInFinder ? "panel_classified.xml"
								 : "panel_avatar_classified.xml";
	LLUICtrlFactory::getInstance()->buildPanel(this, file);

	// Register dispatcher
	gGenericDispatcher.addHandler("classifiedclickthrough",
								  &sClassifiedClickThrough);
}

LLPanelClassified::~LLPanelClassified()
{
    sInstances.remove(this);
}

void LLPanelClassified::reset()
{
	mClassifiedID.setNull();
	mCreatorID.setNull();
	mParcelID.setNull();

	// Don't request data, this isn't valid
	mDataRequested = true;

	mDirty = false;
	mPaidFor = false;

	mPosGlobal.clear();

	clearCtrls();
	resetDirty();
}

bool LLPanelClassified::postBuild()
{
    mSnapshotCtrl = getChild<LLTextureCtrl>("snapshot_ctrl");
	mSnapshotCtrl->setCommitCallback(onCommitAny);
	mSnapshotCtrl->setCallbackUserData(this);
	mSnapshotSize = mSnapshotCtrl->getRect();

    mNameEditor = getChild<LLLineEditor>("given_name_editor");
	mNameEditor->setMaxTextLength(DB_PARCEL_NAME_LEN);
	mNameEditor->setCommitOnFocusLost(true);
	mNameEditor->setFocusReceivedCallback(focusReceived, this);
	mNameEditor->setCommitCallback(onCommitAny);
	mNameEditor->setCallbackUserData(this);
	mNameEditor->setPrevalidate(LLLineEditor::prevalidateASCII);

    mDescEditor = getChild<LLTextEditor>("desc_editor");
	mDescEditor->setCommitOnFocusLost(true);
	mDescEditor->setFocusReceivedCallback(focusReceived, this);
	mDescEditor->setCommitCallback(onCommitAny);
	mDescEditor->setCallbackUserData(this);
	mDescEditor->setTabsToNextField(true);

    mLocationEditor = getChild<LLLineEditor>("location_editor");

    mSetBtn = getChild<LLButton>("set_location_btn");
    mSetBtn->setClickedCallback(onClickSet);
    mSetBtn->setCallbackUserData(this);

    mTeleportBtn = getChild<LLButton>("classified_teleport_btn");
    mTeleportBtn->setClickedCallback(onClickTeleport);
    mTeleportBtn->setCallbackUserData(this);

    mMapBtn = getChild<LLButton>("classified_map_btn");
    mMapBtn->setClickedCallback(onClickMap);
    mMapBtn->setCallbackUserData(this);

	if (mInFinder)
	{
		mProfileBtn  = getChild<LLButton>("classified_profile_btn");
		mProfileBtn->setClickedCallback(onClickProfile);
		mProfileBtn->setCallbackUserData(this);
	}

	mCategoryCombo = getChild<LLComboBox>("classified_category_combo");
	for (LLClassifiedInfo::map_t::iterator
			it = LLClassifiedInfo::sCategories.begin(),
			end = LLClassifiedInfo::sCategories.end();
		 it != end; ++it)
	{
		mCategoryCombo->add(it->second, (void*)((intptr_t)it->first));
	}
	mCategoryCombo->setCurrentByIndex(0);
	mCategoryCombo->setCommitCallback(onCommitAny);
	mCategoryCombo->setCallbackUserData(this);

	mMatureCombo = getChild<LLComboBox>("classified_mature_check");
	mMatureCombo->setCurrentByIndex(0);
	mMatureCombo->setCommitCallback(onCommitAny);
	mMatureCombo->setCallbackUserData(this);
	if (gAgent.wantsPGOnly())
	{
		// Teens do not get to set mature flag. JC
		mMatureCombo->setVisible(false);
		mMatureCombo->setCurrentByIndex(PG_CONTENT);
	}

	if (!mInFinder)
	{
		mAutoRenewCheck = getChild<LLCheckBoxCtrl>("auto_renew_check");
		mAutoRenewCheck->setCommitCallback(onCommitAny);
		mAutoRenewCheck->setCallbackUserData(this);
	}

	mUpdateBtn = getChild<LLButton>("classified_update_btn");
    mUpdateBtn->setClickedCallback(onClickUpdate);
    mUpdateBtn->setCallbackUserData(this);

	if (!mInFinder)
	{
		mClickThroughText = getChild<LLTextBox>("click_through_text");
	}

	resetDirty();

	return true;
}

bool LLPanelClassified::titleIsValid()
{
	// Disallow leading spaces, punctuation, etc. that screw up
	// sort order.
	const std::string& name = mNameEditor->getText();
	if (name.empty())
	{
		gNotifications.add("BlankClassifiedName");
		return false;
	}
	if (!isalnum(name[0]))
	{
		gNotifications.add("ClassifiedMustBeAlphanumeric");
		return false;
	}

	return true;
}

void LLPanelClassified::apply()
{
	// Apply is used for automatically saving results, so only
	// do that if there is a difference, and this is a save not create.
	if (checkDirty() && mPaidFor)
	{
		sendClassifiedInfoUpdate();
	}
}

bool LLPanelClassified::saveCallback(const LLSD& notification,
									 const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	switch (option)
	{
		case 0:		// Save
		{
			sendClassifiedInfoUpdate();
			// fall through to close
		}

		case 1:		// Don't Save
		{
			mForceClose = true;
			// Close containing floater
			if (gFloaterViewp && gFloaterViewp->getParentFloater(this))
			{
				gFloaterViewp->getParentFloater(this)->close();
			}
			break;
		}

		default:	// Cancel
		{
            gAppViewerp->abortQuit();
		}
	}
	return false;
}

bool LLPanelClassified::canClose()
{
	if (mForceClose || !checkDirty())
	{
		return true;
	}

	LLSD args;
	args["NAME"] = mNameEditor->getText();
	gNotifications.add("ClassifiedSave", args, LLSD(),
					   boost::bind(&LLPanelClassified::saveCallback, this,
								   _1, _2));
	return false;
}

// Fill in some reasonable defaults for a new classified.
void LLPanelClassified::initNewClassified()
{
	// TODO:  Don't generate this on the client.
	mClassifiedID.generate();

	mCreatorID = gAgentID;

	mPosGlobal = gAgent.getPositionGlobal();

	mPaidFor = false;

	// Try to fill in the current parcel
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		mNameEditor->setText(parcel->getName());
		//mDescEditor->setText(parcel->getDesc());
		mSnapshotCtrl->setImageAssetID(parcel->getSnapshotID());
		//mPriceEditor->setText("0");
		mCategoryCombo->setCurrentByIndex(0);
	}

	mUpdateBtn->setLabel(getString("publish_txt"));

	// simulate clicking the "location" button
	LLPanelClassified::onClickSet(this);
}

void LLPanelClassified::setClassifiedID(const LLUUID& id)
{
	mClassifiedID = id;
}

//static
void LLPanelClassified::setClickThrough(const LLUUID& classified_id,
										S32 teleport,
										S32 map,
										S32 profile,
										bool from_new_table)
{
	for (panel_list_t::iterator iter = sInstances.begin();
		 iter != sInstances.end(); ++iter)
	{
		LLPanelClassified* self = *iter;
		// For top picks, must match pick id
		if (self->mClassifiedID != classified_id)
		{
			continue;
		}

		// We need to check to see if the data came from the new stat_table
		// or the old classified table. We also need to cache the data from
		// the two separate sources so as to display the aggregate totals.

		if (from_new_table)
		{
			self->mTeleportClicksNew = teleport;
			self->mMapClicksNew = map;
			self->mProfileClicksNew = profile;
		}
		else
		{
			self->mTeleportClicksOld = teleport;
			self->mMapClicksOld = map;
			self->mProfileClicksOld = profile;
		}

		if (self->mClickThroughText)
		{
			std::string msg = llformat("Clicks: %d teleport, %d map, %d profile",
									   self->mTeleportClicksNew + self->mTeleportClicksOld,
									   self->mMapClicksNew + self->mMapClicksOld,
									   self->mProfileClicksNew + self->mProfileClicksOld);
			self->mClickThroughText->setText(msg);
		}
	}
}

// Schedules the panel to request data
// from the server next time it is drawn.
void LLPanelClassified::markForServerRequest()
{
	mDataRequested = false;
}

std::string LLPanelClassified::getClassifiedName()
{
	return mNameEditor->getText();
}

void LLPanelClassified::sendClassifiedInfoRequest()
{
	if (mClassifiedID != mRequestedID)
	{
	    LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_ClassifiedInfoRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->nextBlockFast(_PREHASH_Data);
		msg->addUUIDFast(_PREHASH_ClassifiedID, mClassifiedID);
		gAgent.sendReliableMessage();

		mDataRequested = true;
		mRequestedID = mClassifiedID;

		// While we are at it let's get the stats from the new table if that
		// capability exists.
		const std::string& url =
			gAgent.getRegionCapability("SearchStatRequest");
		if (url.empty())
		{
			return;
		}
		llinfos << "Classified stat request via capability" << llendl;
		LLSD body;
		body["classified_id"] = mClassifiedID;
		LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpPost(url, body,
															  boost::bind(&LLPanelClassified::handleSearchStatResponse,
																		  mClassifiedID,
																		  _1));
	}
}

//static
void LLPanelClassified::handleSearchStatResponse(LLUUID id, LLSD result)
{
	if (!result.isMap())
	{
		llwarns << "Malformed response for classified: " << id << llendl;
		return;
	}

	S32 teleport = result["teleport_clicks"].asInteger();
	S32 map = result["map_clicks"].asInteger();
	S32 profile = result["profile_clicks"].asInteger();
	S32 search_teleport = result["search_teleport_clicks"].asInteger();
	S32 search_map = result["search_map_clicks"].asInteger();
	S32 search_profile = result["search_profile_clicks"].asInteger();

	LLPanelClassified::setClickThrough(id, teleport + search_teleport,
									   map + search_map,
									   profile + search_profile, true);
}

void LLPanelClassified::sendClassifiedInfoUpdate()
{
	// If we don't have a classified id yet, we'll need to generate one,
	// otherwise we'll keep overwriting classified_id 00000 in the database.
	if (mClassifiedID.isNull())
	{
		// TODO:  Don't do this on the client.
		mClassifiedID.generate();
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ClassifiedInfoUpdate);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_Data);
	msg->addUUIDFast(_PREHASH_ClassifiedID, mClassifiedID);
	// TODO: fix this
	U32 category = mCategoryCombo->getCurrentIndex() + 1;
	msg->addU32Fast(_PREHASH_Category, category);
	msg->addStringFast(_PREHASH_Name, mNameEditor->getText());
	msg->addStringFast(_PREHASH_Desc, mDescEditor->getText());

	// fills in on simulator if null
	msg->addUUIDFast(_PREHASH_ParcelID, mParcelID);
	// fills in on simulator if null
	msg->addU32Fast(_PREHASH_ParentEstate, 0);
	msg->addUUIDFast(_PREHASH_SnapshotID, mSnapshotCtrl->getImageAssetID());
	msg->addVector3dFast(_PREHASH_PosGlobal, mPosGlobal);
	bool mature = mMatureCombo->getCurrentIndex() == MATURE_CONTENT;
	bool auto_renew = false;
	if (mAutoRenewCheck)
	{
		auto_renew = mAutoRenewCheck->get();
	}
    // These flags doesn't matter here.
    constexpr bool adult_enabled = false;
	constexpr bool is_pg = false;
	U8 flags = pack_classified_flags_request(auto_renew, is_pg, mature,
											 adult_enabled);
	msg->addU8Fast(_PREHASH_ClassifiedFlags, flags);
	msg->addS32("PriceForListing", mPriceForListing);
	gAgent.sendReliableMessage();

	mDirty = false;
}

//static
void LLPanelClassified::processClassifiedInfoReply(LLMessageSystem* msg,
												   void**)
{
	//lldebugs << "processClassifiedInfoReply()" << llendl;
    // Extract the agent id and verify the message is for this
    // client.
    LLUUID agent_id;
    msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
    if (agent_id != gAgentID)
    {
        llwarns << "Agent ID mismatch. Got: " << agent_id << llendl;
		return;
    }

    LLUUID classified_id;
    msg->getUUIDFast(_PREHASH_Data, _PREHASH_ClassifiedID, classified_id);

    LLUUID creator_id;
    msg->getUUIDFast(_PREHASH_Data, _PREHASH_CreatorID, creator_id);

    LLUUID parcel_id;
    msg->getUUIDFast(_PREHASH_Data, _PREHASH_ParcelID, parcel_id);

	std::string name;
	msg->getStringFast(_PREHASH_Data, _PREHASH_Name, name);

	std::string desc;
	msg->getStringFast(_PREHASH_Data, _PREHASH_Desc, desc);

	LLUUID snapshot_id;
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_SnapshotID, snapshot_id);

    // "Location text" is actually the original
    // name that owner gave the parcel, and the location.
	std::string location_text;

    msg->getStringFast(_PREHASH_Data, _PREHASH_ParcelName, location_text);
	if (!location_text.empty())
	{
		location_text.append(", ");
	}

	std::string sim_name;
	msg->getStringFast(_PREHASH_Data, _PREHASH_SimName, sim_name);

	LLVector3d pos_global;
	msg->getVector3dFast(_PREHASH_Data, _PREHASH_PosGlobal, pos_global);

    S32 region_x = ll_roundp((F32)pos_global.mdV[VX]) % REGION_WIDTH_UNITS;
    S32 region_y = ll_roundp((F32)pos_global.mdV[VY]) % REGION_WIDTH_UNITS;
	S32 region_z = ll_roundp((F32)pos_global.mdV[VZ]);

	std::string buffer = llformat("%s (%d, %d, %d)", sim_name.c_str(),
								  region_x, region_y, region_z);
    location_text.append(buffer);

	U8 flags;
	msg->getU8Fast(_PREHASH_Data, _PREHASH_ClassifiedFlags, flags);
	//bool enabled = is_cf_enabled(flags);
	bool mature = is_cf_mature(flags);
	bool auto_renew = is_cf_auto_renew(flags);

	U32 date = 0;
	msg->getU32Fast(_PREHASH_Data, _PREHASH_CreationDate, date);
	time_t tim = date;
	tm *now = localtime(&tim);

	// future use
	U32 expiration_date = 0;
	msg->getU32("Data", "ExpirationDate", expiration_date);

	U32 category = 0;
	msg->getU32Fast(_PREHASH_Data, _PREHASH_Category, category);

	S32 price_for_listing = 0;
	msg->getS32("Data", "PriceForListing", price_for_listing);

    // Look up the panel to fill in
	for (panel_list_t::iterator iter = sInstances.begin();
		 iter != sInstances.end(); ++iter)
	{
		LLPanelClassified* self = *iter;
		// For top picks, must match pick id
		if (self->mClassifiedID != classified_id)
		{
			continue;
		}

        // Found the panel, now fill in the information
		self->mClassifiedID = classified_id;
		self->mCreatorID = creator_id;
		self->mParcelID = parcel_id;
		self->mPriceForListing = price_for_listing;
		self->mSimName.assign(sim_name);
		self->mPosGlobal = pos_global;

		// Update UI controls
        self->mNameEditor->setText(name);
        self->mDescEditor->setText(desc);
        self->mSnapshotCtrl->setImageAssetID(snapshot_id);
        self->mLocationEditor->setText(location_text);
		self->mLocationChanged = false;

		self->mCategoryCombo->setCurrentByIndex(category - 1);
		if (mature)
		{
			self->mMatureCombo->setCurrentByIndex(MATURE_CONTENT);
		}
		else
		{
			self->mMatureCombo->setCurrentByIndex(PG_CONTENT);
		}
		if (self->mAutoRenewCheck)
		{
			self->mAutoRenewCheck->set(auto_renew);
		}

		std::string datestr;
		timeStructToFormattedString(now,
									gSavedSettings.getString("ShortDateFormat"),
									datestr);
		LLStringUtil::format_map_t string_args;
		string_args["[DATE]"] = datestr;
		string_args["[AMT]"] = llformat("%d", price_for_listing);
		if (self->getChild<LLTextBox>("classified_info_text", true, false))
		{
			self->childSetText("classified_info_text",
							   self->getString("ad_placed_paid", string_args));
		}

		// If we got data from the database, we know the listing is paid for.
		self->mPaidFor = true;

		self->mUpdateBtn->setLabel(self->getString("update_txt"));

		self->resetDirty();

		// I don't know if a second call is deliberate or a bad merge, so I'm
		// leaving it here.
		self->resetDirty();
    }
}

void LLPanelClassified::draw()
{
	refresh();
	LLPanel::draw();
}

void LLPanelClassified::refresh()
{
	if (!mDataRequested)
	{
		sendClassifiedInfoRequest();
	}

    // Check for god mode
    bool godlike = gAgent.isGodlike();
	bool is_self = (gAgentID == mCreatorID);

    // Set button visibility/enablement appropriately
	if (mInFinder)
	{
		// End user doesn't ned to see price twice, or date posted.

		mSnapshotCtrl->setEnabled(godlike);
		if (godlike)
		{
			// make it smaller, so text is more legible
			//mSnapshotCtrl->setOrigin(20, 175);
			mSnapshotCtrl->reshape(360, 270);
		}
		else
		{
			mSnapshotCtrl->setOrigin(mSnapshotSize.mLeft,
									 mSnapshotSize.mBottom);
			mSnapshotCtrl->reshape(mSnapshotSize.getWidth(),
								   mSnapshotSize.getHeight());
			//normal
		}
		mNameEditor->setEnabled(godlike);
		mDescEditor->setEnabled(godlike);
		mCategoryCombo->setEnabled(godlike);
		mCategoryCombo->setVisible(godlike);

		mMatureCombo->setEnabled(godlike);
		mMatureCombo->setVisible(godlike);

		// Jesse (who is the only one who uses this, as far as we can tell
		// Says that he does not want a set location button - he has used it
		// accidently in the past.
		mSetBtn->setVisible(false);
		mSetBtn->setEnabled(false);

		mUpdateBtn->setEnabled(godlike);
		mUpdateBtn->setVisible(godlike);
	}
	else
	{
		mSnapshotCtrl->setEnabled(is_self);
		mNameEditor->setEnabled(is_self);
		mDescEditor->setEnabled(is_self);
		//mPriceEditor->setEnabled(is_self);
		mCategoryCombo->setEnabled(is_self);
		mMatureCombo->setEnabled(is_self);

		if (is_self)
		{
			if (mMatureCombo->getCurrentIndex() == 0)
			{
				// It's a new panel.
				// PG regions should have PG classifieds.
				// Adult should have mature.
				setDefaultAccessCombo();
			}
		}

		if (mAutoRenewCheck)
		{
			mAutoRenewCheck->setEnabled(is_self);
			mAutoRenewCheck->setVisible(is_self);
		}

		mClickThroughText->setEnabled(is_self);
		mClickThroughText->setVisible(is_self);

		mSetBtn->setVisible(is_self);
		mSetBtn->setEnabled(is_self);

		mUpdateBtn->setEnabled(is_self && checkDirty());
		mUpdateBtn->setVisible(is_self);
	}
}

// static
void LLPanelClassified::onClickUpdate(void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;
	if (!self) return;

	// Disallow leading spaces, punctuation, etc. that screw up sort order.
	if (!self->titleIsValid())
	{
		return;
	}

	// If user has not set mature, do not allow publish
	if (self->mMatureCombo->getCurrentIndex() == DECLINE_TO_STATE)
	{
		// Tell user about it
		gNotifications.add("SetClassifiedMature", LLSD(), LLSD(),
						   boost::bind(&LLPanelClassified::confirmMature, self,
									   _1, _2));
	}
	else
	{
		// Mature content flag is set, proceed
		self->gotMature();
	}
}

// Callback from a dialog indicating response to mature notification
bool LLPanelClassified::confirmMature(const LLSD& notification,
									  const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	switch (option)
	{
		case 0:		// 0 == Yes
			mMatureCombo->setCurrentByIndex(MATURE_CONTENT);
			break;
		case 1:		// 1 == No
			mMatureCombo->setCurrentByIndex(PG_CONTENT);
			break;
		default:	// 2 == Cancel
			return false;
	}

	// If we got here it means they set a valid value
	gotMature();
	return false;
}

// Called after we have determined whether this classified has
// mature content or not.
void LLPanelClassified::gotMature()
{
	// if already paid for, just do the update
	if (mPaidFor)
	{
		LLNotification::Params params("PublishClassified");
		params.functor(boost::bind(&LLPanelClassified::confirmPublish,
								   this, _1, _2));
		gNotifications.forceResponse(params, 0);
	}
	else
	{
		// Ask the user how much they want to pay
		LLFloaterPriceForListing::show(callbackGotPriceForListing, this);
	}
}

// static
void LLPanelClassified::callbackGotPriceForListing(S32 option,
												   std::string text,
												   void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;

	// Only do something if user hits publish
	if (option != 0) return;

	S32 price_for_listing = strtol(text.c_str(), NULL, 10);
	if (price_for_listing < MINIMUM_PRICE_FOR_LISTING)
	{
		LLSD args;
		std::string price_text = llformat("%d", MINIMUM_PRICE_FOR_LISTING);
		args["MIN_PRICE"] = price_text;

		gNotifications.add("MinClassifiedPrice", args);
		return;
	}

	// price is acceptable, put it in the dialog for later read by update send
	self->mPriceForListing = price_for_listing;

	LLSD args;
	args["AMOUNT"] = llformat("%d", price_for_listing);
	gNotifications.add("PublishClassified", args, LLSD(),
					   boost::bind(&LLPanelClassified::confirmPublish, self,
								   _1, _2));
}

void LLPanelClassified::resetDirty()
{
	// Tell all the widgets to reset their dirty state since the ad was just
	// saved
	if (mSnapshotCtrl)		mSnapshotCtrl->resetDirty();
	if (mNameEditor)		mNameEditor->resetDirty();
	if (mDescEditor)		mDescEditor->resetDirty();
	if (mLocationEditor)	mLocationEditor->resetDirty();
	mLocationChanged = false;
	if (mCategoryCombo)		mCategoryCombo->resetDirty();
	if (mMatureCombo)		mMatureCombo->resetDirty();
	if (mAutoRenewCheck)	mAutoRenewCheck->resetDirty();
}

// invoked from callbackConfirmPublish
bool LLPanelClassified::confirmPublish(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	// Option 0 = publish
	if (option != 0) return false;

	sendClassifiedInfoUpdate();

	// Big hack: assume that top picks are always in a browser and non-finder
	// classifieds are always in a tab container.
	if (!mInFinder)
	{
		LLTabContainer* tab = (LLTabContainer*)getParent();
		tab->setCurrentTabName(mNameEditor->getText());
	}
#if 0	// TODO: enable this
	else
	{
		LLPanelDirClassifieds* panel = (LLPanelDirClassifieds*)getParent();
		panel->renameClassified(mClassifiedID, mNameEditor->getText());
	}
#endif

	resetDirty();
	return false;
}

// static
void LLPanelClassified::onClickTeleport(void* data)
{
    LLPanelClassified* self = (LLPanelClassified*)data;

    if (self && !self->mPosGlobal.isExactlyZero())
    {
        gAgent.teleportViaLocation(self->mPosGlobal);
		if (gFloaterWorldMapp)
		{
	        gFloaterWorldMapp->trackLocation(self->mPosGlobal);
		}
		self->sendClassifiedClickMessage("teleport");
    }
}

// static
void LLPanelClassified::onClickMap(void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;
	if (self)
	{
		if (gFloaterWorldMapp)
		{
			gFloaterWorldMapp->trackLocation(self->mPosGlobal);
		}
		LLFloaterWorldMap::show(NULL, true);

		self->sendClassifiedClickMessage("map");
	}
}

// static
void LLPanelClassified::onClickProfile(void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;
	if (self)
	{
		LLFloaterAvatarInfo::showFromDirectory(self->mCreatorID);
		self->sendClassifiedClickMessage("profile");
	}
}

#if 0
// static
void LLPanelClassified::onClickLandmark(void* data)
{
    LLPanelClassified* self = (LLPanelClassified*)data;
	create_landmark(self->mNameEditor->getText(), "", self->mPosGlobal);
}
#endif

// static
void LLPanelClassified::onClickSet(void* data)
{
    LLPanelClassified* self = (LLPanelClassified*)data;

	// Save location for later.
	self->mPosGlobal = gAgent.getPositionGlobal();

	std::string location_text;
	std::string regionName = "(will update after publish)";
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		regionName = regionp->getName();
	}
	location_text.assign(regionName);
	location_text.append(", ");

    S32 region_x = ll_roundp((F32)self->mPosGlobal.mdV[VX]) %
				   REGION_WIDTH_UNITS;
    S32 region_y = ll_roundp((F32)self->mPosGlobal.mdV[VY]) %
				   REGION_WIDTH_UNITS;
	S32 region_z = ll_roundp((F32)self->mPosGlobal.mdV[VZ]);

	location_text.append(self->mSimName);
    location_text.append(llformat(" (%d, %d, %d)", region_x, region_y,
								  region_z));

	self->mLocationEditor->setText(location_text);
	self->mLocationChanged = true;

	self->setDefaultAccessCombo();

	// Set this to null so it updates on the next save.
	self->mParcelID.setNull();

	onCommitAny(NULL, data);
}

bool LLPanelClassified::checkDirty()
{
	mDirty = mLocationChanged || mSnapshotCtrl->isDirty() ||
			 mNameEditor->isDirty() || mDescEditor->isDirty() ||
			 mLocationEditor->isDirty() || mCategoryCombo->isDirty() ||
			 mMatureCombo->isDirty() ||
			 (mAutoRenewCheck && mAutoRenewCheck->isDirty());

	return mDirty;
}

// static
void LLPanelClassified::onCommitAny(LLUICtrl* ctrl, void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;
	if (self)
	{
		self->checkDirty();
	}
}

// static
void LLPanelClassified::focusReceived(LLFocusableElement* ctrl, void* data)
{
	// Allow the data to be saved
	onCommitAny((LLUICtrl*)ctrl, data);
}

void LLPanelClassified::sendClassifiedClickMessage(const std::string& type)
{
	// You are allowed to click on your own ads to reassure yourself that the
	// system is working.
	LLSD body;
	body["type"] = type;
	body["from_search"] = mFromSearch;
	body["classified_id"] = mClassifiedID;
	body["parcel_id"] = mParcelID;
	body["dest_pos_global"] = mPosGlobal.getValue();
	body["region_name"] = mSimName;

	const std::string& url = gAgent.getRegionCapability("SearchStatTracking");
	if (url.empty())
	{
		return;
	}
	llinfos << "Sending classified click message via capability" << llendl;
	LLCoreHttpUtil::HttpCoroutineAdapter::messageHttpPost(url, body,
														  "Tracking click report sent.",
														  "Failed to send tracking click report.");
}

////////////////////////////////////////////////////////////////////////////////////////////

LLFloaterPriceForListing::LLFloaterPriceForListing()
:	LLFloater(std::string("PriceForListing")),
	mCallback(NULL),
	mUserData(NULL)
{
}

//virtual
bool LLFloaterPriceForListing::postBuild()
{
	LLLineEditor* edit = getChild<LLLineEditor>("price_edit");
	if (edit)
	{
		edit->setPrevalidate(LLLineEditor::prevalidateNonNegativeS32);
		std::string min_price = llformat("%d", MINIMUM_PRICE_FOR_LISTING);
		edit->setText(min_price);
		edit->selectAll();
		edit->setFocus(true);
	}

	childSetAction("set_price_btn", onClickSetPrice, this);
	childSetAction("cancel_btn", onClickCancel, this);
	setDefaultBtn("set_price_btn");

	return true;
}

//static
void LLFloaterPriceForListing::show(void (*callback)(S32, std::string, void*),
									void* userdata)
{
	LLFloaterPriceForListing* self = new LLFloaterPriceForListing();

	// Builds and adds to gFloaterViewp
	LLUICtrlFactory::getInstance()->buildFloater(self,
												 "floater_price_for_listing.xml");
	self->center();

	self->mCallback = callback;
	self->mUserData = userdata;
}

//static
void LLFloaterPriceForListing::onClickSetPrice(void* data)
{
	buttonCore(0, data);
}

//static
void LLFloaterPriceForListing::onClickCancel(void* data)
{
	buttonCore(1, data);
}

//static
void LLFloaterPriceForListing::buttonCore(S32 button, void* data)
{
	LLFloaterPriceForListing* self = (LLFloaterPriceForListing*)data;
	if (self && self->mCallback)
	{
		std::string text = self->childGetText("price_edit");
		self->mCallback(button, text, self->mUserData);
		self->close();
	}
}

void LLPanelClassified::setDefaultAccessCombo()
{
	LLViewerRegion* regionp = gAgent.getRegion();
	U8 access = regionp ? regionp->getSimAccess() : SIM_ACCESS_MATURE;
	if (access == SIM_ACCESS_PG)
	{
		mMatureCombo->setCurrentByIndex(PG_CONTENT);
	}
	else if (access == SIM_ACCESS_ADULT)
	{
		mMatureCombo->setCurrentByIndex(MATURE_CONTENT);
	}
}

/**
 * @file llpanelpick.cpp
 * @brief LLPanelPick class implementation
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

// Display of a "Top Pick" used both for the global top picks in the
// Find directory, and also for each individual user's picks in their
// profile.

#include "llviewerprecompiledheaders.h"

#include "llpanelpick.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "llparcel.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "lltexturectrl.h"
#include "llfloaterworldmap.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewermessage.h"		// For send_generic_message()
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexteditor.h"
#include "llworldmap.h"

//static
std::list<LLPanelPick*> LLPanelPick::sInstances;

LLPanelPick::LLPanelPick(bool top_pick)
:	LLPanel("Top Picks Panel"),
	mTopPick(top_pick),
	mDataRequested(false),
	mDataReceived(false)
{
	sInstances.push_back(this);

	std::string xml_file = top_pick ? "panel_top_pick.xml"
									: "panel_avatar_pick.xml";
	LLUICtrlFactory::getInstance()->buildPanel(this, xml_file);
}

LLPanelPick::~LLPanelPick()
{
	sInstances.remove(this);
}

void LLPanelPick::reset()
{
	mPickID.setNull();
	mCreatorID.setNull();
	mParcelID.setNull();

	// Don't request data, this isn't valid
	mDataRequested = true;
	mDataReceived = false;

	mPosGlobal.clear();

	clearCtrls();
}

//virtual
bool LLPanelPick::postBuild()
{
	mSnapshotCtrl = getChild<LLTextureCtrl>("snapshot_ctrl");
	mSnapshotCtrl->setCommitCallback(onCommitAny);
	mSnapshotCtrl->setCallbackUserData(this);

	mNameEditor = getChild<LLLineEditor>("given_name_editor");
	mNameEditor->setCommitOnFocusLost(true);
	mNameEditor->setCommitCallback(onCommitAny);
	mNameEditor->setCallbackUserData(this);

	mDescEditor = getChild<LLTextEditor>("desc_editor");
	mDescEditor->setCommitOnFocusLost(true);
	mDescEditor->setCommitCallback(onCommitAny);
	mDescEditor->setCallbackUserData(this);
	mDescEditor->setTabsToNextField(true);

	mLocationEditor = getChild<LLLineEditor>("location_editor");

	mSetBtn = getChild<LLButton>( "set_location_btn");
	mSetBtn->setClickedCallback(onClickSetLocation);
	mSetBtn->setCallbackUserData(this);

	mTeleportBtn = getChild<LLButton>( "pick_teleport_btn");
	mTeleportBtn->setClickedCallback(onClickTeleport);
	mTeleportBtn->setCallbackUserData(this);

	mMapBtn = getChild<LLButton>( "pick_map_btn");
	mMapBtn->setClickedCallback(onClickMap);
	mMapBtn->setCallbackUserData(this);

	mSortOrderText = getChild<LLTextBox>("sort_order_text");

	mSortOrderEditor = getChild<LLLineEditor>("sort_order_editor");
	mSortOrderEditor->setPrevalidate(LLLineEditor::prevalidateInt);
	mSortOrderEditor->setCommitOnFocusLost(true);
	mSortOrderEditor->setCommitCallback(onCommitAny);
	mSortOrderEditor->setCallbackUserData(this);

	mEnabledCheck = getChild<LLCheckBoxCtrl>( "enabled_check");
	mEnabledCheck->setCommitCallback(onCommitAny);
	mEnabledCheck->setCallbackUserData(this);

	return true;
}

// Fill in some reasonable defaults for a new pick.
void LLPanelPick::initNewPick()
{
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		return;
	}
//mk

	mPickID.generate();
	mCreatorID = gAgentID;
	mPosGlobal = gAgent.getPositionGlobal();

	// Try to fill in the current parcel
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		mNameEditor->setText(parcel->getName());
		mDescEditor->setText(parcel->getDesc());
		mSnapshotCtrl->setImageAssetID(parcel->getSnapshotID());
	}

	// Commit to the database, since we've got "new" values.
	sendPickInfoUpdate();
}

void LLPanelPick::setPickID(const LLUUID& pick_id, const LLUUID& creator_id)
{
	mPickID = pick_id;
	mCreatorID = creator_id;
}

// Schedules the panel to request data from the server next time it is drawn
void LLPanelPick::markForServerRequest()
{
	mDataRequested = false;
	mDataReceived = false;
}

std::string LLPanelPick::getPickName()
{
	return mNameEditor->getText();
}

void LLPanelPick::sendPickInfoRequest()
{
	// We must ask for a pick based on the creator Id because the pick database
	// is distributed to the inventory cluster. JC
	std::vector<std::string> strings;
	strings.push_back(mCreatorID.asString());
	strings.push_back(mPickID.asString());
	send_generic_message("pickinforequest", strings);
	mDataRequested = true;
}

void LLPanelPick::sendPickInfoUpdate()
{
	// If we do not have a pick id yet, we'll need to generate one,
	// otherwise we will keep overwriting pick_id 00000 in the database.
	if (mPickID.isNull())
	{
		mPickID.generate();
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("PickInfoUpdate");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgentSessionID);
	msg->nextBlock("Data");
	msg->addUUID("PickID", mPickID);
	msg->addUUID("CreatorID", mCreatorID);
	msg->addBool("TopPick", false);	// Legacy, no more used server-side.
	// fills in on simulator if null
	msg->addUUID("ParcelID", mParcelID);
	msg->addString("Name", mNameEditor->getText());
	msg->addString("Desc", mDescEditor->getText());
	msg->addUUID("SnapshotID", mSnapshotCtrl->getImageAssetID());
	msg->addVector3d("PosGlobal", mPosGlobal);

	// Only top picks have a sort order
	S32 sort_order;
	if (mTopPick)
	{
		sort_order = atoi(mSortOrderEditor->getText().c_str());
	}
	else
	{
		sort_order = 0;
	}
	msg->addS32("SortOrder", sort_order);
	msg->addBool("Enabled", mEnabledCheck->get());
	gAgent.sendReliableMessage();
}

// Returns a location text made up from the owner name, the parcel name, the
// sim name and the coordinates in that sim.
//static
std::string LLPanelPick::createLocationText(const std::string& owner_name,
											std::string parcel_name,
											const std::string& sim_name,
											const LLVector3d& pos_global)
{
	std::string location_text = owner_name;
	// strip leading spaces in parcel name
	while (!parcel_name.empty() && parcel_name[0] == ' ')
	{
		parcel_name = parcel_name.substr(1);
	}
	if (!parcel_name.empty())
	{
		if (location_text.empty())
		{
			location_text = parcel_name;
		}
		else
		{
			location_text += ", " + parcel_name;
		}
	}
	if (!sim_name.empty())
	{
		if (location_text.empty())
		{
			location_text = sim_name;
		}
		else
		{
			location_text += ", " + sim_name;
		}
	}
	if (!pos_global.isNull())
	{
		if (!location_text.empty())
		{
			location_text += " ";
		}
		S32 x = ll_roundp((F32)pos_global.mdV[VX]) % REGION_WIDTH_UNITS;
		S32 y = ll_roundp((F32)pos_global.mdV[VY]) % REGION_WIDTH_UNITS;
		S32 z = ll_roundp((F32)pos_global.mdV[VZ]);
		location_text += llformat("(%d, %d, %d)", x, y, z);
	}

	return location_text;
}

//static
void LLPanelPick::processPickInfoReply(LLMessageSystem* msg, void**)
{
	// Extract the agent id and verify the message is for this client.
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Agent ID mismatch. Got agent ID " << agent_id << llendl;
		return;
	}

	LLUUID pick_id;
	msg->getUUID("Data", "PickID", pick_id);

	LLUUID creator_id;
	msg->getUUID("Data", "CreatorID", creator_id);

	bool top_pick;	// Legacy. Not used any more server-side.
	msg->getBool("Data", "TopPick", top_pick);

	LLUUID parcel_id;
	msg->getUUID("Data", "ParcelID", parcel_id);

	std::string name;
	msg->getString("Data", "Name", name);

	std::string desc;
	msg->getString("Data", "Desc", desc);

	LLUUID snapshot_id;
	msg->getUUID("Data", "SnapshotID", snapshot_id);

	std::string owner_name, parcel_name, sim_name;
	LLVector3d pos_global;
	msg->getString("Data", "User", owner_name);
	msg->getString("Data", "OriginalName", parcel_name);
	msg->getString("Data", "SimName", sim_name);
	msg->getVector3d("Data", "PosGlobal", pos_global);
	std::string location_text = createLocationText(owner_name, parcel_name,
												   sim_name, pos_global);
	S32 sort_order;
	msg->getS32("Data", "SortOrder", sort_order);

	bool enabled;
	msg->getBool("Data", "Enabled", enabled);

	// Look up the panel to fill in
	for (panel_list_t::iterator iter = sInstances.begin();
		 iter != sInstances.end(); ++iter)
	{
		LLPanelPick* self = *iter;
		if (self->mPickID != pick_id)
		{
			continue;
		}

		self->mDataReceived = true;

		// Found the panel, now fill in the information
		self->mPickID = pick_id;
		self->mCreatorID = creator_id;
		self->mParcelID = parcel_id;
		self->mSimName.assign(sim_name);
		self->mPosGlobal = pos_global;

		// Update UI controls

		self->mNameEditor->setText(std::string(name));

		self->mDescEditor->clear();
		self->mDescEditor->setParseHTML(true);
		if (self->mCreatorID == gAgentID)
		{
			self->mDescEditor->setText(desc);
		}
		else
		{
			self->mDescEditor->appendColoredText(desc, false, false,
												 self->mDescEditor->getReadOnlyFgColor());
		}

		self->mSnapshotCtrl->setImageAssetID(snapshot_id);
		self->mLocationEditor->setText(location_text);
		self->mEnabledCheck->set(enabled);

		self->mSortOrderEditor->setText(llformat("%d", sort_order));
	}
}

void LLPanelPick::draw()
{
	refresh();

	LLPanel::draw();
}

void LLPanelPick::refresh()
{
	if (!mDataRequested)
	{
		sendPickInfoRequest();
	}

	// Check for god mode
	bool godlike = gAgent.isGodlike();
	bool is_self = gAgentID == mCreatorID;

	// Set button visibility/enablement appropriately
	if (mTopPick)
	{
		mSnapshotCtrl->setEnabled(godlike);
		mNameEditor->setEnabled(godlike);
		mDescEditor->setEnabled(godlike);

		mSortOrderText->setVisible(godlike);

		mSortOrderEditor->setVisible(godlike);
		mSortOrderEditor->setEnabled(godlike);

		mEnabledCheck->setVisible(godlike);
		mEnabledCheck->setEnabled(godlike);

		mSetBtn->setVisible(godlike);
		mSetBtn->setEnabled(godlike);
	}
	else
	{
		mSnapshotCtrl->setEnabled(is_self);
		mNameEditor->setEnabled(is_self);
		mDescEditor->setEnabled(is_self);

		mSortOrderText->setVisible(false);

		mSortOrderEditor->setVisible(false);
		mSortOrderEditor->setEnabled(false);

		mEnabledCheck->setVisible(false);
		mEnabledCheck->setEnabled(false);

		mSetBtn->setVisible(is_self);
		mSetBtn->setEnabled(is_self);
	}
}

//static
void LLPanelPick::onClickTeleport(void* data)
{
	LLPanelPick* self = (LLPanelPick*)data;
	if (self && !self->mPosGlobal.isExactlyZero())
	{
		gAgent.teleportViaLocation(self->mPosGlobal);
		if (gFloaterWorldMapp)
		{
			gFloaterWorldMapp->trackLocation(self->mPosGlobal);
		}
	}
}

//static
void LLPanelPick::onClickMap(void* data)
{
	LLPanelPick* self = (LLPanelPick*)data;
	if (self && gFloaterWorldMapp)
	{
		gFloaterWorldMapp->trackLocation(self->mPosGlobal);
		LLFloaterWorldMap::show(NULL, true);
	}
}

//static
void LLPanelPick::onClickSetLocation(void* data)
{
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		// Do not allow to set the location while under @showloc
		return;
	}
//mk

	LLPanelPick* self = (LLPanelPick*)data;
	if (!self) return;

	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp) return;

	self->mSimName = regionp->getName();
	self->mPosGlobal = gAgent.getPositionGlobal();

	std::string parcel_name;
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		self->mParcelID = parcel->getID();
		parcel_name = parcel->getName();
	}

	self->mLocationEditor->setText(createLocationText("", parcel_name,
													  self->mSimName,
													  self->mPosGlobal));
	onCommitAny(NULL, data);
}

//static
void LLPanelPick::onCommitAny(LLUICtrl* ctrl, void* data)
{
	LLPanelPick* self = (LLPanelPick*)data;
	// Have we received up to date data for this pick ?
	if (self && self->mDataReceived)
	{
		self->sendPickInfoUpdate();
		LLTabContainer* tab = (LLTabContainer*)self->getParent();
		if (tab)
		{
			tab->setCurrentTabName(self->mNameEditor->getText());
		}
	}
}

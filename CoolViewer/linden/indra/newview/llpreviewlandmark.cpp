/**
 * @file llpreviewlandmark.cpp
 * @brief LLPreviewLandmark class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llpreviewlandmark.h"

#include "llassetstorage.h"
#include "llinventory.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "lllandmarklist.h"
#include "llpanelplace.h"
#include "llviewerregion.h"

////////////////////////////////////////////////////////////////////////////
// LLPreviewLandmark

// static
LLPreviewLandmarkList LLPreviewLandmark::sOrderedInstances;

LLPreviewLandmark::LLPreviewLandmark(const std::string& name,
									 const LLRect& rect,
									 const std::string& title,
									 const LLUUID& item_uuid,
									 bool show_keep_discard,
									 LLViewerInventoryItem* inv_item)
:	LLPreview(name, LLRect(rect.mLeft, rect.mTop, rect.mRight, rect.mBottom),
			  title, item_uuid, LLUUID::null, false, 0, 0, inv_item),
	mLandmark(NULL)
{
	mFactoryMap["place_details_panel"] = LLCallbackMap(createPlaceDetail,
													   this);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_preview_landmark.xml",
												 &getFactoryMap());
	if (show_keep_discard)
	{
		childSetAction("Discard btn", onDiscardBtn, this);
	}
	else
	{
		childSetVisible("Discard btn", false);
	}

#if 0
	childSetCommitCallback("desc_editor", LLPreview::onText, this);
	childSetText("desc_editor", item->getDescription());
	childSetText("name_editor", item->getName());
	childSetPrevalidate("desc_editor",
						&LLLineEditor::prevalidatePrintableNotPipe);

	if (!getHost())
	{
		LLRect curRect = getRect();
		translate(rect.mLeft - curRect.mLeft, rect.mTop - curRect.mTop);
	}
#endif

	sOrderedInstances.push_back(this);
}

//virtual
LLPreviewLandmark::~LLPreviewLandmark()
{
	LLPreviewLandmarkList::iterator this_itr;
	this_itr = std::find(sOrderedInstances.begin(), sOrderedInstances.end(),
						 this);
	if (this_itr != sOrderedInstances.end())
	{
		sOrderedInstances.erase(this_itr);
	}
}

// Distance and direction from avatar to landmark.
// Distance is in meters; degrees is angle from east (north = 90)
void LLPreviewLandmark::getDegreesAndDist(F32* degrees, F64* horiz_dist,
										  F64* vert_dist) const
{
	LLVector3d pos;
	if (mLandmark && mLandmark->getGlobalPos(pos))
	{
		LLVector3d to_vec = pos - gAgent.getPositionGlobal();
		*horiz_dist = sqrt(to_vec.mdV[VX] * to_vec.mdV[VX] +
						   to_vec.mdV[VY] * to_vec.mdV[VY]);
		*vert_dist = to_vec.mdV[VZ];
		*degrees = RAD_TO_DEG * (F32)atan2(to_vec.mdV[VY], to_vec.mdV[VX]);
	}
}

std::string LLPreviewLandmark::getName() const
{
	const LLInventoryItem* item = getItem();
	if (item)
	{
		return item->getName();
	}
	return LLStringUtil::null;
}

LLVector3d LLPreviewLandmark::getPositionGlobal() const
{
	LLVector3d pos;
	if (mLandmark)
	{
		// we can safely ignore the return value here.
		(void)mLandmark->getGlobalPos(pos);
	}
	return pos;
}

// virtual
void LLPreviewLandmark::draw()
{
	const LLInventoryItem* item = getItem();

	if (item && !mLandmark)
	{
		mLandmark = gLandmarkList.getAsset(item->getAssetUUID());
		if (mLandmark && mPlacePanel)
		{
			// we always have these two:
			LLVector3 pos_region = mLandmark->getRegionPos();
			LLUUID landmark_asset_id = item->getAssetUUID();

			// Might be null UUID ?
			LLUUID region_id;
			mLandmark->getRegionID(region_id);

			// might be 0
			LLVector3d pos_global = getPositionGlobal();

			mPlacePanel->displayParcelInfo(pos_region, landmark_asset_id,
										   getItem()->getUUID(), region_id,
										   pos_global);
		}
	}

	LLPreview::draw();
}

// virtual
void LLPreviewLandmark::loadAsset()
{
	const LLInventoryItem* item = getItem();
	if (item && !mLandmark)
	{
		mLandmark = gLandmarkList.getAsset(item->getAssetUUID());
	}
	mAssetStatus = PREVIEW_ASSET_LOADING;
}

// virtual
LLPreview::EAssetStatus LLPreviewLandmark::getAssetStatus()
{
	const LLInventoryItem* item = getItem();
	if (item && gLandmarkList.assetExists(item->getAssetUUID()))
	{
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
	return mAssetStatus;
}

// static
void* LLPreviewLandmark::createPlaceDetail(void* userdata)
{
	LLPreviewLandmark* self = (LLPreviewLandmark*)userdata;
	self->mPlacePanel = new LLPanelPlace();
	LLUICtrlFactory::getInstance()->buildPanel(self->mPlacePanel,
											   "panel_place.xml");
	const LLInventoryItem* item = self->getItem();
	self->mPlacePanel->displayItemInfo(item);

	return self->mPlacePanel;
}

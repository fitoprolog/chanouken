/**
 * @file llfloaternamedesc.cpp
 * @brief LLFloaterNameDesc class implementation
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

#include "llfloaternamedesc.h"

#include "llassetstorage.h"
#include "lldir.h"
#include "llinventorytype.h"
#include "lllineeditor.h"
#include "llresizehandle.h"			// For RESIZE_HANDLE_WIDTH
#include "lluictrlfactory.h"

#include "llfloaterperms.h"
#include "llviewerassetupload.h"	// For upload_new_resource()
#include "llviewercontrol.h"
#include "llviewerwindow.h"			// For gViewerWindowp

constexpr S32 PREVIEW_LINE_HEIGHT = 19;
constexpr S32 PREVIEW_BORDER_WIDTH = 2;
constexpr S32 PREVIEW_RESIZE_HANDLE_SIZE = S32(RESIZE_HANDLE_WIDTH *
											   OO_SQRT2) +
										   PREVIEW_BORDER_WIDTH;
constexpr S32 PREVIEW_HPAD = PREVIEW_RESIZE_HANDLE_SIZE;

LLFloaterNameDesc::LLFloaterNameDesc(const std::string& filename, S32 cost)
:	LLFloater("Name/Description Floater"),
	mFilenameAndPath(filename),
	mCost(cost),
	mCheckTempAsset(false)
{
	mFilename = gDirUtilp->getBaseFileName(filename, false);
#if 0	// SL-5521 Maintain capitalization of filename when making the
		// inventory item. JC
	LLStringUtil::toLower(mFilename);
#endif
}

bool LLFloaterNameDesc::postBuild()
{
	LLRect r;

	std::string asset_name = mFilename;
	LLStringUtil::replaceNonstandardASCII(asset_name, '?');
	LLStringUtil::replaceChar(asset_name, '|', '?');
	LLStringUtil::stripNonprintable(asset_name);
	LLStringUtil::trim(asset_name);
	asset_name = gDirUtilp->getBaseFileName(asset_name, true);

	setTitle(mFilename);

	centerWithin(gViewerWindowp->getRootView()->getRect());

	S32 line_width = getRect().getWidth() - 2 * PREVIEW_HPAD;
	S32 y = getRect().getHeight() - PREVIEW_LINE_HEIGHT;

	r.setLeftTopAndSize(PREVIEW_HPAD, y, line_width, PREVIEW_LINE_HEIGHT);
	y -= PREVIEW_LINE_HEIGHT;

	r.setLeftTopAndSize(PREVIEW_HPAD, y, line_width, PREVIEW_LINE_HEIGHT);

	childSetCommitCallback("name_form", doCommit, this);
	childSetValue("name_form", LLSD(asset_name));

	LLLineEditor* name_editor = getChild<LLLineEditor>("name_form");
	if (name_editor)
	{
		name_editor->setMaxTextLength(DB_INV_ITEM_NAME_STR_LEN);
		name_editor->setPrevalidate(&LLLineEditor::prevalidatePrintableNotPipe);
	}

	y -= llfloor(PREVIEW_LINE_HEIGHT * 1.2f);
	y -= PREVIEW_LINE_HEIGHT;

	r.setLeftTopAndSize(PREVIEW_HPAD, y, line_width, PREVIEW_LINE_HEIGHT);
	childSetCommitCallback("description_form", doCommit, this);
	LLLineEditor* desc_editor = getChild<LLLineEditor>("description_form");
	if (desc_editor)
	{
		desc_editor->setMaxTextLength(DB_INV_ITEM_DESC_STR_LEN);
		desc_editor->setPrevalidate(&LLLineEditor::prevalidatePrintableNotPipe);
	}

	y -= llfloor(PREVIEW_LINE_HEIGHT * 1.2f);

	// Cancel button
	childSetAction("cancel_btn", onBtnCancel, this);

	// OK button
	childSetAction("ok_btn", onBtnOK, this);
	setDefaultBtn("ok_btn");

	return true;
}

LLFloaterNameDesc::~LLFloaterNameDesc()
{
	gFocusMgr.releaseFocusIfNeeded(this); // calls onCommit()
}

// Sub-classes should override this function if they allow editing
void LLFloaterNameDesc::onCommit()
{
}

//static
void LLFloaterNameDesc::doCommit(LLUICtrl*, void* userdata)
{
	LLFloaterNameDesc* self = (LLFloaterNameDesc*)userdata;
	if (self)
	{
		self->onCommit();
	}
}

//static
void LLFloaterNameDesc::onBtnOK(void* userdata)
{
	LLFloaterNameDesc* self =(LLFloaterNameDesc*)userdata;
	if (!self) return;

	self->childDisable("ok_btn"); // don't allow inadvertent extra uploads

	// Kinda hack: assumes that unsubclassed LLFloaterNameDesc is only used for
	// uploading chargeable assets, which it is right now (it's only used
	// unsubclassed for the sound upload dialog, and THAT should be a
	// subclass).
	std::string name = self->childGetValue("name_form").asString();
	std::string desc = self->childGetValue("description_form").asString();
	LLResourceUploadInfo::ptr_t
		info(new LLNewFileResourceUploadInfo(self->mFilenameAndPath,
											 name, desc, 0,
											 LLFolderType::FT_NONE,
											 LLInventoryType::IT_NONE,
											 LLFloaterPerms::getNextOwnerPerms(),
											 LLFloaterPerms::getGroupPerms(),
											 LLFloaterPerms::getEveryonePerms(),
											 self->mCost));
	bool temp_upload = false;
	if (self->mCheckTempAsset)
	{
		temp_upload = gSavedSettings.getBool("TemporaryUpload");
		gSavedSettings.setBool("TemporaryUpload", false);
	}
	upload_new_resource(info, NULL, NULL, temp_upload);

	self->close();
}

//static
void LLFloaterNameDesc::onBtnCancel(void* userdata)
{
	LLFloaterNameDesc* self =(LLFloaterNameDesc*)userdata;
	if (self)
	{
		self->close();
	}
}

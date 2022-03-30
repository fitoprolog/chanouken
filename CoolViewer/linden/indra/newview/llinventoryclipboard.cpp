/** 
 * @file llinventoryclipboard.cpp
 * @brief LLInventoryClipboard class implementation
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

#include "llinventoryclipboard.h"

#include "llinventorymodel.h"

LLInventoryClipboard gInventoryClipBoard;

LLInventoryClipboard::LLInventoryClipboard()
{
}

LLInventoryClipboard::~LLInventoryClipboard()
{
	reset();
}

void LLInventoryClipboard::add(const LLUUID& object)
{
	mObjects.push_back(object);
}

void LLInventoryClipboard::addCut(const LLUUID& object)
{
	mCutObjects.push_back(object);
}

// this stores a single inventory object
void LLInventoryClipboard::store(const LLUUID& object)
{
	reset();
	mObjects.push_back(object);
}

void LLInventoryClipboard::store(const uuid_vec_t& inv_objects)
{
	reset();
	for (S32 i = 0, count = inv_objects.size(); i < count; ++i)
	{
		mObjects.emplace_back(inv_objects[i]);
	}
}

void LLInventoryClipboard::retrieve(uuid_vec_t& inv_objects) const
{
	inv_objects.clear();
	LLUUID object_id;
	for (S32 i = 0, count = mObjects.size(); i < count; ++i)
	{
		object_id = mObjects[i];
		// Only add objects that have not since been purged...
		if (gInventory.getItem(object_id) ||
			gInventory.getCategory(object_id))
		{
			inv_objects.emplace_back(object_id);
		}
	}
}

void LLInventoryClipboard::retrieveCuts(uuid_vec_t& inv_objects) const
{
	inv_objects.clear();
	LLUUID object_id;
	for (S32 i = 0, count = mCutObjects.size(); i < count; ++i)
	{
		object_id = mCutObjects[i];
		// Only add objects that have not since been purged...
		if (gInventory.getItem(object_id) ||
			gInventory.getCategory(object_id))
		{
			inv_objects.emplace_back(object_id);
		}
	}
}

void LLInventoryClipboard::reset()
{
	mObjects.clear();
	mCutObjects.clear();
}

// returns true if the clipboard has something pasteable in it.
bool LLInventoryClipboard::hasContents() const
{
	return !mObjects.empty() || !mCutObjects.empty();
}

bool LLInventoryClipboard::hasCopiedContents() const
{
	return !mObjects.empty();
}

bool LLInventoryClipboard::hasCutContents() const
{
	return !mCutObjects.empty();
}

bool LLInventoryClipboard::isCopied(const LLUUID& id) const
{
	uuid_vec_t::const_iterator end = mObjects.end();
	return std::find(mObjects.begin(), end, id) != end;
}

bool LLInventoryClipboard::isCut(const LLUUID& id) const
{
	uuid_vec_t::const_iterator end = mCutObjects.end();
	return std::find(mCutObjects.begin(), end, id) != end;
}

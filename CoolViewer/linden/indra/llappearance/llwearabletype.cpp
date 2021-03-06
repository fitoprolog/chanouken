/**
 * @file llwearabletype.cpp
 * @brief LLWearableType class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#include "linden_common.h"

#include "llwearabletype.h"

#include "lldictionary.h"
#include "llinventory.h"

static LLTranslationBridge::ptr_t sTrans;

//static
void LLWearableType::initClass(LLTranslationBridge::ptr_t trans)
{
	sTrans = trans;
}

//static
void LLWearableType::cleanupClass()
{
	sTrans.reset();
}

struct WearableEntry : public LLDictionaryEntry
{
	WearableEntry(const std::string& name,
				  const std::string& default_new_name,
				  LLAssetType::EType assetType,
				  LLInventoryType::EIconName iconName,
				  bool disable_camera_switch = false,
				  bool allow_multiwear = true)
	:	LLDictionaryEntry(name),
		mAssetType(assetType),
		mDefaultNewName(default_new_name),
		mIconName(iconName),
		mDisableCameraSwitch(disable_camera_switch),
		mAllowMultiwear(allow_multiwear)
	{
		if (sTrans)
		{
			mLabel = sTrans->getString(name);
		}
		else
		{
			llwarns << "No translation bridge: WearableEntry '" << name
					<< "' will be left untranslated." << llendl;
		}
		if (mLabel.empty())
		{
			mLabel = name;
		}
	}

	const LLAssetType::EType	mAssetType;
	// Keeping mLabel for backward compatibility !
	std::string					mLabel;
	const std::string			mDefaultNewName;
	LLInventoryType::EIconName	mIconName;
	bool mDisableCameraSwitch;
	bool						mAllowMultiwear;
};

class LLWearableDictionary final : public LLDictionary<LLWearableType::EType,
													   WearableEntry>
{
public:
	LLWearableDictionary();
};

// Since it is a small structure, let's initialize it unconditionally (i.e.
// even if we do not log in) at global scope. This saves having to bother with
// a costly LLSingleton (slow, lot's of CPU cycles and cache lines wasted) or
// to find the right place where to construct the class on login...
LLWearableDictionary gWearableDictionary;

LLWearableDictionary::LLWearableDictionary()
{
	addEntry(LLWearableType::WT_SHAPE,        new WearableEntry("shape",       "New Shape",			LLAssetType::AT_BODYPART, 	LLInventoryType::ICONNAME_BODYPART_SHAPE, false, false));
	addEntry(LLWearableType::WT_SKIN,         new WearableEntry("skin",        "New Skin",			LLAssetType::AT_BODYPART, 	LLInventoryType::ICONNAME_BODYPART_SKIN, false, false));
	addEntry(LLWearableType::WT_HAIR,         new WearableEntry("hair",        "New Hair",			LLAssetType::AT_BODYPART, 	LLInventoryType::ICONNAME_BODYPART_HAIR, false, false));
	addEntry(LLWearableType::WT_EYES,         new WearableEntry("eyes",        "New Eyes",			LLAssetType::AT_BODYPART, 	LLInventoryType::ICONNAME_BODYPART_EYES, false, false));
	addEntry(LLWearableType::WT_SHIRT,        new WearableEntry("shirt",       "New Shirt",			LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_SHIRT, false, true));
	addEntry(LLWearableType::WT_PANTS,        new WearableEntry("pants",       "New Pants",			LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_PANTS, false, true));
	addEntry(LLWearableType::WT_SHOES,        new WearableEntry("shoes",       "New Shoes",			LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_SHOES, false, true));
	addEntry(LLWearableType::WT_SOCKS,        new WearableEntry("socks",       "New Socks",			LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_SOCKS, false, true));
	addEntry(LLWearableType::WT_JACKET,       new WearableEntry("jacket",      "New Jacket",		LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_JACKET, false, true));
	addEntry(LLWearableType::WT_GLOVES,       new WearableEntry("gloves",      "New Gloves",		LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_GLOVES, false, true));
	addEntry(LLWearableType::WT_UNDERSHIRT,   new WearableEntry("undershirt",  "New Undershirt",	LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_UNDERSHIRT, false, true));
	addEntry(LLWearableType::WT_UNDERPANTS,   new WearableEntry("underpants",  "New Underpants",	LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_UNDERPANTS, false, true));
	addEntry(LLWearableType::WT_SKIRT,        new WearableEntry("skirt",       "New Skirt",			LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_SKIRT, false, true));
	addEntry(LLWearableType::WT_ALPHA,        new WearableEntry("alpha",       "New Alpha",			LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_ALPHA, false, true));
	addEntry(LLWearableType::WT_TATTOO,       new WearableEntry("tattoo",      "New Tattoo",		LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_TATTOO, false, true));
	addEntry(LLWearableType::WT_UNIVERSAL,    new WearableEntry("universal",   "New Universal",		LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_UNIVERSAL, false, true));
	addEntry(LLWearableType::WT_PHYSICS,      new WearableEntry("physics",     "New Physics",		LLAssetType::AT_CLOTHING, 	LLInventoryType::ICONNAME_CLOTHING_PHYSICS, true, true));

	addEntry(LLWearableType::WT_INVALID,      new WearableEntry("invalid",     "Invalid Wearable", 	LLAssetType::AT_NONE, 		LLInventoryType::ICONNAME_NONE, false, false));
	addEntry(LLWearableType::WT_NONE,      	  new WearableEntry("none",        "Invalid Wearable", 	LLAssetType::AT_NONE, 		LLInventoryType::ICONNAME_NONE, false, false));
}

//static
LLWearableType::EType LLWearableType::typeNameToType(const std::string& type_name)
{
	const LLWearableType::EType wearable = gWearableDictionary.lookup(type_name);
	return wearable;
}

//static
const std::string& LLWearableType::getTypeName(LLWearableType::EType type)
{
	const WearableEntry* entry = gWearableDictionary.lookup(type);
	return entry ? entry->mName : getTypeName(WT_INVALID);
}

//static
std::string LLWearableType::getCapitalizedTypeName(LLWearableType::EType type)
{
	const WearableEntry* entry = gWearableDictionary.lookup(type);
	std::string name;
	if (entry)
	{
		name = entry->mName;
	}
	else
	{
		name = getTypeName(WT_INVALID);
	}
	name[0] = toupper(name[0]);
	return name;
}

//static
const std::string& LLWearableType::getTypeDefaultNewName(LLWearableType::EType type)
{
	const WearableEntry* entry = gWearableDictionary.lookup(type);
	return entry ? entry->mDefaultNewName : getTypeDefaultNewName(WT_INVALID);
}

//static
const std::string& LLWearableType::getTypeLabel(LLWearableType::EType type)
{
	const WearableEntry* entry = gWearableDictionary.lookup(type);
	return entry ? entry->mLabel : getTypeLabel(WT_INVALID);
}

//static
LLAssetType::EType LLWearableType::getAssetType(LLWearableType::EType type)
{
	const WearableEntry* entry = gWearableDictionary.lookup(type);
	return entry ? entry->mAssetType : getAssetType(WT_INVALID);
}

//static
LLInventoryType::EIconName LLWearableType::getIconName(LLWearableType::EType type)
{
	const WearableEntry* entry = gWearableDictionary.lookup(type);
	return entry ? entry->mIconName : getIconName(WT_INVALID);
}

//static
bool LLWearableType::getDisableCameraSwitch(LLWearableType::EType type)
{
	const WearableEntry* entry = gWearableDictionary.lookup(type);
	return entry && entry->mDisableCameraSwitch;
}

//static
bool LLWearableType::getAllowMultiwear(LLWearableType::EType type)
{
	const WearableEntry* entry = gWearableDictionary.lookup(type);
	return entry && entry->mAllowMultiwear;
}

//static
LLWearableType::EType LLWearableType::inventoryFlagsToWearableType(U32 flags)
{
	return (LLWearableType::EType)(flags & LLInventoryItem::II_FLAGS_SUBTYPE_MASK);
}

/**
 * @file llparcel.cpp
 * @brief A land parcel.
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

#include "linden_common.h"

#include <iostream>
#include <utility>

#include "indra_constants.h"

#include "llparcel.h"

#include "llmath.h"
#include "llregionflags.h"
#include "llsd.h"
#include "llsdutil.h"
#include "llsdutil_math.h"
#include "llstreamtools.h"
#include "lltransactiontypes.h"
#include "lltransactionflags.h"
#include "llmessage.h"

constexpr F32 SOME_BIG_NUMBER = 1000.0f;
constexpr F32 SOME_BIG_NEG_NUMBER = -1000.0f;
static const std::string PARCEL_OWNERSHIP_STATUS_STRING[LLParcel::OS_COUNT + 1] =
{
	"leased",
	"lease_pending",
	"abandoned",
	"none"
};

// NOTE: Adding parcel categories also requires updating:
// * floater_directory.xml category combobox
// * floater_about_land.xml category combobox
// * Web site "create event" tools
// DO NOT DELETE ITEMS FROM THIS LIST WITHOUT DEEPLY UNDERSTANDING WHAT YOU ARE
// DOING.
//
static const std::string PARCEL_CATEGORY_STRING[LLParcel::C_COUNT] =
{
	"none",
	"linden",
	"adult",
	"arts",
	"store", // "business" legacy name
	"educational",
	"game",	 // "gaming" legacy name
	"gather", // "hangout" legacy name
	"newcomer",
	"park",
	"home",	 // "residential" legacy name
	"shopping",
	"stage",
	"other",
	"rental"
};

static const std::string PARCEL_CATEGORY_UI_STRING[LLParcel::C_COUNT + 1] =
{
	"None",
	"Linden location",
	"Adult",
	"Arts and culture",
	"Business",
	"Educational",
	"Gaming",
	"Hangout",
	"Newcomer friendly",
	"Parks and nature",
	"Residential",
	"Shopping",
	"Stage",
	"Other",
	"Rental",
	"Any",	 // valid string for parcel searches
};

static const std::string PARCEL_ACTION_STRING[LLParcel::A_COUNT + 1] =
{
	"create",
	"release",
	"absorb",
	"absorbed",
	"divide",
	"division",
	"acquire",
	"relinquish",
	"confirm",
	"unknown"
};

const std::string& category_to_ui_string(LLParcel::ECategory category);
LLParcel::ECategory category_ui_string_to_category(const std::string& s);

LLParcel::LLParcel()
{
	init(LLUUID::null, true, false, false, 0, 0, 0, 0, 0, 1.f, 0);
}

LLParcel::LLParcel(const LLUUID& owner_id,
				   bool modify, bool terraform, bool damage,
				   time_t claim_date, S32 claim_price_per_meter,
				   S32 rent_price_per_meter, S32 area,
				   S32 sim_object_limit, F32 parcel_object_bonus,
				   bool is_group_owned)
{
	init(owner_id, modify, terraform, damage, claim_date,
		 claim_price_per_meter, rent_price_per_meter, area,
		 sim_object_limit, parcel_object_bonus, is_group_owned);
}

void LLParcel::init(const LLUUID& owner_id,
					bool modify, bool terraform, bool damage,
					time_t claim_date, S32 claim_price_per_meter,
					S32 rent_price_per_meter, S32 area,
					S32 sim_object_limit, F32 parcel_object_bonus,
					bool is_group_owned)
{
	mID.setNull();
	mOwnerID = owner_id;
	mGroupOwned = is_group_owned;
	mClaimDate = claim_date;
	mClaimPricePerMeter = claim_price_per_meter;
	mRentPricePerMeter = rent_price_per_meter;
	mArea = area;
	mDiscountRate = 1.0f;
	mDrawDistance = 512.f;

	mUserLookAt.set(0.0f, 0.f, 0.f);
	// Default to using the parcel's landing point, if any.
	mLandingType = L_LANDING_POINT;

	// *FIX: if owner_id != null, should be owned or sale pending,
	// investigate init callers.
	mStatus = OS_NONE;
	mCategory = C_NONE;
	mAuthBuyerID.setNull();
	//mBuyerID.setNull();
	//mJoinNeighbors = 0x0;
	mSaleTimerExpires.setTimerExpirySec(0);
	mSaleTimerExpires.stop();
	mGraceExtension = 0;

	mAuctionID = 0;
	mInEscrow = false;

	mParcelFlags = PF_DEFAULT;
	setParcelFlag(PF_CREATE_OBJECTS, modify);
	setParcelFlag(PF_ALLOW_TERRAFORM, terraform);
	setParcelFlag(PF_ALLOW_DAMAGE, damage);

	mSalePrice = 10000;
	setName(LLStringUtil::null);
	setDesc(LLStringUtil::null);
	setMusicURL(LLStringUtil::null);
	setMediaURL(LLStringUtil::null);
	setMediaDesc(LLStringUtil::null);
	setMediaType(LLStringUtil::null);
	mMediaID.setNull();
	mMediaAutoScale = false;
	mMediaLoop = true;
	mMediaWidth = 0;
	mMediaHeight = 0;
	setMediaCurrentURL(LLStringUtil::null);
	mMediaAllowNavigate = true;
	mMediaURLTimeout = 0.0f;
	mMediaPreventCameraZoom = false;

	mGroupID.setNull();

	mPassPrice = PARCEL_PASS_PRICE_DEFAULT;
	mPassHours = PARCEL_PASS_HOURS_DEFAULT;

	mAABBMin.set(SOME_BIG_NUMBER, SOME_BIG_NUMBER, SOME_BIG_NUMBER);
	mAABBMax.set(SOME_BIG_NEG_NUMBER, SOME_BIG_NEG_NUMBER,
				 SOME_BIG_NEG_NUMBER);

	mLocalID = INVALID_PARCEL_ID;

	setMaxPrimCapacity((S32)(sim_object_limit *
							 area / (F32)(REGION_WIDTH_METERS *
										  REGION_WIDTH_METERS)));
	setSimWideMaxPrimCapacity(0);
	setSimWidePrimCount(0);
	setOwnerPrimCount(0);
	setGroupPrimCount(0);
	setOtherPrimCount(0);
	setSelectedPrimCount(0);
	setTempPrimCount(0);
	setCleanOtherTime(0);
	setRegionPushOverride(false);
	setRegionDenyAnonymousOverride(false);
	setRegionDenyAgeUnverifiedOverride(false);
	setParcelPrimBonus(parcel_object_bonus);

	setPreviousOwnerID(LLUUID::null);
	setPreviouslyGroupOwned(false);

	setSeeAVs(true);
	setAllowGroupAVSounds(true);
	setAllowAnyAVSounds(true);
	setHaveNewParcelLimitData(false);

	setRegionAllowEnvironmentOverride(false);
	setParcelEnvironmentVersion(INVALID_PARCEL_ENVIRONMENT_VERSION);
}

void LLParcel::overrideOwner(const LLUUID& owner_id, bool is_group_owned)
{
	// Override with system permission (LLUUID::null)
	// Overridden parcels have no group
	mOwnerID = owner_id;
	mGroupOwned = is_group_owned;
	if (mGroupOwned)
	{
		mGroupID = mOwnerID;
	}
	else
	{
		mGroupID.setNull();
	}
	mInEscrow = false;
}

void LLParcel::setName(const std::string& name)
{
	// The escaping here must match the escaping in the database
	// abstraction layer.
	mName = name;
	LLStringFn::replace_nonprintable_in_ascii(mName, LL_UNKNOWN_CHAR);
}

void LLParcel::setDesc(const std::string& desc)
{
	// The escaping here must match the escaping in the database
	// abstraction layer.
	mDesc = desc;
	mDesc = rawstr_to_utf8(mDesc);
}

void LLParcel::setMusicURL(const std::string& url)
{
	mMusicURL = url;
	// The escaping here must match the escaping in the database
	// abstraction layer.
	// This should really filter the url in some way. Other than
	// simply requiring non-printable.
	LLStringFn::replace_nonprintable_in_ascii(mMusicURL, LL_UNKNOWN_CHAR);
}

void LLParcel::setMediaURL(const std::string& url)
{
	mMediaURL = url;
	// The escaping here must match the escaping in the database
	// abstraction layer if it's ever added.
	// This should really filter the url in some way. Other than
	// simply requiring non-printable.
	LLStringFn::replace_nonprintable_in_ascii(mMediaURL, LL_UNKNOWN_CHAR);
}

void LLParcel::setMediaDesc(const std::string& desc)
{
	// The escaping here must match the escaping in the database
	// abstraction layer.
	mMediaDesc = desc;
	mMediaDesc = rawstr_to_utf8(mMediaDesc);
}

void LLParcel::setMediaType(const std::string& type)
{
	// The escaping here must match the escaping in the database
	// abstraction layer.
	mMediaType = type;
	mMediaType = rawstr_to_utf8(mMediaType);

	// This code attempts to preserve legacy movie functioning
	if (mMediaType.empty() && !mMediaURL.empty())
	{
		setMediaType(std::string("video/vnd.secondlife.qt.legacy"));
	}
}

void LLParcel::setMediaWidth(S32 width)
{
	mMediaWidth = width;
}

void LLParcel::setMediaHeight(S32 height)
{
	mMediaHeight = height;
}

void LLParcel::setMediaCurrentURL(const std::string& url)
{
	mMediaCurrentURL = url;
	// The escaping here must match the escaping in the database
	// abstraction layer if it's ever added.
	// This should really filter the url in some way. Other than
	// simply requiring non-printable.
	LLStringFn::replace_nonprintable_in_ascii(mMediaCurrentURL, LL_UNKNOWN_CHAR);

}

void LLParcel::setMediaURLResetTimer(F32 time)
{
	mMediaResetTimer.start();
	mMediaResetTimer.setTimerExpirySec(time);
}

void LLParcel::setParcelFlag(U32 flag, bool b)
{
	if (b)
	{
		mParcelFlags |= flag;
	}
	else
	{
		mParcelFlags &= ~flag;
	}
}

bool LLParcel::allowModifyBy(const LLUUID& agent_id,
							 const LLUUID& group_id) const
{
	if (agent_id.isNull())
	{
		// system always can enter
		return true;
	}
	else if (isPublic())
	{
		return true;
	}
	else if (agent_id == mOwnerID)
	{
		// owner can always perform operations
		return true;
	}
	else if (mParcelFlags & PF_CREATE_OBJECTS)
	{
		return true;
	}
	else if ((mParcelFlags & PF_CREATE_GROUP_OBJECTS) && group_id.notNull())
	{
		return (getGroupID() == group_id);
	}

	return false;
}

bool LLParcel::allowTerraformBy(const LLUUID& agent_id) const
{
	if (agent_id.isNull())
	{
		// system always can enter
		return true;
	}
	else if (OS_LEASED == mStatus)
	{
		if (agent_id == mOwnerID)
		{
			// owner can modify leased land
			return true;
		}
		else
		{
			// otherwise check other people
			return mParcelFlags & PF_ALLOW_TERRAFORM;
		}
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------
// File input and output
//-----------------------------------------------------------

bool LLParcel::importAccessEntry(std::istream& input_stream,
								 LLAccessEntry* entry)
{
	skip_to_end_of_next_keyword("{", input_stream);
	while (input_stream.good())
	{
		skip_comments_and_emptyspace(input_stream);
		std::string line, keyword, value;
		get_line(line, input_stream, MAX_STRING);
		get_keyword_and_value(keyword, value, line);

		if (keyword == "}")
		{
			break;
		}
		else if (keyword == "id")
		{
			entry->mID.set(value);
		}
		else if (keyword == "time")
		{
			S32 when = 0;
			LLStringUtil::convertToS32(value, when);
			entry->mTime = when;
		}
		else if (keyword == "flags")
		{
			U32 setting = 0;
			LLStringUtil::convertToU32(value, setting);
			entry->mFlags = setting;
		}
		else if (keyword != "name")	// "name" is deprecated: ignore entirely.
		{
			llwarns << "Unknown keyword in parcel access entry section: <"
					<< keyword << ">" << llendl;
		}
	}
	return input_stream.good();
}

// Assumes we are in a block "ParcelData"
void LLParcel::packMessage(LLMessageSystem* msg)
{
	msg->addU32Fast(_PREHASH_ParcelFlags, getParcelFlags());
	msg->addS32Fast(_PREHASH_SalePrice, getSalePrice());
	msg->addStringFast(_PREHASH_Name, getName());
	msg->addStringFast(_PREHASH_Desc,  getDesc());
	msg->addStringFast(_PREHASH_MusicURL, getMusicURL());
	msg->addStringFast(_PREHASH_MediaURL, getMediaURL());
	msg->addU8("MediaAutoScale", (U8)getMediaAutoScale());
	msg->addUUIDFast(_PREHASH_MediaID, getMediaID());
	msg->addUUIDFast(_PREHASH_GroupID, getGroupID());
	msg->addS32Fast(_PREHASH_PassPrice, mPassPrice);
	msg->addF32Fast(_PREHASH_PassHours, mPassHours);
	msg->addU8Fast(_PREHASH_Category, (U8)mCategory);
	msg->addUUIDFast(_PREHASH_AuthBuyerID, mAuthBuyerID);
	msg->addUUIDFast(_PREHASH_SnapshotID, mSnapshotID);
	msg->addVector3Fast(_PREHASH_UserLocation, mUserLocation);
	msg->addVector3Fast(_PREHASH_UserLookAt, mUserLookAt);
	msg->addU8Fast(_PREHASH_LandingType, (U8)mLandingType);
}

// Assumes we are in a block "ParcelData"
void LLParcel::packMessage(LLSD& msg)
{
	// used in the viewer, the sim uses it's own packer
	msg["local_id"] = getLocalID();
	msg["parcel_flags"] = ll_sd_from_U32(getParcelFlags());
	msg["sale_price"] = getSalePrice();
	msg["name"] = getName();
	msg["description"] = getDesc();
	msg["music_url"] = getMusicURL();
	msg["media_url"] = getMediaURL();
	msg["media_desc"] = getMediaDesc();
	msg["media_type"] = getMediaType();
	msg["media_width"] = getMediaWidth();
	msg["media_height"] = getMediaHeight();
	msg["auto_scale"] = getMediaAutoScale();
	msg["media_loop"] = getMediaLoop();
	msg["media_current_url"] = getMediaCurrentURL();
	msg["obscure_media"] = false; // OBSOLETE - no longer used
	msg["obscure_music"] = false; // OBSOLETE - no longer used
	msg["media_id"] = getMediaID();
	msg["media_allow_navigate"] = getMediaAllowNavigate();
	msg["media_prevent_camera_zoom"] = getMediaPreventCameraZoom();
	msg["media_url_timeout"] = getMediaURLTimeout();
	msg["group_id"] = getGroupID();
	msg["pass_price"] = mPassPrice;
	msg["pass_hours"] = mPassHours;
	msg["category"] = (U8)mCategory;
	msg["auth_buyer_id"] = mAuthBuyerID;
	msg["snapshot_id"] = mSnapshotID;
	msg["user_location"] = ll_sd_from_vector3(mUserLocation);
	msg["user_look_at"] = ll_sd_from_vector3(mUserLookAt);
	msg["landing_type"] = (U8)mLandingType;
	msg["see_avs"] = (LLSD::Boolean) getSeeAVs();
	msg["group_av_sounds"] = (LLSD::Boolean)getAllowGroupAVSounds();
	msg["any_av_sounds"] = (LLSD::Boolean)getAllowAnyAVSounds();
}

void LLParcel::unpackMessage(LLMessageSystem* msg)
{
	std::string buffer;

	msg->getU32Fast(_PREHASH_ParcelData,_PREHASH_ParcelFlags, mParcelFlags);
	msg->getS32Fast(_PREHASH_ParcelData,_PREHASH_SalePrice, mSalePrice);
	msg->getStringFast(_PREHASH_ParcelData,_PREHASH_Name, buffer);
	setName(buffer);
	msg->getStringFast(_PREHASH_ParcelData,_PREHASH_Desc, buffer);
	setDesc(buffer);
	msg->getStringFast(_PREHASH_ParcelData,_PREHASH_MusicURL, buffer);
	setMusicURL(buffer);
	msg->getStringFast(_PREHASH_ParcelData,_PREHASH_MediaURL, buffer);
	setMediaURL(buffer);

	// All default to true for legacy server behavior:
	bool see_avs = true;
	bool any_av_sounds = true;
	bool group_av_sounds = true;
	// New version of server should send all 3 of these values:
	bool have_new_parcel_limit_data = msg->getSizeFast(_PREHASH_ParcelData,
													   _PREHASH_SeeAVs) > 0;
	have_new_parcel_limit_data &= msg->getSizeFast(_PREHASH_ParcelData,
												   _PREHASH_AnyAVSounds) > 0;
	have_new_parcel_limit_data &= msg->getSizeFast(_PREHASH_ParcelData,
												   _PREHASH_GroupAVSounds) > 0;
	if (have_new_parcel_limit_data)
	{
		msg->getBoolFast(_PREHASH_ParcelData, _PREHASH_SeeAVs, see_avs);
		msg->getBoolFast(_PREHASH_ParcelData, _PREHASH_AnyAVSounds,
						 any_av_sounds);
		msg->getBoolFast(_PREHASH_ParcelData, _PREHASH_GroupAVSounds,
						 group_av_sounds);
	}
	setSeeAVs(see_avs);
	setAllowAnyAVSounds(any_av_sounds);
	setAllowGroupAVSounds(group_av_sounds);

	setHaveNewParcelLimitData(have_new_parcel_limit_data);

	// non-optimized version
	U8 temp;
	msg->getU8("ParcelData", "MediaAutoScale", temp);
	mMediaAutoScale = (bool)temp;

	msg->getUUIDFast(_PREHASH_ParcelData,_PREHASH_MediaID, mMediaID);
	msg->getUUIDFast(_PREHASH_ParcelData,_PREHASH_GroupID, mGroupID);
	msg->getS32Fast(_PREHASH_ParcelData,_PREHASH_PassPrice, mPassPrice);
	msg->getF32Fast(_PREHASH_ParcelData,_PREHASH_PassHours, mPassHours);
	U8 category;
	msg->getU8Fast(_PREHASH_ParcelData,_PREHASH_Category, category);
	mCategory = (ECategory)category;
	msg->getUUIDFast(_PREHASH_ParcelData,_PREHASH_AuthBuyerID, mAuthBuyerID);
	msg->getUUIDFast(_PREHASH_ParcelData,_PREHASH_SnapshotID, mSnapshotID);
	msg->getVector3Fast(_PREHASH_ParcelData,_PREHASH_UserLocation,
						mUserLocation);
	msg->getVector3Fast(_PREHASH_ParcelData,_PREHASH_UserLookAt, mUserLookAt);
	U8 landing_type;
	msg->getU8Fast(_PREHASH_ParcelData,_PREHASH_LandingType, landing_type);
	mLandingType = (ELandingType)landing_type;

	// New Media Data
	// Note: the message has been converted to TCP
	if (msg->has("MediaData"))
	{
		msg->getString("MediaData", "MediaDesc", buffer);
		setMediaDesc(buffer);
		msg->getString("MediaData", "MediaType", buffer);
		setMediaType(buffer);
		msg->getS32("MediaData", "MediaWidth", mMediaWidth);
		msg->getS32("MediaData", "MediaHeight", mMediaHeight);
		U8 temp;
		msg->getU8("MediaData", "MediaLoop", temp);
		mMediaLoop = (bool)temp;
		// the ObscureMedia and ObscureMusic flags previously set here are no
		// longer used
	}
	else
	{
		setMediaType(std::string("video/vnd.secondlife.qt.legacy"));
		setMediaDesc(std::string("No Description available without Server Upgrade"));
		mMediaLoop = true;
	}

	if (msg->getNumberOfBlocks("MediaLinkSharing") > 0)
	{
		msg->getString("MediaLinkSharing", "MediaCurrentURL", buffer);
		setMediaCurrentURL(buffer);
		msg->getU8("MediaLinkSharing", "MediaAllowNavigate",
				   mMediaAllowNavigate);
		msg->getU8("MediaLinkSharing", "MediaPreventCameraZoom",
				   mMediaPreventCameraZoom);
		msg->getF32("MediaLinkSharing", "MediaURLTimeout", mMediaURLTimeout);
	}
	else
	{
		setMediaCurrentURL(LLStringUtil::null);
	}
}

void LLParcel::packAccessEntries(LLMessageSystem* msg,
								 const access_map_t& list)
{
	access_map_t::const_iterator cit = list.begin();
	access_map_t::const_iterator end = list.end();

	if (cit == end)
	{
		msg->nextBlockFast(_PREHASH_List);
		msg->addUUIDFast(_PREHASH_ID, LLUUID::null);
		msg->addS32Fast(_PREHASH_Time, 0);
		msg->addU32Fast(_PREHASH_Flags, 0);
		return;
	}

	for ( ; cit != end; ++cit)
	{
		const LLAccessEntry& entry = cit->second;
		msg->nextBlockFast(_PREHASH_List);
		msg->addUUIDFast(_PREHASH_ID, entry.mID);
		msg->addS32Fast(_PREHASH_Time, entry.mTime);
		msg->addU32Fast(_PREHASH_Flags, entry.mFlags);
	}
}

void LLParcel::unpackAccessEntries(LLMessageSystem* msg, access_map_t* list)
{
	LLUUID id;
	S32 time;
	U32 flags;

	S32 count = msg->getNumberOfBlocksFast(_PREHASH_List);
	for (S32 i = 0; i < count; ++i)
	{
		msg->getUUIDFast(_PREHASH_List, _PREHASH_ID, id, i);
		msg->getS32Fast(_PREHASH_List, _PREHASH_Time, time, i);
		msg->getU32Fast(_PREHASH_List, _PREHASH_Flags, flags, i);

		if (id.notNull())
		{
			// C++11 is not the most limpid language... This whole
			// gymnastic with piecewise_construct and forward_as_tuple is
			// here to ensure LLAccessEntry is constructed emplace, and
			// not just the UUID key of the map.
			list->emplace(std::piecewise_construct,
						  std::forward_as_tuple(id),
						  std::forward_as_tuple(id, time, flags));
		}
	}
}

void LLParcel::unpackExperienceEntries(LLMessageSystem* msg, U32 type)
{
	LLUUID id;
	for (S32 i = 0, count = msg->getNumberOfBlocksFast(_PREHASH_List);
		 i < count; ++i)
	{
		msg->getUUIDFast(_PREHASH_List, _PREHASH_ID, id, i);
		if (id.notNull())
		{
			mExperienceKeys.emplace(id, type);
		}
	}
}

void LLParcel::expirePasses(S32 now)
{
	access_map_t::iterator it = mAccessList.begin();
	while (it != mAccessList.end())
	{
		const LLAccessEntry& entry = it->second;
		if (entry.mTime != 0 && entry.mTime < now)
		{
			mAccessList.hmap_erase(it++);
		}
		else
		{
			++it;
		}
	}
}

access_map_t LLParcel::getExperienceKeysByType(U32 type) const
{
	access_map_t access;
	LLAccessEntry entry;
	for (xp_type_map_t::const_iterator it = mExperienceKeys.begin(),
									   end = mExperienceKeys.end();
		 it != end; ++it)
	{
		if (it->second == type)
		{
			entry.mID = it->first;
			access.emplace(entry.mID, entry);
		}
	}
	return access;
}

void LLParcel::clearExperienceKeysByType(U32 type)
{
	xp_type_map_t::iterator it = mExperienceKeys.begin();
	while (it != mExperienceKeys.end())
	{
		if (it->second == type)
		{
			mExperienceKeys.hmap_erase(it++);
		}
		else
		{
			++it;
		}
	}
}

void LLParcel::setExperienceKeyType(const LLUUID& experience_key,
									U32 type)
{
	if (type == EXPERIENCE_KEY_TYPE_NONE)
	{
		mExperienceKeys.erase(experience_key);
	}
	else if (countExperienceKeyType(type) < (U32)PARCEL_MAX_EXPERIENCE_LIST)
	{
		mExperienceKeys.emplace(experience_key, type);
	}
}

U32 LLParcel::countExperienceKeyType(U32 type) const
{
	U32 count = 0;
	for (xp_type_map_t::const_iterator it = mExperienceKeys.begin(),
									   end = mExperienceKeys.end();
		 it != end; ++it)
	{
		if (it->second == type)
		{
			++count;
		}
	}
	return count;
}

bool LLParcel::operator==(const LLParcel& rhs) const
{
   return mOwnerID == rhs.mOwnerID && mParcelFlags == rhs.mParcelFlags &&
		  mClaimDate == rhs.mClaimDate &&
		  mClaimPricePerMeter == rhs.mClaimPricePerMeter &&
		  mRentPricePerMeter == rhs.mRentPricePerMeter;
}

// Calculates rent
S32 LLParcel::getTotalRent() const
{
	return (S32)floor(0.5f + (F32)mArea * (F32)mRentPricePerMeter *
					  (1.0f - mDiscountRate));
}

F32 LLParcel::getAdjustedRentPerMeter() const
{
	return ((F32)mRentPricePerMeter * (1.0f - mDiscountRate));
}

LLVector3 LLParcel::getCenterpoint() const
{
	LLVector3 rv;
	rv.mV[VX] = (getAABBMin().mV[VX] + getAABBMax().mV[VX]) * 0.5f;
	rv.mV[VY] = (getAABBMin().mV[VY] + getAABBMax().mV[VY]) * 0.5f;
	rv.mV[VZ] = 0.0f;
	return rv;
}

void LLParcel::extendAABB(const LLVector3& box_min, const LLVector3& box_max)
{
	// Patch up min corner of AABB
	S32 i;
	for (i = 0; i < 3; ++i)
	{
		if (box_min.mV[i] < mAABBMin.mV[i])
		{
			mAABBMin.mV[i] = box_min.mV[i];
		}
	}

	// Patch up max corner of AABB
	for (i = 0; i < 3; ++i)
	{
		if (box_max.mV[i] > mAABBMax.mV[i])
		{
			mAABBMax.mV[i] = box_max.mV[i];
		}
	}
}

bool LLParcel::addToAccessList(const LLUUID& agent_id, S32 time)
{
	if (mAccessList.size() >= (U32)PARCEL_MAX_ACCESS_LIST)
	{
		return false;
	}
	if (agent_id == getOwnerID())
	{
		// Cannot add owner to these lists
		return false;
	}
	access_map_t::iterator itor = mAccessList.begin();
	while (itor != mAccessList.end())
	{
		const LLAccessEntry& entry = itor->second;
		if (entry.mID == agent_id)
		{
			if (time == 0 || (entry.mTime != 0 && entry.mTime < time))
			{
				mAccessList.hmap_erase(itor++);
			}
			else
			{
				LL_DEBUGS("ParcelAccess") << "Agent " << agent_id
										  << " already in access list ("
										  << (time ? "temporary" : "permanent")
										  << " access)." << LL_ENDL;
				// Existing one expires later
				return false;
			}
		}
		else
		{
			++itor;
		}
	}

	removeFromBanList(agent_id);

	LL_DEBUGS("ParcelAccess") << "Adding agent " << agent_id << " to access list ("
							  << (time ? "temporary" : "permanent")
							  << " access)." << LL_ENDL;
	LLAccessEntry new_entry;
	new_entry.mID = agent_id;
	new_entry.mTime = time;
	new_entry.mFlags = 0x0;
	mAccessList[agent_id] = new_entry;

	return true;
}

bool LLParcel::addToBanList(const LLUUID& agent_id, S32 time)
{
	if (mBanList.size() >= (U32)PARCEL_MAX_ACCESS_LIST)
	{
		// Not using ban list, so not a rational thing to do
		return false;
	}
	if (agent_id == getOwnerID())
	{
		// Cannot add owner to these lists
		return false;
	}

	access_map_t::iterator itor = mBanList.begin();
	while (itor != mBanList.end())
	{
		const LLAccessEntry& entry = itor->second;
		if (entry.mID == agent_id)
		{
			if (time == 0 || (entry.mTime != 0 && entry.mTime < time))
			{
				mBanList.hmap_erase(itor++);
			}
			else
			{
				LL_DEBUGS("ParcelAccess") << "Agent " << agent_id
										  << " already in ban list ("
										  << (time ? "temporary" : "permanent")
										  << " ban)." << LL_ENDL;
				// Existing one expires later
				return false;
			}
		}
		else
		{
			++itor;
		}
	}

	removeFromAccessList(agent_id);

	LL_DEBUGS("ParcelAccess") << "Adding agent " << agent_id << " to ban list ("
							  << (time ? "temporary" : "permanent") << " ban)."
							  << LL_ENDL;
	LLAccessEntry new_entry;
	new_entry.mID = agent_id;
	new_entry.mTime = time;
	new_entry.mFlags = 0x0;
	mBanList[agent_id] = new_entry;

	return true;
}

bool remove_from_access_array(access_map_t* list, const LLUUID& agent_id)
{
	bool removed = false;
	access_map_t::iterator it = list->begin();
	while (it != list->end())
	{
		const LLAccessEntry& entry = it->second;
		if (entry.mID == agent_id)
		{
			list->hmap_erase(it++);
			removed = true;
		}
		else
		{
			++it;
		}
	}
	return removed;
}

bool LLParcel::removeFromAccessList(const LLUUID& agent_id)
{
	return remove_from_access_array(&mAccessList, agent_id);
}

bool LLParcel::removeFromBanList(const LLUUID& agent_id)
{
	return remove_from_access_array(&mBanList, agent_id);
}

//static
const std::string& LLParcel::getOwnershipStatusString(EOwnershipStatus status)
{
	return ownership_status_to_string(status);
}

//static
const std::string& LLParcel::getCategoryString(ECategory category)
{
	return category_to_string(category);
}

//static
const std::string& LLParcel::getCategoryUIString(ECategory category)
{
	return category_to_ui_string(category);
}

//static
LLParcel::ECategory LLParcel::getCategoryFromString(const std::string& string)
{
	return category_string_to_category(string);
}

//static
LLParcel::ECategory LLParcel::getCategoryFromUIString(const std::string& string)
{
	return category_ui_string_to_category(string);
}

//static
const std::string& LLParcel::getActionString(LLParcel::EAction action)
{
	S32 index = 0;
	if (action >= 0 && action < LLParcel::A_COUNT)
	{
		index = action;
	}
	else
	{
		index = A_COUNT;
	}
	return PARCEL_ACTION_STRING[index];
}

bool LLParcel::isSaleTimerExpired(const U64& time)
{
	if (!mSaleTimerExpires.getStarted())
	{
		return false;
	}
	bool expired = mSaleTimerExpires.checkExpirationAndReset(0.0);
	if (expired)
	{
		mSaleTimerExpires.stop();
	}
	return expired;
}

bool LLParcel::isMediaResetTimerExpired(const U64& time)
{
	if (!mMediaResetTimer.getStarted())
	{
		return false;
	}
	bool expired = mMediaResetTimer.checkExpirationAndReset(0.0);
	if (expired)
	{
		mMediaResetTimer.stop();
	}
	return expired;
}

void LLParcel::startSale(const LLUUID& buyer_id, bool is_buyer_group)
{
	// *TODO: this and all Sale related methods need to move out of the
	// LLParcel base class and into server-side-only LLSimParcel class
	setPreviousOwnerID(mOwnerID);
	setPreviouslyGroupOwned(mGroupOwned);

	mOwnerID = buyer_id;
	mGroupOwned = is_buyer_group;
	if (mGroupOwned)
	{
		mGroupID = mOwnerID;
	}
	else
	{
		mGroupID.setNull();
	}
	mSaleTimerExpires.start();
	mSaleTimerExpires.setTimerExpirySec(DEFAULT_USEC_SALE_TIMEOUT /
										SEC_TO_MICROSEC);
	mStatus = OS_LEASE_PENDING;
	mClaimDate = time(NULL);
	setAuctionID(0);
	// clear the autoreturn whenever land changes hands
	setCleanOtherTime(0);
}

void LLParcel::expireSale(U32& type, U8& flags, LLUUID& from_id, LLUUID& to_id)
{
	mSaleTimerExpires.setTimerExpirySec(0.0);
	mSaleTimerExpires.stop();
	setPreviousOwnerID(LLUUID::null);
	setPreviouslyGroupOwned(false);
	setSellWithObjects(false);
	type = TRANS_LAND_RELEASE;
	mStatus = OS_NONE;
	flags = pack_transaction_flags(mGroupOwned, false);
	mAuthBuyerID.setNull();
	from_id = mOwnerID;
	mOwnerID.setNull();
	to_id.setNull();
}

void LLParcel::completeSale(U32& type, U8& flags, LLUUID& to_id)
{
	mSaleTimerExpires.setTimerExpirySec(0.0);
	mSaleTimerExpires.stop();
	mStatus = OS_LEASED;
	type = TRANS_LAND_SALE;
	flags = pack_transaction_flags(mGroupOwned, mGroupOwned);
	to_id = mOwnerID;
	mAuthBuyerID.setNull();

	// Purchased parcels are assumed to no longer be for sale.
	// Otherwise someone can snipe the sale.
	setForSale(false);
	setAuctionID(0);

	// Turn off show directory, since it's a recurring fee that
	// the buyer may not want.
	setParcelFlag(PF_SHOW_DIRECTORY, false);

	//should be cleared on sale.
	mAccessList.clear();
	mBanList.clear();
}

void LLParcel::clearSale()
{
	mSaleTimerExpires.setTimerExpirySec(0.0);
	mSaleTimerExpires.stop();
	if (isPublic())
	{
		mStatus = OS_NONE;
	}
	else
	{
		mStatus = OS_LEASED;
	}
	mAuthBuyerID.setNull();
	setForSale(false);
	setAuctionID(0);
	setPreviousOwnerID(LLUUID::null);
	setPreviouslyGroupOwned(false);
	setSellWithObjects(false);
}

void LLParcel::clearParcel()
{
	overrideParcelFlags(PF_DEFAULT);
	setName(LLStringUtil::null);
	setDesc(LLStringUtil::null);
	setMediaURL(LLStringUtil::null);
	setMediaType(LLStringUtil::null);
	setMediaID(LLUUID::null);
	setMediaDesc(LLStringUtil::null);
	setMediaAutoScale(false);
	setMediaLoop(true);
	mMediaWidth = 0;
	mMediaHeight = 0;
	setMediaCurrentURL(LLStringUtil::null);
	setMediaAllowNavigate(true);
	setMediaPreventCameraZoom(false);
	setMediaURLTimeout(0.0f);
	setMusicURL(LLStringUtil::null);
	setInEscrow(false);
	setAuthorizedBuyerID(LLUUID::null);
	setCategory(C_NONE);
	setSnapshotID(LLUUID::null);
	setUserLocation(LLVector3::zero);
	setUserLookAt(LLVector3::x_axis);
	setLandingType(L_LANDING_POINT);
	setAuctionID(0);
	setGroupID(LLUUID::null);
	setPassPrice(0);
	setPassHours(0.f);
	mAccessList.clear();
	mBanList.clear();
}

void LLParcel::dump()
{
	llinfos << "Parcel: " << mLocalID << " - Area: " << mArea << " - Name: "
			<< mName << " -  Description: " << mDesc << llendl;
}

const std::string& ownership_status_to_string(LLParcel::EOwnershipStatus status)
{
	if (status >= 0 && status < LLParcel::OS_COUNT)
	{
		return PARCEL_OWNERSHIP_STATUS_STRING[status];
	}
	return PARCEL_OWNERSHIP_STATUS_STRING[LLParcel::OS_COUNT];
}

LLParcel::EOwnershipStatus ownership_string_to_status(const std::string& s)
{
	for (S32 i = 0; i < LLParcel::OS_COUNT; ++i)
	{
		if (s == PARCEL_OWNERSHIP_STATUS_STRING[i])
		{
			return (LLParcel::EOwnershipStatus)i;
		}
	}
	return LLParcel::OS_NONE;
}

const std::string& category_to_string(LLParcel::ECategory category)
{
	S32 index = 0;
	if (category >= 0 && category < LLParcel::C_COUNT)
	{
		index = category;
	}
	return PARCEL_CATEGORY_STRING[index];
}

const std::string& category_to_ui_string(LLParcel::ECategory category)
{
	S32 index = 0;
	if (category >= 0 && category < LLParcel::C_COUNT)
	{
		index = category;
	}
	else
	{
		// C_ANY = -1 , but the "Any" string is at the end of the list
		index = (S32)LLParcel::C_COUNT;
	}
	return PARCEL_CATEGORY_UI_STRING[index];
}

LLParcel::ECategory category_string_to_category(const std::string& s)
{
	if (s.empty())
	{
		return LLParcel::C_NONE;
	}

	for (S32 i = 0; i < LLParcel::C_COUNT; ++i)
	{
		if (s == PARCEL_CATEGORY_STRING[i])
		{
			return (LLParcel::ECategory)i;
		}
	}

	llwarns << "Parcel category outside of possibilities: " << s << llendl;
	return LLParcel::C_NONE;
}

LLParcel::ECategory category_ui_string_to_category(const std::string& s)
{
	for (S32 i = 0; i < LLParcel::C_COUNT; ++i)
	{
		if (s == PARCEL_CATEGORY_UI_STRING[i])
		{
			return (LLParcel::ECategory)i;
		}
	}

	// "Any" is a valid category for searches, and is a distinct option from
	// "None" and "Other"
	return LLParcel::C_ANY;
}

/**
 * @file llvisualparam.cpp
 * @brief Implementation of LLPolyMesh class.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

//-----------------------------------------------------------------------------
// Header Files
//-----------------------------------------------------------------------------
#include "linden_common.h"

#include "llvisualparam.h"

//-----------------------------------------------------------------------------
// LLVisualParamInfo()
//-----------------------------------------------------------------------------
LLVisualParamInfo::LLVisualParamInfo()
:	mID(-1),
	mGroup(VISUAL_PARAM_GROUP_TWEAKABLE),
	mMinWeight(0.f),
	mMaxWeight(1.f),
	mDefaultWeight(0.f),
	mSex(SEX_BOTH)
{
}

//-----------------------------------------------------------------------------
// parseXml()
//-----------------------------------------------------------------------------
bool LLVisualParamInfo::parseXml(LLXmlTreeNode* node)
{
	// attribute: id
	static LLStdStringHandle id_string = LLXmlTree::addAttributeString("id");
	node->getFastAttributeS32(id_string, mID);

	// attribute: group
	U32 group = 0;
	static LLStdStringHandle group_string = LLXmlTree::addAttributeString("group");
	if (node->getFastAttributeU32(group_string, group))
	{
		if (group < NUM_VISUAL_PARAM_GROUPS)
		{
			mGroup = (EVisualParamGroup)group;
		}
	}

	// attribute: value_min, value_max
	static LLStdStringHandle value_min_string = LLXmlTree::addAttributeString("value_min");
	static LLStdStringHandle value_max_string = LLXmlTree::addAttributeString("value_max");
	node->getFastAttributeF32(value_min_string, mMinWeight);
	node->getFastAttributeF32(value_max_string, mMaxWeight);

	// attribute: value_default
	F32 default_weight = 0;
	static LLStdStringHandle value_default_string = LLXmlTree::addAttributeString("value_default");
	if (node->getFastAttributeF32(value_default_string, default_weight))
	{
		mDefaultWeight = llclamp(default_weight, mMinWeight, mMaxWeight);
		if (default_weight != mDefaultWeight)
		{
			llwarns << "value_default attribute is out of range in node "
					<< mName << " " << default_weight << llendl;
		}
	}

	// attribute: sex
	std::string sex = "both";
	static LLStdStringHandle sex_string = LLXmlTree::addAttributeString("sex");
	node->getFastAttributeString(sex_string, sex); // optional
	if (sex == "both")
	{
		mSex = SEX_BOTH;
	}
	else if (sex == "male")
	{
		mSex = SEX_MALE;
	}
	else if (sex == "female")
	{
		mSex = SEX_FEMALE;
	}
	else
	{
		llwarns << "Avatar file: <param> has invalid sex attribute: " << sex
				<< llendl;
		return false;
	}

	// attribute: name
	static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
	if (!node->getFastAttributeString(name_string, mName))
	{
		llwarns << "Avatar file: <param> is missing name attribute" << llendl;
		return false;
	}

	// attribute: label
	static LLStdStringHandle label_string = LLXmlTree::addAttributeString("label");
	if (!node->getFastAttributeString(label_string, mDisplayName))
	{
		mDisplayName = mName;
	}

	// JC - make sure the display name includes the capitalization in the XML file,
	// not the lowercased version.
	LLStringUtil::toLower(mName);

	// attribute: label_min
	static LLStdStringHandle label_min_string = LLXmlTree::addAttributeString("label_min");
	if (!node->getFastAttributeString(label_min_string, mMinName))
	{
		mMinName = "Less";
	}

	// attribute: label_max
	static LLStdStringHandle label_max_string = LLXmlTree::addAttributeString("label_max");
	if (!node->getFastAttributeString(label_max_string, mMaxName))
	{
		mMaxName = "More";
	}

	return true;
}

//virtual
void LLVisualParamInfo::toStream(std::ostream &out)
{
	out << mID << "\t";
	out << mName << "\t";
	out << mDisplayName << "\t";
	out << mMinName << "\t";
	out << mMaxName << "\t";
	out << mGroup << "\t";
	out << mMinWeight << "\t";
	out << mMaxWeight << "\t";
	out << mDefaultWeight << "\t";
	out << mSex << "\t";
}

//-----------------------------------------------------------------------------
// LLVisualParam()
//-----------------------------------------------------------------------------
LLVisualParam::LLVisualParam()
:	mCurWeight(0.f),
	mLastWeight(0.f),
	mNext(NULL),
	mTargetWeight(0.f),
	mIsAnimating(false),
	mIsDummy(false),
	mID(-1),
	mInfo(0),
	mParamLocation(LOC_UNKNOWN)
{
}

LLVisualParam::LLVisualParam(const LLVisualParam& other)
:	mCurWeight(other.mCurWeight),
	mLastWeight(other.mLastWeight),
	mNext(other.mNext),
	mTargetWeight(other.mTargetWeight),
	mIsAnimating(other.mIsAnimating),
	mIsDummy(other.mIsDummy),
	mID(other.mID),
	mInfo(other.mInfo),
	mParamLocation(other.mParamLocation)
{
}

//-----------------------------------------------------------------------------
// ~LLVisualParam()
//-----------------------------------------------------------------------------
LLVisualParam::~LLVisualParam()
{
	if (mNext)
	{
		delete mNext;
		mNext = NULL;
	}
}

//=============================================================================
// This virtual functions should always be overridden, but is included here for
// use as templates
//=============================================================================
#if 0
//-----------------------------------------------------------------------------
// setInfo()
//-----------------------------------------------------------------------------

bool LLVisualParam::setInfo(LLVisualParamInfo* info)
{
	llassert(mInfo == NULL);
	if (info->mID < 0)
	{
		return false;
	}
	mInfo = info;
	mID = info->mID;
	setWeight(getDefaultWeight(), false);
	return true;
}
#endif

//-----------------------------------------------------------------------------
// setWeight()
//-----------------------------------------------------------------------------
void LLVisualParam::setWeight(F32 weight, bool upload_bake)
{
	if (mIsAnimating)
	{
		//RN: allow overshoot
		mCurWeight = weight;
	}
	else if (mInfo)
	{
		mCurWeight = llclamp(weight, mInfo->mMinWeight, mInfo->mMaxWeight);
	}
	else
	{
		mCurWeight = weight;
	}

	if (mNext)
	{
		mNext->setWeight(weight, upload_bake);
	}
}

//-----------------------------------------------------------------------------
// setAnimationTarget()
//-----------------------------------------------------------------------------
void LLVisualParam::setAnimationTarget(F32 target_value, bool upload_bake)
{
	// don't animate dummy parameters
	if (mIsDummy)
	{
		setWeight(target_value, upload_bake);
		mTargetWeight = mCurWeight;
		return;
	}

	if (mInfo)
	{
		if (isTweakable())
		{
			mTargetWeight = llclamp(target_value, mInfo->mMinWeight,
									mInfo->mMaxWeight);
		}
	}
	else
	{
		mTargetWeight = target_value;
	}
	mIsAnimating = true;

	if (mNext)
	{
		mNext->setAnimationTarget(target_value, upload_bake);
	}
}

//-----------------------------------------------------------------------------
// setNextParam()
//-----------------------------------------------------------------------------
void LLVisualParam::setNextParam(LLVisualParam* next)
{
	// need to establish mNext before we start changing values on this, else
	// initial value won't get mirrored (we can fix that, but better to forbid
	// this pattern):
	llassert(!mNext && getWeight() == getDefaultWeight());
	mNext = next;
}

//-----------------------------------------------------------------------------
// animate()
//-----------------------------------------------------------------------------
void LLVisualParam::animate(F32 delta, bool upload_bake)
{
	if (mIsAnimating)
	{
		F32 new_weight = (mTargetWeight - mCurWeight) * delta + mCurWeight;
		setWeight(new_weight, upload_bake);
	}
}

//-----------------------------------------------------------------------------
// stopAnimating()
//-----------------------------------------------------------------------------
void LLVisualParam::stopAnimating(bool upload_bake)
{
	if (mIsAnimating && isTweakable())
	{
		mIsAnimating = false;
		setWeight(mTargetWeight, upload_bake);
	}
}

const std::string param_location_name(const EParamLocation& loc)
{
	switch (loc)
	{
		case LOC_UNKNOWN: return "unknown";
		case LOC_AV_SELF: return "self";
		case LOC_AV_OTHER: return "other";
		case LOC_WEARABLE: return "wearable";
		default: return "error";
	}
}

void LLVisualParam::setParamLocation(EParamLocation loc)
{
	if (mParamLocation == LOC_UNKNOWN || loc == LOC_UNKNOWN)
	{
		mParamLocation = loc;
	}
	else if (mParamLocation != loc)
	{
		LL_DEBUGS("VisualParams") << "Param location is already "
								  << mParamLocation << ", not slamming to "
								  << loc << LL_ENDL;
	}
}

/**
 * @file llemote.cpp
 * @brief Implementation of LLEmote class
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

#include "llemote.h"

#include "llcharacter.h"

LLEmote::LLEmote(const LLUUID& id)
:	LLMotion(id)
{
	mCharacter = NULL;

	// RN: flag face joint as highest priority for now, until we implement a
	// proper animation track
	mJointSignature[0][LL_FACE_JOINT_NUM] = 0xff;
	mJointSignature[1][LL_FACE_JOINT_NUM] = 0xff;
	mJointSignature[2][LL_FACE_JOINT_NUM] = 0xff;
}

LLMotion::LLMotionInitStatus LLEmote::onInitialize(LLCharacter* character)
{
	mCharacter = character;
	return STATUS_SUCCESS;
}

bool LLEmote::onActivate()
{
	if (!mCharacter)
	{
		return true;
	}

	LLVisualParam* default_param;
	default_param = mCharacter->getVisualParam("Express_Closed_Mouth");
	if (default_param)
	{
		default_param->setWeight(default_param->getMaxWeight(), false);
	}

	mParam = mCharacter->getVisualParam(mName.c_str());
	if (mParam)
	{
		mParam->setWeight(0.f, false);
		mCharacter->updateVisualParams();
	}

	return true;
}

bool LLEmote::onUpdate(F32 time, U8* joint_mask)
{
	if (mParam && mCharacter)
	{
		F32 weight = mParam->getMinWeight() +
					 mPose.getWeight() *
					 (mParam->getMaxWeight() - mParam->getMinWeight());
		mParam->setWeight(weight, false);

		// Cross fade against the default parameter
		LLVisualParam* default_param =
			mCharacter->getVisualParam("Express_Closed_Mouth");
		if (default_param)
		{
			F32 default_param_weight = default_param->getMinWeight() +
									   (1.f - mPose.getWeight()) *
									   (default_param->getMaxWeight() -
										default_param->getMinWeight());

			default_param->setWeight(default_param_weight, false);
		}

		mCharacter->updateVisualParams();
	}

	return true;
}

void LLEmote::onDeactivate()
{
	if (!mCharacter) return;

	if (mParam)
	{
		mParam->setWeight(mParam->getDefaultWeight(), false);
	}

	LLVisualParam* default_param;
	default_param = mCharacter->getVisualParam("Express_Closed_Mouth");
	if (default_param)
	{
		default_param->setWeight(default_param->getMaxWeight(), false);
	}

	mCharacter->updateVisualParams();
}

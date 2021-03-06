/**
 * @file lltargetingmotion.cpp
 * @brief Implementation of LLTargetingMotion class.
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

#include "linden_common.h"

#include "lltargetingmotion.h"

#include "llcharacter.h"
#include "llcriticaldamp.h"
#include "llvector3d.h"

constexpr F32 TORSO_TARGET_HALF_LIFE = 0.25f;

LLTargetingMotion::LLTargetingMotion(const LLUUID& id)
:	LLMotion(id),
	mCharacter(NULL)
{
	mName = "targeting";
	mTorsoState = new LLJointState;
}

LLMotion::LLMotionInitStatus LLTargetingMotion::onInitialize(LLCharacter* character)
{
	// save character for future use
	mCharacter = character;

	mPelvisJoint = mCharacter->getJoint(LL_JOINT_KEY_PELVIS);
	mTorsoJoint = mCharacter->getJoint(LL_JOINT_KEY_TORSO);
	mRightHandJoint = mCharacter->getJoint(LL_JOINT_KEY_WRISTRIGHT);

	// make sure character skeleton is copacetic
	if (!mPelvisJoint || !mTorsoJoint || !mRightHandJoint)
	{
		llwarns << "Invalid skeleton for targeting motion!" << llendl;
		return STATUS_FAILURE;
	}

	mTorsoState->setJoint(mTorsoJoint);

	// add joint states to the pose
	mTorsoState->setUsage(LLJointState::ROT);
	addJointState(mTorsoState);

	return STATUS_SUCCESS;
}

bool LLTargetingMotion::onUpdate(F32, U8* joint_mask)
{
	LLVector3* lookat_pt =
		(LLVector3*)mCharacter->getAnimationData("LookAtPoint");
	if (!lookat_pt)
	{
		return true;
	}

	F32 slerp_amt = LLCriticalDamp::getInterpolant(TORSO_TARGET_HALF_LIFE);

	LLVector3 target = *lookat_pt;
	target.normalize();

#if 0
	LLVector3 target_plane_normal = LLVector3(1.f, 0.f, 0.f) *
									mPelvisJoint->getWorldRotation();
	LLVector3 torso_dir = LLVector3(1.f, 0.f, 0.f) *
						  (mTorsoJoint->getWorldRotation() *
						   mTorsoState->getRotation());
#endif

	LLVector3 skyward(0.f, 0.f, 1.f);
	LLVector3 left(skyward % target);
	left.normalize();
	LLVector3 up(target % left);
	up.normalize();
	LLQuaternion target_aim_rot(target, left, up);

	LLQuaternion cur_torso_rot = mTorsoJoint->getWorldRotation();

	LLVector3 right_hand_at = LLVector3(0.f, -1.f, 0.f) *
							  mRightHandJoint->getWorldRotation();
	left.set(skyward % right_hand_at);
	left.normalize();
	up.set(right_hand_at % left);
	up.normalize();
	LLQuaternion right_hand_rot(right_hand_at, left, up);

	LLQuaternion new_torso_rot = (cur_torso_rot * ~right_hand_rot) * target_aim_rot;

	// find ideal additive rotation to make torso point in correct direction
	new_torso_rot = new_torso_rot * ~cur_torso_rot;

	// slerp from current additive rotation to ideal additive rotation
	new_torso_rot = nlerp(slerp_amt, mTorsoState->getRotation(), new_torso_rot);

	// constraint overall torso rotation
	LLQuaternion total_rot = new_torso_rot * mTorsoJoint->getRotation();
	total_rot.constrain(F_PI_BY_TWO * 0.8f);
	new_torso_rot = total_rot * ~mTorsoJoint->getRotation();

	mTorsoState->setRotation(new_torso_rot);

	return true;
}

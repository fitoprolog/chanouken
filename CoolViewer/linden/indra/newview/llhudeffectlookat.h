/**
 * @file llhudeffectlookat.h
 * @brief LLHUDEffectLookAt class definition
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

#ifndef LL_LLHUDEFFECTLOOKAT_H
#define LL_LLHUDEFFECTLOOKAT_H

#include "llhudeffect.h"

class LLViewerObject;
class LLVOAvatar;
class LLAttentionSet;

typedef enum e_lookat_type
{
	LOOKAT_TARGET_NONE,
	LOOKAT_TARGET_IDLE,
	LOOKAT_TARGET_AUTO_LISTEN,
	LOOKAT_TARGET_FREELOOK,
	LOOKAT_TARGET_RESPOND,
	LOOKAT_TARGET_HOVER,
	LOOKAT_TARGET_CONVERSATION,
	LOOKAT_TARGET_SELECT,
	LOOKAT_TARGET_FOCUS,
	LOOKAT_TARGET_MOUSELOOK,
	LOOKAT_TARGET_CLEAR,
	LOOKAT_NUM_TARGETS
} ELookAtType;

class LLHUDEffectLookAt final : public LLHUDEffect
{
	friend class LLHUDObject;

protected:
	LOG_CLASS(LLHUDEffectLookAt);

public:
	void markDead() override;
	void setSourceObject(LLViewerObject* objectp) override;

	bool setLookAt(ELookAtType target_type, LLViewerObject* object,
				   LLVector3 position);
	void clearLookAtTarget();

	LL_INLINE ELookAtType getLookAtType()			{ return mTargetType; }
	LL_INLINE const LLVector3& getTargetPos()		{ return mTargetPos; }
	LL_INLINE const LLVector3d& getTargetOffset()	{ return mTargetOffsetGlobal; }
	LL_INLINE bool calcTargetPosition();

	static void updateSettings();

protected:
	LLHUDEffectLookAt(U8 type);
	~LLHUDEffectLookAt() override = default;

	void update() override;
	void render() override;
	void packData(LLMessageSystem* mesgsys) override;
	void unpackData(LLMessageSystem* mesgsys, S32 blocknum) override;

	// Look-at behavior has either target position or target object with offset
	void setTargetObjectAndOffset(LLViewerObject* objp,
								  const LLVector3d& offset);
	void setTargetPosGlobal(const LLVector3d &target_pos_global);

public:
	static bool		sDebugLookAt;

private:
	ELookAtType		mTargetType;
	LLVector3d		mTargetOffsetGlobal;
	LLVector3		mLastSentOffsetGlobal;
	F32				mKillTime;
	F32				mNotifyTime;
	LLFrameTimer	mTimer;
	LLVector3		mTargetPos;
	F32				mLastSendTime;
	LLAttentionSet*	mAttentions;
	bool			mNotified;

	static U32		sMaxLookAtBroadcastDist;
};

#endif // LL_LLHUDEFFECTLOOKAT_H

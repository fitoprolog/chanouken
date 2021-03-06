/**
 * @file llmultigesture.h
 * @brief Gestures that are asset-based and can have multiple steps.
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

#ifndef LL_LLMULTIGESTURE_H
#define LL_LLMULTIGESTURE_H

#include <set>
#include <string>
#include <vector>

#include "llframetimer.h"
#include "llpreprocessor.h"
#include "lluuid.h"

class LLDataPacker;
class LLGestureStep;

class LLMultiGesture
{
protected:
	LOG_CLASS(LLMultiGesture);

public:
	LLMultiGesture();
	virtual ~LLMultiGesture();

	// Maximum number of bytes this could hold once serialized.
	S32 getMaxSerialSize() const;

	bool serialize(LLDataPacker& dp) const;
	bool deserialize(LLDataPacker& dp);

	void dump();

	void reset();

	LL_INLINE const std::string& getTrigger() const	{ return mTrigger; }

protected:
	LLMultiGesture(const LLMultiGesture& gest);
	const LLMultiGesture& operator=(const LLMultiGesture& rhs);

public:
	// name is stored at asset level
	// desc is stored at asset level
	KEY mKey;
	MASK mMask;

	// String, like "/foo" or "hello" that makes it play
	std::string mTrigger;

	// Replaces the trigger substring with this text
	std::string mReplaceText;

	std::vector<LLGestureStep*> mSteps;

	// Is the gesture currently playing?
	bool mPlaying;

	// We're waiting for triggered animations to stop playing
	bool mWaitingAnimations;

	// We're waiting a fixed amount of time
	bool mWaitingTimer;

	// Waiting after the last step played for all animations to complete
	bool mWaitingAtEnd;

	// "instruction pointer" for steps
	S32 mCurrentStep;

	// Timer for waiting
	LLFrameTimer mWaitTimer;

	void (*mDoneCallback)(LLMultiGesture* gesture, void* data);
	void* mCallbackData;

	// Animations that we requested to start
	uuid_list_t mRequestedAnimIDs;

	// Once the animation starts playing (sim says to start playing) the ID is
	// moved from mRequestedAnimIDs to here.
	uuid_list_t mPlayingAnimIDs;
};

// Order must match the library_list in floater_preview_gesture.xml !

enum EStepType
{
	STEP_ANIMATION = 0,
	STEP_SOUND = 1,
	STEP_CHAT = 2,
	STEP_WAIT = 3,

	STEP_EOF = 4
};

class LLGestureStep
{
protected:
	LOG_CLASS(LLGestureStep);

public:
	LLGestureStep() = default;
	virtual ~LLGestureStep() = default;

	virtual EStepType getType() = 0;

	// Return a user-readable label for this step
	virtual std::string getLabel() const = 0;

	virtual S32 getMaxSerialSize() const = 0;
	virtual bool serialize(LLDataPacker& dp) const = 0;
	virtual bool deserialize(LLDataPacker& dp) = 0;

	virtual void dump() = 0;
};

// By default, animation steps start animations.
// If the least significant bit is 1, it will stop animations.
constexpr U32 ANIM_FLAG_STOP = 0x01;

class LLGestureStepAnimation final : public LLGestureStep
{
protected:
	LOG_CLASS(LLGestureStepAnimation);

public:
	LLGestureStepAnimation();

	LL_INLINE EStepType getType() override			{ return STEP_ANIMATION; }

	std::string getLabel() const override;

	S32 getMaxSerialSize() const override;
	bool serialize(LLDataPacker& dp) const override;
	bool deserialize(LLDataPacker& dp) override;

	void dump() override;

public:
	std::string	mAnimName;
	LLUUID		mAnimAssetID;
	U32			mFlags;
};

class LLGestureStepSound final : public LLGestureStep
{
protected:
	LOG_CLASS(LLGestureStepSound);

public:
	LLGestureStepSound();

	LL_INLINE EStepType getType() override			{ return STEP_SOUND; }

	std::string getLabel() const override;

	S32 getMaxSerialSize() const override;
	bool serialize(LLDataPacker& dp) const override;
	bool deserialize(LLDataPacker& dp) override;

	void dump() override;

public:
	std::string	mSoundName;
	LLUUID		mSoundAssetID;
	U32			mFlags;
};

class LLGestureStepChat final : public LLGestureStep
{
protected:
	LOG_CLASS(LLGestureStepChat);

public:
	LLGestureStepChat();

	LL_INLINE EStepType getType() override			{ return STEP_CHAT; }

	std::string getLabel() const override;

	S32 getMaxSerialSize() const override;
	bool serialize(LLDataPacker& dp) const override;
	bool deserialize(LLDataPacker& dp) override;

	void dump() override;

public:
	std::string	mChatText;
	U32			mFlags;
};

constexpr U32 WAIT_FLAG_TIME		= 0x01;
constexpr U32 WAIT_FLAG_ALL_ANIM	= 0x02;

class LLGestureStepWait final : public LLGestureStep
{
protected:
	LOG_CLASS(LLGestureStepWait);

public:
	LLGestureStepWait();

	EStepType getType() override					{ return STEP_WAIT; }

	std::string getLabel() const override;

	S32 getMaxSerialSize() const override;
	bool serialize(LLDataPacker& dp) const override;
	bool deserialize(LLDataPacker& dp) override;

	void dump() override;

public:
	F32 mWaitSeconds;
	U32 mFlags;
};

#endif

/**
 * @file llfloatermove.h
 * @brief Container for buttons for walking, turning, flying
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

#ifndef LL_LLFLOATERMOVE_H
#define LL_LLFLOATERMOVE_H

#include "llfloater.h"

class LLButton;
class LLJoystickAgentSlide;
class LLJoystickAgentTurn;
class LLSpinCtrl;

class LLFloaterMove final :	public LLFloater,
							public LLFloaterSingleton<LLFloaterMove>
{
	friend class LLUISingleton<LLFloaterMove, VisibilityPolicy<LLFloater> >;
	friend class LLAgent;

protected:
	LLFloaterMove(const LLSD& key);
	~LLFloaterMove() override	{}

public:
	void draw() override;
	void onOpen() override;
	void onClose(bool app_quitting) override;

	static void onFlyButtonClicked(void* userdata);
	static F32 getYawRate(F32 time);

private:
	static void turnLeftNudge(void* userdata);
	static void turnLeft(void* userdata);

	static void turnRightNudge(void* userdata);
	static void turnRight(void* userdata);

	static void moveUp(void* userdata);
	static void moveDown(void* userdata);

private:
	LLJoystickAgentTurn*	mForwardButton;
	LLJoystickAgentTurn*	mBackwardButton;
	LLJoystickAgentSlide*	mSlideLeftButton;
	LLJoystickAgentSlide*	mSlideRightButton;
	LLButton*				mTurnLeftButton;
	LLButton*				mTurnRightButton;
	LLButton*				mMoveUpButton;
	LLButton*				mMoveDownButton;
	LLButton*				mFlyButton;
	LLSpinCtrl*				mZOffsetSpinner;
};

#endif	// LL_LLFLOATERMOVE_H

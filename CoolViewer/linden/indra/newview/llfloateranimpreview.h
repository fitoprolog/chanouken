/**
 * @file llfloateranimpreview.h
 * @brief LLFloaterAnimPreview class definition
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

#ifndef LL_LLFLOATERANIMPREVIEW_H
#define LL_LLFLOATERANIMPREVIEW_H

#include "llcharacter.h"
#include "llextendedstatus.h"
#include "llquaternion.h"

#include "lldynamictexture.h"
#include "llfloaternamedesc.h"

class LLButton;
class LLKeyframeMotion;
class LLViewerJointMesh;
class LLVOAvatar;

class LLPreviewAnimation final : public LLViewerDynamicTexture
{
protected:
	LOG_CLASS(LLPreviewAnimation);

protected:
	~LLPreviewAnimation() override;

public:
	LLPreviewAnimation(S32 width, S32 height);

	S8 getType() const override;

	bool render() override;
	void rotate(F32 yaw_radians, F32 pitch_radians);
	void zoom(F32 zoom_delta);
	void setZoom(F32 zoom_amt);
	void pan(F32 right, F32 up);

	LL_INLINE LLVOAvatar* getDummyAvatar()				{ return mDummyAvatar; }

protected:
	LLPointer<LLVOAvatar>	mDummyAvatar;
	LLVector3				mCameraOffset;
	LLVector3				mCameraRelPos;
	F32						mCameraDistance;
	F32						mCameraYaw;
	F32						mCameraPitch;
	F32						mCameraZoom;
};

class LLFloaterAnimPreview final : public LLFloaterNameDesc
{
protected:
	LOG_CLASS(LLFloaterAnimPreview);

public:
	LLFloaterAnimPreview(const std::string& filename);
	~LLFloaterAnimPreview() override;

	bool postBuild() override;
	void draw() override;
	void refresh() override;

private:
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	void onMouseCaptureLost() override;

	std::map <std::string, std::string> getJointAliases();

	void setAnimCallbacks();
	void resetMotion();

	LLVOAvatar* getAvatar();
	LLKeyframeMotion* getMotion();

	static void	onBtnOK(void*);
	static void	onBtnPlay(void*);
	static void	onBtnStop(void*);
	static void onSliderMove(LLUICtrl*, void*);
	static void onCommitBaseAnim(LLUICtrl*, void*);
	static void onCommitLoop(LLUICtrl*, void*);
	static void onCommitLoopIn(LLUICtrl*, void*);
	static void onCommitLoopOut(LLUICtrl*, void*);
	static bool validateLoopIn(LLUICtrl*, void*);
	static bool validateLoopOut(LLUICtrl*, void*);
	static void onCommitName(LLUICtrl*, void*);
	static void onCommitHandPose(LLUICtrl*, void*);
	static void onCommitEmote(LLUICtrl*, void*);
	static void onCommitPriority(LLUICtrl*, void*);
	static void onCommitEaseIn(LLUICtrl*, void*);
	static void onCommitEaseOut(LLUICtrl*, void*);
	static bool validateEaseIn(LLUICtrl*, void*);
	static bool validateEaseOut(LLUICtrl*, void*);
	static void onSaveComplete(const LLUUID& asset_uuid,
							   LLAssetType::EType type,
							   void* user_data,
							   S32 status,
							   LLExtStat ext_status);

private:
	LLPointer< LLPreviewAnimation > mAnimPreview;
	LLButton*						mPlayButton;
	LLButton*						mStopButton;
	LLButton*						mUploadButton;
	LLUIImagePtr					mPlayImage;
	LLUIImagePtr					mPlaySelectedImage;
	LLUIImagePtr					mPauseImage;
	LLUIImagePtr					mPauseSelectedImage;
	LLRect							mPreviewRect;
	LLRectf							mPreviewImageRect;
	LLAssetID						mMotionID;
	LLTransactionID					mTransactionID;
	LLAnimPauseRequest				mPauseRequest;
	S32								mLastMouseX;
	S32								mLastMouseY;
	bool							mInWorld;
	bool							mBadAnimation;

	std::map<std::string, LLUUID>	mIDList;
};

#endif  // LL_LLFLOATERANIMPREVIEW_H

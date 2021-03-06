/**
 * @file llprogressview.h
 * @brief LLProgressView class definition
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

#ifndef LL_LLPROGRESSVIEW_H
#define LL_LLPROGRESSVIEW_H

#include "llpanel.h"
#include "llframetimer.h"

class LLImageRaw;
class LLButton;
class LLProgressBar;

class LLProgressView final : public LLPanel
{
public:
	LLProgressView(const std::string& name, const LLRect& rect);
	~LLProgressView() override;

	bool postBuild() override;

	void draw() override;

	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleKeyHere(KEY key, MASK mask) override;
	void setVisible(bool visible) override;

	void setText(const std::string& text);
	void setPercent(F32 percent);

	// Set it to NULL when you want to eliminate the message.
	void setMessage(const std::string& msg);

	void setCancelButtonVisible(bool b, const std::string& label);

	static void onCancelButtonClicked(void*);
	static void onClickMessage(void*);

protected:
	LLProgressBar* mProgressBar;
	F32 mPercentDone;
	std::string mMessage;
	LLButton*	mCancelBtn;
	LLFrameTimer	mFadeTimer;
	LLFrameTimer mProgressTimer;
	LLRect mOutlineRect;
	bool mMouseDownInActiveArea;
	bool mURLInMessage;

	static LLProgressView* sInstance;
};

extern S32 gStartImageWidth;
extern S32 gStartImageHeight;

#endif // LL_LLPROGRESSVIEW_H

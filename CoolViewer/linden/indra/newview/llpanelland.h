/**
 * @file llpanelland.h
 * @brief Land information in the tool floater, NOT the "About Land" floater
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

#ifndef LL_LLPANELLAND_H
#define LL_LLPANELLAND_H

#include "llpanel.h"

class LLButton;
class LLPanelLandSelectObserver;
class LLTextBox;

class LLPanelLandInfo final : public LLPanel
{
protected:
	LOG_CLASS(LLPanelLandInfo);

public:
	LLPanelLandInfo(const std::string& name);
	~LLPanelLandInfo() override;

	bool postBuild() override;
	void refresh() override;

	static void refreshAll();

private:
	static void onClickClaim(void*);
	static void onClickRelease(void*);
	static void onClickDivide(void*);
	static void onClickJoin(void*);
	static void onClickAbout(void*);
	static void onShowOwnersHelp(void*);

private:
	LLTextBox*		mTextLabelPrice;
	LLTextBox*		mTextPrice;

	LLButton*		mBtnBuyLand;
	LLButton*		mBtnAbandonLand;
	LLButton*		mBtnDivideLand;
	LLButton*		mBtnJoinLand;
	LLButton*		mBtnAboutLand;

	static LLPanelLandSelectObserver* sObserver;
	static LLPanelLandInfo* sInstance;
};

#endif

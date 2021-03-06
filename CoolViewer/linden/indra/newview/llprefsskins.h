/** 
 * @file llprefsskins.h
 * @author James Cook
 * @brief Skins preferences panel in preferences floater
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

#ifndef LL_LLPREFSSKINS_H
#define LL_LLPREFSSKINS_H

#include "llpanel.h"

class LLRadioGroup;

class LLPrefSkins final : public LLPanel
{
	LOG_CLASS(LLPrefSkins);

public:
	LLPrefSkins();

	bool postBuild() override;
	void refresh() override;

	void apply();
	void cancel();

private:
	static void onSelectSkin(LLUICtrl* ctrl, void* data);
	static void onClickClassic(void* data);
	static void onClickSilver(void* data);
	static void onClickDark(void* data);

private:
	LLRadioGroup*	mSkinsSelector;
	std::string		mSkin;
};

#endif // LL_LLPREFSSKINS_H

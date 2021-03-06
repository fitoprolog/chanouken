/**
 * @file llfloatereditui.h
 * @author James Cook
 * @brief In-world UI editor
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATEREDITUI_H
#define LL_LLFLOATEREDITUI_H

#include "llfloater.h"

class LLLineEditor;
class LLSpinCtrl;

class LLFloaterEditUI final : public LLFloater,
							  public LLFloaterSingleton<LLFloaterEditUI>
{
	friend class LLUISingleton<LLFloaterEditUI, VisibilityPolicy<LLFloater> >;

public:
	~LLFloaterEditUI() override;

	void draw() override;
	void refresh() override;

	static void show(void* dummy = NULL);

	static bool processKeystroke(KEY key, MASK mask);

private:
	// Open only via show().
	LLFloaterEditUI(const LLSD&);

	void refreshCore();
	void refreshView(LLView* view);
	void refreshButton(LLView* view);

	static void	navigateHierarchyButtonPressed(void* data);

	static void onCommitLabel(LLUICtrl* ctrl, void* data);
	static void onCommitHeight(LLUICtrl* ctrl, void* data);
	static void onCommitWidth(LLUICtrl* ctrl, void* data);

private:
	LLLineEditor*	mLabelLine;
	LLSpinCtrl*		mWidthSpin;
	LLSpinCtrl*		mHeightSpin;
	LLView*			mLastView;
};

#endif

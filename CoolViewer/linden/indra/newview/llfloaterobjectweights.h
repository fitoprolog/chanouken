/**
 * @file llfloaterobjectweights.h
 * @brief LLFloaterObjectWeights class definition
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#ifndef LL_LLFLOATEROBJECTWEIGHTS_H
#define LL_LLFLOATEROBJECTWEIGHTS_H

#include "llfloater.h"
#include "lltimer.h"

class LLTextBox;

class LLFloaterObjectWeights final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterObjectWeights>
{
	friend class LLUISingleton<LLFloaterObjectWeights,
							   VisibilityPolicy<LLFloater> >;

public:
	bool postBuild() override;
	void refresh() override;
	void draw() override;

	static void show(LLFloater* parent = NULL);

private:
	// Open only via show()
	LLFloaterObjectWeights(const LLSD&);

private:
	LLFloater*						mParentFloater;

	LLTextBox*						mObjectCount;
	LLTextBox*						mObjectImpact;
	LLTextBox*						mObjectPhysics;
	LLTextBox*						mPrimCount;
	LLTextBox*						mPrimImpact;
	LLTextBox*						mPrimPhysics;
	LLTextBox*						mPrimStreaming;
	LLTextBox*						mPrimTriangles;

	LLTimer							mUpdateTimer;
};

#endif

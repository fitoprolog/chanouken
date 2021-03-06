/**
 * @file llfloaternamedesc.h
 * @brief LLFloaterNameDesc class definition
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

#ifndef LL_LLFLOATERNAMEDESC_H
#define LL_LLFLOATERNAMEDESC_H

#include "llfloater.h"

class LLFloaterNameDesc : public LLFloater
{
public:
	LLFloaterNameDesc(const std::string& filename, S32 cost);
	virtual ~LLFloaterNameDesc();

	virtual bool postBuild();

	static void doCommit(LLUICtrl*, void* userdata);

protected:
	// Sub-classes should override this function if they allow editing
	virtual void onCommit();

	static void	onBtnOK(void*);
	static void	onBtnCancel(void*);

protected:
	std::string	mFilenameAndPath;
	std::string	mFilename;
	S32			mCost;
	bool		mCheckTempAsset;
};

#endif  // LL_LLFLOATERNAMEDESC_H

/**
 * @file llprogressbar.h
 * @brief LLProgressBar class definition
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

#ifndef LL_LLPROGRESSBAR_H
#define LL_LLPROGRESSBAR_H

#include "llframetimer.h"
#include "llview.h"

class LLProgressBar : public LLView
{
public:
	LLProgressBar(const std::string& name, const LLRect& rect);
	~LLProgressBar() override;

	void setPercent(F32 percent);

	void setImageBar(const std::string& bar_name);
	void setImageShadow(const std::string& shadow_name);
	void setColorBackground(const LLColor4& c);

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	void draw() override;

protected:
	F32				mPercentDone;

	LLUIImagePtr	mImageBar;
	LLUIImagePtr	mImageShadow;
	LLColor4		mColorBackground;
};

#endif // LL_LLPROGRESSBAR_H

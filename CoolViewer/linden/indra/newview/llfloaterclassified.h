/**
 * @file llfloaterclassified.h
 * @brief Classified information as shown in a floating window from
 * secondlife:// command handler.
 * Just a wrapper for LLPanelClassified.
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

#ifndef LL_FLOATERCLASSIFIED_H
#define LL_FLOATERCLASSIFIED_H

#include "llfloater.h"

class LLPanelClassified;

class LLFloaterClassifiedInfo final : LLFloater
{
public:
	LLFloaterClassifiedInfo(const std::string& name, const LLUUID& id);
	~LLFloaterClassifiedInfo() override;

	void displayClassifiedInfo(const LLUUID& classified_id);

	static LLFloaterClassifiedInfo* show(const LLUUID& classified_id);

	static void* createClassifiedDetail(void* userdata);

private:
	LLPanelClassified*		mClassifiedPanel;
	LLUUID					mClassifiedID;

	typedef fast_hmap<LLUUID, LLFloaterClassifiedInfo*> instances_map_t;
	static instances_map_t	sInstances;
};

#endif // LL_FLOATERCLASSIFIED_H

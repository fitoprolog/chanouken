/**
 * @file llfloatergroupinvite.h
 * @brief This floater is just a wrapper for LLPanelGroupInvite, which
 * is used to invite members to a specific group
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERGROUPINVITE_H
#define LL_LLFLOATERGROUPINVITE_H

#include "llfloater.h"

class LLPanelGroupInvite;

class LLFloaterGroupInvite final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterGroupInvite);

	LLFloaterGroupInvite(const LLUUID& group_id = LLUUID::null);

public:
	~LLFloaterGroupInvite() override;

	static void showForGroup(const LLUUID& group_id,
							 uuid_vec_t* agent_ids = NULL,
							 LLView* parent = NULL);

	static void* createPanel(void* userdata);

public:
	LLUUID					mGroupID;
	LLPanelGroupInvite*		mInvitePanelp;

	typedef fast_hmap<LLUUID, LLFloaterGroupInvite*> instances_map_t;
	static instances_map_t	sInstances;
};

#endif	// LL_LLFLOATERGROUPINVITE_H
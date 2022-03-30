/**
 * @file llinventoryactions.h
 * @brief Definitions of the actions associated with menu items.
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

#ifndef LL_LLINVENTORYACTIONS_H
#define LL_LLINVENTORYACTIONS_H

#include "lluuid.h"

class LLFloaterInventory;
class LLInventoryPanel;
class LLPanelInventory;
class LLViewerInventoryItem;

void init_object_inventory_panel_actions(LLPanelInventory* panel);
void init_inventory_actions(LLFloaterInventory* floater);
void init_inventory_panel_actions(LLInventoryPanel* panel);

// These functions can open items without the inventory being visible:
void open_notecard(LLViewerInventoryItem* inv_item,
				   const std::string& title,
				   const LLUUID& object_id,
				   bool show_keep_discard,
				   const LLUUID& source_id = LLUUID::null,
				   bool take_focus = true);
void open_landmark(LLViewerInventoryItem* inv_item,
				   const std::string& title,
				   bool show_keep_discard,
				   const LLUUID& source_id = LLUUID::null,
				   bool take_focus = true);
void open_texture(const LLUUID& item_id,
				  const std::string& title,
				  bool show_keep_discard,
				  const LLUUID& source_id = LLUUID::null,
				  bool take_focus = true);
void open_callingcard(LLViewerInventoryItem* item);

#endif // LL_LLINVENTORYACTIONS_H

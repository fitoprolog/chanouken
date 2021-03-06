/**
 * @file llnamebox.h
 * @brief display and refresh a name from the name cache
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLNAMEBOX_H
#define LL_LLNAMEBOX_H

#include "lltextbox.h"

class LLNameBox : public LLTextBox
{
public:
	// By default, follows top and left and is mouse-opaque. If no text,
	// text = name. If no font, uses default system font.
	LLNameBox(const std::string& name, const LLRect& rect,
			  const LLUUID& name_id = LLUUID::null, bool is_group = false,
			  const LLFontGL* font = NULL, bool mouse_opaque = true);

	~LLNameBox() override;

	void setNameID(const LLUUID& name_id, bool is_group);

	void refresh(const LLUUID& id, const std::string& fullname, bool is_group);

	static void refreshAll(const LLUUID& id, const std::string& fullname,
						   bool is_group);

	typedef fast_hset<LLNameBox*> namebox_list_t;

private:
	static namebox_list_t sInstances;

private:
	LLUUID mNameID;
};

#endif

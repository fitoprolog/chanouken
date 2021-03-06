/**
 * @file llnamebox.cpp
 * @brief A text display widget
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

#include "linden_common.h"

#include "llnamebox.h"

#include "llcachename.h"

//static
LLNameBox::namebox_list_t LLNameBox::sInstances;

LLNameBox::LLNameBox(const std::string& name,
					 const LLRect& rect,
					 const LLUUID& name_id,
					 bool is_group,
					 const LLFontGL* font,
					 bool mouse_opaque)
:	LLTextBox(name, rect, "(retrieving)", font, mouse_opaque),
	mNameID(name_id)
{
	sInstances.insert(this);
	if (name_id.notNull())
	{
		setNameID(name_id, is_group);
	}
	else
	{
		setText(LLStringUtil::null);
	}
}

LLNameBox::~LLNameBox()
{
	sInstances.erase(this);
}

void LLNameBox::setNameID(const LLUUID& name_id, bool is_group)
{
	mNameID = name_id;

	std::string name;

	if (gCacheNamep)
	{
		if (!is_group)
		{
			gCacheNamep->getFullName(name_id, name);
		}
		else
		{
			gCacheNamep->getGroupName(name_id, name);
		}
	}

	setText(name);
}

void LLNameBox::refresh(const LLUUID& id, const std::string& fullname,
						bool is_group)
{
	if (id == mNameID)
	{
		setText(fullname);
	}
}

void LLNameBox::refreshAll(const LLUUID& id, const std::string& fullname,
						   bool is_group)
{
	for (namebox_list_t::iterator it = sInstances.begin(),
								  end = sInstances.end();
		 it != end; ++it)
	{
		LLNameBox* box = *it;
		box->refresh(id, fullname, is_group);
	}
}

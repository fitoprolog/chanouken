/**
 * @file llstylemap.cpp
 * @brief LLStyleMap class implementation
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

#include "linden_common.h"

#include <utility>

#include "llstylemap.h"

#include "llstring.h"

extern LLUUID gAgentID;		// from ../newview/llagent.cpp

LLStyleMap gStyleMap;

// This is similar to the [] accessor except that if the entry does not already
// exist, then this will create the entry.
const LLStyleSP& LLStyleMap::lookupAgent(const LLUUID& id)
{
	// Find this style in the map or add it if not. This map holds links to
	// residents' profiles.
	iterator it = find(id);
	if (it != end())
	{
		return it->second;
	}

	LLStyleSP style(new LLStyle);
	if (id.notNull() && id != gAgentID)
	{
		style->setColor(LLUI::sHTMLLinkColor);
		std::string link = llformat("secondlife:///app/agent/%s/about",
									id.asString().c_str());
		style->setLinkHREF(link);
	}
	else
	{
		// Make the resident's own name white and do not make the name
		// clickable.
		style->setColor(LLColor4::white);
	}
	it = emplace(id, std::move(style)).first;
	return it->second;
}

// This is similar to lookupAgent for any generic URL encoded style.
const LLStyleSP& LLStyleMap::lookup(const LLUUID& id, const std::string& link)
{
	// Find this style in the map or add it if not.
	iterator it = find(id);
	if (it != end())
	{
		LLStyle* style = it->second.get();
		if (style->getLinkHREF() != link)
		{
			style->setLinkHREF(link);
		}
		return it->second;
	}

	LLStyleSP style(new LLStyle);
	if (id.notNull() && !link.empty())
	{
		style->setColor(LLUI::sHTMLLinkColor);
		style->setLinkHREF(link);
	}
	else
	{
		style->setColor(LLColor4::white);
	}
	it = emplace(id, std::move(style)).first;
	return it->second;
}

void LLStyleMap::update()
{
	for (style_map_t::iterator it = begin(), eit = end(); it != eit; ++it)
	{
		LLStyle* style = it->second.get();
		// Update the link color in case it has been changed.
		style->setColor(LLUI::sHTMLLinkColor);
	}
}

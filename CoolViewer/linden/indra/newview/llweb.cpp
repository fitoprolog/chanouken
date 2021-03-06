/**
 * @file llweb.cpp
 * @brief Functions dealing with web browsers
 * @author James Cook
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

#include "llviewerprecompiledheaders.h"

#include "llweb.h"

#include "llnotifications.h"
#include "llsys.h"						// For LLOSInfo
#include "lluri.h"
#include "llversionviewer.h"

#include "llagent.h"
#include "llfloatermediabrowser.h"
#include "llgridmanager.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"

//static
void LLWeb::initClass()
{
	LLAlertDialog::setURLLoader(&sAlertURLLoader);
}

//static
void LLWeb::loadURL(const std::string& url)
{
	loadURL(url, "");
}

//static
void LLWeb::loadURL(const std::string& url, const std::string& target)
{
	if (target != "_internal" &&
		(target == "_external" || gSavedSettings.getBool("UseExternalBrowser")))
	{
		loadURLExternal(url);
	}
	else
	{
		LLFloaterMediaBrowser::showInstance(url);
	}
}

//static
void LLWeb::loadURLInternal(const std::string& url)
{
	loadURL(url, "_internal");
}

//static
void LLWeb::loadURLExternal(const std::string& url)
{
	loadURLExternal(url, true);
}

bool on_load_url_external_response(const LLSD& notification,
								   const LLSD& response, bool async)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLSD payload = notification["payload"];
		std::string url = payload["url"].asString();
		std::string escaped_url = LLWeb::escapeURL(url);
		if (gWindowp)
		{
			gWindowp->spawnWebBrowser(escaped_url, async);
		}
	}
	return false;
}

//static
void LLWeb::loadURLExternal(const std::string& url, bool async)
{
	LLSD payload;
	payload["url"] = url;
	gNotifications.add("WebLaunchExternalTarget", LLSD(), payload,
					   boost::bind(on_load_url_external_response, _1, _2,
								   async));
}

//static
std::string LLWeb::escapeURL(const std::string& url)
{
	// The CURL curl_escape() function escapes colons, slashes and all
	// characters but A-Z and 0-9. Do a cheesy mini-escape.
	std::string escaped_url;
	S32 len = url.length();
	for (S32 i = 0; i < len; ++i)
	{
		char c = url[i];
		if (c == ' ')
		{
			escaped_url += "%20";
		}
		else if (c == '\\')
		{
			escaped_url += "%5C";
		}
		else
		{
			escaped_url += c;
		}
	}
	return escaped_url;
}

//static
std::string LLWeb::expandURLSubstitutions(const std::string& url,
										  const LLStringUtil::format_map_t& default_subs)
{
	LLStringUtil::format_map_t substitution = default_subs;
	substitution["[VERSION]"] = llformat("%d.%d.%d.%d", LL_VERSION_MAJOR,
										 LL_VERSION_MINOR, LL_VERSION_BRANCH,
										 LL_VERSION_RELEASE);
	substitution["[VERSION_MAJOR]"] = llformat("%d", LL_VERSION_MAJOR);
	substitution["[VERSION_MINOR]"] = llformat("%d", LL_VERSION_MINOR);
	substitution["[VERSION_PATCH]"] = llformat("%d", LL_VERSION_BRANCH);
	substitution["[VERSION_BUILD]"] = llformat("%d", LL_VERSION_RELEASE);
	substitution["[CHANNEL]"] =  gSavedSettings.getString("VersionChannelName");
	substitution["[GRID]"] = LLGridManager::getInstance()->getGridLabel();
	substitution["[OS]"] = LLOSInfo::getInstance()->getOSStringSimple();
	substitution["[SESSION_ID]"] = gAgentSessionID.asString();
	substitution["[FIRST_LOGIN]"] = llformat("%d", gAgent.isFirstLogin());

	// Work out the current language
	std::string lang = LLUI::getLanguage();
	if (lang == "en-us")
	{
		lang = "en";
	}
	substitution["[LANGUAGE]"] = lang;

	// Find the region ID and name
	LLUUID region_id;
	std::string region_name;
	LLViewerRegion* region = gAgent.getRegion();
	if (region)
	{
		region_id = region->getRegionID();
		region_name = LLURI::escape(region->getName());
	}
	substitution["[REGION_ID]"] = region_id.asString();
	substitution["[REGION]"] = region_name;

	// Find the parcel local ID
	S32 parcel_id = 0;
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		parcel_id = parcel->getLocalID();
	}
	substitution["[PARCEL_ID]"] = llformat("%d", parcel_id);

	if (!gIsInSecondLife)
	{
		substitution["SLURL_TYPE"] = "hop";
	}

	// Expand all of the substitution strings and escape the url
	std::string expanded_url = url;
	LLStringUtil::format(expanded_url, substitution);

	return escapeURL(expanded_url);
}

//virtual
void LLWeb::URLLoader::load(const std::string& url)
{
	loadURL(url);
}

//static
LLWeb::URLLoader LLWeb::sAlertURLLoader;

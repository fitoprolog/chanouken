/**
 * @file llcommandhandler.cpp
 * @brief Central registry for text-driven "commands", most of
 * which manipulate user interface.  For example, the command
 * "agent (uuid) about" will open the UI for an avatar's profile.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llcommandhandler.h"

#include "llnotifications.h"
#include "llsdserialize.h"

#include "llappviewer.h"	// gFrameTimeSeconds
#include "llstartup.h"

#define THROTTLE_PERIOD 5	// required seconds between throttled commands

//---------------------------------------------------------------------------
// Underlying registry for command handlers, not directly accessible.
//---------------------------------------------------------------------------
struct LLCommandHandlerInfo
{
	LLCommandHandler::EUntrustedAccess mUntrustedBrowserAccess;
	LLCommandHandler* mHandler;	// safe, all of these are static objects
};

class LLCommandHandlerRegistry : public LLSingleton<LLCommandHandlerRegistry>
{
	friend class LLSingleton<LLCommandHandlerRegistry>;

protected:
	LOG_CLASS(LLCommandHandlerRegistry);

public:
	void add(const char* cmd,
			 LLCommandHandler::EUntrustedAccess untrusted_access,
			 LLCommandHandler* handler);

	bool dispatch(const std::string& cmd, const LLSD& params,
				  const LLSD& query_map, LLMediaCtrl* web,
				  const std::string& nav_type, bool trusted_browser);

	void dump();

private:
	typedef std::map<std::string, LLCommandHandlerInfo> handlers_map_t;
	handlers_map_t mMap;
};

void LLCommandHandlerRegistry::add(const char* cmd,
								   LLCommandHandler::EUntrustedAccess untrusted_access,
								   LLCommandHandler* handler)
{
	LLCommandHandlerInfo info;
	info.mUntrustedBrowserAccess = untrusted_access;
	info.mHandler = handler;

	mMap[cmd] = info;
}

void LLCommandHandlerRegistry::dump()
{
	std::string access;
	llinfos << "Existing command handlers:\n";
	for (handlers_map_t::const_iterator it = mMap.begin(), end = mMap.end();
		 it != end; ++it)
	{
		const std::string& name = it->first;
		const LLCommandHandlerInfo& info = it->second;
		switch (info.mUntrustedBrowserAccess)
		{
			case LLCommandHandler::UNTRUSTED_ALLOW:
				access = "UNTRUSTED_ALLOW";
				break;

			case LLCommandHandler::UNTRUSTED_BLOCK:
				access = "UNTRUSTED_BLOCK";
				break;
				
			case LLCommandHandler::UNTRUSTED_THROTTLE:
				access = "UNTRUSTED_THROTTLE";
				break;

			default:
				access = "Illegal access type !";
		}
		llcont << " - secondlife:///app/" << name << ": " << access << "\n";
	}
	llcont << llendl;
}

bool LLCommandHandlerRegistry::dispatch(const std::string& cmd,
										const LLSD& params,
										const LLSD& query_map,
										LLMediaCtrl* web,
										const std::string& nav_type,
										bool trusted_browser)
{
	static F32 last_throttle_time = 0.f;

	std::map<std::string, LLCommandHandlerInfo>::iterator it = mMap.find(cmd);
	if (it == mMap.end()) return false;

	const LLCommandHandlerInfo& info = it->second;
	if (!trusted_browser)
	{
		switch (info.mUntrustedBrowserAccess)
		{
			case LLCommandHandler::UNTRUSTED_BLOCK:
				// Block request from external browser, but report as "handled"
				// because it was well formatted.
				llwarns << "Blocked SLURL command from untrusted browser"
						<< llendl;
				// Note: commands can arrive before we initialize everything we
				// need for Notification.
				if (LLStartUp::getStartupState() >= STATE_BROWSER_INIT)
				{
					gNotifications.add("UnableToOpenCommandURL");
				}
				return true;

			case LLCommandHandler::UNTRUSTED_THROTTLE:
				if (LLStartUp::getStartupState() < STATE_BROWSER_INIT)
				{
					return true;
				}
				// If user actually clicked on a link, we do not need to
				// throttle it (the throttling mechanism is used to prevent an
				// avalanche of commands via javascript).
				if (nav_type == "clicked")
				{
					break;
				}

				if (gFrameTimeSeconds < last_throttle_time + THROTTLE_PERIOD)
				{
					llwarns_once << "Throttled SLURL command from untrusted browser"
								 << llendl;
					return true;
				}
				last_throttle_time = gFrameTimeSeconds;
				break;

			default:
			case LLCommandHandler::UNTRUSTED_ALLOW:
				// Fall through and let the command be handled
				break;
		}
	}

	LL_DEBUGS("CommandHandler") << "Dispatching '" << cmd << "' with:";
	std::stringstream str1, str2;
	LLSDSerialize::toPrettyXML(params, str1);
	LLSDSerialize::toPrettyXML(query_map, str2);
	LL_CONT << "\nparams = " << str1.str() << "\nquery map = " << str2.str()
			<< LL_ENDL;

	return info.mHandler ? info.mHandler->handle(params, query_map, web)
						 : false;
}

//---------------------------------------------------------------------------
// Automatic registration of commands, runs before main()
//---------------------------------------------------------------------------

LLCommandHandler::LLCommandHandler(const char* cmd,
								   EUntrustedAccess untrusted_access)
{
	LLCommandHandlerRegistry::getInstance()->add(cmd, untrusted_access, this);
}

LLCommandHandler::~LLCommandHandler()
{
	// Do not care about unregistering these, all the handlers should be static
	// objects.
}

//static
void LLCommandHandler::dump()
{
	LLCommandHandlerRegistry::getInstance()->dump();
}

//---------------------------------------------------------------------------
// Public interface
//---------------------------------------------------------------------------

//static
bool LLCommandDispatcher::dispatch(const std::string& cmd, const LLSD& params,
								   const LLSD& query_map, LLMediaCtrl* web,
								   const std::string& nav_type,
								   bool trusted_browser)
{
	return LLCommandHandlerRegistry::getInstance()->dispatch(cmd, params,
															 query_map, web,
															 nav_type,
															 trusted_browser);
}

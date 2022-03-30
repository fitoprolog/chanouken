/**
 * @file llavataractions.cpp
 * @brief avatar-related actions (IM, teleporting, etc)
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

#include "llviewerprecompiledheaders.h"

#include "llavataractions.h"

#include "llcachename.h"
#include "llnotifications.h"

#include "llagent.h"
#include "llcommandhandler.h"
#include "llfloateravatarinfo.h"
#include "llfloaterfriends.h"
#include "llfloaterinspect.h"
#include "llfloatermute.h"
#include "llfloaterpay.h"
#include "llinventorymodel.h"
#include "llimmgr.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewermessage.h"	// send_improved_im() handle_lure() give_money()

//-----------------------------------------------------------------------------
// Command handler
//-----------------------------------------------------------------------------

static void on_name_cache_mute(const LLUUID& agent_id, const std::string& name,
							   bool is_group, bool mute_it)
{
	LLMute mute(agent_id, name, LLMute::AGENT);
	if (LLMuteList::isMuted(agent_id, name))
	{
		if (!mute_it)
		{
			LLMuteList::remove(mute);
		}
	}
	else
	{
		if (mute_it)
		{
			LLMuteList::add(mute);
		}
		LLFloaterMute::selectMute(agent_id);
	}
}

class LLAgentHandler : public LLCommandHandler
{
public:
	LLAgentHandler() : LLCommandHandler("agent", UNTRUSTED_THROTTLE) {}

	bool handle(const LLSD& params, const LLSD& query_map, LLMediaCtrl* web)
	{
		if (params.size() < 2) return false;
		LLUUID agent_id;
		if (!agent_id.set(params[0], false))
		{
			return false;
		}

		const std::string verb = params[1].asString();
		if (verb == "about" || verb == "username" || verb == "displayname" ||
			verb == "completename")
		{
			LLFloaterAvatarInfo::show(agent_id);
		}
		else if (verb == "inspect")
		{
			HBFloaterInspectAvatar::show(agent_id);
		}
		else if (verb == "pay")
		{
			LLAvatarActions::pay(agent_id);
		}
		else if (verb == "offerteleport")
		{
			LLAvatarActions::offerTeleport(agent_id);
		}
		else if (verb == "im")
		{
			LLAvatarActions::startIM(agent_id);
		}
		else if (verb == "requestfriend")
		{
			LLAvatarActions::requestFriendshipDialog(agent_id);
		}
		else if (verb == "mute" || verb == "unmute")
		{
			if (!gCacheNamep) return false;	// Paranoia
			gCacheNamep->get(agent_id, false,
							 boost::bind(&on_name_cache_mute,
										 _1, _2, _3, verb == "mute"));
		}
		else
		{
			return false;
		}

		return true;
	}
};
LLAgentHandler gAgentHandler;

///////////////////////////////////////////////////////////////////////////////
// LLAvatarActions class
///////////////////////////////////////////////////////////////////////////////

static void on_avatar_name_friendship(const LLUUID& id,
									  const LLAvatarName& av_name)
{
	std::string fullname;
	if (!LLAvatarName::sLegacyNamesForFriends &&
		LLAvatarNameCache::useDisplayNames())
	{
		if (LLAvatarNameCache::useDisplayNames() == 2)
		{
			fullname = av_name.mDisplayName;
		}
		else
		{
			fullname = av_name.getNames();
		}
	}
	else
	{
		fullname = av_name.getLegacyName();
	}

	LLAvatarActions::requestFriendshipDialog(id, fullname);
}

//static
void LLAvatarActions::requestFriendshipDialog(const LLUUID& id)
{
	if (id.notNull() && gCacheNamep)
	{
		std::string fullname;
		if (gCacheNamep->getFullName(id, fullname) &&
			(LLAvatarName::sLegacyNamesForFriends ||
			 !LLAvatarNameCache::useDisplayNames()))
		{
			requestFriendshipDialog(id, fullname);
		}
		else
		{
			LLAvatarNameCache::get(id, boost::bind(&on_avatar_name_friendship,
												   _1, _2));
		}
	}
}

static bool callback_add_friend(const LLSD& notification,
								const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLUUID id = notification["payload"]["id"].asUUID();
		std::string message = response["message"].asString();
//MK
		if (gRLenabled && !gRLInterface.canSendIM(id))
		{
			message = "(Hidden)";
		}
//mk
		std::string name = notification["payload"]["name"].asString();
		LLAvatarActions::requestFriendship(id, name, message);
	}
	return false;
}

//static
void LLAvatarActions::requestFriendshipDialog(const LLUUID& id,
											  const std::string& name)
{
	if (id == gAgentID)
	{
		gNotifications.add("AddSelfFriend");
		return;
	}

	LLSD args;
	args["NAME"] = name;
	LLSD payload;
	payload["id"] = id;
	payload["name"] = name;
   	gNotifications.add("AddFriendWithMessage", args, payload,
					   callback_add_friend);
}

//static
void LLAvatarActions::requestFriendship(const LLUUID& id,
										const std::string& name,
										const std::string& message)
{
	const LLUUID& folder_id =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
	send_improved_im(id, name, message, IM_ONLINE, IM_FRIENDSHIP_OFFERED,
					 folder_id);
}

//static
void LLAvatarActions::offerTeleport(const LLUUID& id)
{
	if (id.isNull())
	{
		llwarns << "Null avatar UUID, aborted." << llendl;
	}
	else if (id == gAgentID)
	{
		llwarns << "Cannot teleport self !" << llendl;
	}
	else
	{
		uuid_vec_t ids;
		ids.push_back(id);
		handle_lure(ids);
	}
}

//static
void LLAvatarActions::offerTeleport(const uuid_vec_t& ids) 
{
	if (ids.size() > 0)
	{
		handle_lure(ids);
	}
	else
	{
		llwarns << "Tried to offer teleport to an empty list of avatars"
				<< llendl;
	}
}

static void teleport_request_callback(const LLSD& notification,
									  const LLSD& response)
{
	S32 option = 0;
	if (response.isInteger())
	{
		option = response.asInteger();
	}
	else
	{
		option = LLNotification::getSelectedOption(notification, response);
	}
	if (option == 0)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_ImprovedInstantMessage);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

		msg->nextBlockFast(_PREHASH_MessageBlock);
		msg->addBoolFast(_PREHASH_FromGroup, false);
		LLUUID target_id = notification["substitutions"]["uuid"].asUUID();
		msg->addUUIDFast(_PREHASH_ToAgentID, target_id);
		msg->addU8Fast(_PREHASH_Offline, IM_ONLINE);
		msg->addU8Fast(_PREHASH_Dialog, IM_TELEPORT_REQUEST);
		msg->addUUIDFast(_PREHASH_ID, LLUUID::null);

		// no timestamp necessary
		msg->addU32Fast(_PREHASH_Timestamp, NO_TIMESTAMP);

		std::string name;
		gAgent.buildFullname(name);
		msg->addStringFast(_PREHASH_FromAgentName, name);

//MK
		if (gRLenabled && !gRLInterface.canSendIM(target_id))
		{
			msg->addStringFast(_PREHASH_Message, "(Hidden)");
		}
		else
//mk
		{
			msg->addStringFast(_PREHASH_Message, response["message"]);
		}

		msg->addU32Fast(_PREHASH_ParentEstateID, 0);
		msg->addUUIDFast(_PREHASH_RegionID, LLUUID::null);
		msg->addVector3Fast(_PREHASH_Position, gAgent.getPositionAgent());

		msg->addBinaryDataFast(_PREHASH_BinaryBucket, EMPTY_BINARY_BUCKET,
							   EMPTY_BINARY_BUCKET_SIZE);

		gAgent.sendReliableMessage();
	}
}

//static
void LLAvatarActions::teleportRequest(const LLUUID& id)
{
	if (id.isNull())
	{
		llwarns << "Null avatar UUID, aborted." << llendl;
	}
	else if (id == gAgentID)
	{
		llwarns << "Cannot request a teleport to self !" << llendl;
	}
	else
	{
		LLAvatarName av_name;
		if (LLAvatarNameCache::get(id, &av_name))
		{
			LLSD notification;
			notification["uuid"] = id;
			notification["NAME"] = av_name.getNames();
			LLSD payload;
			gNotifications.add("TeleportRequestPrompt", notification, payload,
							   teleport_request_callback);
		}
		else	// unlikely ... they just picked this name from somewhere...
		{
			// reinvoke this very method after the name resolves
			LLAvatarNameCache::get(id, boost::bind(&teleportRequest, id));
		}
	}
}

static void on_avatar_name_cache_start_im(const LLUUID& agent_id,
										  const LLAvatarName& av_name)
{
	if (gIMMgrp)
	{
		gIMMgrp->setFloaterOpen(true);
		gIMMgrp->addSession(av_name.getLegacyName(), IM_NOTHING_SPECIAL,
							agent_id);
		make_ui_sound("UISndStartIM");
	}
}

//static
void LLAvatarActions::startIM(const LLUUID& id)
{
	if (id.isNull())
	{
		llwarns << "Null avatar UUID, aborted." << llendl;
	}
	else if (id == gAgentID)
	{
		llwarns << "Cannot IM to self !" << llendl;
	}
	else
	{
		LLAvatarNameCache::get(id, boost::bind(&on_avatar_name_cache_start_im,
											   _1, _2));
	}
}

//static
void LLAvatarActions::startIM(const uuid_vec_t& ids, bool friends)
{
	if (!gIMMgrp) return;

	S32 count = ids.size();
	if (count > 1)
	{
		// Group IM
		LLUUID session_id;
		session_id.generate();
		gIMMgrp->setFloaterOpen(true);
		// *TODO: translate
		gIMMgrp->addSession(friends ? "Friends Conference"
									: "Avatars Conference",
							IM_SESSION_CONFERENCE_START, ids[0], ids);
		make_ui_sound("UISndStartIM");
	}
	else if (count == 1)
	{
		// Single avatar
		LLUUID agent_id = ids[0];
		startIM(agent_id);
	}
	else
	{
		llwarns << "Tried to initiate an IM conference with an empty list of participants"
				<< llendl;
	}
}

//static
void LLAvatarActions::pay(const LLUUID& id)
{
	if (id.isNull())
	{
		llwarns << "Null avatar UUID, aborted." << llendl;
	}
	else
	{
		LLFloaterPay::payDirectly(&give_money, id, false);
	}
}

//static
void LLAvatarActions::buildAvatarsList(std::vector<LLAvatarName> avatar_names,
									   std::string& avatars, bool force_legacy,
									   const std::string& separator)
{
	U32 name_usage = force_legacy ? 0 : LLAvatarNameCache::useDisplayNames();
	std::sort(avatar_names.begin(), avatar_names.end());
	for (std::vector<LLAvatarName>::const_iterator it = avatar_names.begin(),
												   end = avatar_names.end();
		 it != end; ++it)
	{
		if (!avatars.empty())
		{
			avatars.append(separator);
		}

		switch (name_usage)
		{
			case 2:
				avatars.append(it->mDisplayName);
				break;

			case 1:
				avatars.append(it->getNames());
				break;

			default:
				avatars.append(it->getLegacyName());
		}
	}
}

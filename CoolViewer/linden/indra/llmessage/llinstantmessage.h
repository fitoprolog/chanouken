/**
 * @file llinstantmessage.h
 * @brief Constants and declarations used by instant messages.
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

#ifndef LL_LLINSTANTMESSAGE_H
#define LL_LLINSTANTMESSAGE_H

#include <string>

#include "lluuid.h"
#include "llvector3.h"

// The ImprovedInstantMessage only supports 8 bits in the "Dialog" field, so do
// not go past the byte boundary
enum EInstantMessage
{
	// Default. ID is meaningless, nothing in the binary bucket.
	IM_NOTHING_SPECIAL = 0,

	// Pops up a messagebox with a single OK button
	IM_MESSAGEBOX = 1,

	// Pops up a countdown messagebox with a single OK button
	// IM_MESSAGEBOX_COUNTDOWN = 2,

	// You have been invited to join a group. ID is the group id. The binary
	// bucket contains a null terminated string representation of the officer
	// or member status and join cost for the invitee. The format is 1 byte for
	// officer/member (O for officer, M for member), and as many bytes as
	// necessary for cost.
	IM_GROUP_INVITATION = 3,

	// Inventory offer. ID is the transaction id. Binary bucket is a list of
	// inventory uuid and type.
	IM_INVENTORY_OFFERED = 4,
	IM_INVENTORY_ACCEPTED = 5,
	IM_INVENTORY_DECLINED = 6,

	// Group vote - DEPRECATED as part of vote removal - DEV-24856
	IM_GROUP_VOTE = 7,

	// Group message. This means that the message is meant for everyone in the
	// agent's group. This will result in a database query to find all
	// participants and start an im session.
	IM_GROUP_MESSAGE_DEPRECATED = 8,

	// Task inventory offer. ID is the transaction id. Binary bucket is a
	// (mostly) complete packed inventory item.
	IM_TASK_INVENTORY_OFFERED = 9,
	IM_TASK_INVENTORY_ACCEPTED = 10,
	IM_TASK_INVENTORY_DECLINED = 11,

	// Copied as pending, type LL_NOTHING_SPECIAL, for new users used by
	// offline tools
	IM_NEW_USER_DEFAULT = 12,

	//
	// Session based messaging - the way that people usually actually
	// communicate with each other.
	//

	// Invite users to a session.
	IM_SESSION_INVITE = 13,

	IM_SESSION_P2P_INVITE = 14,

	// Start a session with your group
	IM_SESSION_GROUP_START = 15,

	// Start a session without a calling card (finder or objects)
	IM_SESSION_CONFERENCE_START = 16,

	// Send a message to a session.
	IM_SESSION_SEND = 17,

	// Leave a session
	IM_SESSION_LEAVE = 18,

	// An instant message from an object - for differentiation on the viewer,
	// since you can't IM an object yet.
	IM_FROM_TASK = 19,

	// Sent an IM to a busy user, this is the auto response
	IM_BUSY_AUTO_RESPONSE = 20,

	// Shows the message in the console and chat history
	IM_CONSOLE_AND_CHAT_HISTORY = 21,

	// IM Types used for luring your friends
	IM_LURE_USER = 22,
	IM_LURE_ACCEPTED = 23,
	IM_LURE_DECLINED = 24,
	IM_GODLIKE_LURE_USER = 25,
	IM_TELEPORT_REQUEST = 26,

	// IM that notifie of a new group election - DEPRECATED
	IM_GROUP_ELECTION_DEPRECATED = 27,

	// IM to tell the user to go to an URL. Put a text message in the message
	// field, and put the url with a trailing \0 in the binary bucket.
	IM_GOTO_URL = 28,

	// A message generated by a script which we do not want to be sent through
	// e-mail. Similar to IM_FROM_TASK, but it is shown as an alert on the
	// viewer.
	IM_FROM_TASK_AS_ALERT = 31,

	// IM from group officer to all group members.
	IM_GROUP_NOTICE = 32,
	IM_GROUP_NOTICE_INVENTORY_ACCEPTED = 33,
	IM_GROUP_NOTICE_INVENTORY_DECLINED = 34,

	IM_GROUP_INVITATION_ACCEPT = 35,
	IM_GROUP_INVITATION_DECLINE = 36,

	IM_GROUP_NOTICE_REQUESTED = 37,

	IM_FRIENDSHIP_OFFERED = 38,
	IM_FRIENDSHIP_ACCEPTED = 39,
	IM_FRIENDSHIP_DECLINED_DEPRECATED = 40,

	IM_TYPING_START = 41,
	IM_TYPING_STOP = 42,

	IM_COUNT
};

constexpr U8 IM_ONLINE = 0;
constexpr U8 IM_OFFLINE = 1;
constexpr U32 NO_TIMESTAMP = 0;
constexpr S32 EMPTY_BINARY_BUCKET_SIZE = 1;

extern const char EMPTY_BINARY_BUCKET[];
extern const std::string SYSTEM_FROM;
extern const std::string INCOMING_IM;
extern const std::string INTERACTIVE_SYSTEM_FROM;

void pack_instant_message(const LLUUID& from_id, bool from_group,
						  const LLUUID& session_id, const LLUUID& to_id,
						  const std::string& name, const std::string& message,
						  U8 offline = IM_ONLINE,
						  EInstantMessage dialog = IM_NOTHING_SPECIAL,
						  const LLUUID& id = LLUUID::null,
						  U32 parent_estate_id = 0,
						  const LLUUID& region_id = LLUUID::null,
						  const LLVector3& position = LLVector3::zero,
						  U32 timestamp = NO_TIMESTAMP,
						  const U8* binary_bucket = (U8*)EMPTY_BINARY_BUCKET,
						  S32 binary_bucket_size = EMPTY_BINARY_BUCKET_SIZE);

#endif // LL_LLINSTANTMESSAGE_H

/**
 * @file lldbstrings.h
 * @brief Database String Lengths.
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

#ifndef LL_LLDBSTRINGS_H
#define LL_LLDBSTRINGS_H

/**
 * Defines the length of strings that are stored in the database (and
 * the size of the buffer large enough to hold each one)
 */

// asset.name							varchar(63)
// -also-
// user_inventory_item.name				varchar(63)
// -also-
// user_inventory_folder.name			varchar(63)  was CAT_NAME_SIZE
// Must be >= DB_FULL_NAME_STR_LEN so that calling cards work
constexpr S32 DB_INV_ITEM_NAME_STR_LEN		= 63;  // was MAX_ASSET_NAME_LENGTH
constexpr S32 DB_INV_ITEM_NAME_BUF_SIZE		= 64;  // was ITEM_NAME_SIZE

// asset.description					varchar(127)
// -also-
// user_inventory_item.description		varchar(127)
constexpr S32 DB_INV_ITEM_DESC_STR_LEN		= 127; // was MAX_ASSET_DESCRIPTION_LENGTH
constexpr S32 DB_INV_ITEM_DESC_BUF_SIZE		= 128; // was ITEM_DESC_SIZE

// groups.name							varchar(35)
constexpr S32 DB_GROUP_NAME_STR_LEN			= 35;
constexpr S32 DB_GROUP_NAME_BUF_SIZE		= 36;
constexpr S32 DB_GROUP_NAME_MIN_LEN			= 4;

//group_roles.name
constexpr U32 DB_GROUP_ROLE_NAME_STR_LEN	= 20;
constexpr U32 DB_GROUP_ROLE_NAME_BUF_SIZE	= DB_GROUP_ROLE_NAME_STR_LEN + 1;

//group_roles.title
constexpr U32 DB_GROUP_ROLE_TITLE_STR_LEN	= 20;
constexpr U32 DB_GROUP_ROLE_TITLE_BUF_SIZE	= DB_GROUP_ROLE_TITLE_STR_LEN + 1;

// group.charter						text
constexpr S32 DB_GROUP_CHARTER_STR_LEN		= 511;
constexpr S32 DB_GROUP_CHARTER_BUF_SIZE		= 512;

// group.officer_title					varchar(20)
// -also-
// group.member_title					varchar(20)
constexpr S32 DB_GROUP_TITLE_STR_LEN		= 20;
constexpr S32 DB_GROUP_TITLE_BUF_SIZE		= 21;

// Since chat and im both dump into the database text message log,
// they derive their max size from the same constant.
constexpr S32 MAX_MSG_STR_LEN = 1023;
constexpr S32 MAX_MSG_BUF_SIZE = 1024;

// instant_message.message				text
constexpr S32 DB_IM_MSG_STR_LEN	 			= MAX_MSG_STR_LEN;
constexpr S32 DB_IM_MSG_BUF_SIZE 			= MAX_MSG_BUF_SIZE;

// groupnotices
constexpr S32 DB_GROUP_NOTICE_SUBJ_STR_LEN	= 63;
constexpr S32 DB_GROUP_NOTICE_SUBJ_STR_SIZE	= 64;
constexpr S32 DB_GROUP_NOTICE_MSG_STR_LEN	= MAX_MSG_STR_LEN - DB_GROUP_NOTICE_SUBJ_STR_LEN;
constexpr S32 DB_GROUP_NOTICE_MSG_STR_SIZE	= MAX_MSG_BUF_SIZE - DB_GROUP_NOTICE_SUBJ_STR_SIZE;

// log_text_message.message				text
constexpr S32 DB_CHAT_MSG_STR_LEN			= MAX_MSG_STR_LEN;
constexpr S32 DB_CHAT_MSG_BUF_SIZE 			= MAX_MSG_BUF_SIZE;

// money_stipend.description			varchar(254)
constexpr S32 DB_STIPEND_DESC_STR_LEN		= 254;
constexpr S32 DB_STIPEND_DESC_BUF_SIZE 		= 255;

// script_email_message.from_email		varchar(78)
constexpr S32 DB_EMAIL_FROM_STR_LEN			= 78;
constexpr S32 DB_EMAIL_FROM_BUF_SIZE		= 79;

// script_email_message.subject			varchar(72)
constexpr S32 DB_EMAIL_SUBJECT_STR_LEN		= 72;
constexpr S32 DB_EMAIL_SUBJECT_BUF_SIZE		= 73;

// system_globals.motd					varchar(254)
constexpr S32 DB_MOTD_STR_LEN				= 254;
constexpr S32 DB_MOTD_BUF_SIZE				= 255;

// Must be <= user_inventory_item.name	so that calling cards work
// First name + " " + last name...or a system assigned "from" name
// instant_message.from_agent_name		varchar(63)
// -also-
// user_mute.mute_agent_name			varchar(63)
constexpr S32 DB_FULL_NAME_STR_LEN			= 63;
constexpr S32 DB_FULL_NAME_BUF_SIZE			= 64;  // was USER_NAME_SIZE

// user.username						varchar(31)
constexpr S32 DB_FIRST_NAME_STR_LEN			= 31;
constexpr S32 DB_FIRST_NAME_BUF_SIZE		= 32; // was MAX_FIRST_NAME

// user_last_name.name					varchar(31)
constexpr S32 DB_LAST_NAME_STR_LEN			= 31;
constexpr S32 DB_LAST_NAME_BUF_SIZE			= 32; // was MAX_LAST_NAME

// user.password						varchar(100)
constexpr S32 DB_USER_PASSWORD_STR_LEN		= 100;
constexpr S32 DB_USER_PASSWORD_BUF_SIZE		= 101; // was MAX_PASSWORD

// user.email							varchar(254)
constexpr S32 DB_USER_EMAIL_ADDR_STR_LEN	= 254;
constexpr S32 DB_USER_EMAIL_ADDR_BUF_SIZE	= 255;

// user.about							text
constexpr S32 DB_USER_ABOUT_STR_LEN			= 511;
constexpr S32 DB_USER_ABOUT_BUF_SIZE		= 512;

// user.fl_about_text					text
// Must be 255 not 256 as gets packed into message Variable 1
constexpr S32 DB_USER_FL_ABOUT_STR_LEN		= 254;
constexpr S32 DB_USER_FL_ABOUT_BUF_SIZE		= 255;

// user.profile_url					text
// Must be 255 not 256 as gets packed into message Variable 1
constexpr S32 DB_USER_PROFILE_URL_STR_LEN	= 254;
constexpr S32 DB_USER_PROFILE_URL_BUF_SIZE	= 255;

// user.want_to							varchar(254)
constexpr S32 DB_USER_WANT_TO_STR_LEN		= 254;
constexpr S32 DB_USER_WANT_TO_BUF_SIZE		= 255;

// user.skills							varchar(254)
constexpr S32 DB_USER_SKILLS_STR_LEN		= 254;
constexpr S32 DB_USER_SKILLS_BUF_SIZE		= 255;

// user_nv.name							varchar(128)
constexpr S32 DB_NV_NAME_STR_LEN			= 128;
constexpr S32 DB_NV_NAME_BUF_SIZE			= 129;

// user_start_location.location_name	varchar(254)
constexpr S32 DB_START_LOCATION_STR_LEN		= 254;
constexpr S32 DB_START_LOCATION_BUF_SIZE	= 255;

// money_tax_assessment.sim				varchar(100)
//constexpr S32 DB_SIM_NAME_STR_LEN			= 100;
//constexpr S32 DB_SIM_NAME_BUF_SIZE		= 101;

// born on date							date
constexpr S32 DB_BORN_STR_LEN				= 15;
constexpr S32 DB_BORN_BUF_SIZE				= 16;

// place.name
constexpr S32 DB_PLACE_NAME_LEN				= 63;
constexpr S32 DB_PLACE_NAME_SIZE			= 64;
constexpr S32 DB_PARCEL_NAME_LEN			= 63;
constexpr S32 DB_PARCEL_NAME_SIZE			= 64;

// place.desc
constexpr S32 DB_PLACE_DESC_LEN				= 255;
constexpr S32 DB_PLACE_DESC_SIZE			= 256;
constexpr S32 DB_PARCEL_DESC_LEN			= 255;
constexpr S32 DB_PARCEL_DESC_SIZE			= 256;
constexpr S32 DB_PARCEL_MUSIC_URL_LEN		= 255;
constexpr S32 DB_PARCEL_MEDIA_URL_LEN		= 255;
constexpr S32 DB_PARCEL_MUSIC_URL_SIZE		= 256;

// Date time that is easily human readable
constexpr S32 DB_DATETIME_STR_LEN			= 35;
constexpr S32 DB_DATETIME_BUF_SIZE			= 36;

// Date time that is not easily human readable
constexpr S32 DB_TERSE_DATETIME_STR_LEN		= 15;
constexpr S32 DB_TERSE_DATETIME_BUF_SIZE	= 16;

// indra.simulator constants
constexpr S32 DB_SIM_NAME_STR_LEN			= 35;
constexpr S32 DB_SIM_NAME_BUF_SIZE			= 36;
constexpr S32 DB_HOST_NAME_STR_LEN			= 100;
constexpr S32 DB_HOST_NAME_BUF_SIZE			= 101;
constexpr S32 DB_ESTATE_NAME_STR_LEN		= 63;
constexpr S32 DB_ESTATE_NAME_BUF_SIZE		= DB_ESTATE_NAME_STR_LEN + 1;

// user_note.note
constexpr S32 DB_USER_NOTE_LEN				= 1023;
constexpr S32 DB_USER_NOTE_SIZE				= 1024;

// pick.name
constexpr S32 DB_PICK_NAME_LEN				= 63;
constexpr S32 DB_PICK_NAME_SIZE				= 64;

// pick.desc
constexpr S32 DB_PICK_DESC_LEN				= 1023;
constexpr S32 DB_PICK_DESC_SIZE				= 1024;

#endif  // LL_LLDBSTRINGS_H

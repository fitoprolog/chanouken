/**
 * @file lltransactiontypes.h
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

#ifndef LL_LLTRANSACTIONTYPES_H
#define LL_LLTRANSACTIONTYPES_H

// *NOTE: The constants in this file are also in the
// transaction_description table in the database. If you add a
// constant here, please add it to the database. eg:
//
//   insert into transaction_description
//     set type = 1000, description = 'Object Claim';
//
// Also add it to the various L$ string lookups on the dataserver
// in lldatamoney

// Money transaction failure codes
constexpr U8 TRANS_FAIL_SIMULATOR_TIMEOUT	= 1;
constexpr U8 TRANS_FAIL_DATASERVER_TIMEOUT	= 2;
constexpr U8 TRANS_FAIL_APPLICATION         = 3;

// Codes up to 999 for error conditions
constexpr S32 TRANS_NULL				= 0;

// Codes 1000-1999 reserved for one-time charges
constexpr S32 TRANS_OBJECT_CLAIM		= 1000;
constexpr S32 TRANS_LAND_CLAIM			= 1001;
constexpr S32 TRANS_GROUP_CREATE		= 1002;
constexpr S32 TRANS_OBJECT_PUBLIC_CLAIM	= 1003;
constexpr S32 TRANS_GROUP_JOIN		    = 1004; // May be moved to group transactions eventually
constexpr S32 TRANS_TELEPORT_CHARGE		= 1100; // FF not sure why this jumps to 1100...
constexpr S32 TRANS_UPLOAD_CHARGE		= 1101;
constexpr S32 TRANS_LAND_AUCTION		= 1102;
constexpr S32 TRANS_CLASSIFIED_CHARGE	= 1103;

// Codes 2000-2999 reserved for recurrent charges
constexpr S32 TRANS_OBJECT_TAX			= 2000;
constexpr S32 TRANS_LAND_TAX			= 2001;
constexpr S32 TRANS_LIGHT_TAX			= 2002;
constexpr S32 TRANS_PARCEL_DIR_FEE		= 2003;
constexpr S32 TRANS_GROUP_TAX		    = 2004; // Taxes incurred as part of group membership
constexpr S32 TRANS_CLASSIFIED_RENEW	= 2005;

// Codes 2100-2999 reserved for recurring billing services
// New codes can be created through an admin interface so may not
// automatically end up in the list below :-(
// So make sure you check the transaction_description table
constexpr S32 TRANS_RECURRING_GENERIC	= 2100;

// Codes 3000-3999 reserved for inventory transactions
constexpr S32 TRANS_GIVE_INVENTORY		= 3000;

// Codes 5000-5999 reserved for transfers between users
constexpr S32 TRANS_OBJECT_SALE			= 5000;
constexpr S32 TRANS_GIFT				= 5001;
constexpr S32 TRANS_LAND_SALE			= 5002;
constexpr S32 TRANS_REFER_BONUS			= 5003;
constexpr S32 TRANS_INVENTORY_SALE		= 5004;
constexpr S32 TRANS_REFUND_PURCHASE		= 5005;
constexpr S32 TRANS_LAND_PASS_SALE		= 5006;
constexpr S32 TRANS_DWELL_BONUS			= 5007;
constexpr S32 TRANS_PAY_OBJECT			= 5008;
constexpr S32 TRANS_OBJECT_PAYS			= 5009;

// Codes 5100-5999 reserved for recurring billing transfers between users
// New codes can be created through an admin interface so may not
// automatically end up in the list below :-(
// So make sure you check the transaction_description table
constexpr S32 TRANS_RECURRING_GENERIC_USER	= 5100;

// Codes 6000-6999 reserved for group transactions
//constexpr S32 TRANS_GROUP_JOIN		= 6000;  //reserved for future use
constexpr S32 TRANS_GROUP_LAND_DEED		= 6001;
constexpr S32 TRANS_GROUP_OBJECT_DEED	= 6002;
constexpr S32 TRANS_GROUP_LIABILITY		= 6003;
constexpr S32 TRANS_GROUP_DIVIDEND		= 6004;
constexpr S32 TRANS_MEMBERSHIP_DUES		= 6005;

// Codes 8000-8999 reserved for one-type credits
constexpr S32 TRANS_OBJECT_RELEASE		= 8000;
constexpr S32 TRANS_LAND_RELEASE		= 8001;
constexpr S32 TRANS_OBJECT_DELETE		= 8002;
constexpr S32 TRANS_OBJECT_PUBLIC_DECAY	= 8003;
constexpr S32 TRANS_OBJECT_PUBLIC_DELETE= 8004;

// Code 9000-9099 reserved for usertool transactions
constexpr S32 TRANS_LINDEN_ADJUSTMENT	= 9000;
constexpr S32 TRANS_LINDEN_GRANT		= 9001;
constexpr S32 TRANS_LINDEN_PENALTY		= 9002;
constexpr S32 TRANS_EVENT_FEE			= 9003;
constexpr S32 TRANS_EVENT_PRIZE			= 9004;

// These must match entries in money_stipend table in MySQL
// Codes 10000-10999 reserved for stipend credits
constexpr S32 TRANS_STIPEND_BASIC		= 10000;
constexpr S32 TRANS_STIPEND_DEVELOPER	= 10001;
constexpr S32 TRANS_STIPEND_ALWAYS		= 10002;
constexpr S32 TRANS_STIPEND_DAILY		= 10003;
constexpr S32 TRANS_STIPEND_RATING		= 10004;
constexpr S32 TRANS_STIPEND_DELTA       = 10005;

#endif

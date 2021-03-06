/**
 * @file llconfirmationmanager.cpp
 * @brief LLConfirmationManager class implementation
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

#include "linden_common.h"

#include "llconfirmationmanager.h"

#include "llnotifications.h"

static bool onConfirmAlert(const LLSD& notification, const LLSD& response,
						   LLConfirmationManager::ListenerBase* listener)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		listener->confirmed("");
	}

	delete listener;
	return false;
}

static bool onConfirmAlertPassword(const LLSD& notification,
								   const LLSD& response,
								   LLConfirmationManager::ListenerBase* listener)
{
	std::string text = response["message"].asString();
	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option == 0)
	{
		listener->confirmed(text);
	}

	delete listener;
	return false;
}


void LLConfirmationManager::confirm(Type type, const std::string& action,
									ListenerBase* listener)
{
	LLSD args;
	args["ACTION"] = action;

	switch (type)
	{
		case TYPE_CLICK:
			gNotifications.add("ConfirmPurchase", args, LLSD(),
							   boost::bind(onConfirmAlert, _1, _2, listener));
		  break;

		case TYPE_PASSWORD:
			gNotifications.add("ConfirmPurchasePassword", args, LLSD(),
							   boost::bind(onConfirmAlertPassword, _1, _2,
										   listener));
		  break;

		case TYPE_NONE:
		default:
		  listener->confirmed("");
	}
}

void LLConfirmationManager::confirm(const std::string& type,
									const std::string& action,
									ListenerBase* listener)
{
	Type decoded_type = TYPE_NONE;

	if (type == "click")
	{
		decoded_type = TYPE_CLICK;
	}
	else if (type == "password")
	{
		decoded_type = TYPE_PASSWORD;
	}

	confirm(decoded_type, action, listener);
}

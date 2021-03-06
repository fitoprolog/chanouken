/**
 * @file llprefsnetwork.h
 * @brief Network preferences panel
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

#ifndef LL_LLPREFSNETWORK_H
#define LL_LLPREFSNETWORK_H

#include "llpanel.h"

class LLButton;

class LLPrefsNetwork final : public LLPanel
{
public:
	LLPrefsNetwork();
	~LLPrefsNetwork() override;

	bool postBuild() override;
	void draw() override;

	void apply();
	void cancel();

private:
	static void onHttpTextureFetchToggled(LLUICtrl* ctrl, void* data);
	static void onClickClearDiskCache(void*);
	static void onClickSetCache(void*);
	static void onClickResetCache(void*);
	static void onCommitPort(LLUICtrl* ctrl, void*);
	static void onCommitSocks5ProxyEnabled(LLUICtrl* ctrl, void* data);
	static void onClickTestProxy(void* user_data);
	static void onSocksSettingsModified(LLUICtrl* ctrl, void* data);
	static void onSocksAuthChanged(LLUICtrl* ctrl, void* data);
	static void updateProxyEnabled(LLPrefsNetwork * self, bool enabled, std::string authtype);

	static void onClickClearBrowserCache(void*);
	static void onClickClearCookies(void*);
	static bool callback_clear_browser_cache(const LLSD& notification, const LLSD& response);
	static bool callback_clear_cookies(const LLSD& notification, const LLSD& response);
	static void onCommitWebProxyEnabled(LLUICtrl* ctrl, void* data);

	static void setCacheCallback(std::string& dir_name, void* data);

private:
	LLButton*				mSetCacheButton;

	static LLPrefsNetwork*	sInstance;
	static bool				sSocksSettingsChanged;
};

#endif

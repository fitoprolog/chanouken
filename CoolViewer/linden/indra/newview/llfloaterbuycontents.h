/**
 * @file llfloaterbuycontents.h
 * @author James Cook
 * @brief LLFloaterBuyContents class header file
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

/**
 * Shows the contents of an object and their permissions when you click
 * "Buy..." on an object with "Sell Contents" checked.
 */

#ifndef LL_LLFLOATERBUYCONTENTS_H
#define LL_LLFLOATERBUYCONTENTS_H

#include "llfloater.h"
#include "llsafehandle.h"

#include "llvoinventorylistener.h"

class LLObjectSelection;
class LLSaleInfo;
class LLViewerObject;

class LLFloaterBuyContents final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterBuyContents>,
	public LLVOInventoryListener
{
	friend class LLUISingleton<LLFloaterBuyContents,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLFloaterBuyContents);

public:
	~LLFloaterBuyContents() override;

	bool postBuild() override;

	static void show(const LLSaleInfo& sale_info);

protected:
	// LLVOInventoryListener interface
	void inventoryChanged(LLViewerObject* obj,
						  LLInventoryObject::object_list_t* inv,
						  S32 serial_num, void* data) override;

private:
	// Open only via show().
	LLFloaterBuyContents(const LLSD&);

	void requestObjectInventories();

	static void onClickBuy(void* data);
	static void onClickCancel(void* data);

protected:
	LLSafeHandle<LLObjectSelection>	mObjectSelection;
	LLSaleInfo						mSaleInfo;
};

#endif

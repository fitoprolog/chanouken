/** 
 * @file llinventoryclipboard.h
 * @brief LLInventoryClipboard class header file
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

#ifndef LL_LLINVENTORYCLIPBOARD_H
#define LL_LLINVENTORYCLIPBOARD_H

#include <vector>

#include "lluuid.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryClipboard
//
// This class is used to cut/copy/paste inventory items around the
// world. This class is accessed through a singleton (only one
// inventory clipboard for now) which can be referenced using the
// instance() method.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryClipboard
{
public:
	// this method adds to the current list.
	void add(const LLUUID& object);

	// add for Cut operation
	void addCut(const LLUUID& object);

	// this stores a single inventory object
	void store(const LLUUID& object);

	// this method stores an array of objects
	void store(const uuid_vec_t& inventory_objects);

	// this method gets the objects in the clipboard by copying them
	// into the array provided.
	void retrieve(uuid_vec_t& inventory_objects) const;

	// this method gets the objects in the clipboard by copying them
	// into the array provided.
	void retrieveCuts(uuid_vec_t& inventory_objects) const;

	// this method empties out the clipboard
	void reset();

	// returns true if the clipboard has something pasteable in it.
	bool hasContents() const;
	bool hasCopiedContents() const;
	bool hasCutContents() const;

	bool isCopied(const LLUUID& id) const;
	bool isCut(const LLUUID& id) const;

protected:
	static LLInventoryClipboard* sInstance;

	uuid_vec_t mObjects;
	uuid_vec_t mCutObjects;

public:
	// please don't actually call these
	LLInventoryClipboard();
	~LLInventoryClipboard();

private:
	// please don't implement these
	LLInventoryClipboard(const LLInventoryClipboard&);
	LLInventoryClipboard& operator=(const LLInventoryClipboard&);
};

extern LLInventoryClipboard gInventoryClipBoard;

#endif // LL_LLINVENTORYCLIPBOARD_H

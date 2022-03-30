/**
 * @file llaisapi.h
 * @brief classes and functions definitions for interfacing with the v3+ ais
 * inventory service.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_LLAISAPI_H
#define LL_LLAISAPI_H

#include "llcoproceduremanager.h"
#include "llcorehttputil.h"
#include "llfastmap.h"
#include "llhttpretrypolicy.h"

#include "llviewerinventory.h"

// Purely static class
class AISAPI
{
	AISAPI() = delete;
	~AISAPI() = delete;

protected:
	LOG_CLASS(AISAPI);

public:
	typedef boost::function<void(const LLUUID& item_id)> completion_t;

	static bool isAvailable();

	static void CreateInventory(const LLUUID& parent_id,
								const LLSD& new_inventory,
								completion_t callback = completion_t());

	static void SlamFolder(const LLUUID& folder_id, const LLSD& new_inventory,
						   completion_t callback = completion_t());

	static void RemoveCategory(const LLUUID& category_id,
							   completion_t callback = completion_t());

	static void RemoveItem(const LLUUID& item_id,
						   completion_t callback = completion_t());

	static void PurgeDescendents(const LLUUID& category_id,
								 completion_t callback = completion_t());

	static void UpdateCategory(const LLUUID& category_id, const LLSD& updates,
							   completion_t callback = completion_t());

	static void UpdateItem(const LLUUID& item_id, const LLSD& updates,
						   completion_t callback = completion_t());

	static void CopyLibraryCategory(const LLUUID& source_id,
									const LLUUID& dest_id,
									bool copy_subfolders,
									completion_t callback = completion_t());

private:
	typedef enum {
		COPYINVENTORY,
		SLAMFOLDER,
		REMOVECATEGORY,
		REMOVEITEM,
		PURGEDESCENDENTS,
		UPDATECATEGORY,
		UPDATEITEM,
		COPYLIBRARYCATEGORY
	} COMMAND_TYPE;

	typedef boost::function<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t,
								 const std::string, LLSD,
								 LLCore::HttpOptions::ptr_t,
								 LLCore::HttpHeaders::ptr_t)> invokationFn_t;

	static void EnqueueAISCommand(const std::string& proc_name,
								  LLCoprocedureManager::coprocedure_t proc);
	static void onIdle(void*);

	static bool getInvCap(std::string& cap);
	static bool getLibCap(std::string& cap);

	static void InvokeAISCommandCoro(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t adapter,
									 invokationFn_t invoke, std::string url,
									 LLUUID target_id, LLSD body,
									 completion_t callback, COMMAND_TYPE type);

private:
	typedef std::pair<std::string,
					  LLCoprocedureManager::coprocedure_t> ais_query_item_t;
	static std::list<ais_query_item_t> sPostponedQuery;
};

class AISUpdate
{
protected:
	LOG_CLASS(AISUpdate);

public:
	AISUpdate(const LLSD& update);
	void parseUpdate(const LLSD& update);
	void parseMeta(const LLSD& update);
	void parseContent(const LLSD& update);
	void parseUUIDArray(const LLSD& content, const std::string& name,
						uuid_list_t& ids);
	void parseLink(const LLSD& link_map);
	void parseItem(const LLSD& link_map);
	void parseCategory(const LLSD& link_map);
	void parseDescendentCount(const LLUUID& category_id, const LLSD& embedded);
	void parseEmbedded(const LLSD& embedded);
	void parseEmbeddedLinks(const LLSD& links);
	void parseEmbeddedItems(const LLSD& links);
	void parseEmbeddedCategories(const LLSD& links);
	void parseEmbeddedItem(const LLSD& item);
	void parseEmbeddedCategory(const LLSD& category);
	void doUpdate();

private:
	void clearParseResults();

private:
	typedef fast_hmap<LLUUID, S32> uuid_int_map_t;
	uuid_int_map_t			mCatDescendentDeltas;
	uuid_int_map_t			mCatDescendentsKnown;
	uuid_int_map_t			mCatVersionsUpdated;

	typedef fast_hmap<LLUUID, LLPointer<LLViewerInventoryItem> >
		deferred_item_map_t;
	deferred_item_map_t		mItemsCreated;
	deferred_item_map_t		mItemsUpdated;

	typedef fast_hmap<LLUUID, LLPointer<LLViewerInventoryCategory> >
		deferred_category_map_t;
	deferred_category_map_t	mCategoriesCreated;
	deferred_category_map_t	mCategoriesUpdated;

	// These keep track of uuid's mentioned in meta values.
	// Useful for filtering out which content we are interested in.
	uuid_list_t				mObjectsDeletedIds;
	uuid_list_t				mItemIds;
	uuid_list_t				mCategoryIds;
};

#endif	// LL_LLAISAPI_H

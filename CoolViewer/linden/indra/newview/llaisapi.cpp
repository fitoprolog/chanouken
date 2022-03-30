/**
 * @file llaisapi.cpp
 * @brief classes and functions implementation for interfacing with the v3+ ais
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

#include "llviewerprecompiledheaders.h"

#include "llaisapi.h"

#include "llcallbacklist.h"
#include "llnotifications.h"
#include "llsdutil.h"

#include "llagent.h"
#include "llinventorymodel.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"

//static
std::list<AISAPI::ais_query_item_t> AISAPI::sPostponedQuery;

//-----------------------------------------------------------------------------
// Classes for AISv3 support.
//-----------------------------------------------------------------------------

//static
bool AISAPI::isAvailable()
{
	static LLCachedControl<bool> use_ais(gSavedSettings, "UseAISForInventory");
	bool available = use_ais && gAgent.hasRegionCapability("InventoryAPIv3");

	static bool pool_created = false;
	if (available && !pool_created)
	{
		pool_created = true;
		LLCoprocedureManager::getInstance()->initializePool("AIS");
	}

	return available;
}

//static
bool AISAPI::getInvCap(std::string& cap)
{
	cap = gAgent.getRegionCapability("InventoryAPIv3");
	return !cap.empty();
}

//static
bool AISAPI::getLibCap(std::string& cap)
{
	cap = gAgent.getRegionCapability("LibraryAPIv3");
	return !cap.empty();
}

// I may be suffering from golden hammer here, but the first part of this bind
// is actually a static cast for &HttpCoroutineAdapter::postAndSuspend so that
// the compiler can identify the correct signature to select. Reads as follows:
// LLSD										: method returning LLSD
// (LLCoreHttpUtil::HttpCoroutineAdapter::*): pointer to member function of
//											  HttpCoroutineAdapter
// (const std::string&, const LLSD&, LLCore::HttpOptions::ptr_t,
//  LLCore::HttpHeaders::ptr_t)				: method signature
#define COROCAST(T)  static_cast<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::*)(const std::string&, const LLSD&, LLCore::HttpOptions::ptr_t, LLCore::HttpHeaders::ptr_t)>(T)
#define COROCAST2(T) static_cast<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::*)(const std::string&, LLCore::HttpOptions::ptr_t, LLCore::HttpHeaders::ptr_t)>(T)
#define COROCAST3(T) static_cast<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::*)(const std::string&, const std::string, LLCore::HttpOptions::ptr_t, LLCore::HttpHeaders::ptr_t)>(T)

//static
void AISAPI::CreateInventory(const LLUUID& parent_id,
							 const LLSD& new_inventory,
							 completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		return;
	}
	LLUUID tid;
	tid.generate();
	url += "/category/" + parent_id.asString() + "?tid=" + tid.asString();
	LL_DEBUGS("Inventory") << "url: " << url << " - New inventory: "
						   << ll_pretty_print_sd(new_inventory) << LL_ENDL;

	invokationFn_t postfn = boost::bind(COROCAST(&LLCoreHttpUtil::HttpCoroutineAdapter::postAndSuspend),
										// _1 -> adapter
										// _2 -> url
										// _3 -> body
										// _4 -> options
										// _5 -> headers
										_1, _2, _3, _4, _5);

	LLCoprocedureManager::coprocedure_t proc(boost::bind(&AISAPI::InvokeAISCommandCoro,
											 _1, postfn, url, parent_id,
											 new_inventory, callback,
											 COPYINVENTORY));
	EnqueueAISCommand("CreateInventory", proc);
}

//static
void AISAPI::SlamFolder(const LLUUID& folder_id, const LLSD& new_inventory,
						completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		return;
	}
	LLUUID tid;
	tid.generate();
	url += "/category/" + folder_id.asString() + "/links?tid=" +
		   tid.asString();
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t putfn = boost::bind(COROCAST(&LLCoreHttpUtil::HttpCoroutineAdapter::putAndSuspend),
									   // _1 -> adapter
									   // _2 -> url
									   // _3 -> body
									   // _4 -> options
									   // _5 -> headers
									   _1, _2, _3, _4, _5);

	LLCoprocedureManager::coprocedure_t proc(boost::bind(&AISAPI::InvokeAISCommandCoro,
											 _1, putfn, url, folder_id,
											 new_inventory, callback,
											 SLAMFOLDER));
	EnqueueAISCommand("SlamFolder", proc);
}

//static
void AISAPI::RemoveCategory(const LLUUID& category_id, completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		return;
	}
	url += "/category/" + category_id.asString();
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t delfn = boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::deleteAndSuspend),
									   // _1 -> adapter
									   // _2 -> url
									   // _3 -> body
									   // _4 -> options
									   // _5 -> headers
									   _1, _2, _4, _5);

	LLCoprocedureManager::coprocedure_t proc(boost::bind(&AISAPI::InvokeAISCommandCoro,
											 _1, delfn, url, category_id,
											 LLSD(), callback,
											 REMOVECATEGORY));
	EnqueueAISCommand("RemoveCategory", proc);
}

//static
void AISAPI::RemoveItem(const LLUUID& item_id, completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		return;
	}
	url += "/item/" + item_id.asString();
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t delfn = boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::deleteAndSuspend),
									   // _1 -> adapter
									   // _2 -> url
									   // _3 -> body
									   // _4 -> options
									   // _5 -> headers
									   _1, _2, _4, _5);

	LLCoprocedureManager::coprocedure_t proc(boost::bind(&AISAPI::InvokeAISCommandCoro,
											 _1, delfn, url, item_id, LLSD(),
											 callback, REMOVEITEM));
	EnqueueAISCommand("RemoveItem", proc);
}

//static
void AISAPI::CopyLibraryCategory(const LLUUID& source_id,
								 const LLUUID& dest_id, bool copy_subfolders,
								 completion_t callback)
{
	std::string url;
	if (!getLibCap(url))
	{
		llwarns << "No cap found" << llendl;
		return;
	}
	LL_DEBUGS("Inventory") << "Copying library category: " << source_id
						   << " => " << dest_id << LL_ENDL;
	LLUUID tid;
	tid.generate();
	url += "/category/" + source_id.asString() + "?tid=" + tid.asString();
	if (!copy_subfolders)
	{
		url += ",depth=0";
	}
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	std::string destination = dest_id.asString();
	invokationFn_t copyfn = boost::bind(COROCAST3(&LLCoreHttpUtil::HttpCoroutineAdapter::copyAndSuspend),
										// _1 -> adapter
										// _2 -> url
										// _3 -> body
										// _4 -> options
										// _5 -> headers
										_1, _2, destination, _4, _5);

	LLCoprocedureManager::coprocedure_t proc(boost::bind(&AISAPI::InvokeAISCommandCoro,
											 _1, copyfn, url, dest_id, LLSD(),
											 callback, COPYLIBRARYCATEGORY));
	EnqueueAISCommand("CopyLibraryCategory", proc);
}

//static
void AISAPI::PurgeDescendents(const LLUUID& category_id, completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		return;
	}
	url += "/category/" + category_id.asString() + "/children";
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t delfn = boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::deleteAndSuspend),
									   // _1 -> adapter
									   // _2 -> url
									   // _3 -> body
									   // _4 -> options
									   // _5 -> headers
									   _1, _2, _4, _5);

	LLCoprocedureManager::coprocedure_t proc(boost::bind(&AISAPI::InvokeAISCommandCoro,
											 _1, delfn, url, category_id,
											 LLSD(), callback,
											 PURGEDESCENDENTS));
	EnqueueAISCommand("PurgeDescendents", proc);
}

//static
void AISAPI::UpdateCategory(const LLUUID& category_id, const LLSD& updates,
							completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		return;
	}
	url += "/category/" + category_id.asString();
	LL_DEBUGS("Inventory") << "url: " << url << " - Request: "
						   << ll_pretty_print_sd(updates) << LL_ENDL;

	invokationFn_t patchfn = boost::bind(COROCAST(&LLCoreHttpUtil::HttpCoroutineAdapter::patchAndSuspend),
										 // _1 -> adapter
										 // _2 -> url
										 // _3 -> body
										 // _4 -> options
										 // _5 -> headers
										 _1, _2, _3, _4, _5);

	LLCoprocedureManager::coprocedure_t proc(boost::bind(&AISAPI::InvokeAISCommandCoro,
											 _1, patchfn, url, category_id,
											 updates, callback,
											 UPDATECATEGORY));
	EnqueueAISCommand("UpdateCategory", proc);
}

//static
void AISAPI::UpdateItem(const LLUUID& item_id, const LLSD& updates,
						completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		return;
	}
	url += "/item/" + item_id.asString();
	LL_DEBUGS("Inventory") << "url: " << url << " - Request: "
						   << ll_pretty_print_sd(updates) << LL_ENDL;

	invokationFn_t patchfn = boost::bind(COROCAST(&LLCoreHttpUtil::HttpCoroutineAdapter::patchAndSuspend),
										 // _1 -> adapter
										 // _2 -> url
										 // _3 -> body
										 // _4 -> options
										 // _5 -> headers
										 _1, _2, _3, _4, _5);

	LLCoprocedureManager::coprocedure_t proc(boost::bind(&AISAPI::InvokeAISCommandCoro,
											 _1, patchfn, url, item_id,
											 updates, callback, UPDATEITEM));
	EnqueueAISCommand("UpdateItem", proc);
}

//static
void AISAPI::EnqueueAISCommand(const std::string& proc_name,
							   LLCoprocedureManager::coprocedure_t proc)
{
	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	U32 pending_in_pool = cpmgr->countPending("AIS");
	if (pending_in_pool < 2 * COPROC_DEFAULT_QUEUE_SIZE / 5)
	{
		LLUUID id = cpmgr->enqueueCoprocedure("AIS", "AIS(" + proc_name + ")",
											  proc);
		if (id.notNull())	// Sucessfully enqueued
		{
			llinfos << "Will retry: " << proc_name << llendl;
			return;
		}
	}

	if (sPostponedQuery.empty())
	{
		gIdleCallbacks.addFunction(onIdle, NULL);
	}
	sPostponedQuery.emplace_back("AIS(" + proc_name + ")", proc);
}

//static
void AISAPI::onIdle(void*)
{
	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	U32 pending_in_pool = cpmgr->countPending("AIS");
	while (pending_in_pool++ < 2 * COPROC_DEFAULT_QUEUE_SIZE / 5 &&
		   !sPostponedQuery.empty())
	{
		ais_query_item_t& item = sPostponedQuery.front();
		LLUUID id = cpmgr->enqueueCoprocedure("AIS", item.first, item.second);
		if (id.isNull())	// Failure to enqueue !
		{
			llinfos << "Will retry: " << item.first << llendl;
			break;
		}
		sPostponedQuery.pop_front();
	}
	if (sPostponedQuery.empty())
	{
		gIdleCallbacks.deleteFunction(onIdle, NULL);
	}
}

//static
void AISAPI::InvokeAISCommandCoro(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t adapter,
								  invokationFn_t invoke, std::string url,
								  LLUUID target_id, LLSD body,
								  completion_t callback, COMMAND_TYPE type)
{
	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	LLCore::HttpHeaders::ptr_t headers;

	options->setTimeout(LLCoreHttpUtil::HTTP_REQUEST_EXPIRY_SECS);

	LL_DEBUGS("Inventory") << "Target: " << target_id << " - Command type: "
						   << (S32)type << " - URL: " << url << LL_ENDL;

	LLSD result = invoke(adapter, url, body, options, headers);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || !result.isMap())
	{
		if (!result.isMap())
		{
			status = gStatusInternalError;
		}
		llwarns << "Inventory error: " << status.toString() << " - Result: "
				<< ll_pretty_print_sd(result) << llendl;

		if (status.getType() == 410) // Gone
		{
			// Item does not exist or was already deleted from server; parent
			// folder is out of sync.
			if (type == REMOVECATEGORY)
			{
				LLViewerInventoryCategory* cat =
					gInventory.getCategory(target_id);
				if (cat)
				{
					llwarns << "Purge failed (folder no longer exists on server) for: "
							<< cat->getName()
							<< " - Local version: " << cat->getVersion()
							<< " - Descendents count: server="
							<< cat->getDescendentCount() << " - viewer="
							<< cat->getViewerDescendentCount() << llendl;
					gInventory.fetchDescendentsOf(cat->getParentUUID());
				}
			}
			else if (type == REMOVEITEM)
			{
				LLViewerInventoryItem* item = gInventory.getItem(target_id);
				if (item)
				{
					llwarns << "Purge failed (item no longer exists on server) for: "
							<< item->getName() << llendl;
					gInventory.onObjectDeletedFromServer(target_id);
				}
			}
		}
	}

	gInventory.onAISUpdateReceived("AISCommand", result);

	if (callback && !callback.empty())
	{
		LLUUID id;
		if (result.has("category_id") && type == COPYLIBRARYCATEGORY)
		{
			id = result["category_id"];
		}
		callback(id);
	}
}

AISUpdate::AISUpdate(const LLSD& update)
{
	parseUpdate(update);
}

void AISUpdate::clearParseResults()
{
	mCatDescendentDeltas.clear();
	mCatDescendentsKnown.clear();
	mCatVersionsUpdated.clear();
	mItemsCreated.clear();
	mItemsUpdated.clear();
	mCategoriesCreated.clear();
	mCategoriesUpdated.clear();
	mObjectsDeletedIds.clear();
	mItemIds.clear();
	mCategoryIds.clear();
}

void AISUpdate::parseUpdate(const LLSD& update)
{
	LL_DEBUGS("Inventory") << "Parsing updates..." << LL_ENDL;
	clearParseResults();
	parseMeta(update);
	parseContent(update);
}

void AISUpdate::parseMeta(const LLSD& update)
{
	LL_DEBUGS("Inventory") << "Meta data:\n" << ll_pretty_print_sd(update)
						   << LL_ENDL;
	// Parse _categories_removed -> mObjectsDeletedIds
	uuid_list_t cat_ids;
	parseUUIDArray(update, "_categories_removed", cat_ids);
	for (uuid_list_t::const_iterator it = cat_ids.begin();
		 it != cat_ids.end(); ++it)
	{
		LLViewerInventoryCategory* cat = gInventory.getCategory(*it);
		if (cat)
		{
			--mCatDescendentDeltas[cat->getParentUUID()];
			mObjectsDeletedIds.emplace(*it);
		}
		else
		{
			llwarns << "Removed category " << *it << " not found." << llendl;
		}
	}

	// Parse _categories_items_removed -> mObjectsDeletedIds
	uuid_list_t item_ids;
	parseUUIDArray(update, "_category_items_removed", item_ids);
	parseUUIDArray(update, "_removed_items", item_ids);
	for (uuid_list_t::const_iterator it = item_ids.begin();
		 it != item_ids.end(); ++it)
	{
		LLViewerInventoryItem* item = gInventory.getItem(*it);
		if (item)
		{
			--mCatDescendentDeltas[item->getParentUUID()];
			mObjectsDeletedIds.emplace(*it);
		}
		else
		{
			llwarns << "Removed item " << *it << " not found." << llendl;
		}
	}

	// Parse _broken_links_removed -> mObjectsDeletedIds
	uuid_list_t broken_link_ids;
	parseUUIDArray(update, "_broken_links_removed", broken_link_ids);
	for (uuid_list_t::const_iterator it = broken_link_ids.begin();
		 it != broken_link_ids.end(); ++it)
	{
		LLViewerInventoryItem* item = gInventory.getItem(*it);
		if (item)
		{
			--mCatDescendentDeltas[item->getParentUUID()];
			mObjectsDeletedIds.emplace(*it);
		}
		else
		{
			llwarns << "Removed broken link " << *it << " not found."
					<< llendl;
		}
	}

	// Parse _created_items
	parseUUIDArray(update, "_created_items", mItemIds);

	// Parse _created_categories
	parseUUIDArray(update, "_created_categories", mCategoryIds);

	// Parse updated category versions.
	const std::string& ucv = "_updated_category_versions";
	if (update.has(ucv))
	{
		for (LLSD::map_const_iterator it = update[ucv].beginMap(),
				end = update[ucv].endMap();
			it != end; ++it)
		{
			const LLUUID id(it->first);
			S32 version = it->second.asInteger();
			mCatVersionsUpdated[id] = version;
		}
	}
}

void AISUpdate::parseContent(const LLSD& update)
{
	LL_DEBUGS("Inventory") << "Update data:\n" << ll_pretty_print_sd(update)
						   << LL_ENDL;
	if (update.has("linked_id"))
	{
		parseLink(update);
	}
	else if (update.has("item_id"))
	{
		parseItem(update);
	}

	if (update.has("category_id"))
	{
		parseCategory(update);
	}
	else if (update.has("_embedded"))
	{
		parseEmbedded(update["_embedded"]);
	}
}

void AISUpdate::parseItem(const LLSD& item_map)
{
	LL_DEBUGS("Inventory") << "Item map:\n" << ll_pretty_print_sd(item_map)
						   << LL_ENDL;
	LLUUID item_id = item_map["item_id"].asUUID();
	LLPointer<LLViewerInventoryItem> new_item(new LLViewerInventoryItem);
	LLViewerInventoryItem* curr_item = gInventory.getItem(item_id);
	if (curr_item)
	{
		// Default to current values where not provided.
		new_item->copyViewerItem(curr_item);
	}
	if (new_item->unpackMessage(item_map))
	{
		if (curr_item)
		{
			mItemsUpdated[item_id] = new_item;
			// This statement is here to cause a new entry with 0
			// delta to be created if it does not already exist;
			// otherwise has no effect.
			mCatDescendentDeltas[new_item->getParentUUID()];
		}
		else
		{
			mItemsCreated[item_id] = new_item;
			++mCatDescendentDeltas[new_item->getParentUUID()];
		}
	}
	else
	{
		llwarns << "Invalid data, cannot parse: " << item_map << llendl;
		gNotifications.add("AISFailure");
	}
}

void AISUpdate::parseLink(const LLSD& link_map)
{
	LL_DEBUGS("Inventory") << "Link map:\n" << ll_pretty_print_sd(link_map)
						   << LL_ENDL;
	LLUUID item_id = link_map["item_id"].asUUID();
	LLPointer<LLViewerInventoryItem> new_link(new LLViewerInventoryItem);
	LLViewerInventoryItem* curr_link = gInventory.getItem(item_id);
	if (curr_link)
	{
		// Default to current values where not provided.
		new_link->copyViewerItem(curr_link);
	}
	if (new_link->unpackMessage(link_map))
	{
		const LLUUID& parent_id = new_link->getParentUUID();
		if (curr_link)
		{
			mItemsUpdated[item_id] = new_link;
			// This statement is here to cause a new entry with 0 delta to be
			// created if it does not already exist; otherwise has no effect.
			mCatDescendentDeltas[parent_id];
		}
		else
		{
			LLPermissions default_perms;
			default_perms.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
			default_perms.initMasks(PERM_NONE, PERM_NONE, PERM_NONE, PERM_NONE,
									PERM_NONE);
			new_link->setPermissions(default_perms);
			LLSaleInfo default_sale_info;
			new_link->setSaleInfo(default_sale_info);
			mItemsCreated[item_id] = new_link;
			++mCatDescendentDeltas[parent_id];
		}
	}
	else
	{
		llwarns << "Invalid data, cannot parse: " << link_map << llendl;
		gNotifications.add("AISFailure");
	}
}

void AISUpdate::parseCategory(const LLSD& category_map)
{
	LLUUID category_id = category_map["category_id"].asUUID();

	// Check descendent count first, as it may be needed
	// to populate newly created categories
	if (category_map.has("_embedded"))
	{
		parseDescendentCount(category_id, category_map["_embedded"]);
	}

	LLPointer<LLViewerInventoryCategory> new_cat;
	LLViewerInventoryCategory* curr_cat = gInventory.getCategory(category_id);
	if (curr_cat)
	{
		// Default to current values where not provided.
		new_cat = new LLViewerInventoryCategory(curr_cat);
	}
	else if (category_map.has("agent_id"))
	{
		new_cat = new LLViewerInventoryCategory(category_map["agent_id"].asUUID());
	}
	else
	{
		new_cat = new LLViewerInventoryCategory(LLUUID::null);
		LL_DEBUGS("Inventory") << "No owner provided, folder "
							   << new_cat->getUUID()
							   << " might be assigned wrong owner" << LL_ENDL;
	}

	bool rv = new_cat->unpackMessage(category_map);
#if 0	// *NOTE: unpackMessage does not unpack version or descendent count.
	if (category_map.has("version"))
	{
		mCatVersionsUpdated[category_id] = category_map["version"].asInteger();
	}
#endif
	if (rv)
	{
		if (curr_cat)
		{
			mCategoriesUpdated[category_id] = new_cat;
			// This statement is here to cause a new entry with 0 delta to be
			// created if it does not already exist; otherwise has no effect.
			mCatDescendentDeltas[new_cat->getParentUUID()];
			// Capture update for the category itself as well.
			mCatDescendentDeltas[category_id];
		}
		else
		{
			// Set version/descendents for newly created categories.
			if (category_map.has("version"))
			{
				S32 version = category_map["version"].asInteger();
				LL_DEBUGS("Inventory") << "Setting version to " << version
									   << " for new category " << category_id
									   << LL_ENDL;
				new_cat->setVersion(version);
			}
			uuid_int_map_t::const_iterator lookup_it;
			lookup_it = mCatDescendentsKnown.find(category_id);
			if (lookup_it != mCatDescendentsKnown.end())
			{
				S32 descendent_count = lookup_it->second;
				LL_DEBUGS("Inventory") << "Setting descendents count to "
									   << descendent_count
									   << " for new category " << category_id
									   << LL_ENDL;
				new_cat->setDescendentCount(descendent_count);
			}
			mCategoriesCreated[category_id] = new_cat;
			++mCatDescendentDeltas[new_cat->getParentUUID()];
		}
	}
	else
	{
		llwarns << "Unpack failed !!!" << llendl;
		llassert(false);
		return;
	}

	// Check for more embedded content.
	if (category_map.has("_embedded"))
	{
		parseEmbedded(category_map["_embedded"]);
	}
}

void AISUpdate::parseDescendentCount(const LLUUID& category_id,
									 const LLSD& embedded)
{
	// We can only determine true descendent count if this contains all
	// descendent types.
	if (embedded.has("categories") && embedded.has("links") &&
		embedded.has("items"))
	{
		mCatDescendentsKnown[category_id] = embedded["categories"].size() +
											embedded["links"].size() +
											embedded["items"].size();
	}
}

void AISUpdate::parseEmbedded(const LLSD& embedded)
{
#if 0
	if (embedded.has("link"))
	{
		parseEmbeddedLinks(embedded["link"]);
	}
#endif
	if (embedded.has("links"))			// _embedded in a category
	{
		parseEmbeddedLinks(embedded["links"]);
	}
	if (embedded.has("items"))			// _embedded in a category
	{
		parseEmbeddedItems(embedded["items"]);
	}
	if (embedded.has("item"))			// _embedded in a link
	{
		parseEmbeddedItem(embedded["item"]);
	}
	if (embedded.has("categories"))		// _embedded in a category
	{
		parseEmbeddedCategories(embedded["categories"]);
	}
	if (embedded.has("category"))		// _embedded in a link
	{
		parseEmbeddedCategory(embedded["category"]);
	}
}

void AISUpdate::parseUUIDArray(const LLSD& content, const std::string& name,
							   uuid_list_t& ids)
{
	if (content.has(name))
	{
		for (LLSD::array_const_iterator it = content[name].beginArray(),
										end = content[name].endArray();
			 it != end; ++it)
		{
			ids.emplace(it->asUUID());
		}
	}
}

void AISUpdate::parseEmbeddedLinks(const LLSD& links)
{
	for (LLSD::map_const_iterator linkit = links.beginMap(),
			linkend = links.endMap();
		linkit != linkend; ++linkit)
	{
		const LLUUID link_id((*linkit).first);
		const LLSD& link_map = (*linkit).second;
		if (mItemIds.end() == mItemIds.find(link_id))
		{
			LL_DEBUGS("Inventory") << "Ignoring link not in items list "
								   << link_id << LL_ENDL;
		}
		else
		{
			parseLink(link_map);
		}
	}
}

void AISUpdate::parseEmbeddedItem(const LLSD& item)
{
	// a single item (_embedded in a link)
	if (item.has("item_id") &&
		mItemIds.find(item["item_id"].asUUID()) != mItemIds.end())
	{
		parseItem(item);
	}
}

void AISUpdate::parseEmbeddedItems(const LLSD& items)
{
	// a map of items (_embedded in a category)
	for (LLSD::map_const_iterator it = items.beginMap(), end = items.endMap();
		 it != end; ++it)
	{
		const LLUUID item_id(it->first);
		const LLSD& item_map = it->second;
		if (mItemIds.end() == mItemIds.find(item_id))
		{
			LL_DEBUGS("Inventory") << "Ignoring item not in items list "
								   << item_id << LL_ENDL;
		}
		else
		{
			parseItem(item_map);
		}
	}
}

void AISUpdate::parseEmbeddedCategory(const LLSD& category)
{
	// a single category (_embedded in a link)
	if (category.has("category_id") &&
		mCategoryIds.find(category["category_id"].asUUID()) != mCategoryIds.end())
	{
		parseCategory(category);
	}
}

void AISUpdate::parseEmbeddedCategories(const LLSD& categories)
{
	// a map of categories (_embedded in a category)
	for (LLSD::map_const_iterator it = categories.beginMap(),
								  end = categories.endMap();
		 it != end; ++it)
	{
		const LLUUID category_id(it->first);
		const LLSD& category_map = it->second;
		if (mCategoryIds.find(category_id) != mCategoryIds.end())
		{
			parseCategory(category_map);
		}
		else
		{
			LL_DEBUGS("Inventory") << "Ignoring category not in categories list: "
								   << category_id << LL_ENDL;
		}
	}
}

void AISUpdate::doUpdate()
{
	// Do version/descendent accounting.
	for (uuid_int_map_t::const_iterator it = mCatDescendentDeltas.begin(),
										end = mCatDescendentDeltas.end();
		 it != end; ++it)
	{
		const LLUUID& cat_id = it->first;
		LLViewerInventoryCategory* cat = gInventory.getCategory(cat_id);
		LL_DEBUGS("Inventory") << "Descendent accounting for category "
							   << (cat ? cat->getName() : "NOT FOUND")
							   << " (" << cat_id << ")" << LL_ENDL;

		// Do not account for update if we just created this category
		if (mCategoriesCreated.count(cat_id))
		{
			LL_DEBUGS("Inventory") << "Skipping version increment for new category "
								   << (cat ? cat->getName() : "NOT FOUND")
								   << " (" << cat_id << ")" << LL_ENDL;
			continue;
		}

		// Do not account for update unless AIS told us it updated that
		// category
		if (!mCatVersionsUpdated.count(cat_id))
		{
			LL_DEBUGS("Inventory") << "Skipping version increment for non-updated category "
								   << (cat ? cat->getName() : "NOT FOUND")
								   << " (" << cat_id << ")" << LL_ENDL;
			continue;
		}

		// If we have a known descendent count, set that now.
		if (cat)
		{
			S32 descendent_delta = it->second;
			LL_DEBUGS("Inventory") << "Updating descendent count for "
								   << cat->getName() << " (" << cat_id
								   << ") with delta " << descendent_delta;
			S32 old_count = cat->getDescendentCount();
			LL_CONT << " from " << old_count << " to "
					<< old_count + descendent_delta << LL_ENDL;
			LLInventoryModel::LLCategoryUpdate up(cat_id, descendent_delta);
			gInventory.accountForUpdate(up);
		}
		else
		{
			LL_DEBUGS("Inventory") << "Skipping version accounting for unknown category "
								   << cat_id << LL_ENDL;
		}
	}

	// CREATE CATEGORIES
	for (deferred_category_map_t::const_iterator
			it = mCategoriesCreated.begin(), end = mCategoriesCreated.end();
		  it != end; ++it)
	{
		LL_DEBUGS("Inventory") << "Creating category " << it->first << LL_ENDL;
		LLPointer<LLViewerInventoryCategory> new_cat = it->second;
		gInventory.updateCategory(new_cat, LLInventoryObserver::CREATE);
	}

	// UPDATE CATEGORIES
	for (deferred_category_map_t::const_iterator
			it = mCategoriesUpdated.begin(), end = mCategoriesUpdated.end();
		 it != end; ++it)
	{
		const LLUUID& cat_id = it->first;
		LLPointer<LLViewerInventoryCategory> new_cat = it->second;
		// Since this is a copy of the category *before* the accounting update,
		// above, we need to transfer back the updated version/descendent
		// count.
		LLViewerInventoryCategory* curr_cat =
			gInventory.getCategory(new_cat->getUUID());
		if (curr_cat)
		{
			LL_DEBUGS("Inventory") << "Updating category: "
								   << new_cat->getName() << " - Id: " << cat_id
								   << LL_ENDL;
			new_cat->setVersion(curr_cat->getVersion());
			new_cat->setDescendentCount(curr_cat->getDescendentCount());
			gInventory.updateCategory(new_cat);
		}
		else
		{
			llwarns << "Failed to update unknown category "
					<< new_cat->getUUID() << llendl;
		}
	}

	// CREATE ITEMS
	for (deferred_item_map_t::const_iterator it = mItemsCreated.begin(),
											 end = mItemsCreated.end();
		 it != end; ++it)
	{
		LL_DEBUGS("Inventory") << "Creating item " << it->first << LL_ENDL;
		LLPointer<LLViewerInventoryItem> new_item = it->second;
		// *FIXME: risky function since it calls updateServer() in some cases.
		// Maybe break out the update/create cases, in which case this is
		// create.
		gInventory.updateItem(new_item, LLInventoryObserver::CREATE);
	}

	// UPDATE ITEMS
	for (deferred_item_map_t::const_iterator it = mItemsUpdated.begin(),
											 end = mItemsUpdated.end();
		 it != end; ++it)
	{
		LL_DEBUGS("Inventory") << "Updating item " << it->first << LL_ENDL;
		LLPointer<LLViewerInventoryItem> new_item = it->second;
		// *FIXME: risky function since it calls updateServer() in some cases.
		// Maybe break out the update/create cases, in which case this is
		// update.
		gInventory.updateItem(new_item);
	}

	// DELETE OBJECTS
	for (uuid_list_t::const_iterator it = mObjectsDeletedIds.begin(),
									 end = mObjectsDeletedIds.end();
		 it != end; ++it)
	{
		const LLUUID& item_id = *it;
		LL_DEBUGS("Inventory") << "Deleting item " << item_id << LL_ENDL;
		gInventory.onObjectDeletedFromServer(item_id, false, false, false);
	}

	// *TODO: how can we use this version info ?  Need to be sure all changes
	// are going through AIS first, or at least through something with a
	// reliable responder.
	// HB: this is mostly irrelevant: the AIS updates can be mixed up with
	// legacy UDP inventory updates, the latter also causing version
	// increments... (28/05/20016: there *should* not be mixed-up AIS/UDP
	// operations any more now: all inventory ops should now have been
	// enabled with AIS).
	// Beside, several requests launched in a raw can see their replies
	// arriving in a different order (because TCP/IP networking does not
	// guarantee that bunches of packets sent in sequence will arrive in the
	// same order) and a race condition insues, falsely producing category
	// versions mismatches... (28/05/20016: this is still a problem)
	// It may however help tracking down bad version accounting in code and
	// was therefore kept as a debug feature.
	// (24/05/2017: also useful for the added 'cat->fetch()', backported from
	// viewer-neko)
	LL_DEBUGS("Inventory") << "Checking updated category versions...";
	for (uuid_int_map_t::iterator it = mCatVersionsUpdated.begin(),
								  end = mCatVersionsUpdated.end();
		 it != end; ++it)
	{
		S32 version = it->second;
		LLViewerInventoryCategory* cat = gInventory.getCategory(it->first);
		if (cat && cat->getVersion() != version)
		{
			LL_CONT << "\nPossible version mismatch for category: "
					<< cat->getName()
					<< " - Viewer-side version: "
					<< cat->getVersion()
					<< " - Server-side version: "
					<< version;
			if (version == LLViewerInventoryCategory::VERSION_UNKNOWN)
			{
				cat->fetch();
			}
#if 0		// Do *not* do this: this can mess up things really badly due to
			// the race conditions described above... HB
			else
			{
				cat->setVersion(version);
			}
#endif
		}
	}
	LL_CONT << "\nChecks done." << LL_ENDL;

	gInventory.notifyObservers();
}

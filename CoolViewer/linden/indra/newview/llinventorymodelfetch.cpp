/**
 * @file llinventorymodelfetch.cpp
 * @brief Implementation of the inventory fetcher.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#include "llinventorymodelfetch.h"

#include "llcallbacklist.h"
#include "llcorebufferarray.h"
#include "llcorehttputil.h"
#include "llhttpconstants.h"
#include "llsdserialize.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llinventorymodel.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewermessage.h"
#include "llviewerregion.h"

// History (may be apocryphal)
//
// Around viewer v2, an HTTP inventory download mechanism was added along with
// inventory LINK items referencing other inventory items. As part of this, at
// login, the entire inventory structure is downloaded 'in the background'
// using the backgroundFetch()/bulkFetch() methods. The UDP path can still be
// used (for OpenSim only) and is found in the 'DEPRECATED OLD CODE' section.
//
// The old UDP path implemented a throttle that adapted itself during running.
// The mechanism survived info HTTP somewhat but was pinned to poll the HTTP
// plumbing at 0.5s intervals. The reasons for this particular value have been
// lost. It is possible to switch between UDP and HTTP while this is happening
// but there may be surprises in what happens in that case.
//
// Conversion to llcorehttp reduced the number of connections used but batches
// more data and queues more requests (but doesn't do pipelining due to libcurl
// restrictions). The poll interval above was re-examined and reduced to get
// inventory into the viewer more quickly.
//
// Possible future work:
//
// * Do not download the entire hierarchy in one go (which might have been how
//   v1 worked). Implications for links (which may not have a valid target) and
//   search which would then be missing data.
//
// * Review the download rate throttling. Slow then fast ?  Detect bandwidth
//   usage and speed up when it drops ?
//
// * An error on a fetch could be due to one item in the batch. If the batch
//   were broken up, perhaps more of the inventory would download (handwave
//   here, not certain this is an issue in practice).
//
// * Conversion to AISv3.

//----------------------------------------------------------------------------
// Helper class BGItemHttpHandler
//----------------------------------------------------------------------------

// HTTP request handler class for single inventory item requests.
//
// We will use a handler-per-request pattern here rather than a shared handler.
// Mainly convenient as this was converted from a Responder class model.
//
// Derives from and is identical to the normal FetchItemHttpHandler except
// that: 1) it uses the background request object which is updated more slowly
// than the foreground and: 2) keeps a count of active requests on the
// LLInventoryModelFetch object to indicate outstanding operations are
// in-flight.

class BGItemHttpHandler : public LLInventoryModel::FetchItemHttpHandler
{
	LOG_CLASS(BGItemHttpHandler);

public:
	BGItemHttpHandler(const LLSD & request_sd)
	:	LLInventoryModel::FetchItemHttpHandler(request_sd)
	{
		LLInventoryModelFetch::getInstance()->incrFetchCount(1);
	}

	~BGItemHttpHandler() override
	{
		LLInventoryModelFetch::getInstance()->incrFetchCount(-1);
	}

	BGItemHttpHandler(const BGItemHttpHandler&) = delete;
	void operator=(const BGItemHttpHandler&) = delete;
};

//----------------------------------------------------------------------------
// Helper class BGFolderHttpHandler
//----------------------------------------------------------------------------

// HTTP request handler class for folders.
//
// Handler for FetchInventoryDescendents2 and FetchLibDescendents2 caps
// requests for folders.

class BGFolderHttpHandler : public LLCore::HttpHandler
{
	LOG_CLASS(BGFolderHttpHandler);

public:
	BGFolderHttpHandler(const LLSD& request_sd,
						const uuid_vec_t& recursive_cats)
	:	LLCore::HttpHandler(),
		mRequestSD(request_sd),
		mRecursiveCatUUIDs(recursive_cats)
	{
		LLInventoryModelFetch::getInstance()->incrFetchCount(1);
	}

	~BGFolderHttpHandler() override
	{
		LLInventoryModelFetch::getInstance()->incrFetchCount(-1);
	}

	BGFolderHttpHandler(const BGFolderHttpHandler&) = delete;
	void operator=(const BGFolderHttpHandler&) = delete;

public:
	void onCompleted(LLCore::HttpHandle handle,
					 LLCore::HttpResponse* response) override;

	bool getIsRecursive(const LLUUID& cat_id) const;

private:
	void processData(LLSD& body, LLCore::HttpResponse* response);
	void processFailure(LLCore::HttpStatus status,
						LLCore::HttpResponse* response);
	void processFailure(const char* const reason,
						LLCore::HttpResponse* response);

private:
	LLSD				mRequestSD;
	// *HACK for storing away which cat fetches are recursive:
	const uuid_vec_t	mRecursiveCatUUIDs;
};

void BGFolderHttpHandler::onCompleted(LLCore::HttpHandle handle,
									  LLCore::HttpResponse* response)
{
	do	// Single-pass do-while used for common exit handling
	{
		LLCore::HttpStatus status = response->getStatus();
		if (!status)
		{
			processFailure(status, response);
			break;
		}

		// Response body should be present.
		LLCore::BufferArray* body = response->getBody();
		if (!body || ! body->size())
		{
			llwarns << "Missing data in inventory folder query." << llendl;
			processFailure("HTTP response missing expected body", response);
			break;
		}

		// Could test 'Content-Type' header but probably unreliable.

		// Convert response to LLSD
		LLSD body_llsd;
		if (!LLCoreHttpUtil::responseToLLSD(response, true, body_llsd))
		{
			// INFOS-level logging will occur on the parsed failure
			processFailure("HTTP response contained malformed LLSD", response);
			break;
		}

		// Expect top-level structure to be a map
		if (!body_llsd.isMap())
		{
			processFailure("LLSD response not a map", response);
			break;
		}

		// Check for 200-with-error failures
		//
		// See comments in llinventorymodel.cpp about this mode of error.
		if (body_llsd.has("error"))
		{
			processFailure("Inventory application error (200-with-error)",
						   response);
			break;
		}

		// Okay, process data if possible
		processData(body_llsd, response);
	}
	while (false);
}

void BGFolderHttpHandler::processData(LLSD& content,
									  LLCore::HttpResponse* response)
{
	LLInventoryModelFetch* fetcher = LLInventoryModelFetch::getInstance();

	const LLUUID& lof =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);

	// API V2 and earlier should probably be testing for "error" map in
	// response as an application-level error.
	// Instead, we assume success and attempt to extract information.
	if (content.has("folders"))
	{
		LLSD folders = content["folders"];
		for (LLSD::array_const_iterator folder_it = folders.beginArray(),
										folder_end = folders.endArray();
			folder_it != folder_end; ++folder_it)
		{
			LLSD folder_sd = *folder_it;

#if 0		// This should never happen.
			if (folder_sd["agent_id"] != gAgentID)
			{
				llwarns << "Got a UpdateInventoryItem for the wrong agent."
						<< llendl;
				break;
			}
#endif

			const LLUUID& parent_id = folder_sd["folder_id"];
			const LLUUID& owner_id = folder_sd["owner_id"];
			S32 version  = (S32)folder_sd["version"].asInteger();
			S32 descendents = (S32)folder_sd["descendents"].asInteger();
			LLPointer<LLViewerInventoryCategory> tcategory =
				new LLViewerInventoryCategory(owner_id);

			if (parent_id.isNull())
			{
				LLSD items = folder_sd["items"];
				LLPointer<LLViewerInventoryItem> titem =
					new LLViewerInventoryItem;

				for (LLSD::array_const_iterator item_it = items.beginArray(),
												item_end = items.endArray();
					 item_it != item_end; ++item_it)
				{
					if (lof.notNull())
					{
						LLSD item = *item_it;
						titem->unpackMessage(item);

						LLInventoryModel::update_list_t update;
						update.emplace_back(lof, 1);
						gInventory.accountForUpdate(update);

						titem->setParent(lof);
						titem->updateParentOnServer(false);
#if 0
						gInventory.updateItem(titem);
#endif
					}
				}
			}

			if (!gInventory.getCategory(parent_id))
			{
				continue;
			}

			for (LLSD::array_const_iterator
					category_it = folder_sd["categories"].beginArray(),
					category_end = folder_sd["categories"].endArray();
				 category_it != category_end; ++category_it)
			{
				LLSD category = *category_it;
				tcategory->fromLLSD(category);

				if (getIsRecursive(tcategory->getUUID()))
				{
					fetcher->addRequestAtBack(tcategory->getUUID(), true,
											  true);
				}
				else if (!gInventory.isCategoryComplete(tcategory->getUUID()))
				{
					gInventory.updateCategory(tcategory);
				}
			}

			LLSD items = folder_sd["items"];
			LLPointer<LLViewerInventoryItem> titem = new LLViewerInventoryItem;
			for (LLSD::array_const_iterator
					item_it = folder_sd["items"].beginArray(),
					item_end = folder_sd["items"].endArray();
				 item_it != item_end; ++item_it)
			{
				LLSD item = *item_it;
				titem->unpackMessage(item);
				gInventory.updateItem(titem);
			}

			// Set version and descendentcount according to message.
			LLViewerInventoryCategory* cat = gInventory.getCategory(parent_id);
			if (cat)
			{
				cat->setVersion(version);
				cat->setDescendentCount(descendents);
			}
		}
	}

	if (content.has("bad_folders"))
	{
		LL_DEBUGS("InventoryFetch") << "Bad folders LLSD:\n";
		std::stringstream str;
		LLSDSerialize::toPrettyXML(content["bad_folders"], str);
		LL_CONT << str.str() << str.str() << LL_ENDL;

		for (LLSD::array_const_iterator
				folder_it = content["bad_folders"].beginArray(),
				folder_end = content["bad_folders"].endArray();
			folder_it != folder_end; ++folder_it)
		{
			const LLSD& folder_sd = *folder_it;
			// These folders failed on the dataserver. We probably do not want
			// to retry them.
			if (folder_sd.has("folder_id"))
			{
				llwarns << "Folder: " << folder_sd["folder_id"].asString()
						<< " - Error: " << folder_sd["error"].asString()
						<< llendl;
			}
		}
	}

	if (fetcher->isBulkFetchProcessingComplete())
	{
		fetcher->setAllFoldersFetched();
	}
#if 0
	gInventory.notifyObservers();
#endif
}

void BGFolderHttpHandler::processFailure(LLCore::HttpStatus status,
										 LLCore::HttpResponse* response)
{
	const std::string& ct(response->getContentType());
	llwarns << "Inventory folder fetch failure - Status: "
			<< status.toTerseString() << " - Reason: " << status.toString()
			<< " - Content-type: " << ct << " - Content (abridged): "
			<< LLCoreHttpUtil::responseToString(response) << llendl;

	// Could use a 404 test here to try to detect revoked caps...

	LLInventoryModelFetch* fetcher = LLInventoryModelFetch::getInstance();

	// This was originally the request retry logic for the inventory request
	// which tested on HTTP_INTERNAL_ERROR status. This retry logic was unbound
	// and lacked discrimination as to the cause of the retry. The new HTTP
	// library should be doing adquately on retries but I want to keep the
	// structure of a retry for reference.
#if 0
	// Timed out or curl failure
	for (LLSD::array_const_iterator
			folder_it = mRequestSD["folders"].beginArray(),
			folder_end = mRequestSD["folders"].endArray();
		folder_it != folder_end; ++folder_it)
	{
		LLSD folder_sd = *folder_it;
		const LLUUID& folder_id = folder_sd["folder_id"].asUUID();
		fetcher->addRequestAtFront(folder_id, getIsRecursive(folder_id), true);
	}
#else
	if (fetcher->isBulkFetchProcessingComplete())
	{
		fetcher->setAllFoldersFetched();
	}
#endif

#if 0 // Avoid: "Call was made to notifyObservers within notifyObservers !"
	  // Since there was an error, no update happened anyway...
	gInventory.notifyObservers();
#endif
}

void BGFolderHttpHandler::processFailure(const char* const reason,
										 LLCore::HttpResponse* response)
{
	llwarns << "Inventory folder fetch failure - Status: internal error - Reason: "
			<< reason << " - Content (abridged): "
			<< LLCoreHttpUtil::responseToString(response) << llendl;

	LLInventoryModelFetch* fetcher = LLInventoryModelFetch::getInstance();

	// Reverse of previous processFailure() method, this is invoked when
	// response structure is found to be invalid. Original always re-issued the
	// request (without limit). This does the same but be aware that this may
	// be a source of problems. Philosophy is that inventory folders are so
	// essential to operation that this is a reasonable action.
#if 1
	for (LLSD::array_const_iterator
			folder_it = mRequestSD["folders"].beginArray(),
			folder_end = mRequestSD["folders"].endArray();
		folder_it != folder_end; ++folder_it)
	{
		LLSD folder_sd = *folder_it;
		const LLUUID& folder_id = folder_sd["folder_id"];
		fetcher->addRequestAtFront(folder_id, getIsRecursive(folder_id), true);
	}
#else
	if (fetcher->isBulkFetchProcessingComplete())
	{
		fetcher->setAllFoldersFetched();
	}
#endif

#if 0 // Avoid: "Call was made to notifyObservers within notifyObservers !"
	  // Since there was an error, no update happened anyway...
	gInventory.notifyObservers();
#endif
}

bool BGFolderHttpHandler::getIsRecursive(const LLUUID& cat_id) const
{
	return std::find(mRecursiveCatUUIDs.begin(), mRecursiveCatUUIDs.end(),
					 cat_id) != mRecursiveCatUUIDs.end();
}

///////////////////////////////////////////////////////////////////////////////
// LLInventoryModelFetch class proper
///////////////////////////////////////////////////////////////////////////////

constexpr S32 MAX_FETCH_RETRIES = 10;

LLInventoryModelFetch::LLInventoryModelFetch()
:	mBackgroundFetchActive(false),
	mAllFoldersFetched(false),
	mFetchCount(0),
	mRecursiveInventoryFetchStarted(false),
	mRecursiveLibraryFetchStarted(false),
	mNumFetchRetries(0),
	mMinTimeBetweenFetches(0.3f),
	mMaxTimeBetweenFetches(10.f),
	mTimelyFetchPending(false)
{
}

bool LLInventoryModelFetch::isBulkFetchProcessingComplete() const
{
	return mFetchQueue.empty() && mFetchCount <= 0;
}

bool LLInventoryModelFetch::libraryFetchStarted() const
{
	return mRecursiveLibraryFetchStarted;
}

bool LLInventoryModelFetch::libraryFetchCompleted() const
{
	return libraryFetchStarted() &&
		   fetchQueueContainsNoDescendentsOf(gInventory.getLibraryRootFolderID());
}

bool LLInventoryModelFetch::libraryFetchInProgress() const
{
	return libraryFetchStarted() && !libraryFetchCompleted();
}

bool LLInventoryModelFetch::inventoryFetchStarted() const
{
	return mRecursiveInventoryFetchStarted;
}

bool LLInventoryModelFetch::inventoryFetchCompleted() const
{
	return inventoryFetchStarted() &&
		   fetchQueueContainsNoDescendentsOf(gInventory.getRootFolderID());
}

bool LLInventoryModelFetch::inventoryFetchInProgress() const
{
	return inventoryFetchStarted() && !inventoryFetchCompleted();
}

bool LLInventoryModelFetch::isEverythingFetched() const
{
	return mAllFoldersFetched;
}

bool LLInventoryModelFetch::backgroundFetchActive() const
{
	return mBackgroundFetchActive;
}

void LLInventoryModelFetch::addRequestAtFront(const LLUUID& id, bool recursive,
											  bool is_category)
{
	mFetchQueue.push_front(FetchQueueInfo(id, recursive, is_category));
}

void LLInventoryModelFetch::addRequestAtBack(const LLUUID& id, bool recursive,
											 bool is_category)
{
	mFetchQueue.emplace_back(id, recursive, is_category);
}

void LLInventoryModelFetch::start(const LLUUID& cat_id, bool recursive)
{
	if (mAllFoldersFetched && cat_id.isNull())
	{
		return;
	}

	LL_DEBUGS("InventoryFetch") << "Start fetching category: " << cat_id
								<< ", recursive: " << recursive << LL_ENDL;

	mBackgroundFetchActive = true;
	if (cat_id.isNull())
	{
		if (!mRecursiveInventoryFetchStarted)
		{
			mRecursiveInventoryFetchStarted |= recursive;
			mFetchQueue.emplace_back(gInventory.getRootFolderID(), recursive);
			gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
		}
		if (!mRecursiveLibraryFetchStarted)
		{
			mRecursiveLibraryFetchStarted |= recursive;
			mFetchQueue.emplace_back(gInventory.getLibraryRootFolderID(),
									 recursive);
			gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
		}
	}
	else
	{
		// Specific folder requests go to front of queue.
		if (mFetchQueue.empty() || mFetchQueue.front().mUUID != cat_id)
		{
			mFetchQueue.push_front(FetchQueueInfo(cat_id, recursive));
			gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
		}
		if (cat_id == gInventory.getLibraryRootFolderID())
		{
			mRecursiveLibraryFetchStarted |= recursive;
		}
		if (cat_id == gInventory.getRootFolderID())
		{
			mRecursiveInventoryFetchStarted |= recursive;
		}
	}
}

void LLInventoryModelFetch::findLostItems()
{
	mBackgroundFetchActive = true;
	mFetchQueue.emplace_back(LLUUID::null, true);
	gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
}

void LLInventoryModelFetch::stopBackgroundFetch()
{
	if (mBackgroundFetchActive)
	{
		mBackgroundFetchActive = false;
		gIdleCallbacks.deleteFunction(backgroundFetchCB, NULL);
		mFetchCount = 0;
		mMinTimeBetweenFetches = 0.f;
	}
}

void LLInventoryModelFetch::setAllFoldersFetched()
{
	if (mRecursiveInventoryFetchStarted && mRecursiveLibraryFetchStarted)
	{
		mAllFoldersFetched = true;
	}
	stopBackgroundFetch();
	llinfos << "Inventory background fetch completed" << llendl;
}

//static
void LLInventoryModelFetch::backgroundFetchCB(void*)
{
	LLInventoryModelFetch::getInstance()->backgroundFetch();
}

void LLInventoryModelFetch::backgroundFetch()
{
	if (!mBackgroundFetchActive)
	{
		return;
	}

	// Wait until we receive the agent region capabilities
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp || !regionp->capabilitiesReceived())
	{
		return;
	}

	// If we will be using the capability, we will be sending batches and the
	// background thing is not as important.
	const std::string& url =
		gAgent.getRegionCapability("FetchInventoryDescendents2");
	if (!url.empty())
	{
		bulkFetch(url);
		return;
	}

	//---------------------------------------------------------------------
	// DEPRECATED OLD CODE (kept for OpenSim)
	//---------------------------------------------------------------------

	// No more categories to fetch, stop fetch process.
	if (mFetchQueue.empty())
	{
		setAllFoldersFetched();
		return;
	}

	F32 fast_fetch = lerp(mMinTimeBetweenFetches, mMaxTimeBetweenFetches,
						  0.1f);
	F32 slow_fetch = lerp(mMinTimeBetweenFetches, mMaxTimeBetweenFetches,
						  0.5f);
	if (mTimelyFetchPending && mFetchTimer.getElapsedTimeF32() > slow_fetch)
	{
		// Double timeouts on failure.
		mMinTimeBetweenFetches = llmin(mMinTimeBetweenFetches * 2.f, 10.f);
		mMaxTimeBetweenFetches = llmin(mMaxTimeBetweenFetches * 2.f, 120.f);
		LL_DEBUGS("InventoryFetch") << "Inventory fetch times grown to ("
									<< mMinTimeBetweenFetches << ", "
									<< mMaxTimeBetweenFetches << ")"
									<< LL_ENDL;
		// Fetch is no longer considered "timely" although we will wait for
		// full time-out.
		mTimelyFetchPending = false;
	}

	while (!mFetchQueue.empty() && !gDisconnected)
	{
		const FetchQueueInfo info = mFetchQueue.front();
		if (info.mIsCategory)
		{
			LLViewerInventoryCategory* cat =
				gInventory.getCategory(info.mUUID);
			if (!cat)
			{
				// Category has been deleted, remove from queue.
				mFetchQueue.pop_front();
				continue;
			}

			if (mFetchTimer.getElapsedTimeF32() > mMinTimeBetweenFetches &&
				cat->getVersion() ==
					LLViewerInventoryCategory::VERSION_UNKNOWN)
			{
				// Category exists but has no children yet, fetch the
				// descendants for now, just request every time and rely on
				// retry timer to throttle.
				if (cat->fetch())
				{
					mFetchTimer.reset();
					mTimelyFetchPending = true;
				}
				else
				{
					// The category also tracks if it has expired and here it
					// says it has not yet. Get out of here because nothing is
					// going to happen until we update the timers.
					break;
				}
			}
			// Do I have all my children ?
			else if (gInventory.isCategoryComplete(info.mUUID))
			{
				// Finished with this category, remove from queue.
				mFetchQueue.pop_front();

				// Add all children to queue.
				LLInventoryModel::cat_array_t* categories;
				LLInventoryModel::item_array_t* items;
				gInventory.getDirectDescendentsOf(cat->getUUID(), categories,
												  items);
				for (LLInventoryModel::cat_array_t::const_iterator
						it = categories->begin(), end = categories->end();
					 it != end; ++it)
				{
					mFetchQueue.emplace_back((*it)->getUUID(),
											 info.mRecursive);
				}

				// We received a response in less than the fast time.
				if (mTimelyFetchPending &&
					mFetchTimer.getElapsedTimeF32() < fast_fetch)
				{
					// Shrink timeouts based on success.
					mMinTimeBetweenFetches = llmax(mMinTimeBetweenFetches * 0.8f,
												   0.3f);
					mMaxTimeBetweenFetches = llmax(mMaxTimeBetweenFetches * 0.8f,
												   10.f);
					LL_DEBUGS("InventoryFetch") << "Inventory fetch interval shrunk to ["
												<< mMinTimeBetweenFetches
												<< ", "
												<< mMaxTimeBetweenFetches
												<< "]" << LL_ENDL;
				}

				mTimelyFetchPending = false;
				continue;
			}
			else if (mFetchTimer.getElapsedTimeF32() > mMaxTimeBetweenFetches)
			{
				// Received first packet, but our num descendants does not
				// match db's num descendants so try again later.
				mFetchQueue.pop_front();

				if (mNumFetchRetries++ < MAX_FETCH_RETRIES)
				{
					// Push on back of queue
					mFetchQueue.emplace_back(info);
				}
				mTimelyFetchPending = false;
				mFetchTimer.reset();
			}

			// Not enough time has elapsed to do a new fetch
			break;
		}
		else
		{
			LLViewerInventoryItem* itemp = gInventory.getItem(info.mUUID);
			mFetchQueue.pop_front();
			if (!itemp)
			{
				continue;
			}

			if (mFetchTimer.getElapsedTimeF32() > mMinTimeBetweenFetches)
			{
				itemp->fetchFromServer();
				mFetchTimer.reset();
				mTimelyFetchPending = true;
			}
			else if (itemp->isFinished())
			{
				mTimelyFetchPending = false;
			}
			else if (mFetchTimer.getElapsedTimeF32() > mMaxTimeBetweenFetches)
			{
				mFetchQueue.emplace_back(info);
				mFetchTimer.reset();
				mTimelyFetchPending = false;
			}
			// Not enough time has elapsed to do a new fetch
			break;
		}
	}
}

void LLInventoryModelFetch::incrFetchCount(S32 fetching)
{
	mFetchCount += fetching;
	if (mFetchCount < 0)
	{
		llwarns_once << "Inventory fetch count fell below zero." << llendl;
		mFetchCount = 0;
	}
}

// Bundle up a bunch of requests to send all at once.
//static
void LLInventoryModelFetch::bulkFetch(const std::string& url)
{
	// Background fetch is called from gIdleCallbacks in a loop until
	// background fetch is stopped. If there are items in mFetchQueue, we want
	// to check the time since the last bulkFetch was sent. If it exceeds our
	// retry time, go ahead and fire off another batch.

	// *TODO: these values could be tweaked at runtime to effect a fast/slow
	// fetch throttle. Once login is complete and the scene is mostly loaded,
	// we could turn up the throttle and fill missing inventory quicker.
	constexpr U32 max_batch_size = 10;
	// Outstanding requests, not connections
	constexpr S32 max_concurrent_fetches = 12;

	// *HACK: clean this up when old code goes away entirely.
	constexpr F32 new_min_time = 0.05f;
	if (mMinTimeBetweenFetches < new_min_time)
	{
		mMinTimeBetweenFetches = new_min_time;
	}

	if (mFetchCount)
	{
		// Process completed background HTTP requests
		gInventory.handleResponses(false);
		// Just processed a bunch of items.
		gInventory.notifyObservers();
	}

	if (gDisconnected || mFetchCount > max_concurrent_fetches ||
		mFetchTimer.getElapsedTimeF32() < mMinTimeBetweenFetches)
	{
		return; // Just bail if we are disconnected
	}

	U32 item_count = 0;
	U32 folder_count = 0;

	static LLCachedControl<U32> inventory_sort_order(gSavedSettings,
													 "InventorySortOrder");
	U32 sort_order = (U32)inventory_sort_order & 0x1;

	uuid_vec_t recursive_cats;

	LLSD folder_request_body;
	LLSD folder_request_body_lib;
	LLSD item_request_body;
	LLSD item_request_body_lib;

	const LLUUID& lib_owner_id = gInventory.getLibraryOwnerID();

	while (!mFetchQueue.empty() && item_count + folder_count < max_batch_size)
	{
		const FetchQueueInfo& fetch_info = mFetchQueue.front();
		if (fetch_info.mIsCategory)
		{
			const LLUUID& cat_id = fetch_info.mUUID;
			if (cat_id.isNull()) // DEV-17797
			{
				LLSD folder_sd;
				folder_sd["folder_id"]		= LLUUID::null.asString();
				folder_sd["owner_id"]		= gAgentID;
				folder_sd["sort_order"]		= (LLSD::Integer)sort_order;
				folder_sd["fetch_folders"]	= false;
				folder_sd["fetch_items"]	= true;
				folder_request_body["folders"].append(folder_sd);
				++folder_count;
			}
			else
			{
				const LLViewerInventoryCategory* cat =
					gInventory.getCategory(cat_id);
				if (cat)
				{
					if (cat->getVersion() ==
							LLViewerInventoryCategory::VERSION_UNKNOWN)
					{
						LLSD folder_sd;
						folder_sd["folder_id"] = cat->getUUID();
						folder_sd["owner_id"] = cat->getOwnerID();
						folder_sd["sort_order"] = (LLSD::Integer)sort_order;
						// (LLSD::Boolean)sFullFetchStarted;
						folder_sd["fetch_folders"] = true;
						folder_sd["fetch_items"] = true;

						if (cat->getOwnerID() == lib_owner_id)
						{
							folder_request_body_lib["folders"].append(folder_sd);
						}
						else
						{
							folder_request_body["folders"].append(folder_sd);
						}
						++folder_count;
					}
					// May already have this folder, but append child folders
					// to list.
					if (fetch_info.mRecursive)
					{
						LLInventoryModel::cat_array_t* categories;
						LLInventoryModel::item_array_t* items;
						gInventory.getDirectDescendentsOf(cat->getUUID(),
														  categories, items);
						for (LLInventoryModel::cat_array_t::const_iterator
								it = categories->begin(), end = categories->end();
							 it != end; ++it)
						{
							mFetchQueue.emplace_back((*it)->getUUID(),
													 fetch_info.mRecursive);
						}
					}
				}
			}

			if (fetch_info.mRecursive)
			{
				recursive_cats.emplace_back(cat_id);
			}
		}
		else
		{
			LLViewerInventoryItem* itemp =
				gInventory.getItem(fetch_info.mUUID);
			if (itemp)
			{
				LLSD item_sd;
				item_sd["owner_id"]	= itemp->getPermissions().getOwner();
				item_sd["item_id"]	= itemp->getUUID();
				if (itemp->getPermissions().getOwner() == gAgentID)
				{
					item_request_body.append(item_sd);
				}
				else
				{
					item_request_body_lib.append(item_sd);
				}
#if 0
				itemp->fetchFromServer();
#endif
				++item_count;
			}
		}

		mFetchQueue.pop_front();
	}

	// Issue HTTP POST requests to fetch folders and items
	if (item_count + folder_count > 0)
	{
		if (folder_request_body["folders"].size())
		{
			if (folder_request_body["folders"].size())
			{
				LLCore::HttpHandler::ptr_t handler(new BGFolderHttpHandler(folder_request_body,
																		   recursive_cats));
				gInventory.requestPost(false, url, folder_request_body,
									   handler, "Inventory Folder");
			}
		}

		if (folder_request_body_lib["folders"].size())
		{
			const std::string& url =
				gAgent.getRegionCapability("FetchLibDescendents2");
			if (!url.empty())
			{
				LLCore::HttpHandler::ptr_t handler(new BGFolderHttpHandler(folder_request_body_lib,
																		   recursive_cats));
				gInventory.requestPost(false, url, folder_request_body_lib,
									   handler, "Library Folder");
			}
		}

		if (item_request_body.size())
		{
			const std::string& url =
				gAgent.getRegionCapability("FetchInventory2");
			if (!url.empty())
			{
				LLSD body;
				body["items"] = item_request_body;
				LLCore::HttpHandler::ptr_t handler(new BGItemHttpHandler(body));
				gInventory.requestPost(false, url, body, handler,
									   "Inventory Item");
			}
		}

		if (item_request_body_lib.size())
		{
			const std::string& url = gAgent.getRegionCapability("FetchLib2");
			if (!url.empty())
			{
				LLSD body;
				body["items"] = item_request_body_lib;
				LLCore::HttpHandler::ptr_t handler(new BGItemHttpHandler(body));
				gInventory.requestPost(false, url, body, handler,
									   "Library Item");
			}
		}

		mFetchTimer.reset();
	}
	else if (isBulkFetchProcessingComplete())
	{
		setAllFoldersFetched();
	}
}

bool LLInventoryModelFetch::fetchQueueContainsNoDescendentsOf(const LLUUID& cat_id) const
{
	for (fetch_queue_t::const_iterator it = mFetchQueue.begin(),
									   end = mFetchQueue.end();
		 it != end; ++it)
	{
		const LLUUID& fetch_id = it->mUUID;
		if (gInventory.isObjectDescendentOf(fetch_id, cat_id))
		{
			return false;
		}
	}
	return true;
}

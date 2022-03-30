/**
 * @file llinventorymodelfetch.h
 * @brief LLInventoryModelFetch class header file
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

#ifndef LL_LLINVENTORYMODELFETCH_H
#define LL_LLINVENTORYMODELFETCH_H

#include <deque>

#include "llcorehttpheaders.h"
#include "llcorehttpoptions.h"
#include "llcorehttprequest.h"
#include "llsingleton.h"
#include "lluuid.h"

// This class handles background fetches, which are fetches of inventory
// folder. Fetches can be recursive or not.

class LLInventoryModelFetch : public LLSingleton<LLInventoryModelFetch>
{
	friend class LLSingleton<LLInventoryModelFetch>;

protected:
	LOG_CLASS(LLInventoryModelFetch);

public:
	LLInventoryModelFetch();

	// Start and stop background breadth-first fetching of inventory contents.
	// This gets triggered when performing a filter-search.
	void start(const LLUUID& cat_id = LLUUID::null, bool recursive = true);

	bool backgroundFetchActive() const;
	// Completing the fetch once per session should be sufficient:
	bool isEverythingFetched() const;

	bool libraryFetchStarted() const;
	bool libraryFetchCompleted() const;
	bool libraryFetchInProgress() const;

	bool inventoryFetchStarted() const;
	bool inventoryFetchCompleted() const;
	bool inventoryFetchInProgress() const;

    void findLostItems();

	void incrFetchCount(S32 fetching);

	bool isBulkFetchProcessingComplete() const;
	void setAllFoldersFetched();

	void addRequestAtFront(const LLUUID& id, bool recursive, bool is_category);
	void addRequestAtBack(const LLUUID& id, bool recursive, bool is_category);

protected:
	void bulkFetch(const std::string& url);

	void backgroundFetch();
	static void backgroundFetchCB(void*);	// Background fetch idle method
	void stopBackgroundFetch();				// Stops the fetch process

	bool fetchQueueContainsNoDescendentsOf(const LLUUID& cat_id) const;

private:
	LLFrameTimer	mFetchTimer;

	S32				mFetchCount;
	S32				mNumFetchRetries;

	F32				mMinTimeBetweenFetches;
	F32				mMaxTimeBetweenFetches;

 	bool			mRecursiveInventoryFetchStarted;
	bool			mRecursiveLibraryFetchStarted;
	bool			mAllFoldersFetched;
	bool			mBackgroundFetchActive;
	bool			mTimelyFetchPending;

	struct FetchQueueInfo
	{
		FetchQueueInfo(const LLUUID& id, bool recursive,
					   bool is_category = true)
		:	mUUID(id),
			mIsCategory(is_category),
			mRecursive(recursive)
		{
		}

		LLUUID	mUUID;
		bool	mIsCategory;
		bool	mRecursive;
	};
	typedef std::deque<FetchQueueInfo> fetch_queue_t;
	fetch_queue_t	mFetchQueue;
};

#endif // LL_LLINVENTORYMODELFETCH_H

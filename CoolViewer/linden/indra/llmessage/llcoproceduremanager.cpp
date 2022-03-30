/**
 * @file llcoproceduremanager.cpp
 * @author Rider Linden
 * @brief Singleton class for managing asset uploads to the sim.
 *
 * $LicenseInfo:firstyear=2015&license=viewergpl$
 *
 * Copyright (c) 2015, Linden Research, Inc.
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

#include <deque>

#include "boost/assign.hpp"
#include "boost/core/noncopyable.hpp"
#include "boost/make_shared.hpp"

#include "llatomic.h"
#include "llthreadsafequeue.h"

#include "llcoproceduremanager.h"

// Map of pool sizes for known pools
static std::map<std::string, U32> sDefaultPoolSizes =
	boost::assign::map_list_of
		(std::string("Upload"), 1)
		(std::string("AssetStorage"), 16)
		// Keep AIS serialized to avoid getting COF out-of-sync
		(std::string("AIS"), 1);

#define DEFAULT_POOL_SIZE 5

///////////////////////////////////////////////////////////////////////////////
// LLCoprocedurePool class
///////////////////////////////////////////////////////////////////////////////

class LLCoprocedurePool : private boost::noncopyable
{
protected:
	LOG_CLASS(LLCoprocedurePool);

public:
	typedef LLCoprocedureManager::coprocedure_t coprocedure_t;

	LLCoprocedurePool(const std::string& name, size_t size);

	// Places the coprocedure on the queue for processing.
	//
	// @param name Is used for debugging and should identify this coroutine.
	// @param proc Is a bound function to be executed
	//
	// @return This method returns a UUID that can be used later to cancel
	//         execution.
	LLUUID enqueueCoprocedure(const std::string& name, coprocedure_t proc);

	// Requests a shutdown of the upload manager.
	void shutdown();

	LL_INLINE U32 countActive() 			{ return mNumActiveCoprocs; }
	LL_INLINE U32 countPending() 			{ return mNumPendingCoprocs; }

	LL_INLINE U32 count()
	{
		return mNumActiveCoprocs + mNumPendingCoprocs;
	}

private:
	void coprocedureInvokerCoro(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t adapter);

private:
	struct QueuedCoproc
	{
		typedef boost::shared_ptr<QueuedCoproc> ptr_t;

		QueuedCoproc(const std::string& name, const LLUUID& id,
					 coprocedure_t proc)
		:	mName(name),
			mId(id),
			mProc(proc)
		{
		}

		std::string					mName;
		LLUUID						mId;
		coprocedure_t				mProc;
	};

	std::string						mPoolName;

	LLEventStream					mWakeupTrigger;

	typedef std::map<std::string,
					 LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t> adapter_map_t;
	adapter_map_t					mCoroMapping;

	typedef LLThreadSafeQueue<QueuedCoproc::ptr_t> coproc_queue_t;
	coproc_queue_t					mPendingCoprocs;

	LLAtomicU32						mNumActiveCoprocs;
	LLAtomicU32						mNumPendingCoprocs;

	LLCore::HttpRequest::policy_t	mHTTPPolicy;

	bool							mShutdown;
};

LLCoprocedurePool::LLCoprocedurePool(const std::string& pool_name, size_t size)
:	mPoolName(pool_name),
	mPendingCoprocs(COPROC_DEFAULT_QUEUE_SIZE),
	mNumActiveCoprocs(0),
	mNumPendingCoprocs(0),
	mShutdown(false),
	mWakeupTrigger("CoprocedurePool" + pool_name, true),
	mHTTPPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID)
{
	std::string adapt_name = mPoolName + "Adapter";
	std::string full_name = "LLCoprocedurePool(" + mPoolName +
							")::coprocedureInvokerCoro";
	LLCoros* coros = LLCoros::getInstance();
	for (size_t count = 0; count < size; ++count)
	{
		LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t adapter =
			boost::make_shared<LLCoreHttpUtil::HttpCoroutineAdapter>(adapt_name,
																	 mHTTPPolicy);
		std::string pooled_coro =
			coros->launch(full_name,
						  boost::bind(&LLCoprocedurePool::coprocedureInvokerCoro,
									  this, adapter));
		mCoroMapping.emplace(pooled_coro, adapter);
	}

	llinfos << "Created coprocedure pool named \"" << mPoolName << "\" with "
			<< size << " items." << llendl;

	mWakeupTrigger.post(LLSD());
}

void LLCoprocedurePool::shutdown()
{
	mShutdown = true;
	mWakeupTrigger.post(LLSD());
}

LLUUID LLCoprocedurePool::enqueueCoprocedure(const std::string& name,
											 coprocedure_t proc)
{
	LLUUID id;
	id.generate();
	if (mPendingCoprocs.tryPushFront(boost::make_shared<QueuedCoproc>(name, id,
																	  proc)))
	{
		++mNumPendingCoprocs;
		LL_DEBUGS("CoreHttp") << "Coprocedure(" << name << ") enqueued with id="
							  << id << " in pool: " << mPoolName << LL_ENDL;

		mWakeupTrigger.post(LLSD());
		return id;
	}

	llwarns << "Failure to enqueue new coprocedure " << name << " in pool: "
			<< mPoolName << llendl;
	return LLUUID::null;
}

void LLCoprocedurePool::coprocedureInvokerCoro(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t adapter)
{
	while (!mShutdown)
	{
		llcoro::suspendUntilEventOn(mWakeupTrigger);
		while (!mShutdown && mPendingCoprocs.size())
		{
			QueuedCoproc::ptr_t coproc = mPendingCoprocs.popBack();
			if (!coproc)
			{
				break;
			}
			++mNumActiveCoprocs;
			--mNumPendingCoprocs;
			LL_DEBUGS("CoreHttp") << "Dequeued and invoking coprocedure("
								  << coproc->mName << ") with id="
								  << coproc->mId << " in pool: " << mPoolName
								  << LL_ENDL;

			try
			{
				coproc->mProc(adapter, coproc->mId);
			}
			catch (std::exception& e)
			{
				llwarns << "Coprocedure(" << coproc->mName << ") id="
						<< coproc->mId << " threw an exception !  Message=\""
						<< e.what() << "\"" << " in pool: " << mPoolName
						<< llendl;
			}
			catch (...)
			{
				llwarns << "A non std::exception was thrown from "
						<< coproc->mName << " with id=" << coproc->mId
						<< " in pool: " << mPoolName << llendl;
			}

			--mNumActiveCoprocs;
			LL_DEBUGS("CoreHttp") << "Finished coprocedure("
								  << coproc->mName << ") in pool: "
								  << mPoolName
								  << " - Coprocedures still active: "
								  << mNumActiveCoprocs
								  << " - Coprocedures still pending: "
								  << mNumPendingCoprocs << LL_ENDL;
		}
	}

	llinfos << "Exiting coroutine for pool: " << mPoolName << llendl;
}

///////////////////////////////////////////////////////////////////////////////
// LLCoprocedureManager class
///////////////////////////////////////////////////////////////////////////////

LLCoprocedureManager::pool_ptr_t LLCoprocedureManager::initializePool(const std::string& pool_name)
{
	// Attempt to look up a pool size in the configuration. If found use it.
	std::string key_name = "PoolSize" + pool_name;
	size_t size = 0;

	if (pool_name.empty())
	{
		llerrs << "Poolname must not be empty" << llendl;
	}

	if (mPropertyQueryFn && !mPropertyQueryFn.empty())
	{
		size = mPropertyQueryFn(key_name);
	}

	if (size == 0)
	{
		// If not found grab the known default... If there is no known default
		// use a reasonable number like 5.
		std::map<std::string, U32>::iterator it =
			sDefaultPoolSizes.find(pool_name);
		size = it == sDefaultPoolSizes.end() ? DEFAULT_POOL_SIZE : it->second;

		if (mPropertyDefineFn && !mPropertyDefineFn.empty())
		{
			mPropertyDefineFn(key_name, size);
		}
		llinfos << "No setting for \"" << key_name
				<< "\" setting pool size to default of " << size << llendl;
	}

	pool_ptr_t pool = boost::make_shared<LLCoprocedurePool>(pool_name, size);
	if (!pool)
	{
		llerrs << "Unable to create pool named \"" << pool_name << "\" FATAL !"
			   << llendl;
	}
	mPoolMap.emplace(pool_name, pool);

	return pool;
}

LLUUID LLCoprocedureManager::enqueueCoprocedure(const std::string& pool,
												const std::string& name,
												coprocedure_t proc)
{
	// Attempt to find the pool and enqueue the procedure. If the pool does not
	// exist, create it.
	pool_ptr_t target_pool;

	pool_map_t::iterator it = mPoolMap.find(pool);
	if (it == mPoolMap.end() || !it->second)
	{
		llwarns << "Pool " << pool
				<< " was not initialized. Initializing it now (could cause a crash)."
				<< llendl;
		target_pool = initializePool(pool);
	}
	else
	{
		target_pool = it->second;
	}

	return target_pool->enqueueCoprocedure(name, proc);
}

void LLCoprocedureManager::cleanup()
{
	for (pool_map_t::const_iterator it = mPoolMap.begin(), end = mPoolMap.end();
		 it != end; ++it)
	{
		if (it->second)
		{
			it->second->shutdown();
		}
	}
#if 0	// Do NOT destroy pools now: this causes crashes on exit. The map will
		// be "naturally" destroyed/cleared on LLCoprocedureManager destruction
		// in the compiler generated code (by destructors chaining virtue).
	mPoolMap.clear();
#endif
}

void LLCoprocedureManager::setPropertyMethods(setting_query_t queryfn,
											  setting_upd_t updatefn)
{
	mPropertyQueryFn = queryfn;
	mPropertyDefineFn = updatefn;

	// Workaround until we get mutex into initializePool
	initializePool("Upload");
}

U32 LLCoprocedureManager::countPending() const
{
	U32 count = 0;
	for (pool_map_t::const_iterator it = mPoolMap.begin(), end = mPoolMap.end();
		 it != end; ++it)
	{
		if (it->second)
		{
			count += it->second->countPending();
		}
	}
	return count;
}

U32 LLCoprocedureManager::countPending(const std::string& pool) const
{
	pool_map_t::const_iterator it = mPoolMap.find(pool);
	return it != mPoolMap.end() && it->second ? it->second->countPending() : 0;
}

U32 LLCoprocedureManager::countActive() const
{
	U32 count = 0;
	for (pool_map_t::const_iterator it = mPoolMap.begin(), end = mPoolMap.end();
		 it != end; ++it)
	{
		if (it->second)
		{
			count += it->second->countActive();
		}
	}
	return count;
}

U32 LLCoprocedureManager::countActive(const std::string& pool) const
{
	pool_map_t::const_iterator it = mPoolMap.find(pool);
	return it != mPoolMap.end() && it->second ? it->second->countActive() : 0;
}

U32 LLCoprocedureManager::count() const
{
	U32 count = 0;
	for (pool_map_t::const_iterator it = mPoolMap.begin(), end = mPoolMap.end();
		 it != end; ++it)
	{
		if (it->second)
		{
			count += it->second->count();
		}
	}
	return count;
}

U32 LLCoprocedureManager::count(const std::string& pool) const
{
	pool_map_t::const_iterator it = mPoolMap.find(pool);
	return it != mPoolMap.end() && it->second ? it->second->count() : 0;
}

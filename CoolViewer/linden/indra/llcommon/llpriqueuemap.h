/**
 * @file llpriqueuemap.h
 * @brief Priority queue implementation
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
#ifndef LL_LLPRIQUEUEMAP_H
#define LL_LLPRIQUEUEMAP_H

#include <map>

#include "llpreprocessor.h"

#include "stdtypes.h"

// Priority queue, implemented under the hood as a map. Needs to be done this
// way because none of the standard STL containers provide a representation
// where it's easy to reprioritize.

template <class DATA>
class LLPQMKey
{
public:
	LLPQMKey(F32 priority, DATA data)
	:	mPriority(priority),
		mData(data)
	{
	}

	LL_INLINE bool operator<(const LLPQMKey& b) const
	{
		if (mPriority > b.mPriority)
		{
			return true;
		}
		if (mPriority < b.mPriority)
		{
			return false;
		}
		if (mData > b.mData)
		{
			return true;
		}
		return false;
	}

public:
	F32 mPriority;
	DATA mData;
};

template <class DATA_TYPE>
class LLPriQueueMap
{
public:
	typedef typename std::map<LLPQMKey<DATA_TYPE>, DATA_TYPE>::iterator pqm_iter;
	typedef void (*set_pri_fn)(DATA_TYPE& data, F32 priority);
	typedef F32 (*get_pri_fn)(DATA_TYPE& data);

	LLPriQueueMap(set_pri_fn set_pri, get_pri_fn get_pri)
	:	mSetPriority(set_pri),
		mGetPriority(get_pri)
	{
	}

	void push(F32 priority, DATA_TYPE data)
	{
#if LL_DEBUG
		pqm_iter iter = mMap.find(LLPQMKey<DATA_TYPE>(priority, data));
		if (iter != mMap.end())
		{
			llerrs << "Pushing already existing data onto queue !" << llendl;
		}
#endif
		mMap.emplace(LLPQMKey<DATA_TYPE>(priority, data), data);
	}

	bool pop(DATA_TYPE* datap)
	{
		pqm_iter iter;
		iter = mMap.begin();
		if (iter == mMap.end())
		{
			return false;
		}
		*datap = iter->second;
		mMap.erase(iter);

		return true;
	}

	void reprioritize(F32 new_priority, DATA_TYPE data)
	{
		pqm_iter iter;
		F32 cur_priority = mGetPriority(data);
		LLPQMKey<DATA_TYPE> cur_key(cur_priority, data);
		iter = mMap.find(cur_key);
		if (iter == mMap.end())
		{
			llwarns << "Data not on priority queue !" << llendl;
			// OK, try iterating through all of the data and seeing if we just
			// screwed up the priority somehow.
			for (iter = mMap.begin(); iter != mMap.end(); ++iter)
			{
				if (iter->second == data)
				{
					llerrs << "Data on priority queue but priority not matched !"
						   << llendl;
				}
			}
			return;
		}

		mMap.erase(iter);
		mSetPriority(data, new_priority);
		push(new_priority, data);
	}

	LL_INLINE S32 getLength() const				{ return (S32)mMap.size(); }

public:
	// *HACK: public for use by the transfer manager, ugh.
	std::map<LLPQMKey<DATA_TYPE>, DATA_TYPE> mMap;

protected:
	void	(*mSetPriority)(DATA_TYPE &data, F32 priority);
	F32		(*mGetPriority)(DATA_TYPE &data);
};

#endif // LL_LLPRIQUEUEMAP_H

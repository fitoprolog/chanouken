/**
 * @file llstaticstringtable.h
 * @brief A static hashed string table
 *
 * $LicenseInfo:firstyear=2013&license=viewergpl$
 *
 * Copyright (c) 2013, Linden Research, Inc.
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

#ifndef LL_STATIC_STRING_TABLE_H
#define LL_STATIC_STRING_TABLE_H

#include <string>

#include "lldefs.h"
#include "llfastmap.h"
#include "llpreprocessor.h"
#include "llstl.h"

class LLStaticHashedString
{
public:
	LLStaticHashedString(const std::string& s)
	:	mString(s)
	{
		mStringHash = makehash(s);
	}

	LL_INLINE const std::string& String() const		{ return mString; }
	LL_INLINE size_t Hash() const					{ return mStringHash; }

	LL_INLINE bool operator==(const LLStaticHashedString& b) const
	{
		return Hash() == b.Hash();
	}

protected:
	size_t makehash(const std::string& s)
	{
		size_t len = s.size();
		const char* c = s.c_str();
		size_t hashval = 0;
		for (size_t i = 0; i < len; ++i)
		{
			hashval = (hashval << 5) + hashval + *c++;
		}
		return hashval;
	}

protected:
	std::string	mString;
	size_t		mStringHash;
};

struct LLStaticStringHasher
{
	enum { bucket_size = 8 };

	LL_INLINE size_t operator()(const LLStaticHashedString& key_value) const noexcept
	{
		return key_value.Hash();
	}

	LL_INLINE bool operator()(const LLStaticHashedString& left,
							  const LLStaticHashedString& right) const noexcept
	{
		return left.Hash() < right.Hash();
	}
};

template<typename MappedObject>
class LLStaticStringTable
:	public safe_hmap<LLStaticHashedString, MappedObject, LLStaticStringHasher>
{
};

#endif

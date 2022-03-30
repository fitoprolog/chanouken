/**
 * @file llstringtable.h
 * @brief The LLStringTable class provides a _fast_ method for finding
 * unique copies of strings.
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

#ifndef LL_STRING_TABLE_H
#define LL_STRING_TABLE_H

#include <list>
#include <set>
#include <string>

#include "lldefs.h"
#include "llerror.h"
#include "llformat.h"

constexpr U32 MAX_STRINGS_LENGTH = 256;

// Used to compare the contents of two pointers (e.g. std::string*)
template <typename T>
struct compare_pointer_contents
{
	typedef const T* Tptr;
	bool operator()(const Tptr& a, const Tptr& b) const
	{
		return *a < *b;
	}
};

class LL_COMMON_API LLStringTableEntry
{
public:
	LLStringTableEntry(const char* str);
	~LLStringTableEntry();

	LL_INLINE void incCount()		{ ++mCount; }
	LL_INLINE bool decCount()		{ return --mCount != 0; }

	char* mString;
	S32  mCount;
};

class LL_COMMON_API LLStringTable
{
protected:
	LOG_CLASS(LLStringTable);

public:
	LLStringTable(int tablesize);
	~LLStringTable();

	char* checkString(const char* str);
	char* checkString(const std::string& str);
	LLStringTableEntry* checkStringEntry(const char* str);
	LLStringTableEntry* checkStringEntry(const std::string& str);

	char* addString(const char* str);
	char* addString(const std::string& str);
	LLStringTableEntry* addStringEntry(const char* str);
	LLStringTableEntry* addStringEntry(const std::string& str);
	void removeString(const char* str);

public:
	S32 mMaxEntries;
	S32 mUniqueEntries;

	typedef std::list<LLStringTableEntry*> string_list_t;
	typedef string_list_t* string_list_ptr_t;
	string_list_ptr_t* mStringList;
};

extern LL_COMMON_API LLStringTable gStringTable;

//============================================================================

// This class is designed to be used locally, e.g. as a member of an LLXmlTree.
// Strings can be inserted only, then quickly looked up

typedef const std::string* LLStdStringHandle;

class LL_COMMON_API LLStdStringTable
{
protected:
	LOG_CLASS(LLStdStringTable);

public:
	LLStdStringTable(S32 tablesize = 0)
	{
		if (tablesize == 0)
		{
			tablesize = 256; // default
		}
		// Make sure tablesize is power of 2
		for (S32 i = 31; i > 0; --i)
		{
			if (tablesize & (1 << i))
			{
				if (tablesize >= (3 << (i - 1)))
				{
					tablesize = 1 << (i + 1);
				}
				else
				{
					tablesize = 1 << i;
				}
				break;
			}
		}
		mTableSize = tablesize;
		mStringList = new string_set_t[tablesize];
	}

	~LLStdStringTable()
	{
		cleanup();
		delete[] mStringList;
	}

	void cleanup()
	{
		// remove strings
		for (S32 i = 0; i < mTableSize; ++i)
		{
			string_set_t& stringset = mStringList[i];
			for (string_set_t::iterator iter = stringset.begin(),
										end = stringset.end();
				 iter != end; ++iter)
			{
				delete *iter;
			}
			stringset.clear();
		}
	}

	LL_INLINE LLStdStringHandle lookup(const std::string& s)
	{
		U32 hashval = makehash(s);
		return lookup(hashval, s);
	}

	LL_INLINE LLStdStringHandle checkString(const std::string& s)
	{
		U32 hashval = makehash(s);
		return lookup(hashval, s);
	}

	LL_INLINE LLStdStringHandle insert(const std::string& s)
	{
		U32 hashval = makehash(s);
		LLStdStringHandle result = lookup(hashval, s);
		if (result == NULL)
		{
			result = new std::string(s);
			mStringList[hashval].insert(result);
		}
		return result;
	}

	LL_INLINE LLStdStringHandle addString(const std::string& s)
	{
		return insert(s);
	}

private:
	LL_INLINE U32 makehash(const std::string& s)
	{
		S32 len = (S32)s.size();
		const char* c = s.c_str();
		U32 hashval = 0;
		for (S32 i = 0; i < len; ++i)
		{
			hashval = (hashval << 5) + hashval + *c++;
		}
		return hashval & (mTableSize - 1);
	}

	LL_INLINE LLStdStringHandle lookup(U32 hashval, const std::string& s)
	{
		string_set_t& stringset = mStringList[hashval];
		LLStdStringHandle handle = &s;
		// compares actual strings:
		string_set_t::iterator iter = stringset.find(handle);
		if (iter != stringset.end())
		{
			return *iter;
		}
		return NULL;
	}

private:
	S32 mTableSize;
	typedef std::set<LLStdStringHandle,
					 compare_pointer_contents<std::string> > string_set_t;
	string_set_t* mStringList; // [mTableSize]
};

#endif

/**
 * @file llstringtable.cpp
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

#include "linden_common.h"

#include "llstringtable.h"
#include "llstl.h"

LLStringTable gStringTable(32768);

LLStringTableEntry::LLStringTableEntry(const char* str)
:	mString(NULL),
	mCount(1)
{
	// Copy string
	U32 length = (U32)strlen(str) + 1;
	length = llmin(length, MAX_STRINGS_LENGTH);
	mString = new char[length];
	strncpy(mString, str, length);
	mString[length - 1] = 0;
}

LLStringTableEntry::~LLStringTableEntry()
{
	if (mString)
	{
		delete[] mString;
		mString = NULL;
	}
	mCount = 0;
}

LLStringTable::LLStringTable(int tablesize)
:	mUniqueEntries(0)
{
	if (!tablesize)
	{
		tablesize = 4096; // Some arbitrary default
	}
	// Make sure tablesize is power of 2
	for (S32 i = 31; i > 0; --i)
	{
		if (tablesize & (1 << i))
		{
			if (tablesize >= 3 << (i - 1))
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
	mMaxEntries = tablesize;

	// ALlocate strings
	mStringList = new string_list_ptr_t[mMaxEntries];
	// Clear strings
	for (S32 i = 0; i < mMaxEntries; ++i)
	{
		mStringList[i] = NULL;
	}
}

LLStringTable::~LLStringTable()
{
	if (mStringList)
	{
		for (S32 i = 0; i < mMaxEntries; ++i)
		{
			if (mStringList[i])
			{
				for (string_list_t::iterator iter = mStringList[i]->begin(),
											 end = mStringList[i]->end();
					 iter != end; ++iter)
				{
					LLStringTableEntry* entry = *iter;
					if (entry)
					{
						delete entry;
					}
				}
				delete mStringList[i];
			}
		}
		delete[] mStringList;
		mStringList = NULL;
	}
}

static U32 hash_my_string(const char* str, int max_entries)
{
	U32 retval = 0;
	while (*str)
	{
		retval = (retval << 4) + *str;
		U32 x = retval & 0xf0000000;
		if (x)
		{
			retval = retval ^ (x >> 24);
		}
		retval = retval & (~x);
		++str;
	}
	// max_entries is guaranteed to be a power of 2
	return retval & (max_entries - 1);
}

char* LLStringTable::checkString(const std::string& str)
{
	return checkString(str.c_str());
}

char* LLStringTable::checkString(const char* str)
{
    LLStringTableEntry* entry = checkStringEntry(str);
	return entry ? entry->mString : NULL;
}

LLStringTableEntry* LLStringTable::checkStringEntry(const std::string& str)
{
    return checkStringEntry(str.c_str());
}

LLStringTableEntry* LLStringTable::checkStringEntry(const char* str)
{
	if (str)
	{
		char* ret_val;
		LLStringTableEntry* entry;
		U32 hash_value = hash_my_string(str, mMaxEntries);
		string_list_t* strlist = mStringList[hash_value];
		if (strlist)
		{
			string_list_t::iterator iter;
			for (iter = strlist->begin(); iter != strlist->end(); ++iter)
			{
				entry = *iter;
				ret_val = entry->mString;
				if (!strncmp(ret_val, str, MAX_STRINGS_LENGTH))
				{
					return entry;
				}
			}
		}
	}
	return NULL;
}

char* LLStringTable::addString(const std::string& str)
{
	// RN: safe to use temporary c_str since string is copied
	return addString(str.c_str());
}

char* LLStringTable::addString(const char* str)
{
    LLStringTableEntry* entry = addStringEntry(str);
	return entry ? entry->mString : NULL;
}

LLStringTableEntry* LLStringTable::addStringEntry(const std::string& str)
{
    return addStringEntry(str.c_str());
}

LLStringTableEntry* LLStringTable::addStringEntry(const char* str)
{
	if (!str)
	{
		return NULL;
	}

	U32 hash_value = hash_my_string(str, mMaxEntries);
	string_list_t* strlist = mStringList[hash_value];
	if (strlist)
	{
		string_list_t::iterator iter;
		for (iter = strlist->begin(); iter != strlist->end(); ++iter)
		{
			LLStringTableEntry* entry = *iter;
			char* entry_str = entry->mString;
			if (!strncmp(entry_str, str, MAX_STRINGS_LENGTH))
			{
				entry->incCount();
				return entry;
			}
		}
	}
	else
	{
		strlist = new string_list_t;
		mStringList[hash_value] = strlist;
	}

	// Not found, so add !
	if (++mUniqueEntries > mMaxEntries)
	{
		llerrs << "String table too small to store a new entry: "
			   << mMaxEntries << " stored." << llendl;
	}

	LLStringTableEntry* newentry = new LLStringTableEntry(str);
	strlist->push_front(newentry);
	LL_DEBUGS("StringTable") << mUniqueEntries << "/" << mMaxEntries
							 << " unique entries." << LL_ENDL;
	return newentry;
}

void LLStringTable::removeString(const char* str)
{
	if (!str)
	{
		return;
	}

	U32 hash_value = hash_my_string(str, mMaxEntries);
	string_list_t* strlist = mStringList[hash_value];
	if (!strlist)
	{
		return;
	}

	string_list_t::iterator iter;
	for (iter = strlist->begin(); iter != strlist->end(); ++iter)
	{
		LLStringTableEntry* entry = *iter;
		char* entry_str = entry->mString;
		if (!strncmp(entry_str, str, MAX_STRINGS_LENGTH))
		{
			if (!entry->decCount())
			{
				if (--mUniqueEntries < 0)
				{
					llerrs << "Trying to remove too many strings !" << llendl;
				}
				strlist->remove(entry);
				delete entry;
			}
			return;
		}
	}
}

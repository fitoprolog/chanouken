/**
 * @file llmemory.cpp
 * @brief Very special memory allocation/deallocation stuff here
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

#include "linden_common.h"

#include "llmemory.h"

#include "llsys.h"

#if LL_JEMALLOC
# include "jemalloc/jemalloc.h"
#endif
#if !LL_DARWIN
# include <malloc.h>
#endif

#if LL_WINDOWS
# include <windows.h>
# include <psapi.h>
#elif LL_DARWIN
# include <sys/types.h>
# include <mach/task.h>
# include <mach/mach_init.h>
#elif LL_LINUX
# include <unistd.h>
#endif

void* gReservedFreeSpace = NULL;

//static
U32 LLMemory::sMaxPhysicalMemInKB = 0;
U32 LLMemory::sMaxVirtualMemInKB = 0;
U32 LLMemory::sAvailPhysicalMemInKB = U32_MAX;
U32 LLMemory::sAvailVirtualMemInKB = U32_MAX;
U32 LLMemory::sAllocatedMemInKB = 0;
U32 LLMemory::sAllocatedPageSizeInKB = 0;
U32 LLMemory::sMaxHeapSizeInKB = U32_MAX;
U32	LLMemory::sSafetyMarginKB = 0;
U32	LLMemory::sThresholdMarginKB = 0;
bool LLMemory::sFailedAllocation = false;

void ll_assert_aligned_error()
{
	llerrs << "Alignment check failed !" << llendl;
}

//static
void LLMemory::initClass()
{
#if LL_JEMALLOC
	unsigned int oval, nval = 0;
	size_t osz = sizeof(oval);
	size_t nsz = sizeof(nval);
	mallctl("thread.arena", &oval, &osz, &nval, nsz);
#endif

	if (!gReservedFreeSpace)
	{
		// Reserve some space that we will free() on crash to try and avoid out
		// of memory conditions while dumping the stack trace log... 256Kb
		// should be plenty enough.
		gReservedFreeSpace = malloc(262144);
	}
}

//static
void LLMemory::cleanupClass()
{
	if (gReservedFreeSpace)
	{
		free(gReservedFreeSpace);
		gReservedFreeSpace = NULL;
	}
}

//static
void LLMemory::allocationFailed(size_t size)
{
	sFailedAllocation = true;
	if (size > 0)
	{
		llwarns << "Memory allocation failure for size: " << size << llendl;
	}
}

//static
void LLMemory::setSafetyMarginKB(U32 margin, F32 threshold_ratio)
{
	if (margin < 49152)
	{
		margin = 49152;
	}
	if (threshold_ratio > 5.0f)
	{
		threshold_ratio = 5.0f;
	}
	else if (threshold_ratio < 2.0f)
	{
		threshold_ratio = 2.0f;
	}
	sThresholdMarginKB = (U32)(threshold_ratio * (F32)margin);
	sSafetyMarginKB = margin;
#if LL_WINDOWS
	// Add 48Mb more to make for increased fragmentation
	sSafetyMarginKB += 49152;
#endif
}

//static
void LLMemory::updateMemoryInfo(bool trim_heap)
{
	if (trim_heap)
	{
		// Trim the heap down from freed memory so that we can compute the
		// actual available virtual space.
		// *TODO: implement heap trimming for macOS, if at all possible (if
		// not, try and use jemalloc with macOS ?)...
		// Note 1: jemalloc v5+ does properly release memory to the system, so
		// there is no need for trimming here.
		// Note 2: also trim the system's malloc() pool, even when jemalloc is
		// used because not *all* structures (especially the C++ ones allocated
		// via the new() operator) are allocated via jemalloc !
#if LL_LINUX
		malloc_trim(100 * 1024 * 1024); 	// Trim all but 100Mb
#elif LL_WINDOWS
		_heapmin();
#endif
	}

	LLMemoryInfo::getMaxMemoryKB(sMaxPhysicalMemInKB, sMaxVirtualMemInKB);
	LLMemoryInfo::getAvailableMemoryKB(sAvailPhysicalMemInKB,
									   sAvailVirtualMemInKB);
#if LL_WINDOWS
	HANDLE self = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS counters;

	if (!GetProcessMemoryInfo(self, &counters, sizeof(counters)))
	{
		llwarns << "GetProcessMemoryInfo failed" << llendl;
		sAllocatedPageSizeInKB = sMaxVirtualMemInKB - sAvailVirtualMemInKB;
		sAllocatedMemInKB = 0;
	}
	else
	{
		sAllocatedPageSizeInKB = (U32)(counters.PagefileUsage / 1024);
		sAllocatedMemInKB = (U32)(counters.WorkingSetSize / 1024);
	}
#else
	sAllocatedPageSizeInKB = sMaxVirtualMemInKB - sAvailVirtualMemInKB;
	sAllocatedMemInKB = (U32)(LLMemory::getCurrentRSS() / 1024);
#endif
}

// This function is to test if there is enough space with the size in the
// virtual address space. It does not do any real allocation: on success, it
// returns the address where the memory chunk can fit in, otherwise it returns
// NULL.
//static
void* LLMemory::tryToAlloc(void* address, U32 size)
{
#if LL_WINDOWS
	address = VirtualAlloc(address, size, MEM_RESERVE | MEM_TOP_DOWN, PAGE_NOACCESS);
	if (address)
	{
		if (!VirtualFree(address, 0, MEM_RELEASE))
		{
			llerrs << "error happens when free some memory reservation." << llendl;
		}
	}
	return address;
#else
	return (void*)0x01; // skip checking
#endif
}

#if LL_JEMALLOC
#define JEMALLOC_STATS_STRING_SIZE 262144
static void jemalloc_write_cb(void* data, const char* s)
{
	if (data && s)
	{
		std::string* buff = (std::string*)data;
		size_t buff_len = buff->length();
		size_t line_len = strlen(s);
		if (buff_len + line_len >= JEMALLOC_STATS_STRING_SIZE)
		{
			line_len = JEMALLOC_STATS_STRING_SIZE - buff_len - 1;
		}
		if (line_len > 0)
		{
			buff->append(s, line_len);
		}
	}
}
#endif

//static
void LLMemory::logMemoryInfo()
{
	updateMemoryInfo();

#if LL_JEMALLOC
	unsigned int arenas, arena;
	size_t sz = sizeof(arenas);
	mallctl("opt.narenas", &arenas, &sz, NULL, 0);
	U32 opt_arenas = arenas;
	mallctl("arenas.narenas", &arenas, &sz, NULL, 0);
	mallctl("thread.arena", &arena, &sz, NULL, 0);
	llinfos << "jemalloc initialized with " << opt_arenas
			<< " arenas, using now " << arenas
			<< " arenas, main thread using arena " << arena << "." << llendl;

	bool stats_enabled = false;
	sz = sizeof(stats_enabled);
	mallctl("config.stats", &stats_enabled, &sz, NULL, 0);
	if (stats_enabled)
	{
		std::string malloc_stats_str;
		// IMPORTANT: we cannot reserve memory during jemalloc_write_cb() call
		// by malloc_stats_print(), so we reserve a fixed string buffer.
		malloc_stats_str.reserve(JEMALLOC_STATS_STRING_SIZE);
		malloc_stats_print(jemalloc_write_cb, &malloc_stats_str, NULL);
		llinfos << "jemalloc stats:\n" << malloc_stats_str << llendl;
	}
#endif

	llinfos << "System memory information: Max physical memory: "
			<< sMaxPhysicalMemInKB << "KB - Allocated physical memory: "
			<< sAllocatedMemInKB << "KB - Available physical memory: "
			<< sAvailPhysicalMemInKB << "KB - Allocated virtual memory: "
			<< sAllocatedPageSizeInKB << "KB" << llendl;
}

//----------------------------------------------------------------------------

#if LL_WINDOWS

U64 LLMemory::getCurrentRSS()
{
	HANDLE self = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS counters;

	if (!GetProcessMemoryInfo(self, &counters, sizeof(counters)))
	{
		llwarns << "GetProcessMemoryInfo failed" << llendl;
		return 0;
	}

	return counters.WorkingSetSize;
}

//static
U32 LLMemory::getWorkingSetSize()
{
    PROCESS_MEMORY_COUNTERS pmc;
	U32 ret = 0;

    if (GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof(pmc)))
	{
		ret = pmc.WorkingSetSize;
	}

	return ret;
}

#elif LL_DARWIN

U64 LLMemory::getCurrentRSS()
{
	U64 resident_size = 0;
	mach_task_basic_info_data_t basic_info;
	mach_msg_type_number_t basic_info_count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
				  (task_info_t)&basic_info, &basic_info_count) == KERN_SUCCESS)
	{
		resident_size = basic_info.resident_size;
	}
	else
	{
		llwarns << "task_info failed" << llendl;
	}

	return resident_size;
}

U32 LLMemory::getWorkingSetSize()
{
	return 0;
}

#elif LL_LINUX

U64 LLMemory::getCurrentRSS()
{
	static const char stat_path[] = "/proc/self/stat";
	LLFILE* fp = LLFile::open(stat_path, "r");
	U64 rss = 0;

	if (fp == NULL)
	{
		llwarns << "Could not open " << stat_path << llendl;
	}
	else
	{
		// Eee-yew !  See Documentation/filesystems/proc.txt in your nearest
		// friendly kernel tree for details.
		{
			int ret = fscanf(fp, "%*d (%*[^)]) %*c %*d %*d %*d %*d %*d %*d %*d "
							 "%*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %Lu",
							 &rss);
			if (ret != 1)
			{
				llwarns << "Could not parse contents of " << stat_path
						<< llendl;
				rss = 0;
			}
		}

		LLFile::close(fp);
	}

	return rss;
}

U32 LLMemory::getWorkingSetSize()
{
	return 0;
}

#else

U64 LLMemory::getCurrentRSS()
{
	return 0;
}

U32 LLMemory::getWorkingSetSize()
{
	return 0;
}

#endif

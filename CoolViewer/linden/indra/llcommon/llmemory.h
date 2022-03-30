/**
 * @file llmemory.h
 * @brief Memory allocation/deallocation header-stuff goes here.
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

#ifndef LL_MEMORY_H
#define LL_MEMORY_H

#include <set>
#include <string.h>		// For memcpy()
#if !LL_WINDOWS
# include <stdint.h>
#endif

#if SSE2NEON
# include "sse2neon.h"
#else
# include <immintrin.h>
#endif

#if LL_JEMALLOC
# include "jemalloc/jemalloc.h"
#elif !LL_WINDOWS
# include <stdlib.h>
#endif

#include "llerror.h"
#include "hbtracy.h"

// Utilities and macros used for SSE2 optimized maths

template <typename T> T* LL_NEXT_ALIGNED_ADDRESS(T* address) 
{ 
	return reinterpret_cast<T*>(
		(reinterpret_cast<uintptr_t>(address) + 0xF) & ~0xF);
}

template <typename T> T* LL_NEXT_ALIGNED_ADDRESS_64(T* address) 
{ 
	return reinterpret_cast<T*>(
		(reinterpret_cast<uintptr_t>(address) + 0x3F) & ~0x3F);
}

LL_COMMON_API void ll_assert_aligned_error();
#if LL_DEBUG
# define ll_assert_aligned(ptr,alignment) if (LL_UNLIKELY(reinterpret_cast<uintptr_t>(ptr) % (U32)alignment != 0)) ll_assert_aligned_error()
#else
# define ll_assert_aligned(ptr,alignment)
#endif

// Copy words 16-bytes blocks from src to dst. Source and destination MUST NOT
// OVERLAP. Source and dest must be 16-byte aligned and size must be multiple
// of 16.
LL_INLINE void ll_memcpy_nonaliased_aligned_16(char* __restrict dst,
											   const char* __restrict src,
											   size_t bytes)
{
	llassert(src != NULL && dst != NULL);
	llassert(bytes > 0 && bytes % sizeof(F32) == 0 && bytes % 16 == 0);
	llassert(src < dst ? src + bytes <= dst : dst + bytes <= src);
	ll_assert_aligned(src, 16);
	ll_assert_aligned(dst, 16);

	char* end = dst + bytes;

	if (bytes > 64)
	{
		// Find start of 64 bytes-aligned area within block
		void* begin_64 = LL_NEXT_ALIGNED_ADDRESS_64(dst);

		// At least 64 bytes before the end of the destination, switch to 16
		// bytes copies
		void* end_64 = end - 64;

		// Prefetch the head of the 64 bytes area now
		_mm_prefetch((char*)begin_64, _MM_HINT_NTA);
		_mm_prefetch((char*)begin_64 + 64, _MM_HINT_NTA);
		_mm_prefetch((char*)begin_64 + 128, _MM_HINT_NTA);
		_mm_prefetch((char*)begin_64 + 192, _MM_HINT_NTA);

		// Copy 16 bytes chunks until we're 64 bytes aligned
		while (dst < begin_64)
		{

			_mm_store_ps((F32*)dst, _mm_load_ps((F32*)src));
			dst += 16;
			src += 16;
		}

		// Copy 64 bytes chunks up to your tail
		//
		// Might be good to shmoo the 512b prefetch offset (characterize
		// performance for various values)
		while (dst < end_64)
		{
			_mm_prefetch((char*)src + 512, _MM_HINT_NTA);
			_mm_prefetch((char*)dst + 512, _MM_HINT_NTA);
			_mm_store_ps((F32*)dst, _mm_load_ps((F32*)src));
			_mm_store_ps((F32*)(dst + 16), _mm_load_ps((F32*)(src + 16)));
			_mm_store_ps((F32*)(dst + 32), _mm_load_ps((F32*)(src + 32)));
			_mm_store_ps((F32*)(dst + 48), _mm_load_ps((F32*)(src + 48)));
			dst += 64;
			src += 64;
		}
	}

	// Copy remainder 16 bytes tail chunks (or ALL 16 bytes chunks for
	// sub-64 bytes copies)
	while (dst < end)
	{
		_mm_store_ps((F32*)dst, _mm_load_ps((F32*)src));
		dst += 16;
		src += 16;
	}
}

// Purely static class
class LL_COMMON_API LLMemory
{
	LLMemory() = delete;
	~LLMemory() = delete;

protected:
	LOG_CLASS(LLMemory);

public:
	// These two methods must only be called from the main thread. Called from
	// indra/newview/llappviewer*.cpp:
	static void initClass();
	static void cleanupClass();

	// Return the resident set size of the current process, in bytes.
	// Return value is zero if not known.
	static U64 getCurrentRSS();
	static U32 getWorkingSetSize();

	static void* tryToAlloc(void* address, U32 size);
	static void initMaxHeapSizeKB(U32 size_kb)	{ sMaxHeapSizeInKB = size_kb; }
	static U32 getMaxHeapSizeKB()				{ return sMaxHeapSizeInKB; }

	static void setSafetyMarginKB(U32 margin, F32 threshold_ratio);
	static U32 getSafetyMarginKB()				{ return sSafetyMarginKB; }
	static U32 getThresholdMarginKB()			{ return sThresholdMarginKB; }

	static void updateMemoryInfo(bool trim_heap = false);
	static void logMemoryInfo();

	static U32 getMaxPhysicalMemKB()			{ return sMaxPhysicalMemInKB; }
	static U32 getMaxVirtualMemKB()				{ return sMaxVirtualMemInKB; }
	static U32 getAvailablePhysicalMemKB()		{ return sAvailPhysicalMemInKB; }
	static U32 getAvailableVirtualMemKB()		{ return sAvailVirtualMemInKB; }
	static U32 getAllocatedMemKB()				{ return sAllocatedMemInKB; }
	static U32 getAllocatedPageSizeKB()			{ return sAllocatedPageSizeInKB; }

	static void allocationFailed(size_t size = 0);
	static void resetFailedAllocation()			{ sFailedAllocation =  false; }
	LL_INLINE static bool hasFailedAllocation()	{ return sFailedAllocation; }

private:
	static bool sFailedAllocation;

	static U32	sMaxPhysicalMemInKB;
	static U32	sMaxVirtualMemInKB;
	static U32	sAvailPhysicalMemInKB;
	static U32	sAvailVirtualMemInKB;
	static U32	sAllocatedMemInKB;
	static U32	sAllocatedPageSizeInKB;

	static U32	sMaxHeapSizeInKB;
	static U32	sSafetyMarginKB;
	static U32	sThresholdMarginKB;
};

// Generic aligned memory management. Note that other, usage-specific functions
// are used to allocate (pooled) memory for images, vertex buffers and volumes.
// See: llimage/llimage.h, llrender/llvertexbuffer.h and llmath/llvolume.h.

// NOTE: since the memory functions below use void* pointers instead of char*
// (because void* is the type used by malloc and jemalloc), strict aliasing is
// not possible on structures allocated with them. Make sure you forbid your
// compiler to optimize with strict aliasing assumption (i.e. for gcc, DO use
// the -fno-strict-aliasing option) !

// IMPORTANT: returned hunk MUST be freed with ll_aligned_free_16().
LL_INLINE void* ll_aligned_malloc_16(size_t size, bool track_failure = true)
{
	if (LL_UNLIKELY(size <= 0)) return NULL;

	void* ptr;
#if LL_JEMALLOC || LL_DARWIN
	// with jemalloc or macOS, all malloc() calls are 16-bytes aligned
	ptr = malloc(size);
#elif LL_WINDOWS
	ptr = _aligned_malloc(size, 16);
#else
	if (LL_UNLIKELY(posix_memalign(&ptr, 16, size) != 0))
	{
		// Out of memory
		if (track_failure)
		{
			LLMemory::allocationFailed(size);
		}
		return NULL;
	}
#endif
	if (LL_UNLIKELY(ptr == NULL && track_failure))
	{
		LLMemory::allocationFailed(size);
	}

	LL_TRACY_ALLOC(ptr, size, trc_mem_align16);

	return ptr;
}

LL_INLINE void ll_aligned_free_16(void* p) noexcept
{
	LL_TRACY_FREE(p, trc_mem_align16);

	if (LL_LIKELY(p))
	{
#if LL_JEMALLOC || LL_DARWIN
		// with jemalloc or macOS, all realloc() calls are 16-bytes aligned
		free(p);
#elif LL_WINDOWS
		_aligned_free(p);
#else
		free(p); // posix_memalign() is compatible with free()
#endif
	}
}

// IMPORTANT: returned hunk MUST be freed with ll_aligned_free_16().
LL_INLINE void* ll_aligned_realloc_16(void* ptr, size_t size, size_t old_size)
{
	if (LL_UNLIKELY(size == old_size && ptr))
	{
		return ptr;
	}

	LL_TRACY_FREE(ptr, trc_mem_align16);

	void* ret;
#if LL_JEMALLOC || LL_DARWIN
	// With jemalloc or macOS, all realloc() calls are 16-bytes aligned
	ret = realloc(ptr, size);
#elif LL_WINDOWS
	ret = _aligned_realloc(ptr, size, 16);
#else
	if (LL_LIKELY(posix_memalign(&ret, 16, size) == 0))
	{
		if (LL_LIKELY(ptr))
		{
			// FIXME: memcpy is SLOW
			memcpy(ret, ptr, old_size < size ? old_size : size);
			free(ptr);
		}
		LL_TRACY_ALLOC(ret, size, trc_mem_align16);
		return ret;
	}
	else
	{
		LLMemory::allocationFailed(size);
		return NULL;
	}
#endif
	if (LL_UNLIKELY(ret == NULL))
	{
		LLMemory::allocationFailed(size);
	}
	LL_TRACY_ALLOC(ret, size, trc_mem_align16);
	return ret;
}

LL_INLINE void* ll_aligned_malloc(size_t size, int align)
{
	if (LL_UNLIKELY(size <= 0)) return NULL;

	void* addr;

#if LL_JEMALLOC
	addr = mallocx(size, MALLOCX_ALIGN((size_t)align) | MALLOCX_TCACHE_NONE);
#elif LL_WINDOWS
	addr = _aligned_malloc(size, align);
#else
	addr = malloc(size + (align - 1) + sizeof(void*));
	if (LL_LIKELY(addr))
	{
		char* aligned = ((char*)addr) + sizeof(void*);
		aligned += align - ((uintptr_t)aligned & (align - 1));
		((void**)aligned)[-1] = addr;
		addr = (void*)aligned;
	}
#endif

	if (LL_UNLIKELY(addr == NULL))
	{
		LLMemory::allocationFailed(size);
	}

	LL_TRACY_ALLOC(addr, size, trc_mem_align);

	return addr;
}

LL_INLINE void ll_aligned_free(void* addr) noexcept
{
	LL_TRACY_FREE(addr, trc_mem_align);

	if (LL_LIKELY(addr))
	{
#if LL_JEMALLOC
		dallocx(addr, 0);
#elif LL_WINDOWS
		_aligned_free(addr);
#else
		free(((void**)addr)[-1]);
#endif
	}
}

#endif	// LL_MEMORY_H

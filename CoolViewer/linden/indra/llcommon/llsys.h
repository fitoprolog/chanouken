/**
 * @file llsys.h
 * @brief System information debugging classes.
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

#ifndef LL_SYS_H
#define LL_SYS_H

#include <string>

#include "llsingleton.h"
#include "stdtypes.h"

class LLProcessorInfo;

class LL_COMMON_API LLOSInfo final : public LLSingleton<LLOSInfo>
{
	friend class LLSingleton<LLOSInfo>;

protected:
	LOG_CLASS(LLOSInfo);

	LLOSInfo();
	~LLOSInfo() override = default;

public:
	LL_INLINE const std::string& getOSString() const
	{
		return mOSString;
	}

	LL_INLINE const std::string& getOSStringSimple() const
	{
		return mOSStringSimple;
	}

	LL_INLINE const std::string& getOSVersionString() const
	{
		return mOSVersionString;
	}

	static U32 getProcessVirtualSizeKB();
	static U32 getProcessResidentSizeKB();

#if LL_LINUX
	LL_INLINE U32 getKernelVersionMajor() const
	{
		return mVersionMajor;
	}

	LL_INLINE U32 getKernelVersionMinor() const
	{
		return mVersionMinor;
	}
#endif

private:
	std::string	mOSString;
	std::string	mOSStringSimple;
	std::string	mOSVersionString;
#if LL_LINUX
	U32			mVersionMajor;
	U32			mVersionMinor;
#endif
};

class LL_COMMON_API LLCPUInfo final : public LLSingleton<LLCPUInfo>
{
	friend class LLSingleton<LLCPUInfo>;

protected:
	LOG_CLASS(LLCPUInfo);

	LLCPUInfo();
	~LLCPUInfo() override;

public:
	LL_INLINE std::string getCPUString() const		{ return mCPUString; }
	LL_INLINE bool hasSSE2() const					{ return mHasSSE2; }
	LL_INLINE F64 getMHz() const					{ return mCPUMHz; }
	// Family is "AMD K8" or "Intel Pentium Pro"
	LL_INLINE const std::string& getFamily() const	{ return mFamily; }

	std::string getInfo() const;

	// The following methods are for now only actually implemented for Linux
	// (they are no-operations under macOS and Windows). HB

	// Emits its own set of llinfos and llwarns, so no need for a returned
	// success boolean.
	static void setMainThreadCPUAffinifty(U32 cpu_mask);
	// Returns 1 when successful, 0 when failed, -1 when waiting for main
	// thread affinity to be set (i.e. when it should be retried).
	static S32 setThreadCPUAffinity();

	// Returns the CPU single-core performance relatively to a 9700K @ 5GHz
	// as a factor (1.f = same perfs, larger factor = better perfs).
	static F32 benchmarkFactor();

private:
	LLProcessorInfo*	mImpl;
	F64					mCPUMHz;
	std::string			mFamily;
	std::string			mCPUString;
	bool				mHasSSE2;

	static U32			sMainThreadAffinityMask;
	static bool			sMainThreadAffinitySet;
};

//=============================================================================
// LLMemoryInfo class (purely static class)
//=============================================================================

class LL_COMMON_API LLMemoryInfo
{
	LLMemoryInfo() = delete;
	~LLMemoryInfo() = delete;

protected:
	LOG_CLASS(LLMemoryInfo);

public:
	static U32 getPhysicalMemoryKB();	// Memory size in KiloBytes

	// Get the available memory infomation in KiloBytes.
	static void getMaxMemoryKB(U32& max_physical_mem_kb,
							   U32& max_virtual_mem_kb);

	static void getAvailableMemoryKB(U32& avail_physical_mem_kb,
									 U32& avail_virtual_mem_kb);

	LL_INLINE static void setAllowSwapping(bool allow)
	{
		sAllowSwapping = allow;
	}

	static std::string getInfo();

private:
	static bool	sAllowSwapping;
};

#endif // LL_LLSYS_H

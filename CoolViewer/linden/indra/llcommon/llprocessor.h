/**
 * @file llprocessor.h
 * @brief Code to figure out the processor. Originally by Benjamin Jurke.
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

#ifndef LLPROCESSOR_H
#define LLPROCESSOR_H

#include "llsd.h"

// Note: this is not flagged LL_COMMON_API, because this class shall only be
// used by LLCPUInfo, internally to llcommon.
class LLProcessorInfo
{
	friend class LLCPUInfo;

protected:	// Access limited to friend class
	LOG_CLASS(LLProcessorInfo);

	LLProcessorInfo();
	virtual ~LLProcessorInfo() = default;

	F64 getCPUFrequency() const;

	bool hasSSE2() const;

	std::string getCPUFamilyName() const;
	std::string getCPUBrandName() const;

	// This is virtual to support a different Linux format.
	virtual std::string getCPUFeatureDescription() const;

protected:
	void setInfo(S32 info_type, const LLSD& value);
    LLSD getInfo(S32 info_type, const LLSD& default_val) const;

	void setConfig(S32 config_type, const LLSD& value);
	LLSD getConfig(S32 config_type, const LLSD& default_val) const;

	void setExtension(const std::string& name);
	bool hasExtension(const std::string& name) const;

	// Returns the time needed (in ms) to find the prime numbers smaller than
	// upper_limit, using the sieve of Eratosthenes algorithm (with compiler
	// optimizations turned off). HB
	static F64 benchmark(U32 upper_limit);

private:
	void getCPUIDInfo();

	void setInfo(const std::string& name, const LLSD& value);
	LLSD getInfo(const std::string& name, const LLSD& defaultVal) const;

	void setConfig(const std::string& name, const LLSD& value);
	LLSD getConfig(const std::string& name, const LLSD& defaultVal) const;

private:
	LLSD mProcessorInfo;
};

#endif // LLPROCESSOR_H

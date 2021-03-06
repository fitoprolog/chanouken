/**
 * @file llcriticaldamp.cpp
 * @brief Implementation of the critical damping functionality.
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

#include "llcriticaldamp.h"

// Static members
LLFrameTimer LLCriticalDamp::sInternalTimer;
std::map<F32, F32> LLCriticalDamp::sInterpolants;
F32 LLCriticalDamp::sTimeDelta = 0.f;

//static
void LLCriticalDamp::updateInterpolants()
{
	sTimeDelta = sInternalTimer.getElapsedTimeAndResetF32();

	for (std::map<F32, F32>::iterator it = sInterpolants.begin(),
									  end = sInterpolants.end();
		 it != end; ++it)
	{
		it->second = llclamp(1.f - powf(2.f, -sTimeDelta / it->first),
							 0.f, 1.f);
	}
}

//static
F32 LLCriticalDamp::getInterpolant(F32 time_constant, bool use_cache)
{
	if (time_constant == 0.f)
	{
		return 1.f;
	}

	if (use_cache)
	{
		std::map<F32, F32>::iterator it = sInterpolants.find(time_constant);
		if (it != sInterpolants.end())
		{
			return it->second;
		}
	}

	F32 interpolant = llclamp(1.f - powf(2.f, -sTimeDelta / time_constant),
							  0.f, 1.f);
	if (use_cache)
	{
		sInterpolants[time_constant] = interpolant;
	}

	return interpolant;
}

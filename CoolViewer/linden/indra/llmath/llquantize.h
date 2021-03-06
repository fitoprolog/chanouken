/**
 * @file llquantize.h
 * @brief useful routines for quantizing floats to various length ints
 * and back out again
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

#ifndef LL_LLQUANTIZE_H
#define LL_LLQUANTIZE_H

#include "llpreprocessor.h"

constexpr U16 U16MAX = 65535;
alignas(16) const F32 F_U16MAX_4A[4] = { 65535.f, 65535.f, 65535.f, 65535.f };

constexpr F32 OOU16MAX = 1.f / (F32)(U16MAX);
alignas(16) const F32 F_OOU16MAX_4A[4] = { OOU16MAX, OOU16MAX, OOU16MAX, OOU16MAX };

constexpr U8 U8MAX = 255;
alignas(16) const F32 F_U8MAX_4A[4] = { 255.f, 255.f, 255.f, 255.f };

constexpr F32 OOU8MAX = 1.f / (F32)(U8MAX);
alignas(16) const F32 F_OOU8MAX_4A[4] = { OOU8MAX, OOU8MAX, OOU8MAX, OOU8MAX };

LL_INLINE U16 F32_to_U16_ROUND(F32 val, F32 lower, F32 upper)
{
	val = llclamp(val, lower, upper);
	// Make sure that the value is positive and normalized to <0, 1>
	val -= lower;
	val /= (upper - lower);

	// Round the value and return the U16
	return (U16)(ll_round(val * U16MAX));
}

LL_INLINE U16 F32_to_U16(F32 val, F32 lower, F32 upper)
{
	val = llclamp(val, lower, upper);
	// Make sure that the value is positive and normalized to <0, 1>
	val -= lower;
	val /= (upper - lower);

	// Return the U16
	return (U16)(llfloor(val * U16MAX));
}

LL_INLINE F32 U16_to_F32(U16 ival, F32 lower, F32 upper)
{
	F32 val = ival * OOU16MAX;
	F32 delta = (upper - lower);
	val *= delta;
	val += lower;

	F32 max_error = delta * OOU16MAX;

	// Make sure that zero's come through as zero
	if (fabsf(val) < max_error)
	{
		val = 0.f;
	}

	return val;
}

LL_INLINE U8 F32_to_U8_ROUND(F32 val, F32 lower, F32 upper)
{
	val = llclamp(val, lower, upper);
	// Make sure that the value is positive and normalized to <0, 1>
	val -= lower;
	val /= (upper - lower);

	// Return the rounded U8
	return (U8)(ll_round(val * U8MAX));
}

LL_INLINE U8 F32_to_U8(F32 val, F32 lower, F32 upper)
{
	val = llclamp(val, lower, upper);
	// Make sure that the value is positive and normalized to <0, 1>
	val -= lower;
	val /= (upper - lower);

	// Return the U8
	return (U8)(llfloor(val * U8MAX));
}

LL_INLINE F32 U8_to_F32(U8 ival, F32 lower, F32 upper)
{
	F32 val = ival * OOU8MAX;
	F32 delta = (upper - lower);
	val *= delta;
	val += lower;

	F32 max_error = delta * OOU8MAX;

	// Make sure that zero's come through as zero
	if (fabsf(val) < max_error)
	{
		val = 0.f;
	}

	return val;
}

#endif

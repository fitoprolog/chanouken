/**
 * @file llmeshoptimizer.cpp
 * @brief Wrapper around the meshoptimizer library
 *
 * $LicenseInfo:firstyear=2021&license=viewergpl$
 *
 * Copyright (c) 2021, Linden Research, Inc.
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

#include "meshoptimizer.h"

#include "llmeshoptimizer.h"

#include "llmath.h"

//static
void LLMeshOptimizer::generateShadowIndexBuffer(U16* dest, const U16* indices,
												U64 idx_count,
												const LLVector4a* vert_pos,
												U64 vert_count,
												U64 vert_pos_stride)
{
	meshopt_generateShadowIndexBuffer<U16>(dest, indices, idx_count,
										   (const F32*)vert_pos, vert_count,
										   sizeof(LLVector4a),
										   vert_pos_stride);
}

//static
U64 LLMeshOptimizer::simplifyU32(U32* dest, const U32* indices, U64 idx_count,
								 const LLVector4a* vert_pos, U64 vert_count,
								 U64 vert_pos_stride, U64 target_idx_count,
								 F32 target_error, bool sloppy,
								 F32* result_error)
{
	if (sloppy)
	{
		return meshopt_simplifySloppy<U32>(dest, indices, idx_count,
										   (const F32*)vert_pos, vert_count,
										   vert_pos_stride, target_idx_count,
										   target_error, result_error);
	}

	return meshopt_simplify<U32>(dest, indices, idx_count, (const F32*)vert_pos,
								 vert_count, vert_pos_stride, target_idx_count,
								 target_error, result_error);
}

//static
U64 LLMeshOptimizer::simplify(U16* dest, const U16* indices, U64 idx_count,
							  const LLVector4a* vert_pos, U64 vert_count,
							  U64 vert_pos_stride, U64 target_idx_count,
							  F32 target_error, bool sloppy, F32* result_error)
{
	if (sloppy)
	{
		return meshopt_simplifySloppy<U16>(dest, indices, idx_count,
										   (const F32*)vert_pos, vert_count,
										   vert_pos_stride, target_idx_count,
										   target_error, result_error);
	}

	return meshopt_simplify<U16>(dest, indices, idx_count, (const F32*)vert_pos,
								 vert_count, vert_pos_stride, target_idx_count,
								 target_error, result_error);
}

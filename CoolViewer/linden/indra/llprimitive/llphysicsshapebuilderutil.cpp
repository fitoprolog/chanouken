/**
 * @file llphysicsshapebuilderutil.cpp
 * @brief Generic system to convert LL(Physics)VolumeParams to physics shapes
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#include "llphysicsshapebuilderutil.h"

//static
void LLPhysicsShapeBuilderUtil::determinePhysicsShape(const LLPhysicsVolumeParams& volume_params,
													  const LLVector3& scale,
													  PhysicsShapeSpecification& spec_out)
{
	const LLProfileParams& profile_params = volume_params.getProfileParams();
	const LLPathParams& path_params = volume_params.getPathParams();

	spec_out.mScale = scale;

	F32 avg_scale = (scale[VX] + scale[VY] + scale[VZ]) / 3.0f;
	if (avg_scale == 0.f)
	{
		avg_scale = F32_MIN;	// Paranoia: avoid divide by zero
	}

	// Count the scale elements that are small
	S32 min_size_counts = 0;
	for (S32 i = 0; i < 3; ++i)
	{
		if (scale[i] < SHAPE_BUILDER_CONVEXIFICATION_SIZE)
		{
			++min_size_counts;
		}
	}

	F32 path_cut_limit = SHAPE_BUILDER_IMPLICIT_THRESHOLD_PATH_CUT / avg_scale;
	bool profile_complete = profile_params.getBegin() <= path_cut_limit &&
							profile_params.getEnd() >= 1.0f - path_cut_limit;

	bool path_complete = path_params.getBegin() <= path_cut_limit &&
						path_params.getEnd() >= 1.0f - path_cut_limit;

	F32 hollow_limit = SHAPE_BUILDER_IMPLICIT_THRESHOLD_HOLLOW / avg_scale;
	F32 shear_limit = SHAPE_BUILDER_IMPLICIT_THRESHOLD_SHEAR / avg_scale;
	bool simple_params = volume_params.getHollow() <= hollow_limit &&
						 fabs(path_params.getShearX()) <= shear_limit &&
						 fabs(path_params.getShearY()) <= shear_limit &&
						 !volume_params.isMeshSculpt() && !volume_params.isSculpt();

	if (simple_params && profile_complete)
	{
		// Try to create an implicit shape or convexified
		F32 taper_limit = SHAPE_BUILDER_IMPLICIT_THRESHOLD_TAPER / avg_scale;
		bool no_taper = fabs(path_params.getScaleX() - 1.0f) <= taper_limit &&
						fabs(path_params.getScaleY() - 1.0f) <= taper_limit;

		F32 twist_limit = SHAPE_BUILDER_IMPLICIT_THRESHOLD_TWIST / avg_scale;
		bool no_twist = fabs(path_params.getTwistBegin()) <= twist_limit &&
						fabs(path_params.getTwistEnd()) <= twist_limit;

		// Box
		if (no_taper && no_twist &&
			profile_params.getCurveType() == LL_PCODE_PROFILE_SQUARE &&
			path_params.getCurveType() == LL_PCODE_PATH_LINE)
		{
			spec_out.mType = PhysicsShapeSpecification::BOX;
			if (!path_complete)
			{
				// Side lengths
				spec_out.mScale[VX] = llmax(scale[VX],
										    SHAPE_BUILDER_MIN_GEOMETRY_SIZE);
				spec_out.mScale[VY] = llmax(scale[VY],
										    SHAPE_BUILDER_MIN_GEOMETRY_SIZE);
				spec_out.mScale[VZ] = llmax(scale[VZ] *
										    (path_params.getEnd() -
											 path_params.getBegin()),
										    SHAPE_BUILDER_MIN_GEOMETRY_SIZE);

				spec_out.mCenter.set(0.f, 0.f,
									 0.5f * scale[VZ] *
									 (path_params.getEnd() +
									  path_params.getBegin() - 1.0f));
			}
			return;
		}

		// Sphere
		if (path_complete && no_twist &&
			profile_params.getCurveType() == LL_PCODE_PROFILE_CIRCLE_HALF &&
			path_params.getCurveType() == LL_PCODE_PATH_CIRCLE &&
			fabs(volume_params.getTaper()) <= taper_limit)
		{
			if (scale[VX] == scale[VZ] && scale[VY] == scale[VZ])
			{
				// Perfect sphere
				spec_out.mType = PhysicsShapeSpecification::SPHERE;
				spec_out.mScale = scale;
				return;
			}
			else if (min_size_counts > 1)
			{
				// Small or narrow sphere: we can boxify
				for (S32 i = 0; i < 3; ++i)
				{
					if (spec_out.mScale[i] <
							SHAPE_BUILDER_CONVEXIFICATION_SIZE)
					{
						// Reduce each small dimension size to split the
						// approximation errors
						spec_out.mScale[i] *= 0.75f;
					}
				}
				spec_out.mType = PhysicsShapeSpecification::BOX;
				return;
			}
		}

		// Cylinder
		if (no_taper && scale[VX] == scale[VY] &&
			profile_params.getCurveType() == LL_PCODE_PROFILE_CIRCLE &&
			path_params.getCurveType() == LL_PCODE_PATH_LINE &&
			volume_params.getBeginS() <= path_cut_limit &&
			volume_params.getEndS() >= 1.0f - path_cut_limit)
		{
			if (min_size_counts > 1)
			{
				// Small or narrow sphere; we can boxify
				for (S32 i = 0; i < 3; ++i)
				{
					if (spec_out.mScale[i] <
							SHAPE_BUILDER_CONVEXIFICATION_SIZE)
					{
						// Reduce each small dimension size to split the
						// approximation errors
						spec_out.mScale[i] *= 0.75f;
					}
				}

				spec_out.mType = PhysicsShapeSpecification::BOX;
			}
			else
			{
				spec_out.mType = PhysicsShapeSpecification::CYLINDER;
				F32 length = (volume_params.getPathParams().getEnd() -
							  volume_params.getPathParams().getBegin()) *
							 scale[VZ];

				spec_out.mScale[VY] = spec_out.mScale[VX];
				spec_out.mScale[VZ] = length;
				// The minus one below fixes the fact that begin and end range
				// from 0 to 1 not -1 to 1.
				spec_out.mCenter.set(0.f, 0.f,
									 0.5f * scale[VZ] *
									 (volume_params.getPathParams().getBegin() +
									  volume_params.getPathParams().getEnd() -
									  1.f));
			}

			return;
		}
	}

	if (min_size_counts == 3 ||
		// Possible dead code here--who wants to take it out?
		(path_complete && profile_complete && min_size_counts > 1 &&
		 path_params.getCurveType() == LL_PCODE_PATH_LINE))
	{
		// It is not simple but we might be able to convexify this shape if the
		// path and profile are complete or the path is linear and both path
		// and profile are complete --> we can boxify it
		spec_out.mType = PhysicsShapeSpecification::BOX;
		spec_out.mScale = scale;
		return;
	}

	// Special case for big, very thin objects: bump the small dimensions up to
	// the COLLISION_TOLERANCE
	if (min_size_counts == 1 &&		// One dimension is small
		avg_scale > 3.f)			// ... but others are fairly large
	{
		for (S32 i = 0; i < 3; ++i)
		{
			spec_out.mScale[i] = llmax(spec_out.mScale[i],
									   COLLISION_TOLERANCE);
		}
	}

	if (volume_params.shouldForceConvex())
	{
		spec_out.mType = PhysicsShapeSpecification::USER_CONVEX;
	}
	// Make a simpler convex shape if we can.
	else if (volume_params.isConvex() ||	// is convex
			 min_size_counts > 1)			// two or more small dimensions
	{
		spec_out.mType = PhysicsShapeSpecification::PRIM_CONVEX;
	}
	else if (volume_params.isSculpt())
	{
		if (volume_params.isMeshSculpt())
		{
			// Check if one dimension is smaller than min
			if (scale[0] < SHAPE_BUILDER_CONVEXIFICATION_SIZE_MESH ||
				scale[1] < SHAPE_BUILDER_CONVEXIFICATION_SIZE_MESH ||
				scale[2] < SHAPE_BUILDER_CONVEXIFICATION_SIZE_MESH)
			{
				spec_out.mType = PhysicsShapeSpecification::PRIM_CONVEX;
			}
			else
			{
				spec_out.mType = PhysicsShapeSpecification::USER_MESH;
			}
		}
		else
		{
			spec_out.mType = PhysicsShapeSpecification::SCULPT;
		}
	}
	else
	{
		// Resort to mesh
		spec_out.mType = PhysicsShapeSpecification::PRIM_MESH;
	}
}

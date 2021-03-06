/**
 * @file llpatchvertexarray.cpp
 * @brief Implementation of the LLPatchVertexArray class.
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

#include "llviewerprecompiledheaders.h"

#include "llpatchvertexarray.h"

#include "llsurfacepatch.h"

LLPatchVertexArray::LLPatchVertexArray()
:	mSurfaceWidth(0),
	mPatchWidth(0),
	mPatchOrder(0),
	mRenderLevelp(NULL),
	mRenderStridep(NULL)
{
}

LLPatchVertexArray::LLPatchVertexArray(U32 surface_width, U32 patch_width,
									   F32 meters_per_grid)
:	mRenderLevelp(NULL),
	mRenderStridep(NULL)
{
	create(surface_width, patch_width, meters_per_grid);
}

LLPatchVertexArray::~LLPatchVertexArray()
{
	destroy();
}

void LLPatchVertexArray::create(U32 surface_width, U32 patch_width,
								F32 meters_per_grid)
{
	// PART 1 -- Make sure the arguments are good...
	// Make sure patch_width is not greater than surface_width
	if (patch_width > surface_width)
	{
		U32 temp = patch_width;
		patch_width = surface_width;
		surface_width = temp;
	}

	// Make sure (surface_width - 1) is equal to a power_of_two (the -1 is
	// there because an LLSurface has a buffer of 1 on its East and North
	// edges).
	U32 power_of_two = 1;
	while (power_of_two < surface_width - 1)
	{
		power_of_two *= 2;
	}

	// Variable region size support
	if (power_of_two != surface_width - 1)
	{
		surface_width = power_of_two + 1;
	}

	mSurfaceWidth = surface_width;

	// Make sure that patch_width is a factor of (surface_width - 1)
	U32 ratio = (surface_width - 1) / patch_width;
	F32 fratio = (F32)(surface_width - 1) / (F32)patch_width;
	if (fratio != (F32)ratio)
	{
		llerrs << "patch_width is not a factor of surface_width-1. Can't proceed further !"
			   << llendl;
	}

	// Make sure patch_width is a power of two
	power_of_two = 1;
	U32 patch_order = 0;
	while (power_of_two < patch_width)
	{
		power_of_two *= 2;
		patch_order += 1;
	}

	// Variable region size support
	if (power_of_two != patch_width)
	{
		patch_width = power_of_two;
	}

	mPatchWidth = patch_width;
	mPatchOrder = patch_order;

	// PART 2 -- Allocate memory for the render level table
	if (mPatchWidth)
	{
		mRenderLevelp = new (std::nothrow) U32[2 * mPatchWidth + 1];
		mRenderStridep = new (std::nothrow) U32[mPatchOrder + 1];
		if (!mRenderLevelp || !mRenderStridep)
		{
			// init() and some other things all want to deref these
			// pointers, so this is serious.
			llerrs << "Memory allocation failure, can't proceed further !"
				   << llendl;
		}
	}
	else
	{
		llerrs << "Resulting patch width is zero !  Input parameters: surface_width = "
			   << surface_width << " - patch_width = " << patch_width
			   << " - meters_per_grid = " << meters_per_grid
			   << ". Can't proceed further !" << llendl;
	}

	// Now that we have allocated memory, we can initialize the arrays...
	init();
}

void LLPatchVertexArray::destroy()
{
	if (mPatchWidth)
	{
		delete[] mRenderLevelp;
		delete[] mRenderStridep;
		mSurfaceWidth = mPatchWidth = mPatchOrder = 0;
	}
}

// Initializes the triangle strip arrays.
void LLPatchVertexArray::init()
{
	// We need to build two look-up tables...

	// render_level -> render_stride
	// A 16x16 patch has 5 render levels : 2^0 to 2^4
	// render_level		render_stride
	//		4				1
	//		3				2
	//		2				4
	//		1				8
	//		0				16
	U32 level;
	U32 stride = mPatchWidth;
	for (level = 0; level < mPatchOrder + 1; ++level)
	{
		mRenderStridep[level] = stride;
		stride /= 2;
	}

	// render_level <- render_stride.
#if 0
	// For a 16x16 patch we'll clamp the render_strides to 0 through 16
	// and enter the nearest render_level in the table. Of course, only
	// power-of-two render strides are actually used.
	//
	// render_stride	render_level
	//		0				4
	//		1				4	*
	//		2				3	*
	//		3				3
	//		4				2	*
	//		5				2
	//		6				2
	//		7				1
	//		8				1	*
	//		9				1
	//		10				1
	//		11				1
	//		12				1
	//		13				0
	//		14				0
	//		15				0
	//		16				Always 0

	level = mPatchOrder;
	for (stride = 0; stride < mPatchWidth; ++stride)
	{
		if ((F32)stride > 2.1f * mRenderStridep[level])
		{
			--level;
		}
		mRenderLevelp[stride] = level;
	}
#endif

	// This method is more agressive about putting triangles onscreen
	level = mPatchOrder;
	U32 k = 2;
	mRenderLevelp[0] = mPatchOrder;
	mRenderLevelp[1] = mPatchOrder;
	stride = 2;
	while (stride < 2 * mPatchWidth)
	{
		for (U32 j = 0; j < k && stride < 2 * mPatchWidth; ++j)
		{
			mRenderLevelp[stride++] = level;
		}
		k *= 2;
		--level;
	}
	mRenderLevelp[2 * mPatchWidth] = 0;
}

std::ostream& operator<<(std::ostream& s, const LLPatchVertexArray& va)
{
	s << "{ \n";
	s << "  mSurfaceWidth = " << va.mSurfaceWidth << "\n";
	s << "  mPatchWidth = " << va.mPatchWidth << "\n";
	s << "  mPatchOrder = " << va.mPatchOrder << "\n";
	s << "  mRenderStridep = \n";
	for (U32 i = 0; i < va.mPatchOrder + 1; ++i)
	{
		s << "    " << i << "    " << va.mRenderStridep[i] << "\n";
	}
	s << "  mRenderLevelp = \n";
	for (U32 i = 0; i < 2 * va.mPatchWidth + 1; ++i)
	{
		s << "    " << i << "    " << va.mRenderLevelp[i] << "\n";
	}
	s << "}";
	return s;
}

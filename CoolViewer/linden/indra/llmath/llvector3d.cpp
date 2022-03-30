/**
 * @file llvector3d.cpp
 * @brief LLVector3d class implementation.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#include "llvector3d.h"

#include "llvector4.h"
#include "llmatrix4.h"
#include "llmatrix3.h"
#include "llquaternion.h"
#include "llquantize.h"

// LLVector3d
// WARNING: do not use these for global const definitions !
// For example:
//		const LLQuaternion(0.5f * F_PI, LLVector3d::zero);
// at the top of a *.cpp file might not give you what you think.
const LLVector3d LLVector3d::zero(0.0, 0.0, 0.0);
const LLVector3d LLVector3d::x_axis(1.0, 0.0, 0.0);
const LLVector3d LLVector3d::y_axis(0.0, 1.0, 0.0);
const LLVector3d LLVector3d::z_axis(0.0, 0.0, 1.0);
const LLVector3d LLVector3d::x_axis_neg(-1, 0.0, 0.0);
const LLVector3d LLVector3d::y_axis_neg(0.0, -1.0, 0.0);
const LLVector3d LLVector3d::z_axis_neg(0.0, 0.0, -1.0);

// Clamps each values to range (min,max). Returns true if data changed.
bool LLVector3d::clamp(F64 min, F64 max)
{
	bool ret = false;

	if (mdV[0] < min) { mdV[0] = min; ret = true; }
	if (mdV[1] < min) { mdV[1] = min; ret = true; }
	if (mdV[2] < min) { mdV[2] = min; ret = true; }

	if (mdV[0] > max) { mdV[0] = max; ret = true; }
	if (mdV[1] > max) { mdV[1] = max; ret = true; }
	if (mdV[2] > max) { mdV[2] = max; ret = true; }

	return ret;
}

// Sets all values to absolute value of their original values
// Returns true if data changed
bool LLVector3d::abs()
{
	bool ret = false;

	if (mdV[0] < 0.0) { mdV[0] = -mdV[0]; ret = true; }
	if (mdV[1] < 0.0) { mdV[1] = -mdV[1]; ret = true; }
	if (mdV[2] < 0.0) { mdV[2] = -mdV[2]; ret = true; }

	return ret;
}

std::ostream& operator<<(std::ostream& s, const LLVector3d& a)
{
	s << "{ " << a.mdV[VX] << ", " << a.mdV[VY] << ", " << a.mdV[VZ] << " }";
	return s;
}

const LLVector3d& LLVector3d::operator=(const LLVector4& a)
{
	mdV[0] = a.mV[0];
	mdV[1] = a.mV[1];
	mdV[2] = a.mV[2];
	return *this;
}

const LLVector3d& LLVector3d::rotVec(const LLMatrix3& mat)
{
	*this = *this * mat;
	return *this;
}

const LLVector3d& LLVector3d::rotVec(const LLQuaternion& q)
{
	*this = *this * q;
	return *this;
}

const LLVector3d& LLVector3d::rotVec(F64 angle, const LLVector3d& vec)
{
	if (!vec.isExactlyZero() && angle)
	{
		*this = *this * LLMatrix3((F32)angle, vec);
	}
	return *this;
}

const LLVector3d& LLVector3d::rotVec(F64 angle, F64 x, F64 y, F64 z)
{
	LLVector3d vec(x, y, z);
	if (!vec.isExactlyZero() && angle)
	{
		*this = *this * LLMatrix3((F32)angle, vec);
	}
	return *this;
}

bool LLVector3d::parseVector3d(const std::string& buf, LLVector3d* value)
{
	if (!value || buf.empty())
	{
		return false;
	}

	LLVector3d v;
	S32 count = sscanf(buf.c_str(), "%lf %lf %lf",
					   v.mdV, v.mdV + 1, v.mdV + 2);
	if (count != 3)
	{
		return false;
	}

	value->set(v);
	return true;
}
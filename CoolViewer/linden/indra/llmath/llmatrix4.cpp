/**
 * @file llmatrix4.cpp
 * @brief LLMatrix4 class implementation.
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

#include "llmatrix4.h"

#include "llmatrix3.h"
#include "llquaternion.h"

// LLMatrix4

// Constructors

LLMatrix4::LLMatrix4(const LLMatrix3& mat, const LLVector4& vec)
{
	mMatrix[0][0] = mat.mMatrix[0][0];
	mMatrix[0][1] = mat.mMatrix[0][1];
	mMatrix[0][2] = mat.mMatrix[0][2];
	mMatrix[0][3] = 0.f;

	mMatrix[1][0] = mat.mMatrix[1][0];
	mMatrix[1][1] = mat.mMatrix[1][1];
	mMatrix[1][2] = mat.mMatrix[1][2];
	mMatrix[1][3] = 0.f;

	mMatrix[2][0] = mat.mMatrix[2][0];
	mMatrix[2][1] = mat.mMatrix[2][1];
	mMatrix[2][2] = mat.mMatrix[2][2];
	mMatrix[2][3] = 0.f;

	mMatrix[3][0] = vec.mV[0];
	mMatrix[3][1] = vec.mV[1];
	mMatrix[3][2] = vec.mV[2];
	mMatrix[3][3] = 1.f;
}

LLMatrix4::LLMatrix4(const LLMatrix3& mat)
{
	mMatrix[0][0] = mat.mMatrix[0][0];
	mMatrix[0][1] = mat.mMatrix[0][1];
	mMatrix[0][2] = mat.mMatrix[0][2];
	mMatrix[0][3] = 0.f;

	mMatrix[1][0] = mat.mMatrix[1][0];
	mMatrix[1][1] = mat.mMatrix[1][1];
	mMatrix[1][2] = mat.mMatrix[1][2];
	mMatrix[1][3] = 0.f;

	mMatrix[2][0] = mat.mMatrix[2][0];
	mMatrix[2][1] = mat.mMatrix[2][1];
	mMatrix[2][2] = mat.mMatrix[2][2];
	mMatrix[2][3] = 0.f;

	mMatrix[3][0] = 0.f;
	mMatrix[3][1] = 0.f;
	mMatrix[3][2] = 0.f;
	mMatrix[3][3] = 1.f;
}

LLMatrix4::LLMatrix4(const LLQuaternion& q)
{
	*this = initRotation(q);
}

LLMatrix4::LLMatrix4(const LLQuaternion& q, const LLVector4& pos)
{
	*this = initRotTrans(q, pos);
}

LLMatrix4::LLMatrix4(F32 angle, const LLVector4& vec, const LLVector4& pos)
{
	initRotTrans(LLQuaternion(angle, vec), pos);
}

LLMatrix4::LLMatrix4(F32 angle, const LLVector4& vec)
{
	initRotation(LLQuaternion(angle, vec));

	mMatrix[3][0] = mMatrix[3][1] = mMatrix[3][2] = 0.f;
	mMatrix[3][3] = 1.f;
}

LLMatrix4::LLMatrix4(F32 roll, F32 pitch, F32 yaw, const LLVector4& pos)
{
	initRotTrans(LLQuaternion(LLMatrix3(roll, pitch, yaw)), pos);
}

LLMatrix4::LLMatrix4(F32 roll, F32 pitch, F32 yaw)
{
	initRotation(LLQuaternion(LLMatrix3(roll, pitch, yaw)));

	mMatrix[3][0] = mMatrix[3][1] = mMatrix[3][2] = 0.f;
	mMatrix[3][3] = 1.f;
}

// Assignment methods

void LLMatrix4::set(const F32* mat)
{
#if 0
	mMatrix[0][0] = mat[0];
	mMatrix[0][1] = mat[1];
	mMatrix[0][2] = mat[2];
	mMatrix[0][3] = mat[3];

	mMatrix[1][0] = mat[4];
	mMatrix[1][1] = mat[5];
	mMatrix[1][2] = mat[6];
	mMatrix[1][3] = mat[7];

	mMatrix[2][0] = mat[8];
	mMatrix[2][1] = mat[9];
	mMatrix[2][2] = mat[10];
	mMatrix[2][3] = mat[11];

	mMatrix[3][0] = mat[12];
	mMatrix[3][1] = mat[13];
	mMatrix[3][2] = mat[14];
	mMatrix[3][3] = mat[15];
#else
	memcpy((void*)mMatrix, (void*)mat, 16 * sizeof(F32));
#endif
}

const LLMatrix4& LLMatrix4::setZero()
{
	mMatrix[0][0] = 0.f;
	mMatrix[0][1] = 0.f;
	mMatrix[0][2] = 0.f;
	mMatrix[0][3] = 0.f;

	mMatrix[1][0] = 0.f;
	mMatrix[1][1] = 0.f;
	mMatrix[1][2] = 0.f;
	mMatrix[1][3] = 0.f;

	mMatrix[2][0] = 0.f;
	mMatrix[2][1] = 0.f;
	mMatrix[2][2] = 0.f;
	mMatrix[2][3] = 0.f;

	mMatrix[3][0] = 0.f;
	mMatrix[3][1] = 0.f;
	mMatrix[3][2] = 0.f;
	mMatrix[3][3] = 0.f;
	return *this;
}

// Various useful methods

const LLMatrix4& LLMatrix4::transpose()
{
	LLMatrix4 mat;
	mat.mMatrix[0][0] = mMatrix[0][0];
	mat.mMatrix[1][0] = mMatrix[0][1];
	mat.mMatrix[2][0] = mMatrix[0][2];
	mat.mMatrix[3][0] = mMatrix[0][3];

	mat.mMatrix[0][1] = mMatrix[1][0];
	mat.mMatrix[1][1] = mMatrix[1][1];
	mat.mMatrix[2][1] = mMatrix[1][2];
	mat.mMatrix[3][1] = mMatrix[1][3];

	mat.mMatrix[0][2] = mMatrix[2][0];
	mat.mMatrix[1][2] = mMatrix[2][1];
	mat.mMatrix[2][2] = mMatrix[2][2];
	mat.mMatrix[3][2] = mMatrix[2][3];

	mat.mMatrix[0][3] = mMatrix[3][0];
	mat.mMatrix[1][3] = mMatrix[3][1];
	mat.mMatrix[2][3] = mMatrix[3][2];
	mat.mMatrix[3][3] = mMatrix[3][3];

	*this = mat;
	return *this;
}

F32 LLMatrix4::determinant() const
{
	F32 value = mMatrix[0][3] * mMatrix[1][2] * mMatrix[2][1] * mMatrix[3][0] -
				mMatrix[0][2] * mMatrix[1][3] * mMatrix[2][1] * mMatrix[3][0] -
				mMatrix[0][3] * mMatrix[1][1] * mMatrix[2][2] * mMatrix[3][0] +
				mMatrix[0][1] * mMatrix[1][3] * mMatrix[2][2] * mMatrix[3][0] +
				mMatrix[0][2] * mMatrix[1][1] * mMatrix[2][3] * mMatrix[3][0] -
				mMatrix[0][1] * mMatrix[1][2] * mMatrix[2][3] * mMatrix[3][0] -
				mMatrix[0][3] * mMatrix[1][2] * mMatrix[2][0] * mMatrix[3][1] +
				mMatrix[0][2] * mMatrix[1][3] * mMatrix[2][0] * mMatrix[3][1] +
				mMatrix[0][3] * mMatrix[1][0] * mMatrix[2][2] * mMatrix[3][1] -
				mMatrix[0][0] * mMatrix[1][3] * mMatrix[2][2] * mMatrix[3][1] -
				mMatrix[0][2] * mMatrix[1][0] * mMatrix[2][3] * mMatrix[3][1] +
				mMatrix[0][0] * mMatrix[1][2] * mMatrix[2][3] * mMatrix[3][1] +
				mMatrix[0][3] * mMatrix[1][1] * mMatrix[2][0] * mMatrix[3][2] -
				mMatrix[0][1] * mMatrix[1][3] * mMatrix[2][0] * mMatrix[3][2] -
				mMatrix[0][3] * mMatrix[1][0] * mMatrix[2][1] * mMatrix[3][2] +
				mMatrix[0][0] * mMatrix[1][3] * mMatrix[2][1] * mMatrix[3][2] +
				mMatrix[0][1] * mMatrix[1][0] * mMatrix[2][3] * mMatrix[3][2] -
				mMatrix[0][0] * mMatrix[1][1] * mMatrix[2][3] * mMatrix[3][2] -
				mMatrix[0][2] * mMatrix[1][1] * mMatrix[2][0] * mMatrix[3][3] +
				mMatrix[0][1] * mMatrix[1][2] * mMatrix[2][0] * mMatrix[3][3] +
				mMatrix[0][2] * mMatrix[1][0] * mMatrix[2][1] * mMatrix[3][3] -
				mMatrix[0][0] * mMatrix[1][2] * mMatrix[2][1] * mMatrix[3][3] -
				mMatrix[0][1] * mMatrix[1][0] * mMatrix[2][2] * mMatrix[3][3] +
				mMatrix[0][0] * mMatrix[1][1] * mMatrix[2][2] * mMatrix[3][3];

	return value;
}

// Only works for pure orthonormal, homogeneous transform matrices.
const LLMatrix4& LLMatrix4::invert()
{
	// transpose the rotation part

	F32 temp = mMatrix[VX][VY];
	mMatrix[VX][VY] = mMatrix[VY][VX];
	mMatrix[VY][VX] = temp;

	temp = mMatrix[VX][VZ];
	mMatrix[VX][VZ] = mMatrix[VZ][VX];
	mMatrix[VZ][VX] = temp;

	temp = mMatrix[VY][VZ];
	mMatrix[VY][VZ] = mMatrix[VZ][VY];
	mMatrix[VZ][VY] = temp;

	// Rotate the translation part by the new rotation (temporarily store in
	// empty column of matrix)
	for (U32 j = 0; j < 3; ++j)
	{
		mMatrix[j][VW] = mMatrix[VW][VX] * mMatrix[VX][j] +
						 mMatrix[VW][VY] * mMatrix[VY][j] +
						 mMatrix[VW][VZ] * mMatrix[VZ][j];
	}

	// Negate and copy the temporary vector back to the tranlation row
	mMatrix[VW][VX] = -mMatrix[VX][VW];
	mMatrix[VW][VY] = -mMatrix[VY][VW];
	mMatrix[VW][VZ] = -mMatrix[VZ][VW];

   	// Zero the empty column again
	mMatrix[VX][VW] = mMatrix[VY][VW] = mMatrix[VZ][VW] = 0.0f;

	return *this;
}

// does an actual inversion
const LLMatrix4& LLMatrix4::invert_real()
{
	F32* m = &mMatrix[0][0];

	F32 inv[16];
	F32 det;

	inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] -
			 m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] -
			 m[13] * m[7] * m[10];
	inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] +
			 m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
			 m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
	inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
			 m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
	inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] +
			  m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
			  m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
	inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] +
			 m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] +
			 m[13] * m[3] * m[10];
	inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] -
			 m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] -
			 m[12] * m[3] * m[10];
	inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] +
			 m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] +
			 m[12] * m[3] * m[9];
	inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] -
			  m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
			  m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
	inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] -
			 m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] -
			 m[13] * m[3] * m[6];
	inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
			 m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
	inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
			  m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
	inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] +
			  m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] +
			  m[12] * m[2] * m[5];
	inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
			 m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
	inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
			 m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
	inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
			  m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
	inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
			  m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

	det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

	if (det == 0)
	{
		setZero();
	}
	else
	{
		det = 1.0 / det;

		for (U32 i = 0; i < 16; ++i)
		{
			m[i] = inv[i] * det;
		}
	}

	return *this;
}

// Convenience function for simplifying comparison-heavy code by intentionally
// stomping values in [-FLT_EPS,FLT_EPS] to 0.0f
void LLMatrix4::condition()
{
	for (U32 i = 0; i < 3; ++i)
	{
		for (U32 j = 0; j < 3; ++j)
		{
			mMatrix[i][j] = mMatrix[i][j] > -FLT_EPSILON &&
							mMatrix[i][j] < FLT_EPSILON ? 0.f : mMatrix[i][j];
		}
	}
}

// SJB: This code is correct for a logicly stored (non-transposed) matrix;
// Our matrices are stored transposed, OpenGL style, so this generates the
// INVERSE quaternion (-x, -y, -z, w) !
// Because we use similar logic in LLQuaternion::getMatrix3, we are internally
// consistant so everything works OK :)
LLQuaternion LLMatrix4::quaternion() const
{
	LLQuaternion quat;

	F32 tr = mMatrix[0][0] + mMatrix[1][1] + mMatrix[2][2];

	// Check the diagonal
	if (tr > 0.f)
	{
		F32 s = sqrtf(tr + 1.f);
		quat.mQ[VS] = s * 0.5f;
		s = 0.5f / s;
		quat.mQ[VX] = (mMatrix[1][2] - mMatrix[2][1]) * s;
		quat.mQ[VY] = (mMatrix[2][0] - mMatrix[0][2]) * s;
		quat.mQ[VZ] = (mMatrix[0][1] - mMatrix[1][0]) * s;
	}
	else	// Diagonal is negative
	{
		U32 i = 0;
		if (mMatrix[1][1] > mMatrix[0][0])
		{
			i = 1;
		}
		if (mMatrix[2][2] > mMatrix[i][i])
		{
			i = 2;
		}

		static const U32 nxt[3] = { 1, 2, 0 };
		U32 j = nxt[i];
		U32 k = nxt[j];

		F32 s = sqrtf((mMatrix[i][i] - (mMatrix[j][j] + mMatrix[k][k])) + 1.f);

		F32 q[4];
		q[i] = s * 0.5f;

		if (s != 0.f)
		{
			s = 0.5f / s;
		}

		q[3] = (mMatrix[j][k] - mMatrix[k][j]) * s;
		q[j] = (mMatrix[i][j] + mMatrix[j][i]) * s;
		q[k] = (mMatrix[i][k] + mMatrix[k][i]) * s;

		quat.set(q);
	}
	return quat;
}

void LLMatrix4::initRows(const LLVector4& row0, const LLVector4& row1,
						 const LLVector4& row2, const LLVector4& row3)
{
	mMatrix[0][0] = row0.mV[0];
	mMatrix[0][1] = row0.mV[1];
	mMatrix[0][2] = row0.mV[2];
	mMatrix[0][3] = row0.mV[3];

	mMatrix[1][0] = row1.mV[0];
	mMatrix[1][1] = row1.mV[1];
	mMatrix[1][2] = row1.mV[2];
	mMatrix[1][3] = row1.mV[3];

	mMatrix[2][0] = row2.mV[0];
	mMatrix[2][1] = row2.mV[1];
	mMatrix[2][2] = row2.mV[2];
	mMatrix[2][3] = row2.mV[3];

	mMatrix[3][0] = row3.mV[0];
	mMatrix[3][1] = row3.mV[1];
	mMatrix[3][2] = row3.mV[2];
	mMatrix[3][3] = row3.mV[3];
}

const LLMatrix4& LLMatrix4::initRotation(F32 angle, const LLVector4& vec)
{
	LLMatrix3 mat(angle, vec);
	return initMatrix(mat);
}

const LLMatrix4& LLMatrix4::initRotation(F32 roll, F32 pitch, F32 yaw)
{
	LLMatrix3 mat(roll, pitch, yaw);
	return initMatrix(mat);
}

const LLMatrix4& LLMatrix4::initRotation(const LLQuaternion& q)
{
	LLMatrix3 mat(q);
	return initMatrix(mat);
}

const LLMatrix4& LLMatrix4::initRotTrans(F32 angle, const LLVector3& axis,
										 const LLVector3& translation)
{
	LLMatrix3 mat(angle, axis);
	initMatrix(mat);
	setTranslation(translation);
	return *this;
}

const LLMatrix4& LLMatrix4::initRotTrans(F32 roll, F32 pitch, F32 yaw,
										 const LLVector4& translation)
{
	LLMatrix3 mat(roll, pitch, yaw);
	initMatrix(mat);
	setTranslation(translation);
	return *this;
}

const LLMatrix4& LLMatrix4::initRotTrans(const LLQuaternion& q,
										 const LLVector4& translation)
{
	LLMatrix3	mat(q);
	initMatrix(mat);
	setTranslation(translation);
	return *this;
}

const LLMatrix4& LLMatrix4::initScale(const LLVector3& scale)
{
	setIdentity();

	mMatrix[VX][VX] = scale.mV[VX];
	mMatrix[VY][VY] = scale.mV[VY];
	mMatrix[VZ][VZ] = scale.mV[VZ];

	return *this;
}

const LLMatrix4& LLMatrix4::initAll(const LLVector3& scale,
									const LLQuaternion& q,
									const LLVector3& pos)
{
	F32 sx = scale.mV[0];
	F32 sy = scale.mV[1];
	F32 sz = scale.mV[2];

	F32 xx = q.mQ[VX] * q.mQ[VX];
	F32 xy = q.mQ[VX] * q.mQ[VY];
	F32 xz = q.mQ[VX] * q.mQ[VZ];
	F32 xw = q.mQ[VX] * q.mQ[VW];

	F32 yy = q.mQ[VY] * q.mQ[VY];
	F32 yz = q.mQ[VY] * q.mQ[VZ];
	F32 yw = q.mQ[VY] * q.mQ[VW];

	F32 zz = q.mQ[VZ] * q.mQ[VZ];
	F32 zw = q.mQ[VZ] * q.mQ[VW];

    mMatrix[0][0]  = (1.f - 2.f * (yy + zz)) * sx;
    mMatrix[0][1]  = (2.f * (xy + zw)) * sx;
    mMatrix[0][2]  = (2.f * (xz - yw)) * sx;

    mMatrix[1][0]  = (2.f * (xy - zw)) *sy;
    mMatrix[1][1]  = (1.f - 2.f * (xx + zz)) * sy;
    mMatrix[1][2]  = (2.f * (yz + xw)) * sy;

    mMatrix[2][0]  = (2.f * (xz + yw)) * sz;
    mMatrix[2][1]  = (2.f * (yz - xw)) * sz;
    mMatrix[2][2]  = (1.f - 2.f * (xx + yy)) * sz;

	mMatrix[3][0]  = pos.mV[0];
	mMatrix[3][1]  = pos.mV[1];
	mMatrix[3][2]  = pos.mV[2];
	mMatrix[3][3]  = 1.0;

	// TODO -- should we set the translation portion to zero?
	return *this;
}

const LLMatrix4& LLMatrix4::rotate(F32 angle, const LLVector4& vec)
{
	LLMatrix4 mat(angle, vec);
	*this *= mat;
	return *this;
}

const LLMatrix4& LLMatrix4::rotate(F32 roll, F32 pitch, F32 yaw)
{
	LLMatrix4 mat(roll, pitch, yaw);
	*this *= mat;
	return *this;
}

const LLMatrix4& LLMatrix4::rotate(const LLQuaternion& q)
{
	LLMatrix4 mat(q);
	*this *= mat;
	return *this;
}

const LLMatrix4& LLMatrix4::translate(const LLVector3& vec)
{
	mMatrix[3][0] += vec.mV[0];
	mMatrix[3][1] += vec.mV[1];
	mMatrix[3][2] += vec.mV[2];
	return *this;
}

void LLMatrix4::setFwdRow(const LLVector3& row)
{
	mMatrix[VX][VX] = row.mV[VX];
	mMatrix[VX][VY] = row.mV[VY];
	mMatrix[VX][VZ] = row.mV[VZ];
}

void LLMatrix4::setLeftRow(const LLVector3& row)
{
	mMatrix[VY][VX] = row.mV[VX];
	mMatrix[VY][VY] = row.mV[VY];
	mMatrix[VY][VZ] = row.mV[VZ];
}

void LLMatrix4::setUpRow(const LLVector3& row)
{
	mMatrix[VZ][VX] = row.mV[VX];
	mMatrix[VZ][VY] = row.mV[VY];
	mMatrix[VZ][VZ] = row.mV[VZ];
}

void LLMatrix4::setFwdCol(const LLVector3& col)
{
	mMatrix[VX][VX] = col.mV[VX];
	mMatrix[VY][VX] = col.mV[VY];
	mMatrix[VZ][VX] = col.mV[VZ];
}

void LLMatrix4::setLeftCol(const LLVector3& col)
{
	mMatrix[VX][VY] = col.mV[VX];
	mMatrix[VY][VY] = col.mV[VY];
	mMatrix[VZ][VY] = col.mV[VZ];
}

void LLMatrix4::setUpCol(const LLVector3& col)
{
	mMatrix[VX][VZ] = col.mV[VX];
	mMatrix[VY][VZ] = col.mV[VY];
	mMatrix[VZ][VZ] = col.mV[VZ];
}

const LLMatrix4& LLMatrix4::setTranslation(F32 tx, F32 ty, F32 tz)
{
	mMatrix[VW][VX] = tx;
	mMatrix[VW][VY] = ty;
	mMatrix[VW][VZ] = tz;
	return *this;
}

const LLMatrix4& LLMatrix4::setTranslation(const LLVector3& translation)
{
	mMatrix[VW][VX] = translation.mV[VX];
	mMatrix[VW][VY] = translation.mV[VY];
	mMatrix[VW][VZ] = translation.mV[VZ];
	return *this;
}

const LLMatrix4& LLMatrix4::setTranslation(const LLVector4& translation)
{
	mMatrix[VW][VX] = translation.mV[VX];
	mMatrix[VW][VY] = translation.mV[VY];
	mMatrix[VW][VZ] = translation.mV[VZ];
	return *this;
}

// LLMatrix3 Extraction and Setting
LLMatrix3 LLMatrix4::getMat3() const
{
	LLMatrix3 retmat;

	retmat.mMatrix[0][0] = mMatrix[0][0];
	retmat.mMatrix[0][1] = mMatrix[0][1];
	retmat.mMatrix[0][2] = mMatrix[0][2];

	retmat.mMatrix[1][0] = mMatrix[1][0];
	retmat.mMatrix[1][1] = mMatrix[1][1];
	retmat.mMatrix[1][2] = mMatrix[1][2];

	retmat.mMatrix[2][0] = mMatrix[2][0];
	retmat.mMatrix[2][1] = mMatrix[2][1];
	retmat.mMatrix[2][2] = mMatrix[2][2];

	return retmat;
}

const LLMatrix4& LLMatrix4::initMatrix(const LLMatrix3& mat)
{
	mMatrix[0][0] = mat.mMatrix[0][0];
	mMatrix[0][1] = mat.mMatrix[0][1];
	mMatrix[0][2] = mat.mMatrix[0][2];
	mMatrix[0][3] = 0.f;

	mMatrix[1][0] = mat.mMatrix[1][0];
	mMatrix[1][1] = mat.mMatrix[1][1];
	mMatrix[1][2] = mat.mMatrix[1][2];
	mMatrix[1][3] = 0.f;

	mMatrix[2][0] = mat.mMatrix[2][0];
	mMatrix[2][1] = mat.mMatrix[2][1];
	mMatrix[2][2] = mat.mMatrix[2][2];
	mMatrix[2][3] = 0.f;

	mMatrix[3][0] = 0.f;
	mMatrix[3][1] = 0.f;
	mMatrix[3][2] = 0.f;
	mMatrix[3][3] = 1.f;
	return *this;
}

const LLMatrix4& LLMatrix4::initMatrix(const LLMatrix3& mat,
									   const LLVector4& translation)
{
	mMatrix[0][0] = mat.mMatrix[0][0];
	mMatrix[0][1] = mat.mMatrix[0][1];
	mMatrix[0][2] = mat.mMatrix[0][2];
	mMatrix[0][3] = 0.f;

	mMatrix[1][0] = mat.mMatrix[1][0];
	mMatrix[1][1] = mat.mMatrix[1][1];
	mMatrix[1][2] = mat.mMatrix[1][2];
	mMatrix[1][3] = 0.f;

	mMatrix[2][0] = mat.mMatrix[2][0];
	mMatrix[2][1] = mat.mMatrix[2][1];
	mMatrix[2][2] = mat.mMatrix[2][2];
	mMatrix[2][3] = 0.f;

	mMatrix[3][0] = translation.mV[0];
	mMatrix[3][1] = translation.mV[1];
	mMatrix[3][2] = translation.mV[2];
	mMatrix[3][3] = 1.f;
	return *this;
}

// LLMatrix4 Operators

LLVector4 operator*(const LLVector4& a, const LLMatrix4& b)
{
	// Operate "to the left" on row-vector a
	return LLVector4(a.mV[VX] * b.mMatrix[VX][VX] +
					 a.mV[VY] * b.mMatrix[VY][VX] +
					 a.mV[VZ] * b.mMatrix[VZ][VX] +
					 a.mV[VW] * b.mMatrix[VW][VX],

					 a.mV[VX] * b.mMatrix[VX][VY] +
					 a.mV[VY] * b.mMatrix[VY][VY] +
					 a.mV[VZ] * b.mMatrix[VZ][VY] +
					 a.mV[VW] * b.mMatrix[VW][VY],

					 a.mV[VX] * b.mMatrix[VX][VZ] +
					 a.mV[VY] * b.mMatrix[VY][VZ] +
					 a.mV[VZ] * b.mMatrix[VZ][VZ] +
					 a.mV[VW] * b.mMatrix[VW][VZ],

					 a.mV[VX] * b.mMatrix[VX][VW] +
					 a.mV[VY] * b.mMatrix[VY][VW] +
					 a.mV[VZ] * b.mMatrix[VZ][VW] +
					 a.mV[VW] * b.mMatrix[VW][VW]);
}

LLVector4 rotate_vector(const LLVector4& a, const LLMatrix4& b)
{
	// Rotates but does not translate
	// Operate "to the left" on row-vector a
	LLVector4 vec;
	vec.mV[VX] = a.mV[VX] * b.mMatrix[VX][VX] +
				 a.mV[VY] * b.mMatrix[VY][VX] +
				 a.mV[VZ] * b.mMatrix[VZ][VX];

	vec.mV[VY] = a.mV[VX] * b.mMatrix[VX][VY] +
				 a.mV[VY] * b.mMatrix[VY][VY] +
				 a.mV[VZ] * b.mMatrix[VZ][VY];

	vec.mV[VZ] = a.mV[VX] * b.mMatrix[VX][VZ] +
				 a.mV[VY] * b.mMatrix[VY][VZ] +
				 a.mV[VZ] * b.mMatrix[VZ][VZ];
#if 0
	vec.mV[VW] = a.mV[VX] * b.mMatrix[VX][VW] +
				 a.mV[VY] * b.mMatrix[VY][VW] +
				 a.mV[VZ] * b.mMatrix[VZ][VW];
#else
	vec.mV[VW] = a.mV[VW];
#endif
	return vec;
}

LLVector3 rotate_vector(const LLVector3& a, const LLMatrix4& b)
{
	// Rotates but does not translate. Operate "to the left" on row-vector a.
	LLVector3 vec;
	vec.mV[VX] = a.mV[VX] * b.mMatrix[VX][VX] +
				 a.mV[VY] * b.mMatrix[VY][VX] +
				 a.mV[VZ] * b.mMatrix[VZ][VX];

	vec.mV[VY] = a.mV[VX] * b.mMatrix[VX][VY] +
				 a.mV[VY] * b.mMatrix[VY][VY] +
				 a.mV[VZ] * b.mMatrix[VZ][VY];

	vec.mV[VZ] = a.mV[VX] * b.mMatrix[VX][VZ] +
				 a.mV[VY] * b.mMatrix[VY][VZ] +
				 a.mV[VZ] * b.mMatrix[VZ][VZ];
	return vec;
}

bool operator==(const LLMatrix4& a, const LLMatrix4& b)
{
	for (U32 i = 0; i < NUM_VALUES_IN_MAT4; ++i)
	{
		for (U32 j = 0; j < NUM_VALUES_IN_MAT4; ++j)
		{
			if (a.mMatrix[j][i] != b.mMatrix[j][i])
			{
				return false;
			}
		}
	}
	return true;
}

bool operator!=(const LLMatrix4& a, const LLMatrix4& b)
{
	for (U32 i = 0; i < NUM_VALUES_IN_MAT4; ++i)
	{
		for (U32 j = 0; j < NUM_VALUES_IN_MAT4; ++j)
		{
			if (a.mMatrix[j][i] != b.mMatrix[j][i])
			{
				return true;
			}
		}
	}
	return false;
}

bool operator<(const LLMatrix4& a, const LLMatrix4& b)
{
	for (U32 i = 0; i < NUM_VALUES_IN_MAT4; ++i)
	{
		for (U32 j = 0; j < NUM_VALUES_IN_MAT4; ++j)
		{
			if (a.mMatrix[i][j] != b.mMatrix[i][j])
			{
				return a.mMatrix[i][j] < b.mMatrix[i][j];
			}
		}
	}

	return false;
}

const LLMatrix4& operator*=(LLMatrix4& a, F32 k)
{
	for (U32 i = 0; i < NUM_VALUES_IN_MAT4; ++i)
	{
		for (U32 j = 0; j < NUM_VALUES_IN_MAT4; ++j)
		{
			a.mMatrix[j][i] *= k;
		}
	}
	return a;
}

std::ostream& operator<<(std::ostream& s, const LLMatrix4& a)
{
	s << "{ " << a.mMatrix[VX][VX] << ", " << a.mMatrix[VX][VY] << ", "
	  << a.mMatrix[VX][VZ] << ", " << a.mMatrix[VX][VW] << "; "
	  << a.mMatrix[VY][VX] << ", " << a.mMatrix[VY][VY] << ", "
	  << a.mMatrix[VY][VZ] << ", " << a.mMatrix[VY][VW] << "; "
	  << a.mMatrix[VZ][VX] << ", " << a.mMatrix[VZ][VY] << ", "
	  << a.mMatrix[VZ][VZ] << ", " << a.mMatrix[VZ][VW] << "; "
	  << a.mMatrix[VW][VX] << ", " << a.mMatrix[VW][VY] << ", "
	  << a.mMatrix[VW][VZ] << ", " << a.mMatrix[VW][VW] << " }";
	return s;
}

LLSD LLMatrix4::getValue() const
{
	LLSD ret;

	ret[0] = mMatrix[0][0];
	ret[1] = mMatrix[0][1];
	ret[2] = mMatrix[0][2];
	ret[3] = mMatrix[0][3];

	ret[4] = mMatrix[1][0];
	ret[5] = mMatrix[1][1];
	ret[6] = mMatrix[1][2];
	ret[7] = mMatrix[1][3];

	ret[8] = mMatrix[2][0];
	ret[9] = mMatrix[2][1];
	ret[10] = mMatrix[2][2];
	ret[11] = mMatrix[2][3];

	ret[12] = mMatrix[3][0];
	ret[13] = mMatrix[3][1];
	ret[14] = mMatrix[3][2];
	ret[15] = mMatrix[3][3];

	return ret;
}

void LLMatrix4::setValue(const LLSD& data)
{
	mMatrix[0][0] = data[0].asReal();
	mMatrix[0][1] = data[1].asReal();
	mMatrix[0][2] = data[2].asReal();
	mMatrix[0][3] = data[3].asReal();

	mMatrix[1][0] = data[4].asReal();
	mMatrix[1][1] = data[5].asReal();
	mMatrix[1][2] = data[6].asReal();
	mMatrix[1][3] = data[7].asReal();

	mMatrix[2][0] = data[8].asReal();
	mMatrix[2][1] = data[9].asReal();
	mMatrix[2][2] = data[10].asReal();
	mMatrix[2][3] = data[11].asReal();

	mMatrix[3][0] = data[12].asReal();
	mMatrix[3][1] = data[13].asReal();
	mMatrix[3][2] = data[14].asReal();
	mMatrix[3][3] = data[15].asReal();
}
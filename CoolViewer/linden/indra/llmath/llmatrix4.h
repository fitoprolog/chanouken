/**
 * @file llmatrix4.h
 * @brief LLMatrix4 class header file.
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

#ifndef LL_M4MATH_H
#define LL_M4MATH_H

#include "llvector3.h"
#include "llvector4.h"

class LLMatrix3;
class LLQuaternion;

// NOTA BENE: Currently assuming a right-handed, x-forward, y-left, z-up
// universe

// Us versus OpenGL:

// Even though OpenGL uses column vectors and we use row vectors, we can plug
// our matrices directly into OpenGL. This is because OpenGL numbers its
// matrices going columnwise:
//
// OpenGL indexing:          Our indexing:
// 0  4  8 12                [0][0] [0][1] [0][2] [0][3]
// 1  5  9 13                [1][0] [1][1] [1][2] [1][3]
// 2  6 10 14                [2][0] [2][1] [2][2] [2][3]
// 3  7 11 15                [3][0] [3][1] [3][2] [3][3]
//
// So when you're looking at OpenGL related matrices online, our matrices will
// be "transposed". But our matrices can be plugged directly into OpenGL and
// work fine!
//

// We're using row vectors - [vx, vy, vz, vw]
//
// There are several different ways of thinking of matrices, if you mix them
// up, you'll get very confused.
//
// One way to think about it is a matrix that takes the origin frame A
// and rotates it into B': i.e. A*M = B
//
//		Vectors:
//		f - forward axis of B expressed in A
//		l - left axis of B expressed in A
//		u - up axis of B expressed in A
//
//		|  0: fx  1: fy  2: fz  3:0 |
//  M = |  4: lx  5: ly  6: lz  7:0 |
//      |  8: ux  9: uy 10: uz 11:0 |
//      | 12: 0  13: 0  14:  0 15:1 |
//
//
//
//
// Another way to think of matrices is matrix that takes a point p in frame A,
// and puts it into frame B:
// This is used most commonly for the modelview matrix.
//
// so p*M = p'
//
//		Vectors:
//      f - forward of frame B in frame A
//      l - left of frame B in frame A
//      u - up of frame B in frame A
//      o - origin of frame frame B in frame A
//
//		|  0: fx  1: lx  2: ux  3:0 |
//  M = |  4: fy  5: ly  6: uy  7:0 |
//      |  8: fz  9: lz 10: uz 11:0 |
//      | 12:-of 13:-ol 14:-ou 15:1 |
//
//		of, ol, and ou mean the component of the "global" origin o in the f
//      axis, l axis, and u axis.
//

constexpr U32 NUM_VALUES_IN_MAT4 = 4;

class LLMatrix4
{
public:
	// Initializes Matrix to identity matrix
	LLMatrix4()
	{
		setIdentity();
	}

	// Initializes Matrix to values in mat
	LL_INLINE explicit LLMatrix4(const F32* mat)	{ set(mat); }

	// Initializes Matrix to values in mat and sets position to (0,0,0)
	explicit LLMatrix4(const LLMatrix3& mat);

	// Initializes Matrix with rotation q and sets position to (0,0,0)
	explicit LLMatrix4(const LLQuaternion& q);

	// Initializes Matrix to values in mat and pos
	LLMatrix4(const LLMatrix3& mat, const LLVector4& pos);

	// These are really, really, inefficient as implemented! - djs

	// Initializes Matrix with rotation q and position pos
	LLMatrix4(const LLQuaternion& q, const LLVector4& pos);

	// Initializes Matrix with axis-angle and position
	LLMatrix4(F32 angle, const LLVector4& vec, const LLVector4& pos);

	// Initializes Matrix with axis-angle and sets position to (0,0,0)
	LLMatrix4(F32 angle, const LLVector4& vec);

	// Initializes Matrix with Euler angles
	LLMatrix4(F32 roll, F32 pitch, F32 yaw, const LLVector4& pos);

	// Initializes Matrix with Euler angles
	LLMatrix4(F32 roll, F32 pitch, F32 yaw);

	void set(const F32* mat);

	// Returns a "this" as an F32 pointer.
	LL_INLINE F32* getF32ptr()
	{
		return (F32*)mMatrix;
	}

	// Returns a "this" as a const F32 pointer.
	LL_INLINE const F32* const getF32ptr() const
	{
		return (const F32* const)mMatrix;
	}

	LLSD getValue() const;
	void setValue(const LLSD&);

	//////////////////////////////
	//
	// Matrix initializers - these replace any existing values in the matrix
	//

	void initRows(const LLVector4& row0, const LLVector4& row1,
				  const LLVector4& row2, const LLVector4& row3);

	// various useful matrix functions
	LL_INLINE const LLMatrix4& setIdentity();	// Load identity matrix
	LL_INLINE bool isIdentity() const;
	const LLMatrix4& setZero();		// Clears matrix to all zeros.

	// Calculate rotation matrix for rotating angle radians about vec
	const LLMatrix4& initRotation(F32 angle, const LLVector4& axis);

	// Calculate rotation matrix from Euler angles
	const LLMatrix4& initRotation(F32 roll, F32 pitch, F32 yaw);

	// Set with Quaternion and position
	const LLMatrix4& initRotation(const LLQuaternion& q);

	// Position Only
	const LLMatrix4& initMatrix(const LLMatrix3& mat);
	const LLMatrix4& initMatrix(const LLMatrix3& mat,
								const LLVector4& translation);

	 // Rotation from axis angle + translation
	const LLMatrix4& initRotTrans(F32 angle, const LLVector3& axis,
								  const LLVector3& translation);

	// Rotation from Euler + translation
	const LLMatrix4& initRotTrans(F32 roll, F32 pitch, F32 yaw,
								  const LLVector4& pos);

	// Set with Quaternion and position
	const LLMatrix4& initRotTrans(const LLQuaternion& q, const LLVector4& pos);

	const LLMatrix4& initScale(const LLVector3& scale);

	// Set all
	const LLMatrix4& initAll(const LLVector3& scale, const LLQuaternion& q,
							 const LLVector3& pos);

	///////////////////////////
	//
	// Matrix setters - set some properties without modifying others
	//

	// Sets matrix to translate by (x,y,z)
	const LLMatrix4& setTranslation(F32 x, F32 y, F32 z);

	void setFwdRow(const LLVector3& row);
	void setLeftRow(const LLVector3& row);
	void setUpRow(const LLVector3& row);

	void setFwdCol(const LLVector3& col);
	void setLeftCol(const LLVector3& col);
	void setUpCol(const LLVector3& col);

	const LLMatrix4& setTranslation(const LLVector4& translation);
	const LLMatrix4& setTranslation(const LLVector3& translation);

	// Convenience function for simplifying comparison-heavy code by
	// intentionally stomping values in [-FLT_EPS,FLT_EPS] to 0.0f
	void condition();

	///////////////////////////
	//
	// Get properties of a matrix
	//

	F32 determinant() const;			// Returns determinant
	LLQuaternion quaternion() const;	// Returns quaternion

	LL_INLINE LLVector4 getFwdRow4() const;
	LL_INLINE LLVector4 getLeftRow4() const;
	LL_INLINE LLVector4 getUpRow4() const;

	LLMatrix3 getMat3() const;

	const LLVector3& getTranslation() const	{ return *(LLVector3*)&mMatrix[3][0]; }

	///////////////////////////
	//
	// Operations on an existing matrix
	//

	const LLMatrix4& transpose();	// Transpose LLMatrix4
	const LLMatrix4& invert();		// Invert LLMatrix4
	const LLMatrix4& invert_real();	// Invert LLMatrix4, works for all matrices

	// Rotate existing matrix
	// These are really, really, inefficient as implemented ! - djs

	// Rotate matrix by rotating angle radians about vec
	const LLMatrix4& rotate(F32 angle, const LLVector4& vec);

	// Rotate matrix by Euler angles
	const LLMatrix4& rotate(F32 roll, F32 pitch, F32 yaw);

	// Rotate matrix by Quaternion
	const LLMatrix4& rotate(const LLQuaternion& q);

	// Translate existing matrix
	// Translate matrix by (vec[VX], vec[VY], vec[VZ])
	const LLMatrix4& translate(const LLVector3& vec);

	///////////////////////
	//
	// Operators
	//

#if 0
	// Return a * b
	friend LL_INLINE LLMatrix4 operator*(const LLMatrix4& a,
										 const LLMatrix4& b);
#endif

	// Return transform of vector a by matrix b
	friend LLVector4 operator*(const LLVector4& a, const LLMatrix4& b);

	// Return full transform of a by matrix b
	friend LL_INLINE const LLVector3 operator*(const LLVector3& a,
											   const LLMatrix4& b);

	// Rotates a but does not translate
	friend LLVector4 rotate_vector(const LLVector4& a, const LLMatrix4& b);

	// Rotates a but does not translate
	friend LLVector3 rotate_vector(const LLVector3& a, const LLMatrix4& b);

	// Returns a == b
	friend bool operator==(const LLMatrix4& a, const LLMatrix4& b);
	// Returns a != b
	friend bool operator!=(const LLMatrix4& a, const LLMatrix4& b);
	// Returns a < b
	friend bool operator<(const LLMatrix4& a, const LLMatrix4& b);

	// Returns a + b
	friend LL_INLINE const LLMatrix4& operator+=(LLMatrix4& a, const LLMatrix4& b);
	// Returns a - b
	friend LL_INLINE const LLMatrix4& operator-=(LLMatrix4& a, const LLMatrix4& b);
	// Returns a * b
	friend LL_INLINE const LLMatrix4& operator*=(LLMatrix4& a, const LLMatrix4& b);
	// Returns a * b
	friend LL_INLINE const LLMatrix4& operator*=(LLMatrix4& a, const F32& b);
	// Streams a
	friend std::ostream& operator<<(std::ostream& s, const LLMatrix4& a);

public:
	F32	mMatrix[NUM_VALUES_IN_MAT4][NUM_VALUES_IN_MAT4];
};

LL_INLINE LLVector4 LLMatrix4::getFwdRow4() const
{
	return LLVector4(mMatrix[VX][VX], mMatrix[VX][VY], mMatrix[VX][VZ],
					 mMatrix[VX][VW]);
}

LL_INLINE LLVector4 LLMatrix4::getLeftRow4() const
{
	return LLVector4(mMatrix[VY][VX], mMatrix[VY][VY], mMatrix[VY][VZ],
					 mMatrix[VY][VW]);
}

LL_INLINE LLVector4 LLMatrix4::getUpRow4() const
{
	return LLVector4(mMatrix[VZ][VX], mMatrix[VZ][VY], mMatrix[VZ][VZ],
					 mMatrix[VZ][VW]);
}

LL_INLINE const LLMatrix4& LLMatrix4::setIdentity()
{
	mMatrix[0][0] = 1.f;
	mMatrix[0][1] = 0.f;
	mMatrix[0][2] = 0.f;
	mMatrix[0][3] = 0.f;

	mMatrix[1][0] = 0.f;
	mMatrix[1][1] = 1.f;
	mMatrix[1][2] = 0.f;
	mMatrix[1][3] = 0.f;

	mMatrix[2][0] = 0.f;
	mMatrix[2][1] = 0.f;
	mMatrix[2][2] = 1.f;
	mMatrix[2][3] = 0.f;

	mMatrix[3][0] = 0.f;
	mMatrix[3][1] = 0.f;
	mMatrix[3][2] = 0.f;
	mMatrix[3][3] = 1.f;
	return *this;
}

LL_INLINE bool LLMatrix4::isIdentity() const
{
	return mMatrix[0][0] == 1.f && mMatrix[0][1] == 0.f &&
		   mMatrix[0][2] == 0.f && mMatrix[0][3] == 0.f &&

		   mMatrix[1][0] == 0.f && mMatrix[1][1] == 1.f &&
		   mMatrix[1][2] == 0.f && mMatrix[1][3] == 0.f &&

		   mMatrix[2][0] == 0.f && mMatrix[2][1] == 0.f &&
		   mMatrix[2][2] == 1.f && mMatrix[2][3] == 0.f &&

		   mMatrix[3][0] == 0.f && mMatrix[3][1] == 0.f &&
		   mMatrix[3][2] == 0.f && mMatrix[3][3] == 1.f;
}

#if 0
LL_INLINE LLMatrix4 operator*(const LLMatrix4& a, const LLMatrix4& b)
{
	LLMatrix4 mat;
	for (U32 i = 0; i < NUM_VALUES_IN_MAT4; ++i)
	{
		for (U32 j = 0; j < NUM_VALUES_IN_MAT4; ++j)
		{
			mat.mMatrix[j][i] = a.mMatrix[j][0] * b.mMatrix[0][i] +
							    a.mMatrix[j][1] * b.mMatrix[1][i] +
							    a.mMatrix[j][2] * b.mMatrix[2][i] +
								a.mMatrix[j][3] * b.mMatrix[3][i];
		}
	}
	return mat;
}
#endif

LL_INLINE const LLMatrix4& operator*=(LLMatrix4& a, const LLMatrix4& b)
{
	LLMatrix4 mat;
	for (U32 i = 0; i < NUM_VALUES_IN_MAT4; ++i)
	{
		for (U32 j = 0; j < NUM_VALUES_IN_MAT4; ++j)
		{
			mat.mMatrix[j][i] = a.mMatrix[j][0] * b.mMatrix[0][i] +
							    a.mMatrix[j][1] * b.mMatrix[1][i] +
							    a.mMatrix[j][2] * b.mMatrix[2][i] +
								a.mMatrix[j][3] * b.mMatrix[3][i];
		}
	}
	a = mat;
	return a;
}

LL_INLINE const LLMatrix4& operator*=(LLMatrix4& a, const F32& b)
{
	LLMatrix4 mat;
	for (U32 i = 0; i < NUM_VALUES_IN_MAT4; ++i)
	{
		for (U32 j = 0; j < NUM_VALUES_IN_MAT4; ++j)
		{
			mat.mMatrix[j][i] = a.mMatrix[j][i] * b;
		}
	}
	a = mat;
	return a;
}

LL_INLINE const LLMatrix4& operator+=(LLMatrix4& a, const LLMatrix4& b)
{
	LLMatrix4 mat;
	for (U32 i = 0; i < NUM_VALUES_IN_MAT4; ++i)
	{
		for (U32 j = 0; j < NUM_VALUES_IN_MAT4; ++j)
		{
			mat.mMatrix[j][i] = a.mMatrix[j][i] + b.mMatrix[j][i];
		}
	}
	a = mat;
	return a;
}

LL_INLINE const LLMatrix4& operator-=(LLMatrix4& a, const LLMatrix4& b)
{
	LLMatrix4 mat;
	for (U32 i = 0; i < NUM_VALUES_IN_MAT4; ++i)
	{
		for (U32 j = 0; j < NUM_VALUES_IN_MAT4; ++j)
		{
			mat.mMatrix[j][i] = a.mMatrix[j][i] - b.mMatrix[j][i];
		}
	}
	a = mat;
	return a;
}

// Operates "to the left" on row-vector a
//
// When avatar vertex programs are off, this function is a hot spot in profiles
// due to software skinning in LLViewerJointMesh::updateGeometry().  JC
LL_INLINE const LLVector3 operator*(const LLVector3& a, const LLMatrix4& b)
{
	// This is better than making a temporary LLVector3.  This eliminates an
	// unnecessary LLVector3() constructor and also helps the compiler to
	// realize that the output floats do not alias the input floats, hence
	// eliminating redundant loads of a.mV[0], etc.  JC
	return LLVector3(a.mV[VX] * b.mMatrix[VX][VX] +
					 a.mV[VY] * b.mMatrix[VY][VX] +
					 a.mV[VZ] * b.mMatrix[VZ][VX] +
					 b.mMatrix[VW][VX],

					 a.mV[VX] * b.mMatrix[VX][VY] +
					 a.mV[VY] * b.mMatrix[VY][VY] +
					 a.mV[VZ] * b.mMatrix[VZ][VY] +
					 b.mMatrix[VW][VY],

					 a.mV[VX] * b.mMatrix[VX][VZ] +
					 a.mV[VY] * b.mMatrix[VY][VZ] +
					 a.mV[VZ] * b.mMatrix[VZ][VZ] +
					 b.mMatrix[VW][VZ]);
}

#endif

/**
 * @file llmatrix3a.inl
 * @brief LLMatrix3a inline definitions
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (C) 2010, Linden Research, Inc.
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

#ifndef LL_INL_INCLUDE
# error "You cannot #include this file yourself, #include llmath.h instead !"
#endif

LL_INLINE LLMatrix3a::LLMatrix3a(const LLVector4a& c0, const LLVector4a& c1,
								 const LLVector4a& c2)
{
	setColumns(c0, c1, c2);
}

LL_INLINE void LLMatrix3a::loadu(const LLMatrix3& src)
{
	mColumns[0].load3(src.mMatrix[0]);
	mColumns[1].load3(src.mMatrix[1]);
	mColumns[2].load3(src.mMatrix[2]);
}

LL_INLINE void LLMatrix3a::setRows(const LLVector4a& r0,
								   const LLVector4a& r1,
								   const LLVector4a& r2)
{
	mColumns[0] = r0;
	mColumns[1] = r1;
	mColumns[2] = r2;
	setTranspose(*this);
}

LL_INLINE void LLMatrix3a::setColumns(const LLVector4a& c0,
									  const LLVector4a& c1,
									  const LLVector4a& c2)
{
	mColumns[0] = c0;
	mColumns[1] = c1;
	mColumns[2] = c2;
}

LL_INLINE void LLMatrix3a::setTranspose(const LLMatrix3a& src)
{
	const LLQuad srcCol0 = src.mColumns[0];
	const LLQuad srcCol1 = src.mColumns[1];
	const LLQuad unpacklo = _mm_unpacklo_ps(srcCol0, srcCol1);
	mColumns[0] = _mm_movelh_ps(unpacklo, src.mColumns[2]);
	mColumns[1] = _mm_shuffle_ps(_mm_movehl_ps(srcCol0, unpacklo),
								 src.mColumns[2],
								 _MM_SHUFFLE(0, 1, 1, 0));
	mColumns[2] = _mm_shuffle_ps(_mm_unpackhi_ps(srcCol0, srcCol1),
								 src.mColumns[2],
								 _MM_SHUFFLE(0, 2, 1, 0));
}

LL_INLINE const LLVector4a& LLMatrix3a::getColumn(U32 column) const
{
	llassert(column < 3);
	return mColumns[column];
}

LL_INLINE void LLMatrix3a::setLerp(const LLMatrix3a& a,
								   const LLMatrix3a& b, F32 w)
{
	mColumns[0].setLerp(a.mColumns[0], b.mColumns[0], w);
	mColumns[1].setLerp(a.mColumns[1], b.mColumns[1], w);
	mColumns[2].setLerp(a.mColumns[2], b.mColumns[2], w);
}

LL_INLINE LLBool32 LLMatrix3a::isFinite() const
{
	return mColumns[0].isFinite3() && mColumns[1].isFinite3() &&
		   mColumns[2].isFinite3();
}

LL_INLINE void LLMatrix3a::getDeterminant(LLVector4a& dest) const
{
	LLVector4a col1xcol2; col1xcol2.setCross3(mColumns[1], mColumns[2]);
	dest.setAllDot3(col1xcol2, mColumns[0]);
}

LL_INLINE LLSimdScalar LLMatrix3a::getDeterminant() const
{
	LLVector4a col1xcol2; col1xcol2.setCross3(mColumns[1], mColumns[2]);
	return col1xcol2.dot3(mColumns[0]);
}

LL_INLINE bool LLMatrix3a::isApproximatelyEqual(const LLMatrix3a& rhs,
												F32 tolerance) const
{
	return rhs.getColumn(0).equals3(mColumns[0], tolerance) &&
		   rhs.getColumn(1).equals3(mColumns[1], tolerance) &&
		   rhs.getColumn(2).equals3(mColumns[2], tolerance);
}

LL_INLINE const LLMatrix3a& LLMatrix3a::getIdentity()
{
	extern const LLMatrix3a LL_M3A_IDENTITY;
	return LL_M3A_IDENTITY;
}

LL_INLINE bool LLRotation::isOkRotation() const
{
	LLMatrix3a transpose; transpose.setTranspose(*this);
	LLMatrix3a product; product.setMul(*this, transpose);

	LLSimdScalar detMinusOne = getDeterminant() - 1.f;

	return product.isApproximatelyEqual(LLMatrix3a::getIdentity()) &&
		   detMinusOne.getAbs() < F_APPROXIMATELY_ZERO;
}

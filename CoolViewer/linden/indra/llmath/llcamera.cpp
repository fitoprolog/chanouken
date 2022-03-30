/**
 * @file llcamera.cpp
 * @brief Implementation of the LLCamera class.
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

#include "llmath.h"

#include "llcamera.h"

// ---------------- Constructors and destructors ----------------

LLCamera::LLCamera()
:	LLCoordFrame(),
	mView(DEFAULT_FIELD_OF_VIEW),
	mAspect(DEFAULT_ASPECT_RATIO),
	mViewHeightInPixels(-1),			// invalid height
	mNearPlane(DEFAULT_NEAR_PLANE),
	mFarPlane(DEFAULT_FAR_PLANE),
	mFixedDistance(-1.f),
	mPlaneCount(6),
	mFrustumCornerDist(0.f)
{
	for (U32 i = 0; i < PLANE_MASK_NUM; ++i)
	{
		mPlaneMask[i] = PLANE_MASK_NONE;
	}

	calculateFrustumPlanes();
}

LLCamera::LLCamera(F32 vertical_fov_rads, F32 aspect_ratio,
				   S32 view_height_in_pixels, F32 near_plane, F32 far_plane)
:	LLCoordFrame(),
	mViewHeightInPixels(view_height_in_pixels),
	mFixedDistance(-1.f),
	mPlaneCount(6),
	mFrustumCornerDist(0.f)
{
	for (U32 i = 0; i < PLANE_MASK_NUM; ++i)
	{
		mPlaneMask[i] = PLANE_MASK_NONE;
	}

	mAspect = llclamp(aspect_ratio, MIN_ASPECT_RATIO, MAX_ASPECT_RATIO);
	mNearPlane = llclamp(near_plane, MIN_NEAR_PLANE, MAX_NEAR_PLANE);
	if (far_plane < 0) far_plane = DEFAULT_FAR_PLANE;
	mFarPlane = llclamp(far_plane, MIN_FAR_PLANE, MAX_FAR_PLANE);

	setView(vertical_fov_rads);
}

// ---------------- LLCamera::getFoo() member functions ----------------

F32 LLCamera::getMinView() const
{
	// minimum vertical fov needs to be constrained in narrow windows.
	return mAspect > 1	? MIN_FIELD_OF_VIEW // wide views
						// clamps minimum width in narrow views
						: MIN_FIELD_OF_VIEW / mAspect;
}

F32 LLCamera::getMaxView() const
{
	// maximum vertical fov needs to be constrained in wide windows.
	return mAspect > 1 ? MAX_FIELD_OF_VIEW / mAspect  // clamps maximum width in wide views
					   : MAX_FIELD_OF_VIEW; // narrow views
}

// ---------------- LLCamera::setFoo() member functions ----------------

void LLCamera::setUserClipPlane(LLPlane& plane)
{
	mPlaneCount = AGENT_PLANE_USER_CLIP_NUM;
	mAgentPlanes[AGENT_PLANE_USER_CLIP] = plane;
	mPlaneMask[AGENT_PLANE_USER_CLIP] = plane.calcPlaneMask();
}

void LLCamera::disableUserClipPlane()
{
	mPlaneCount = AGENT_PLANE_NO_USER_CLIP_NUM;
}

void LLCamera::setView(F32 vertical_fov_rads)
{
	mView = llclamp(vertical_fov_rads, MIN_FIELD_OF_VIEW, MAX_FIELD_OF_VIEW);
	calculateFrustumPlanes();
}

void LLCamera::setViewHeightInPixels(S32 height)
{
	mViewHeightInPixels = height;

	// Don't really need to do this, but update the pixel meter ratio with it.
	calculateFrustumPlanes();
}

void LLCamera::setAspect(F32 aspect_ratio)
{
	mAspect = llclamp(aspect_ratio, MIN_ASPECT_RATIO, MAX_ASPECT_RATIO);
	calculateFrustumPlanes();
}

void LLCamera::setNear(F32 near_plane)
{
	mNearPlane = llclamp(near_plane, MIN_NEAR_PLANE, MAX_NEAR_PLANE);
	calculateFrustumPlanes();
}

void LLCamera::setFar(F32 far_plane)
{
	mFarPlane = llclamp(far_plane, MIN_FAR_PLANE, MAX_FAR_PLANE);
	calculateFrustumPlanes();
}

// ---------------- read/write to buffer ----------------

size_t LLCamera::writeFrustumToBuffer(char* buffer) const
{
	memcpy(buffer, &mView, sizeof(F32));
	buffer += sizeof(F32);
	memcpy(buffer, &mAspect, sizeof(F32));
	buffer += sizeof(F32);
	memcpy(buffer, &mNearPlane, sizeof(F32));
	buffer += sizeof(F32);
	memcpy(buffer, &mFarPlane, sizeof(F32));
	return 4 * sizeof(F32);
}

size_t LLCamera::readFrustumFromBuffer(const char* buffer)
{
	memcpy(&mView, buffer, sizeof(F32));
	buffer += sizeof(F32);
	memcpy(&mAspect, buffer, sizeof(F32));
	buffer += sizeof(F32);
	memcpy(&mNearPlane, buffer, sizeof(F32));
	buffer += sizeof(F32);
	memcpy(&mFarPlane, buffer, sizeof(F32));
	return 4 * sizeof(F32);
}

// ---------------- test methods  ----------------

static const LLVector4a sFrustumScaler[] = {
	LLVector4a(-1, -1, -1),
	LLVector4a(1, -1, -1),
	LLVector4a(-1, 1, -1),
	LLVector4a(1, 1, -1),
	LLVector4a(-1, -1, 1),
	LLVector4a(1, -1, 1),
	LLVector4a(-1, 1, 1),
	LLVector4a(1, 1, 1)
};

bool LLCamera::isChanged()
{
	bool changed = false;
	for (U32 i = 0; i < mPlaneCount; ++i)
	{
		U8 mask = mPlaneMask[i];
		if (mask != 0xff && !changed)
		{
			changed = !mAgentPlanes[i].equal(mLastAgentPlanes[i]);
		}
		mLastAgentPlanes[i].set(mAgentPlanes[i]);
	}

	return changed;
}

S32 LLCamera::AABBInFrustum(const LLVector4a& center, const LLVector4a& radius,
							const LLPlane* planes)
{
	if (!planes)
	{
		// use agent space
		planes = mAgentPlanes;
	}

	U8 mask = 0;
	bool result = false;
	LLVector4a rscale, maxp, minp;
	LLSimdScalar d;
	// mAgentPlanes[] size is 7
	U32 max_planes = llmin(mPlaneCount, (U32)AGENT_PLANE_USER_CLIP_NUM);
	for (U32 i = 0; i < max_planes; ++i)
	{
		mask = mPlaneMask[i];
		if (mask < PLANE_MASK_NUM)
		{
			const LLPlane& p(planes[i]);
			p.getAt<3>(d);
			rscale.setMul(radius, sFrustumScaler[mask]);
			minp.setSub(center, rscale);
			d = -d;
			if (p.dot3(minp).getF32() > d)
			{
				return 0;
			}

			if (!result)
			{
				maxp.setAdd(center, rscale);
				result = (p.dot3(maxp).getF32() > d);
			}
		}
	}

	return result ? 1 : 2;
}

// Exactly same as the function AABBInFrustum(...), except uses mRegionPlanes
// instead of mAgentPlanes.
S32 LLCamera::AABBInRegionFrustum(const LLVector4a& center,
								  const LLVector4a& radius)
{
	return AABBInFrustum(center, radius, mRegionPlanes);
}

S32 LLCamera::AABBInFrustumNoFarClip(const LLVector4a& center,
									 const LLVector4a& radius,
									 const LLPlane* planes)
{
	if (!planes)
	{
		// use agent space
		planes = mAgentPlanes;
	}

	U8 mask = 0;
	bool result = false;
	LLVector4a rscale, maxp, minp;
	LLSimdScalar d;
	// mAgentPlanes[] size is 7
	U32 max_planes = llmin(mPlaneCount, (U32)AGENT_PLANE_USER_CLIP_NUM);
	for (U32 i = 0; i < max_planes; ++i)
	{
		mask = mPlaneMask[i];
		if (i != 5 && mask < PLANE_MASK_NUM)
		{
			const LLPlane& p(planes[i]);
			p.getAt<3>(d);
			rscale.setMul(radius, sFrustumScaler[mask]);
			minp.setSub(center, rscale);
			d = -d;
			if (p.dot3(minp).getF32() > d)
			{
				return 0;
			}

			if (!result)
			{
				maxp.setAdd(center, rscale);
				result = p.dot3(maxp).getF32() > d;
			}
		}
	}

	return result ? 1 : 2;
}

// Exactly same as the function AABBInFrustumNoFarClip(...) except uses
// mRegionPlanes instead of mAgentPlanes.
S32 LLCamera::AABBInRegionFrustumNoFarClip(const LLVector4a& center,
										   const LLVector4a& radius)
{
	return AABBInFrustumNoFarClip(center, radius, mRegionPlanes);
}

S32 LLCamera::sphereInFrustumQuick(const LLVector3& sphere_center,
								   F32 radius)
{
	LLVector3 dist = sphere_center - mFrustCenter;
	F32 dsq = dist * dist;
	F32 rsq = mFarPlane * 0.5f + radius;
	rsq *= rsq;
	return dsq < rsq ? 1 : 0;
}

// *HACK: This version is still around because the version below doesn't work
// unless the agent planes are initialized.
// Returns 1 if sphere is in frustum, 2 if fully in frustum, otherwise 0.
// NOTE: 'center' is in absolute frame.
S32 LLCamera::sphereInFrustumOld(const LLVector3& sphere_center,
								 F32 radius) const
{
	// Returns 1 if sphere is in frustum, 0 if not.
	// modified so that default view frust is along X with Z vertical
	F32 x, y, z, rightDist, leftDist, topDist, bottomDist;

	// Subtract the view position
	//LLVector3 relative_center;
	//relative_center = sphere_center - getOrigin();
	LLVector3 rel_center(sphere_center);
	rel_center -= mOrigin;

	bool all_in = true;

	// Transform relative_center.x to camera frame
	x = mXAxis * rel_center;
	if (x < MIN_NEAR_PLANE - radius)
	{
		return 0;
	}
	else if (x < MIN_NEAR_PLANE + radius)
	{
		all_in = false;
	}

	if (x > mFarPlane + radius)
	{
		return 0;
	}
	else if (x > mFarPlane - radius)
	{
		all_in = false;
	}

	// Transform relative_center.y to camera frame
	y = mYAxis * rel_center;

	// distance to plane is the dot product of (x, y, 0) * plane_normal
	rightDist = x * mLocalPlanes[PLANE_RIGHT][VX] +
				y * mLocalPlanes[PLANE_RIGHT][VY];
	if (rightDist < -radius)
	{
		return 0;
	}
	else if (rightDist < radius)
	{
		all_in = false;
	}

	leftDist = x * mLocalPlanes[PLANE_LEFT][VX] +
			   y * mLocalPlanes[PLANE_LEFT][VY];
	if (leftDist < -radius)
	{
		return 0;
	}
	else if (leftDist < radius)
	{
		all_in = false;
	}

	// Transform relative_center.y to camera frame
	z = mZAxis * rel_center;

	topDist = x * mLocalPlanes[PLANE_TOP][VX] +
			  z * mLocalPlanes[PLANE_TOP][VZ];
	if (topDist < -radius)
	{
		return 0;
	}
	else if (topDist < radius)
	{
		all_in = false;
	}

	bottomDist = x * mLocalPlanes[PLANE_BOTTOM][VX] +
				 z * mLocalPlanes[PLANE_BOTTOM][VZ];
	if (bottomDist < -radius)
	{
		return 0;
	}
	else if (bottomDist < radius)
	{
		all_in = false;
	}

	return all_in ? 2 : 1;
}

// HACK: This (presumably faster) version only currently works if you set up
// the frustum planes using GL. At some point we should get those planes
// through another mechanism, and then we can get rid of the "old" version
// above.

// Return 1 if sphere is in frustum, 2 if fully in frustum, otherwise 0.
// NOTE: 'center' is in absolute frame.
S32 LLCamera::sphereInFrustum(const LLVector3& sphere_center,
							  F32 radius) const
{
	// Returns 1 if sphere is in frustum, 0 if not.
	bool res = false;
	for (S32 i = 0; i < 6; ++i)
	{
		if (mPlaneMask[i] != PLANE_MASK_NONE)
		{
			F32 d = mAgentPlanes[i].dist(sphere_center);

			if (d > radius)
			{
				return 0;
			}
			res = res || d > -radius;
		}
	}

	return res ? 1 : 2;
}

// return height of a sphere of given radius, located at center, in pixels
F32 LLCamera::heightInPixels(const LLVector3& center, F32 radius) const
{
	if (radius == 0.f) return 0.f;

	// If height initialized
	if (mViewHeightInPixels > -1)
	{
		// Convert sphere to coord system with 0,0,0 at camera
		LLVector3 vec = center - mOrigin;

		// Compute distance to sphere
		F32 dist = vec.length();

		// Calculate angle of whole object
		F32 angle = 2.f * atan2f(radius, dist);

		// Calculate fraction of field of view
		F32 fraction_of_fov = angle / mView;

		// Compute number of pixels tall, based on vertical field of view
		return (fraction_of_fov * mViewHeightInPixels);
	}
	else
	{
		// return invalid height
		return -1.f;
	}
}

// If pos is visible, return the distance from pos to the camera. Use fudge
// distance to scale rad against top/bot/left/right planes. Otherwise, return
// -distance
F32 LLCamera::visibleDistance(const LLVector3& pos, F32 rad, F32 fudgedist,
							  U32 planemask) const
{
	if (mFixedDistance > 0)
	{
		return mFixedDistance;
	}
	LLVector3 dvec = pos - mOrigin;
	// Check visibility
	F32 dist = dvec.length();
	if (dist > rad)
	{
 		F32 dp, tdist;
 		dp = dvec * mXAxis;
  		if (dp < -rad)
		{
  			return -dist;
		}

		rad *= fudgedist;
		LLVector3 tvec(pos);
		for (S32 p = 0; p < PLANE_NUM; ++p)
		{
			if (!(planemask & (1 << p)))
			{
				continue;
			}
			tdist = -(mWorldPlanes[p].dist(tvec));
			if (tdist > rad)
			{
				return -dist;
			}
		}
	}
	return dist;
}

// Like visibleDistance, except uses mHorizPlanes[], which are left and right
//  planes perpindicular to (0,0,1) in world space
F32 LLCamera::visibleHorizDistance(const LLVector3& pos, F32 rad,
								   F32 fudgedist, U32 planemask) const
{
	if (mFixedDistance > 0)
	{
		return mFixedDistance;
	}
	LLVector3 dvec = pos - mOrigin;
	// Check visibility
	F32 dist = dvec.length();
	if (dist > rad)
	{
		rad *= fudgedist;
		LLVector3 tvec(pos);
		for (S32 p = 0; p < HORIZ_PLANE_NUM; ++p)
		{
			if (!(planemask & (1 << p)))
			{
				continue;
			}
			F32 tdist = -(mHorizPlanes[p].dist(tvec));
			if (tdist > rad)
			{
				return -dist;
			}
		}
	}
	return dist;
}

// ---------------- friends and operators ----------------

std::ostream& operator<<(std::ostream& s, const LLCamera& C)
{
	s << "{ \n";
	s << "  Center = " << C.getOrigin() << "\n";
	s << "  AtAxis = " << C.getXAxis() << "\n";
	s << "  LeftAxis = " << C.getYAxis() << "\n";
	s << "  UpAxis = " << C.getZAxis() << "\n";
	s << "  View = " << C.getView() << "\n";
	s << "  Aspect = " << C.getAspect() << "\n";
	s << "  NearPlane   = " << C.mNearPlane << "\n";
	s << "  FarPlane    = " << C.mFarPlane << "\n";
	s << "  TopPlane    = " << C.mLocalPlanes[LLCamera::PLANE_TOP][VX] << "  "
							<< C.mLocalPlanes[LLCamera::PLANE_TOP][VY] << "  "
							<< C.mLocalPlanes[LLCamera::PLANE_TOP][VZ] << "\n";
	s << "  BottomPlane = " << C.mLocalPlanes[LLCamera::PLANE_BOTTOM][VX] << "  "
							<< C.mLocalPlanes[LLCamera::PLANE_BOTTOM][VY] << "  "
							<< C.mLocalPlanes[LLCamera::PLANE_BOTTOM][VZ] << "\n";
	s << "  LeftPlane   = " << C.mLocalPlanes[LLCamera::PLANE_LEFT][VX] << "  "
							<< C.mLocalPlanes[LLCamera::PLANE_LEFT][VY] << "  "
							<< C.mLocalPlanes[LLCamera::PLANE_LEFT][VZ] << "\n";
	s << "  RightPlane  = " << C.mLocalPlanes[LLCamera::PLANE_RIGHT][VX] << "  "
							<< C.mLocalPlanes[LLCamera::PLANE_RIGHT][VY] << "  "
							<< C.mLocalPlanes[LLCamera::PLANE_RIGHT][VZ] << "\n";
	s << "}";
	return s;
}

// ----------------  private member functions ----------------

void LLCamera::calculateFrustumPlanes()
{
	// The planes only change when any of the frustum descriptions change.
	// They are not affected by changes of the position of the Frustum
	// because they are known in the view frame and the position merely
	// provides information on how to get from the absolute frame to the
	// view frame.
	F32 top = mFarPlane * (F32)tanf(0.5f * mView);
	F32 left = top * mAspect;
	calculateFrustumPlanes(left, -left, top, -top);
}

LLPlane planeFromPoints(LLVector3 p1, LLVector3 p2, LLVector3 p3)
{
	LLVector3 n = (p2 - p1) % (p3 - p1);
	n.normalize();
	return LLPlane(p1, n);
}

void LLCamera::ignoreAgentFrustumPlane(S32 idx)
{
	if (idx >= 0 && idx < (S32)mPlaneCount)
	{
		mPlaneMask[idx] = PLANE_MASK_NONE;
		mAgentPlanes[idx].clear();
	}
}

void LLCamera::calcAgentFrustumPlanes(LLVector3* frust)
{
	for (U32 i = 0; i < AGENT_FRUSTRUM_NUM; ++i)
	{
		mAgentFrustum[i] = frust[i];
	}

	mFrustumCornerDist = (frust[5] - getOrigin()).length();

	// Frust contains the 8 points of the frustum, calculate 6 planes

	// Order of planes is important, keep most likely to fail in the front of
	// the list

	// near - frust[0], frust[1], frust[2]
	mAgentPlanes[AGENT_PLANE_NEAR] = planeFromPoints(frust[0], frust[1],
													 frust[2]);

	// Far
	mAgentPlanes[AGENT_PLANE_FAR] = planeFromPoints(frust[5], frust[4],
													frust[6]);

	// Left
	mAgentPlanes[AGENT_PLANE_LEFT] = planeFromPoints(frust[4], frust[0],
													 frust[7]);

	// Right
	mAgentPlanes[AGENT_PLANE_RIGHT] = planeFromPoints(frust[1], frust[5],
													  frust[6]);

	// Top
	mAgentPlanes[AGENT_PLANE_TOP] = planeFromPoints(frust[3], frust[2],
													frust[6]);

	// Bottom
	mAgentPlanes[AGENT_PLANE_BOTTOM] = planeFromPoints(frust[1], frust[0],
													   frust[4]);

	// Cache plane octant facing mask for use in AABBInFrustum
	for (U32 i = 0; i < mPlaneCount; ++i)
	{
		mPlaneMask[i] = mAgentPlanes[i].calcPlaneMask();
	}
}

// Calculate regional planes from mAgentPlanes. Vector "shift" is the vector of
// the region origin in the agent space.
void LLCamera::calcRegionFrustumPlanes(const LLVector3& shift,
									   F32 far_clip_distance)
{
	LLVector3 p = getOrigin();
	LLVector3 n(mAgentPlanes[5][0], mAgentPlanes[5][1], mAgentPlanes[5][2]);
	F32 dd = n * p;
	F32 far_w;
	if (dd + mAgentPlanes[5][3] < 0)	// Signed distance
	{
		far_w = -far_clip_distance - dd;
	}
	else
	{
		far_w = far_clip_distance - dd;
	}
	far_w += n * shift;

	F32 d;
	for (S32 i = 0 ; i < 7; ++i)
	{
		if (mPlaneMask[i] != 0xff)
		{
			n.set(mAgentPlanes[i][0], mAgentPlanes[i][1], mAgentPlanes[i][2]);

			if (i != 5)
			{
				d = mAgentPlanes[i][3] + n * shift;
			}
			else
			{
				d = far_w;
			}
			mRegionPlanes[i].setVec(n, d);
		}
	}
}

void LLCamera::calculateFrustumPlanes(F32 left, F32 right, F32 top, F32 bottom)
{
	// For each plane we need to define 3 points (LLVector3's) in camera view
	// space. The order in which we pass the points to planeFromPoints()
	// matters, because the plane normal has a degeneracy of 2; we want it
	// pointing _into_ the frustum.

	LLVector3 a, b, c;
	b.set(mFarPlane, right, top);
	c.set(mFarPlane, right, bottom);
	mLocalPlanes[PLANE_RIGHT].setVec(a, b, c);

	c.set(mFarPlane, left, top);
	mLocalPlanes[PLANE_TOP].setVec(a, c, b);

	b.set(mFarPlane, left, bottom);
	mLocalPlanes[PLANE_LEFT].setVec(a, b, c);

	c.set(mFarPlane, right, bottom);
	mLocalPlanes[PLANE_BOTTOM].setVec(a, c, b);

	// Calculate center and radius squared of frustum in world absolute
	// coordinates
	mFrustCenter = LLVector3::x_axis * mFarPlane * 0.5f;
	mFrustCenter = transformToAbsolute(mFrustCenter);
	mFrustRadiusSquared = mFarPlane * 0.5f;
	// pad radius squared by 5%
	mFrustRadiusSquared *= mFrustRadiusSquared * 1.05f;
}

// x and y are in WINDOW space, so x = Y-Axis (left/right), y= Z-Axis(Up/Down)
void LLCamera::calculateFrustumPlanesFromWindow(F32 x1, F32 y1, F32 x2, F32 y2)
{
	F32 view_height = (F32)tanf(0.5f * mView) * mFarPlane;
	F32 view_width = view_height * mAspect;

	F32 left = x1 * -2.f * view_width;
	F32 right = x2 * -2.f * view_width;
	F32 bottom = y1 * 2.f * view_height;
	F32 top = y2 * 2.f * view_height;

	calculateFrustumPlanes(left, right, top, bottom);
}

void LLCamera::calculateWorldFrustumPlanes()
{
	LLVector3 center = mOrigin - mXAxis * mNearPlane;
	mWorldPlanePos = center;

	LLVector3 norm, pnorm;
	for (S32 p = 0; p < PLANE_NUM; ++p)
	{
		mLocalPlanes[p].getVector3(pnorm);
		norm = rotateToAbsolute(pnorm);
		norm.normalize();
		F32 d = -(center * norm);
		mWorldPlanes[p] = LLPlane(norm, d);
	}

	// Horizontal planes, perpendicular to (0,0,1);
	F32 yaw = getYaw();

	mLocalPlanes[PLANE_LEFT].getVector3(norm);
	norm.rotVec(yaw, LLVector3::z_axis);
	F32 d = -(mOrigin * norm);
	mHorizPlanes[HORIZ_PLANE_LEFT] = LLPlane(norm, d);

	mLocalPlanes[PLANE_RIGHT].getVector3(norm);
	norm.rotVec(yaw, LLVector3::z_axis);
	d = -(mOrigin * norm);
	mHorizPlanes[HORIZ_PLANE_RIGHT] = LLPlane(norm, d);
}

// NOTE: this is the OpenGL matrix that will transform the default OpenGL view
// (-Z=at, Y=up) to the default view of the LLCamera class (X=at, Z=up):
//
// F32 cfr_transform =  {  0.f,  0.f, -1.f,  0.f,   // -Z becomes X
// 						  -1.f,  0.f,  0.f,  0.f,   // -X becomes Y
//  					   0.f,  1.f,  0.f,  0.f,   //  Y becomes Z
// 						   0.f,  0.f,  0.f,  1.f };

/**
 * @file llsurfacepatch.h
 * @brief LLSurfacePatch class definition
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

#ifndef LL_LLSURFACEPATCH_H
#define LL_LLSURFACEPATCH_H

#include "llpointer.h"
#include "llvector3.h"
#include "llvector3d.h"

#include "llagent.h"
#include "llvosurfacepatch.h"

class LLColor4U;
class LLSurface;
class LLVector2;
class LLViewerRegion;

// A patch shouldn't know about its visibility since that really depends on the
// camera that is looking (or not looking) at it.  So, anything about a patch
// that is specific to a camera should be in the class below.
class LLPatchVisibilityInfo
{
public:
	LLPatchVisibilityInfo()
	:	mIsVisible(false),
		mDistance(0.f),
		mRenderLevel(0),
		mRenderStride(0)
	{
	}

	~LLPatchVisibilityInfo()							{}

	bool	mIsVisible;
	F32		mDistance;			// Distance from camera
	S32		mRenderLevel;
	U32		mRenderStride;
};

class LLSurfacePatch
{
	friend class LLSurface;

protected:
	LOG_CLASS(LLSurfacePatch);

public:
	LLSurfacePatch();
	~LLSurfacePatch();

	void connectNeighbor(LLSurfacePatch* neighborp, U32 direction);
	void disconnectNeighbor(LLSurface* surfacep);

	void setNeighborPatch(U32 direction, LLSurfacePatch* neighborp);

	LL_INLINE LLSurfacePatch* getNeighborPatch(U32 direction) const
	{
		return mNeighborPatches[direction];
	}

	bool updateTexture();

	void updateVerticalStats();
	void updateNormals();

	void updateEastEdge();
	void updateNorthEdge();

	void updateCameraDistanceRegion(const LLVector3& pos_region);
	void updateVisibility();
	void updateGL();

	void dirtyZ(); // Dirty the z values of this patch
	LL_INLINE void setHasReceivedData()					{ mHasReceivedData = true; }
	LL_INLINE bool getHasReceivedData() const			{ return mHasReceivedData; }

	F32 getDistance() const;
	LL_INLINE F32 getMaxZ() const						{ return mMaxZ; }
	LL_INLINE F32 getMinZ() const						{ return mMinZ; }
	LL_INLINE F32 getMeanComposition() const			{ return mMeanComposition; }
	LL_INLINE F32 getMinComposition() const				{ return mMinComposition; }
	LL_INLINE F32 getMaxComposition() const				{ return mMaxComposition; }
	LL_INLINE const LLVector3& getCenterRegion() const	{ return mCenterRegion; }
	LL_INLINE const U64& getLastUpdateTime() const		{ return mLastUpdateTime; }
	LL_INLINE LLSurface* getSurface() const				{ return mSurfacep; }

	// get the point at the offset.
	LLVector3 getPointAgent(U32 x,  U32 y) const;

	LLVector2 getTexCoords(U32 x, U32 y) const;

	void calcNormal(U32 x, U32 y, U32 stride);
	const LLVector3& getNormal(U32 x, U32 y) const;

	void eval(U32 x, U32 y, U32 stride, LLVector3* vertex, LLVector3* normal,
			  LLVector2* tex0, LLVector2* tex1);

	LL_INLINE LLVector3 getOriginAgent() const			{ return gAgent.getPosAgentFromGlobal(mOriginGlobal); }
	LL_INLINE const LLVector3d& getOriginGlobal() const	{ return mOriginGlobal; }
	void setOriginGlobal(const LLVector3d& origin_global);

	// connectivity -- each LLPatch points at 5 neighbors (or NULL)
	// +---+---+---+
	// |   | 2 | 5 |
	// +---+---+---+
	// | 3 | 0 | 1 |
	// +---+---+---+
	// | 6 | 4 |   |
	// +---+---+---+

	LL_INLINE bool getVisible() const					{ return mVisInfo.mIsVisible; }
	LL_INLINE U32 getRenderStride() const				{ return mVisInfo.mRenderStride; }
	LL_INLINE S32 getRenderLevel() const				{ return mVisInfo.mRenderLevel; }

	void setSurface(LLSurface* surfacep);
	LL_INLINE void setDataZ(F32* data_z)				{ mDataZ = data_z; }
	LL_INLINE void setDataNorm(LLVector3* data_norm)	{ mDataNorm = data_norm; }
	LL_INLINE F32* getDataZ() const						{ return mDataZ; }

	void dirty();	// Mark this surface patch as dirty...
	LL_INLINE void clearDirty()							{ mDirty = false; }

	LL_INLINE void clearVObj()							{ mVObjp = NULL; }

private:
	void updateCompositionStats(LLViewerRegion* regionp);

protected:
	bool mHasReceivedData;	// has the patch EVER received height data?
	bool mSTexUpdate;		// Does the surface texture need to be updated?

	LLSurfacePatch* mNeighborPatches[8]; // Adjacent patches
	bool mNormalsInvalid[9];  // Which normals are invalid

	bool mDirty;
	bool mDirtyZStats;
	bool mHeightsGenerated;

	U32 mDataOffset;
	F32* mDataZ;
	LLVector3* mDataNorm;

	// Pointer to the LLVOSurfacePatch object which is used in the new renderer.
	LLPointer<LLVOSurfacePatch> mVObjp;

	// All of the camera-dependent stuff should be in its own class...
	LLPatchVisibilityInfo mVisInfo;

	// Pointers to beginnings of patch data fields
	LLVector3d mOriginGlobal;
	LLVector3 mOriginRegion;

	// Height field stats
	LLVector3 mCenterRegion; // Center in region-local coords
	F32 mMinZ, mMaxZ, mMeanZ;
	F32 mRadius;

	F32 mMinComposition;
	F32 mMaxComposition;
	F32 mMeanComposition;

	U8 mConnectedEdge;		// This flag is non-zero iff patch is on at least one edge
							// of LLSurface that is "connected" to another LLSurface
	U64 mLastUpdateTime;	// Time patch was last updated

	LLSurface* mSurfacep; // Pointer to "parent" surface
};

#endif // LL_LLSURFACEPATCH_H

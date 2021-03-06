/**
 * @file llflexibleobject.h
 * @author JJ Ventrella, Andrew Meadows, Tom Yedwab
 * @brief Flexible object definition
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

/**
 * This is for specifying objects in the world that are animated and
 * rendered locally - on the viewer. Flexible Objects are linear arrays
 * of positions, which stay at a fixed distance from each other. One
 * position is fixed as an "anchor" and is attached to some other object
 * in the world, determined by the server. All the other positions are
 * updated according to local physics.
 */

#ifndef LL_LLFLEXIBLEOBJECT_H
#define LL_LLFLEXIBLEOBJECT_H

#include "llprimitive.h"
#include "llvovolume.h"
#include "llwind.h"

// 10 ms for the whole thing !
constexpr F32 FLEXIBLE_OBJECT_TIMESLICE	= 0.003f;
constexpr U32 FLEXIBLE_OBJECT_MAX_LOD	= 10;

// See llprimitive.h for LLFlexibleObjectData and DEFAULT/MIN/MAX values

struct LLFlexibleObjectSection
{
	// Input parameters
	LLVector2		mScale;
	LLQuaternion	mAxisRotation;
	// Simulated state
	LLVector3		mPosition;
	LLVector3		mVelocity;
	LLVector3		mDirection;
	LLQuaternion	mRotation;
	// Derivatives (Not all currently used, will come back with LLVolume
	// changes to automagically generate normals)
	LLVector3		mdPosition;
	//LLMatrix4		mRotScale;
	//LLMatrix4		mdRotScale;
};

class LLVolumeImplFlexible final : public LLVolumeInterface
{
protected:
	LOG_CLASS(LLVolumeImplFlexible);

public:
	static void initClass();
	static void updateClass();
	static void dumpStats();

	LLVolumeImplFlexible(LLViewerObject* volume,
						 LLFlexibleObjectData* attributes);

	~LLVolumeImplFlexible() override;

	// LLVolumeInterface overrides

	LL_INLINE LLVolumeInterfaceType getInterfaceType() const override
	{
		return INTERFACE_FLEXIBLE;
	}

	void doIdleUpdate() override;

	bool doUpdateGeometry(LLDrawable* drawable) override;

	LLVector3 getPivotPosition() const override;

	LL_INLINE void onSetVolume(const LLVolumeParams&, S32) override
	{
	}

	void onSetScale(const LLVector3& scale, bool damped) override;

	void onParameterChanged(U16 param_type, LLNetworkData* data, bool in_use,
							bool local_origin) override;

	void onShift(const LLVector4a& shift_vector) override;

	LL_INLINE bool isVolumeUnique() const override			{ return true; }
	LL_INLINE bool isVolumeGlobal() const override			{ return true; }
	LL_INLINE bool isActive() const override				{ return true; }

	const LLMatrix4& getWorldMatrix(LLXformMatrix* xform) const override;

	void updateRelativeXform(bool force_identity = false) override;

	LL_INLINE U32 getID() const override					{ return mID; }

	void preRebuild() override;

	LLVector3 getFramePosition() const;
	LLQuaternion getFrameRotation() const;

	// New methods

	void updateRenderRes();
	void doFlexibleUpdate(); // Called to update the simulation

	// Called to rebuild the geometry:
	void doFlexibleRebuild(bool rebuild_volume);

	void setParentPositionAndRotationDirectly(LLVector3 p, LLQuaternion r);

	LL_INLINE void setCollisionSphere(LLVector3 position, F32 radius)
	{
		mCollisionSpherePosition = position;
		mCollisionSphereRadius = radius;
	}

	LLVector3 getEndPosition();
	LLQuaternion getEndRotation();
	LLVector3 getNodePosition(S32 node_idx);
	LLVector3 getAnchorPosition() const;

private:
	void setAttributesOfAllSections	(LLVector3* in_scale = NULL);

	void remapSections(LLFlexibleObjectSection* source, S32 source_sections,
					   LLFlexibleObjectSection* dest, S32 dest_sections);

public:
	// Global setting for update rate
	static F32					sUpdateFactor;

private:
    // Backlink only; do not make this an LLPointer.
	LLViewerObject*				mVO;

	LLTimer						mTimer;
	LLVector3					mAnchorPosition;
	LLVector3					mParentPosition;
	LLQuaternion				mParentRotation;
	LLQuaternion				mLastFrameRotation;
	LLQuaternion				mLastSegmentRotation;
	LLFlexibleObjectData*		mAttributes;
	LLFlexibleObjectSection		mSection[(1 << FLEXIBLE_OBJECT_MAX_SECTIONS) + 1];
	S32							mInitializedRes;
	S32							mSimulateRes;
	S32							mRenderRes;
	U64							mLastFrameNum;
	U32							mLastUpdatePeriod;
	LLVector3					mCollisionSpherePosition;
	F32							mCollisionSphereRadius;
	U32							mID;
	bool						mInitialized;
	bool						mUpdated;

	S32							mInstanceIndex;

	typedef std::vector<LLVolumeImplFlexible*> instances_list_t;
	static instances_list_t		sInstanceList;
};

#endif // LL_LLFLEXIBLEOBJECT_H

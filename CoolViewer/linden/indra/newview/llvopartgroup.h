/**
 * @file llvopartgroup.h
 * @brief Group of particle systems
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

#ifndef LL_LLVOPARTGROUP_H
#define LL_LLVOPARTGROUP_H

#include "llframetimer.h"
#include "llvertexbuffer.h"
#include "llcolor3.h"
#include "llvector3.h"

#include "llviewerobject.h"
#include "llviewerpartsim.h"
#include "llviewerregion.h"

// This is just below the maximum size for our vertex buffer (which would be
// 65536 / 6). Note that this is *not* RenderMaxPartCount; we keep a larger
// number than the maximum of RenderMaxPartCount (8192) so that spammy particle
// systems do not trust the vertex buffer and that enough room is (hopefully)
// left for low count/low frequency particle systems when they start emitting
// once every few seconds or minutes...
// *TODO: force the spammy particle systems to free the vertex buffer when full
#define LL_MAX_PARTICLE_COUNT 10920

class LLViewerPartGroup;

class LLVOPartGroup : public LLAlphaObject
{
public:
	static void initClass();
	static void restoreGL();
	static void destroyGL();
	static S32 findAvailableVBSlot();
	static void freeVBSlot(S32 idx);

	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_COLOR |
							LLVertexBuffer::MAP_EMISSIVE |
							LLVertexBuffer::MAP_TEXTURE_INDEX
	};

	LLVOPartGroup(const LLUUID& id, LLPCode pcode, LLViewerRegion* regionp);

	LL_INLINE LLVOPartGroup* asVOPartGroup() override	{ return this; }

	// Whether this object needs to do an idleUpdate:
	LL_INLINE bool isActive() const override			{ return false; }

	LL_INLINE void idleUpdate(const F64&) override		{}

	F32 getBinRadius() override;
	void updateSpatialExtents(LLVector4a& min, LLVector4a& max) override;

	LL_INLINE U32 getPartitionType() const override
	{
		return LLViewerRegion::PARTITION_PARTICLE;
	}

	bool lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end,
							  S32 face, bool pick_transparent,
							  bool pick_rigged, S32* face_hit,
							  LLVector4a* intersection,
							  // Unused pointers:
							  LLVector2* tex_coord = NULL,
							  LLVector4a* normal = NULL,
							  LLVector4a* tangent = NULL) override;

	void setPixelAreaAndAngle() override;

	LL_INLINE void updateTextures() override			{}
	LL_INLINE void updateFaceSize(S32) override			{}

	LLDrawable* createDrawable() override;
	bool updateGeometry(LLDrawable* drawable) override;
	void getGeometry(const LLViewerPart& part,
					 LLStrider<LLVector4a>& verticesp);
	void getGeometry(S32 idx,
					 LLStrider<LLVector4a>& verticesp,
					 LLStrider<LLVector3>& normalsp,
					 LLStrider<LLVector2>& texcoordsp,
					 LLStrider<LLColor4U>& colorsp,
					 LLStrider<LLColor4U>& emissivep,
					 LLStrider<U16>& indicesp) override;

	F32 getPartSize(S32 idx) override;
	bool getBlendFunc(S32 idx, U32& src, U32& dst) override;
	const LLUUID& getPartOwner(S32 idx);
	const LLUUID& getPartSource(S32 idx);

	LL_INLINE void setViewerPartGroup(LLViewerPartGroup* group)
	{
		mViewerPartGroupp = group;
	}

	LL_INLINE LLViewerPartGroup* getViewerPartGroup()	{ return mViewerPartGroupp; }

protected:
	~LLVOPartGroup() override = default;
	virtual const LLVector3& getCameraPosition() const;

public:
	// Vertex buffer for holding all particles
	static LLPointer<LLVertexBuffer> sVB;

private:
	static S32* sVBSlotCursor;
	static S32 sVBSlotFree[LL_MAX_PARTICLE_COUNT];

protected:
	LLViewerPartGroup* mViewerPartGroupp;
};

class LLVOHUDPartGroup final : public LLVOPartGroup
{
public:
	LLVOHUDPartGroup(const LLUUID& id, LLPCode pcode, LLViewerRegion* regionp)
	:	LLVOPartGroup(id, pcode, regionp)
	{
	}

protected:
	LLDrawable* createDrawable() override;

	LL_INLINE U32 getPartitionType() const override
	{
		return LLViewerRegion::PARTITION_HUD_PARTICLE;
	}

	LL_INLINE const LLVector3& getCameraPosition() const override
	{
		return LLVector3::x_axis_neg;
	}
};

#endif // LL_LLVOPARTGROUP_H

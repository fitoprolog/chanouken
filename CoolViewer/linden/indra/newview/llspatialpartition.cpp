/**
 * @file llspatialpartition.cpp
 * @brief LLSpatialGroup class implementation and supporting functions
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "llspatialpartition.h"

#include "llfasttimer.h"
#include "llglslshader.h"
#include "lloctree.h"
#include "llphysicsshapebuilderutil.h"
#include "llrender.h"
#include "llvolume.h"
#include "llvolumeoctree.h"

#include "llappviewer.h"
#include "llface.h"
#include "llfloatertools.h"
#include "llmeshrepository.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewerwindow.h"			// For gDebugRaycast*
#include "llvoavatar.h"
#include "llvolumemgr.h"
#include "llvovolume.h"

U32 LLSpatialGroup::sNodeCount = 0;
bool LLSpatialGroup::sNoDelete = false;

U32 gOctreeMaxCapacity;
F32 gOctreeMinSize;
// Must be adjusted upwards for OpenSim grids to avoid the dreaded
// "Element exceeds range of spatial partition" issue on TPs and its
// consequences (crashes or massive occlusion issues).
LLVector4a gOctreeMaxMag(1024.f * 1024.f);

spatial_groups_set_t gVisibleSelectedGroups;

static F32 sLastMaxTexPriority = 1.f;
static F32 sCurMaxTexPriority = 1.f;

bool LLSpatialPartition::sTeleportRequested = false;

//static counter for frame to switch LOD on

void sg_assert(bool expr)
{
#if LL_OCTREE_PARANOIA_CHECK
	if (!expr)
	{
		llerrs << "Octree invalid !" << llendl;
	}
#endif
}

// Returns:
//	0 if sphere and AABB are not intersecting
//	1 if they are
//	2 if AABB is entirely inside sphere
S32 LLSphereAABB(const LLVector3& center, const LLVector3& size,
				 const LLVector3& pos, const F32& rad)
{
	S32 ret = 2;

	LLVector3 min = center - size;
	LLVector3 max = center + size;
	for (U32 i = 0; i < 3; ++i)
	{
		if (min.mV[i] > pos.mV[i] + rad || max.mV[i] < pos.mV[i] - rad)
		{
			// Totally outside
			return 0;
		}

		if (min.mV[i] < pos.mV[i] - rad || max.mV[i] > pos.mV[i] + rad)
		{
			// Intersecting
			ret = 1;
		}
	}

	return ret;
}

LLSpatialGroup::~LLSpatialGroup()
{
#if LL_DEBUG
	if (gDebugGL)
	{
# if 0	// Note that this might actually "normally" happen...
		if (sNoDelete)
		{
			llerrs << "Deleted while in sNoDelete mode !" << llendl;
		}
# endif
		gPipeline.checkReferences(this);
	}
#endif

	--sNodeCount;

	clearDrawMap();
}

void LLSpatialGroup::clearDrawMap()
{
	mDrawMap.clear();
}

bool LLSpatialGroup::isHUDGroup()
{
	LLSpatialPartition* partition = getSpatialPartition();
	return partition && partition->isHUDPartition();
}

void LLSpatialGroup::validate()
{
	ll_assert_aligned(this, 64);

#if LL_OCTREE_PARANOIA_CHECK
	sg_assert(!isState(DIRTY));
	sg_assert(!isDead());

	LLVector4a myMin;
	myMin.setSub(mBounds[0], mBounds[1]);
	LLVector4a myMax;
	myMax.setAdd(mBounds[0], mBounds[1]);

	validateDrawMap();

	for (element_iter i = getDataBegin(); i != getDataEnd(); ++i)
	{
		LLDrawable* drawable = *i;
		sg_assert(drawable->getSpatialGroup() == this);
		if (drawable->getSpatialBridge())
		{
			sg_assert(drawable->getSpatialBridge() ==
					  getSpatialPartition()->asBridge());
		}
#	if 0
		if (drawable->isSpatialBridge())
		{
			LLSpatialPartition* part = drawable->asPartition();
			if (!part)
			{
				llerrs << "Drawable reports it is a spatial bridge but not a partition."
					   << llendl;
			}
			LLSpatialGroup* group =
				(LLSpatialGroup*)part->mOctree->getListener(0);
			if (group)
			{
				group->validate();
			}
		}
#	endif
	}

	for (U32 i = 0; i < mOctreeNode->getChildCount(); ++i)
	{
		LLSpatialGroup* group =
			(LLSpatialGroup*)mOctreeNode->getChild(i)->getListener(0);
		if (!group) continue;

		group->validate();

		// Ensure all children are enclosed in this node
		LLVector4a center = group->mBounds[0];
		LLVector4a size = group->mBounds[1];

		LLVector4a min;
		min.setSub(center, size);
		LLVector4a max;
		max.setAdd(center, size);

		for (U32 j = 0; j < 3; ++j)
		{
			sg_assert(min[j] >= myMin[j] - 0.02f);
			sg_assert(max[j] <= myMax[j] + 0.02f);
		}
	}
#endif
}

void LLSpatialGroup::validateDrawMap()
{
#if LL_OCTREE_PARANOIA_CHECK
	for (draw_map_t::iterator i = mDrawMap.begin(); i != mDrawMap.end(); ++i)
	{
		LLSpatialGroup::drawmap_elem_t& draw_vec = i->second;
		for (drawmap_elem_t::iterator j = draw_vec.begin();
			 j != draw_vec.end(); ++j)
		{
			LLDrawInfo& params = **j;
			params.validate();
		}
	}
#endif
}

bool LLSpatialGroup::updateInGroup(LLDrawable* drawablep, bool immediate)
{
	if (!drawablep)
	{
		llwarns << "NULL drawable !" << llendl;
		return false;
	}

	drawablep->updateSpatialExtents();

	OctreeNode* parent = mOctreeNode->getOctParent();

	if (mOctreeNode->isInside(drawablep->getPositionGroup()) &&
		(mOctreeNode->contains(drawablep->getEntry()) ||
		 (drawablep->getBinRadius() > mOctreeNode->getSize()[0] &&
		  parent && parent->getElementCount() >= gOctreeMaxCapacity)))
	{
		unbound();
		setState(OBJECT_DIRTY);
#if 0
		setState(GEOM_DIRTY);
#endif
		return true;
	}

	return false;
}

bool LLSpatialGroup::addObject(LLDrawable* drawablep)
{
	if (!drawablep)
	{
		return false;
	}
	else
	{
		drawablep->setGroup(this);
		setState(OBJECT_DIRTY | GEOM_DIRTY);
		setOcclusionState(DISCARD_QUERY, STATE_MODE_ALL_CAMERAS);
		gPipeline.markRebuild(this, true);
		if (drawablep->isSpatialBridge())
		{
			mBridgeList.push_back((LLSpatialBridge*)drawablep);
		}
		if (drawablep->getRadius() > 1.f)
		{
			setState(IMAGE_DIRTY);
		}
	}

	return true;
}

void LLSpatialGroup::rebuildGeom()
{
	if (!isDead())
	{
		getSpatialPartition()->rebuildGeom(this);

		if (hasState(MESH_DIRTY))
		{
			gPipeline.markMeshDirty(this);
		}
	}
}

void LLSpatialGroup::rebuildMesh()
{
	if (!isDead())
	{
		getSpatialPartition()->rebuildMesh(this);
	}
}

void LLSpatialPartition::rebuildGeom(LLSpatialGroup* group)
{
	if (group->isDead() || !group->hasState(LLSpatialGroup::GEOM_DIRTY))
	{
		return;
	}

	if (group->changeLOD())
	{
		group->mLastUpdateDistance = group->mDistance;
		group->mLastUpdateViewAngle = group->mViewAngle;
	}

	LL_FAST_TIMER(FTM_REBUILD_VBO);

	group->clearDrawMap();

	// Get geometry count
	U32 index_count = 0;
	U32 vertex_count = 0;

	{
		LL_FAST_TIMER(FTM_ADD_GEOMETRY_COUNT);
		addGeometryCount(group, vertex_count, index_count);
	}

	if (vertex_count > 0 && index_count > 0)
	{
		// Create vertex buffer containing volume geometry for this node
		{
			LL_FAST_TIMER(FTM_CREATE_VB);
			group->mBuilt = 1.f;
			if (group->mVertexBuffer.isNull() ||
				!group->mVertexBuffer->isWriteable() ||
				(group->mBufferUsage != (U32)group->mVertexBuffer->getUsage() &&
				 LLVertexBuffer::sEnableVBOs))
			{
				group->mVertexBuffer = createVertexBuffer(mVertexDataMask,
														  group->mBufferUsage);
				if (group->mVertexBuffer.isNull() ||
					!group->mVertexBuffer->allocateBuffer(vertex_count,
														  index_count,
														  true))
				{
					llwarns << "Failure to allocate a vertex buffer with "
							<< vertex_count << " vertices and "
							<< index_count << " indices" << llendl;
					group->mVertexBuffer = NULL;
					group->mBufferMap.clear();
					group->mLastUpdateTime = gFrameTimeSeconds;
					group->clearState(LLSpatialGroup::GEOM_DIRTY);
					return;
				}
			}
			else if (!group->mVertexBuffer->resizeBuffer(vertex_count,
														 index_count))
			{
				llwarns << "Failure to resize a vertex buffer with "
						<< vertex_count << " vertices and "
						<< index_count << " indices" << llendl;
				group->mVertexBuffer = NULL;
				group->mBufferMap.clear();
				group->mLastUpdateTime = gFrameTimeSeconds;
				group->clearState(LLSpatialGroup::GEOM_DIRTY);
				return;

			}
		}

		{
			LL_FAST_TIMER(FTM_GET_GEOMETRY);
			getGeometry(group);
		}
	}
	else
	{
		group->mVertexBuffer = NULL;
		group->mBufferMap.clear();
	}

	group->mLastUpdateTime = gFrameTimeSeconds;
	group->clearState(LLSpatialGroup::GEOM_DIRTY);
}

void LLSpatialPartition::rebuildMesh(LLSpatialGroup* group)
{
}

LLSpatialGroup* LLSpatialGroup::getParent()
{
	return (LLSpatialGroup*)LLViewerOctreeGroup::getParent();
}

bool LLSpatialGroup::removeObject(LLDrawable* drawablep, bool from_octree)
{
	if (!drawablep)
	{
		return false;
	}

	unbound();
	if (mOctreeNode && !from_octree)
	{
		drawablep->setGroup(NULL);
	}
	else
	{
		drawablep->setGroup(NULL);
		setState(GEOM_DIRTY);
		gPipeline.markRebuild(this, true);

		if (drawablep->isSpatialBridge())
		{
			for (bridge_list_t::iterator i = mBridgeList.begin(),
										 end = mBridgeList.end();
				 i != end; ++i)
			{
				if (*i == drawablep)
				{
					mBridgeList.erase(i);
					break;
				}
			}
		}

		if (getElementCount() == 0)
		{
			// Delete draw map on last element removal since a rebuild might
			// never happen
			clearDrawMap();
		}
	}

	return true;
}

void LLSpatialGroup::shift(const LLVector4a& offset)
{
	LLVector4a t = mOctreeNode->getCenter();
	t.add(offset);
	mOctreeNode->setCenter(t);
	mOctreeNode->updateMinMax();
	mBounds[0].add(offset);
	mExtents[0].add(offset);
	mExtents[1].add(offset);
	mObjectBounds[0].add(offset);
	mObjectExtents[0].add(offset);
	mObjectExtents[1].add(offset);

	LLSpatialPartition* partition = getSpatialPartition();
	if (!partition)
	{
		llwarns_once << "NULL octree partition !" << llendl;
		llassert(false);
		return;
	}

	U32 type = partition->mPartitionType;
	if (!partition->mRenderByGroup &&
		type != LLViewerRegion::PARTITION_TREE &&
		type != LLViewerRegion::PARTITION_TERRAIN &&
		type != LLViewerRegion::PARTITION_AVATAR &&
		type != LLViewerRegion::PARTITION_PUPPET &&
		type != LLViewerRegion::PARTITION_BRIDGE)
	{
		setState(GEOM_DIRTY);
		gPipeline.markRebuild(this, true);
	}
}

class LLSpatialSetState : public OctreeTraveler
{
public:
	LLSpatialSetState(U32 state)
	:	mState(state)
	{
	}

	void visit(const OctreeNode* branch) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)branch->getListener(0);
		if (group)
		{
			group->setState(mState);
		}
	}

public:
	U32 mState;
};

class LLSpatialSetStateDiff final : public LLSpatialSetState
{
public:
	LLSpatialSetStateDiff(U32 state)
	:	LLSpatialSetState(state)
	{
	}

	void traverse(const OctreeNode* n) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)n->getListener(0);
		if (group && !group->hasState(mState))
		{
			OctreeTraveler::traverse(n);
		}
	}
};

void LLSpatialGroup::setState(U32 state, S32 mode)
{
	llassert(state <= LLSpatialGroup::STATE_MASK);

	if (mode > STATE_MODE_SINGLE)
	{
		if (mode == STATE_MODE_DIFF)
		{
			LLSpatialSetStateDiff setter(state);
			setter.traverse(mOctreeNode);
		}
		else
		{
			LLSpatialSetState setter(state);
			setter.traverse(mOctreeNode);
		}
	}
	else
	{
		mState |= state;
	}
}

class LLSpatialClearState : public OctreeTraveler
{
public:
	LLSpatialClearState(U32 state)
	:	mState(state)
	{
	}

	void visit(const OctreeNode* branch) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)branch->getListener(0);
		if (group)
		{
			group->clearState(mState);
		}
	}

public:
	U32 mState;
};

class LLSpatialClearStateDiff final : public LLSpatialClearState
{
public:
	LLSpatialClearStateDiff(U32 state)
	:	LLSpatialClearState(state)
	{
	}

	void traverse(const OctreeNode* n) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)n->getListener(0);
		if (group && group->hasState(mState))
		{
			OctreeTraveler::traverse(n);
		}
	}
};

void LLSpatialGroup::clearState(U32 state, S32 mode)
{
	llassert(state <= LLSpatialGroup::STATE_MASK);

	if (mode > STATE_MODE_SINGLE)
	{
		if (mode == STATE_MODE_DIFF)
		{
			LLSpatialClearStateDiff clearer(state);
			clearer.traverse(mOctreeNode);
		}
		else
		{
			LLSpatialClearState clearer(state);
			clearer.traverse(mOctreeNode);
		}
	}
	else
	{
		mState &= ~state;
	}
}

//======================================
//		Octree Listener Implementation
//======================================

LLSpatialGroup::LLSpatialGroup(OctreeNode* node, LLSpatialPartition* part)
:	LLOcclusionCullingGroup(node, part),
	mObjectBoxSize(1.f),
	mGeometryBytes(0),
	mSurfaceArea(0.f),
	mBuilt(0.f),
	mVertexBuffer(NULL),
	mBufferUsage(part->mBufferUsage),
	mDistance(0.f),
	mDepth(0.f),
	mLastUpdateDistance(-1.f),
	mLastUpdateTime(gFrameTimeSeconds)
{
	ll_assert_aligned(this, 16);

	++sNodeCount;

	mViewAngle.splat(0.f);
	mLastUpdateViewAngle.splat(-1.f);

	sg_assert(mOctreeNode->getListenerCount() == 0);
	setState(SG_INITIAL_STATE_MASK);
	gPipeline.markRebuild(this, true);

	mRadius = 1;
	mPixelArea = 1024.f;
}

void LLSpatialGroup::updateDistance(LLCamera &camera)
{
	if (LLViewerCamera::sCurCameraID != LLViewerCamera::CAMERA_WORLD)
	{
		llwarns << "Attempted to update distance for camera other than world camera !"
				<< llendl;
		return;
	}

	if (gShiftFrame)
	{
		return;
	}

#if LL_DEBUG
	if (hasState(OBJECT_DIRTY))
	{
		llerrs << "Spatial group dirty on distance update." << llendl;
	}
#endif

	if (!isEmpty())
	{
		LLSpatialPartition* partition = getSpatialPartition();
		if (!partition)
		{
			llwarns_once << "NULL octree partition !" << llendl;
			llassert(false);
			return;
		}
		mRadius = partition->mRenderByGroup ? mObjectBounds[1].getLength3().getF32()
											: mOctreeNode->getSize().getLength3().getF32();
		mDistance = partition->calcDistance(this, camera);
		mPixelArea = partition->calcPixelArea(this, camera);
	}
}

F32 LLSpatialPartition::calcDistance(LLSpatialGroup* group, LLCamera& camera)
{
	LLVector4a eye;
	LLVector4a origin;
	origin.load3(camera.getOrigin().mV);
	eye.setSub(group->mObjectBounds[0], origin);

	F32 dist = 0.f;

	if (group->mDrawMap.find(LLRenderPass::PASS_ALPHA) != group->mDrawMap.end())
	{
		LLVector4a v = eye;
		dist = eye.getLength3().getF32();
		eye.normalize3fast();

		if (!group->hasState(LLSpatialGroup::ALPHA_DIRTY))
		{
			if (!group->getSpatialPartition()->isBridge())
			{
				LLVector4a view_angle = eye;

				LLVector4a diff;
				diff.setSub(view_angle, group->mLastUpdateViewAngle);

				if (diff.getLength3().getF32() > 0.64f)
				{
					group->mViewAngle = view_angle;
					group->mLastUpdateViewAngle = view_angle;
					// For occasional alpha sorting within the group.
					// NOTE: If there is a trivial way to detect that alpha
					// sorting here would not change the render order, not
					// setting this node to dirty would be a very good thing.
					group->setState(LLSpatialGroup::ALPHA_DIRTY);
					gPipeline.markRebuild(group, false);
				}
			}
		}

		// calculate depth of node for alpha sorting

		LLVector3 at = camera.getAtAxis();

		LLVector4a ata;
		ata.load3(at.mV);

		LLVector4a t = ata;
		// front of bounding box
		t.mul(0.25f);
		t.mul(group->mObjectBounds[1]);
		v.sub(t);

		group->mDepth = v.dot3(ata).getF32();
	}
	else
	{
		dist = eye.getLength3().getF32();
	}

	if (dist < 16.f)
	{
		dist /= 16.f;
		dist *= dist;
		dist *= 16.f;
	}

	return dist;
}

F32 LLSpatialPartition::calcPixelArea(LLSpatialGroup* group, LLCamera& camera)
{
	return LLPipeline::calcPixelArea(group->mObjectBounds[0],
									 group->mObjectBounds[1], camera);
}

F32 LLSpatialGroup::getUpdateUrgency() const
{
	if (!isVisible())
	{
		return 0.f;
	}
	else if (mDistance <= 0.f)
	{
		return F32_MAX;
	}
	else
	{
		F32 time = gFrameTimeSeconds - mLastUpdateTime + 4.f;
		return time +
			   (mObjectBounds[1].dot3(mObjectBounds[1]).getF32() + 1.f) /
			   mDistance;
	}
}

bool LLSpatialGroup::changeLOD()
{
	if (hasState(ALPHA_DIRTY | OBJECT_DIRTY))
	{
		// A rebuild is going to happen, update distance and LoD
		return true;
	}

	if (getSpatialPartition()->mSlopRatio > 0.f)
	{
		F32 ratio = (mDistance - mLastUpdateDistance) /
					 llmax(mLastUpdateDistance, mRadius);

		if (fabsf(ratio) >= getSpatialPartition()->mSlopRatio)
		{
			return true;
		}

		if (mDistance > mRadius * 2.f)
		{
			return false;
		}
	}

	return needsUpdate();
}

void LLSpatialGroup::handleInsertion(const TreeNode* node,
									 LLViewerOctreeEntry* entry)
{
	if (entry)
	{
		addObject((LLDrawable*)entry->getDrawable());
		unbound();
		setState(OBJECT_DIRTY);
	}
	else
	{
		llwarns << "Tried to insert a NULL drawable in node " << node
				<< llendl;
		llassert(false);
	}
}

void LLSpatialGroup::handleRemoval(const TreeNode* node,
								   LLViewerOctreeEntry* entry)
{
	if (entry)
	{
		removeObject((LLDrawable*)entry->getDrawable(), true);
		LLViewerOctreeGroup::handleRemoval(node, entry);
	}
	else
	{
		llwarns << "Tried to remove a NULL drawable from node " << node
				<< llendl;
		llassert(false);
	}
}

void LLSpatialGroup::handleDestruction(const TreeNode* node)
{
	if (isDead())
	{
		return;
	}
	setState(DEAD);

	for (element_iter i = getDataBegin(); i != getDataEnd(); ++i)
	{
		LLViewerOctreeEntry* entry = *i;
		if (entry && entry->getGroup() == this &&
			entry->hasDrawable())
		{
			((LLDrawable*)entry->getDrawable())->setGroup(NULL);
		}
	}

	// Clean up avatar attachment stats
	LLSpatialBridge* bridge = getSpatialPartition()->asBridge();
	if (bridge && bridge->mAvatar.notNull())
	{
		bridge->mAvatar->subtractAttachmentBytes(mGeometryBytes);
		bridge->mAvatar->subtractAttachmentArea(mSurfaceArea);
	}

	clearDrawMap();
	mVertexBuffer = NULL;
	mBufferMap.clear();
	mOctreeNode = NULL;
}

void LLSpatialGroup::handleChildAddition(const OctreeNode* parent,
										 OctreeNode* child)
{
	if (!child)
	{
		llwarns << "Attempted to add a NULL child node" << llendl;
		llassert(false);
	}
	else if (child->getListenerCount() == 0)
	{
		new LLSpatialGroup(child, getSpatialPartition());
	}
	else
	{
		llwarns << "LLSpatialGroup redundancy detected." << llendl;
		llassert(false);
	}

	unbound();

	assert_states_valid(this);
}

void LLSpatialGroup::destroyGL(bool keep_occlusion)
{
	setState(GEOM_DIRTY | IMAGE_DIRTY);

	if (!keep_occlusion)
	{
		// Going to need a rebuild
		gPipeline.markRebuild(this, true);
	}

	mLastUpdateTime = gFrameTimeSeconds;
	mVertexBuffer = NULL;
	mBufferMap.clear();

	clearDrawMap();

	if (!keep_occlusion)
	{
		releaseOcclusionQueryObjectNames();
	}

	for (LLSpatialGroup::element_iter i = getDataBegin(), end = getDataEnd();
		 i != end; ++i)
	{
		LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
		if (drawable)
		{
			for (S32 j = 0, count = drawable->getNumFaces(); j < count; ++j)
			{
				LLFace* facep = drawable->getFace(j);
				if (facep)
				{
					facep->clearVertexBuffer();
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

LLSpatialPartition::LLSpatialPartition(U32 data_mask,
									   bool render_by_group,
									   U32 buffer_usage,
									   LLViewerRegion* regionp)
:	mRenderByGroup(render_by_group),
	mBridge(NULL)
{
	mRegionp = regionp;
	mVertexDataMask = data_mask;
	// In the end, the buffer usage may not be what we want it to be (because
	// of sUseStreamDraw, sPreferStreamDraw and sGLCoreProfile values), and
	// since we will compare the actual vertex buffer usage with mBufferUsage
	// in rebuildGeom(), we must ensure that they are both in sync, else we
	// recreate the vertex buffer for nothing... HB
	mBufferUsage = LLVertexBuffer::determineUsage(buffer_usage);
	mDepthMask = false;
	mSlopRatio = 0.25f;
	mInfiniteFarClip = false;

	new LLSpatialGroup(mOctree, this);
}

LLSpatialGroup* LLSpatialPartition::put(LLDrawable* drawablep,
										bool was_visible)
{
	drawablep->updateSpatialExtents();

	// Keep drawable from being garbage collected
	LLPointer<LLDrawable> ptr = drawablep;

	if (!drawablep->getGroup())
	{
		assert_octree_valid(mOctree);
		mOctree->insert(drawablep->getEntry());
		assert_octree_valid(mOctree);
	}

	LLSpatialGroup* group = drawablep->getSpatialGroup();
	if (group && was_visible &&
		group->isOcclusionState(LLSpatialGroup::QUERY_PENDING))
	{
		group->setOcclusionState(LLSpatialGroup::DISCARD_QUERY,
								 LLSpatialGroup::STATE_MODE_ALL_CAMERAS);
	}

	return group;
}

bool LLSpatialPartition::remove(LLDrawable* drawablep, LLSpatialGroup* curp)
{
	if (curp->removeObject(drawablep))
	{
		drawablep->setGroup(NULL);
		assert_octree_valid(mOctree);
		return true;
	}
	else
	{
		llwarns << "Failed to remove drawable from octree !" << llendl;
		llassert(false);
		return false;
	}
}

void LLSpatialPartition::move(LLDrawable* drawablep, LLSpatialGroup* curp,
							  bool immediate)
{
	// Sanity check submitted by open source user Bushing Spatula who was
	// seeing crashing here (see VWR-424 reported by Bunny Mayne)
	if (!drawablep)
	{
		llwarns << "Bad drawable !" << llendl;
		llassert(false);
		return;
	}

	bool was_visible = curp && curp->isVisible();

	if (curp && curp->getSpatialPartition() != this)
	{
		// Keep drawable from being garbage collected
		LLPointer<LLDrawable> ptr = drawablep;
		if (curp->getSpatialPartition()->remove(drawablep, curp))
		{
			put(drawablep, was_visible);
			return;
		}
		else
		{
			llwarns << "Drawable lost between spatial partitions on outbound transition."
					 << llendl;
			llassert(false);
		}
	}

	if (curp && curp->updateInGroup(drawablep, immediate))
	{
		// Already updated, do not need to do anything
		assert_octree_valid(mOctree);
		return;
	}

	// Keep drawable from being garbage collected
	LLPointer<LLDrawable> ptr = drawablep;
	if (curp && !remove(drawablep, curp))
	{
		llwarns << "Move could not find existing spatial group !" << llendl;
		llassert(false);
	}

	put(drawablep, was_visible);
}

class LLSpatialShift final : public OctreeTraveler
{
public:
	LLSpatialShift(const LLVector4a& offset)
	:	mOffset(offset)
	{
	}

	void visit(const OctreeNode* branch) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)branch->getListener(0);
		if (group)
		{
			group->shift(mOffset);
		}
	}

public:
	const LLVector4a& mOffset;
};

void LLSpatialPartition::shift(const LLVector4a& offset)
{
	// Shift octree node bounding boxes by offset
	LLSpatialShift shifter(offset);
	shifter.traverse(mOctree);
}

class LLOctreeCull : public LLViewerOctreeCull
{
protected:
	LOG_CLASS(LLOctreeCull);

public:
	LLOctreeCull(LLCamera* camera)
	:	LLViewerOctreeCull(camera)
	{
	}

	bool earlyFail(LLViewerOctreeGroup* base_group) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)base_group;
		if (!group)
		{
			llwarns_once << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return true;
		}
#if 1
		if (LLPipeline::sReflectionRender)
		{
			return false;
		}
#endif
		group->checkOcclusion();

			// Never occlusion cull the root node
		if (group->getOctreeNode()->getParent() &&
			// Ignore occlusion if disabled
		  	LLPipeline::sUseOcclusion &&
			group->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			gPipeline.markOccluder(group);
			return true;
		}

		return false;
	}

	S32 frustumCheck(const LLViewerOctreeGroup* group) override
	{
		S32 res = AABBInFrustumNoFarClipGroupBounds(group);
		if (res != 0)
		{
			res = llmin(res, AABBSphereIntersectGroupExtents(group));
		}
		return res;
	}

	S32 frustumCheckObjects(const LLViewerOctreeGroup* group) override
	{
		S32 res = AABBInFrustumNoFarClipObjectBounds(group);
		if (res != 0)
		{
			res = llmin(res, AABBSphereIntersectObjectExtents(group));
		}
		return res;
	}

	void processGroup(LLViewerOctreeGroup* base_group) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)base_group;
		if (!group)
		{
			llwarns_once << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		if (group->needsUpdate() ||
			group->getVisible(LLViewerCamera::sCurCameraID) <
				LLDrawable::getCurrentFrame() - 1)
		{
			group->doOcclusion(mCamera);
		}
		gPipeline.markNotCulled(group, *mCamera);
	}
};

class LLOctreeCullNoFarClip final : public LLOctreeCull
{
public:
	LLOctreeCullNoFarClip(LLCamera* camera)
	:	LLOctreeCull(camera)
	{
	}

	S32 frustumCheck(const LLViewerOctreeGroup* group) override
	{
		return AABBInFrustumNoFarClipGroupBounds(group);
	}

	S32 frustumCheckObjects(const LLViewerOctreeGroup* group) override
	{
		return AABBInFrustumNoFarClipObjectBounds(group);
	}
};

class LLOctreeCullShadow : public LLOctreeCull
{
public:
	LLOctreeCullShadow(LLCamera* camera)
	:	LLOctreeCull(camera)
	{
	}

	S32 frustumCheck(const LLViewerOctreeGroup* group) override
	{
		return AABBInFrustumGroupBounds(group);
	}

	S32 frustumCheckObjects(const LLViewerOctreeGroup* group) override
	{
		return AABBInFrustumObjectBounds(group);
	}
};

class LLOctreeCullVisExtents final : public LLOctreeCullShadow
{
protected:
	LOG_CLASS(LLOctreeCullVisExtents);

public:
	LLOctreeCullVisExtents(LLCamera* camera, LLVector4a& min, LLVector4a& max)
	:	LLOctreeCullShadow(camera),
		mMin(min),
		mMax(max),
		mEmpty(true)
	{
	}

	bool earlyFail(LLViewerOctreeGroup* base_group) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)base_group;
		if (!group)
		{
			llwarns_once << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return true;
		}

			// Never occlusion cull the root node
		if (group->getOctreeNode()->getParent() &&
			// Ignore occlusion if disabled
		  	LLPipeline::sUseOcclusion &&
			group->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			return true;
		}

		return false;
	}

	void traverse(const OctreeNode* n) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)n->getListener(0);
		if (!group)
		{
			llwarns_once << "NULL spatial group for octree node "
						 << n << " !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		if (earlyFail(group))
		{
			return;
		}

		if (mRes == 2 ||
			(mRes && group->hasState(LLSpatialGroup::SKIP_FRUSTUM_CHECK)))
		{
			// Do not need to do frustum check
			OctreeTraveler::traverse(n);
		}
		else
		{
			mRes = frustumCheck(group);
			if (mRes)
			{
				// At least partially in, run on down
				OctreeTraveler::traverse(n);
			}
			mRes = 0;
		}
	}

	void processGroup(LLViewerOctreeGroup* base_group) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)base_group;
		if (!group)
		{
			llwarns_once << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		llassert(!group->hasState(LLSpatialGroup::DIRTY) && !group->isEmpty());

		if (mRes >= 2 || AABBInFrustumObjectBounds(group) > 0)
		{
			mEmpty = false;
			const LLVector4a* exts = group->getObjectExtents();
			update_min_max(mMin, mMax, exts[0]);
			update_min_max(mMin, mMax, exts[1]);
		}
	}

public:
	bool		mEmpty;
	LLVector4a&	mMin;
	LLVector4a&	mMax;
};

class LLOctreeCullDetectVisible final : public LLOctreeCullShadow
{
protected:
	LOG_CLASS(LLOctreeCullDetectVisible);

public:
	LLOctreeCullDetectVisible(LLCamera* camera)
	:	LLOctreeCullShadow(camera),
		mResult(false)
	{
	}

	bool earlyFail(LLViewerOctreeGroup* base_group) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)base_group;
		if (!group)
		{
			llwarns_once << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return true;
		}

		if (mResult || // Already found a node, do not check any more
			 // Never occlusion cull the root node
			(group->getOctreeNode()->getParent() &&
			 // Ignore occlusion if disabled
			 LLPipeline::sUseOcclusion &&
			 group->isOcclusionState(LLSpatialGroup::OCCLUDED)))
		{
			return true;
		}

		return false;
	}

	void processGroup(LLViewerOctreeGroup* base_group) override
	{
		if (base_group && base_group->isVisible())
		{
			mResult = true;
		}
	}

public:
	bool mResult;
};

class LLOctreeSelect final : public LLOctreeCull
{
public:
	LLOctreeSelect(LLCamera* camera, std::vector<LLDrawable*>* results)
	:	LLOctreeCull(camera),
		mResults(results)
	{
	}

	LL_INLINE bool earlyFail(LLViewerOctreeGroup* group) override
	{
		return false;
	}

	LL_INLINE void preprocess(LLViewerOctreeGroup* group) override
	{
	}

	void processGroup(LLViewerOctreeGroup* base_group) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)base_group;
		if (!group)
		{
			llwarns_once << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		OctreeNode* branch = group->getOctreeNode();
		if (!branch)
		{
			llwarns_once << "NULL octree node !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		for (OctreeNode::const_element_iter i = branch->getDataBegin(),
											end = branch->getDataEnd();
			 i != end; ++i)
		{
			LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
			if (drawable && !drawable->isDead())
			{
				if (drawable->isSpatialBridge())
				{
					drawable->setVisible(*mCamera, mResults, true);
				}
				else
				{
					mResults->push_back(drawable);
				}
			}
		}
	}

public:
	std::vector<LLDrawable*>* mResults;
};

void drawBox(const LLVector3& c, const LLVector3& r)
{
	static const LLVector3 v1(-1.f, 1.f, -1.f);
	static const LLVector3 v2(-1.f, 1.f, 1.f);
	static const LLVector3 v3(1.f, 1.f, -1.f);
	static const LLVector3 v4(1.f, 1.f, 1.f);
	static const LLVector3 v5(1.f, -1.f, -1.f);
	static const LLVector3 v6(1.f, -1.f, 1.f);
	static const LLVector3 v7(-1.f, -1.f, -1.f);
	static const LLVector3 v8(-1.f, -1.f, 1.f);

	LLVertexBuffer::unbind();

	gGL.begin(LLRender::TRIANGLE_STRIP);

	// Left front
	gGL.vertex3fv((c + r.scaledVec(v1)).mV);
	gGL.vertex3fv((c + r.scaledVec(v2)).mV);

	// Right front
	gGL.vertex3fv((c + r.scaledVec(v3)).mV);
	gGL.vertex3fv((c + r.scaledVec(v4)).mV);

	// Right back
 	gGL.vertex3fv((c + r.scaledVec(v5)).mV);
	gGL.vertex3fv((c + r.scaledVec(v6)).mV);

	// Left back
	gGL.vertex3fv((c + r.scaledVec(v7)).mV);
	gGL.vertex3fv((c + r.scaledVec(v8)).mV);

	// Left front
	gGL.vertex3fv((c + r.scaledVec(v1)).mV);
	gGL.vertex3fv((c + r.scaledVec(v2)).mV);

	gGL.end();

	// Bottom
	gGL.begin(LLRender::TRIANGLE_STRIP);
	gGL.vertex3fv((c + r.scaledVec(v3)).mV);
	gGL.vertex3fv((c + r.scaledVec(v5)).mV);
	gGL.vertex3fv((c + r.scaledVec(v1)).mV);
	gGL.vertex3fv((c + r.scaledVec(v7)).mV);
	gGL.end();

	// Top
	gGL.begin(LLRender::TRIANGLE_STRIP);
	gGL.vertex3fv((c + r.scaledVec(v4)).mV);
	gGL.vertex3fv((c + r.scaledVec(v2)).mV);
	gGL.vertex3fv((c + r.scaledVec(v6)).mV);
	gGL.vertex3fv((c + r.scaledVec(v8)).mV);
	gGL.end();
}

void drawBox(const LLVector4a& c, const LLVector4a& r)
{
	drawBox(reinterpret_cast<const LLVector3&>(c),
			reinterpret_cast<const LLVector3&>(r));
}

void drawBoxOutline(const LLVector3& pos, const LLVector3& size)
{
	LLVector3 v1 = size.scaledVec(LLVector3(1.f, 1.f, 1.f));
	LLVector3 v2 = size.scaledVec(LLVector3(-1.f, 1.f, 1.f));
	LLVector3 v3 = size.scaledVec(LLVector3(-1.f, -1.f, 1.f));
	LLVector3 v4 = size.scaledVec(LLVector3(1.f, -1.f, 1.f));

	gGL.begin(LLRender::LINES);

	// Top
	gGL.vertex3fv((pos + v1).mV);
	gGL.vertex3fv((pos + v2).mV);
	gGL.vertex3fv((pos + v2).mV);
	gGL.vertex3fv((pos + v3).mV);
	gGL.vertex3fv((pos + v3).mV);
	gGL.vertex3fv((pos + v4).mV);
	gGL.vertex3fv((pos + v4).mV);
	gGL.vertex3fv((pos + v1).mV);

	// Bottom
	gGL.vertex3fv((pos - v1).mV);
	gGL.vertex3fv((pos - v2).mV);
	gGL.vertex3fv((pos - v2).mV);
	gGL.vertex3fv((pos - v3).mV);
	gGL.vertex3fv((pos - v3).mV);
	gGL.vertex3fv((pos - v4).mV);
	gGL.vertex3fv((pos - v4).mV);
	gGL.vertex3fv((pos - v1).mV);

	// Right
	gGL.vertex3fv((pos + v1).mV);
	gGL.vertex3fv((pos - v3).mV);

	gGL.vertex3fv((pos + v4).mV);
	gGL.vertex3fv((pos - v2).mV);

	// Left
	gGL.vertex3fv((pos + v2).mV);
	gGL.vertex3fv((pos - v4).mV);

	gGL.vertex3fv((pos + v3).mV);
	gGL.vertex3fv((pos - v1).mV);

	gGL.end();
}

void drawBoxOutline(const LLVector4a& pos, const LLVector4a& size)
{
	drawBoxOutline(reinterpret_cast<const LLVector3&>(pos),
				   reinterpret_cast<const LLVector3&>(size));
}

class LLOctreeDirty : public OctreeTraveler
{
protected:
	LOG_CLASS(LLOctreeDirty);

public:
	LLOctreeDirty(bool no_rebuild)
	:	mNoRebuild(no_rebuild)
	{
	}

	void visit(const OctreeNode* state) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)state->getListener(0);
		if (!group)
		{
			llwarns_once << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		group->destroyGL();

		if (!mNoRebuild)
		{
			for (LLSpatialGroup::element_iter i = group->getDataBegin(),
											  end = group->getDataEnd();
				 i != end; ++i)
			{
				LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
				if (!drawable)
				{
					llwarns_once << "NULL drawable found in spatial group "
								 << std::hex << group << std::dec << llendl;
					continue;
				}
				LLViewerObject* vobjp = drawable->getVObj().get();
				if (!vobjp)
				{
					continue;
				}
				vobjp->resetVertexBuffers();
				if (!group->getSpatialPartition()->mRenderByGroup)
				{
					gPipeline.markRebuild(drawable, LLDrawable::REBUILD_ALL,
										  true);
				}
			}
		}

		for (LLSpatialGroup::bridge_list_t::iterator
				i = group->mBridgeList.begin(),
				end = group->mBridgeList.end();
			 i != end; ++i)
		{
			LLSpatialBridge* bridge = *i;
			if (bridge)
			{
				traverse(bridge->mOctree);
			}
			else
			{
				llwarns_once << "NULL bridge found in spatial group "
							 << std::hex << group << std::dec << llendl;
			}
		}
	}

private:
	bool mNoRebuild;
};

void LLSpatialPartition::restoreGL()
{
}

void LLSpatialPartition::resetVertexBuffers()
{
	LLOctreeDirty dirty(sTeleportRequested);
	dirty.traverse(mOctree);
}

bool LLSpatialPartition::getVisibleExtents(LLCamera& camera, LLVector3& vis_min,
										   LLVector3& vis_max)
{
	LLVector4a vis_min_a, vis_max_a;
	vis_min_a.load3(vis_min.mV);
	vis_max_a.load3(vis_max.mV);

	{
		LL_FAST_TIMER(FTM_CULL_REBOUND);
		LLSpatialGroup* group = (LLSpatialGroup*)mOctree->getListener(0);
		if (group)
		{
			group->rebound();
		}
	}

	LLOctreeCullVisExtents vis(&camera, vis_min_a, vis_max_a);
	vis.traverse(mOctree);

	vis_min.set(vis_min_a.getF32ptr());
	vis_max.set(vis_max_a.getF32ptr());

	return vis.mEmpty;
}

bool LLSpatialPartition::visibleObjectsInFrustum(LLCamera& camera)
{
	LLOctreeCullDetectVisible vis(&camera);
	vis.traverse(mOctree);
	return vis.mResult;
}

S32 LLSpatialPartition::cull(LLCamera& camera,
							 std::vector<LLDrawable*>* results,
							 bool for_select)
{
#if LL_OCTREE_PARANOIA_CHECK
	((LLSpatialGroup*)mOctree->getListener(0))->checkStates();
#endif
	{
		LL_FAST_TIMER(FTM_CULL_REBOUND);
		LLSpatialGroup* group = (LLSpatialGroup*)mOctree->getListener(0);
		if (group)
		{
			group->rebound();
		}
	}

#if LL_OCTREE_PARANOIA_CHECK
	((LLSpatialGroup*)mOctree->getListener(0))->validate();
#endif

#if 0
	if (for_select)
#endif
	{
		LLOctreeSelect selecter(&camera, results);
		selecter.traverse(mOctree);
	}

	return 0;
}

S32 LLSpatialPartition::cull(LLCamera& camera, bool do_occlusion)
{
#if LL_OCTREE_PARANOIA_CHECK
	((LLSpatialGroup*)mOctree->getListener(0))->checkStates();
#endif
	{
		LL_FAST_TIMER(FTM_CULL_REBOUND);
		LLSpatialGroup* group = (LLSpatialGroup*)mOctree->getListener(0);
		if (group)
		{
			group->rebound();
		}
	}

#if LL_OCTREE_PARANOIA_CHECK
	((LLSpatialGroup*)mOctree->getListener(0))->validate();
#endif

	if (LLPipeline::sShadowRender)
	{
		LL_FAST_TIMER(FTM_FRUSTUM_CULL);
		LLOctreeCullShadow culler(&camera);
		culler.traverse(mOctree);
	}
	else if (mInfiniteFarClip || !LLPipeline::sUseFarClip)
	{
		LL_FAST_TIMER(FTM_FRUSTUM_CULL);
		LLOctreeCullNoFarClip culler(&camera);
		culler.traverse(mOctree);
	}
	else
	{
		LL_FAST_TIMER(FTM_FRUSTUM_CULL);
		LLOctreeCull culler(&camera);
		culler.traverse(mOctree);
	}

	return 0;
}

void pushVerts(LLDrawInfo* params, U32 mask)
{
	LLRenderPass::applyModelMatrix(*params);
	params->mVertexBuffer->setBuffer(mask);
	params->mVertexBuffer->drawRange(params->mParticle ? LLRender::POINTS
													   : LLRender::TRIANGLES,
									 params->mStart, params->mEnd,
									 params->mCount, params->mOffset);
}

void pushVerts(LLSpatialGroup* group, U32 mask)
{
	for (LLSpatialGroup::draw_map_t::const_iterator
			i = group->mDrawMap.begin(), end = group->mDrawMap.end();
		 i != end; ++i)
	{
		for (LLSpatialGroup::drawmap_elem_t::const_iterator
				j = i->second.begin(), end2 = i->second.end();
			 j != end2; ++j)
		{
			LLDrawInfo* params = *j;
			pushVerts(params, mask);
		}
	}
}

void pushVerts(LLFace* face, U32 mask)
{
	if (face)
	{
		llassert(face->verify());

		LLVertexBuffer* buffer = face->getVertexBuffer();
		if (buffer && face->getGeomCount() >= 3)
		{
			buffer->setBuffer(mask);
			U16 start = face->getGeomStart();
			U16 end = start + face->getGeomCount() - 1;
			U32 count = face->getIndicesCount();
			U16 offset = face->getIndicesStart();
			buffer->drawRange(LLRender::TRIANGLES, start, end, count, offset);
		}
	}
}

void pushVerts(LLDrawable* drawable, U32 mask)
{
	for (S32 i = 0, count = drawable->getNumFaces(); i < count; ++i)
	{
		LLFace* face = drawable->getFace(i);
		if (face)
		{
			pushVerts(face, mask);
		}
	}
}

void pushVerts(LLVolume* volume)
{
	if (!volume) return;

	LLVertexBuffer::unbind();
	for (S32 i = 0, count = volume->getNumVolumeFaces(); i < count; ++i)
	{
		const LLVolumeFace& face = volume->getVolumeFace(i);
		LLVertexBuffer::drawElements(LLRender::TRIANGLES, face.mNumVertices,
									 face.mPositions, NULL, face.mNumIndices,
									 face.mIndices);
	}
}

void pushBufferVerts(LLVertexBuffer* buffer, U32 mask)
{
	if (buffer)
	{
		buffer->setBuffer(mask);
		buffer->drawRange(LLRender::TRIANGLES, 0, buffer->getNumVerts() - 1,
						  buffer->getNumIndices(), 0);
	}
}

void pushBufferVerts(LLSpatialGroup* group, U32 mask, bool push_alpha = true)
{
	if (group->getSpatialPartition()->mRenderByGroup &&
		!group->mDrawMap.empty())
	{
		LLDrawInfo* params = *(group->mDrawMap.begin()->second.begin());
		LLRenderPass::applyModelMatrix(*params);

		if (push_alpha)
		{
			pushBufferVerts(group->mVertexBuffer, mask);
		}

		for (LLSpatialGroup::buffer_map_t::const_iterator
				i = group->mBufferMap.begin(), end = group->mBufferMap.end();
			 i != end; ++i)
		{
			for (LLSpatialGroup::buffer_texture_map_t::const_iterator
					j = i->second.begin(), end2 = i->second.end();
				 j != end2; ++j)
			{
				for (LLSpatialGroup::buffer_list_t::const_iterator
						k = j->second.begin(), end3 = j->second.end();
					 k != end3; ++k)
				{
					pushBufferVerts(*k, mask);
				}
			}
		}
	}
}

void pushVertsColorCoded(LLSpatialGroup* group, U32 mask)
{
	static const LLColor4 colors[] = {
		LLColor4::green,
		LLColor4::green1,
		LLColor4::green2,
		LLColor4::green3,
		LLColor4::green4,
		LLColor4::green5,
		LLColor4::green6
	};

	constexpr U32 col_count = LL_ARRAY_SIZE(colors);

	U32 col = 0;

	for (LLSpatialGroup::draw_map_t::const_iterator
			i = group->mDrawMap.begin(), end = group->mDrawMap.end();
		 i != end; ++i)
	{
		for (LLSpatialGroup::drawmap_elem_t::const_iterator
				j = i->second.begin(), end2 = i->second.end();
			 j != end2 ; ++j)
		{
			LLDrawInfo* params = *j;
			LLRenderPass::applyModelMatrix(*params);
			gGL.diffuseColor4f(colors[col].mV[0], colors[col].mV[1],
							   colors[col].mV[2], 0.5f);
			params->mVertexBuffer->setBuffer(mask);
			params->mVertexBuffer->drawRange(params->mParticle ? LLRender::POINTS
															   : LLRender::TRIANGLES,
											 params->mStart, params->mEnd,
											 params->mCount, params->mOffset);
			col = (col + 1) % col_count;
		}
	}
}

// Renders solid object bounding box, color coded by buffer usage and activity
void renderOctree(LLSpatialGroup* group)
{
	gGL.setSceneBlendType(LLRender::BT_ADD_WITH_ALPHA);
	LLVector4 col;
	if (group->mBuilt > 0.f)
	{
		group->mBuilt -= 2.f * gFrameIntervalSeconds;
		if (group->mBufferUsage == GL_STATIC_DRAW_ARB)
		{
			col.set(1.f, 0.f, 0.f, group->mBuilt * 0.5f);
		}
		else
		{
#if 1
			col.set(0.1f, 0.1f, 1.f, 0.1f);
#else
			col.set(1.f, 1.f, 0.f, sinf(group->mBuilt * F_PI) * 0.5f);
#endif
		}

		if (group->mBufferUsage != GL_STATIC_DRAW_ARB)
		{
			LLGLDepthTest gl_depth(false, false);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			gGL.diffuseColor4f(1.f, 0.f, 0.f, group->mBuilt);
			gGL.flush();
			gGL.lineWidth(5.f);
			const LLVector4a* bounds = group->getObjectBounds();
			drawBoxOutline(bounds[0], bounds[1]);
			gGL.flush();
			gGL.lineWidth(1.f);
			gGL.flush();

			for (LLSpatialGroup::element_iter i = group->getDataBegin(),
											  end = group->getDataEnd();
				 i != end; ++i)
			{
				LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
				if (!drawable || drawable->isDead()) continue;

				U32 count = drawable->getNumFaces();
				if (!count) continue;

				if (!group->getSpatialPartition()->isBridge())
				{
					gGL.pushMatrix();
					LLVector3 trans = drawable->getRegion()->getOriginAgent();
					gGL.translatef(trans.mV[0], trans.mV[1], trans.mV[2]);
				}

				for (U32 j = 0; j < count; ++j)
				{
					LLFace* face = drawable->getFace(j);
					if (face && face->getVertexBuffer())
					{
						if (gFrameTimeSeconds - face->mLastUpdateTime < 0.5f)
						{
							gGL.diffuseColor4f(0.f, 1.f, 0.f, group->mBuilt);
						}
						else if (gFrameTimeSeconds - face->mLastMoveTime < 0.5f)
						{
							gGL.diffuseColor4f(1.f, 0.f, 0.f, group->mBuilt);
						}
						else
						{
							continue;
						}

						face->getVertexBuffer()->setBuffer(LLVertexBuffer::MAP_VERTEX);
#if 0
						drawBox((face->mExtents[0] + face->mExtents[1]) * 0.5f,
								(face->mExtents[1] - face->mExtents[0]) * 0.5f);
#endif
						face->getVertexBuffer()->draw(LLRender::TRIANGLES,
													  face->getIndicesCount(),
													  face->getIndicesStart());
					}
				}

				if (!group->getSpatialPartition()->isBridge())
				{
					gGL.popMatrix();
				}
			}
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
		}
	}
	else if (group->mBufferUsage == GL_STATIC_DRAW_ARB && !group->isEmpty() &&
			 group->getSpatialPartition()->mRenderByGroup)
	{
		col.set(0.8f, 0.4f, 0.1f, 0.1f);
	}
	else
	{
		col.set(0.1f, 0.1f, 1.f, 0.1f);
	}

	gGL.diffuseColor4fv(col.mV);
	LLVector4a fudge;
	fudge.splat(0.001f);

#if 0
	LLVector4a size = group->mObjectBounds[1];
	size.mul(1.01f);
	size.add(fudge);

	{
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);
		drawBox(group->mObjectBounds[0], fudge);
	}
#endif

	gGL.setSceneBlendType(LLRender::BT_ALPHA);

#if 0
	if (group->mBuilt <= 0.f)
#endif
	{
#if 0
		// Draw opaque outline
		gGL.diffuseColor4f(col.mV[0], col.mV[1], col.mV[2], 1.f);
		drawBoxOutline(group->mObjectBounds[0], group->mObjectBounds[1]);
#endif

		gGL.diffuseColor4f(0.f, 1.f, 1.f, 1.f);
		const LLVector4a* bounds = group->getBounds();
		drawBoxOutline(bounds[0], bounds[1]);

#if 0
		// Draw bounding box for draw info
		if (group->getSpatialPartition()->mRenderByGroup)
		{
			gGL.diffuseColor4f(1.f, 0.75f, 0.25f, 0.6f);
			for (LLSpatialGroup::draw_map_t::iterator
					i = group->mDrawMap.begin(), end = group->mDrawMap.end();
				 i != end; ++i)
			{
				for (LLSpatialGroup::drawmap_elem_t::iterator
						j = i->second.begin(), end2 = i->second.end();
					 j != end2; ++j)
				{
					LLDrawInfo* draw_info = *j;
					LLVector4a center;
					center.setAdd(draw_info->mExtents[1], draw_info->mExtents[0]);
					center.mul(0.5f);
					LLVector4a size;
					size.setSub(draw_info->mExtents[1], draw_info->mExtents[0]);
					size.mul(0.5f);
					drawBoxOutline(center, size);
				}
			}
		}
#endif
	}

#if 0
	LLSpatialGroup::OctreeNode* node = group->mOctreeNode;
	gGL.diffuseColor4f(0.f, 1.f, 0.f, 1.f);
	drawBoxOutline(LLVector3(node->getCenter()), LLVector3(node->getSize()));
#endif

	stop_glerror();
}

#if 0
void renderVisibility(LLSpatialGroup* group, LLCamera* camera)
{
# if 0
	LLGLEnable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	LLGLEnable cull(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	bool render_objects =
		(!LLPipeline::sUseOcclusion ||
		 !group->isOcclusionState(LLSpatialGroup::OCCLUDED)) &&
		group->isVisible() && !group->isEmpty();
	if (render_objects)
	{
		LLGLDepthTest depth_under(GL_TRUE, GL_FALSE);
		LLGLDisable blend(GL_BLEND);
		gGL.diffuseColor4f(0.f, 0.75f, 0.f, 0.5f);
		pushBufferVerts(group, LLVertexBuffer::MAP_VERTEX, false);

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		gGL.lineWidth(4.f);
		gGL.diffuseColor4f(0.f, 0.5f, 0.f, 1.f);
		pushBufferVerts(group, LLVertexBuffer::MAP_VERTEX, false);
		gGL.lineWidth(1.f);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		bool selected = false;
		for (LLSpatialGroup::element_iter iter = group->getDataBegin(),
										  end = group->getDataEnd();
			 iter != end; ++iter)
		{
			LLDrawable* drawable = (LLDrawable*)(*iter)->getDrawable();
			if (drawable && drawable->getVObj().notNull() &&
				drawable->getVObj()->isSelected())
			{
				selected = true;
				break;
			}
		}

		if (selected)
		{
			// store for rendering occlusion volume as overlay
			if (!group->getSpatialPartition()->isBridge())
			{
				gVisibleSelectedGroups.insert(group);
			}
		}
	}
# else
	if (render_objects)
	{
		LLGLDepthTest depth_under(GL_TRUE, GL_FALSE, GL_GREATER);
		gGL.diffuseColor4f(0.f, 0.5f, 0.f, 0.5f);
		gGL.diffuseColor4f(0.f, 0.5f, 0.f, 0.5f);
		pushBufferVerts(group, LLVertexBuffer::MAP_VERTEX);
	}

	{
		LLGLDepthTest depth_over(GL_TRUE, GL_FALSE, GL_LEQUAL);

		if (render_objects)
		{
			gGL.diffuseColor4f(0.f, 0.5f, 0.f, 1.f);
			gGL.diffuseColor4f(0.f, 0.5f, 0.f, 1.f);
			pushBufferVerts(group, LLVertexBuffer::MAP_VERTEX);
		}

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		if (render_objects)
		{
			gGL.diffuseColor4f(0.f, 0.75f, 0.f, 0.5f);
			gGL.diffuseColor4f(0.f, 0.75f, 0.f, 0.5f);
			pushBufferVerts(group, LLVertexBuffer::MAP_VERTEX);
		}

		bool selected = false;
		for (LLSpatialGroup::element_iter iter = group->getDataBegin(),
										  end = group->getDataEnd();
			 iter != end; ++iter)
		{
			LLDrawable* drawable = (LLDrawable*)(*iter)->getDrawable();
			if (drawable && drawable->getVObj().notNull() &&
				drawable->getVObj()->isSelected())
			{
				selected = true;
				break;
			}
		}

		if (selected)
		{
			// Store for rendering occlusion volume as overlay
			if (!group->getSpatialPartition()->isBridge())
			{
				gVisibleSelectedGroups.insert(group);
			}
		}
	}
# endif
}
#endif

void renderXRay(LLSpatialGroup* group, LLCamera* camera)
{
	if (!group->isVisible() || group->isEmpty() ||
		(LLPipeline::sUseOcclusion &&
		 group->isOcclusionState(LLSpatialGroup::OCCLUDED)))
	{
		return;
	}

	pushBufferVerts(group, LLVertexBuffer::MAP_VERTEX, false);

	bool selected = false;
	for (LLSpatialGroup::element_iter iter = group->getDataBegin(),
									  end = group->getDataEnd();
		 iter != end; ++iter)
	{
		LLDrawable* drawable = (LLDrawable*)(*iter)->getDrawable();
		if (drawable && drawable->getVObj().notNull() &&
			drawable->getVObj()->isSelected())
		{
			selected = true;
			break;
		}
	}

	if (!selected)
	{
		return;
	}

	// Store for rendering occlusion volume as overlay
	if (group->getSpatialPartition()->isBridge())
	{
		gVisibleSelectedGroups.insert(group->getSpatialPartition()->asBridge()->getSpatialGroup());
	}
	else
	{
		gVisibleSelectedGroups.insert(group);
	}
}

void renderCrossHairs(LLVector3 position, F32 size, LLColor4 color)
{
	gGL.color4fv(color.mV);
	gGL.begin(LLRender::LINES);
	{
		gGL.vertex3fv((position - LLVector3(size, 0.f, 0.f)).mV);
		gGL.vertex3fv((position + LLVector3(size, 0.f, 0.f)).mV);
		gGL.vertex3fv((position - LLVector3(0.f, size, 0.f)).mV);
		gGL.vertex3fv((position + LLVector3(0.f, size, 0.f)).mV);
		gGL.vertex3fv((position - LLVector3(0.f, 0.f, size)).mV);
		gGL.vertex3fv((position + LLVector3(0.f, 0.f, size)).mV);
	}
	gGL.end();
}

void renderUpdateType(LLDrawable* drawablep)
{
	LLViewerObject* vobj = drawablep->getVObj();
	if (!vobj || OUT_UNKNOWN == vobj->getLastUpdateType())
	{
		return;
	}

	LLGLEnable blend(GL_BLEND);

	switch (vobj->getLastUpdateType())
	{
		case OUT_FULL:
			gGL.diffuseColor4f(0.f, 1.f, 0.f, 0.5f);
			break;

		case OUT_TERSE_IMPROVED:
			gGL.diffuseColor4f(0.f, 1.f, 1.f, 0.5f);
			break;

		case OUT_FULL_COMPRESSED:
			if (vobj->getLastUpdateCached())
			{
				gGL.diffuseColor4f(1.f, 0.f, 0.f, 0.5f);
			}
			else
			{
				gGL.diffuseColor4f(1.f, 1.f, 0.f, 0.5f);
			}
			break;

		case OUT_FULL_CACHED:
			gGL.diffuseColor4f(0.f, 0.f, 1.f, 0.5f);
			break;

		default:
			llwarns << "Unknown update_type " << vobj->getLastUpdateType()
					<< llendl;
	}

	S32 num_faces = drawablep->getNumFaces();
	if (num_faces)
	{
		for (S32 i = 0; i < num_faces; ++i)
		{
			LLFace* face = drawablep->getFace(i);
			if (face)
			{
				pushVerts(face, LLVertexBuffer::MAP_VERTEX);
			}
		}
	}
}

void renderBoundingBox(LLDrawable* drawable, bool set_color = true)
{
	if (set_color)
	{
		if (drawable->isSpatialBridge())
		{
			gGL.diffuseColor4f(1.f, 0.5f, 0.f, 1.f);
		}
		else if (drawable->getVOVolume())
		{
			if (drawable->isRoot())
			{
				gGL.diffuseColor4f(1.f, 1.f, 0.f, 1.f);
			}
			else
			{
				gGL.diffuseColor4f(0.f, 1.f, 0.f, 1.f);
			}
		}
		else if (drawable->getVObj())
		{
			switch (drawable->getVObj()->getPCode())
			{
				case LLViewerObject::LL_VO_SURFACE_PATCH:
					gGL.diffuseColor4f(0.f, 1.f, 1.f, 1.f);
					break;

				case LLViewerObject::LL_VO_CLOUDS:
					gGL.diffuseColor4f(0.5f, 0.5f, 0.5f, 1.f);
					break;

				case LLViewerObject::LL_VO_PART_GROUP:
				case LLViewerObject::LL_VO_HUD_PART_GROUP:
					gGL.diffuseColor4f(0.f, 0.f, 1.f, 1.f);
					break;

				case LLViewerObject::LL_VO_VOID_WATER:
				case LLViewerObject::LL_VO_WATER:
					gGL.diffuseColor4f(0.f, 0.5f, 1.f, 1.f);
					break;

				case LL_PCODE_LEGACY_TREE:
					gGL.diffuseColor4f(0.f, 0.5f, 0.f, 1.f);
					break;

				default:
					gGL.diffuseColor4f(1.f, 0.f, 1.f, 1.f);
			}
		}
		else
		{
			gGL.diffuseColor4f(1.f, 0.f, 0.f, 1.f);
		}
	}

	const LLVector4a* ext;
	LLVector4a pos, size;

	if (drawable->getVOVolume())
	{
		// Render face bounding boxes
		for (S32 i = 0, count = drawable->getNumFaces(); i < count; ++i)
		{
			LLFace* facep = drawable->getFace(i);
			if (facep)
			{
				ext = facep->mExtents;

				pos.setAdd(ext[0], ext[1]);
				pos.mul(0.5f);
				size.setSub(ext[1], ext[0]);
				size.mul(0.5f);

				drawBoxOutline(pos, size);
			}
		}
	}

	// Render drawable bounding box
	ext = drawable->getSpatialExtents();

	pos.setAdd(ext[0], ext[1]);
	pos.mul(0.5f);
	size.setSub(ext[1], ext[0]);
	size.mul(0.5f);

	LLViewerObject* vobj = drawable->getVObj();
	if (vobj && vobj->onActiveList())
	{
		gGL.flush();
		gGL.lineWidth(llmax(4.f * sinf(gFrameTimeSeconds * 2.f) + 1.f, 1.f));
		drawBoxOutline(pos,size);
		gGL.flush();
		gGL.lineWidth(1.f);
	}
	else
	{
		drawBoxOutline(pos, size);
	}

	stop_glerror();
}

void renderNormals(LLDrawable* drawablep)
{
	if (!drawablep->isVisible())
	{
		return;
	}

	LLVOVolume* vol = drawablep->getVOVolume();
	if (!vol)
	{
		return;
	}

	LLVolume* volume = vol->getVolume();
	if (!volume)
	{
		return;
	}

	LLVertexBuffer::unbind();

	// Drawable's normals & tangents are stored in model space, i.e. before any
	// scaling is applied. SL-13490: using pos + normal to compute the second
	// vertex of a normal line segment does not work when there is a non-
	// uniform scale in the mix. Normals require MVP-inverse-transpose
	// transform. We get that effect here by pre-applying the inverse scale
	// (twice, because one forward scale will be re-applied via the MVP in the
	// vertex shader)

	LLVector3 scale_v3 = vol->getScale();
	F32 scale_len = scale_v3.length();
	LLVector4a obj_scale(scale_v3.mV[VX], scale_v3.mV[VY], scale_v3.mV[VZ]);
	obj_scale.normalize3();

	// Normals & tangent line segments get scaled along with the object. Divide
	// by scale length to keep the as-viewed lengths (relatively) constant with
	// the debug setting length.
	static LLCachedControl<F32> norm_scale(gSavedSettings,
										   "RenderDebugNormalScale");
	F32 draw_length = norm_scale / scale_len;

	// Create inverse-scale vector for normals
	LLVector4a inv_scale(1.f / scale_v3.mV[VX], 1.f / scale_v3.mV[VY],
						 1.f / scale_v3.mV[VZ]);
	inv_scale.mul(inv_scale);  // Squared, to apply inverse scale twice
	inv_scale.normalize3fast();

	gGL.pushMatrix();
	gGL.multMatrix(vol->getRelativeXform().getF32ptr());

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	LLVector4a p, v;
	for (S32 i = 0, count = volume->getNumVolumeFaces(); i < count; ++i)
	{
		const LLVolumeFace& face = volume->getVolumeFace(i);

		gGL.flush();
		gGL.diffuseColor4f(1.f, 1.f, 0.f, 1.f);
		gGL.begin(LLRender::LINES);
		for (S32 j = 0; j < face.mNumVertices; ++j)
		{
			v.setMul(face.mNormals[j], 1.f);
			// Pre-scale normal, so it is left with an inverse-transpose xform
			// after MVP
			v.mul(inv_scale);
			v.normalize3fast();
			v.mul(draw_length);
			p.setAdd(face.mPositions[j], v);

			gGL.vertex3fv(face.mPositions[j].getF32ptr());
			gGL.vertex3fv(p.getF32ptr());
		}
		gGL.end();

		if (!face.mTangents)
		{
			continue;
		}

		// Tangents are simple vectors and do not require reorientation via
		// pre-scaling
		gGL.flush();
		gGL.diffuseColor4f(0.f, 1.f, 1.f, 1.f);
		gGL.begin(LLRender::LINES);
		for (S32 j = 0; j < face.mNumVertices; ++j)
		{
			v.setMul(face.mTangents[j], 1.f);
			v.normalize3fast();
			v.mul(draw_length);

			p.setAdd(face.mPositions[j], v);
			gGL.vertex3fv(face.mPositions[j].getF32ptr());
			gGL.vertex3fv(p.getF32ptr());
		}
		gGL.end();
	}

	gGL.popMatrix();
	stop_glerror();
}

void renderTexturePriority(LLDrawable* drawable)
{
	for (S32 face = 0, count = drawable->getNumFaces(); face < count; ++face)
	{
		LLFace* facep = drawable->getFace(face);
		if (!facep) continue;

		LLVector4 cold(0.f, 0.f, 0.25f);
		LLVector4 hot(1.f, 0.25f, 0.25f);

		LLVector4 boost_cold(0.f, 0.f, 0.f, 0.f);
		LLVector4 boost_hot(0.f, 1.f, 0.f, 1.f);

		LLGLDisable blend(GL_BLEND);

#if 0
		LLViewerTexture* imagep = facep->getTexture();
		if (imagep)
#endif
		{
#if 0
			F32 vsize = imagep->mMaxVirtualSize;
#else
			F32 vsize = facep->getPixelArea();
#endif
			if (vsize > sCurMaxTexPriority)
			{
				sCurMaxTexPriority = vsize;
			}

			F32 t = vsize / sLastMaxTexPriority;

			LLVector4 col = lerp(cold, hot, t);
			gGL.diffuseColor4fv(col.mV);
		}
#if 0
		else
		{
			gGL.diffuseColor4f(1.f, 0.f, 1.f, 1.f);
		}
#endif

		LLVector4a center;
		center.setAdd(facep->mExtents[1], facep->mExtents[0]);
		center.mul(0.5f);
		LLVector4a size;
		size.setSub(facep->mExtents[1], facep->mExtents[0]);
		size.mul(0.5f);
		size.add(LLVector4a(0.01f));
		drawBox(center, size);

#if 0
		S32 boost = imagep->getBoostLevel();
		if (boost > LLGLTexture::BOOST_NONE)
		{
			F32 t = (F32)boost / (F32)(LLGLTexture::BOOST_MAX_LEVEL - 1);
			LLVector4 col = lerp(boost_cold, boost_hot, t);
			LLGLEnable blend_on(GL_BLEND);
			gGL.blendFunc(GL_SRC_ALPHA, GL_ONE);
			gGL.diffuseColor4fv(col.mV);
			drawBox(center, size);
			gGL.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
#endif
	}
	stop_glerror();
}

void renderPoints(LLDrawable* drawablep)
{
	LLGLDepthTest depth(GL_FALSE, GL_FALSE);
	if (drawablep->getNumFaces())
	{
		gGL.begin(LLRender::POINTS);
		gGL.diffuseColor3f(1.f, 1.f, 1.f);
		for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
		{
			LLFace* face = drawablep->getFace(i);
			if (face)
			{
				gGL.vertex3fv(face->mCenterLocal.mV);
			}
		}
		gGL.end();
		stop_glerror();
	}
}

void renderTextureAnim(LLDrawInfo* params)
{
	if (!params->mTextureMatrix)
	{
		return;
	}

	LLGLEnable blend(GL_BLEND);
	gGL.diffuseColor4f(1.f, 1.f, 0.f, 0.5f);
	pushVerts(params, LLVertexBuffer::MAP_VERTEX);
	stop_glerror();
}

void renderBatchSize(LLDrawInfo* params)
{
	if (params->mTextureList.empty())
	{
		return;
	}
	LLGLEnable offset(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.f, 1.f);
	gGL.diffuseColor4ubv((GLubyte*)&(params->mDebugColor));
	pushVerts(params, LLVertexBuffer::MAP_VERTEX);
	stop_glerror();
}

void renderShadowFrusta(LLDrawInfo* params)
{
	LLGLEnable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_ADD);

	LLVector4a center;
	center.setAdd(params->mExtents[1], params->mExtents[0]);
	center.mul(0.5f);
	LLVector4a size;
	size.setSub(params->mExtents[1],params->mExtents[0]);
	size.mul(0.5f);

	if (gPipeline.mShadowCamera[4].AABBInFrustum(center, size))
	{
		gGL.diffuseColor3f(1.f, 0.f, 0.f);
		pushVerts(params, LLVertexBuffer::MAP_VERTEX);
	}
	if (gPipeline.mShadowCamera[5].AABBInFrustum(center, size))
	{
		gGL.diffuseColor3f(0.f, 1.f, 0.f);
		pushVerts(params, LLVertexBuffer::MAP_VERTEX);
	}
	if (gPipeline.mShadowCamera[6].AABBInFrustum(center, size))
	{
		gGL.diffuseColor3f(0.f, 0.f, 1.f);
		pushVerts(params, LLVertexBuffer::MAP_VERTEX);
	}
	if (gPipeline.mShadowCamera[7].AABBInFrustum(center, size))
	{
		gGL.diffuseColor3f(1.f, 0.f, 1.f);
		pushVerts(params, LLVertexBuffer::MAP_VERTEX);
	}

	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	stop_glerror();
}

void renderLights(LLDrawable* drawablep)
{
	if (!drawablep->isLight())
	{
		return;
	}

	if (drawablep->getNumFaces())
	{
		LLGLEnable blend(GL_BLEND);
		gGL.diffuseColor4f(0.f, 1.f, 1.f, 0.5f);

		for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
		{
			LLFace* face = drawablep->getFace(i);
			if (face)
			{
				pushVerts(face, LLVertexBuffer::MAP_VERTEX);
			}
		}

		const LLVector4a* ext = drawablep->getSpatialExtents();

		LLVector4a pos;
		pos.setAdd(ext[0], ext[1]);
		pos.mul(0.5f);
		LLVector4a size;
		size.setSub(ext[1], ext[0]);
		size.mul(0.5f);

		{
			LLGLDepthTest depth(GL_FALSE, GL_TRUE);
			gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
			drawBoxOutline(pos, size);
		}

		gGL.diffuseColor4f(1.f, 1.f, 0.f, 1.f);
		F32 rad = drawablep->getVOVolume()->getLightRadius();
		drawBoxOutline(pos, LLVector4a(rad));
		stop_glerror();
	}
}

class LLRenderOctreeRaycast final : public LLOctreeTriangleRayIntersect
{
public:
	LLRenderOctreeRaycast(const LLVector4a& start, const LLVector4a& dir,
						  F32* closest_t)
	:	LLOctreeTriangleRayIntersect(start, dir, NULL, closest_t, NULL, NULL,
									 NULL, NULL)
	{
	}

	void visit(const LLOctreeNode<LLVolumeTriangle>* branch) override
	{
		LLVolumeOctreeListener* vl = (LLVolumeOctreeListener*)branch->getListener(0);

		LLVector3 center, size;

		if (branch->isEmpty())
		{
			gGL.diffuseColor3f(1.f, 0.2f, 0.f);
			center.set(branch->getCenter().getF32ptr());
			size.set(branch->getSize().getF32ptr());
		}
		else if (vl)
		{
			gGL.diffuseColor3f(0.75f, 1.f, 0.f);
			center.set(vl->mBounds[0].getF32ptr());
			size.set(vl->mBounds[1].getF32ptr());
		}

		drawBoxOutline(center, size);

		for (U32 i = 0; i < 2; ++i)
		{
			LLGLDepthTest depth(GL_TRUE, GL_FALSE,
								i == 1 ? GL_LEQUAL : GL_GREATER);

			if (i == 1)
			{
				gGL.diffuseColor4f(0.f, 1.f, 1.f, 0.5f);
			}
			else
			{
				gGL.diffuseColor4f(0.f, 0.5f, 0.5f, 0.25f);
				drawBoxOutline(center, size);
			}

			if (i == 1)
			{
				gGL.flush();
				gGL.lineWidth(3.f);
			}

			gGL.begin(LLRender::TRIANGLES);
			for (LLOctreeNode<LLVolumeTriangle>::const_element_iter
					iter = branch->getDataBegin(), end = branch->getDataEnd();
				 iter != end; ++iter)
			{
				const LLVolumeTriangle* tri = *iter;

				gGL.vertex3fv(tri->mV[0]->getF32ptr());
				gGL.vertex3fv(tri->mV[1]->getF32ptr());
				gGL.vertex3fv(tri->mV[2]->getF32ptr());
			}
			gGL.end();

			if (i == 1)
			{
				gGL.flush();
				gGL.lineWidth(1.f);
			}
		}
	}
};

void renderRaycast(LLDrawable* drawablep)
{
	if (!drawablep->getNumFaces())
	{
		return;
	}

	LLGLEnable blend(GL_BLEND);
	gGL.diffuseColor4f(0.f, 1.f, 1.f, 0.5f);

	LLVOVolume* vobj = drawablep->getVOVolume();
	if (vobj && !vobj->isDead())
	{
#if 0
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		pushVerts(drawablep->getFace(gDebugRaycastFaceHit),
									 LLVertexBuffer::MAP_VERTEX);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

		LLVolume* volume = vobj->getVolume();

		bool transform = true;
		if (drawablep->isState(LLDrawable::RIGGED))
		{
			volume = vobj->getRiggedVolume();
			transform = false;
		}

		if (volume)
		{
			LLVector3 trans = drawablep->getRegion()->getOriginAgent();

			for (S32 i = 0, count = volume->getNumVolumeFaces(); i < count;
				 ++i)
			{
				const LLVolumeFace& face = volume->getVolumeFace(i);

				gGL.pushMatrix();
				gGL.translatef(trans.mV[0], trans.mV[1], trans.mV[2]);
				gGL.multMatrix(vobj->getRelativeXform().getF32ptr());

				LLVector4a start, end;
				if (transform)
				{
					LLVector3 v_start(gDebugRaycastStart.getF32ptr());
					LLVector3 v_end(gDebugRaycastEnd.getF32ptr());

					v_start = vobj->agentPositionToVolume(v_start);
					v_end = vobj->agentPositionToVolume(v_end);

					start.load3(v_start.mV);
					end.load3(v_end.mV);
				}
				else
				{
					start = gDebugRaycastStart;
					end = gDebugRaycastEnd;
				}

				LLVector4a dir;
				dir.setSub(end, start);

				gGL.flush();
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

				{
					// Render face positions
#if 0				// Do not use fixed functions any more. Use VBOs.
					LLVertexBuffer::unbind();
					gGL.diffuseColor4f(0.f, 1.f, 1.f, 0.5f);
					glVertexPointer(3, GL_FLOAT, sizeof(LLVector4a),
									face.mPositions);
					gGL.syncMatrices();
					glDrawElements(GL_TRIANGLES, face.mNumIndices,
								   GL_UNSIGNED_SHORT, face.mIndices);
#else
					gGL.diffuseColor4f(0.f, 1.f, 1.f, 0.5f);
					LLVertexBuffer::drawElements(LLRender::TRIANGLES,
												 face.mNumVertices,
												 face.mPositions, NULL,
												 face.mNumIndices,
												 face.mIndices);
#endif
				}

				if (!volume->isUnique())
				{
					F32 t = 1.f;

					if (!face.mOctree)
					{
						((LLVolumeFace*)&face)->createOctree();
					}

					LLRenderOctreeRaycast render(start, dir, &t);
					render.traverse(face.mOctree);
				}

				gGL.popMatrix();
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
		}
	}
	else if (drawablep->isAvatar())
	{
		if (drawablep->getVObj() == gDebugRaycastObject)
		{
			LLGLDepthTest depth(GL_FALSE);
			LLVOAvatar* av = (LLVOAvatar*) drawablep->getVObj().get();
			av->renderCollisionVolumes();
		}
	}

	if (drawablep->getVObj() == gDebugRaycastObject)
	{
		// Draw intersection point
		gGL.pushMatrix();
		gGL.loadMatrix(gGLModelView);
		LLVector3 translate(gDebugRaycastIntersection.getF32ptr());
		gGL.translatef(translate.mV[0], translate.mV[1], translate.mV[2]);
		LLCoordFrame orient;
		LLVector4a debug_binormal;
		debug_binormal.setCross3(gDebugRaycastNormal, gDebugRaycastTangent);
		debug_binormal.mul(gDebugRaycastTangent.getF32ptr()[3]);
		LLVector3 normal(gDebugRaycastNormal.getF32ptr());
		LLVector3 binormal(debug_binormal.getF32ptr());
		orient.lookDir(normal, binormal);
		LLMatrix4 rotation;
		orient.getRotMatrixToParent(rotation);
		gGL.multMatrix(rotation.getF32ptr());

		gGL.diffuseColor4f(1.f, 0.f, 0.f, 0.5f);
		drawBox(LLVector3::zero, LLVector3(0.1f, 0.022f, 0.022f));
		gGL.diffuseColor4f(0.f, 1.f, 0.f, 0.5f);
		drawBox(LLVector3::zero, LLVector3(0.021f, 0.1f, 0.021f));
		gGL.diffuseColor4f(0.f, 0.f, 1.f, 0.5f);
		drawBox(LLVector3::zero, LLVector3(0.02f, 0.02f, 0.1f));
		gGL.popMatrix();

		// Draw bounding box of prim
		const LLVector4a* ext = drawablep->getSpatialExtents();

		LLVector4a pos;
		pos.setAdd(ext[0], ext[1]);
		pos.mul(0.5f);
		LLVector4a size;
		size.setSub(ext[1], ext[0]);
		size.mul(0.5f);

		LLGLDepthTest depth(GL_FALSE, GL_TRUE);
		gGL.diffuseColor4f(0.f, 0.5f, 0.5f, 1.f);
		drawBoxOutline(pos, size);
	}
}

void renderAgentTarget(LLVOAvatar* avatar)
{
	// Render these for self only (why, I don't know)
	if (avatar->isSelf())
	{
		renderCrossHairs(avatar->getPositionAgent(), 0.2f,
						 LLColor4(1.f, 0.f, 0.f, 0.8f));
		renderCrossHairs(avatar->mDrawable->getPositionAgent(), 0.2f,
						 LLColor4(1.f, 0.f, 0.f, 0.8f));
		renderCrossHairs(avatar->mRoot->getWorldPosition(), 0.2f,
						 LLColor4(1.f, 1.f, 1.f, 0.8f));
		renderCrossHairs(avatar->mPelvisp->getWorldPosition(), 0.2f,
						 LLColor4(0.f, 0.f, 1.f, 0.8f));
	}
}

class LLOctreeRenderNonOccluded final : public OctreeTraveler
{
public:
	LLOctreeRenderNonOccluded(LLCamera* camera)
	:	mCamera(camera)
	{
	}

	void traverse(const OctreeNode* node) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)node->getListener(0);
		if (!group) return;

		const LLVector4a* bounds = group->getBounds();
		if (!mCamera || mCamera->AABBInFrustumNoFarClip(bounds[0], bounds[1]))
		{
			node->accept(this);

			for (U32 i = 0, count = node->getChildCount(); i < count; ++i)
			{
				traverse(node->getChild(i));
			}

			// Draw tight fit bounding boxes for spatial group
			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCTREE))
			{
				group->rebuildGeom();
				group->rebuildMesh();
				renderOctree(group);
			}
#if 0
			// Render visibility wireframe
			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCCLUSION))
			{
				group->rebuildGeom();
				group->rebuildMesh();

				gGL.flush();
				gGL.pushMatrix();
				gGLLastMatrix = NULL;
				gGL.loadMatrix(gGLModelView);
				renderVisibility(group, mCamera);
				gGLLastMatrix = NULL;
				gGL.popMatrix();
				gGL.diffuseColor4f(1, 1, 1, 1);
				stop_glerror();
			}
#endif
		}
	}

	void visit(const OctreeNode* branch) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)branch->getListener(0);
		if (!group) return;

		const LLVector4a* bounds = group->getBounds();
		if (group->hasState(LLSpatialGroup::GEOM_DIRTY) ||
			(mCamera &&
			 !mCamera->AABBInFrustumNoFarClip(bounds[0], bounds[1])))
		{
			return;
		}

		LLGLDisable stencil(GL_STENCIL_TEST);

		group->rebuildGeom();
		group->rebuildMesh();

		if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_BBOXES))
		{
			if (!group->isEmpty())
			{
				gGL.diffuseColor3f(0.f, 0.f, 1.f);
				const LLVector4a* obj_bounds = group->getObjectBounds();
				drawBoxOutline(obj_bounds[0], obj_bounds[1]);
			}
		}

		static LLCachedControl<bool> for_self_only(gSavedSettings,
												   "ShowAvatarDebugForSelfOnly");
		for (OctreeNode::const_element_iter i = branch->getDataBegin(),
											end = branch->getDataEnd();
			 i != end; ++i)
		{
			LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
			if (!drawable || drawable->isDead()) continue;

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_BBOXES))
			{
				renderBoundingBox(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_NORMALS))
			{
				renderNormals(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_BUILD_QUEUE))
			{
				if (drawable->isState(LLDrawable::IN_REBUILD_Q2))
				{
					gGL.diffuseColor4f(0.6f, 0.6f, 0.1f, 1.f);
					const LLVector4a* ext = drawable->getSpatialExtents();
					LLVector4a center;
					center.setAdd(ext[0], ext[1]);
					center.mul(0.5f);
					LLVector4a size;
					size.setSub(ext[1], ext[0]);
					size.mul(0.5f);
					drawBoxOutline(center, size);
				}
			}

			if (drawable->getVOVolume() &&
				gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY))
			{
				renderTexturePriority(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_POINTS))
			{
				renderPoints(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_LIGHTS))
			{
				renderLights(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_RAYCAST))
			{
				renderRaycast(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_UPDATE_TYPE))
			{
				renderUpdateType(drawable);
			}
#if 0		// *TODO
			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_RENDER_COMPLEXITY))
			{
				renderComplexityDisplay(drawable);
			}
			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXEL_DENSITY))
			{
				renderTexelDensity(drawable);
			}
#endif
			LLViewerObject* objectp = drawable->getVObj().get();
			LLVOAvatar* avatarp = objectp ? objectp->asAvatar() : NULL;
			if (avatarp && (!for_self_only || avatarp->isSelf()))
			{
				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_AVATAR_VOLUME))
				{
					avatarp->renderCollisionVolumes();
				}

				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_AVATAR_JOINTS))
				{
					avatarp->renderJoints();
					avatarp->renderBones();
				}

				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_AGENT_TARGET))
				{
					renderAgentTarget(avatarp);
				}
			}

			if (gDebugGL)
			{
				for (U32 i = 0, count = drawable->getNumFaces();
					 i < count; ++i)
				{
					LLFace* facep = drawable->getFace(i);
					if (facep && facep->mDrawInfo)
					{
						U8 index = facep->getTextureIndex();
						if (index < FACE_DO_NOT_BATCH_TEXTURES)
						{
							if (facep->mDrawInfo->mTextureList.size() <= index)
							{
								llwarns << "Face texture index out of bounds." << llendl;
							}
							else if (facep->mDrawInfo->mTextureList[index] != facep->getTexture())
							{
								llwarns << "Face texture index incorrect." << llendl;
							}
						}
					}
				}
			}
		}

		for (LLSpatialGroup::draw_map_t::iterator i = group->mDrawMap.begin(),
												  end = group->mDrawMap.end();
			 i != end; ++i)
		{
			LLSpatialGroup::drawmap_elem_t& draw_vec = i->second;
			for (LLSpatialGroup::drawmap_elem_t::iterator
					j = draw_vec.begin(), end2 = draw_vec.end();
				 j != end2; ++j)
			{
				LLDrawInfo* draw_info = *j;
				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_ANIM))
				{
					renderTextureAnim(draw_info);
				}
				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_BATCH_SIZE))
				{
					renderBatchSize(draw_info);
				}
				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA))
				{
					renderShadowFrusta(draw_info);
				}
			}
		}
	}

public:
	LLCamera* mCamera;
};

class LLOctreeRenderXRay final : public OctreeTraveler
{
public:
	LLOctreeRenderXRay(LLCamera* camera): mCamera(camera) {}

	void traverse(const OctreeNode* node) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)node->getListener(0);
		if (!group) return;

		const LLVector4a* bounds = group->getBounds();
		if (!mCamera ||
			mCamera->AABBInFrustumNoFarClip(bounds[0], bounds[1]))
		{
			node->accept(this);

			for (U32 i = 0, count = node->getChildCount(); i < count; ++i)
			{
				traverse(node->getChild(i));
			}

			// Render visibility wireframe
			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCCLUSION))
			{
				group->rebuildGeom();
				group->rebuildMesh();

				gGL.flush();
				gGL.pushMatrix();
				gGLLastMatrix = NULL;
				gGL.loadMatrix(gGLModelView);
				renderXRay(group, mCamera);
				gGLLastMatrix = NULL;
				gGL.popMatrix();
				stop_glerror();
			}
		}
	}

	LL_INLINE void visit(const OctreeNode* node) override
	{
	}

public:
	LLCamera* mCamera;
};

class LLOctreePushBBoxVerts final : public OctreeTraveler
{
public:
	LLOctreePushBBoxVerts(LLCamera* camera)
	:	mCamera(camera)
	{
	}

	void traverse(const OctreeNode* node) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)node->getListener(0);
		if (!group) return;

		const LLVector4a* bounds = group->getBounds();
		if (!mCamera ||
			mCamera->AABBInFrustum(bounds[0],bounds[1]))
		{
			node->accept(this);

			for (U32 i = 0; i < node->getChildCount(); ++i)
			{
				traverse(node->getChild(i));
			}
		}
	}

	void visit(const OctreeNode* branch) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)branch->getListener(0);
		if (!group) return;

		const LLVector4a* bounds = group->getBounds();
		if (group->hasState(LLSpatialGroup::GEOM_DIRTY) ||
			(mCamera &&
			 !mCamera->AABBInFrustumNoFarClip(bounds[0], bounds[1])))
		{
			return;
		}

		for (OctreeNode::const_element_iter i = branch->getDataBegin(),
											end = branch->getDataEnd();
			 i != end; ++i)
		{
			LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
			if (drawable)
			{
				renderBoundingBox(drawable, false);
			}
		}
	}

public:
	LLCamera* mCamera;
};

void LLSpatialPartition::renderIntersectingBBoxes(LLCamera* camera)
{
	LLOctreePushBBoxVerts pusher(camera);
	pusher.traverse(mOctree);
}

class LLOctreeStateCheck final : public OctreeTraveler
{
protected:
	LOG_CLASS(LLOctreeStateCheck);

public:
	LLOctreeStateCheck()
	{
		for (U32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
		{
			mInheritedMask[i] = 0;
		}
	}

	void traverse(const OctreeNode* node) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)node->getListener(0);
		if (!group) return;

		node->accept(this);

		U32 temp[LLViewerCamera::NUM_CAMERAS];

		for (U32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
		{
			temp[i] = mInheritedMask[i];
			mInheritedMask[i] |= group->mOcclusionState[i] &
								 LLSpatialGroup::OCCLUDED;
		}

		for (U32 i = 0; i < node->getChildCount(); ++i)
		{
			traverse(node->getChild(i));
		}

		for (U32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
		{
			mInheritedMask[i] = temp[i];
		}
	}

	void visit(const OctreeNode* state) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)state->getListener(0);
		if (!group) return;

		for (U32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
		{
			if (mInheritedMask[i] &&
				!(group->mOcclusionState[i] & mInheritedMask[i]))
			{
				llerrs << "Spatial group failed inherited mask test."
					   << llendl;
			}
		}

		if (group->hasState(LLSpatialGroup::DIRTY))
		{
			assert_parent_state(group, LLSpatialGroup::DIRTY);
		}
	}

	void assert_parent_state(LLSpatialGroup* group, U32 state)
	{
		LLSpatialGroup* parent = group->getParent();
		while (parent)
		{
			if (!parent->hasState(state))
			{
				llerrs << "Spatial group failed parent state check." << llendl;
			}
			parent = parent->getParent();
		}
	}

public:
	U32 mInheritedMask[LLViewerCamera::NUM_CAMERAS];
};

S32 get_physics_detail(const LLVolumeParams& volume_params,
					   const LLVector3& scale)
{
	constexpr S32 DEFAULT_DETAIL = 1;
	constexpr F32 LARGE_THRESHOLD = 5.f;
	constexpr F32 MEGA_THRESHOLD = 25.f;

	S32 detail = DEFAULT_DETAIL;
	F32 avg_scale = (scale[0] + scale[1] + scale[2]) / 3.f;

	if (avg_scale > LARGE_THRESHOLD)
	{
		++detail;
		if (avg_scale > MEGA_THRESHOLD)
		{
			++detail;
		}
	}

	return detail;
}

void renderMeshBaseHull(LLVOVolume* volume, U32 data_mask, LLColor4& color,
						LLColor4& line_color)
{
	LLUUID mesh_id = volume->getVolume()->getParams().getSculptID();
	LLModel::Decomposition* decomp = gMeshRepo.getDecomposition(mesh_id);

	static const LLVector3 size(0.25f, 0.25f, 0.25f);

	if (decomp)
	{
		if (!decomp->mBaseHullMesh.empty())
		{
			gGL.diffuseColor4fv(color.mV);
			LLVertexBuffer::drawArrays(LLRender::TRIANGLES,
									   decomp->mBaseHullMesh.mPositions);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			gGL.diffuseColor4fv(line_color.mV);
			LLVertexBuffer::drawArrays(LLRender::TRIANGLES,
									   decomp->mBaseHullMesh.mPositions);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
		else
		{
			gMeshRepo.buildPhysicsMesh(*decomp);
			gGL.diffuseColor4f(0.f, 1.f, 1.f, 1.f);
			drawBoxOutline(LLVector3::zero, size);
		}
	}
	else
	{
		gGL.diffuseColor3f(1.f, 0.f, 1.f);
		drawBoxOutline(LLVector3::zero, size);
	}
}

void render_hull(LLModel::PhysicsMesh& mesh, const LLColor4& color,
				 const LLColor4& line_color)
{
	if (mesh.mPositions.empty() || mesh.mNormals.empty())
	{
		return;
	}
	gGL.diffuseColor4fv(color.mV);
	LLVertexBuffer::drawArrays(LLRender::TRIANGLES, mesh.mPositions);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	gGL.lineWidth(3.f);
	gGL.diffuseColor4fv(line_color.mV);
	LLVertexBuffer::drawArrays(LLRender::TRIANGLES, mesh.mPositions);
	gGL.lineWidth(1.f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void renderPhysicsShape(LLDrawable* drawable, LLVOVolume* volume)
{
	U8 physics_type = volume->getPhysicsShapeType();
	if (physics_type == LLViewerObject::PHYSICS_SHAPE_NONE ||
		volume->isFlexible())
	{
		return;
	}

	// Not allowed to return at this point without rendering *something*

	static LLCachedControl<F32> threshold(gSavedSettings, "ObjectCostHighThreshold");
	F32 cost = volume->getObjectCost();

	static LLCachedControl<LLColor4> low(gSavedSettings, "ObjectCostLowColor");
	static LLCachedControl<LLColor4> mid(gSavedSettings, "ObjectCostMidColor");
	static LLCachedControl<LLColor4> high(gSavedSettings, "ObjectCostHighColor");

	F32 normalized_cost = 1.f - expf(-cost / threshold);

	LLColor4 color;
	if (normalized_cost <= 0.5f)
	{
		color = lerp(low, mid, 2.f * normalized_cost);
	}
	else
	{
		color = lerp(mid, high, 2.f * (normalized_cost - 0.5f));
	}

	LLColor4 line_color = color * 0.5f;

	U32 data_mask = LLVertexBuffer::MAP_VERTEX;

	LLVolumeParams volume_params = volume->getVolume()->getParams();

	LLPhysicsVolumeParams physics_params(volume_params,
		physics_type == LLViewerObject::PHYSICS_SHAPE_CONVEX_HULL);

	LLPhysicsShapeBuilderUtil::PhysicsShapeSpecification physics_spec;
	LLPhysicsShapeBuilderUtil::determinePhysicsShape(physics_params,
													 volume->getScale(),
													 physics_spec);

	U32 type = physics_spec.getType();

	LLVector3 size(0.25f, 0.25f, 0.25f);

	gGL.pushMatrix();
	gGL.multMatrix(volume->getRelativeXform().getF32ptr());

	LLGLEnable(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(3.f, 3.f);

	if (type == LLPhysicsShapeBuilderUtil::PhysicsShapeSpecification::USER_MESH)
	{
		LLUUID mesh_id = volume->getVolume()->getParams().getSculptID();
		LLModel::Decomposition* decomp = gMeshRepo.getDecomposition(mesh_id);
		if (decomp)
		{
			// Render a physics based mesh

			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

			if (!decomp->mHull.empty())
			{
				// Decomposition exists, use that
				if (decomp->mMesh.empty())
				{
					gMeshRepo.buildPhysicsMesh(*decomp);
				}

				for (U32 i = 0; i < decomp->mMesh.size(); ++i)
				{
					render_hull(decomp->mMesh[i], color, line_color);
				}
			}
			else if (!decomp->mPhysicsShapeMesh.empty())
			{
				// Decomp has physics mesh, render that mesh
				gGL.diffuseColor4fv(color.mV);
				LLVertexBuffer::drawArrays(LLRender::TRIANGLES,
										   decomp->mPhysicsShapeMesh.mPositions);

				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				gGL.diffuseColor4fv(line_color.mV);
				LLVertexBuffer::drawArrays(LLRender::TRIANGLES,
										   decomp->mPhysicsShapeMesh.mPositions);
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
			else
			{
				// No mesh or decomposition, render base hull
				renderMeshBaseHull(volume, data_mask, color, line_color);

				if (decomp->mPhysicsShapeMesh.empty())
				{
					// Attempt to fetch physics shape mesh if available
					gMeshRepo.fetchPhysicsShape(mesh_id);
				}
			}
		}
		else
		{
			gGL.diffuseColor3f(1.f, 1.f, 0.f);
			drawBoxOutline(LLVector3::zero, size);
		}
	}
	else if (type == LLPhysicsShapeBuilderUtil::PhysicsShapeSpecification::USER_CONVEX ||
			 type == LLPhysicsShapeBuilderUtil::PhysicsShapeSpecification::PRIM_CONVEX)
	{
		if (volume->isMesh())
		{
			renderMeshBaseHull(volume, data_mask, color, line_color);
		}
		else
		{
			LLVolumeParams volume_params = volume->getVolume()->getParams();
			S32 detail = get_physics_detail(volume_params, volume->getScale());
			LLVolume* phys_volume = gVolumeMgrp->refVolume(volume_params,
														   detail);
			if (!phys_volume->mHullPoints)
			{
				// Build convex hull
				std::vector<LLVector3> pos;
				std::vector<U16> index;

				S32 index_offset = 0;

				for (S32 i = 0, count = phys_volume->getNumVolumeFaces();
					 i < count; ++i)
				{
					const LLVolumeFace& face = phys_volume->getVolumeFace(i);
					if (index_offset + face.mNumVertices > 65535)
					{
						continue;
					}

					for (S32 j = 0; j < face.mNumVertices; ++j)
					{
						pos.emplace_back(face.mPositions[j].getF32ptr());
					}

					for (S32 j = 0; j < face.mNumIndices; ++j)
					{
						index.push_back(face.mIndices[j] + index_offset);
					}

					index_offset += face.mNumVertices;
				}

				LLConvexDecomposition* decomp =
					LLConvexDecomposition::getInstance();
				if (decomp && !pos.empty() && !index.empty())
				{
					LLCDMeshData mesh;
					mesh.mIndexBase = &index[0];
					mesh.mVertexBase = pos[0].mV;
					mesh.mNumVertices = pos.size();
					mesh.mVertexStrideBytes = 12;
					mesh.mIndexStrideBytes = 6;
					mesh.mIndexType = LLCDMeshData::INT_16;

					mesh.mNumTriangles = index.size() / 3;

					LLCDMeshData res;
					decomp->generateSingleHullMeshFromMesh(&mesh, &res);

					// Copy res into phys_volume
					phys_volume->mHullPoints =
						(LLVector4a*)allocate_volume_mem(sizeof(LLVector4a) *
														 res.mNumVertices);
					if (!phys_volume->mHullPoints)
					{
						gVolumeMgrp->unrefVolume(phys_volume);
						gGL.popMatrix();
						return;
					}
					phys_volume->mNumHullPoints = res.mNumVertices;

					S32 idx_size = (res.mNumTriangles * 3 * 2 + 0xF) & ~0xF;
					phys_volume->mHullIndices =
						(U16*)allocate_volume_mem(idx_size);
					if (!phys_volume->mHullIndices)
					{
						free_volume_mem(phys_volume->mHullPoints);
						gVolumeMgrp->unrefVolume(phys_volume);
						gGL.popMatrix();
						return;
					}
					phys_volume->mNumHullIndices = res.mNumTriangles * 3;

					const F32* v = res.mVertexBase;

					for (S32 i = 0; i < res.mNumVertices; ++i)
					{
						F32* p = (F32*)((U8*)v+i*res.mVertexStrideBytes);
						phys_volume->mHullPoints[i].load3(p);
					}

					if (res.mIndexType == LLCDMeshData::INT_16)
					{
						for (S32 i = 0; i < res.mNumTriangles; ++i)
						{
							U16* idx = (U16*)(((U8*)res.mIndexBase) +
											  i * res.mIndexStrideBytes);

							phys_volume->mHullIndices[i * 3] = idx[0];
							phys_volume->mHullIndices[i * 3 + 1] = idx[1];
							phys_volume->mHullIndices[i * 3 + 2] = idx[2];
						}
					}
					else
					{
						for (S32 i = 0; i < res.mNumTriangles; ++i)
						{
							U32* idx = (U32*)(((U8*)res.mIndexBase) +
											  i * res.mIndexStrideBytes);

							phys_volume->mHullIndices[i * 3] = (U16)idx[0];
							phys_volume->mHullIndices[i * 3 + 1] = (U16)idx[1];
							phys_volume->mHullIndices[i * 3 + 2] = (U16)idx[2];
						}
					}
				}
			}

			if (phys_volume->mHullPoints && phys_volume->mNumHullIndices &&
				phys_volume->mHullIndices && phys_volume->mNumHullPoints)
			{
				// Render hull

				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

				gGL.diffuseColor4fv(line_color.mV);
				LLVertexBuffer::unbind();

				llassert(LLGLSLShader::sCurBoundShader != 0);

				LLVertexBuffer::drawElements(LLRender::TRIANGLES,
											 phys_volume->mNumHullPoints,
											 phys_volume->mHullPoints, NULL,
											 phys_volume->mNumHullIndices,
											 phys_volume->mHullIndices);

				gGL.diffuseColor4fv(color.mV);
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				LLVertexBuffer::drawElements(LLRender::TRIANGLES,
											 phys_volume->mNumHullPoints,
											 phys_volume->mHullPoints, NULL,
											 phys_volume->mNumHullIndices,
											 phys_volume->mHullIndices);
			}
			else
			{
				gGL.diffuseColor4f(1.f, 0.1f, 1.f, 1.f);
				drawBoxOutline(LLVector3::zero, size);
			}

			free_volume_mem(phys_volume->mHullPoints);
			free_volume_mem(phys_volume->mHullIndices);
			gVolumeMgrp->unrefVolume(phys_volume);
		}
	}
	else if (type == LLPhysicsShapeBuilderUtil::PhysicsShapeSpecification::BOX)
	{
		LLVector3 center = physics_spec.getCenter();
		LLVector3 scale = physics_spec.getScale();
		LLVector3 vscale = volume->getScale() * 2.f;
		scale.set(scale[0] / vscale[0], scale[1] / vscale[1],
				  scale[2] / vscale[2]);

		gGL.diffuseColor4fv(color.mV);
		drawBox(center, scale);
	}
	else if	(type == LLPhysicsShapeBuilderUtil::PhysicsShapeSpecification::SPHERE)
	{
		LLVolumeParams volume_params;
		volume_params.setType(LL_PCODE_PROFILE_CIRCLE_HALF,
							  LL_PCODE_PATH_CIRCLE);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(1.f, 1.f);
		volume_params.setShear(0.f, 0.f);
		LLVolume* sphere = gVolumeMgrp->refVolume(volume_params, 3);

		gGL.diffuseColor4fv(color.mV);
		pushVerts(sphere);
		gVolumeMgrp->unrefVolume(sphere);
	}
	else if (type == LLPhysicsShapeBuilderUtil::PhysicsShapeSpecification::CYLINDER)
	{
		LLVolumeParams volume_params;
		volume_params.setType(LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_LINE);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(1.f, 1.f);
		volume_params.setShear(0.f, 0.f);
		LLVolume* cylinder = gVolumeMgrp->refVolume(volume_params, 3);

		gGL.diffuseColor4fv(color.mV);
		pushVerts(cylinder);
		gVolumeMgrp->unrefVolume(cylinder);
	}
	else if (type == LLPhysicsShapeBuilderUtil::PhysicsShapeSpecification::PRIM_MESH)
	{
		LLVolumeParams volume_params = volume->getVolume()->getParams();
		S32 detail = get_physics_detail(volume_params, volume->getScale());

		LLVolume* phys_volume = gVolumeMgrp->refVolume(volume_params, detail);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		gGL.diffuseColor4fv(line_color.mV);
		pushVerts(phys_volume);

		gGL.diffuseColor4fv(color.mV);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		pushVerts(phys_volume);
		gVolumeMgrp->unrefVolume(phys_volume);
	}
	else if (type == LLPhysicsShapeBuilderUtil::PhysicsShapeSpecification::PRIM_CONVEX)
	{
		LLVolumeParams volume_params = volume->getVolume()->getParams();
		S32 detail = get_physics_detail(volume_params, volume->getScale());

		LLVolume* phys_volume = gVolumeMgrp->refVolume(volume_params, detail);

		if (phys_volume->mHullPoints && phys_volume->mHullIndices)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			gGL.diffuseColor4fv(line_color.mV);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			LLVertexBuffer::drawElements(LLRender::TRIANGLES,
										 phys_volume->mNumHullPoints,
										 phys_volume->mHullPoints, NULL,
										 phys_volume->mNumHullIndices,
										 phys_volume->mHullIndices);

			gGL.diffuseColor4fv(color.mV);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			LLVertexBuffer::drawElements(LLRender::TRIANGLES,
										 phys_volume->mNumHullPoints,
										 phys_volume->mHullPoints, NULL,
										 phys_volume->mNumHullIndices,
										 phys_volume->mHullIndices);
		}
		else
		{
			gGL.diffuseColor3f(1.f, 0.f, 1.f);
			drawBoxOutline(LLVector3::zero, size);
			gMeshRepo.buildHull(volume_params, detail);
		}
		gVolumeMgrp->unrefVolume(phys_volume);
	}
	else if (type == LLPhysicsShapeBuilderUtil::PhysicsShapeSpecification::SCULPT)
	{
		// *TODO: implement sculpted prim physics display
	}
	else
	{
		llerrs << "Unhandled type" << llendl;
	}

	gGL.popMatrix();
}

void renderPhysicsShapes(LLSpatialGroup* group)
{
	for (OctreeNode::const_element_iter i = group->getDataBegin(),
										end = group->getDataEnd();
		 i != end; ++i)
	{
		LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
		if (!drawable)
		{
			llwarns_once << "NULL drawable found in spatial group: "
						 << std::hex << group << std::dec << llendl;
			continue;
		}
		if (drawable->isSpatialBridge())
		{
			LLSpatialBridge* bridge = drawable->asPartition()->asBridge();
			if (bridge)
			{
				gGL.pushMatrix();
				gGL.multMatrix(bridge->mDrawable->getRenderMatrix().getF32ptr());
				bridge->renderPhysicsShapes();
				gGL.popMatrix();
			}
		}
		else
		{
			LLVOVolume* volume = drawable->getVOVolume();
			if (volume && !volume->isAttachment() &&
				volume->getPhysicsShapeType() != LLViewerObject::PHYSICS_SHAPE_NONE)
			{
				if (!group->getSpatialPartition()->isBridge())
				{
					gGL.pushMatrix();
					LLVector3 trans = drawable->getRegion()->getOriginAgent();
					gGL.translatef(trans.mV[0], trans.mV[1], trans.mV[2]);
					renderPhysicsShape(drawable, volume);
					gGL.popMatrix();
				}
				else
				{
					renderPhysicsShape(drawable, volume);
				}
			}
			else
			{
				LLViewerObject* object = drawable->getVObj();
				if (object &&
					object->getPCode() == LLViewerObject::LL_VO_SURFACE_PATCH)
				{
					gGL.pushMatrix();
					gGL.multMatrix(object->getRegion()->mRenderMatrix.getF32ptr());
					// Push face vertices for terrain
					for (S32 i = 0, count = drawable->getNumFaces(); i < count;
						 ++i)
					{
						LLFace* face = drawable->getFace(i);
						if (face)
						{
							LLVertexBuffer* buff = face->getVertexBuffer();
							if (buff)
							{
								glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

								buff->setBuffer(LLVertexBuffer::MAP_VERTEX);
								gGL.diffuseColor3f(0.2f, 0.5f, 0.3f);
								buff->draw(LLRender::TRIANGLES,
										   buff->getNumIndices(), 0);

								gGL.diffuseColor3f(0.2f, 1.f, 0.3f);
								glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
								buff->draw(LLRender::TRIANGLES,
										   buff->getNumIndices(), 0);
							}
						}
					}
					gGL.popMatrix();
				}
			}
		}
	}
}

class LLOctreeRenderPhysicsShapes final : public OctreeTraveler
{
public:
	LLOctreeRenderPhysicsShapes(LLCamera* camera)
	:	mCamera(camera)
	{
	}

	void traverse(const OctreeNode* node) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)node->getListener(0);
		if (!group) return;

		const LLVector4a* bounds = group->getBounds();
		if (!mCamera ||
			mCamera->AABBInFrustumNoFarClip(bounds[0], bounds[1]))
		{
			node->accept(this);

			for (U32 i = 0; i < node->getChildCount(); ++i)
			{
				traverse(node->getChild(i));
			}

			group->rebuildGeom();
			group->rebuildMesh();

			renderPhysicsShapes(group);
		}
	}

	LL_INLINE void visit(const OctreeNode* branch) override
	{
	}

public:
	LLCamera* mCamera;
};

void LLSpatialPartition::renderPhysicsShapes()
{
	LLSpatialBridge* bridge = asBridge();
	LLCamera* camera = &gViewerCamera;

	if (bridge)
	{
		camera = NULL;
	}

	gGL.flush();
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.lineWidth(3.f);
	LLOctreeRenderPhysicsShapes render_physics(camera);
	render_physics.traverse(mOctree);
	gGL.flush();
	gGL.lineWidth(1.f);
}

void LLSpatialPartition::renderDebug()
{
	if (!gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCTREE |
									  LLPipeline::RENDER_DEBUG_OCCLUSION |
									  LLPipeline::RENDER_DEBUG_LIGHTS |
									  LLPipeline::RENDER_DEBUG_BATCH_SIZE |
									  LLPipeline::RENDER_DEBUG_UPDATE_TYPE |
									  LLPipeline::RENDER_DEBUG_BBOXES |
									  LLPipeline::RENDER_DEBUG_NORMALS |
									  LLPipeline::RENDER_DEBUG_POINTS |
									  LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY |
									  LLPipeline::RENDER_DEBUG_TEXTURE_ANIM |
									  LLPipeline::RENDER_DEBUG_RAYCAST |
									  LLPipeline::RENDER_DEBUG_AVATAR_VOLUME |
									  LLPipeline::RENDER_DEBUG_AVATAR_JOINTS |
									  LLPipeline::RENDER_DEBUG_AGENT_TARGET |
									  //LLPipeline::RENDER_DEBUG_BUILD_QUEUE |
									  LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA |
									  LLPipeline::RENDER_DEBUG_RENDER_COMPLEXITY |
									  LLPipeline::RENDER_DEBUG_TEXEL_DENSITY))
	{
		return;
	}
//MK
	if (gRLenabled && gRLInterface.mVisionRestricted)
	{
		return;
	}
//mk

	gDebugProgram.bind();

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY))
	{
#if 0
		sLastMaxTexPriority = lerp(sLastMaxTexPriority,
								   sCurMaxTexPriority,
								   gFrameIntervalSeconds);
#else
		sLastMaxTexPriority = (F32)gViewerCamera.getScreenPixelArea();
#endif
		sCurMaxTexPriority = 0.f;
	}

	LLGLDisable cullface(GL_CULL_FACE);
	LLGLEnable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gPipeline.disableLights();

	LLSpatialBridge* bridge = asBridge();
	LLCamera* camera = &gViewerCamera;

	if (bridge)
	{
		camera = NULL;
	}

	LLOctreeStateCheck checker;
	checker.traverse(mOctree);

	LLOctreeRenderNonOccluded render_debug(camera);
	render_debug.traverse(mOctree);

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCCLUSION))
	{
		LLGLEnable cull(GL_CULL_FACE);

		LLGLEnable blend(GL_BLEND);
		LLGLDepthTest depth_under(GL_TRUE, GL_FALSE, GL_GREATER);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		gGL.diffuseColor4f(0.5f, 0.f, 0.f, 0.25f);

		LLGLEnable offset(GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(-1.f, -1.f);

		LLOctreeRenderXRay xray(camera);
		xray.traverse(mOctree);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	gDebugProgram.unbind();

	stop_glerror();
}

void LLSpatialGroup::drawObjectBox(LLColor4 col)
{
	gGL.diffuseColor4fv(col.mV);
	LLVector4a size;
	size = mObjectBounds[1];
	size.mul(1.01f);
	size.add(LLVector4a(0.001f));
	drawBox(mObjectBounds[0], size);
}

bool LLSpatialPartition::isHUDPartition()
{
	return mPartitionType == LLViewerRegion::PARTITION_HUD;
}

bool LLSpatialPartition::isVisible(const LLVector3& v)
{
	return gViewerCamera.sphereInFrustum(v, 4.f) != 0;
}

class alignas(16) LLOctreeIntersect final
:	public LLOctreeTraveler<LLViewerOctreeEntry>
{
protected:
	LOG_CLASS(LLOctreeIntersect);

public:
	LLOctreeIntersect(const LLVector4a& start, const LLVector4a& end,
					  bool pick_transparent, bool pick_rigged, S32* face_hit,
					  LLVector4a* intersection, LLVector2* tex_coord,
					  LLVector4a* normal, LLVector4a* tangent)
	:	mStart(start),
		mEnd(end),
		mFaceHit(face_hit),
		mIntersection(intersection),
		mTexCoord(tex_coord),
		mNormal(normal),
		mTangent(tangent),
		mHit(NULL),
		mPickTransparent(pick_transparent),
		mPickRigged(pick_rigged)
	{
	}

	void visit(const OctreeNode* branch) override
	{
		for (OctreeNode::const_element_iter i = branch->getDataBegin(),
											end = branch->getDataEnd();
			 i != end; ++i)
		{
			check(*i);
		}
	}

	LLDrawable* check(const OctreeNode* node)
	{
		if (!node)
		{
			llwarns << "NULL node passed to LLOctreeIntersect::check()"
					<< llendl;
			return NULL;
		}

		node->accept(this);

		LLMatrix4a local_matrix4a;
		for (U32 i = 0; i < node->getChildCount(); ++i)
		{
			const OctreeNode* child = node->getChild(i);
			if (!child)
			{
				llwarns << "NULL spatial partition for node " << node
						<< llendl;
				continue;
			}

			LLSpatialGroup* group = (LLSpatialGroup*)child->getListener(0);
			if (!group)
			{
				llwarns << "NULL spatial group for child " << child
						<< " of node " << node << llendl;
				continue;
			}

			const LLVector4a* bounds = group->getBounds();
			LLVector4a size = bounds[1];
			LLVector4a center = bounds[0];

			LLVector4a local_start = mStart;
			LLVector4a local_end = mEnd;

			LLSpatialPartition* part = group->getSpatialPartition();
			if (part)
			{
				if (part->isBridge())
				{
					LLDrawable* drawable = part->asBridge()->mDrawable;
					if (drawable)
					{
						LLMatrix4 local_matrix = drawable->getRenderMatrix();
						local_matrix.invert();

						local_matrix4a.loadu(local_matrix);
						local_matrix4a.affineTransform(mStart, local_start);
						local_matrix4a.affineTransform(mEnd, local_end);
					}
					else
					{
						llwarns << "NULL drawable for spatial partition bridge of group "
								<< group << " of child " << child
								<< " of node " << node << llendl;
					}
				}
			}
			else
			{
				llwarns << "NULL spatial partition for group " << group
						<< " of child " << child << " of node " << node
						<< llendl;
			}

			if (LLLineSegmentBoxIntersect(local_start, local_end, center,
										  size))
			{
				check(child);
			}
		}

		return mHit;
	}

	bool check(LLViewerOctreeEntry* entry)
	{
		LLDrawable* drawable = (LLDrawable*)entry->getDrawable();
		if (!drawable || !gPipeline.hasRenderType(drawable->getRenderType()) ||
			!drawable->isVisible())
		{
			return false;
		}

		if (drawable->isSpatialBridge())
		{
			LLSpatialPartition* part = drawable->asPartition();
			if (part)
			{
				LLSpatialBridge* bridge = part->asBridge();
				if (bridge && gPipeline.hasRenderType(bridge->mDrawableType))
				{
					check(part->mOctree);
				}
			}
			else
			{
				llwarns << "NULL spatial partition for drawable " << drawable
						<< llendl;
			}
		}
		else
		{
			LLViewerObject* vobj = drawable->getVObj();
			if (vobj)
			{
				LLVector4a intersection;
				bool skip_check = false;
				if (vobj->isAvatar())
				{
					LLVOAvatar* av = (LLVOAvatar*)vobj;
					if (mPickRigged ||
						(av->isSelf() && LLFloaterTools::isVisible()))
					{
						LLViewerObject* hit =
							av->lineSegmentIntersectRiggedAttachments(mStart,
																	  mEnd, -1,
																	  mPickTransparent,
																	  mPickRigged,
																	  mFaceHit,
																	  &intersection,
																	  mTexCoord,
																	  mNormal,
																	  mTangent);
						if (hit)
						{
							mEnd = intersection;
							if (mIntersection)
							{
								*mIntersection = intersection;
							}
							mHit = hit->mDrawable;
							skip_check = true;
						}
					}
				}
				if (!skip_check &&
					vobj->lineSegmentIntersect(mStart, mEnd, -1,
											   mPickTransparent, mPickRigged,
											   mFaceHit, &intersection,
											   mTexCoord, mNormal, mTangent))
				{
					// Shorten the ray so we only find CLOSER hits:
					mEnd = intersection;

					if (mIntersection)
					{
						*mIntersection = intersection;
					}
					mHit = vobj->mDrawable;
				}
			}
		}

		return false;
	}

public:
	LLVector4a	mStart;
	LLVector4a	mEnd;
	S32*		mFaceHit;
	LLVector4a*	mIntersection;
	LLVector2*	mTexCoord;
	LLVector4a*	mNormal;
	LLVector4a*	mTangent;
	LLDrawable*	mHit;
	bool		mPickTransparent;
	bool		mPickRigged;
};

LLDrawable* LLSpatialPartition::lineSegmentIntersect(const LLVector4a& start,
													 const LLVector4a& end,
													 bool pick_transparent,
													 bool pick_rigged,
													 S32* face_hit,
													 LLVector4a* intersection,
													 LLVector2* tex_coord,
													 LLVector4a* normal,
													 LLVector4a* tangent)
{
	LLOctreeIntersect intersect(start, end, pick_transparent, pick_rigged,
								face_hit, intersection, tex_coord, normal,
								tangent);
	LLDrawable* drawable = intersect.check(mOctree);
	return drawable;
}

///////////////////////////////////////////////////////////////////////////////
// LLDrawInfo class
///////////////////////////////////////////////////////////////////////////////

LLDrawInfo::LLDrawInfo(U16 start, U16 end, U32 count, U32 offset,
					   LLViewerTexture* texture, LLVertexBuffer* buffer,
					   bool fullbright, U8 bump, bool particle, F32 part_size)
:	mVertexBuffer(buffer),
	mTexture(texture),
	mTextureMatrix(NULL),
	mModelMatrix(NULL),
	mStart(start),
	mEnd(end),
	mCount(count),
	mOffset(offset),
	mFullbright(fullbright),
	mBump(bump),
	mParticle(particle),
	mPartSize(part_size),
	mVSize(0.f),
	mGroup(NULL),
	mFace(NULL),
	mDistance(0.f),
	mDrawMode(LLRender::TRIANGLES),
	mBlendFuncSrc(LLRender::BF_SOURCE_ALPHA),
	mBlendFuncDst(LLRender::BF_ONE_MINUS_SOURCE_ALPHA),
	mHasGlow(false),
	mMaterial(NULL),
	mShaderMask(0),
	mSpecColor(1.f, 1.f, 1.f, 0.5f),
	mEnvIntensity(0.f),
	mAlphaMaskCutoff(0.5f),
	mDiffuseAlphaMode(0)
{
	mVertexBuffer->validateRange(mStart, mEnd, mCount, mOffset);
	mDebugColor = (rand() << 16) + rand();
}

LLDrawInfo::~LLDrawInfo()
{
	if (mFace)
	{
		mFace->setDrawInfo(NULL);
	}

#if LL_DEBUG
	if (gDebugGL)
	{
		gPipeline.checkReferences(this);
	}
#endif
}

void LLDrawInfo::validate()
{
	if (!mVertexBuffer->validateRange(mStart, mEnd, mCount, mOffset))
	{
#if LL_OCTREE_PARANOIA_CHECK
		llerrs << "Invalid range !" << llendl;
#else
		llwarns << "Invalid range !" << llendl;
#endif
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLGeometryManager class
///////////////////////////////////////////////////////////////////////////////

LLVertexBuffer* LLGeometryManager::createVertexBuffer(U32 type_mask, U32 usage)
{
	return new LLVertexBuffer(type_mask, usage);
}

///////////////////////////////////////////////////////////////////////////////
// LLCullResult class
///////////////////////////////////////////////////////////////////////////////

void LLCullResult::clear()
{
	mVisibleGroups.clear();
	mAlphaGroups.clear();
	mOcclusionGroups.clear();
	mDrawableGroups.clear();
	mVisibleList.clear();
	mVisibleBridge.clear();

	for (U32 i = 0; i < LLRenderPass::NUM_RENDER_TYPES; ++i)
	{
		mRenderMap[i].clear();
	}
}

void LLCullResult::pushDrawInfo(U32 type, LLDrawInfo* draw_info)
{
	if (type < LLRenderPass::NUM_RENDER_TYPES)
	{
		mRenderMap[type].push_back(draw_info);
		return;
	}
	llwarns << "Render type: " << type
			<< " is greater or equal to NUM_RENDER_TYPES for draw info: "
			<< std::hex << (intptr_t)draw_info << std::dec << ". Skipped."
			<< llendl;
}

void LLCullResult::assertDrawMapsEmpty()
{
	for (U32 i = 0; i < LLRenderPass::NUM_RENDER_TYPES; ++i)
	{
		if (hasRenderMap(i))
		{
			llerrs << "Stale LLDrawInfo's in LLCullResult !" << llendl;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Used by LLSpatialBridge

class LLOctreeMarkNotCulled final : public OctreeTraveler
{
public:
	LLOctreeMarkNotCulled(LLCamera* camera_in)
	:	mCamera(camera_in)
	{
	}

	void traverse(const OctreeNode* node) override
	{
		if (!node)
		{
			llwarns_once << "NULL node !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		LLSpatialGroup* group = (LLSpatialGroup*)node->getListener(0);
		if (!group)
		{
			llwarns_once << "NULL satial group for node " << node
						 << " !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		group->setVisible();
		OctreeTraveler::traverse(node);
	}

	void visit(const OctreeNode* branch) override
	{
		if (branch)
		{
			gPipeline.markNotCulled((LLSpatialGroup*)branch->getListener(0),
									*mCamera);
		}
		else
		{
			llwarns_once << "NULL branch !  Skipping..." << llendl;
			llassert(false);
		}
	}

public:
	LLCamera* mCamera;
};

///////////////////////////////////////////////////////////////////////////////
// LLSpatialBridge class. Spatial partition bridging drawable.
// Used to be in lldrawable.cpp, which was silly since it is declared in
// llspatialpartition.h. HB
///////////////////////////////////////////////////////////////////////////////

#define FORCE_INVISIBLE_AREA 16.f

LLSpatialBridge::LLSpatialBridge(LLDrawable* root, bool render_by_group,
								 U32 data_mask, LLViewerRegion* regionp)
:	LLDrawable(root->getVObj(), true),
	LLSpatialPartition(data_mask, render_by_group, GL_STREAM_DRAW_ARB, regionp)
{
	llassert(root && root->getRegion());

	mBridge = this;
	mDrawable = root;
	root->setSpatialBridge(this);

	mRenderType = mDrawable->mRenderType;
	mDrawableType = mDrawable->mRenderType;

	mPartitionType = LLViewerRegion::PARTITION_VOLUME;

	mOctree->balance();

	LLSpatialPartition* part =
		mDrawable->getRegion()->getSpatialPartition(mPartitionType);
	// PARTITION_VOLUME cannot be NULL
	llassert(part);
	part->put(this);
}

LLSpatialBridge::~LLSpatialBridge()
{
	if (mEntry)
	{
		LLSpatialGroup* group = getSpatialGroup();
		if (group)
		{
			group->getSpatialPartition()->remove(this, group);
		}
	}

	// Delete octree here so listeners will still be able to access bridge
	// specific state
	destroyTree();
}

void LLSpatialBridge::destroyTree()
{
	delete mOctree;
	mOctree = NULL;
}

void LLSpatialBridge::updateSpatialExtents()
{
	LLSpatialGroup* root = (LLSpatialGroup*)mOctree->getListener(0);

	{
		LL_FAST_TIMER(FTM_CULL_REBOUND);
		root->rebound();
	}

	const LLVector4a* root_bounds = root->getBounds();
	static LLVector4a size;
	size = root_bounds[1];

	// VECTORIZE THIS
	static LLMatrix4a mat;
	mat.loadu(mDrawable->getXform()->getWorldMatrix());

	static const LLVector4a t(0.f, 0.f, 0.f, 0.f);
	LLVector4a center;
	mat.affineTransform(t, center);

	static LLVector4a offset;
	mat.rotate(root_bounds[0], offset);
	center.add(offset);

	// Get 4 corners of bounding box
	static LLVector4a v[4];
	mat.rotate(size, v[0]);

	static LLVector4a scale;
	scale.set(-1.f, -1.f, 1.f);
	scale.mul(size);
	mat.rotate(scale, v[1]);

	scale.set(1.f, -1.f, -1.f);
	scale.mul(size);
	mat.rotate(scale, v[2]);

	scale.set(-1.f, 1.f, -1.f);
	scale.mul(size);
	mat.rotate(scale, v[3]);

	static LLVector4a new_min, new_max, min, max, delta;
	new_min = new_max = center;
	for (U32 i = 0; i < 4; ++i)
	{
		delta.setAbs(v[i]);
		min.setSub(center, delta);
		max.setAdd(center, delta);

		new_min.setMin(new_min, min);
		new_max.setMax(new_max, max);
	}
	setSpatialExtents(new_min, new_max);

	static LLVector4a diagonal;
	diagonal.setSub(new_max, new_min);
	mRadius = diagonal.getLength3().getF32() * 0.5f;

	LLVector4a& pos = getGroupPosition();
	pos.setAdd(new_min, new_max);
	pos.mul(0.5f);
	updateBinRadius();
}

void LLSpatialBridge::updateBinRadius()
{
	setBinRadius(llmin(mOctree->getSize()[0] * 0.5f, 256.f));
}

LLCamera LLSpatialBridge::transformCamera(LLCamera& camera)
{
	LLXformMatrix* mat = mDrawable->getXform();
	LLVector3 center = LLVector3::zero * mat->getWorldMatrix();
	LLQuaternion rot = ~mat->getRotation();
	LLCamera ret = camera;

	LLVector3 delta = (ret.getOrigin() - center) * rot;
	if (!delta.isFinite())
	{
		delta.clear();
	}

	LLVector3 look_at = ret.getAtAxis() * rot;
	LLVector3 up_axis = ret.getUpAxis() * rot;
	LLVector3 left_axis = ret.getLeftAxis() * rot;

	ret.setOrigin(delta);
	ret.setAxes(look_at, left_axis, up_axis);
	return ret;
}

void LLSpatialBridge::setVisible(LLCamera& camera_in,
								 std::vector<LLDrawable*>* results,
								 bool for_select)
{
	if (!gPipeline.hasRenderType(mDrawableType))
	{
		return;
	}

	// *HACK: do not draw attachments for avatars that have not been visible in
	// more than a frame
	LLViewerObject* vobj = mDrawable->getVObj();
	if (vobj && vobj->isAttachment() && !vobj->isHUDAttachment())
	{
		LLDrawable* parent = mDrawable->getParent();
		if (parent)
		{
			LLViewerObject* objparent = parent->getVObj();
			if (!objparent || objparent->isDead())
			{
				return;
			}
			if (objparent->isAvatar())
			{
				LLVOAvatar* avatarp = (LLVOAvatar*)objparent;
				if (!avatarp->isVisible() || avatarp->isImpostor() ||
					!avatarp->isFullyLoaded())
				{
					return;
				}
			}

			LLDrawable* drawablep = objparent->mDrawable;
			LLSpatialGroup* group = drawablep->getSpatialGroup();
			if (!group ||
				LLDrawable::getCurrentFrame() - drawablep->getVisible() > 1)
			{
				return;
			}
		}
	}

	LLSpatialGroup* group = (LLSpatialGroup*)mOctree->getListener(0);
	group->rebound();

	LLVector4a center;
	const LLVector4a* exts = getSpatialExtents();
	center.setAdd(exts[0], exts[1]);
	center.mul(0.5f);
	LLVector4a size;
	size.setSub(exts[1], exts[0]);
	size.mul(0.5f);

	if ((LLPipeline::sShadowRender && camera_in.AABBInFrustum(center, size)) ||
		LLPipeline::sImpostorRender ||
		(camera_in.AABBInFrustumNoFarClip(center, size) &&
		AABBSphereIntersect(exts[0], exts[1], camera_in.getOrigin(),
							camera_in.mFrustumCornerDist)))
	{
		if (!LLPipeline::sImpostorRender && !LLPipeline::sShadowRender &&
			LLPipeline::calcPixelArea(center, size, camera_in) < FORCE_INVISIBLE_AREA)
		{
			return;
		}

		LLDrawable::setVisible(camera_in);

		if (for_select)
		{
			results->push_back(mDrawable);
			if (mDrawable->getVObj())
			{
				LLViewerObject::const_child_list_t& child_list =
					mDrawable->getVObj()->getChildren();
				for (LLViewerObject::child_list_t::const_iterator
						iter = child_list.begin(), end = child_list.end();
					 iter != end; ++iter)
				{
					LLViewerObject* child = *iter;
					LLDrawable* drawable = child->mDrawable;
					if (drawable)
					{
						results->push_back(drawable);
					}
				}
			}
		}
		else
		{
			LLCamera trans_camera = transformCamera(camera_in);
			LLOctreeMarkNotCulled culler(&trans_camera);
			culler.traverse(mOctree);
		}
	}
}

void LLSpatialBridge::updateDistance(LLCamera& camera_in, bool force_update)
{
	if (!mDrawable)
	{
		markDead();
		return;
	}

	if (gShiftFrame)
	{
		return;
	}

	if (mDrawable->getVObj())
	{
		if (mDrawable->getVObj()->isAttachment())
		{
			LLDrawable* parent = mDrawable->getParent();
			if (parent && parent->getVObj())
			{
				LLVOAvatar* av = parent->getVObj()->asAvatar();
				if (av && av->isImpostor())
				{
					return;
				}
			}
		}

		LLCamera camera = transformCamera(camera_in);

		mDrawable->updateDistance(camera, force_update);

		LLViewerObject::const_child_list_t& child_list =
			mDrawable->getVObj()->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				iter = child_list.begin(), end = child_list.end();
			 iter != end; ++iter)
		{
			LLViewerObject* child = *iter;
			LLDrawable* drawable = child->mDrawable;
			if (drawable && !drawable->isAvatar())
			{
				drawable->updateDistance(camera, force_update);
			}
		}
	}
}

void LLSpatialBridge::makeActive()
{
	// It is an error to make a spatial bridge active (it is already active)
	llerrs << "makeActive called on spatial bridge" << llendl;
}

void LLSpatialBridge::move(LLDrawable* drawablep, LLSpatialGroup* curp,
						   bool immediate)
{
	LLSpatialPartition::move(drawablep, curp, immediate);
	gPipeline.markMoved(this, false);
}

bool LLSpatialBridge::updateMove()
{
	if (mDrawable && mDrawable->mVObjp && mDrawable->getRegion())
	{
		LLSpatialPartition* part =
			mDrawable->getRegion()->getSpatialPartition(mPartitionType);
		mOctree->balance();
		if (part)
		{
			part->move(this, getSpatialGroup(), true);
		}
		return true;
	}

	llwarns_once << "Bad spatial bridge (NULL drawable or mVObjp or region)."
				 << llendl;
	return false;
}

void LLSpatialBridge::shiftPos(const LLVector4a& vec)
{
	LLDrawable::shift(vec);
}

void LLSpatialBridge::cleanupReferences()
{
	LLDrawable::cleanupReferences();
	if (mDrawable)
	{
		mDrawable->setGroup(NULL);

		if (mDrawable->getVObj())
		{
			LLViewerObject::const_child_list_t& child_list =
				mDrawable->getVObj()->getChildren();
			for (LLViewerObject::child_list_t::const_iterator
					iter = child_list.begin(), end = child_list.end();
				 iter != end; ++iter)
			{
				LLViewerObject* child = *iter;
				LLDrawable* drawable = child->mDrawable;
				if (drawable)
				{
					drawable->setGroup(NULL);
				}
			}
		}

		mDrawable->setSpatialBridge(NULL);
		mDrawable = NULL;
	}
}

LLBridgePartition::LLBridgePartition(LLViewerRegion* regionp)
:	LLSpatialPartition(0, false, 0, regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_VOLUME;
	mPartitionType = LLViewerRegion::PARTITION_BRIDGE;
	mLODPeriod = 16;
	mSlopRatio = 0.25f;
}

LLAvatarPartition::LLAvatarPartition(LLViewerRegion* regionp)
:	LLBridgePartition(regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_AVATAR;
	mPartitionType = LLViewerRegion::PARTITION_AVATAR;
}

LLPuppetPartition::LLPuppetPartition(LLViewerRegion* regionp)
:	LLBridgePartition(regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_PUPPET;
	mPartitionType = LLViewerRegion::PARTITION_PUPPET;
}

LLHUDBridge::LLHUDBridge(LLDrawable* drawablep, LLViewerRegion* regionp)
:	LLVolumeBridge(drawablep, regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_HUD;
	mPartitionType = LLViewerRegion::PARTITION_HUD;
	mSlopRatio = 0.f;
}

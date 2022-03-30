/**
 * @file llspatialpartition.h
 * @brief LLSpatialGroup header file including definitions for supporting functions
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

#ifndef LL_LLSPATIALPARTITION_H
#define LL_LLSPATIALPARTITION_H

#include <queue>

#include "llcubemap.h"
#include "llfastmap.h"
#include "llmaterial.h"
#include "lloctree.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "llvertexbuffer.h"

#include "lldrawable.h"
#include "lldrawpool.h"
#include "llface.h"
#include "llviewercamera.h"

#define SG_MIN_DIST_RATIO 0.00001f
#define SG_STATE_INHERIT_MASK (OCCLUDED)
#define SG_INITIAL_STATE_MASK (DIRTY | GEOM_DIRTY)

class LLSpatialPartition;
class LLSpatialBridge;
class LLSpatialGroup;
class LLViewerOctreePartition;
class LLViewerRegion;

void pushVerts(LLFace* face, U32 mask);

class alignas(16) LLDrawInfo : public LLRefCount
{
protected:
	~LLDrawInfo();

public:
	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE LLDrawInfo(const LLDrawInfo& rhs)
	{
		*this = rhs;
	}

	LL_INLINE const LLDrawInfo& operator=(const LLDrawInfo& rhs)
	{
		llerrs << "Illegal operation !" << llendl;
		return *this;
	}

	LLDrawInfo(U16 start, U16 end, U32 count, U32 offset,
			   LLViewerTexture* image, LLVertexBuffer* buffer,
			   bool fullbright = false, U8 bump = 0, bool particle = false,
			   F32 part_size = 0);

	void validate();

	struct CompareTexture
	{
		LL_INLINE bool operator()(const LLDrawInfo& lhs, const LLDrawInfo& rhs)
		{
			return lhs.mTexture > rhs.mTexture;
		}
	};

	struct CompareTexturePtr
	{
		// Sort by texture
		LL_INLINE bool operator()(const LLPointer<LLDrawInfo>& lhs,
								  const LLPointer<LLDrawInfo>& rhs)
		{
			// sort by pointer, sort NULL down to the end
			return lhs.get() != rhs.get() &&
				   (lhs.isNull() ||
					(rhs.notNull() &&
					 lhs->mTexture.get() > rhs->mTexture.get()));
		}
	};

	struct CompareVertexBuffer
	{
		// Sort by texture
		LL_INLINE bool operator()(const LLPointer<LLDrawInfo>& lhs,
								  const LLPointer<LLDrawInfo>& rhs)
		{
			// sort by pointer, sort NULL down to the end
			return lhs.get() != rhs.get() &&
				   (lhs.isNull() ||
					(rhs.notNull() &&
					 lhs->mVertexBuffer.get() > rhs->mVertexBuffer.get()));
		}
	};

	struct CompareTexturePtrMatrix
	{
		LL_INLINE bool operator()(const LLPointer<LLDrawInfo>& lhs,
								  const LLPointer<LLDrawInfo>& rhs)
		{
			return lhs.get() != rhs.get() &&
				   (lhs.isNull() ||
					(rhs.notNull() &&
					 (lhs->mTexture.get() > rhs->mTexture.get() ||
					  (lhs->mTexture.get() == rhs->mTexture.get() &&
					   lhs->mModelMatrix > rhs->mModelMatrix))));
		}
	};

	struct CompareMatrixTexturePtr
	{
		LL_INLINE bool operator()(const LLPointer<LLDrawInfo>& lhs,
								  const LLPointer<LLDrawInfo>& rhs)
		{
			return lhs.get() != rhs.get() &&
				   (lhs.isNull() ||
					(rhs.notNull() &&
					 (lhs->mModelMatrix > rhs->mModelMatrix ||
					  (lhs->mModelMatrix == rhs->mModelMatrix &&
					   lhs->mTexture.get() > rhs->mTexture.get()))));
		}
	};

	struct CompareBump
	{
		LL_INLINE bool operator()(const LLPointer<LLDrawInfo>& lhs,
								  const LLPointer<LLDrawInfo>& rhs)
		{
			// Sort by mBump value, sort NULL down to the end
			return lhs.get() != rhs.get() &&
				   (lhs.isNull() ||
					(rhs.notNull() && lhs->mBump > rhs->mBump));
		}
	};

	struct CompareDistanceGreater
	{
		LL_INLINE bool operator()(const LLPointer<LLDrawInfo>& lhs,
								  const LLPointer<LLDrawInfo>& rhs)
		{
			// Sort by mBump value, sort NULL down to the end
			return lhs.get() != rhs.get() &&
				   (lhs.isNull() ||
					(rhs.notNull() && lhs->mDistance > rhs->mDistance));
		}
	};

public:
	alignas(16) LLVector4a		mExtents[2];
	LLFace*						mFace;		// Associated face

	LLPointer<LLVertexBuffer>	mVertexBuffer;
	LLPointer<LLViewerTexture>	mTexture;

	LLSpatialGroup*				mGroup;
	const LLMatrix4*			mTextureMatrix;
	const LLMatrix4*			mModelMatrix;
	U16							mStart;
	U16							mEnd;
	U32							mCount;
	U32							mOffset;
	F32							mPartSize;
	F32							mVSize;
	F32							mDistance;
	LLRender::eGeomModes		mDrawMode;
	U32							mBlendFuncSrc;
	U32							mBlendFuncDst;
	S32							mDebugColor;

	// If mMaterial is null, the following parameters are unused:
	LLMaterialPtr				mMaterial;
	LLMaterialID				mMaterialID;
	U32							mShaderMask;
	LLPointer<LLViewerTexture>	mSpecularMap;
	LLPointer<LLViewerTexture>	mNormalMap;
	// XYZ = Specular RGB, W = Specular Exponent:
	LLVector4					mSpecColor;
	F32							mEnvIntensity;
	F32							mAlphaMaskCutoff;

	bool						mFullbright;
	bool						mParticle;
	bool						mHasGlow;
	U8							mBump;
	U8							mShiny;
	U8							mDiffuseAlphaMode;

	typedef std::vector<LLPointer<LLViewerTexture> > tex_vec_t;
	tex_vec_t					mTextureList;
};

class alignas(64) LLSpatialGroup final : public LLOcclusionCullingGroup
{
	friend class LLSpatialPartition;
	friend class LLOctreeStateCheck;

protected:
	LOG_CLASS(LLSpatialGroup);

	~LLSpatialGroup() override;

public:
	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LLSpatialGroup(const LLSpatialGroup& rhs)
	:	LLOcclusionCullingGroup(rhs)
	{
		*this = rhs;
	}

	const LLSpatialGroup& operator=(const LLSpatialGroup& rhs)
	{
		llerrs << "Illegal operation!" << llendl;
		return *this;
	}

	typedef std::vector<LLPointer<LLSpatialGroup> > sg_vector_t;
	typedef std::vector<LLPointer<LLSpatialBridge> > bridge_list_t;
	typedef std::vector<LLPointer<LLDrawInfo> > drawmap_elem_t;
	typedef fast_hmap<U32, drawmap_elem_t > draw_map_t;
	typedef std::vector<LLPointer<LLVertexBuffer> > buffer_list_t;
	typedef fast_hmap<LLFace*, buffer_list_t> buffer_texture_map_t;
	typedef fast_hmap<U32, buffer_texture_map_t> buffer_map_t;

	struct CompareDistanceGreater
	{
		LL_INLINE bool operator()(const LLSpatialGroup* const& lhs,
								  const LLSpatialGroup* const& rhs)
		{
			return lhs->mDistance > rhs->mDistance;
		}
	};

	struct CompareUpdateUrgency
	{
		LL_INLINE bool operator()(const LLPointer<LLSpatialGroup> lhs,
								  const LLPointer<LLSpatialGroup> rhs)
		{
			return lhs->getUpdateUrgency() > rhs->getUpdateUrgency();
		}
	};

	struct CompareDepthGreater
	{
		LL_INLINE bool operator()(const LLSpatialGroup* const& lhs,
								  const LLSpatialGroup* const& rhs)
		{
			return lhs->mDepth > rhs->mDepth;
		}
	};

	typedef enum
	{
		GEOM_DIRTY				= LLViewerOctreeGroup::INVALID_STATE,
		ALPHA_DIRTY				= (GEOM_DIRTY << 1),
		IN_IMAGE_QUEUE			= (ALPHA_DIRTY << 1),
		IMAGE_DIRTY				= (IN_IMAGE_QUEUE << 1),
		MESH_DIRTY				= (IMAGE_DIRTY << 1),
		NEW_DRAWINFO			= (MESH_DIRTY << 1),
		IN_BUILD_Q1				= (NEW_DRAWINFO << 1),
		IN_BUILD_Q2				= (IN_BUILD_Q1 << 1),
		STATE_MASK				= 0x0000FFFF,
	} eSpatialState;

	LLSpatialGroup(OctreeNode* node, LLSpatialPartition* part);

	bool isHUDGroup();

	void clearDrawMap();
	void validate();
	void validateDrawMap();

	void setState(U32 state, S32 mode);
	void clearState(U32 state, S32 mode);
	LL_INLINE void clearState(U32 state)				{ mState &= ~state; }

	LLSpatialGroup* getParent();

	bool addObject(LLDrawable* drawablep);
	bool removeObject(LLDrawable* drawablep, bool from_octree = false);

	// Update position if it's in the group:
	bool updateInGroup(LLDrawable* drawablep, bool immediate = false);

	void shift(const LLVector4a& offset);
	void destroyGL(bool keep_occlusion = false);

	void updateDistance(LLCamera& camera);
	F32 getUpdateUrgency() const;
	bool changeLOD();
	void rebuildGeom();
	void rebuildMesh();

	LL_INLINE void setState(U32 state)					{ mState |= state; }
	LL_INLINE void dirtyGeom()							{ setState(GEOM_DIRTY); }
	LL_INLINE void dirtyMesh()							{ setState(MESH_DIRTY); }

	void drawObjectBox(LLColor4 col);

	LL_INLINE LLSpatialPartition* getSpatialPartition()
	{
		return (LLSpatialPartition*)mSpatialPartition;
	}

	// LLOcclusionCullingGroup overrides
	void handleInsertion(const TreeNode* node,
						 LLViewerOctreeEntry* entry) override;
	void handleRemoval(const TreeNode* node,
					   LLViewerOctreeEntry* entry) override;
	void handleDestruction(const TreeNode* node) override;
	void handleChildAddition(const OctreeNode* parent,
							 OctreeNode* child) override;

public:
	LLVector4a					mViewAngle;
	LLVector4a					mLastUpdateViewAngle;

	// Cached llmax(mObjectBounds[1].getLength3(), 10.f)
	F32							mObjectBoxSize;

protected:
	static S32 sLODSeed;

public:
	LLPointer<LLVertexBuffer>	mVertexBuffer;
	U32							mBufferUsage;

	F32							mBuilt;
	F32							mDistance;
	F32							mDepth;
	F32							mLastUpdateDistance;
	F32							mLastUpdateTime;
	F32							mPixelArea;
	F32							mRadius;

	// Used by volumes to track how many bytes of geometry data are in this
	// node:
	U32							mGeometryBytes;
	// Used by volumes to track estimated surface area of geometry in this
	// node:
	F32							mSurfaceArea;

	bridge_list_t				mBridgeList;

	// Used by volume buffers to attempt to reuse vertex buffers:
	buffer_map_t				mBufferMap;

	draw_map_t					mDrawMap;

	static U32					sNodeCount;

	// Deletion of spatial groups and draw info not allowed if true:
	static bool					sNoDelete;
};

class LLGeometryManager
{
public:
	std::vector<LLFace*> mFaceList;
	virtual ~LLGeometryManager()						{}
	virtual void rebuildGeom(LLSpatialGroup* group) = 0;
	virtual void rebuildMesh(LLSpatialGroup* group) = 0;
	virtual void getGeometry(LLSpatialGroup* group) = 0;
	virtual void addGeometryCount(LLSpatialGroup* group, U32& vertex_count,
								  U32& index_count);

	virtual LLVertexBuffer* createVertexBuffer(U32 type_mask, U32 usage);
};

class LLSpatialPartition : public LLViewerOctreePartition,
						   public LLGeometryManager
{
protected:
	LOG_CLASS(LLSpatialPartition);

public:
	LLSpatialPartition(U32 data_mask, bool render_by_group, U32 buffer_usage,
					   LLViewerRegion* regionp);

	LLSpatialGroup* put(LLDrawable* drawablep, bool was_visible = false);
	bool remove(LLDrawable* drawablep, LLSpatialGroup* curp);

	LLDrawable* lineSegmentIntersect(const LLVector4a& start,
									 const LLVector4a& end,
									 bool pick_transparent, bool pick_rigged,
									 S32* face_hit,
									 LLVector4a* intersection = NULL,
									 LLVector2* tex_coord = NULL,
									 LLVector4a* normal = NULL,
									 LLVector4a* tangent = NULL);

	// If the drawable moves, move it here.
	virtual void move(LLDrawable* drawablep, LLSpatialGroup* curp,
					  bool immediate = false);
	virtual void shift(const LLVector4a& offset);

	virtual F32 calcDistance(LLSpatialGroup* group, LLCamera& camera);
	virtual F32 calcPixelArea(LLSpatialGroup* group, LLCamera& camera);

	void rebuildGeom(LLSpatialGroup* group) override;
	void rebuildMesh(LLSpatialGroup* group) override;

	bool visibleObjectsInFrustum(LLCamera& camera);

	// Cull on arbitrary frustum
	S32 cull(LLCamera& camera, bool do_occlusion = false) override;
	S32 cull(LLCamera& camera, std::vector<LLDrawable*>* res, bool for_sel);

	bool isVisible(const LLVector3& v);
	bool isHUDPartition();

	LL_INLINE LLSpatialBridge* asBridge()			{ return mBridge; }
	LL_INLINE bool isBridge()						{ return asBridge() != NULL; }

	void renderPhysicsShapes();
	void renderDebug();
	void renderIntersectingBBoxes(LLCamera* camera);
	void restoreGL();
	void resetVertexBuffers();

	bool getVisibleExtents(LLCamera& camera, LLVector3& visMin,
						   LLVector3& visMax);

public:
	// NULL for non-LLSpatialBridge instances, otherwise, mBridge == this. Uses
	// a pointer instead of making "isBridge" and "asBridge" virtual so it is
	// safe to call asBridge() from the destructor:
	LLSpatialBridge*	mBridge;

	U32					mBufferUsage;
	U32					mVertexDataMask;

	// Percentage distance must change before drawables receive LOD update
	// (default is 0.25):
	F32					mSlopRatio;

	// If true, frustum culling ignores far clip plane:
	bool				mInfiniteFarClip;

	// If true, objects in this partition will be written to depth during alpha
	// rendering:
	bool				mDepthMask;

	const bool			mRenderByGroup;

	// started to issue a teleport request
	static bool			sTeleportRequested;
};

// Class for creating bridges between spatial partitions
class LLSpatialBridge : public LLDrawable, public LLSpatialPartition
{
protected:
	LOG_CLASS(LLSpatialBridge);

	~LLSpatialBridge() override;

public:
	typedef std::vector<LLPointer<LLSpatialBridge> > bridge_vector_t;

	LLSpatialBridge(LLDrawable* root, bool render_by_group, U32 data_mask,
					LLViewerRegion* regionp);

	void destroyTree();

	LLCamera transformCamera(LLCamera& camera);

	// LLDrawable overrides

	LL_INLINE bool isSpatialBridge() const override		{ return true; }

	LL_INLINE LLSpatialPartition* asPartition() override
	{
		return this;
	}

	void updateSpatialExtents() override;
	void updateBinRadius() override;
	void setVisible(LLCamera& camera_in,
					std::vector<LLDrawable*>* results = NULL,
					bool for_select = false) override;
	void updateDistance(LLCamera& camera_in, bool force_update) override;
	void makeActive() override;
	void shiftPos(const LLVector4a& vec) override;
	void cleanupReferences() override;

	bool updateMove() override;

	// LLSpatialPartition override
	void move(LLDrawable* drawablep, LLSpatialGroup* curp,
			  bool immediate = false) override;

public:
	LLDrawable*				mDrawable;
	LLPointer<LLVOAvatar>	mAvatar;
};

class LLCullResult
{
protected:
	LOG_CLASS(LLCullResult);

public:
	LL_INLINE LLCullResult()							{}

	typedef std::vector<LLSpatialGroup*> sg_list_t;
	typedef std::vector<LLDrawable*> drawable_list_t;
	typedef std::vector<LLSpatialBridge*> bridge_list_t;
	typedef std::vector<LLDrawInfo*> drawinfo_list_t;

	typedef sg_list_t::iterator sg_iterator;
	typedef bridge_list_t::iterator bridge_iterator;
	typedef drawinfo_list_t::iterator drawinfo_iterator;
	typedef drawable_list_t::iterator drawable_iterator;

	void clear();

	LL_INLINE sg_iterator beginVisibleGroups()			{ return mVisibleGroups.begin(); }
	LL_INLINE sg_iterator endVisibleGroups()			{ return mVisibleGroups.end(); }

	LL_INLINE sg_iterator beginAlphaGroups()			{ return mAlphaGroups.begin(); }
	LL_INLINE sg_iterator endAlphaGroups()				{ return mAlphaGroups.end(); }

	LL_INLINE sg_iterator beginOcclusionGroups()		{ return mOcclusionGroups.begin(); }
	LL_INLINE sg_iterator endOcclusionGroups()			{ return mOcclusionGroups.end(); }
	LL_INLINE bool hasOcclusionGroups() const			{ return !mOcclusionGroups.empty(); }

	LL_INLINE sg_iterator beginDrawableGroups()			{ return mDrawableGroups.begin(); }
	LL_INLINE sg_iterator endDrawableGroups()			{ return mDrawableGroups.end(); }

	LL_INLINE drawable_iterator beginVisibleList()		{ return mVisibleList.begin(); }
	LL_INLINE drawable_iterator endVisibleList()		{ return mVisibleList.end(); }

	LL_INLINE bridge_iterator beginVisibleBridge()		{ return mVisibleBridge.begin(); }
	LL_INLINE bridge_iterator endVisibleBridge()		{ return mVisibleBridge.end(); }

	LL_INLINE drawinfo_iterator beginRenderMap(U32 typ)	{ return mRenderMap[typ].begin(); }
	LL_INLINE drawinfo_iterator endRenderMap(U32 type)	{ return mRenderMap[type].end(); }

	LL_INLINE bool hasRenderMap(U32 type) const
	{
		return type < LLRenderPass::NUM_RENDER_TYPES &&
			   !mRenderMap[type].empty();
	}

	LL_INLINE void pushVisibleGroup(LLSpatialGroup* g)	{ mVisibleGroups.push_back(g); }
	LL_INLINE void pushAlphaGroup(LLSpatialGroup* g)	{ mAlphaGroups.push_back(g); }

	LL_INLINE void pushOcclusionGroup(LLSpatialGroup* g)
	{
		mOcclusionGroups.push_back(g);
	}

	LL_INLINE void pushDrawableGroup(LLSpatialGroup* g)	{ mDrawableGroups.push_back(g); }
	LL_INLINE void pushDrawable(LLDrawable* drawable)	{ mVisibleList.push_back(drawable); }
	LL_INLINE void pushBridge(LLSpatialBridge* bridge)	{ mVisibleBridge.push_back(bridge); }
	void pushDrawInfo(U32 type, LLDrawInfo* draw_info);

	void assertDrawMapsEmpty();

private:
	sg_list_t		mVisibleGroups;
	sg_list_t		mAlphaGroups;
	sg_list_t		mOcclusionGroups;
	sg_list_t		mDrawableGroups;
	drawable_list_t	mVisibleList;
	bridge_list_t	mVisibleBridge;
	drawinfo_list_t	mRenderMap[LLRenderPass::NUM_RENDER_TYPES];
};

// Spatial partition for water (implemented in llvowater.cpp)
class LLWaterPartition : public LLSpatialPartition
{
public:
	LLWaterPartition(LLViewerRegion* regionp);

	LL_INLINE void getGeometry(LLSpatialGroup*) override
	{
	}

	LL_INLINE void addGeometryCount(LLSpatialGroup*, U32&, U32&) override
	{
	}
};

// Spatial partition for hole and edge water (implemented in llvowater.cpp)
class LLVoidWaterPartition : public LLWaterPartition
{
public:
	LLVoidWaterPartition(LLViewerRegion* regionp);
};

// Spatial partition for terrain (impelmented in llvosurfacepatch.cpp)
class LLTerrainPartition final : public LLSpatialPartition
{
public:
	LLTerrainPartition(LLViewerRegion* regionp);
	void getGeometry(LLSpatialGroup* group) override;
	LLVertexBuffer* createVertexBuffer(U32 type_mask, U32 usage) override;
};

// Spatial partition for trees (implemented in llvotree.cpp)
class LLTreePartition final : public LLSpatialPartition
{
public:
	LLTreePartition(LLViewerRegion* regionp);

	LL_INLINE void getGeometry(LLSpatialGroup*) override
	{
	}

	LL_INLINE void addGeometryCount(LLSpatialGroup*, U32&, U32&) override
	{
	}
};

// Spatial partition for particles (implemented in llvopartgroup.cpp)
class LLParticlePartition : public LLSpatialPartition
{
public:
	LLParticlePartition(LLViewerRegion* regionp);
	void rebuildGeom(LLSpatialGroup* group) override;
	void getGeometry(LLSpatialGroup* group) override;
	void addGeometryCount(LLSpatialGroup* group, U32& vertex_count,
						  U32& index_count) override;

	LL_INLINE F32 calcPixelArea(LLSpatialGroup*, LLCamera&) override
	{
		return 1024.f;
	}

protected:
	U32 mRenderPass;
};

class LLHUDParticlePartition final : public LLParticlePartition
{
public:
	LLHUDParticlePartition(LLViewerRegion* regionp);
};

// Spatial partition for grass (implemented in llvograss.cpp)
class LLGrassPartition final : public LLSpatialPartition
{
public:
	LLGrassPartition(LLViewerRegion* regionp);
	void getGeometry(LLSpatialGroup* group) override;
	void addGeometryCount(LLSpatialGroup* group, U32& vertex_count,
						  U32& index_count) override;

protected:
	U32 mRenderPass;
};

// Spatial partition for clouds (implemented in llvoclouds.cpp)
class LLCloudPartition final : public LLParticlePartition
{
public:
	LLCloudPartition(LLViewerRegion* regionp);
};

// Class for wrangling geometry out of volumes (implemented in llvovolume.cpp)
class LLVolumeGeometryManager : public LLGeometryManager
{
protected:
	LOG_CLASS(LLVolumeGeometryManager);

public:
	typedef enum
	{
		NONE = 0,
		BATCH_SORT,
		DISTANCE_SORT
	} eSortType;

	LLVolumeGeometryManager();
	~LLVolumeGeometryManager() override;

	void rebuildGeom(LLSpatialGroup* group) override;
	void rebuildMesh(LLSpatialGroup* group) override;
	LL_INLINE void getGeometry(LLSpatialGroup*) override	{}
	void addGeometryCount(LLSpatialGroup* group, U32& vertex_count,
						  U32& index_count) override;
	void genDrawInfo(LLSpatialGroup* group, U32 mask,
					 LLFace** faces, U32 face_count,
					 bool distance_sort = false, bool batch_textures = false,
					 bool no_materials = false);
	void registerFace(LLSpatialGroup* group, LLFace* facep, U32 type);

private:
	void allocateFaces(U32 max_face_count);
	void freeFaces();

private:
	static S32		sInstanceCount;
	static LLFace**	sFullbrightFaces;
	static LLFace**	sBumpFaces;
	static LLFace**	sSimpleFaces;
	static LLFace**	sNormFaces;
	static LLFace**	sSpecFaces;
	static LLFace**	sNormSpecFaces;
	static LLFace** sAlphaFaces;
};

// Spatial partition that uses volume geometry manager (implemented in
// llvovolume.cpp)
class LLVolumePartition final : public LLSpatialPartition,
								public LLVolumeGeometryManager
{
public:
	LLVolumePartition(LLViewerRegion* regionp);

	LL_INLINE void rebuildGeom(LLSpatialGroup* group) override
	{
		LLVolumeGeometryManager::rebuildGeom(group);
	}

	LL_INLINE void getGeometry(LLSpatialGroup* group) override
	{
		LLVolumeGeometryManager::getGeometry(group);
	}

	LL_INLINE void rebuildMesh(LLSpatialGroup* group) override
	{
		LLVolumeGeometryManager::rebuildMesh(group);
	}

	LL_INLINE void addGeometryCount(LLSpatialGroup* group, U32& vertex_count,
									U32& index_count) override
	{
		LLVolumeGeometryManager::addGeometryCount(group, vertex_count,
												  index_count);
	}
};

// Spatial bridge that uses volume geometry manager (implemented in
// llvovolume.cpp)
class LLVolumeBridge : public LLSpatialBridge, public LLVolumeGeometryManager
{
public:
	LLVolumeBridge(LLDrawable* drawable, LLViewerRegion* regionp);

	LL_INLINE void rebuildGeom(LLSpatialGroup* group) override
	{
		LLVolumeGeometryManager::rebuildGeom(group);
	}

	LL_INLINE void getGeometry(LLSpatialGroup* group) override
	{
		LLVolumeGeometryManager::getGeometry(group);
	}

	LL_INLINE void rebuildMesh(LLSpatialGroup* group) override
	{
		LLVolumeGeometryManager::rebuildMesh(group);
	}

	LL_INLINE void addGeometryCount(LLSpatialGroup* group, U32& vertex_count,
									U32& index_count) override
	{
		LLVolumeGeometryManager::addGeometryCount(group, vertex_count,
												  index_count);
	}
};

// Spatial attachment bridge that uses volume geometry manager (implemented in
// llvovolume.cpp)
class LLAvatarBridge final : public LLVolumeBridge
{
public:
	LLAvatarBridge(LLDrawable* drawable, LLViewerRegion* regionp);
};

class LLPuppetBridge final : public LLVolumeBridge
{
public:
	LLPuppetBridge(LLDrawable* drawable, LLViewerRegion* regionp);
};

class LLHUDBridge final : public LLVolumeBridge
{
public:
	LLHUDBridge(LLDrawable* drawablep, LLViewerRegion* regionp);

	// HUD objects do not shift with region crossing. That would be silly.
	LL_INLINE void shiftPos(const LLVector4a&) override		{}

	LL_INLINE F32 calcPixelArea(LLSpatialGroup*, LLCamera&) override
	{
		return 1024.f;
	}
};

// Spatial partition that holds nothing but spatial bridges
class LLBridgePartition : public LLSpatialPartition
{
public:
	LLBridgePartition(LLViewerRegion* regionp);

	LL_INLINE void getGeometry(LLSpatialGroup*) override
	{
	}

	LL_INLINE void addGeometryCount(LLSpatialGroup*, U32&, U32&) override
	{
	}
};

// Spatial partition that holds nothing but spatial bridges
class LLAvatarPartition final : public LLBridgePartition
{
public:
	LLAvatarPartition(LLViewerRegion* regionp);
};

// Spatial partition that holds nothing but spatial bridges
class LLPuppetPartition final : public LLBridgePartition
{
public:
	LLPuppetPartition(LLViewerRegion* regionp);
};

class LLHUDPartition final : public LLBridgePartition
{
public:
	LLHUDPartition(LLViewerRegion* regionp);

	// HUD objects do not shift with region crossing. That would be silly.
	LL_INLINE void shift(const LLVector4a&) override	{}
};

// Also called from LLPipeline
void drawBox(const LLVector4a& c, const LLVector4a& r);
void drawBoxOutline(const LLVector3& pos, const LLVector3& size);

typedef fast_hset<LLSpatialGroup*> spatial_groups_set_t;
extern spatial_groups_set_t gVisibleSelectedGroups;

#endif //LL_LLSPATIALPARTITION_H

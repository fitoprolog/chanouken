/**
 * @file llpipeline.h
 * @brief Rendering pipeline definitions
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

#ifndef LL_LLPIPELINE_H
#define LL_LLPIPELINE_H

#include <stack>

#include "llcamera.h"
#include "llfastmap.h"
#include "llerror.h"
#include "llpointer.h"
#include "llrendertarget.h"
#include "llstat.h"
#include "llvertexbuffer.h"

#include "lldrawable.h"
#include "lldrawpool.h"
#include "llspatialpartition.h"

// DO NOT USE !  This causes causes nasty occlusion buffers corruptions !  HB
#define LL_USE_DYNAMIC_TEX_FBO 0

// Used for mesh outline selection shading in LL's viewer. Not (yet) used here.
#define LL_NEW_DEPTH_STENCIL 0

class LLAgent;
class LLCubeMap;
class LLCullResult;
class LLDisplayPrimitive;
class LLDrawPoolAlpha;
class LLEdge;
class LLFace;
class LLGLSLShader;
class LLRenderFunc;
class LLTextureEntry;
class LLViewerObject;
class LLViewerTexture;
class LLVOAvatar;
class LLVOPartGroup;

typedef enum e_avatar_skinning_method
{
	SKIN_METHOD_SOFTWARE,
	SKIN_METHOD_VERTEX_PROGRAM
} EAvatarSkinningMethod;

bool LLRayAABB(const LLVector3& center, const LLVector3& size,
			   const LLVector3& origin, const LLVector3& dir,
			   LLVector3& coord, F32 epsilon = 0);

class LLPipeline
{
protected:
	LOG_CLASS(LLPipeline);

public:
	LLPipeline();

	void destroyGL();
	void restoreGL();
	void resetVertexBuffers();
	void doResetVertexBuffers(bool forced = false);
	void resizeScreenTexture();
	void resizeShadowTexture();
	void releaseGLBuffers();
	void releaseLUTBuffers();
	void releaseScreenBuffers();
	void releaseShadowTargets();
	void releaseShadowTarget(U32 index);	
	void createGLBuffers();
	void createLUTBuffers();

	void allocateScreenBuffer(U32 res_x, U32 res_y);
	bool allocateScreenBuffer(U32 res_x, U32 res_y, U32 samples);
	bool allocateShadowBuffer(U32 res_x, U32 res_y);
	void allocatePhysicsBuffer();

	void resetVertexBuffers(LLDrawable* drawable);
	void generateImpostor(LLVOAvatar* avatar);
	void bindScreenToTexture();
	void renderFinalize();

	void init();
	void cleanup();
	void dumpStats();
	LL_INLINE bool isInit()						{ return mInitialized; };

	// Gets a draw pool from pool type (POOL_SIMPLE, POOL_MEDIA) and texture.
	// Returns the draw pool, or NULL if not found.
	LLDrawPool* findPool(U32 pool_type, LLViewerTexture* tex0 = NULL);

	// Gets a draw pool for faces of the appropriate type and texture. Creates
	// if necessary. Always returns a draw pool.
	LLDrawPool* getPool(U32 pool_type, LLViewerTexture* tex0 = NULL);

	// Figures out draw pool type from texture entry. Creates a new pool if
	// necessary.
	static LLDrawPool* getPoolFromTE(const LLTextureEntry* te,
									 LLViewerTexture* te_image);
	static U32 getPoolTypeFromTE(const LLTextureEntry* te,
								 LLViewerTexture* imagep);

	// Only to be used by LLDrawPool classes for splitting pools !
	void addPool(LLDrawPool* poolp);
	void removePool(LLDrawPool* poolp);

	void allocDrawable(LLViewerObject* obj);

	void unlinkDrawable(LLDrawable*);

	// Object related methods
	void markVisible(LLDrawable* drawablep, LLCamera& camera);
	void markOccluder(LLSpatialGroup* group);

	// Downsample source to dest, taking the maximum depth value per pixel in
	// source and writing to dest. If source's depth buffer cannot be bound for
	// reading, a scratch space depth buffer must be provided.
	void downsampleDepthBuffer(LLRenderTarget& source, LLRenderTarget& dest,
							   LLRenderTarget* scratch_space = NULL);

	void doOcclusion(LLCamera& camera, LLRenderTarget& source,
					 LLRenderTarget& dest, LLRenderTarget* scratch = NULL);
	void doOcclusion(LLCamera& camera);
	void markNotCulled(LLSpatialGroup* group, LLCamera& camera);
	void markMoved(LLDrawable* drawablep, bool damped_motion = false);
	void markShift(LLDrawable* drawablep);
	void markTextured(LLDrawable* drawablep);
	void markGLRebuild(LLGLUpdate* glu);
	void markRebuild(LLSpatialGroup* group, bool priority = false);
	void markRebuild(LLDrawable* drawablep,
					 LLDrawable::EDrawableFlags flag = LLDrawable::REBUILD_ALL,
					 bool priority = false);
	void markPartitionMove(LLDrawable* drawablep);
	void markMeshDirty(LLSpatialGroup* group);

	// Gets the object between start and end that is closest to start.
	LLViewerObject* lineSegmentIntersectInWorld(const LLVector4a& start,
												const LLVector4a& end,
												bool pick_transparent,
												bool pick_rigged,
												// Returns the face hit
												S32* face_hit,
												// Returns the intersection point
												LLVector4a* intersection = NULL,
												// Returns texture coordinates
												LLVector2* tex_coord = NULL,
												// Returns the surface normal
												LLVector4a* normal = NULL,
												// Return the surface tangent
												LLVector4a* tangent = NULL);

	// Gets the closest particle to start between start and end, returns the
	// LLVOPartGroup and particle index:
	LLVOPartGroup* lineSegmentIntersectParticle(const LLVector4a& start,
												const LLVector4a& end,
												LLVector4a* intersection,
												S32* face_hit);

	LLViewerObject* lineSegmentIntersectInHUD(const LLVector4a& start,
											  const LLVector4a& end,
											  bool pick_transparent,
												// Returns the face hit
											  S32* face_hit,
											  // Returns the intersection point
											  LLVector4a* intersection = NULL,
											  // Returns texture coordinates
											  LLVector2* tex_coord = NULL,
											  // Returns the surface normal
											  LLVector4a* normal = NULL,
											  // Return the surface tangent
											  LLVector4a* tangent = NULL);

	// Something about these textures has changed.  Dirty them.
	void dirtyPoolObjectTextures(const LLViewerTextureList::dirty_list_t& textures);

	void resetDrawOrders();

	U32 addObject(LLViewerObject* obj);

	LL_INLINE S32 getLightingDetail() const		{ return mLightingDetail; }
	S32 getMaxLightingDetail() const;
	S32 setLightingDetail(S32 level);

	LL_INLINE bool getUseVertexShaders() const	{ return mVertexShadersLoaded != -1; }
	LL_INLINE bool canUseVertexShaders() const	{ return mVertexShadersLoaded == 1; }
	bool canUseWindLightShaders() const;
	bool canUseWindLightShadersOnObjects() const;

	// Phases
	void resetFrameStats();

	void updateMoveDampedAsync(LLDrawable* drawablep);
	void updateMoveNormalAsync(LLDrawable* drawablep);
	void updateMovedList(LLDrawable::draw_vec_t& move_list);
	void updateMove(bool balance_vo_cache);
	bool visibleObjectsInFrustum(LLCamera& camera);
	bool getVisibleExtents(LLCamera& camera, LLVector3& min, LLVector3& max);
	bool getVisiblePointCloud(LLCamera& camera, LLVector3& min, LLVector3& max,
							  std::vector<LLVector3>& fp,
							  LLVector3 light_dir = LLVector3::zero);

	// If water_clip is 0, ignore water plane, 1, cull to above plane, -1, cull
	// to below plane
	void updateCull(LLCamera& camera, LLCullResult& result, S32 water_clip = 0,
					LLPlane* plane = NULL, bool hud_attachments = false);

	void createObjects(F32 max_dtime);
	void createObject(LLViewerObject* vobj);
	void processPartitionQ();
	void updateGeom(F32 max_dtime);
	void updateGL();
	void rebuildPriorityGroups();
	void rebuildGroups();
	void clearRebuildGroups();
	void clearRebuildDrawables();

	// Calculates pixel area of given box from vantage point of given camera
	static F32 calcPixelArea(LLVector3 center, LLVector3 size,
							 LLCamera& camera);
	static F32 calcPixelArea(const LLVector4a& center, const LLVector4a& size,
							 LLCamera& camera);

	void stateSort(LLCamera& camera, LLCullResult& result);
	void stateSort(LLSpatialGroup* group, LLCamera& camera);
	void stateSort(LLSpatialBridge* bridge, LLCamera& camera,
				   bool fov_changed = false);
	void stateSort(LLDrawable* drawablep, LLCamera& camera);
	void postSort(LLCamera& camera);
	void forAllVisibleDrawables(void (*func)(LLDrawable*));

	void renderObjects(U32 type, U32 mask, bool texture = true,
					   bool batch_texture = false);
	void renderMaskedObjects(U32 type, U32 mask, bool texture = true,
							 bool batch_texture = false);
	void renderFullbrightMaskedObjects(U32 type, U32 mask, bool texture = true,
							 bool batch_texture = false);

	void renderGroups(LLRenderPass* pass, U32 type, U32 mask, bool texture);

	void grabReferences(LLCullResult& result);
	void clearReferences();

	// Checks references and asserts that there are no references in sCull
	// results to the provided data
	void checkReferences(LLFace* face);
	void checkReferences(LLDrawable* drawable);
	void checkReferences(LLDrawInfo* draw_info);
	void checkReferences(LLSpatialGroup* group);

	void renderGeom(LLCamera& camera);
	void renderGeomDeferred(LLCamera& camera);
	void renderGeomPostDeferred(LLCamera& camera, bool do_occlusion = true);
	void renderGeomShadow(LLCamera& camera);

	// Common version (takes care of choosing which version to use among the
	// WL and EE ones, based on gUseNewShaders)
	void bindDeferredShader(LLGLSLShader& shader);
	// Windlight renderer version
	void bindDeferredShaderWL(LLGLSLShader& shader, U32 light_index);
	// Extended environment renderer version
	void bindDeferredShaderEE(LLGLSLShader& shader, LLRenderTarget* light);
	void unbindDeferredShader(LLGLSLShader& shader);

	void setupSpotLight(LLGLSLShader& shader, LLDrawable* drawablep);

	void renderDeferredLighting();

	// Common version (takes care of choosing which version to use among the
	// WL and EE ones, based on gUseNewShaders)
	void generateWaterReflection();
	// Windlight renderer version
	void generateWaterReflectionWL(bool skip_avatar_update);
	// Extended environment renderer version
	void generateWaterReflectionEE(bool skip_avatar_update);

	void generateSunShadow();
	void renderHighlight(const LLViewerObject* obj, F32 fade);

	void renderShadow(const LLMatrix4a& view, const LLMatrix4a& proj,
					  LLCamera& camera, LLCullResult& result, bool use_shader,
					  bool use_occlusion, U32 target_width);
	void renderHighlights();
	void renderDebug();
	void renderPhysicsDisplay();
	
	// Returns 0 when the object is not to be highlighted, 1 when it can be
	// both highlighted and marked with a beacon, and 2 when it may can be
	// highlighted only.
	static U32 highlightable(const LLViewerObject* vobj);

	void rebuildPools(); // Rebuild pools

#if LL_DEBUG && 0
	// Debugging method.
	// Find the lists which have references to this object
	void findReferences(LLDrawable* drawablep);
#endif

	// Verify that all data in the pipeline is "correct":
	bool verify();

	LL_INLINE S32 getLightCount() const			{ return mLights.size(); }

	void calcNearbyLights(LLCamera& camera);
	void setupHWLights(LLDrawPool* pool);
	void setupAvatarLights(bool for_edit = false);
	void enableLights(U32 mask);
	void enableLightsStatic();
	void enableLightsDynamic();
	void enableLightsAvatar();
	void enableLightsPreview();
	void enableLightsAvatarEdit();
	void enableLightsFullbright();
	void disableLights();

	void shiftObjects(const LLVector3& offset);

	void setLight(LLDrawable* drawablep, bool is_light);

	LL_INLINE bool hasRenderBatches(U32 type) const
	{
		return sCull->hasRenderMap(type);
	}

	LL_INLINE LLCullResult::drawinfo_iterator beginRenderMap(U32 type)
	{
		return sCull->beginRenderMap(type);
	}

	LL_INLINE LLCullResult::drawinfo_iterator endRenderMap(U32 type)
	{
		return sCull->endRenderMap(type);
	}

	LL_INLINE LLCullResult::sg_iterator beginAlphaGroups()
	{
		return sCull->beginAlphaGroups();
	}

	LL_INLINE LLCullResult::sg_iterator endAlphaGroups()
	{
		return sCull->endAlphaGroups();
	}

	void addTrianglesDrawn(S32 index_count,
						   LLRender::eGeomModes mode = LLRender::TRIANGLES);

	LL_INLINE bool hasRenderDebugFeatureMask(U32 mask) const
	{
		return (mRenderDebugFeatureMask & mask) != 0;
	}

	LL_INLINE bool hasRenderDebugMask(U32 mask) const
	{
		return (mRenderDebugMask & mask) != 0;
	}

	LL_INLINE void setRenderDebugMask(U32 mask)
	{
		mRenderDebugMask = mask;
	}

	LL_INLINE bool hasRenderType(U32 type) const
	{
    	// STORM-365: LLViewerJointAttachment::setAttachmentVisibility() is
		// setting type to 0 to actually mean "do not render". We then need to
		// test that value here and return false to prevent attachment to
		// render (in mouselook for instance).
		// *TODO: reintroduce RENDER_TYPE_NONE in LLRenderTypeMask and
		// initialize its mRenderTypeEnabled[RENDER_TYPE_NONE] to false
		// explicitely
		return type != 0 && mRenderTypeEnabled[type];
	}

	bool hasAnyRenderType(U32 type, ...) const;

	void setRenderTypeMask(U32 type, ...);
	void orRenderTypeMask(U32 type, ...);
	void andRenderTypeMask(U32 type, ...);
	void clearRenderTypeMask(U32 type, ...);
	void setAllRenderTypes();

	void pushRenderTypeMask();
	void popRenderTypeMask();

	static void toggleRenderType(U32 type);

	// For UI control of render features
	static bool hasRenderTypeControl(void* data);
	static void toggleRenderDebug(void* data);
	static void toggleRenderDebugFeature(void* data);
	static void toggleRenderTypeControl(void* data);
	static bool toggleRenderTypeControlNegated(void* data);
	static bool toggleRenderDebugControl(void* data);
	static bool toggleRenderDebugFeatureControl(void* data);
	static void setRenderDebugFeatureControl(U32 bit, bool value);
	// sets which UV setup to display in highlight overlay
	static void setRenderHighlightTextureChannel(LLRender::eTexIndex channel)
	{
		sRenderHighlightTextureChannel = channel;
	}
 
	static void updateRenderDeferred();
	static void refreshCachedSettings();

	static void throttleNewMemoryAllocation(bool disable);

	void addDebugBlip(const LLVector3& position, const LLColor4& color);

	LLSpatialPartition* getSpatialPartition(LLViewerObject* vobj);

private:
	void createAuxVBs();
	void unloadShaders();
	void addToQuickLookup(LLDrawPool* new_poolp);
	void removeFromQuickLookup(LLDrawPool* poolp);
	bool updateDrawableGeom(LLDrawable* drawable, bool priority);

	void connectRefreshCachedSettingsSafe(const char* name);

public:
	enum { GPU_CLASS_MAX = 3 };

	enum LLRenderTypeMask
	{
		// Following are pool types (some are also object types)
		RENDER_TYPE_SKY							= LLDrawPool::POOL_SKY,
		RENDER_TYPE_WL_SKY						= LLDrawPool::POOL_WL_SKY,
		RENDER_TYPE_GROUND						= LLDrawPool::POOL_GROUND,
		RENDER_TYPE_TERRAIN						= LLDrawPool::POOL_TERRAIN,
		RENDER_TYPE_SIMPLE						= LLDrawPool::POOL_SIMPLE,
		RENDER_TYPE_GRASS						= LLDrawPool::POOL_GRASS,
		RENDER_TYPE_ALPHA_MASK					= LLDrawPool::POOL_ALPHA_MASK,
		RENDER_TYPE_FULLBRIGHT_ALPHA_MASK		= LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK,
		RENDER_TYPE_FULLBRIGHT					= LLDrawPool::POOL_FULLBRIGHT,
		RENDER_TYPE_BUMP						= LLDrawPool::POOL_BUMP,
		RENDER_TYPE_MATERIALS					= LLDrawPool::POOL_MATERIALS,
		RENDER_TYPE_AVATAR						= LLDrawPool::POOL_AVATAR,
		RENDER_TYPE_PUPPET						= LLDrawPool::POOL_PUPPET,
		RENDER_TYPE_TREE						= LLDrawPool::POOL_TREE,
		RENDER_TYPE_INVISIBLE					= LLDrawPool::POOL_INVISIBLE,
		RENDER_TYPE_VOIDWATER					= LLDrawPool::POOL_VOIDWATER,
		RENDER_TYPE_WATER						= LLDrawPool::POOL_WATER,
 		RENDER_TYPE_ALPHA						= LLDrawPool::POOL_ALPHA,
		RENDER_TYPE_GLOW						= LLDrawPool::POOL_GLOW,
		RENDER_TYPE_PASS_SIMPLE 				= LLRenderPass::PASS_SIMPLE,
		RENDER_TYPE_PASS_GRASS					= LLRenderPass::PASS_GRASS,
		RENDER_TYPE_PASS_FULLBRIGHT				= LLRenderPass::PASS_FULLBRIGHT,
		RENDER_TYPE_PASS_INVISIBLE				= LLRenderPass::PASS_INVISIBLE,
		RENDER_TYPE_PASS_INVISI_SHINY			= LLRenderPass::PASS_INVISI_SHINY,
		RENDER_TYPE_PASS_FULLBRIGHT_SHINY		= LLRenderPass::PASS_FULLBRIGHT_SHINY,
		RENDER_TYPE_PASS_SHINY					= LLRenderPass::PASS_SHINY,
		RENDER_TYPE_PASS_BUMP					= LLRenderPass::PASS_BUMP,
		RENDER_TYPE_PASS_POST_BUMP				= LLRenderPass::PASS_POST_BUMP,
		RENDER_TYPE_PASS_GLOW					= LLRenderPass::PASS_GLOW,
		RENDER_TYPE_PASS_ALPHA					= LLRenderPass::PASS_ALPHA,
		RENDER_TYPE_PASS_ALPHA_MASK				= LLRenderPass::PASS_ALPHA_MASK,
		RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK	= LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK,
		RENDER_TYPE_PASS_MATERIAL				= LLRenderPass::PASS_MATERIAL,
		RENDER_TYPE_PASS_MATERIAL_ALPHA			= LLRenderPass::PASS_MATERIAL_ALPHA,
		RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK	= LLRenderPass::PASS_MATERIAL_ALPHA_MASK,
		RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE= LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
		RENDER_TYPE_PASS_SPECMAP				= LLRenderPass::PASS_SPECMAP,
		RENDER_TYPE_PASS_SPECMAP_BLEND			= LLRenderPass::PASS_SPECMAP_BLEND,
		RENDER_TYPE_PASS_SPECMAP_MASK			= LLRenderPass::PASS_SPECMAP_MASK,
		RENDER_TYPE_PASS_SPECMAP_EMISSIVE		= LLRenderPass::PASS_SPECMAP_EMISSIVE,
		RENDER_TYPE_PASS_NORMMAP				= LLRenderPass::PASS_NORMMAP,
		RENDER_TYPE_PASS_NORMMAP_BLEND			= LLRenderPass::PASS_NORMMAP_BLEND,
		RENDER_TYPE_PASS_NORMMAP_MASK			= LLRenderPass::PASS_NORMMAP_MASK,
		RENDER_TYPE_PASS_NORMMAP_EMISSIVE		= LLRenderPass::PASS_NORMMAP_EMISSIVE,
		RENDER_TYPE_PASS_NORMSPEC				= LLRenderPass::PASS_NORMSPEC,
		RENDER_TYPE_PASS_NORMSPEC_BLEND			= LLRenderPass::PASS_NORMSPEC_BLEND,
		RENDER_TYPE_PASS_NORMSPEC_MASK			= LLRenderPass::PASS_NORMSPEC_MASK,
		RENDER_TYPE_PASS_NORMSPEC_EMISSIVE		= LLRenderPass::PASS_NORMSPEC_EMISSIVE,
		// Following are object types (only used in drawable mRenderType)
		RENDER_TYPE_HUD							= LLRenderPass::NUM_RENDER_TYPES,
		RENDER_TYPE_VOLUME,
		RENDER_TYPE_PARTICLES,
		RENDER_TYPE_CLOUDS,
		RENDER_TYPE_HUD_PARTICLES,
		NUM_RENDER_TYPES,
		END_RENDER_TYPES						= NUM_RENDER_TYPES
	};

	enum LLRenderDebugFeatureMask
	{
		RENDER_DEBUG_FEATURE_UI					= 0x0001,
		RENDER_DEBUG_FEATURE_SELECTED			= 0x0002,
		RENDER_DEBUG_FEATURE_HIGHLIGHTED		= 0x0004,
		RENDER_DEBUG_FEATURE_DYNAMIC_TEXTURES	= 0x0008,
		RENDER_DEBUG_FEATURE_FLEXIBLE			= 0x0010,
		RENDER_DEBUG_FEATURE_FOG				= 0x0020,
		RENDER_DEBUG_FEATURE_FR_INFO			= 0x0080,
	};

	enum LLRenderDebugMask
	{
		RENDER_DEBUG_COMPOSITION		= 0x00000001,
		RENDER_DEBUG_VERIFY				= 0x00000002,
		RENDER_DEBUG_BBOXES				= 0x00000004,
		RENDER_DEBUG_OCTREE				= 0x00000008,
		RENDER_DEBUG_WIND_VECTORS		= 0x00000010,
		RENDER_DEBUG_OCCLUSION			= 0x00000020,
		RENDER_DEBUG_POINTS				= 0x00000040,
		RENDER_DEBUG_TEXTURE_PRIORITY	= 0x00000080,
		RENDER_DEBUG_TEXTURE_AREA		= 0x00000100,
		RENDER_DEBUG_FACE_AREA			= 0x00000200,
		RENDER_DEBUG_PARTICLES			= 0x00000400,
		RENDER_DEBUG_GLOW				= 0x00000800,
		RENDER_DEBUG_TEXTURE_ANIM		= 0x00001000,
		RENDER_DEBUG_LIGHTS				= 0x00002000,
		RENDER_DEBUG_BATCH_SIZE			= 0x00004000,
		RENDER_DEBUG_ALPHA_BINS			= 0x00008000,
		RENDER_DEBUG_RAYCAST			= 0x00010000,
		RENDER_DEBUG_AVATAR_DRAW_INFO	= 0x00020000,
		RENDER_DEBUG_SHADOW_FRUSTA		= 0x00040000,
		RENDER_DEBUG_SCULPTED			= 0x00080000,
		RENDER_DEBUG_AVATAR_VOLUME		= 0x00100000,
		RENDER_DEBUG_AVATAR_JOINTS		= 0x00200000,
		RENDER_DEBUG_BUILD_QUEUE		= 0x00400000,
		RENDER_DEBUG_AGENT_TARGET		= 0x00800000,
		RENDER_DEBUG_UPDATE_TYPE		= 0x01000000,
		RENDER_DEBUG_PHYSICS_SHAPES	 	= 0x02000000,
		RENDER_DEBUG_NORMALS			= 0x04000000,
		RENDER_DEBUG_LOD_INFO			= 0x08000000,
		RENDER_DEBUG_RENDER_COMPLEXITY	= 0x10000000,
		RENDER_DEBUG_ATTACHMENT_INFO	= 0x20000000,
		RENDER_DEBUG_TEXEL_DENSITY		= 0x40000000,
		RENDER_DEBUG_TEXTURE_SIZE		= 0x80000000
	};

public:
	// Aligned members
	alignas(16) LLMatrix4a			mShadowModelview[6];
	alignas(16) LLMatrix4a			mShadowProjection[6];
	alignas(16) LLMatrix4a			mSunShadowMatrix[6];
	LLMatrix4a						mReflectionModelView;
	LLVector4a						mTransformedSunDir;
	LLVector4a						mTransformedMoonDir;

	bool							mBackfaceCull;
	bool							mNeedsDrawStats;
	S32								mBatchCount;
	S32								mMatrixOpCount;
	S32								mTextureMatrixOps;
	S32								mMaxBatchSize;
	S32								mMinBatchSize;
	S32								mTrianglesDrawn;
	S32								mNumVisibleNodes;
	LLStat							mTrianglesDrawnStat;

	// 0 = no occlusion, 1 = read only, 2 = read/write:
	static S32						sUseOcclusion;
	static S32						sVisibleLightCount;
	static bool						sFreezeTime;
	static bool						sShowHUDAttachments;
	static bool						sDelayVBUpdate;
	static bool						sAutoMaskAlphaDeferred;
	static bool						sAutoMaskAlphaNonDeferred;
	static bool						sRenderBump;
	static bool						sBakeSunlight;
	static bool						sNoAlpha;
	static bool						sUseFarClip;
	static bool						sShadowRender;
	static bool						sWaterReflections;
	static bool						sDynamicLOD;
	static bool						sPickAvatar;
	static bool						sReflectionRender;
	static bool						sImpostorRender;
	static bool						sImpostorRenderAlphaDepthPass;
	static bool						sUnderWaterRender;
	static bool						sCanRenderGlow;
	static bool						sTextureBindTest;
	static bool						sRenderFrameTest;
	static bool						sRenderAttachedLights;
	static bool						sRenderAttachedParticles;
	static bool						sRenderDeferred;
	static bool						sRenderTransparentWater;
	static bool						sRenderWaterCull;
	static bool						sMemAllocationThrottled;
	static bool						sRenderWater;	// Used by llvosky.cpp
	static bool						sRenderingHUDs;

	// Utility buffer for rendering post effects, gets abused by
	// renderDeferredLighting
	LLPointer<LLVertexBuffer> 			mDeferredVB;

	// Utility buffer for glow combine
	LLPointer<LLVertexBuffer> 			mGlowCombineVB;

	// Utility buffer for rendering cubes, 8 vertices are corners of a cube
	// [-1, 1]
	LLPointer<LLVertexBuffer>			mCubeVB;

	LLRenderTarget						mScreen;
	LLRenderTarget						mDeferredScreen;
	LLRenderTarget						mFXAABuffer;
	LLRenderTarget						mDeferredDepth;
	LLRenderTarget						mOcclusionDepth;
	LLRenderTarget						mDeferredLight;
	LLRenderTarget						mHighlight;
	LLRenderTarget						mPhysicsDisplay;

	// Water reflection texture
	LLRenderTarget						mWaterRef;

	// Water distortion texture (refraction)
	LLRenderTarget						mWaterDis;

#if LL_USE_DYNAMIC_TEX_FBO
	LLRenderTarget						mBake;
#endif

	// Texture for making the glow
	LLRenderTarget						mGlow[3];

	// Sun shadow map
	LLRenderTarget						mShadow[6];
	LLRenderTarget						mShadowOcclusion[6];
	std::vector<LLVector3>				mShadowFrustPoints[4];
	LLCamera							mShadowCamera[8];
	LLVector3							mShadowExtents[4][2];

	LLPointer<LLDrawable>				mShadowSpotLight[2];
	F32									mSpotLightFade[2];
	LLPointer<LLDrawable>				mTargetShadowSpotLight[2];

	LLVector4							mSunClipPlanes;

	LLCullResult						mSky;
	LLCullResult						mReflectedObjects;
	LLCullResult						mRefractedObjects;

	// Noise map
	U32									mNoiseMap;
	U32									mTrueNoiseMap;
	U32									mLightFunc;

	LLColor4							mSunDiffuse;
	LLColor4							mMoonDiffuse;
	LLVector4							mSunDir;
	LLVector4							mMoonDir;

	// -1 = failed, 0 = unloaded, 1 = loaded
	S32									mVertexShadersLoaded;

	bool								mIsSunUp;
	bool								mInitialized;

protected:
	bool								mRenderTypeEnabled[NUM_RENDER_TYPES];
	std::stack<std::string>				mRenderTypeEnableStack;

	U32									mRenderDebugFeatureMask;
	U32									mRenderDebugMask;

	U32									mOldRenderDebugMask;

	// Screen texture
	U32 								mScreenWidth;
	U32 								mScreenHeight;

	LLDrawable::draw_vec_t				mMovedList;
	LLDrawable::draw_vec_t				mMovedBridge;
	LLDrawable::draw_vec_t				mShiftList;

	struct Light
	{
		LL_INLINE Light(LLDrawable* ptr, F32 d, F32 f = 0.0f)
		:	drawable(ptr),
			dist(d),
			fade(f)
		{
		}
		LLPointer<LLDrawable> drawable;
		F32 dist;
		F32 fade;

		struct compare
		{
			LL_INLINE bool operator()(const Light& a, const Light& b) const
			{
				if (a.dist < b.dist)
				{
					return true;
				}
				else if (a.dist > b.dist)
				{
					return false;
				}
				else
				{
					return a.drawable < b.drawable;
				}
			}
		};
	};
	typedef std::set<Light, Light::compare> light_set_t;

	LLDrawable::draw_set_t				mLights;
	light_set_t							mNearbyLights; // Lights near camera
	LLColor4							mHWLightColors[8];

	/////////////////////////////////////////////
	// Different queues of drawables being processed.

	LLDrawable::draw_list_t 			mBuildQ1; // Priority
	LLDrawable::draw_list_t 			mBuildQ2; // Non-priority
	LLSpatialGroup::sg_vector_t			mGroupQ1; // Priority
	LLSpatialGroup::sg_vector_t			mGroupQ2; // Non-priority
	// A place to save mGroupQ1 until it is safe to unref
	LLSpatialGroup::sg_vector_t			mGroupSaveQ1;

	// Drawables that need to update their spatial partition radius
	LLDrawable::draw_vec_t				mPartitionQ;

	// Groups that need rebuildMesh called
	LLSpatialGroup::sg_vector_t			mMeshDirtyGroup;
	U32 mMeshDirtyQueryObject;

	bool								mGroupQ2Locked;
	bool								mGroupQ1Locked;

	// If true, clear vertex buffers on next update
	bool								mResetVertexBuffers;

	LLViewerObject::vobj_list_t			mCreateQ;

	LLDrawable::draw_set_t				mRetexturedList;

	//////////////////////////////////////////////////
	// Draw pools are responsible for storing all rendered data and performing
	// the actual rendering of objects.

	struct compare_pools
	{
		LL_INLINE bool operator()(const LLDrawPool* a,
								  const LLDrawPool* b) const
		{
			if (!a)
			{
				return true;
			}
			if (!b)
			{
				return false;
			}

			S32 atype = a->getType();
			S32 btype = b->getType();
			if (atype < btype)
			{
				return true;
			}
			if (atype > btype)
			{
				return false;
			}

			return a->getId() < b->getId();
		}
	};
 	typedef std::set<LLDrawPool*, compare_pools> pool_set_t;
	pool_set_t							mPools;
	LLDrawPool*							mLastRebuildPool;

	// For quick-lookups into mPools (mapped by texture pointer)
	fast_hmap<uintptr_t, LLDrawPool*>	mTerrainPools;
	fast_hmap<uintptr_t, LLDrawPool*>	mTreePools;
	LLDrawPoolAlpha*					mAlphaPool;
	LLDrawPool*							mSkyPool;
	LLDrawPool*							mTerrainPool;
	LLDrawPool*							mWaterPool;
	LLDrawPool*							mGroundPool;
	LLRenderPass*						mSimplePool;
	LLRenderPass*						mGrassPool;
	LLRenderPass*						mAlphaMaskPool;
	LLRenderPass*						mFullbrightAlphaMaskPool;
	LLRenderPass*						mFullbrightPool;
	LLDrawPool*							mInvisiblePool;
	LLDrawPool*							mGlowPool;
	LLDrawPool*							mBumpPool;
	LLDrawPool*							mMaterialsPool;
	LLDrawPool*							mWLSkyPool;
	// Note: no need to keep an quick-lookup to avatar pools, since there is
	// only one per avatar

public:
	// beacon highlights
	std::vector<LLFace*>				mHighlightFaces;

protected:
	std::vector<LLFace*>				mSelectedFaces;

	class DebugBlip
	{
	public:
		LLColor4 mColor;
		LLVector3 mPosition;
		F32 mAge;

		DebugBlip(const LLVector3& position, const LLColor4& color)
		:	mColor(color),
			mPosition(position),
			mAge(0.f)
		{
		}
	};

	std::list<DebugBlip>				mDebugBlips;

	LLPointer<LLViewerFetchedTexture>	mFaceSelectImagep;

	U32									mLightMask;
	U32									mLightMovingMask;
	S32									mLightingDetail;

public:
	static LLCullResult*				sCull;

	// Debug use
	static U32							sCurRenderPoolType;

	static LLRender::eTexIndex			sRenderHighlightTextureChannel;

	// Cached settings:

	static LLColor4						PreviewAmbientColor;
	static LLColor4						PreviewDiffuse0;
	static LLColor4						PreviewSpecular0;
	static LLColor4						PreviewDiffuse1;
	static LLColor4						PreviewSpecular1;
	static LLColor4						PreviewDiffuse2;
	static LLColor4						PreviewSpecular2;
	static LLVector3					PreviewDirection0;
	static LLVector3					PreviewDirection1;
	static LLVector3					PreviewDirection2;
	static LLVector3					RenderGlowLumWeights;
	static LLVector3					RenderGlowWarmthWeights;
	static LLVector3					RenderSSAOEffect;
	static LLVector3					RenderShadowGaussian;
	static LLVector3					RenderShadowClipPlanes;
	static LLVector3					RenderShadowOrthoClipPlanes;
	static LLVector3					RenderShadowSplitExponent;
	static U32							sRenderByOwner;
	static F32							RenderDeferredSunWash;
	static F32							RenderDeferredDisplayGamma;
	static U32							RenderFSAASamples;
	static U32							RenderResolutionDivisor;
	static S32							RenderShadowDetail;
	static F32							RenderShadowResolutionScale;
	static S32							RenderLightingDetail;
	static U32							DebugBeaconLineWidth;
	static F32							RenderGlowMinLuminance;
	static F32							RenderGlowMaxExtractAlpha;
	static F32							RenderGlowWarmthAmount;
	static U32							RenderGlowResolutionPow;
	static U32							RenderGlowIterations;
	static F32							RenderGlowWidth;
	static F32							RenderGlowStrength;
	static F32							CameraFocusTransitionTime;
	static F32							CameraFNumber;
	static F32							CameraFocalLength;
	static F32							CameraFieldOfView;
	static F32							RenderShadowNoise;
	static F32							RenderShadowBlurSize;
	static F32							RenderSSAOScale;
	static U32							RenderSSAOMaxScale;
	static F32							RenderSSAOFactor;
	static F32							RenderShadowBiasError;
	static F32							RenderShadowOffset;
	static F32							RenderShadowOffsetNoSSAO;
	static F32							RenderShadowBias;
	static F32							RenderSpotShadowOffset;
	static F32							RenderSpotShadowBias;
	static F32							RenderShadowBlurDistFactor;
	static S32							RenderReflectionDetail;
	static F32							RenderFarClip;
	static F32							RenderShadowErrorCutoff;
	static F32							RenderShadowFOVCutoff;
	static F32							CameraMaxCoF;
	static F32							CameraDoFResScale;
	static U32							RenderAutoHideGeometryMemoryLimit;
	static F32							RenderAutoHideSurfaceAreaLimit;
	static bool							WindLightUseAtmosShaders;
	static bool							RenderAvatarVP;
	static bool							RenderDeferred;
	static bool							RenderDeferredSSAO;
	static bool							RenderDelayCreation;
	static bool							RenderAnimateRes;
	static bool							RenderSpotLightsInNondeferred;
	static bool							RenderDepthOfField;
	static bool							RenderDepthOfFieldInEditMode;
	static bool							RenderDeferredAtmospheric;
	static bool							RenderGlow;
	static bool							RenderWaterReflections;
	static bool							CameraOffset;
	static bool							sRenderScriptedBeacons;
	static bool							sRenderScriptedTouchBeacons;
	static bool							sRenderPhysicalBeacons;
	static bool							sRenderPermanentBeacons;
	static bool							sRenderCharacterBeacons;
	static bool							sRenderSoundBeacons;
	static bool							sRenderInvisibleSoundBeacons;
	static bool							sRenderParticleBeacons;
	static bool							sRenderMOAPBeacons;
	static bool							sRenderHighlight;
	static bool							sRenderBeacons;
	static bool							sRenderAttachments;

	static bool							sRenderBeaconsFloaterOpen;
};

void render_bbox(const LLVector3& min, const LLVector3& max);
void render_hud_elements();

extern LLPipeline gPipeline;
extern bool gShiftFrame;
extern const LLMatrix4* gGLLastMatrix;

#endif	// LL_LLPIPELINE_H

/**
 * @file lldrawpool.h
 * @brief LLDrawPool class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLDRAWPOOL_H
#define LL_LLDRAWPOOL_H

#include "llcolor4u.h"
#include "llvector2.h"
#include "llvector3.h"
#include "llvertexbuffer.h"

#include "llviewertexturelist.h"

class LLFace;
class LLViewerTexture;
class LLViewerFetchedTexture;
class LLSpatialGroup;
class LLDrawInfo;

class LLDrawPool
{
protected:
	LOG_CLASS(LLDrawPool);

public:
	static S32 sNumDrawPools;

	enum
	{
		// Correspond to LLPipeline render type
		POOL_GROUND = 1,
		POOL_TERRAIN,
		POOL_SIMPLE,
		POOL_FULLBRIGHT,
		POOL_BUMP,
		POOL_MATERIALS,
		POOL_TREE,
		POOL_ALPHA_MASK,
		POOL_FULLBRIGHT_ALPHA_MASK,
		POOL_SKY,
		POOL_WL_SKY,
		POOL_GRASS,
		POOL_INVISIBLE,	// See below *
		POOL_AVATAR,
		POOL_PUPPET,	// Animesh
		POOL_VOIDWATER,
		POOL_WATER,
		POOL_GLOW,
		POOL_ALPHA,
		NUM_POOL_TYPES,
		// * invisiprims work by rendering to the depth buffer but not the color
		//   buffer, occluding anything rendered after them and the LLDrawPool
		//   types enum controls what order things are rendered in so, it has
		//   absolute control over what invisprims block invisiprims being
		//   rendered in pool_invisible shiny/bump mapped objects in rendered
		//   in POOL_BUMP
	};

	LLDrawPool(U32 type);
	virtual ~LLDrawPool() = default;

	virtual bool isDead() = 0;

	LL_INLINE S32 getId() const								{ return mId; }
	LL_INLINE U32 getType() const							{ return mType; }

	LL_INLINE virtual void beginRenderPass(S32 pass)		{}
	virtual void endRenderPass(S32 pass);
	LL_INLINE virtual S32 getNumPasses()					{ return 1; }

	LL_INLINE virtual void beginDeferredPass(S32 pass)		{}
	LL_INLINE virtual void endDeferredPass(S32 pass)		{}
	LL_INLINE virtual S32 getNumDeferredPasses()			{ return 0; }
	LL_INLINE virtual void renderDeferred(S32 pass = 0)		{}

	LL_INLINE virtual void beginPostDeferredPass(S32 pass)	{}
	LL_INLINE virtual void endPostDeferredPass(S32 pass)	{}
	LL_INLINE virtual S32 getNumPostDeferredPasses()		{ return 0; }
	LL_INLINE virtual void renderPostDeferred(S32 p = 0)	{}

	LL_INLINE virtual void beginShadowPass(S32 pass)		{}
	LL_INLINE virtual void endShadowPass(S32 pass)			{}
	LL_INLINE virtual S32 getNumShadowPasses()				{ return 0; }
	LL_INLINE virtual void renderShadow(S32 pass = 0)		{}

	virtual void render(S32 pass = 0) = 0;
	virtual void prerender() = 0;
	virtual U32 getVertexDataMask() = 0;

	// Verify that all data in the draw pool is correct:
	LL_INLINE virtual bool verify() const					{ return true; }

	LL_INLINE virtual S32 getShaderLevel() const			{ return mShaderLevel; }

	static LLDrawPool* createPool(U32 type, LLViewerTexture* tex0 = NULL);
	virtual LLViewerTexture* getTexture() = 0;
	LL_INLINE virtual bool isFacePool()						{ return false; }
	LL_INLINE virtual bool isPoolTerrain()					{ return false; }
	virtual void resetDrawOrders() = 0;

protected:
	S32 mShaderLevel;
	S32	mId;
	U32 mType;				// Type of draw pool
};

class LLRenderPass : public LLDrawPool
{
public:
	enum
	{
		PASS_SIMPLE = NUM_POOL_TYPES,
		PASS_GRASS,
		PASS_FULLBRIGHT,
		PASS_INVISIBLE,
		PASS_INVISI_SHINY,
		PASS_FULLBRIGHT_SHINY,
		PASS_SHINY,
		PASS_BUMP,
		PASS_POST_BUMP,
		PASS_MATERIAL,
		PASS_MATERIAL_ALPHA,
		PASS_MATERIAL_ALPHA_MASK,
		PASS_MATERIAL_ALPHA_EMISSIVE,
		PASS_SPECMAP,
		PASS_SPECMAP_BLEND,
		PASS_SPECMAP_MASK,
		PASS_SPECMAP_EMISSIVE,
		PASS_NORMMAP,
		PASS_NORMMAP_BLEND,
		PASS_NORMMAP_MASK,
		PASS_NORMMAP_EMISSIVE,
		PASS_NORMSPEC,
		PASS_NORMSPEC_BLEND,
		PASS_NORMSPEC_MASK,
		PASS_NORMSPEC_EMISSIVE,
		PASS_GLOW,
		PASS_ALPHA,
		PASS_ALPHA_MASK,
		PASS_FULLBRIGHT_ALPHA_MASK,
		PASS_ALPHA_INVISIBLE,
		NUM_RENDER_TYPES,
	};

	LLRenderPass(U32 type);

	LL_INLINE LLViewerTexture* getTexture() override	{ return NULL; }
	LL_INLINE bool isDead() override					{ return false; }
	LL_INLINE void resetDrawOrders() override			{}

	static void applyModelMatrix(LLDrawInfo& params);
	virtual void pushBatches(U32 type, U32 mask, bool texture = true,
							 bool batch_textures = false);
	virtual void pushMaskBatches(U32 type, U32 mask, bool texture = true,
								 bool batch_textures = false);
	virtual void pushBatch(LLDrawInfo& params, U32 mask, bool texture,
						   bool batch_textures = false);
	virtual void renderGroup(LLSpatialGroup* group, U32 type, U32 mask,
							 bool texture = true);
	virtual void renderGroups(U32 type, U32 mask, bool texture = true);
	virtual void renderTexture(U32 type, U32 mask, bool batch_textures = true);
};

class LLFacePool : public LLDrawPool
{
protected:
	LOG_CLASS(LLFacePool);

public:
	typedef std::vector<LLFace*> face_array_t;

	enum
	{
		SHADER_LEVEL_SCATTERING = 2
	};

public:
	LLFacePool(U32 type);
	~LLFacePool() override;

	LL_INLINE bool isDead() override					{ return mReferences.empty(); }

	LL_INLINE LLViewerTexture* getTexture() override	{ return NULL; }

	virtual void enqueue(LLFace* face);
	virtual bool addFace(LLFace* face);
	virtual bool removeFace(LLFace* face);

	// Verify that all data in the draw pool is correct:
	bool verify() const override;

	void resetDrawOrders() override;
	void resetAll();

	void destroy();

	void buildEdges();

	void addFaceReference(LLFace* facep);
	void removeFaceReference(LLFace* facep);

	void printDebugInfo() const;

	LL_INLINE bool isFacePool() override				{ return true; }

	friend class LLFace;
	friend class LLPipeline;

public:
	face_array_t	mDrawFace;
	face_array_t	mMoveFace;
	face_array_t	mReferences;

public:
	class LLOverrideFaceColor
	{
	public:
		LLOverrideFaceColor(LLDrawPool* pool)
		:	mOverride(sOverrideFaceColor),
			mPool(pool)
		{
			sOverrideFaceColor = true;
		}

		LLOverrideFaceColor(LLDrawPool* pool, const LLColor4& color)
		:	mOverride(sOverrideFaceColor),
			mPool(pool)
		{
			sOverrideFaceColor = true;
			setColor(color);
		}

		LLOverrideFaceColor(LLDrawPool* pool, const LLColor4U& color)
		:	mOverride(sOverrideFaceColor),
			mPool(pool)
		{
			sOverrideFaceColor = true;
			setColor(color);
		}

		LLOverrideFaceColor(LLDrawPool* pool, F32 r, F32 g, F32 b, F32 a)
		:	mOverride(sOverrideFaceColor),
			mPool(pool)
		{
			sOverrideFaceColor = true;
			setColor(r, g, b, a);
		}

		~LLOverrideFaceColor()
		{
			sOverrideFaceColor = mOverride;
		}

		void setColor(const LLColor4& color);
		void setColor(const LLColor4U& color);
		void setColor(F32 r, F32 g, F32 b, F32 a);

	public:
		LLDrawPool*	mPool;
		bool		mOverride;
		static bool	sOverrideFaceColor;
	};
};

#endif //LL_LLDRAWPOOL_H

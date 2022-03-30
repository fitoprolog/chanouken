/**
 * @file llface.cpp
 * @brief LLFace class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llface.h"

#include "llfasttimer.h"
#include "llgl.h"
#include "llmatrix4a.h"
#include "llrender.h"
#include "llvolume.h"

#include "lldrawpoolavatar.h"
#include "lldrawpoolbump.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertextureanim.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoclouds.h"
#include "llvopartgroup.h"
#include "llvosky.h"
#include "llvovolume.h"

#define LL_MAX_INDICES_COUNT 1000000

static LLStaticHashedString sTextureIndexIn("texture_index_in");
static LLStaticHashedString sColorIn("color_in");

#define DOTVEC(a,b) (a.mV[0]*b.mV[0] + a.mV[1]*b.mV[1] + a.mV[2]*b.mV[2])

/*
For each vertex, given:
	B - binormal
	T - tangent
	N - normal
	P - position

The resulting texture coordinate <u,v> is:

	u = 2(B dot P)
	v = 2(T dot P)
*/
void planarProjection(LLVector2& tc, const LLVector4a& normal,
					  const LLVector4a& center, const LLVector4a& vec)
{
	LLVector4a binormal;
	F32 d = normal[0];
	if (d <= -0.5f)
	{
		binormal.set(0.f, -1.f, 0.f);
	}
	else if (d >= 0.5f)
	{
		binormal.set(0.f, 1.f, 0.f);
	}
	else if (normal[1] > 0.f)
	{
		binormal.set(-1.f, 0.f, 0.f);
	}
	else
	{
		binormal.set(1.f, 0.f, 0.f);
	}

	LLVector4a tangent;
	tangent.setCross3(binormal, normal);

	tc.mV[1] = -2.f * tangent.dot3(vec).getF32() + 0.5f;
	tc.mV[0] = 2.f * binormal.dot3(vec).getF32() + 0.5f;
}

void LLFace::init(LLDrawable* drawablep, LLViewerObject* objp)
{
	mLastUpdateTime = gFrameTimeSeconds;
	mLastMoveTime = 0.f;
	mLastSkinTime = gFrameTimeSeconds;
	mVSize = 0.f;
	mPixelArea = 16.f;
	mState = GLOBAL;
	mDrawPoolp = NULL;
	mPoolType = 0;
	mCenterLocal = objp->getPosition();
	mCenterAgent = drawablep->getPositionAgent();
	mDistance = 0.f;

	mGeomCount = 0;
	mGeomIndex = 0;
	mIndicesCount = 0;

	// Special value to indicate uninitialized position
	mIndicesIndex = 0xFFFFFFFF;

	for (U32 i = 0; i < LLRender::NUM_TEXTURE_CHANNELS; ++i)
	{
		mIndexInTex[i] = 0;
		mTexture[i] = NULL;
	}

	mTEOffset = -1;
	mTextureIndex = FACE_DO_NOT_BATCH_TEXTURES;

	setDrawable(drawablep);
	mVObjp = objp;

	mReferenceIndex = -1;

	mTextureMatrix = NULL;
	mDrawInfo = NULL;

	mFaceColor = LLColor4(1.f, 0.f, 0.f, 1.f);

	mImportanceToCamera = 0.f;
	mBoundingSphereRadius = 0.f;

	mHasMedia = false;
	mIsMediaAllowed = true;
}

void LLFace::destroy()
{
#if LL_DEBUG
	if (gDebugGL)
	{
		gPipeline.checkReferences(this);
	}
#endif

	for (U32 i = 0; i < LLRender::NUM_TEXTURE_CHANNELS; ++i)
	{
		if (mTexture[i].notNull())
		{
			mTexture[i]->removeFace(i, this);
		}
	}

	if (isState(PARTICLE))
	{
		LLVOPartGroup::freeVBSlot(mGeomIndex / 4);
		clearState(PARTICLE);
	}

	if (mDrawPoolp)
	{
		if (isState(RIGGED) &&
			(mDrawPoolp->getType() == LLDrawPool::POOL_AVATAR ||
			 mDrawPoolp->getType() == LLDrawPool::POOL_PUPPET))
		{
			LLDrawPoolAvatar* avpoolp =
				dynamic_cast<LLDrawPoolAvatar*>(mDrawPoolp);
			if (avpoolp)
			{
				avpoolp->removeRiggedFace(this);
			}
			else
			{
				llwarns << "Draw pool " << std::hex << mDrawPoolp
						<< " flagged as avatar pool for rigged face " << this
						<< std::dec << ", but not casting to LLDrawPoolAvatar"
						<< llendl;
				llassert(false);
				mDrawPoolp->removeFace(this);
			}
		}
		else
		{
			mDrawPoolp->removeFace(this);
		}

		mDrawPoolp = NULL;
	}

	if (mTextureMatrix)
	{
		delete mTextureMatrix;
		mTextureMatrix = NULL;

		if (mDrawablep.notNull())
		{
			LLSpatialGroup* group = mDrawablep->getSpatialGroup();
			if (group)
			{
				group->dirtyGeom();
				gPipeline.markRebuild(group, true);
			}
		}
	}

	setDrawInfo(NULL);

	mDrawablep = NULL;
	mVObjp = NULL;
}

void LLFace::setPool(LLFacePool* pool)
{
	mDrawPoolp = pool;
}

void LLFace::setPool(LLFacePool* new_pool, LLViewerTexture* texturep)
{
	if (!new_pool)
	{
		llerrs << "Setting pool to null !" << llendl;
	}

	if (new_pool != mDrawPoolp)
	{
		// Remove from old pool
		if (mDrawPoolp)
		{
			mDrawPoolp->removeFace(this);

			if (mDrawablep)
			{
				gPipeline.markRebuild(mDrawablep, LLDrawable::REBUILD_ALL,
									  true);
			}
		}
		mGeomIndex = 0;

		// Add to new pool
		if (new_pool)
		{
			new_pool->addFace(this);
		}
		mDrawPoolp = new_pool;
	}

	setTexture(texturep);
}

void LLFace::setTexture(U32 ch, LLViewerTexture* tex)
{
	llassert(ch < LLRender::NUM_TEXTURE_CHANNELS);

	if (mTexture[ch] == tex)
	{
		return;
	}

	if (mTexture[ch].notNull())
	{
		mTexture[ch]->removeFace(ch, this);
	}

	if (tex)
	{
		tex->addFace(ch, this);
	}

	mTexture[ch] = tex;
}

void LLFace::setTexture(LLViewerTexture* tex)
{
	setDiffuseMap(tex);
}

void LLFace::setDiffuseMap(LLViewerTexture* tex)
{
	setTexture(LLRender::DIFFUSE_MAP, tex);
}

void LLFace::setNormalMap(LLViewerTexture* tex)
{
	setTexture(LLRender::NORMAL_MAP, tex);
}

void LLFace::setSpecularMap(LLViewerTexture* tex)
{
	setTexture(LLRender::SPECULAR_MAP, tex);
}

void LLFace::dirtyTexture()
{
	LLDrawable* drawablep = getDrawable();
	if (!drawablep)
	{
		return;
	}

	if (mVObjp.notNull() && mVObjp->getVolume())
	{
		LLVOVolume* vobj = drawablep->getVOVolume();
		bool mark_rebuild = false;
		bool update_complexity = false;
		for (U32 ch = 0; ch < LLRender::NUM_TEXTURE_CHANNELS; ++ch)
		{
			if (mTexture[ch].notNull() && mTexture[ch]->getComponents() == 4)
			{
				mark_rebuild = true;
				// Dirty texture on an alpha object should be treated as an LoD
				// update
				if (vobj)
				{
					vobj->mLODChanged = true;
					// If vobj is an avatar, its render complexity may have
					// changed
					update_complexity = true;
				}
			}
		}
		if (mark_rebuild)
		{
			gPipeline.markRebuild(drawablep, LLDrawable::REBUILD_VOLUME);
		}
		if (update_complexity)
		{
			vobj->updateVisualComplexity();
		}
	}

	gPipeline.markTextured(drawablep);
}

void LLFace::notifyAboutCreatingTexture(LLViewerTexture* texture)
{
	LLDrawable* drawablep = getDrawable();
	if (drawablep && mVObjp.notNull() && mVObjp->getVolume())
	{
		LLVOVolume* vobj = drawablep->getVOVolume();
		if (vobj && vobj->notifyAboutCreatingTexture(texture))
		{
			gPipeline.markTextured(drawablep);
			gPipeline.markRebuild(drawablep, LLDrawable::REBUILD_VOLUME);
		}
	}
}

void LLFace::notifyAboutMissingAsset(LLViewerTexture* texture)
{
	LLDrawable* drawablep = getDrawable();
	if (drawablep && mVObjp.notNull() && mVObjp->getVolume())
	{
		LLVOVolume* vobj = drawablep->getVOVolume();
		if (vobj && vobj->notifyAboutMissingAsset(texture))
		{
			gPipeline.markTextured(drawablep);
			gPipeline.markRebuild(drawablep, LLDrawable::REBUILD_VOLUME);
		}
	}
}

void LLFace::switchTexture(U32 ch, LLViewerTexture* new_texture)
{
	llassert(ch < LLRender::NUM_TEXTURE_CHANNELS);

	if (mTexture[ch] == new_texture)
	{
		return;
	}

	if (!new_texture)
	{
		llerrs << "Cannot switch to a null texture." << llendl;
		return;
	}

	if (mTexture[ch].notNull())
	{
		new_texture->addTextureStats(mTexture[ch]->getMaxVirtualSize());
	}

	if (ch == LLRender::DIFFUSE_MAP)
	{
		LLViewerObject* vobj = getViewerObject();
		if (vobj)
		{
			vobj->changeTEImage(mTEOffset, new_texture);
		}
	}
	setTexture(ch, new_texture);
	dirtyTexture();
}

void LLFace::setDrawable(LLDrawable* drawable)
{
	mDrawablep = drawable;
	mXform = &drawable->mXform;
}

void LLFace::setSize(U32 num_vertices, U32 num_indices, bool align)
{
	if (align)
	{
		// Allocate vertices in blocks of 4 for alignment
		num_vertices = (num_vertices + 0x3) & ~0x3;
	}

	if (mGeomCount != num_vertices || mIndicesCount != num_indices)
	{
		mGeomCount = num_vertices;
		mIndicesCount = num_indices;
		mVertexBuffer = NULL;
	}

	llassert(verify());
}

void LLFace::setGeomIndex(U16 idx)
{
	if (mGeomIndex != idx)
	{
		mGeomIndex = idx;
		mVertexBuffer = NULL;
	}
}

void LLFace::setTextureIndex(U8 index)
{
	if (index != mTextureIndex)
	{
		mTextureIndex = index;

		if (mTextureIndex != FACE_DO_NOT_BATCH_TEXTURES)
		{
			mDrawablep->setState(LLDrawable::REBUILD_POSITION);
		}
		else if (mDrawInfo && !mDrawInfo->mTextureList.empty())
		{
			llwarns << "Face " << std::hex << (intptr_t)this << std::dec
					<< " with no texture index references indexed texture draw info."
					<< llendl;
		}
	}
}

void LLFace::setIndicesIndex(U32 idx)
{
	if (mIndicesIndex != idx)
	{
		mIndicesIndex = idx;
		mVertexBuffer = NULL;
	}
}

U16 LLFace::getGeometryAvatar(LLStrider<LLVector3>& vertices,
							  LLStrider<LLVector3>& normals,
							  LLStrider<LLVector2>& tex_coords,
							  LLStrider<F32>& vertex_weights,
							  LLStrider<LLVector4a>& clothing_weights)
{
	if (mVertexBuffer.notNull())
	{
		mVertexBuffer->getVertexStrider(vertices, mGeomIndex, mGeomCount);
		mVertexBuffer->getNormalStrider(normals, mGeomIndex, mGeomCount);
		mVertexBuffer->getTexCoord0Strider(tex_coords, mGeomIndex, mGeomCount);
		mVertexBuffer->getWeightStrider(vertex_weights, mGeomIndex,
										mGeomCount);
		mVertexBuffer->getClothWeightStrider(clothing_weights, mGeomIndex,
											 mGeomCount);
	}

	return mGeomIndex;
}

U16 LLFace::getGeometry(LLStrider<LLVector3>& vertices,
						LLStrider<LLVector3>& normals,
					    LLStrider<LLVector2>& tex_coords,
						LLStrider<U16> &indicesp)
{
	if (mVertexBuffer.notNull())
	{
		mVertexBuffer->getVertexStrider(vertices, mGeomIndex, mGeomCount);
		if (mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_NORMAL))
		{
			mVertexBuffer->getNormalStrider(normals, mGeomIndex, mGeomCount);
		}
		if (mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0))
		{
			mVertexBuffer->getTexCoord0Strider(tex_coords, mGeomIndex,
											   mGeomCount);
		}

		mVertexBuffer->getIndexStrider(indicesp, mIndicesIndex, mIndicesCount);
	}

	return mGeomIndex;
}

void LLFace::updateCenterAgent()
{
	if (mDrawablep->isActive())
	{
		mCenterAgent = mCenterLocal * getRenderMatrix();
	}
	else
	{
		mCenterAgent = mCenterLocal;
	}
}

void LLFace::renderSelected(LLViewerTexture* imagep, const LLColor4& color)
{
	if (mDrawablep.isNull() || mDrawablep->getSpatialGroup() == NULL)
	{
		return;
	}

	mDrawablep->getSpatialGroup()->rebuildGeom();
	mDrawablep->getSpatialGroup()->rebuildMesh();

	if (mDrawablep.isNull() || mVertexBuffer.isNull())
	{
		return;
	}

	if (mGeomCount > 0 && mIndicesCount > 0)
	{
		gGL.getTexUnit(0)->bind(imagep);

		gGL.pushMatrix();
		if (mDrawablep->isActive())
		{
			gGL.multMatrix(mDrawablep->getRenderMatrix().getF32ptr());
		}
		else
		{
			gGL.multMatrix(mDrawablep->getRegion()->mRenderMatrix.getF32ptr());
		}

		if (mDrawablep->isState(LLDrawable::RIGGED))
		{
			LLVOVolume* volume = mDrawablep->getVOVolume();
			if (volume)
			{
				LLRiggedVolume* rigged = volume->getRiggedVolume();
				if (rigged)
				{
					// BENTO: called when selecting a face during edit of a
					// mesh object
					LLGLEnable offset(GL_POLYGON_OFFSET_FILL);
					glPolygonOffset(-1.f, -1.f);
					gGL.multMatrix(volume->getRelativeXform().getF32ptr());
					const LLVolumeFace& vol_face =
						rigged->getVolumeFace(getTEOffset());
					if (LLVertexBuffer::sDisableVBOMapping)
					{
						LLVertexBuffer::unbind();
						glVertexPointer(3, GL_FLOAT, 16, vol_face.mPositions);
						if (vol_face.mTexCoords)
						{
							glEnableClientState(GL_TEXTURE_COORD_ARRAY);
							glTexCoordPointer(2, GL_FLOAT, 8,
											  vol_face.mTexCoords);
						}
						gGL.syncMatrices();
						glDrawElements(GL_TRIANGLES, vol_face.mNumIndices,
									   GL_UNSIGNED_SHORT, vol_face.mIndices);
						glDisableClientState(GL_TEXTURE_COORD_ARRAY);
					}
					else
					{
						LLGLSLShader* prev_shader = NULL;
						if (LLGLSLShader::sCurBoundShaderPtr != &gHighlightProgram &&
							gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_INTERFACE))
						{
							prev_shader = LLGLSLShader::sCurBoundShaderPtr;
							gHighlightProgram.bind();
						}
						gGL.diffuseColor4fv(color.mV);
						LLVertexBuffer::drawElements(LLRender::TRIANGLES,
													 vol_face.mNumVertices,
													 vol_face.mPositions,
													 vol_face.mTexCoords,
													 vol_face.mNumIndices,
													 vol_face.mIndices);
						if (prev_shader)
						{
							prev_shader->bind();
						}
					}
				}
			}
		}
		else
		{
			gGL.diffuseColor4fv(color.mV);
			LLGLEnable poly_offset(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(-1.f, -1.f);
			// Disable per-vertex color to prevent fixed-function pipeline from
			// using it. We want glColor color, not vertex color !
			mVertexBuffer->setBuffer(mVertexBuffer->getTypeMask() &
									 ~LLVertexBuffer::MAP_COLOR);
			mVertexBuffer->draw(LLRender::TRIANGLES, mIndicesCount,
								mIndicesIndex);
		}

		gGL.popMatrix();
	}
}

void LLFace::setDrawInfo(LLDrawInfo* draw_info)
{
	if (draw_info)
	{
		if (draw_info->mFace)
		{
			draw_info->mFace->setDrawInfo(NULL);
		}
		draw_info->mFace = this;
	}

	if (mDrawInfo)
	{
		mDrawInfo->mFace = NULL;
	}

	mDrawInfo = draw_info;
}

void LLFace::printDebugInfo() const
{
	LLFacePool* poolp = getPool();
	llinfos << "Object: " << getViewerObject()->mID << llendl;
	if (getDrawable().notNull())
	{
		llinfos << "Type: "
				<< LLPrimitive::pCodeToString(getDrawable()->getVObj()->getPCode())
				<< llendl;
	}
	if (getTexture())
	{
		llinfos << "Texture: " << getTexture() << " Comps: "
				<< (U32)getTexture()->getComponents() << llendl;
	}
	else
	{
		llinfos << "No texture: " << llendl;
	}

	llinfos << "Face: " << this << llendl;
	llinfos << "State: " << getState() << llendl;
	llinfos << "Geom Index Data:" << llendl;
	llinfos << "--------------------" << llendl;
	llinfos << "GI: " << mGeomIndex << " Count:" << mGeomCount << llendl;
	llinfos << "Face Index Data:" << llendl;
	llinfos << "--------------------" << llendl;
	llinfos << "II: " << mIndicesIndex << " Count:" << mIndicesCount << llendl;
	llinfos << llendl;

	if (poolp)
	{
		poolp->printDebugInfo();

		S32 pool_references = 0;
		for (std::vector<LLFace*>::iterator iter = poolp->mReferences.begin();
			 iter != poolp->mReferences.end(); iter++)
		{
			LLFace *facep = *iter;
			if (facep == this)
			{
				llinfos << "Pool reference: " << pool_references << llendl;
				pool_references++;
			}
		}

		if (pool_references != 1)
		{
			llinfos << "Incorrect number of pool references!" << llendl;
		}
	}

#if 0
	llinfos << "Indices:" << llendl;
	llinfos << "--------------------" << llendl;

	const U32* indicesp = getRawIndices();
	S32 indices_count = getIndicesCount();
	S32 geom_start = getGeomStart();

	for (S32 i = 0; i < indices_count; ++i)
	{
		llinfos << i << ":" << indicesp[i] << ":"
				<< (S32)(indicesp[i] - geom_start) << llendl;
	}
	llinfos << llendl;

	llinfos << "Vertices:" << llendl;
	llinfos << "--------------------" << llendl;
	for (S32 i = 0; i < mGeomCount; ++i)
	{
		llinfos << mGeomIndex + i << ":" << poolp->getVertex(mGeomIndex + i)
				<< llendl;
	}
	llinfos << llendl;
#endif
}

// Transform the texture coordinates for this face.
static void xform(LLVector2& tex_coord, F32 cos_ang, F32 sin_ang, F32 off_s,
				  F32 off_t, F32 mag_s, F32 mag_t)
{
	// Texture transforms are done about the center of the face.
	F32 s = tex_coord.mV[0] - 0.5f;
	F32 t = tex_coord.mV[1] - 0.5f;

	// Handle rotation
	F32 temp = s;
	s = s * cos_ang + t * sin_ang;
	t = -temp * sin_ang + t * cos_ang;

	// Then scale
	s *= mag_s;
	t *= mag_t;

	// Then offset
	s += off_s + 0.5f;
	t += off_t + 0.5f;

	tex_coord.mV[0] = s;
	tex_coord.mV[1] = t;
}

// Transform the texture coordinates for this face.
static void xform4a(LLVector4a& tex_coord, const LLVector4a& trans,
					const LLVector4Logical& mask, const LLVector4a& rot0,
					const LLVector4a& rot1, const LLVector4a& offset,
					const LLVector4a& scale)
{
	// Tex coord is two coords, <s0, t0, s1, t1>
	LLVector4a st;

	// Texture transforms are done about the center of the face.
	st.setAdd(tex_coord, trans);

	// Handle rotation
	LLVector4a rot_st;

	// <s0 * cos_ang, s0*-sin_ang, s1*cos_ang, s1*-sin_ang>
	LLVector4a s0;
	s0.splat(st, 0.f);
	LLVector4a s1;
	s1.splat(st, 2.f);
	LLVector4a ss;
	ss.setSelectWithMask(mask, s1, s0);

	LLVector4a a;
	a.setMul(rot0, ss);

	// <t0*sin_ang, t0*cos_ang, t1*sin_ang, t1*cos_ang>
	LLVector4a t0;
	t0.splat(st, 1.f);
	LLVector4a t1;
	t1.splat(st, 3.f);
	LLVector4a tt;
	tt.setSelectWithMask(mask, t1, t0);

	LLVector4a b;
	b.setMul(rot1, tt);

	st.setAdd(a, b);

	// Then scale
	st.mul(scale);

	// Then offset
	tex_coord.setAdd(st, offset);
}

#if LL_DEBUG
// Defined in llspatialpartition.cpp
extern LLVector4a gOctreeMaxMag;

bool less_than_max_mag(const LLVector4a& vec)
{
	LLVector4a val;
	val.setAbs(vec);
	return (val.lessThan(gOctreeMaxMag).getGatheredBits() & 0x7) == 0x7;
}
#endif

bool LLFace::genVolumeBBoxes(const LLVolume& volume, S32 f,
							 const LLMatrix4& mat_vert_in,
							 bool global_volume)
{
	// Get the bounding box
	if (mDrawablep->isState(LLDrawable::REBUILD_VOLUME |
							LLDrawable::REBUILD_POSITION |
							LLDrawable::REBUILD_RIGGED))
	{
		if (f >= volume.getNumVolumeFaces())
		{
			llwarns << "Attempt to generate bounding box for invalid face index !"
					<< llendl;
			return false;
		}

		const LLVolumeFace& face = volume.getVolumeFace(f);
#if 0	// Disabled: it causes rigged meshes not to rez at all !  HB
		// MAINT-8264: stray vertices, especially in low LODs, cause bounding
		// box errors.
		if (face.mNumVertices < 3)
		{
			return false;
		}
#endif
		llassert(less_than_max_mag(face.mExtents[0]));
		llassert(less_than_max_mag(face.mExtents[1]));

		// VECTORIZE THIS
		LLMatrix4a mat_vert;
		mat_vert.loadu(mat_vert_in);
		mat_vert.matMulBoundBox(face.mExtents, mExtents);

		LLVector4a& new_min = mExtents[0];
		LLVector4a& new_max = mExtents[1];

		if (!mDrawablep->isActive())
		{
			// Shift position for region
			LLVector4a offset;
			offset.load3(mDrawablep->getRegion()->getOriginAgent().mV);
			new_min.add(offset);
			new_max.add(offset);
		}

		LLVector4a t;
		t.setAdd(new_min, new_max);
		t.mul(0.5f);

		mCenterLocal.set(t.getF32ptr());

		t.setSub(new_max, new_min);
		mBoundingSphereRadius = t.getLength3().getF32() * 0.5f;

		updateCenterAgent();
	}

	return true;
}

// Converts surface coordinates to texture coordinates, based on the values in
// the texture entry; probably should be integrated with getGeometryVolume()
// for its texture coordinate generation but I'll leave that to someone more
// familiar with the implications.
LLVector2 LLFace::surfaceToTexture(LLVector2 surface_coord,
								   const LLVector4a& position,
								   const LLVector4a& normal)
{
	LLVector2 tc = surface_coord;

	const LLTextureEntry* tep = getTextureEntry();
	if (!tep)
	{
		// Cannot do much without the texture entry
		return surface_coord;
	}

	// VECTORIZE THIS
	// see if we have a non-default mapping
    U8 texgen = tep->getTexGen();
	if (texgen != LLTextureEntry::TEX_GEN_DEFAULT)
	{
		LLVOVolume* volume = mDrawablep->getVOVolume();
		if (!volume)	// Paranoia
		{
			return surface_coord;
		}

		LLVector4a& center =
			*(volume->getVolume()->getVolumeFace(mTEOffset).mCenter);

		LLVector4a volume_position;
		LLVector3 v_position(position.getF32ptr());
		volume_position.load3(volume->agentPositionToVolume(v_position).mV);

		if (!volume->isVolumeGlobal())
		{
			LLVector4a scale;
			scale.load3(mVObjp->getScale().mV);
			volume_position.mul(scale);
		}

		LLVector4a volume_normal;
		LLVector3 v_normal(normal.getF32ptr());
		volume_normal.load3(volume->agentDirectionToVolume(v_normal).mV);
		volume_normal.normalize3fast();

		if (texgen == LLTextureEntry::TEX_GEN_PLANAR)
		{
			planarProjection(tc, volume_normal, center, volume_position);
		}
	}

	if (mTextureMatrix)	// If we have a texture matrix, use it
	{
		LLVector3 tc3(tc);
		tc3 = tc3 * *mTextureMatrix;
		tc = LLVector2(tc3);
	}
	else				// Otherwise use the texture entry parameters
	{
		xform(tc, cosf(tep->getRotation()), sinf(tep->getRotation()),
			  tep->mOffsetS, tep->mOffsetT, tep->mScaleS, tep->mScaleT);
	}

	return tc;
}

// Returns scale compared to default texgen, and face orientation as calculated
// by planarProjection(). This is needed to match planar texgen parameters.
void LLFace::getPlanarProjectedParams(LLQuaternion* face_rot,
									  LLVector3* face_pos, F32* scale) const
{
	LLViewerObject* vobj = getViewerObject();
	if (!vobj) return;

	const LLVolumeFace& vf = vobj->getVolume()->getVolumeFace(mTEOffset);
	if (!vf.mNormals || !vf.mTangents) return;

	const LLVector4a& normal4a = vf.mNormals[0];
	const LLVector4a& tangent = vf.mTangents[0];

	LLVector4a binormal4a;
	binormal4a.setCross3(normal4a, tangent);
	binormal4a.mul(tangent.getF32ptr()[3]);

	LLVector2 projected_binormal;
	planarProjection(projected_binormal, normal4a, *vf.mCenter, binormal4a);

	// This normally happens in xform():
	projected_binormal -= LLVector2(0.5f, 0.5f);

	*scale = projected_binormal.length();

	// Rotate binormal to match what planarProjection() thinks it is, then find
	// rotation from that:
	projected_binormal.normalize();
	F32 ang = acosf(projected_binormal.mV[VY]);
	if (projected_binormal.mV[VX] < 0.f)
	{
		ang = -ang;
	}

	// VECTORIZE THIS
	LLVector3 binormal(binormal4a.getF32ptr());
	LLVector3 normal(normal4a.getF32ptr());
	binormal.rotVec(ang, normal);
	LLQuaternion local_rot(binormal % normal, binormal, normal);

	const LLMatrix4& vol_mat = getWorldMatrix();
	*face_rot = local_rot * vol_mat.quaternion();
	*face_pos = vol_mat.getTranslation();
}

// Returns the necessary texture transform to align this face's TE to align_to's TE
bool LLFace::calcAlignedPlanarTE(const LLFace* align_to,  LLVector2* res_st_offset,
								 LLVector2* res_st_scale, F32* res_st_rot,
								 S32 map) const
{
	if (!align_to)
	{
		return false;
	}
	const LLTextureEntry* orig_tep = align_to->getTextureEntry();
	if (!orig_tep || orig_tep->getTexGen() != LLTextureEntry::TEX_GEN_PLANAR ||
		getTextureEntry()->getTexGen() != LLTextureEntry::TEX_GEN_PLANAR)
	{
		return false;
	}

	LLMaterial* mat = orig_tep->getMaterialParams();
	if (!mat && map != LLRender::DIFFUSE_MAP)
	{
		llwarns_once << "Face " << std::hex << (intptr_t)this << std::dec
					 << " is set to use specular or normal map but has no material, defaulting to diffuse"
					 << llendl;
		map = LLRender::DIFFUSE_MAP;
	}

	F32 map_rot = 0.f;
	F32 map_scl_s = 0.f;
	F32 map_scl_t = 0.f;
	F32 map_off_s = 0.f;
	F32 map_off_t = 0.f;
	switch (map)
	{
		case LLRender::DIFFUSE_MAP:
			map_rot = orig_tep->getRotation();
			map_scl_s = orig_tep->mScaleS;
			map_scl_t = orig_tep->mScaleT;
			map_off_s = orig_tep->mOffsetS;
			map_off_t = orig_tep->mOffsetT;
			break;

		case LLRender::NORMAL_MAP:
			if (mat->getNormalID().isNull())
			{
				return false;
			}
			map_rot = mat->getNormalRotation();
			map_scl_s = mat->getNormalRepeatX();
			map_scl_t = mat->getNormalRepeatY();
			map_off_s = mat->getNormalOffsetX();
			map_off_t = mat->getNormalOffsetY();
			break;

		case LLRender::SPECULAR_MAP:
			if (mat->getSpecularID().isNull())
			{
				return false;
			}
			map_rot = mat->getSpecularRotation();
			map_scl_s = mat->getSpecularRepeatX();
			map_scl_t = mat->getSpecularRepeatY();
			map_off_s = mat->getSpecularOffsetX();
			map_off_t = mat->getSpecularOffsetY();
			break;

		default:
			return false;
	}

	LLVector3 orig_pos, this_pos;
	LLQuaternion orig_face_rot, this_face_rot;
	F32 orig_proj_scale, this_proj_scale;
	align_to->getPlanarProjectedParams(&orig_face_rot, &orig_pos,
									   &orig_proj_scale);
	getPlanarProjectedParams(&this_face_rot, &this_pos, &this_proj_scale);

	// The rotation of "this face's" texture:
	LLQuaternion orig_st_rot = LLQuaternion(map_rot, LLVector3::z_axis) *
							   orig_face_rot;
	LLQuaternion this_st_rot = orig_st_rot * ~this_face_rot;
	F32 x_ang, y_ang, z_ang;
	this_st_rot.getEulerAngles(&x_ang, &y_ang, &z_ang);
	*res_st_rot = z_ang;

	// Offset and scale of "this face's" texture:
	LLVector3 centers_dist = (this_pos - orig_pos) * ~orig_st_rot;
	LLVector3 st_scale(map_scl_s, map_scl_t, 1.f);
	st_scale *= orig_proj_scale;
	centers_dist.scaleVec(st_scale);
	LLVector2 orig_st_offset(map_off_s, map_off_t);

	*res_st_offset = orig_st_offset + (LLVector2)centers_dist;
	res_st_offset->mV[VX] -= (S32)res_st_offset->mV[VX];
	res_st_offset->mV[VY] -= (S32)res_st_offset->mV[VY];

	st_scale /= this_proj_scale;
	*res_st_scale = (LLVector2)st_scale;

	return true;
}

void LLFace::updateRebuildFlags()
{
	if (mDrawablep->isState(LLDrawable::REBUILD_VOLUME))
	{
		// This rebuild is zero overhead (direct consequence of some change
		// that affects this face)
		mLastUpdateTime = gFrameTimeSeconds;
	}
	else
	{
		// This rebuild is overhead (side effect of some change that does not
		// affect this face)
		mLastMoveTime = gFrameTimeSeconds;
	}
}

bool LLFace::canRenderAsMask()
{
	if (LLPipeline::sNoAlpha)
	{
		return true;
	}

	const LLTextureEntry* te = getTextureEntry();
	if (!te || !getViewerObject() || !getTexture())
	{
		return false;
	}

	LLMaterial* mat = te->getMaterialParams();
	if (mat &&
		mat->getDiffuseAlphaMode() == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND)
	{
		return false;
	}

	const LLVolume* volp = getViewerObject()->getVolumeConst();
	if (!volp)
	{
		return false;
	}

		// Cannot treat as mask if face is part of a flexible object
	if (!volp->isUnique() &&
		// Cannot treat as mask if we have face alpha
		te->getColor().mV[3] == 1.f &&
		// Glowing masks are hard to implement; do not mask
		te->getGlow() == 0.f &&
		// HUD attachments are NOT maskable (else they would get affected by
		// day light)
		!getViewerObject()->isHUDAttachment() &&
		// Texture actually qualifies for masking (lazily recalculated but
		// expensive)
		getTexture()->getIsAlphaMask())
	{
		if (LLPipeline::sRenderDeferred)
		{
			if (te->getFullbright())
			{
				// Fullbright objects are NOT subject to the deferred rendering
				// pipe
				return LLPipeline::sAutoMaskAlphaNonDeferred;
			}
			else
			{
				return LLPipeline::sAutoMaskAlphaDeferred;
			}
		}
		else
		{
			return LLPipeline::sAutoMaskAlphaNonDeferred;
		}
	}

	return false;
}

#if defined(GCC_VERSION)
// Work-around for a spurious "res0.LLVector4a::mQ may be used uninitialized
// in this function" warning.
# pragma GCC diagnostic ignored "-Wuninitialized"
#endif

// Do not use map range (generates many redundant unmap calls)
#define USE_MAP_RANGE 0
bool LLFace::getGeometryVolume(const LLVolume& volume, const S32& f,
							   const LLMatrix4& mat_vert_in,
							   const LLMatrix3& mat_norm_in,
							   const U16& index_offset, bool force_rebuild)
{
	LL_FAST_TIMER(FTM_FACE_GET_GEOM);
	llassert(verify());
	if (f < 0 || f >= volume.getNumVolumeFaces())
	{
		llwarns << "Attempt to get a non-existent volume face: "
				<< volume.getNumVolumeFaces()
				<< " total faces and requested face index = " << f << llendl;
		return false;
	}
	const LLVolumeFace& vf = volume.getVolumeFace(f);
	S32 num_vertices = (S32)vf.mNumVertices;
	S32 num_indices = (S32)vf.mNumIndices;
	num_vertices = llclamp(num_vertices, 0, (S32)mGeomCount);
	num_indices = llclamp(num_indices, 0, (S32)mIndicesCount);

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCTREE))
	{
		updateRebuildFlags();
	}

	if (mDrawablep.isNull())
	{
		llwarns << "NULL drawable !" << llendl;
		return false;
	}

	if (!mDrawablep->getVOVolume())
	{
		llwarns << "NULL volume !" << llendl;
		return false;
	}

	if (mVObjp.isNull())
	{
		llwarns << "NULL viewer object !" << llendl;
		return false;
	}

	if (!mVObjp->getVolume())
	{
		llwarns << "NULL viewer object volume !" << llendl;
		return false;
	}

	if (mVertexBuffer.isNull())
	{
		llwarns << "NULL vertex buffer !" << llendl;
		return false;
	}

#if USE_MAP_RANGE
	bool map_range = gGLManager.mHasMapBufferRange ||
					 gGLManager.mHasFlushBufferRange;
#else
	constexpr bool map_range = false;
#endif

	if (num_indices + (S32)mIndicesIndex > mVertexBuffer->getNumIndices())
	{
		if (gDebugGL)
		{
			llwarns << "Index buffer overflow !  Indices Count: "
					<< mIndicesCount << " - VF Num Indices: " << num_indices
					<< " -  Indices Index: " << mIndicesIndex
					<< " - VB Num Indices: " << mVertexBuffer->getNumIndices()
					<< " - Face Index: " << f << " - Pool Type: " << mPoolType
					<< llendl;
		}
		return false;
	}

	if (num_vertices + mGeomIndex > mVertexBuffer->getNumVerts())
	{
		if (gDebugGL)
		{
			llwarns << "Vertex buffer overflow !" << llendl;
		}
		return false;
	}

	if (!vf.mTexCoords || !vf.mNormals || !vf.mPositions)
	{
		llwarns_once << "vf got NULL pointer(s) !" << llendl;
		return false;
	}

	LLStrider<LLVector3> vert;
	LLStrider<LLVector2> tex_coords0;
	LLStrider<LLVector2> tex_coords1;
	LLStrider<LLVector2> tex_coords2;
	LLStrider<LLVector3> norm;
	LLStrider<LLColor4U> colors;
	LLStrider<LLVector3> tangent;
	LLStrider<U16> indicesp;
	LLStrider<LLVector4a> wght;

	bool full_rebuild = force_rebuild ||
						mDrawablep->isState(LLDrawable::REBUILD_VOLUME);

	LLVector3 scale;
	if (mDrawablep->getVOVolume()->isVolumeGlobal())
	{
		scale.set(1.f, 1.f, 1.f);
	}
	else
	{
		scale = mVObjp->getScale();
	}

	bool rebuild_pos = full_rebuild ||
					   mDrawablep->isState(LLDrawable::REBUILD_POSITION);
	bool rebuild_color = full_rebuild ||
						 mDrawablep->isState(LLDrawable::REBUILD_COLOR);
	bool rebuild_emissive =
		rebuild_color &&
		mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_EMISSIVE);
	bool rebuild_tcoord = full_rebuild ||
						  mDrawablep->isState(LLDrawable::REBUILD_TCOORD);
	bool rebuild_normal =
		rebuild_pos && mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_NORMAL);
	bool rebuild_tangent =
		rebuild_pos &&
		mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TANGENT);
	bool rebuild_weights =
		rebuild_pos &&
		mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_WEIGHT4);

	const LLTextureEntry* tep = mVObjp->getTE(f);
	if (!tep) rebuild_color = false;	// cannot get color when tep is NULL
	const U8 bump_code = tep ? tep->getBumpmap() : 0;

	if (mDrawablep->isStatic())
	{
		setState(GLOBAL);
	}
	else
	{
		clearState(GLOBAL);
	}

	LLColor4U color = (tep ? LLColor4U(tep->getColor()) : LLColor4U::white);
//MK
	if (gRLenabled && gRLInterface.mContainsCamTextures &&
		getViewerObject() && !getViewerObject()->isAttachment())
	{
		color = LLColor4::white;
	}
//mk

	if (rebuild_color)	// false if tep == NULL
	{
		// Decide if shiny goes in alpha channel of color

		// Alpha channel MUST contain transparency, not shiny:
		if (getPoolType() != LLDrawPool::POOL_ALPHA)
		{
			LLMaterial* mat = tep->getMaterialParams().get();

			bool shiny_in_alpha = false;

			if (LLPipeline::sRenderDeferred)
			{
				// Store shiny in alpha if we do not have a specular map
				if  (!mat || mat->getSpecularID().isNull())
				{
					shiny_in_alpha = true;
				}
			}
			else if (!mat ||
					 mat->getDiffuseAlphaMode() !=
						LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
			{
				shiny_in_alpha = true;
			}

			if (shiny_in_alpha)
			{
				static const LLColor4U shine_steps(0, 64, 128, 191);
				U8 index = tep->getShiny();
				if (index > 3)
				{
					llwarns << "Shiny index too large (" << index
							<< ") for face " << f << " of object "
							<< mVObjp->getID() << llendl;
					llassert(false);
					index = 3;
				}
				color.mV[3] = shine_steps.mV[tep->getShiny()];
			}
		}
	}

    // INDICES
	bool result;
	if (full_rebuild)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_INDEX);
		result = mVertexBuffer->getIndexStrider(indicesp, mIndicesIndex,
												mIndicesCount, map_range);
		if (!result)
		{
			llwarns << "getIndexStrider() failed !" << llendl;
			return false;
		}

		volatile __m128i* dst = (__m128i*)indicesp.get();
		__m128i* src = (__m128i*)vf.mIndices;
		__m128i offset = _mm_set1_epi16(index_offset);

		S32 end = num_indices / 8;

		for (S32 i = 0; i < end; ++i)
		{
			__m128i res = _mm_add_epi16(src[i], offset);
			_mm_storeu_si128((__m128i*)dst++, res);
		}

		{
			LL_FAST_TIMER(FTM_FACE_GEOM_INDEX_TAIL);
			U16* idx = (U16*)dst;

			for (S32 i = end * 8; i < num_indices; ++i)
			{
				*idx++ = vf.mIndices[i] + index_offset;
			}
		}

#if USE_MAP_RANGE
		if (map_range)
		{
			mVertexBuffer->flush();
		}
#endif
	}

	LLMatrix4a mat_normal;
	mat_normal.loadu(mat_norm_in);

	F32 r = 0.f, os = 0.f, ot = 0.f, ms = 0.f, mt = 0.f;
	F32 cos_ang = 0.f, sin_ang = 0.f;
	bool do_xform = false;
	if (rebuild_tcoord && tep)
	{
		r  = tep->getRotation();
		os = tep->mOffsetS;
		ot = tep->mOffsetT;
		ms = tep->mScaleS;
		mt = tep->mScaleT;
		cos_ang = cosf(r);
		sin_ang = sinf(r);
		do_xform = cos_ang != 1.f || sin_ang != 0.f || os != 0.f ||
				   ot != 0.f || ms != 1.f || mt != 1.f;
	}

	if (rebuild_tcoord)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_TEXTURE);

		// Bump setup
		LLVector4a binormal_dir(-sin_ang, cos_ang, 0.f);
		LLVector4a bump_s_prim_light_ray(0.f, 0.f, 0.f);
		LLVector4a bump_t_prim_light_ray(0.f, 0.f, 0.f);

		LLQuaternion bump_quat;
		if (mDrawablep->isActive())
		{
			bump_quat = LLQuaternion(mDrawablep->getRenderMatrix());
		}

		if (bump_code)
		{
			mVObjp->getVolume()->genTangents(f);
			F32 offset_multiple = 1.f / 256.f;
			switch (bump_code)
			{
				case BE_NO_BUMP:
					offset_multiple = 0.f;
					break;

				case BE_BRIGHTNESS:
				case BE_DARKNESS:
				{
					LLViewerTexture* tex =
						mTexture[LLRender::DIFFUSE_MAP].get();
					if (tex && tex->hasGLTexture())
					{
						// Offset by approximately one texel
						S32 cur_discard = tex->getDiscardLevel();
						S32 max_size = llmax(tex->getWidth(),
											 tex->getHeight());
						max_size <<= cur_discard;
						constexpr F32 ARTIFICIAL_OFFSET = 2.f;
						offset_multiple = ARTIFICIAL_OFFSET / (F32)max_size;
					}
					break;
				}

				default:  // Standard bumpmap texture assumed to be 256x256
					break;
			}

			F32 s_scale = 1.f;
			F32 t_scale = 1.f;
			if (tep)
			{
				tep->getScale(&s_scale, &t_scale);
			}

			// Use the nudged south when coming from above Sun angle, such
			// that emboss mapping always shows up on the upward faces of cubes
			// when it is noon (since a lot of builders build with the Sun
			// forced to noon).
			const LLVector3& sun_ray = gSky.mVOSkyp->mBumpSunDir;
			LLVector3 primary_light_ray;
			if (sun_ray.mV[VZ] > 0.f)
			{
				primary_light_ray = sun_ray;
			}
			else
			{
				primary_light_ray = gSky.getMoonDirection();
			}
			bump_s_prim_light_ray.load3((offset_multiple * s_scale *
											primary_light_ray).mV);
			bump_t_prim_light_ray.load3((offset_multiple * t_scale *
											primary_light_ray).mV);
		}

		const LLTextureEntry* tent = getTextureEntry();
		U8 texgen = tent ? tent->getTexGen()
						 : LLTextureEntry::TEX_GEN_DEFAULT;
		if (rebuild_tcoord && texgen != LLTextureEntry::TEX_GEN_DEFAULT)
		{
			// Planar texgen needs binormals
			mVObjp->getVolume()->genTangents(f);
		}

		LLVOVolume* vobj = (LLVOVolume*)((LLViewerObject*)mVObjp);
		U8 tex_mode = vobj->mTexAnimMode;

		// When texture animation is in play, override specular and normal map
		// tex coords with diffuse texcoords.
		bool tex_anim = vobj->mTextureAnimp != NULL;

		if (isState(TEXTURE_ANIM))
		{
			if (!tex_mode)
			{
				clearState(TEXTURE_ANIM);
			}
			else
			{
				os = ot = r = sin_ang = 0.f;
				cos_ang = ms = mt = 1.f;
				do_xform = false;
			}

			if (getVirtualSize() >= MIN_TEX_ANIM_SIZE || isState(RIGGED))
			{
				// Do not override texture transform during tc bake
				tex_mode = 0;
			}
		}

		LLVector4a scalea;
		scalea.load3(scale.mV);

		bool do_bump =
			bump_code &&
			mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD1);
		LLMaterial* mat = tep->getMaterialParams().get();
		if (mat && !do_bump)
		{
			do_bump =
				mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD1) ||
				mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD2);
		}
		bool do_tex_mat = tex_mode && mTextureMatrix;

		if (!do_bump)
		{
			// Not bump mapped, might be able to do a cheap update
			result = mVertexBuffer->getTexCoord0Strider(tex_coords0,
														mGeomIndex,
														mGeomCount);
			if (!result)
			{
				llwarns << "getTexCoord0Strider() failed !" << llendl;
				return false;
			}

			if (texgen != LLTextureEntry::TEX_GEN_PLANAR)
			{
				LL_FAST_TIMER(FTM_FACE_TEX_QUICK);
				if (!do_tex_mat)
				{
					if (!do_xform)
					{
						LL_FAST_TIMER(FTM_FACE_TEX_QUICK_NO_XFORM);
						S32 tc_size = (num_vertices * 2 * sizeof(F32) + 0xF) &
									  ~0xF;
						LLVector4a::memcpyNonAliased16((F32*)tex_coords0.get(),
													   (F32*)vf.mTexCoords,
													   tc_size);
					}
					else
					{
						LL_FAST_TIMER(FTM_FACE_TEX_QUICK_XFORM);
						F32* dst = (F32*)tex_coords0.get();
						LLVector4a* src = (LLVector4a*)vf.mTexCoords;

						LLVector4a trans;
						trans.splat(-0.5f);

						LLVector4a rot0;
						rot0.set(cos_ang, -sin_ang, cos_ang, -sin_ang);

						LLVector4a rot1;
						rot1.set(sin_ang, cos_ang, sin_ang, cos_ang);

						LLVector4a scale;
						scale.set(ms, mt, ms, mt);

						LLVector4a offset;
						offset.set(os + 0.5f, ot + 0.5f, os + 0.5f, ot + 0.5f);

						LLVector4Logical mask;
						mask.clear();
						mask.setElement<2>();
						mask.setElement<3>();

						U32 count = num_vertices / 2 + num_vertices % 2;
						for (U32 i = 0; i < count; ++i)
						{
							LLVector4a res = *src++;
							xform4a(res, trans, mask, rot0, rot1, offset,
									scale);
							res.store4a(dst);
							dst += 4;
						}
					}
				}
				else
				{
					// Do tex mat, no texgen, no bump
					for (S32 i = 0; i < num_vertices; ++i)
					{
						LLVector2 tc(vf.mTexCoords[i]);
						LLVector3 tmp(tc.mV[0], tc.mV[1], 0.f);
						tmp = tmp * *mTextureMatrix;
						tc.mV[0] = tmp.mV[0];
						tc.mV[1] = tmp.mV[1];
						*tex_coords0++ = tc;
					}
				}
			}
			else
			{
				// No bump, tex gen planar
				LL_FAST_TIMER(FTM_FACE_TEX_QUICK_PLANAR);
				if (do_tex_mat)
				{
					for (S32 i = 0; i < num_vertices; ++i)
					{
						LLVector2 tc(vf.mTexCoords[i]);
						LLVector4a& norm = vf.mNormals[i];
						LLVector4a& center = *(vf.mCenter);
						LLVector4a vec = vf.mPositions[i];
						vec.mul(scalea);
						planarProjection(tc, norm, center, vec);

						LLVector3 tmp(tc.mV[0], tc.mV[1], 0.f);
						tmp = tmp * *mTextureMatrix;
						tc.mV[0] = tmp.mV[0];
						tc.mV[1] = tmp.mV[1];

						*tex_coords0++ = tc;
					}
				}
				else
				{
					for (S32 i = 0; i < num_vertices; ++i)
					{
						LLVector2 tc(vf.mTexCoords[i]);
						LLVector4a& norm = vf.mNormals[i];
						LLVector4a& center = *(vf.mCenter);
						LLVector4a vec = vf.mPositions[i];
						vec.mul(scalea);
						planarProjection(tc, norm, center, vec);

						xform(tc, cos_ang, sin_ang, os, ot, ms, mt);

						*tex_coords0++ = tc;
					}
				}
			}

#if USE_MAP_RANGE
			if (map_range)
			{
				mVertexBuffer->flush();
			}
#endif
		}
		else
		{
			// Bump mapped or has material, just do the whole expensive loop
			LL_FAST_TIMER(FTM_FACE_TEX_DEFAULT);
			std::vector<LLVector2> bump_tc;

			if (mat && mat->getNormalID().notNull())
			{
				// Writing out normal and specular texture coordinates, not
				// bump offsets
				do_bump = false;
			}

			LLStrider<LLVector2> dst;

			for (U32 ch = 0; ch < 3; ++ch)
			{
				switch (ch)
				{
					case 0:
					{
						result = mVertexBuffer->getTexCoord0Strider(dst,
																	mGeomIndex,
																	mGeomCount,
																	map_range);
						if (!result)
						{
							llwarns << "getTexCoord0Strider() failed !"
									<< llendl;
							return false;
						}
						break;
					}

					case 1:
					{
						if (!mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD1))
						{
							continue;
						}
						result = mVertexBuffer->getTexCoord1Strider(dst,
																	mGeomIndex,
																	mGeomCount,
																	map_range);
						if (!result)
						{
							llwarns << "getTexCoord1Strider() failed !"
									<< llendl;
							return false;
						}
						if (mat && !tex_anim)
						{
							r = mat->getNormalRotation();
							mat->getNormalOffset(os, ot);
							mat->getNormalRepeat(ms, mt);

							cos_ang = cosf(r);
							sin_ang = sinf(r);
						}
						break;
					}

					case 2:
					{
						if (!mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD2))
						{
							continue;
						}
						result = mVertexBuffer->getTexCoord2Strider(dst,
																	mGeomIndex,
																	mGeomCount,
																	map_range);
						if (!result)
						{
							llwarns << "getTexCoord2Strider() failed !"
									<< llendl;
							return false;
						}
						if (mat && !tex_anim)
						{
							r = mat->getSpecularRotation();
							mat->getSpecularOffset(os, ot);
							mat->getSpecularRepeat(ms, mt);

							cos_ang = cosf(r);
							sin_ang = sinf(r);
						}
					}
				}

				if (!mat && do_bump)
				{
					bump_tc.reserve(num_vertices);
				}

				if (texgen == LLTextureEntry::TEX_GEN_PLANAR &&
					!(tex_mode && mTextureMatrix))
				{
					S32 i = 0;
#if defined(__AVX2__)
					if (num_vertices >= 8)
					{
						__m256 cos_vec = _mm256_set1_ps(cos_ang);
						__m256 sin_vec = _mm256_set1_ps(sin_ang);
						__m256 off = _mm256_set1_ps(-0.5f);
						__m256 osoff = _mm256_set1_ps(os + 0.5f);
						__m256 otoff = _mm256_set1_ps(ot + 0.5f);
						__m256 ms_vec = _mm256_set1_ps(ms);
						__m256 mt_vec = _mm256_set1_ps(mt);
						F32 sv[8], tv[8];
						LLVector4a& center = *(vf.mCenter);

						do
						{
							for (S32 j = 0; j < 8; ++j, ++i)
							{
								LLVector2 tcv(vf.mTexCoords[i]);
								LLVector4a vec = vf.mPositions[i];
								vec.mul(scalea);
								planarProjection(tcv, vf.mNormals[i], center,
												 vec);
								sv[j] = tcv.mV[0];
								tv[j] = tcv.mV[1];
							}

							__m256 svv = _mm256_loadu_ps(sv);
							__m256 tvv = _mm256_loadu_ps(tv);

							// Texture transforms are done about the center of
							// the face
							svv = _mm256_add_ps(svv, off);
							tvv = _mm256_add_ps(tvv, off);

							// Transform the texture coordinates for this face.
							__m256 coss = _mm256_mul_ps(svv, cos_vec);
							__m256 sins = _mm256_mul_ps(svv, sin_vec);
							svv = _mm256_fmadd_ps(tvv, sin_vec, coss);
							tvv = _mm256_fmsub_ps(tvv, cos_vec, sins);

							// Then scale and offset
							svv = _mm256_fmadd_ps(svv, ms_vec, osoff);
							tvv = _mm256_fmadd_ps(tvv, mt_vec, otoff);

							_mm256_storeu_ps(sv, svv);
							_mm256_storeu_ps(tv, tvv);

							for (S32 j = 0; j < 8; ++j)
							{
								LLVector2 tc(sv[j], tv[j]);
								*dst++ = tc;

								if (!mat && do_bump)
								{
									bump_tc.emplace_back(tc);
								}
							}
						}
						while (i + 8 <= num_vertices);
					}
#endif
					// SSE2 version
					if (i + 4 <= num_vertices)
					{
						__m128 cos_vec = _mm_set1_ps(cos_ang);
						__m128 sin_vec = _mm_set1_ps(sin_ang);
						__m128 off = _mm_set1_ps(-0.5f);
						__m128 osoff = _mm_set1_ps(os + 0.5f);
						__m128 otoff = _mm_set1_ps(ot + 0.5f);
						__m128 ms_vec = _mm_set1_ps(ms);
						__m128 mt_vec = _mm_set1_ps(mt);
						F32 sv[4], tv[4];
						LLVector4a& center = *(vf.mCenter);

						do
						{
							for (S32 j = 0; j < 4; ++j, ++i)
							{
								LLVector2 tcv(vf.mTexCoords[i]);
								LLVector4a vec = vf.mPositions[i];
								vec.mul(scalea);
								planarProjection(tcv, vf.mNormals[i], center,
												 vec);
								sv[j] = tcv.mV[0];
								tv[j] = tcv.mV[1];
							}

							__m128 svv = _mm_loadu_ps(sv);
							__m128 tvv = _mm_loadu_ps(tv);

							// Texture transforms are done about the center of
							// the face
							svv = _mm_add_ps(svv, off);
							tvv = _mm_add_ps(tvv, off);

							// Transform the texture coordinates for this face.
							__m128 coss = _mm_mul_ps(svv, cos_vec);
							__m128 sins = _mm_mul_ps(svv, sin_vec);
							// No fmadd/fmsub in SSE2: two steps needed...
							svv = _mm_add_ps(_mm_mul_ps(tvv, sin_vec), coss);
							tvv = _mm_sub_ps(_mm_mul_ps(tvv, cos_vec), sins);

							// Then scale and offset
							svv = _mm_add_ps(_mm_mul_ps(svv, ms_vec), osoff);
							tvv = _mm_add_ps(_mm_mul_ps(tvv, mt_vec), otoff);

							_mm_storeu_ps(sv, svv);
							_mm_storeu_ps(tv, tvv);

							for (S32 j = 0; j < 4; ++j)
							{
								LLVector2 tc(sv[j], tv[j]);
								*dst++ = tc;

								if (!mat && do_bump)
								{
									bump_tc.emplace_back(tc);
								}
							}
						}
						while (i + 4 <= num_vertices);
					}

					while (i < num_vertices)
					{
						LLVector2 tc(vf.mTexCoords[i]);
						LLVector4a& norm = vf.mNormals[i];
						LLVector4a& center = *(vf.mCenter);

						LLVector4a vec = vf.mPositions[i++];
						vec.mul(scalea);
						planarProjection(tc, norm, center, vec);

						// Texture transforms are done about the center of the face.
						F32 s = tc.mV[0] - 0.5f;
						F32 t = tc.mV[1] - 0.5f;
							
						// Handle rotation
						F32 temp = s;
						s = s * cos_ang + t * sin_ang;
						t = -temp * sin_ang + t * cos_ang;

						// Then scale
						s *= ms;
						t *= mt;
						// Then offset
						s += os + 0.5f;
						t += ot + 0.5f;
						tc.mV[0] = s;
						tc.mV[1] = t;

						*dst++ = tc;

						if (!mat && do_bump)
						{
							bump_tc.emplace_back(tc);
						}
					}
				}
				else if (tex_mode && mTextureMatrix) 
				{
					for (S32 i = 0; i < num_vertices; ++i)
					{
						LLVector2 tc(vf.mTexCoords[i]);
						if (texgen == LLTextureEntry::TEX_GEN_PLANAR)
						{
							LLVector4a& norm = vf.mNormals[i];
							LLVector4a& center = *(vf.mCenter);
							LLVector4a vec = vf.mPositions[i];
							vec.mul(scalea);
							planarProjection(tc, norm, center, vec);
						}
						LLVector3 tmp(tc.mV[0], tc.mV[1], 0.f);
						tmp = tmp * *mTextureMatrix;

						tc.mV[0] = tmp.mV[0];
						tc.mV[1] = tmp.mV[1];

						*dst++ = tc;

						if (!mat && do_bump)
						{
							bump_tc.emplace_back(tc);
						}
					}
				}
				else
				{
					S32 i = 0;
#if defined(__AVX2__)
					if (num_vertices >= 8)
					{
						__m256 cos_vec = _mm256_set1_ps(cos_ang);
						__m256 sin_vec = _mm256_set1_ps(sin_ang);
						__m256 off = _mm256_set1_ps(-0.5f);
						__m256 osoff = _mm256_set1_ps(os + 0.5f);
						__m256 otoff = _mm256_set1_ps(ot + 0.5f);
						__m256 ms_vec = _mm256_set1_ps(ms);
						__m256 mt_vec = _mm256_set1_ps(mt);
						F32 sv[8], tv[8];
						do
						{
							sv[0] = vf.mTexCoords[i].mV[0];
							tv[0] = vf.mTexCoords[i++].mV[1];
							sv[1] = vf.mTexCoords[i].mV[0];
							tv[1] = vf.mTexCoords[i++].mV[1];
							sv[2] = vf.mTexCoords[i].mV[0];
							tv[2] = vf.mTexCoords[i++].mV[1];
							sv[3] = vf.mTexCoords[i].mV[0];
							tv[3] = vf.mTexCoords[i++].mV[1];
							sv[4] = vf.mTexCoords[i].mV[0];
							tv[4] = vf.mTexCoords[i++].mV[1];
							sv[5] = vf.mTexCoords[i].mV[0];
							tv[5] = vf.mTexCoords[i++].mV[1];
							sv[6] = vf.mTexCoords[i].mV[0];
							tv[6] = vf.mTexCoords[i++].mV[1];
							sv[7] = vf.mTexCoords[i].mV[0];
							tv[7] = vf.mTexCoords[i++].mV[1];

							__m256 svv = _mm256_loadu_ps(sv);
							__m256 tvv = _mm256_loadu_ps(tv);

							// Texture transforms are done about the center of
							// the face
							svv = _mm256_add_ps(svv, off);
							tvv = _mm256_add_ps(tvv, off);

							// Transform the texture coordinates for this face.
							__m256 coss = _mm256_mul_ps(svv, cos_vec);
							__m256 sins = _mm256_mul_ps(svv, sin_vec);
							svv = _mm256_fmadd_ps(tvv, sin_vec, coss);
							tvv = _mm256_fmsub_ps(tvv, cos_vec, sins);

							// Then scale and offset
							svv = _mm256_fmadd_ps(svv, ms_vec, osoff);
							tvv = _mm256_fmadd_ps(tvv, mt_vec, otoff);

							_mm256_storeu_ps(sv, svv);
							_mm256_storeu_ps(tv, tvv);

							for (S32 j = 0; j < 8; ++j)
							{
								LLVector2 tc(sv[j], tv[j]);
								*dst++ = tc;

								if (!mat && do_bump)
								{
									bump_tc.emplace_back(tc);
								}
							}
						}
						while (i + 8 <= num_vertices);
					}
#endif
					// SSE2 version
					if (i + 4 <= num_vertices)
					{
						__m128 cos_vec = _mm_set1_ps(cos_ang);
						__m128 sin_vec = _mm_set1_ps(sin_ang);
						__m128 off = _mm_set1_ps(-0.5f);
						__m128 osoff = _mm_set1_ps(os + 0.5f);
						__m128 otoff = _mm_set1_ps(ot + 0.5f);
						__m128 ms_vec = _mm_set1_ps(ms);
						__m128 mt_vec = _mm_set1_ps(mt);
						F32 sv[4], tv[4];
						do
						{
							sv[0] = vf.mTexCoords[i].mV[0];
							tv[0] = vf.mTexCoords[i++].mV[1];
							sv[1] = vf.mTexCoords[i].mV[0];
							tv[1] = vf.mTexCoords[i++].mV[1];
							sv[2] = vf.mTexCoords[i].mV[0];
							tv[2] = vf.mTexCoords[i++].mV[1];
							sv[3] = vf.mTexCoords[i].mV[0];
							tv[3] = vf.mTexCoords[i++].mV[1];
							__m128 svv = _mm_loadu_ps(sv);
							__m128 tvv = _mm_loadu_ps(tv);

							// Texture transforms are done about the center of
							// the face
							svv = _mm_add_ps(svv, off);
							tvv = _mm_add_ps(tvv, off);

							// Transform the texture coordinates for this face.
							__m128 coss = _mm_mul_ps(svv, cos_vec);
							__m128 sins = _mm_mul_ps(svv, sin_vec);
							// No fmadd/fmsub in SSE2: two steps needed...
							svv = _mm_add_ps(_mm_mul_ps(tvv, sin_vec), coss);
							tvv = _mm_sub_ps(_mm_mul_ps(tvv, cos_vec), sins);

							// Then scale and offset
							svv = _mm_add_ps(_mm_mul_ps(svv, ms_vec), osoff);
							tvv = _mm_add_ps(_mm_mul_ps(tvv, mt_vec), otoff);

							_mm_storeu_ps(sv, svv);
							_mm_storeu_ps(tv, tvv);

							for (S32 j = 0; j < 4; ++j)
							{
								LLVector2 tc(sv[j], tv[j]);
								*dst++ = tc;

								if (!mat && do_bump)
								{
									bump_tc.emplace_back(tc);
								}
							}
						}
						while (i + 4 <= num_vertices);
					}

					while (i < num_vertices)
					{
						LLVector2 tc(vf.mTexCoords[i++]);
						xform(tc, cos_ang, sin_ang, os, ot, ms, mt);
						*dst++ = tc;

						if (!mat && do_bump)
						{
							bump_tc.emplace_back(tc);
						}
					}
				}
			}

#if USE_MAP_RANGE
			if (map_range)
			{
				mVertexBuffer->flush();
			}
#endif

			if (!mat && do_bump)
			{
				result = mVertexBuffer->getTexCoord1Strider(tex_coords1,
															mGeomIndex,
															mGeomCount,
															map_range);
				if (!result)
				{
					llwarns << "getTexCoord1Strider() failed !" << llendl;
					return false;
				}

				LLMatrix4a tangent_to_object;
				LLVector4a tangent, binorm, t, binormal;
				LLVector3 t2;
				for (S32 i = 0; i < num_vertices; ++i)
				{
					tangent = vf.mTangents[i];

					binorm.setCross3(vf.mNormals[i], tangent);
					binorm.mul(tangent.getF32ptr()[3]);

					tangent_to_object.setRows(tangent, binorm, vf.mNormals[i]);
					tangent_to_object.rotate(binormal_dir, t);

					mat_normal.rotate(t, binormal);
					// VECTORIZE THIS
					if (mDrawablep->isActive())
					{
						t2.set(binormal.getF32ptr());
						t2 *= bump_quat;
						binormal.load3(t2.mV);
					}
					binormal.normalize3fast();

					*tex_coords1++ = bump_tc[i] +
						 LLVector2(bump_s_prim_light_ray.dot3(tangent).getF32(),
								   bump_t_prim_light_ray.dot3(binormal).getF32());
				}

#if USE_MAP_RANGE
				if (map_range)
				{
					mVertexBuffer->flush();
				}
#endif
			}
		}
	}

	if (rebuild_pos)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_POSITION);
		llassert(num_vertices > 0);

		result = mVertexBuffer->getVertexStrider(vert, mGeomIndex, mGeomCount,
												 map_range);
		if (!result)
		{
			llwarns << "getVertexStrider() failed !" << llendl;
			return false;
		}

		LLVector4a* src = vf.mPositions;
		LLVector4a* end = src + num_vertices;

		LLMatrix4a mat_vert;
		mat_vert.loadu(mat_vert_in);

		S32 index =
			mTextureIndex < FACE_DO_NOT_BATCH_TEXTURES ? mTextureIndex : 0;
		llassert(index <= LLGLSLShader::sIndexedTextureChannels - 1);

		F32 val = 0.f;
		S32* vp = (S32*)&val;
		*vp = index;
		LLVector4a tex_idx(0.f, 0.f, 0.f, val);

		LLVector4Logical mask;
		mask.clear();
		mask.setElement<3>();

		F32* dst = (F32*)vert.get();
		F32* end_f32 = dst + mGeomCount * 4;

		LLVector4a res0, tmp;

		{
			LL_FAST_TIMER(FTM_FACE_POSITION_STORE);

			while (src < end)
			{
				mat_vert.affineTransform(*src++, res0);
				tmp.setSelectWithMask(mask, tex_idx, res0);
				tmp.store4a((F32*)dst);
				dst += 4;
			}
		}

		{
			LL_FAST_TIMER(FTM_FACE_POSITION_PAD);
			while (dst < end_f32)
			{
				res0.store4a((F32*)dst);
				dst += 4;
			}
		}

#if USE_MAP_RANGE
		if (map_range)
		{
			mVertexBuffer->flush();
		}
#endif
	}

	if (rebuild_normal)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_NORMAL);
		result = mVertexBuffer->getNormalStrider(norm, mGeomIndex,
												 mGeomCount, map_range);
		if (!result)
		{
			llwarns << "getNormalStrider() failed !" << llendl;
			return false;
		}

		F32* normals = (F32*)norm.get();
		LLVector4a* src = vf.mNormals;
		LLVector4a* end = src + num_vertices;
		LLVector4a normal;
		while (src < end)
		{
			mat_normal.rotate(*src++, normal);
			normal.store4a(normals);
			normals += 4;
		}

#if USE_MAP_RANGE
		if (map_range)
		{
			mVertexBuffer->flush();
		}
#endif
	}

	if (rebuild_tangent)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_TANGENT);
		result = mVertexBuffer->getTangentStrider(tangent, mGeomIndex,
												  mGeomCount, map_range);
		if (!result)
		{
			llwarns << "getTangentStrider() failed !" << llendl;
			return false;
		}

		F32* tangents = (F32*)tangent.get();

		mVObjp->getVolume()->genTangents(f);

		LLVector4Logical mask;
		mask.clear();
		mask.setElement<3>();

		LLVector4a* src = vf.mTangents;
		LLVector4a* end = vf.mTangents + num_vertices;

		LLVector4a tangent_out;
		while (src < end)
		{
			mat_normal.rotate(*src, tangent_out);
			tangent_out.normalize3fast();
			tangent_out.setSelectWithMask(mask, *src++, tangent_out);
			tangent_out.store4a(tangents);
			tangents += 4;
		}

#if USE_MAP_RANGE
		if (map_range)
		{
			mVertexBuffer->flush();
		}
#endif
	}

	if (rebuild_weights && vf.mWeights)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_WEIGHTS);
		result = mVertexBuffer->getWeight4Strider(wght, mGeomIndex, mGeomCount,
												  map_range);
		if (!result)
		{
			llwarns << "getWeight4Strider() failed !" << llendl;
			return false;
		}
#if 1
		F32* weights = (F32*)wght.get();
		LLVector4a::memcpyNonAliased16(weights, (F32*)vf.mWeights,
									   num_vertices * 4 * sizeof(F32));
#else
		for (S32 i = 0; i < num_vertices; ++i)
		{
			*wght++ = vf.mWeights[i];
		}
#endif
#if USE_MAP_RANGE
		if (map_range)
		{
			mVertexBuffer->flush();
		}
#endif
	}

	if (rebuild_color &&
		mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_COLOR))
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_COLOR);
		result = mVertexBuffer->getColorStrider(colors, mGeomIndex, mGeomCount,
												map_range);
		if (!result)
		{
			llwarns << "getColorStrider() failed !" << llendl;
			return false;
		}

		U32 vec[4];
		vec[0] = vec[1] = vec[2] = vec[3] = color.asRGBA();

		LLVector4a src;
		src.loadua((F32*)vec);

		F32* dst = (F32*)colors.get();
		S32 num_vecs = (num_vertices + 3) / 4; // Rounded up
		for (S32 i = 0; i < num_vecs; ++i)
		{
			src.store4a(dst);
			dst += 4;
		}

#if USE_MAP_RANGE
		if (map_range)
		{
			mVertexBuffer->flush();
		}
#endif
	}

	if (rebuild_emissive)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_EMISSIVE);
		LLStrider<LLColor4U> emissive;
		result = mVertexBuffer->getEmissiveStrider(emissive, mGeomIndex,
												   mGeomCount, map_range);
		if (!result)
		{
			llwarns << "getEmissiveStrider() failed !" << llendl;
			return false;
		}

		F32 glowf = llmax(0.f, getTextureEntry()->getGlow());
		U8 glow = (U8)llmin((S32)(glowf * 255.f), 255);

		LLColor4U glow4u = LLColor4U(0, 0, 0, glow);
		U32 glow32 = glow4u.asRGBA();

		U32 vec[4];
		vec[0] = vec[1] = vec[2] = vec[3] = glow32;

		LLVector4a src;
		src.loadua((F32*)vec);

		F32* dst = (F32*)emissive.get();
		S32 num_vecs = (num_vertices + 3) / 4; // Rounded up
		for (S32 i = 0; i < num_vecs; ++i)
		{
			src.store4a(dst);
			dst += 4;
		}

#if USE_MAP_RANGE
		if (map_range)
		{
			mVertexBuffer->flush();
		}
#endif
	}

	if (rebuild_tcoord)
	{
		mTexExtents[0].set(0.f, 0.f);
		mTexExtents[1].set(1.f, 1.f);
		xform(mTexExtents[0], cos_ang, sin_ang, os, ot, ms, mt);
		xform(mTexExtents[1], cos_ang, sin_ang, os, ot, ms, mt);

		F32 es = vf.mTexCoordExtents[1].mV[0] - vf.mTexCoordExtents[0].mV[0];
		F32 et = vf.mTexCoordExtents[1].mV[1] - vf.mTexCoordExtents[0].mV[1];
		mTexExtents[0][0] *= es;
		mTexExtents[1][0] *= es;
		mTexExtents[0][1] *= et;
		mTexExtents[1][1] *= et;
	}

	return true;
}

// Check if the face has a media
bool LLFace::hasMedia() const
{
	if (mHasMedia)
	{
		return true;
	}

	LLViewerTexture* tex = mTexture[LLRender::DIFFUSE_MAP].get();
	if (tex)
	{
		return tex->hasParcelMedia();
	}

	return false; // no media.
}

constexpr F32 LEAST_IMPORTANCE = 0.05f;
constexpr F32 LEAST_IMPORTANCE_FOR_LARGE_IMAGE = 0.3f;

void LLFace::resetVirtualSize()
{
	setVirtualSize(0.f);
	mImportanceToCamera = 0.f;
}

F32 LLFace::getTextureVirtualSize()
{
	F32 radius;
	F32 cos_angle_to_view_dir;
	bool in_frustum = calcPixelArea(cos_angle_to_view_dir, radius);

	if (mPixelArea < F_ALMOST_ZERO || !in_frustum)
	{
		setVirtualSize(0.f);
		return 0.f;
	}

	// Get area of circle in texture space
	LLVector2 tdim = mTexExtents[1] - mTexExtents[0];
	F32 texel_area = (tdim * 0.5f).lengthSquared() * F_PI;
	if (texel_area <= 0)
	{
		// Probably animated, use default
		texel_area = 1.f;
	}

	F32 face_area;
	if (mVObjp->isSculpted() && texel_area > 1.f)
	{
		// Sculpts can break assumptions about texel area
		face_area = mPixelArea;
	}
	else
	{
		// Apply texel area to face area to get accurate ratio
		// face_area /= llclamp(texel_area, 1.f/64.f, 16.f);
		face_area =  mPixelArea / llclamp(texel_area, 0.015625f, 128.f);
	}

	face_area = adjustPixelArea(mImportanceToCamera, face_area);

	// If it is a large image, shrink face_area by considering the partial
	// overlapping:
	if (mImportanceToCamera < 1.f &&
		face_area > LLViewerTexture::sMinLargeImageSize &&
		mImportanceToCamera > LEAST_IMPORTANCE_FOR_LARGE_IMAGE)
	{
		LLViewerTexture* tex = mTexture[LLRender::DIFFUSE_MAP].get();
		if (tex && tex->isLargeImage())
		{
			face_area *= adjustPartialOverlapPixelArea(cos_angle_to_view_dir,
													   radius);
		}
	}

	setVirtualSize(face_area);

	return face_area;
}

bool LLFace::calcPixelArea(F32& cos_angle_to_view_dir, F32& radius)
{
	// VECTORIZE THIS
	// Get area of circle around face
	LLVector4a center;
	center.load3(getPositionAgent().mV);
	LLVector4a size;
	size.setSub(mExtents[1], mExtents[0]);
	size.mul(0.5f);

	F32 size_squared = size.dot3(size).getF32();
	LLVector4a t;
	t.load3(gViewerCamera.getOrigin().mV);
	LLVector4a look_at;
	look_at.setSub(center, t);

	F32 dist = look_at.getLength3().getF32();
	dist = llmax(dist - size.getLength3().getF32(), 0.001f);
	// Ramp down distance for nearby objects
	if (dist < 16.f)
	{
		dist *= 0.0625f; // /= 16.f;
		dist *= dist;
		dist *= 16.f;
	}

	look_at.normalize3fast();

	// Get area of circle around node
	F32 app_angle = atanf(sqrtf(size_squared) / dist);
	radius = app_angle * LLDrawable::sCurPixelAngle;
	mPixelArea = radius * radius * F_PI;
	LLVector4a x_axis;
	x_axis.load3(gViewerCamera.getXAxis().mV);
	cos_angle_to_view_dir = look_at.dot3(x_axis).getF32();

	// If face has media, check if the face is out of the view frustum.
	if (hasMedia())
	{
		if (!gViewerCamera.AABBInFrustum(center, size))
		{
			mImportanceToCamera = 0.f;
			return false;
		}
		if (cos_angle_to_view_dir > gViewerCamera.getCosHalfFov())
		{
			// The center is within the view frustum
			cos_angle_to_view_dir = 1.f;
		}
		else
		{
			LLVector4a d;
			d.setSub(look_at, x_axis);

			if (dist * dist * d.dot3(d) < size_squared)
			{
				cos_angle_to_view_dir = 1.f;
			}
		}
	}

	if (dist < mBoundingSphereRadius) // Camera is very close
	{
		cos_angle_to_view_dir = 1.f;
		mImportanceToCamera = 1.f;
	}
	else
	{
		mImportanceToCamera = calcImportanceToCamera(cos_angle_to_view_dir,
													 dist);
	}

	return true;
}

// The projection of the face partially overlaps with the screen
F32 LLFace::adjustPartialOverlapPixelArea(F32 cos_angle_to_view_dir,
										  F32 radius)
{
	F32 screen_radius = (F32)llmax(gViewerWindowp->getWindowDisplayWidth(),
								   gViewerWindowp->getWindowDisplayHeight());
	F32 center_angle = acosf(cos_angle_to_view_dir);
	F32 d = center_angle * LLDrawable::sCurPixelAngle;
	if (d + radius <= screen_radius + 5.f)
	{
		return 1.f;
	}

#if 0	// Calculating the intersection area of two circles is too expensive
	F32 radius_square = radius * radius;
	F32 d_square = d * d;
	F32 screen_radius_square = screen_radius * screen_radius;
	face_area = radius_square *
				acosf((d_square + radius_square - screen_radius_square) /
					  (2.f * d * radius)) +
				screen_radius_square *
				acosf((d_square + screen_radius_square - radius_square) /
					  (2.f * d * screen_radius)) -
				0.5f * sqrtf((-d + radius + screen_radius) *
				(d + radius - screen_radius) * (d - radius + screen_radius) *
				(d + radius + screen_radius));
	return face_area;
#else	// This is a good estimation: bounding box of the bounding sphere:
	F32 alpha = llclamp(0.5f * (radius + screen_radius - d) / radius,
						0.f, 1.f);
	return alpha * alpha;
#endif
}

constexpr S8 FACE_IMPORTANCE_LEVEL = 4;

// { distance, importance_weight }
static const F32 FACE_IMPORTANCE_TO_CAMERA_OVER_DISTANCE[FACE_IMPORTANCE_LEVEL][2] =
{
	{ 16.1f, 1.f },
	{ 32.1f, 0.5f },
	{ 48.1f, 0.2f },
	{ 96.1f, 0.05f }
};

// { cosf(angle), importance_weight }
static const F32 FACE_IMPORTANCE_TO_CAMERA_OVER_ANGLE[FACE_IMPORTANCE_LEVEL][2] =
{
	{ 0.985f /*cosf(10 degrees)*/, 1.f },
	{ 0.94f  /*cosf(20 degrees)*/, 0.8f },
	{ 0.866f /*cosf(30 degrees)*/, 0.64f },
	{ 0.f, 0.36f }
};

//static
F32 LLFace::calcImportanceToCamera(F32 cos_angle_to_view_dir, F32 dist)
{
	if (cos_angle_to_view_dir <= gViewerCamera.getCosHalfFov() ||
		dist >= FACE_IMPORTANCE_TO_CAMERA_OVER_DISTANCE[FACE_IMPORTANCE_LEVEL - 1][0])
	{
		return 0.f;
	}

	if (gViewerCamera.getAverageSpeed() > 10.f ||
		gViewerCamera.getAverageAngularSpeed() > 1.f)
	{
		// If camera moves or rotates too fast, ignore the importance factor
		return 0.f;
	}

	S32 i = 0;
	while (i < FACE_IMPORTANCE_LEVEL &&
		   dist > FACE_IMPORTANCE_TO_CAMERA_OVER_DISTANCE[i][0])
	{
		 ++i;
	}
	i = llmin(i, FACE_IMPORTANCE_LEVEL - 1);
	F32 dist_factor = FACE_IMPORTANCE_TO_CAMERA_OVER_DISTANCE[i][1];

	i = 0;
	while (i < FACE_IMPORTANCE_LEVEL &&
		   cos_angle_to_view_dir < FACE_IMPORTANCE_TO_CAMERA_OVER_ANGLE[i][0])
	{
		 ++i;
	}
	i = llmin(i, FACE_IMPORTANCE_LEVEL - 1);

	return dist_factor * FACE_IMPORTANCE_TO_CAMERA_OVER_ANGLE[i][1];
}

//static
F32 LLFace::adjustPixelArea(F32 importance, F32 pixel_area)
{
	if (pixel_area > LLViewerTexture::sMaxSmallImageSize)
	{
		if (importance < LEAST_IMPORTANCE)
		{
			// If the face is not important, do not load hi-res.
			constexpr F32 MAX_LEAST_IMPORTANCE_IMAGE_SIZE = 128.f * 128.f;
			pixel_area = llmin(pixel_area * 0.5f,
							   MAX_LEAST_IMPORTANCE_IMAGE_SIZE);
		}
		else if (pixel_area > LLViewerTexture::sMinLargeImageSize)
		{
			// If is large image, shrink face_area by considering the partial
			// overlapping.
			if (importance < LEAST_IMPORTANCE_FOR_LARGE_IMAGE)
			{
				// If the face is not important, do not load hi-res.
				pixel_area = LLViewerTexture::sMinLargeImageSize;
			}
		}
	}

	return pixel_area;
}

bool LLFace::verify(const U32* indices_array) const
{
	bool ok = true;

	if (mVertexBuffer.isNull())
	{
	 	// No vertex buffer, face is implicitly valid
		return true;
	}

	// First, check whether the face data fits within the pool's range.
	if (mGeomIndex + mGeomCount > mVertexBuffer->getNumVerts())
	{
		ok = false;
		llinfos << "Face references invalid vertices !" << llendl;
	}

	S32 indices_count = (S32)getIndicesCount();
	if (!indices_count)
	{
		return true;
	}

	if (indices_count > LL_MAX_INDICES_COUNT)
	{
		ok = false;
		llinfos << "Face has bogus indices count" << llendl;
	}

	if (mIndicesIndex + mIndicesCount > (U32)mVertexBuffer->getNumIndices())
	{
		ok = false;
		llinfos << "Face references invalid indices !" << llendl;
	}

#if 0
	S32 geom_start = getGeomStart();
	S32 geom_count = mGeomCount;

	const U32* indicesp = indices_array ? indices_array + mIndicesIndex
										: getRawIndices();

	for (S32 i = 0; i < indices_count; ++i)
	{
		S32 delta = indicesp[i] - geom_start;
		if (0 > delta)
		{
			llwarns << "Face index too low !" << llendl;
			llinfos << "i:" << i << " Index:" << indicesp[i] << " GStart: "
					<< geom_start << llendl;
			ok = false;
		}
		else if (delta >= geom_count)
		{
			llwarns << "Face index too high !" << llendl;
			llinfos << "i:" << i << " Index:" << indicesp[i] << " GEnd: "
					<< geom_start + geom_count << llendl;
			ok = false;
		}
	}
#endif

	if (!ok)
	{
		printDebugInfo();
	}
	return ok;
}

const LLColor4& LLFace::getRenderColor() const
{
	if (isState(USE_FACE_COLOR))
	{
		  return mFaceColor; // Face Color
	}
	else
	{
		const LLTextureEntry* tep = getTextureEntry();
		return tep ? tep->getColor() : LLColor4::white;
	}
}

void LLFace::renderSetColor() const
{
	if (!LLFacePool::LLOverrideFaceColor::sOverrideFaceColor)
	{
		const LLColor4* color = &(getRenderColor());
		gGL.diffuseColor4fv(color->mV);
	}
}

S32 LLFace::pushVertices() const
{
	if (mIndicesCount)
	{
		LLRender::eGeomModes render_type = LLRender::TRIANGLES;
		if (mDrawInfo)
		{
			render_type = mDrawInfo->mDrawMode;
		}
		mVertexBuffer->drawRange(render_type, mGeomIndex,
								 mGeomIndex + mGeomCount - 1,
								 mIndicesCount, mIndicesIndex);
		gPipeline.addTrianglesDrawn(mIndicesCount, render_type);
	}

	return mIndicesCount;
}

const LLMatrix4& LLFace::getRenderMatrix() const
{
	return mDrawablep->getRenderMatrix();
}

S32 LLFace::renderElements() const
{
	S32 ret = 0;

	if (isState(GLOBAL))
	{
		ret = pushVertices();
	}
	else
	{
		gGL.pushMatrix();
		gGL.multMatrix(getRenderMatrix().getF32ptr());
		ret = pushVertices();
		gGL.popMatrix();
	}

	return ret;
}

S32 LLFace::renderIndexed()
{
	if (mDrawablep.isNull() || mDrawPoolp == NULL)
	{
		return 0;
	}

	return renderIndexed(mDrawPoolp->getVertexDataMask());
}

S32 LLFace::renderIndexed(U32 mask)
{
	if (mVertexBuffer.isNull())
	{
		return 0;
	}

	mVertexBuffer->setBuffer(mask);
	return renderElements();
}

S32 LLFace::getColors(LLStrider<LLColor4U>& colors)
{
	if (!mGeomCount)
	{
		return -1;
	}

	mVertexBuffer->getColorStrider(colors, mGeomIndex, mGeomCount);

	return mGeomIndex;
}

S32	LLFace::getIndices(LLStrider<U16>& indicesp)
{
	mVertexBuffer->getIndexStrider(indicesp, mIndicesIndex, mIndicesCount);
	llassert(indicesp[0] != indicesp[1]);
	return mIndicesIndex;
}

LLVector3 LLFace::getPositionAgent() const
{
	if (mDrawablep.isNull() || mDrawablep->isStatic())
	{
		return mCenterAgent;
	}
	else
	{
		return mCenterLocal * getRenderMatrix();
	}
}

LLViewerTexture* LLFace::getTexture(U32 ch) const
{
	if (ch < LLRender::NUM_TEXTURE_CHANNELS)
	{
		return mTexture[ch];
	}
	else
	{
		llassert (false);
		return NULL;
	}
}

void LLFace::setVertexBuffer(LLVertexBuffer* buffer)
{
	mVertexBuffer = buffer;
	llassert(verify());
}

void LLFace::clearVertexBuffer()
{
	if (mVertexBuffer.notNull())
	{
		mVertexBuffer = NULL;
	}
}

//static
U32 LLFace::getRiggedDataMask(U32 type)
{
	static const U32 rigged_data_mask[] =
	{
		LLDrawPoolAvatar::RIGGED_MATERIAL_MASK,
		LLDrawPoolAvatar::RIGGED_MATERIAL_ALPHA_VMASK,
		LLDrawPoolAvatar::RIGGED_MATERIAL_ALPHA_MASK_MASK,
		LLDrawPoolAvatar::RIGGED_MATERIAL_ALPHA_EMISSIVE_MASK,
		LLDrawPoolAvatar::RIGGED_SPECMAP_VMASK,
		LLDrawPoolAvatar::RIGGED_SPECMAP_BLEND_MASK,
		LLDrawPoolAvatar::RIGGED_SPECMAP_MASK_MASK,
		LLDrawPoolAvatar::RIGGED_SPECMAP_EMISSIVE_MASK,
		LLDrawPoolAvatar::RIGGED_NORMMAP_VMASK,
		LLDrawPoolAvatar::RIGGED_NORMMAP_BLEND_MASK,
		LLDrawPoolAvatar::RIGGED_NORMMAP_MASK_MASK,
		LLDrawPoolAvatar::RIGGED_NORMMAP_EMISSIVE_MASK,
		LLDrawPoolAvatar::RIGGED_NORMSPEC_VMASK,
		LLDrawPoolAvatar::RIGGED_NORMSPEC_BLEND_MASK,
		LLDrawPoolAvatar::RIGGED_NORMSPEC_MASK_MASK,
		LLDrawPoolAvatar::RIGGED_NORMSPEC_EMISSIVE_MASK,
		LLDrawPoolAvatar::RIGGED_SIMPLE_MASK,
		LLDrawPoolAvatar::RIGGED_FULLBRIGHT_MASK,
		LLDrawPoolAvatar::RIGGED_SHINY_MASK,
		LLDrawPoolAvatar::RIGGED_FULLBRIGHT_SHINY_MASK,
		LLDrawPoolAvatar::RIGGED_GLOW_MASK,
		LLDrawPoolAvatar::RIGGED_ALPHA_MASK,
		LLDrawPoolAvatar::RIGGED_FULLBRIGHT_ALPHA_MASK,
		LLDrawPoolAvatar::RIGGED_DEFERRED_BUMP_MASK,
		LLDrawPoolAvatar::RIGGED_DEFERRED_SIMPLE_MASK,
	};

	llassert(type < sizeof(rigged_data_mask) / sizeof(U32));

	return rigged_data_mask[type];
}

U32 LLFace::getRiggedVertexBufferDataMask() const
{
	U32 data_mask = 0;
	for (U32 i = 0, count = mRiggedIndex.size(); i < count; ++i)
	{
		if (mRiggedIndex[i] > -1)
		{
			data_mask |= getRiggedDataMask(i);
		}
	}

	return data_mask;
}

S32 LLFace::getRiggedIndex(U32 type) const
{
	if (mRiggedIndex.empty())
	{
		return -1;
	}

	llassert(type < mRiggedIndex.size());

	return mRiggedIndex[type];
}

void LLFace::setRiggedIndex(U32 type, S32 index)
{
	if (mRiggedIndex.empty())
	{
		mRiggedIndex.resize(LLDrawPoolAvatar::NUM_RIGGED_PASSES);
		for (U32 i = 0, count = mRiggedIndex.size(); i < count; ++i)
		{
			mRiggedIndex[i] = -1;
		}
	}

	llassert(type < mRiggedIndex.size());

	mRiggedIndex[type] = index;
}

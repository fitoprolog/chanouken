/**
 * @file lldrawpool.cpp
 * @brief LLDrawPool class implementation
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

#include "llviewerprecompiledheaders.h"

#include "lldrawpool.h"

#include "llglslshader.h"
#include "llrender.h"

#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolavatar.h"
#include "lldrawpoolbump.h"
#include "lldrawpoolground.h"
#include "lldrawpoolmaterials.h"
#include "lldrawpoolsimple.h"
#include "lldrawpoolsky.h"
#include "lldrawpooltree.h"
#include "lldrawpoolterrain.h"
#include "lldrawpoolwater.h"
#include "lldrawpoolwlsky.h"
#include "llface.h"
#include "llpipeline.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"	// For debug listing.

///////////////////////////////////////////////////////////////////////////////
// LLDrawPool class
///////////////////////////////////////////////////////////////////////////////

//static
S32 LLDrawPool::sNumDrawPools = 0;

LLDrawPool* LLDrawPool::createPool(U32 type, LLViewerTexture* tex0)
{
	LLDrawPool* poolp = NULL;
	switch (type)
	{
		case POOL_SIMPLE:
			poolp = new LLDrawPoolSimple();
			break;

		case POOL_GRASS:
			poolp = new LLDrawPoolGrass();
			break;

		case POOL_ALPHA_MASK:
			poolp = new LLDrawPoolAlphaMask();
			break;

		case POOL_FULLBRIGHT_ALPHA_MASK:
			poolp = new LLDrawPoolFullbrightAlphaMask();
			break;

		case POOL_FULLBRIGHT:
			poolp = new LLDrawPoolFullbright();
			break;

		case POOL_INVISIBLE:
			poolp = new LLDrawPoolInvisible();
			break;

		case POOL_GLOW:
			poolp = new LLDrawPoolGlow();
			break;

		case POOL_ALPHA:
			poolp = new LLDrawPoolAlpha();
			break;

		case POOL_AVATAR:
		case POOL_PUPPET:
			poolp = new LLDrawPoolAvatar(type);
			break;

		case POOL_TREE:
			poolp = new LLDrawPoolTree(tex0);
			break;

		case POOL_TERRAIN:
			poolp = new LLDrawPoolTerrain(tex0);
			break;

		case POOL_SKY:
			poolp = new LLDrawPoolSky();
			break;

		case POOL_VOIDWATER:
		case POOL_WATER:
			poolp = new LLDrawPoolWater();
			break;

		case POOL_GROUND:
			poolp = new LLDrawPoolGround();
			break;

		case POOL_BUMP:
			poolp = new LLDrawPoolBump();
			break;

		case POOL_MATERIALS:
			poolp = new LLDrawPoolMaterials();
			break;

		case POOL_WL_SKY:
			poolp = new LLDrawPoolWLSky();
			break;

		default:
			llerrs << "Unknown draw pool type !" << llendl;
	}

	llassert(poolp->mType == type);
	return poolp;
}

LLDrawPool::LLDrawPool(U32 type)
{
	mType = type;
	++sNumDrawPools;
	mId = sNumDrawPools;
	mShaderLevel = 0;
}

//virtual
void LLDrawPool::endRenderPass(S32 pass)
{
	// Make sure channel 0 is active channel
	gGL.getTexUnit(0)->activate();
}

///////////////////////////////////////////////////////////////////////////////
// LLFacePool class
///////////////////////////////////////////////////////////////////////////////

LLFacePool::LLFacePool(U32 type)
:	LLDrawPool(type)
{
	resetDrawOrders();
}

LLFacePool::~LLFacePool()
{
	destroy();
}

void LLFacePool::destroy()
{
	if (!mReferences.empty())
	{
		llinfos << mReferences.size()
				<< " references left on deletion of draw pool!" << llendl;
	}
}

void LLFacePool::enqueue(LLFace* facep)
{
	mDrawFace.push_back(facep);
}

//virtual
bool LLFacePool::addFace(LLFace* facep)
{
	addFaceReference(facep);
	return true;
}

//virtual
bool LLFacePool::removeFace(LLFace* facep)
{
	removeFaceReference(facep);
	vector_replace_with_last(mDrawFace, facep);
	return true;
}

// Not absolutely sure if we should be resetting all of the chained pools as
// well - djs
void LLFacePool::resetDrawOrders()
{
	mDrawFace.resize(0);
}

void LLFacePool::removeFaceReference(LLFace* facep)
{
	if (facep->getReferenceIndex() != -1)
	{
		if (facep->getReferenceIndex() != (S32)mReferences.size())
		{
			LLFace* back = mReferences.back();
			mReferences[facep->getReferenceIndex()] = back;
			back->setReferenceIndex(facep->getReferenceIndex());
		}
		mReferences.pop_back();
	}
	facep->setReferenceIndex(-1);
}

void LLFacePool::addFaceReference(LLFace* facep)
{
	if (facep->getReferenceIndex() == -1)
	{
		facep->setReferenceIndex(mReferences.size());
		mReferences.push_back(facep);
	}
}

bool LLFacePool::verify() const
{
	bool ok = true;

	for (std::vector<LLFace*>::const_iterator iter = mDrawFace.begin(),
											  end = mDrawFace.end();
		 iter != end; ++iter)
	{
		const LLFace* facep = *iter;
		if (facep->getPool() != this)
		{
			llinfos << "Face in wrong pool !" << llendl;
			facep->printDebugInfo();
			ok = false;
		}
		else if (!facep->verify())
		{
			ok = false;
		}
	}

	return ok;
}

void LLFacePool::printDebugInfo() const
{
	llinfos << "Pool " << this << " Type: " << getType() << llendl;
}

bool LLFacePool::LLOverrideFaceColor::sOverrideFaceColor = false;

void LLFacePool::LLOverrideFaceColor::setColor(const LLColor4& color)
{
	gGL.diffuseColor4fv(color.mV);
}

void LLFacePool::LLOverrideFaceColor::setColor(const LLColor4U& color)
{
	gGL.diffuseColor4ubv(color.mV);
}

void LLFacePool::LLOverrideFaceColor::setColor(F32 r, F32 g, F32 b, F32 a)
{
	gGL.diffuseColor4f(r, g, b, a);
}

///////////////////////////////////////////////////////////////////////////////
// LLRenderPass class
///////////////////////////////////////////////////////////////////////////////

LLRenderPass::LLRenderPass(U32 type)
:	LLDrawPool(type)
{
}

void LLRenderPass::renderGroup(LLSpatialGroup* group, U32 type, U32 mask,
							   bool texture)
{
	LLSpatialGroup::drawmap_elem_t& draw_info = group->mDrawMap[type];

	for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(),
												  end = draw_info.end();
		 k != end; ++k)
	{
		LLDrawInfo* pparams = *k;
		if (pparams)
		{
			pushBatch(*pparams, mask, texture);
		}
	}
}

void LLRenderPass::renderTexture(U32 type, U32 mask, bool batch_textures)
{
	pushBatches(type, mask, true, batch_textures);
}

void LLRenderPass::pushBatches(U32 type, U32 mask, bool texture,
							   bool batch_textures)
{
	for (LLCullResult::drawinfo_iterator i = gPipeline.beginRenderMap(type),
										 end = gPipeline.endRenderMap(type);
		 i != end; ++i)
	{
		LLDrawInfo* pparams = *i;
		if (pparams)
		{
			pushBatch(*pparams, mask, texture, batch_textures);
		}
	}
}

void LLRenderPass::pushMaskBatches(U32 type, U32 mask, bool texture,
								   bool batch_textures)
{
	for (LLCullResult::drawinfo_iterator i = gPipeline.beginRenderMap(type),
										 end = gPipeline.endRenderMap(type);
		 i != end; ++i)
	{
		LLDrawInfo* pparams = *i;
		if (pparams)
		{
			if (LLGLSLShader::sCurBoundShaderPtr)
			{
				LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(pparams->mAlphaMaskCutoff);
			}
			else
			{
				gGL.flush();
			}

			pushBatch(*pparams, mask, texture, batch_textures);
		}
	}
}

void LLRenderPass::applyModelMatrix(LLDrawInfo& params)
{
	if (params.mModelMatrix != gGLLastMatrix)
	{
		gGLLastMatrix = params.mModelMatrix;
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.loadMatrix(gGLModelView.getF32ptr());
		if (params.mModelMatrix)
		{
			gGL.multMatrix(params.mModelMatrix->getF32ptr());
		}
		++gPipeline.mMatrixOpCount;
	}
}

void LLRenderPass::pushBatch(LLDrawInfo& params, U32 mask, bool texture,
							 bool batch_textures)
{
	if (!params.mCount)
	{
		return;
	}

	applyModelMatrix(params);

	bool tex_setup = false;

	if (texture)
	{
		if (batch_textures && params.mTextureList.size() > 1)
		{
			for (U32 i = 0, count = params.mTextureList.size(); i < count; ++i)
			{
				const LLPointer<LLViewerTexture>& tex = params.mTextureList[i];
				if (tex.notNull())
				{
					gGL.getTexUnit(i)->bind(tex);
				}
			}
		}
		// Not batching textures or batch has only 1 texture: might need a
		// texture matrix.
		else if (params.mTexture.notNull())
		{
			params.mTexture->addTextureStats(params.mVSize);
			LLTexUnit* unit0 = gGL.getTexUnit(0);
			unit0->bind(params.mTexture);
			if (params.mTextureMatrix)
			{
				tex_setup = true;
				unit0->activate();
				gGL.matrixMode(LLRender::MM_TEXTURE);
				gGL.loadMatrix(params.mTextureMatrix->getF32ptr());
				++gPipeline.mTextureMatrixOps;
			}
		}
		else
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		}
	}

	if (params.mVertexBuffer.notNull())
	{
		if (params.mGroup)
		{
			params.mGroup->rebuildMesh();
		}
		params.mVertexBuffer->setBuffer(mask);
		params.mVertexBuffer->drawRange(params.mDrawMode, params.mStart,
										params.mEnd, params.mCount,
										params.mOffset);
		gPipeline.addTrianglesDrawn(params.mCount, params.mDrawMode);
	}

	if (tex_setup)
	{
		gGL.matrixMode(LLRender::MM_TEXTURE0);
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
}

void LLRenderPass::renderGroups(U32 type, U32 mask, bool texture)
{
	gPipeline.renderGroups(this, type, mask, texture);
}

/**
 * @file llvoground.cpp
 * @brief LLVOGround class implementation
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

#include "llvoground.h"

#include "lldrawable.h"
#include "lldrawpoolground.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"

LLVOGround::LLVOGround(const LLUUID& id, LLPCode pcode,
					   LLViewerRegion* regionp)
:	LLStaticViewerObject(id, pcode, regionp, true)
{
	mCanSelect = false;
}

LLDrawable* LLVOGround::createDrawable()
{
	gPipeline.allocDrawable(this);
	mDrawable->setLit(false);

	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_GROUND);
	LLDrawPoolGround* poolp = (LLDrawPoolGround*)gPipeline.getPool(LLDrawPool::POOL_GROUND);

	mDrawable->addFace(poolp, NULL);

	return mDrawable;
}

bool LLVOGround::updateGeometry(LLDrawable* drawable)
{
	LLStrider<LLVector3> verticesp;
	LLStrider<LLVector3> normalsp;
	LLStrider<LLVector2> texCoordsp;
	LLStrider<U16> indicesp;
	S32 index_offset;
	LLFace* face;

	LLDrawPoolGround* poolp = (LLDrawPoolGround*)gPipeline.getPool(LLDrawPool::POOL_GROUND);

	if (drawable->getNumFaces() < 1)
	{
		drawable->addFace(poolp, NULL);
	}
	face = drawable->getFace(0);
	if (!face)
	{
		llerrs << "Failure to allocate a new face !" << llendl;
	}

	if (!face->getVertexBuffer())
	{
		face->setSize(5, 12);
		LLVertexBuffer* buff = new LLVertexBuffer(LLDrawPoolGround::VERTEX_DATA_MASK,
												  GL_STREAM_DRAW_ARB);
		if (!buff->allocateBuffer(face->getGeomCount(),
								  face->getIndicesCount(), true))
		{
			llwarns << "Failure to allocate a vertex buffer with "
					<< face->getGeomCount() << " vertices and "
					<< face->getIndicesCount() << " indices" << llendl;
		}

		face->setGeomIndex(0);
		face->setIndicesIndex(0);
		face->setVertexBuffer(buff);
	}

	index_offset = face->getGeometry(verticesp ,normalsp, texCoordsp, indicesp);
	if (index_offset == -1)
	{
		return true;
	}

	LLVector3 at_dir = gViewerCamera.getAtAxis();
	at_dir.mV[VZ] = 0.f;
	if (at_dir.normalize() < 0.01)
	{
		// We really don't care, as we're not looking anywhere near the horizon.
	}
	LLVector3 left_dir = gViewerCamera.getLeftAxis();
	left_dir.mV[VZ] = 0.f;
	left_dir.normalize();

	// Our center top point
	LLColor4 ground_color = gSky.getSkyFogColor();
	ground_color.mV[3] = 1.f;
	face->setFaceColor(ground_color);

	*(verticesp++)  = LLVector3(64, 64, 0);
	*(verticesp++)  = LLVector3(-64, 64, 0);
	*(verticesp++)  = LLVector3(-64, -64, 0);
	*(verticesp++)  = LLVector3(64, -64, 0);
	*(verticesp++)  = LLVector3(0, 0, -1024);

	// Triangles for each side
	*indicesp++ = index_offset + 0;
	*indicesp++ = index_offset + 1;
	*indicesp++ = index_offset + 4;

	*indicesp++ = index_offset + 1;
	*indicesp++ = index_offset + 2;
	*indicesp++ = index_offset + 4;

	*indicesp++ = index_offset + 2;
	*indicesp++ = index_offset + 3;
	*indicesp++ = index_offset + 4;

	*indicesp++ = index_offset + 3;
	*indicesp++ = index_offset + 0;
	*indicesp++ = index_offset + 4;

	*(texCoordsp++) = LLVector2(0.f, 0.f);
	*(texCoordsp++) = LLVector2(1.f, 0.f);
	*(texCoordsp++) = LLVector2(1.f, 1.f);
	*(texCoordsp++) = LLVector2(0.f, 1.f);
	*(texCoordsp++) = LLVector2(0.5f, 0.5f);

	face->getVertexBuffer()->flush();

	return true;
}

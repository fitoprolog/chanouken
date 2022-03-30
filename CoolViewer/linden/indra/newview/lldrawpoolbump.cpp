/**
 * @file lldrawpoolbump.cpp
 * @brief LLDrawPoolBump class implementation
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

#include <utility>

#include "lldrawpoolbump.h"

#include "llcubemap.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llglheaders.h"
#include "llrender.h"
#include "lltextureentry.h"

#include "lldrawable.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"

// static
LLStandardBumpmap gStandardBumpmapList[TEM_BUMPMAP_COUNT];

// static
U32 LLStandardBumpmap::sStandardBumpmapCount = 0;

// static
LLBumpImageList gBumpImageList;

constexpr S32 STD_BUMP_LATEST_FILE_VERSION = 1;

constexpr U32 VERTEX_MASK_SHINY = LLVertexBuffer::MAP_VERTEX |
								  LLVertexBuffer::MAP_NORMAL |
								  LLVertexBuffer::MAP_COLOR;
constexpr U32 VERTEX_MASK_BUMP = LLVertexBuffer::MAP_VERTEX |
								 LLVertexBuffer::MAP_TEXCOORD0 |
								 LLVertexBuffer::MAP_TEXCOORD1;

U32 LLDrawPoolBump::sVertexMask = VERTEX_MASK_SHINY;

static LLGLSLShader* shader = NULL;
static S32 sCubeChannel = -1;
static S32 sDiffuseChannel = -1;
static S32 sBumpChannel = -1;

// static
void LLStandardBumpmap::init()
{
	LLStandardBumpmap::restoreGL();
}

// static
void LLStandardBumpmap::shutdown()
{
	LLStandardBumpmap::destroyGL();
}

// static
void LLStandardBumpmap::restoreGL()
{
	if (!gTextureList.isInitialized())
	{
		// Note: loading pre-configuration sometimes triggers this call.
		// But it is safe to return here because bump images will be reloaded
		// during initialization later.
		return;
	}

	// Cannot assert; we destroyGL and restoreGL a lot during *first* startup,
	// which populates this list already, THEN we explicitly init the list as
	// part of *normal* startup.  Sigh.  So clear the list every time before we
	// (re-)add the standard bumpmaps.
	//llassert(LLStandardBumpmap::sStandardBumpmapCount == 0);
	clear();
	llinfos << "Adding standard bumpmaps." << llendl;
	gStandardBumpmapList[sStandardBumpmapCount++] = LLStandardBumpmap("None");       // BE_NO_BUMP
	gStandardBumpmapList[sStandardBumpmapCount++] = LLStandardBumpmap("Brightness"); // BE_BRIGHTNESS
	gStandardBumpmapList[sStandardBumpmapCount++] = LLStandardBumpmap("Darkness");   // BE_DARKNESS

	std::string file_name = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
														   "std_bump.ini");
	LLFILE* file = LLFile::open(file_name, "rt");
	if (!file)
	{
		llwarns << "Could not open std_bump <" << file_name << ">" << llendl;
		return;
	}

	S32 file_version = 0;

	S32 fields_read = fscanf(file, "LLStandardBumpmap version %d",
							 &file_version);
	if (fields_read != 1)
	{
		llwarns << "Bad LLStandardBumpmap header" << llendl;
		LLFile::close(file);
		return;
	}

	if (file_version > STD_BUMP_LATEST_FILE_VERSION)
	{
		llwarns << "LLStandardBumpmap has newer version (" << file_version
				<< ") than viewer (" << STD_BUMP_LATEST_FILE_VERSION << ")"
				<< llendl;
		LLFile::close(file);
		return;
	}

	while (!feof(file) &&
		   LLStandardBumpmap::sStandardBumpmapCount < (U32)TEM_BUMPMAP_COUNT)
	{
		// *NOTE: This buffer size is hard coded into scanf() below.
		char label[256] = "";
		char bump_image_id[256] = "";
		fields_read = fscanf(file, "\n%255s %255s", label, bump_image_id);
		bump_image_id[UUID_STR_LENGTH - 1] = 0;	// Truncate file name to UUID
		if (fields_read == EOF)
		{
			break;
		}
		if (fields_read != 2)
		{
			llwarns << "Bad LLStandardBumpmap entry" << llendl;
			break;
		}

		gStandardBumpmapList[sStandardBumpmapCount].mLabel = label;
		gStandardBumpmapList[sStandardBumpmapCount].mImage = LLViewerTextureManager::getFetchedTexture(LLUUID(bump_image_id));
		gStandardBumpmapList[sStandardBumpmapCount].mImage->setBoostLevel(LLGLTexture::LOCAL);
		gStandardBumpmapList[sStandardBumpmapCount].mImage->setLoadedCallback(LLBumpImageList::onSourceStandardLoaded,
																			  0, true, false, NULL, NULL);
		gStandardBumpmapList[sStandardBumpmapCount++].mImage->forceToSaveRawImage(0, 30.f);
	}

	LLFile::close(file);
}

// static
void LLStandardBumpmap::clear()
{
	llinfos << "Clearing standard bumpmaps." << llendl;
	for (U32 i = 0; i < LLStandardBumpmap::sStandardBumpmapCount; ++i)
	{
		gStandardBumpmapList[i].mLabel.assign("");
		gStandardBumpmapList[i].mImage = NULL;
	}
	sStandardBumpmapCount = 0;
}

// static
void LLStandardBumpmap::destroyGL()
{
	clear();
}

////////////////////////////////////////////////////////////////

void LLDrawPoolBump::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

//virtual
S32 LLDrawPoolBump::getNumPasses()
{
	static LLCachedControl<bool> render_object_bump(gSavedSettings,
													"RenderObjectBump");
	if (render_object_bump)
	{
		if (mShaderLevel > 1)
		{
			if (LLPipeline::sImpostorRender)
			{
				return 2;
			}
			return 3;
		}
		if (LLPipeline::sImpostorRender)
		{
			return 1;
		}
		return 2;
	}
	return 0;
}

void LLDrawPoolBump::beginRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_BUMP);
	switch (pass)
	{
		case 0:
			beginShiny();
			break;

		case 1:
			if (mShaderLevel > 1)
			{
				beginFullbrightShiny();
			}
			else
			{
				beginBump();
			}
			break;

		case 2:
			beginBump();
			break;

		default:
			llassert(false);
	}
}

void LLDrawPoolBump::render(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_BUMP);

	if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_BUMP))
	{
		return;
	}

	switch (pass)
	{
		case 0:
			renderShiny();
			break;

		case 1:
			if (mShaderLevel > 1)
			{
				renderFullbrightShiny();
			}
			else
			{
				renderBump();
			}
			break;

		case 2:
			renderBump();
			break;

		default:
			llassert(false);
	}
}

void LLDrawPoolBump::endRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_BUMP);
	switch (pass)
	{
		case 0:
			endShiny();
			break;

		case 1:
			if (mShaderLevel > 1)
			{
				endFullbrightShiny();
			}
			else
			{
				endBump();
			}
			break;

		case 2:
			endBump();
			break;

		default:
			llassert(false);
	}

	// to cleanup texture channels
	LLRenderPass::endRenderPass(pass);
}

//static
void LLDrawPoolBump::beginShiny(bool invisible)
{
	LL_FAST_TIMER(FTM_RENDER_SHINY);
	if ((!invisible && !gPipeline.hasRenderBatches(LLRenderPass::PASS_SHINY))||
		(invisible && !gPipeline.hasRenderBatches(LLRenderPass::PASS_INVISI_SHINY)))
	{
		return;
	}

	mShiny = true;
	sVertexMask = VERTEX_MASK_SHINY;
	// Second pass: environment map
	if (!invisible && mShaderLevel > 1)
	{
		sVertexMask = VERTEX_MASK_SHINY | LLVertexBuffer::MAP_TEXCOORD0;
	}

	if (getShaderLevel() > 0)
	{
		if (LLPipeline::sUnderWaterRender)
		{
			shader = &gObjectShinyWaterProgram;
		}
		else
		{
			shader = &gObjectShinyProgram;
		}
		shader->bind();
		if (gUseNewShaders)
		{
			shader->uniform1i(LLShaderMgr::NO_ATMO,
							  LLPipeline::sRenderingHUDs ? 1 : 0);
		}
	}
	else
	{
		shader = NULL;
	}

	bindCubeMap(shader, mShaderLevel, sDiffuseChannel, sCubeChannel,
				invisible);

	if (mShaderLevel > 1)
	{
		// Indexed texture rendering, channel 0 is always diffuse
		sDiffuseChannel = 0;
	}
}

//static
void LLDrawPoolBump::bindCubeMap(LLGLSLShader* shader, S32 shader_level,
								 S32& diffuse_channel, S32& cube_channel,
								 bool invisible)
{
	LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
	if (!cube_map)
	{
		return;
	}

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	if (!invisible && shader)
	{
		LLMatrix4 mat(gGLModelView.getF32ptr());
		LLVector3 vec = LLVector3(gShinyOrigin) * mat;
		LLVector4 vec4(vec, gShinyOrigin.mV[3]);
		shader->uniform4fv(LLShaderMgr::SHINY_ORIGIN, 1, vec4.mV);
		if (shader_level > 1)
		{
			cube_map->setMatrix(1);
			// Make sure that texture coord generation happens for tex unit 1,
			// as this is the one we use for the cube map in the one pass shiny
			// shaders
			cube_channel = shader->enableTexture(LLShaderMgr::ENVIRONMENT_MAP,
												 LLTexUnit::TT_CUBE_MAP);
			cube_map->enableTexture(cube_channel);
			diffuse_channel = shader->enableTexture(LLShaderMgr::DIFFUSE_MAP);
		}
		else
		{
			cube_channel = shader->enableTexture(LLShaderMgr::ENVIRONMENT_MAP,
												 LLTexUnit::TT_CUBE_MAP);
			diffuse_channel = -1;
			cube_map->setMatrix(0);
			cube_map->enableTexture(cube_channel);
		}
		gGL.getTexUnit(cube_channel)->bind(cube_map);
		unit0->activate();
	}
	else
	{
		cube_channel = 0;
		diffuse_channel = -1;
		unit0->disable();
		cube_map->enableTexture(0);
		cube_map->setMatrix(0);
		unit0->bind(cube_map);
	}
}

void LLDrawPoolBump::renderShiny(bool invisible)
{
	LL_FAST_TIMER(FTM_RENDER_SHINY);
	if ((!invisible &&
		 !gPipeline.hasRenderBatches(LLRenderPass::PASS_SHINY)) ||
		(invisible &&
		 !gPipeline.hasRenderBatches(LLRenderPass::PASS_INVISI_SHINY)))
	{
		return;
	}

	if (gSky.mVOSkyp->getCubeMap())
	{
		LLGLEnable blend_enable(GL_BLEND);
		if (!invisible && mShaderLevel > 1)
		{
			LLRenderPass::pushBatches(LLRenderPass::PASS_SHINY,
									  sVertexMask | LLVertexBuffer::MAP_TEXTURE_INDEX,
									  true, true);
		}
		else if (!invisible)
		{
			renderGroups(LLRenderPass::PASS_SHINY, sVertexMask);
		}
#if 0	// Deprecated
		else // invisible
		{
			renderGroups(LLRenderPass::PASS_INVISI_SHINY, sVertexMask);
		}
#endif
	}
}

//static
void LLDrawPoolBump::unbindCubeMap(LLGLSLShader* shader, S32 shader_level,
								   S32& diffuse_channel, bool invisible)
{
	LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
	if (!cube_map)
	{
		return;
	}

	if (!invisible && shader_level > 1)
	{
		shader->disableTexture(LLShaderMgr::ENVIRONMENT_MAP,
							   LLTexUnit::TT_CUBE_MAP);

		if (gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT) > 0)
		{
			if (diffuse_channel)
			{
				shader->disableTexture(LLShaderMgr::DIFFUSE_MAP);
			}
		}
	}

	// Moved below shader->disableTexture call to avoid false alarms from
	// auto-re-enable of textures on stage 0 - MAINT-755
	cube_map->disableTexture();
	cube_map->restoreMatrix();
}

void LLDrawPoolBump::endShiny(bool invisible)
{
	LL_FAST_TIMER(FTM_RENDER_SHINY);
	if ((!invisible &&
		 !gPipeline.hasRenderBatches(LLRenderPass::PASS_SHINY)) ||
		(invisible &&
		 !gPipeline.hasRenderBatches(LLRenderPass::PASS_INVISI_SHINY)))
	{
		return;
	}

	unbindCubeMap(shader, mShaderLevel, sDiffuseChannel, invisible);
	if (shader)
	{
		shader->unbind();
	}

	sDiffuseChannel = -1;
	sCubeChannel = 0;
	mShiny = false;
}

void LLDrawPoolBump::beginFullbrightShiny()
{
	LL_FAST_TIMER(FTM_RENDER_SHINY);
	if (!gPipeline.hasRenderBatches(LLRenderPass::PASS_FULLBRIGHT_SHINY))
	{
		return;
	}

	sVertexMask = VERTEX_MASK_SHINY | LLVertexBuffer::MAP_TEXCOORD0;

	// Second pass: environment map

	if (LLPipeline::sUnderWaterRender)
	{
		shader = &gObjectFullbrightShinyWaterProgram;
	}
	else if (LLPipeline::sRenderDeferred)
	{
		shader = &gDeferredFullbrightShinyProgram;
	}
	else
	{
		shader = &gObjectFullbrightShinyProgram;
	}

	LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
	if (cube_map)
	{
		LLMatrix4 mat(gGLModelView.getF32ptr());
		shader->bind();
		if (gUseNewShaders)
		{
			shader->uniform1i(LLShaderMgr::NO_ATMO,
							  LLPipeline::sRenderingHUDs ? 1 : 0);
		}

		LLVector3 vec = LLVector3(gShinyOrigin) * mat;
		LLVector4 vec4(vec, gShinyOrigin.mV[3]);
		shader->uniform4fv(LLShaderMgr::SHINY_ORIGIN, 1, vec4.mV);

		cube_map->setMatrix(1);
		// Make sure that texture coord generation happens for tex unit 1, as
		// that's the one we use for the cube map in the one pass shiny shaders
		gGL.getTexUnit(1)->disable();
		sCubeChannel = shader->enableTexture(LLShaderMgr::ENVIRONMENT_MAP,
											 LLTexUnit::TT_CUBE_MAP);
		cube_map->enableTexture(sCubeChannel);
		sDiffuseChannel = shader->enableTexture(LLShaderMgr::DIFFUSE_MAP);

		gGL.getTexUnit(sCubeChannel)->bind(cube_map);
		gGL.getTexUnit(0)->activate();
	}

	if (mShaderLevel > 1)
	{
		// Indexed texture rendering, channel 0 is always diffuse
		sDiffuseChannel = 0;
	}

	mShiny = true;
}

void LLDrawPoolBump::renderFullbrightShiny()
{
	LL_FAST_TIMER(FTM_RENDER_SHINY);
	if (!gPipeline.hasRenderBatches(LLRenderPass::PASS_FULLBRIGHT_SHINY))
	{
		return;
	}

	if (gSky.mVOSkyp->getCubeMap())
	{
		LLGLEnable blend_enable(GL_BLEND);

		if (mShaderLevel > 1)
		{
			LLRenderPass::pushBatches(LLRenderPass::PASS_FULLBRIGHT_SHINY,
									  sVertexMask | LLVertexBuffer::MAP_TEXTURE_INDEX,
									  true, true);
		}
		else
		{
			LLRenderPass::renderTexture(LLRenderPass::PASS_FULLBRIGHT_SHINY,
										sVertexMask);
		}

		gGL.setSceneBlendType(LLRender::BT_ALPHA);
	}
}

void LLDrawPoolBump::endFullbrightShiny()
{
	LL_FAST_TIMER(FTM_RENDER_SHINY);
	if (!gPipeline.hasRenderBatches(LLRenderPass::PASS_FULLBRIGHT_SHINY))
	{
		return;
	}

	LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
	if (cube_map)
	{
		cube_map->disableTexture();
		cube_map->restoreMatrix();

#if 0
		if (sDiffuseChannel != 0)
		{
			shader->disableTexture(LLShaderMgr::DIFFUSE_MAP);
		}
		LLTexUnit* unit0 = gGL.getTexUnit(0);
		unit0->activate();
		unit0->enable(LLTexUnit::TT_TEXTURE);
#endif

		shader->unbind();
	}

#if 0
	unit0->unbind(LLTexUnit::TT_TEXTURE);
#endif

	sDiffuseChannel = -1;
	sCubeChannel = 0;
	mShiny = false;
}

void LLDrawPoolBump::renderGroup(LLSpatialGroup* group, U32 type, U32 mask,
								 bool texture)
{
	LLSpatialGroup::drawmap_elem_t& draw_info = group->mDrawMap[type];

	for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(),
												  end = draw_info.end();
		 k != end; ++k)
	{
		LLDrawInfo& params = **k;

		applyModelMatrix(params);

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
}

// static
bool LLDrawPoolBump::bindBumpMap(LLDrawInfo& params, S32 channel)
{
	return bindBumpMap(params.mBump, params.mTexture, params.mVSize, channel);
}

//static
bool LLDrawPoolBump::bindBumpMap(LLFace* face, S32 channel)
{
	const LLTextureEntry* te = face->getTextureEntry();
	return te && bindBumpMap(te->getBumpmap(), face->getTexture(),
							 face->getVirtualSize(), channel);
}

// static
bool LLDrawPoolBump::bindBumpMap(U8 bump_code, LLViewerTexture* texture,
								 F32 vsize, S32 channel)
{
	LLViewerFetchedTexture* tex =
		LLViewerTextureManager::staticCastToFetchedTexture(texture);
	if (!tex)
	{
		// If the texture is not a fetched texture
		return false;
	}

	LLViewerTexture* bump = NULL;

	switch (bump_code)
	{
		case BE_NO_BUMP:
			break;

		case BE_BRIGHTNESS:
		case BE_DARKNESS:
			bump = gBumpImageList.getBrightnessDarknessImage(tex, bump_code);
			break;

		default:
			if (bump_code < LLStandardBumpmap::sStandardBumpmapCount)
			{
				bump = gStandardBumpmapList[bump_code].mImage;
				gBumpImageList.addTextureStats(bump_code, tex->getID(), vsize);
			}
	}

	if (bump)
	{
		if (channel == -2)
		{
			gGL.getTexUnit(1)->bind(bump);
			gGL.getTexUnit(0)->bind(bump);
		}
		else
		{
			gGL.getTexUnit(channel)->bind(bump);
		}
		return true;
	}

	return false;
}

// Optional second pass: emboss bump map
//static
void LLDrawPoolBump::beginBump(U32 pass)
{
	if (!gPipeline.hasRenderBatches(pass))
	{
		return;
	}

	LL_FAST_TIMER(FTM_RENDER_BUMP);

	// Optional second pass: emboss bump map
	sVertexMask = VERTEX_MASK_BUMP;

	gObjectBumpProgram.bind();

	gGL.setSceneBlendType(LLRender::BT_MULT_X2);
	stop_glerror();
}

//static
void LLDrawPoolBump::renderBump(U32 pass)
{
	if (!gPipeline.hasRenderBatches(pass))
	{
		return;
	}

	LL_FAST_TIMER(FTM_RENDER_BUMP);
	LLGLDisable fog(GL_FOG);
	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
	LLGLEnable blend(GL_BLEND);
	gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	// Get rid of z-fighting with non-bump pass.
	LLGLEnable poly_offset(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.f, -1.f);
	renderBump(pass, sVertexMask);
}

//static
void LLDrawPoolBump::endBump(U32 pass)
{
	if (!gPipeline.hasRenderBatches(pass))
	{
		return;
	}

	gObjectBumpProgram.unbind();

	gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

S32 LLDrawPoolBump::getNumDeferredPasses()
{
	static LLCachedControl<bool> render_object_bump(gSavedSettings,
													"RenderObjectBump");
	return render_object_bump ? 1 : 0;
}

void LLDrawPoolBump::beginDeferredPass(S32 pass)
{
	if (!gPipeline.hasRenderBatches(LLRenderPass::PASS_BUMP))
	{
		return;
	}
	LL_FAST_TIMER(FTM_RENDER_BUMP);
	mShiny = true;
	gDeferredBumpProgram.bind();
	sDiffuseChannel = gDeferredBumpProgram.enableTexture(LLShaderMgr::DIFFUSE_MAP);
	sBumpChannel = gDeferredBumpProgram.enableTexture(LLShaderMgr::BUMP_MAP);
	gGL.getTexUnit(sDiffuseChannel)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(sBumpChannel)->unbind(LLTexUnit::TT_TEXTURE);
}

void LLDrawPoolBump::endDeferredPass(S32 pass)
{
	if (!gPipeline.hasRenderBatches(LLRenderPass::PASS_BUMP))
	{
		return;
	}
	LL_FAST_TIMER(FTM_RENDER_BUMP);
	mShiny = false;
	gDeferredBumpProgram.disableTexture(LLShaderMgr::DIFFUSE_MAP);
	gDeferredBumpProgram.disableTexture(LLShaderMgr::BUMP_MAP);
	gDeferredBumpProgram.unbind();
	gGL.getTexUnit(0)->activate();
}

void LLDrawPoolBump::renderDeferred(S32 pass)
{
	if (!gPipeline.hasRenderBatches(LLRenderPass::PASS_BUMP))
	{
		return;
	}
	LL_FAST_TIMER(FTM_RENDER_BUMP);

	U32 type = LLRenderPass::PASS_BUMP;
	LLCullResult::drawinfo_iterator begin = gPipeline.beginRenderMap(type);
	LLCullResult::drawinfo_iterator end = gPipeline.endRenderMap(type);

	U32 mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 |
			   LLVertexBuffer::MAP_TANGENT | LLVertexBuffer::MAP_NORMAL |
			   LLVertexBuffer::MAP_COLOR;

	for (LLCullResult::drawinfo_iterator i = begin; i != end; ++i)
	{
		LLDrawInfo& params = **i;

		gDeferredBumpProgram.setMinimumAlpha(params.mAlphaMaskCutoff);
		bindBumpMap(params, sBumpChannel);
		pushBatch(params, mask, true);
	}
}

void LLDrawPoolBump::beginPostDeferredPass(S32 pass)
{
	switch (pass)
	{
		case 0:
			beginFullbrightShiny();
			break;
		case 1:
			beginBump(LLRenderPass::PASS_POST_BUMP);
	}
}

void LLDrawPoolBump::endPostDeferredPass(S32 pass)
{
	switch (pass)
	{
		case 0:
			endFullbrightShiny();
			break;

		case 1:
			endBump(LLRenderPass::PASS_POST_BUMP);
	}

	// To disable texture channels
	LLRenderPass::endRenderPass(pass);
}

void LLDrawPoolBump::renderPostDeferred(S32 pass)
{
	switch (pass)
	{
		case 0:
			renderFullbrightShiny();
			break;

		case 1:
			renderBump(LLRenderPass::PASS_POST_BUMP);
	}
}

////////////////////////////////////////////////////////////////
// List of bump-maps created from other textures.

void LLBumpImageList::init()
{
	llassert(mBrightnessEntries.size() == 0);
	llassert(mDarknessEntries.size() == 0);

	LLStandardBumpmap::init();
	LLStandardBumpmap::restoreGL();
}

void LLBumpImageList::clear()
{
	llinfos << "Clearing dynamic bumpmaps." << llendl;
	// these will be re-populated on-demand
	mBrightnessEntries.clear();
	mDarknessEntries.clear();

	LLStandardBumpmap::clear();
}

void LLBumpImageList::shutdown()
{
	clear();
	LLStandardBumpmap::shutdown();
}

void LLBumpImageList::destroyGL()
{
	clear();
	LLStandardBumpmap::destroyGL();
}

void LLBumpImageList::restoreGL()
{
	if (!gTextureList.isInitialized())
	{
		// Safe to return here because bump images will be reloaded during
		// initialization later.
		return;
	}

	LLStandardBumpmap::restoreGL();
	// Images will be recreated as they are needed.
}

LLBumpImageList::~LLBumpImageList()
{
	// Shutdown should have already been called.
	llassert(mBrightnessEntries.size() == 0);
	llassert(mDarknessEntries.size() == 0);
}

// Note: Does nothing for entries in gStandardBumpmapList that are not actually
// standard bump images (e.g. none, brightness, and darkness)
void LLBumpImageList::addTextureStats(U8 bump, const LLUUID& base_image_id,
									  F32 virtual_size)
{
	bump &= TEM_BUMP_MASK;
	LLViewerFetchedTexture* bump_image = gStandardBumpmapList[bump].mImage;
	if (bump_image)
	{
		bump_image->addTextureStats(virtual_size);
	}
}

void LLBumpImageList::updateImages()
{
	for (bump_image_map_t::iterator iter = mBrightnessEntries.begin(),
									end = mBrightnessEntries.end();
		 iter != end; )
	{
		bump_image_map_t::iterator curiter = iter++;
		LLViewerTexture* image = curiter->second;
		if (image)
		{
			bool destroy = true;
			if (image->hasGLTexture())
			{
				if (image->getBoundRecently())
				{
					destroy = false;
				}
				else
				{
					image->destroyGLTexture();
				}
			}

			if (destroy)
			{
				// Deletes the image thanks to reference counting
				mBrightnessEntries.erase(curiter);
			}
		}
	}

	for (bump_image_map_t::iterator iter = mDarknessEntries.begin(),
									end = mDarknessEntries.end();
		 iter != end; )
	{
		bump_image_map_t::iterator curiter = iter++;
		LLViewerTexture* image = curiter->second;
		if (image)
		{
			bool destroy = true;
			if (image->hasGLTexture())
			{
				if (image->getBoundRecently())
				{
					destroy = false;
				}
				else
				{
					image->destroyGLTexture();
				}
			}

			if (destroy)
			{
				// Deletes the image thanks to reference counting
				mDarknessEntries.erase(curiter);
			}
		}
	}
}

// Note: the caller SHOULD NOT keep the pointer that this function returns.
// It may be updated as more data arrives.
LLViewerTexture* LLBumpImageList::getBrightnessDarknessImage(LLViewerFetchedTexture* src_image,
															 U8 bump_code)
{
	llassert(bump_code == BE_BRIGHTNESS || bump_code == BE_DARKNESS);

	LLViewerTexture* bump = NULL;

	bump_image_map_t* entries_list = NULL;
	void (*callback_func)(bool success, LLViewerFetchedTexture* src_vi,
						  LLImageRaw* src, LLImageRaw* aux_src,
						  S32 discard_level, bool is_final,
						  void* userdata) = NULL;

	switch (bump_code)
	{
		case BE_BRIGHTNESS:
			entries_list = &mBrightnessEntries;
			callback_func = LLBumpImageList::onSourceBrightnessLoaded;
			break;

		case BE_DARKNESS:
			entries_list = &mDarknessEntries;
			callback_func = LLBumpImageList::onSourceDarknessLoaded;
			break;

		default:
			llassert(false);
			return NULL;
	}

	bump_image_map_t::iterator iter = entries_list->find(src_image->getID());
	if (iter != entries_list->end() && iter->second.notNull())
	{
		bump = iter->second;
	}
	else
	{
		(*entries_list)[src_image->getID()] =
			LLViewerTextureManager::getLocalTexture(true);
		// In case callback was called immediately and replaced the image:
		bump = (*entries_list)[src_image->getID()];
	}

	if (!src_image->hasCallbacks())
	{
		// If image has no callbacks but resolutions do not match, trigger
		// raw image loaded callback again
		if (src_image->getWidth() != bump->getWidth() ||
#if 0
			(LLPipeline::sRenderDeferred && bump->getComponents() != 4) ||
#endif
			src_image->getHeight() != bump->getHeight())
		{
			src_image->setBoostLevel(LLGLTexture::BOOST_BUMP);
			src_image->setLoadedCallback(callback_func, 0, true, false,
										 new LLUUID(src_image->getID()), NULL);
			src_image->forceToSaveRawImage(0);
		}
	}

	return bump;
}

// static
void LLBumpImageList::onSourceBrightnessLoaded(bool success,
											   LLViewerFetchedTexture* src_vi,
											   LLImageRaw* src,
											   LLImageRaw* aux_src,
											   S32 discard_level,
											   bool is_final,
											   void* userdata)
{
	LLUUID* source_asset_id = (LLUUID*)userdata;
	LLBumpImageList::onSourceLoaded(success, src_vi, src, *source_asset_id,
									BE_BRIGHTNESS);
	if (is_final)
	{
		delete source_asset_id;
	}
}

// static
void LLBumpImageList::onSourceDarknessLoaded(bool success,
											 LLViewerFetchedTexture* src_vi,
											 LLImageRaw* src,
											 LLImageRaw* aux_src,
											 S32 discard_level,
											 bool is_final,
											 void* userdata)
{
	LLUUID* source_asset_id = (LLUUID*)userdata;
	LLBumpImageList::onSourceLoaded(success, src_vi, src, *source_asset_id,
									BE_DARKNESS);
	if (is_final)
	{
		delete source_asset_id;
	}
}

void LLBumpImageList::onSourceStandardLoaded(bool success,
											 LLViewerFetchedTexture* src_vi,
											 LLImageRaw* src,
											 LLImageRaw* aux_src,
											 S32 discard_level,
											 bool, void*)
{
	if (success && LLPipeline::sRenderDeferred)
	{
		LL_FAST_TIMER(FTM_BUMP_SOURCE_STANDARD_LOADED);
		LLPointer<LLImageRaw> nrm_image = new LLImageRaw(src->getWidth(),
														 src->getHeight(), 4);
		{
			LL_FAST_TIMER(FTM_BUMP_GEN_NORMAL);
			generateNormalMapFromAlpha(src, nrm_image);
		}
		src_vi->setExplicitFormat(GL_RGBA, GL_RGBA);
		{
			LL_FAST_TIMER(FTM_BUMP_CREATE_TEXTURE);
			src_vi->createGLTexture(src_vi->getDiscardLevel(), nrm_image);
		}
	}
}

void LLBumpImageList::generateNormalMapFromAlpha(LLImageRaw* src,
												 LLImageRaw* nrm_image)
{
	U8* nrm_data = nrm_image->getData();
	S32 resx = src->getWidth();
	S32 resy = src->getHeight();

	U8* src_data = src->getData();
	S32 src_cmp = src->getComponents();

	static LLCachedControl<F32> norm_scale(gSavedSettings,
										   "RenderNormalMapScale");
	// Generate normal map from pseudo-heightfield
	LLVector3 up, down, left, right, norm;
	up.mV[VY] = -norm_scale;
	down.mV[VY] = norm_scale;
	left.mV[VX] = -norm_scale;
	right.mV[VX] = norm_scale;
	static const LLVector3 offset(0.5f, 0.5f ,0.5f);
	U32 idx = 0;
	for (S32 j = 0; j < resy; ++j)
	{
		for (S32 i = 0; i < resx; ++i)
		{
			S32 rx = (i + 1) % resx;
			S32 ry = (j + 1) % resy;

			S32 lx = (i - 1) % resx;
			if (lx < 0)
			{
				lx += resx;
			}

			S32 ly = (j - 1) % resy;
			if (ly < 0)
			{
				ly += resy;
			}

			F32 ch = (F32)src_data[(j * resx + i) * src_cmp + src_cmp - 1];

			right.mV[VZ] = (F32)src_data[(j * resx + rx + 1) * src_cmp - 1] - ch;
			left.mV[VZ] = (F32)src_data[(j * resx + lx + 1) * src_cmp - 1] - ch;
			up.mV[VZ] = (F32)src_data[(ly * resx + i + 1) * src_cmp - 1] - ch;
			down.mV[VZ] = (F32)src_data[(ry * resx + i + 1) * src_cmp - 1] - ch;

			norm = right % down + down % left + left % up + up % right;
			norm.normalize();
			norm *= 0.5f;
			norm += offset;

			idx = (j * resx + i) * 4;
			nrm_data[idx] = (U8)(norm.mV[0] * 255);
			nrm_data[idx + 1] = (U8)(norm.mV[1] * 255);
			nrm_data[idx + 2] = (U8)(norm.mV[2] * 255);
			nrm_data[idx + 3] = src_data[(j * resx + i) * src_cmp + src_cmp - 1];
		}
	}
}

// static
void LLBumpImageList::onSourceLoaded(bool success, LLViewerTexture* src_vi,
									 LLImageRaw* src, LLUUID& source_asset_id,
									 EBumpEffect bump_code)
{
	LL_FAST_TIMER(FTM_BUMP_SOURCE_LOADED);

	if (!success)
	{
		return;
	}

	if (!src || !src->getData())	// Paranoia
	{
		llwarns << "No image data for bump texture: " << source_asset_id
				<< llendl;
		return;
	}

	bump_image_map_t& entries_list(bump_code == BE_BRIGHTNESS ?
										gBumpImageList.mBrightnessEntries :
										gBumpImageList.mDarknessEntries);
	bump_image_map_t::iterator iter = entries_list.find(source_asset_id);
	bool needs_update = iter == entries_list.end() || iter->second.isNull() ||
						iter->second->getWidth() != src->getWidth() ||
						iter->second->getHeight() != src->getHeight();
	if (needs_update)
	{
		// If bump not cached yet or has changed resolution...
		LL_FAST_TIMER(FTM_BUMP_SOURCE_ENTRIES_UPDATE);
		// Make sure an entry exists for this image
		iter = entries_list.emplace(src_vi->getID(),
									LLViewerTextureManager::getLocalTexture(true)).first;
	}
	else
	{
		// Nothing to do
		return;
	}

	LLPointer<LLImageRaw> dst_image = new LLImageRaw(src->getWidth(),
													 src->getHeight(), 1);
	if (dst_image.isNull())
	{
		llwarns << "Could not create a new raw image for bump: "
				<< src_vi->getID() << ". Out of memory !" << llendl;
		return;
	}

	U8* dst_data = dst_image->getData();
	S32 dst_data_size = dst_image->getDataSize();

	U8* src_data = src->getData();
	S32 src_data_size = src->getDataSize();

	S32 src_components = src->getComponents();

	// Convert to luminance and then scale and bias that to get ready for
	// embossed bump mapping (0-255 maps to 127-255).

	// Convert to fixed point so we don't have to worry about precision or
	// clamping.
	constexpr S32 FIXED_PT = 8;
	constexpr S32 R_WEIGHT = S32(0.2995f * F32(1 << FIXED_PT));
	constexpr S32 G_WEIGHT = S32(0.5875f * F32(1 << FIXED_PT));
	constexpr S32 B_WEIGHT = S32(0.1145f * F32(1 << FIXED_PT));

	S32 minimum = 255;
	S32 maximum = 0;

	switch (src_components)
	{
		case 1:
		case 2:
		{
			LL_FAST_TIMER(FTM_BUMP_SOURCE_MIN_MAX);
			if (src_data_size == dst_data_size * src_components)
			{
				for (S32 i = 0, j = 0; i < dst_data_size;
					 i++, j += src_components)
				{
					dst_data[i] = src_data[j];
					if (dst_data[i] < minimum)
					{
						minimum = dst_data[i];
					}
					if (dst_data[i] > maximum)
					{
						maximum = dst_data[i];
					}
				}
			}
			else
			{
				llassert(false);
				dst_image->clear();
			}
			break;
		}

		case 3:
		case 4:
		{
			LL_FAST_TIMER(FTM_BUMP_SOURCE_RGB2LUM);
			if (src_data_size == dst_data_size * src_components)
			{
				for (S32 i = 0, j = 0; i < dst_data_size;
					 i++, j+= src_components)
				{
					// RGB to luminance
					dst_data[i] = (R_WEIGHT * src_data[j] +
								   G_WEIGHT * src_data[j + 1] +
								   B_WEIGHT * src_data[j + 2]) >> FIXED_PT;
					if (dst_data[i] < minimum)
					{
						minimum = dst_data[i];
					}
					if (dst_data[i] > maximum)
					{
						maximum = dst_data[i];
					}
				}
			}
			else
			{
				llassert(false);
				dst_image->clear();
			}
			break;
		}

		default:
			llassert(false);
			dst_image->clear();
	}

	if (maximum > minimum)
	{
		LL_FAST_TIMER(FTM_BUMP_SOURCE_RESCALE);
		U8 bias_and_scale_lut[256];
		F32 twice_one_over_range = 2.f / (maximum - minimum);
		S32 i;
		// Advantage: exaggerates the effect in midrange. Disadvantage: clamps
		// at the extremes.
		constexpr F32 ARTIFICIAL_SCALE = 2.f;
		if (bump_code == BE_DARKNESS)
		{
			for (i = minimum; i <= maximum; ++i)
			{
				F32 minus_one_to_one = F32(maximum - i) *
									   twice_one_over_range - 1.f;
				bias_and_scale_lut[i] = llclampb(ll_round(127 *
														  minus_one_to_one *
														  ARTIFICIAL_SCALE +
														  128));
			}
		}
		else
		{
			for (i = minimum; i <= maximum; ++i)
			{
				F32 minus_one_to_one = F32(i - minimum) *
									   twice_one_over_range - 1.f;
				bias_and_scale_lut[i] = llclampb(ll_round(127 *
														  minus_one_to_one *
														  ARTIFICIAL_SCALE +
														  128));
			}
		}

		for (i = 0; i < dst_data_size; ++i)
		{
			dst_data[i] = bias_and_scale_lut[dst_data[i]];
		}
	}

	//---------------------------------------------------
	// Immediately assign bump to a smart pointer in case some local smart
	// pointer accidentally releases it.
	LLPointer<LLViewerTexture> bump = iter->second;

	if (!LLPipeline::sRenderDeferred)
	{
		LL_FAST_TIMER(FTM_BUMP_SOURCE_CREATE);
		bump->setExplicitFormat(GL_ALPHA8, GL_ALPHA);
		bump->createGLTexture(0, dst_image);
	}
	else	// Convert to normal map
	{
		// Disable compression on normal maps to prevent errors below
		bump->getGLImage()->setAllowCompression(false);

		{
			LL_FAST_TIMER(FTM_BUMP_SOURCE_CREATE);
			bump->setExplicitFormat(GL_RGBA8, GL_ALPHA);
			bump->createGLTexture(0, dst_image);
		}

		{
			static LLCachedControl<F32> norm_scale(gSavedSettings,
												   "RenderNormalMapScale");
			LL_FAST_TIMER(FTM_BUMP_SOURCE_GEN_NORMAL);
			gPipeline.mScreen.bindTarget();

			LLGLDepthTest depth(GL_FALSE);
			LLGLDisable cull(GL_CULL_FACE);
			LLGLDisable blend(GL_BLEND);
			gGL.setColorMask(true, true);
			gNormalMapGenProgram.bind();

			static LLStaticHashedString sNormScale("norm_scale");
			static LLStaticHashedString sStepX("stepX");
			static LLStaticHashedString sStepY("stepY");
			gNormalMapGenProgram.uniform1f(sNormScale, norm_scale);
			gNormalMapGenProgram.uniform1f(sStepX, 1.f / bump->getWidth());
			gNormalMapGenProgram.uniform1f(sStepY, 1.f / bump->getHeight());

			LLVector2 v((F32)bump->getWidth() / gPipeline.mScreen.getWidth(),
						(F32)bump->getHeight() / gPipeline.mScreen.getHeight());

			gGL.getTexUnit(0)->bind(bump);

			S32 width = bump->getWidth();
			S32 height = bump->getHeight();

			S32 screen_width = gPipeline.mScreen.getWidth();
			S32 screen_height = gPipeline.mScreen.getHeight();

			glViewport(0, 0, screen_width, screen_height);

			for (S32 left = 0; left < width; left += screen_width)
			{
				S32 right = left + screen_width;
				right = llmin(right, width);

				F32 left_tc = (F32)left / width;
				F32 right_tc = (F32)right / width;

				for (S32 bottom = 0; bottom < height; bottom += screen_height)
				{
					S32 top = bottom+screen_height;
					top = llmin(top, height);

					F32 bottom_tc = (F32)bottom/height;
					F32 top_tc = (F32)(bottom + screen_height) / height;
					top_tc = llmin(top_tc, 1.f);

					F32 screen_right = (F32)(right - left) / screen_width;
					F32 screen_top = (F32)(top - bottom) / screen_height;

					gGL.begin(LLRender::TRIANGLE_STRIP);
					gGL.texCoord2f(left_tc, bottom_tc);
					gGL.vertex2f(0, 0);

					gGL.texCoord2f(left_tc, top_tc);
					gGL.vertex2f(0, screen_top);

					gGL.texCoord2f(right_tc, bottom_tc);
					gGL.vertex2f(screen_right, 0);

					gGL.texCoord2f(right_tc, top_tc);
					gGL.vertex2f(screen_right, screen_top);

					gGL.end();

					S32 w = right-left;
					S32 h = top-bottom;
					gGL.flush();
					glCopyTexSubImage2D(GL_TEXTURE_2D, 0, left, bottom,
										0, 0, w, h);
				}
			}

			glGenerateMipmap(GL_TEXTURE_2D);

			gPipeline.mScreen.flush();

			gNormalMapGenProgram.unbind();
#if 0
			generateNormalMapFromAlpha(dst_image, nrm_image);
#endif
			stop_glerror();
		}
	}

	iter->second = std::move(bump); // Derefs (and deletes) old image
}

void LLDrawPoolBump::renderBump(U32 type, U32 mask)
{
	LLCullResult::drawinfo_iterator begin = gPipeline.beginRenderMap(type);
	LLCullResult::drawinfo_iterator end = gPipeline.endRenderMap(type);

	for (LLCullResult::drawinfo_iterator i = begin; i != end; ++i)
	{
		LLDrawInfo& params = **i;

		if (LLDrawPoolBump::bindBumpMap(params))
		{
			pushBatch(params, mask, false);
		}
	}
}

void LLDrawPoolBump::pushBatch(LLDrawInfo& params, U32 mask, bool texture,
							   bool batch_textures)
{
	applyModelMatrix(params);

	bool tex_setup = false;

	U32 count = 0;
	if (batch_textures && (count = params.mTextureList.size()) > 1)
	{
		for (U32 i = 0; i < count; ++i)
		{
			const LLPointer<LLViewerTexture>& tex = params.mTextureList[i];
			if (tex.notNull())
			{
				gGL.getTexUnit(i)->bind(tex);
			}
		}
	}
	else
	{
		// Not batching textures or batch has only 1 texture: might need a
		// texture matrix
		if (params.mTextureMatrix)
		{
			if (mShiny)
			{
				gGL.getTexUnit(0)->activate();
				gGL.matrixMode(LLRender::MM_TEXTURE);
			}
			else
			{
				gGL.getTexUnit(0)->activate();
				gGL.matrixMode(LLRender::MM_TEXTURE);
				gGL.loadMatrix(params.mTextureMatrix->getF32ptr());
				++gPipeline.mTextureMatrixOps;
			}

			gGL.loadMatrix(params.mTextureMatrix->getF32ptr());
			++gPipeline.mTextureMatrixOps;

			tex_setup = true;
		}

		if (mShiny && mShaderLevel > 1 && texture)
		{
			if (params.mTexture.notNull())
			{
				gGL.getTexUnit(sDiffuseChannel)->bind(params.mTexture);
				params.mTexture->addTextureStats(params.mVSize);
			}
			else
			{
				gGL.getTexUnit(sDiffuseChannel)->unbind(LLTexUnit::TT_TEXTURE);
			}
		}
	}

	if (params.mGroup)
	{
		params.mGroup->rebuildMesh();
	}
	params.mVertexBuffer->setBuffer(mask);
	params.mVertexBuffer->drawRange(params.mDrawMode, params.mStart,
									params.mEnd, params.mCount,
									params.mOffset);
	gPipeline.addTrianglesDrawn(params.mCount, params.mDrawMode);
	if (tex_setup)
	{
		if (mShiny)
		{
			gGL.getTexUnit(0)->activate();
		}
		else
		{
			gGL.getTexUnit(0)->activate();
			gGL.matrixMode(LLRender::MM_TEXTURE);
		}
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
	stop_glerror();
}

// Renders invisiprims
void LLDrawPoolInvisible::render(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_INVISIBLE);

	if (gPipeline.canUseVertexShaders())
	{
		gOcclusionProgram.bind();
	}

	U32 invisi_mask = LLVertexBuffer::MAP_VERTEX;
	glStencilMask(0);
	gGL.setColorMask(false, false);
	pushBatches(LLRenderPass::PASS_INVISIBLE, invisi_mask, false);
	gGL.setColorMask(true, false);
	glStencilMask(0xFFFFFFFF);

	if (gPipeline.canUseVertexShaders())
	{
		gOcclusionProgram.unbind();
	}

	if (gPipeline.hasRenderBatches(LLRenderPass::PASS_INVISI_SHINY))
	{
		beginShiny(true);
		renderShiny(true);
		endShiny(true);
	}
}

void LLDrawPoolInvisible::beginDeferredPass(S32 pass)
{
	beginRenderPass(pass);
}

void LLDrawPoolInvisible::endDeferredPass(S32 pass)
{
	endRenderPass(pass);
}

void LLDrawPoolInvisible::renderDeferred(S32 pass)
{
	static LLCachedControl<bool> deferred_invisible(gSavedSettings,
													"RenderDeferredInvisible");
	if (deferred_invisible || gUseNewShaders)
	{
		LL_FAST_TIMER(FTM_RENDER_INVISIBLE);

		if (gPipeline.canUseVertexShaders())
		{
			gOcclusionProgram.bind();
		}

		U32 invisi_mask = LLVertexBuffer::MAP_VERTEX;
		glStencilMask(0);
		//glStencilOp(GL_ZERO, GL_KEEP, GL_REPLACE);
		gGL.setColorMask(false, false);
		pushBatches(LLRenderPass::PASS_INVISIBLE, invisi_mask, false);
		gGL.setColorMask(true, true);
		//glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0xFFFFFFFF);

		if (gPipeline.canUseVertexShaders())
		{
			gOcclusionProgram.unbind();
		}

		if (gPipeline.hasRenderBatches(LLRenderPass::PASS_INVISI_SHINY))
		{
			beginShiny(true);
			renderShiny(true);
			endShiny(true);
		}
	}
}

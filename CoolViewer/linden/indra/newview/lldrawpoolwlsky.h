/** 
 * @file lldrawpoolwlsky.h
 * @brief LLDrawPoolWLSky class definition
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_DRAWPOOLWLSKY_H
#define LL_DRAWPOOLWLSKY_H

#include "llimagegl.h"
#include "llsettingssky.h"

#include "lldrawpool.h"

class LLGLSLShader;

class LLDrawPoolWLSky final : public LLDrawPool
{
protected:
	LOG_CLASS(LLDrawPoolWLSky);

public:
	static constexpr U32 SKY_VERTEX_DATA_MASK =
		LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0;
	static constexpr U32 STAR_VERTEX_DATA_MASK =
		LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_COLOR |
		LLVertexBuffer::MAP_TEXCOORD0;
	static constexpr U32 ADV_ATMO_SKY_VERTEX_DATA_MASK =
		LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0;

	LLDrawPoolWLSky();

	LL_INLINE U32 getVertexDataMask() override			{ return SKY_VERTEX_DATA_MASK; }

	LL_INLINE bool isDead() override					{ return false; }

	LL_INLINE S32 getNumDeferredPasses() override		{ return 1; }
	void beginDeferredPass(S32) override;
	void endDeferredPass(S32) override;
	void renderDeferred(S32 pass) override;

	void beginRenderPass(S32) override;
	void endRenderPass(S32) override;
	LL_INLINE S32 getNumPasses() override				{ return 1; }
	void render(S32 pass = 0) override;
	LL_INLINE void prerender() override					{}

	// Verify that all data in the draw pool is correct
	LL_INLINE bool verify() const override				{ return true; }

	LL_INLINE S32 getShaderLevel() const override		{ return mShaderLevel; }

	LL_INLINE LLViewerTexture* getTexture() override	{ return NULL; }
	LL_INLINE bool isFacePool() override				{ return false; }
	LL_INLINE void resetDrawOrders() override			{}

	static void cleanupGL();
	static void restoreGL();

private:
	void renderDome(LLGLSLShader* shader) const;

	// Windlight version
	void renderSkyHazeWL() const;

	// Extended environment version.
	void renderSkyHazeEE() const;

	// Windlight version
	void renderSkyCloudsWL() const;
	// Extended environment version. NOTE: LL's EEP viewer also got a
	// renderSkyCloudsDeferred() method , but it is exactly identical to their
	// renderSkyClouds() method, which is our renderSkyCloudsEE() method here.
	void renderSkyCloudsEE() const;

	void renderStarsWL() const;	// Windlight version
	// Extended environment version
	void renderStarsEE() const;

	void renderHeavenlyBodiesWL();	// Windlight version
	// Extended environment version
	void renderHeavenlyBodiesEE();

	// Extended environment specific methods
	void renderSkyHazeDeferred() const;
	void renderStarsDeferred() const;

private:
	LLSettingsSky::ptr_t				mCurrentSky;
	LLVector3							mCameraOrigin;
	F32									mCamHeightLocal;

	static LLGLSLShader*				sCloudShader;
	static LLGLSLShader*				sSkyShader;
	static LLGLSLShader*				sSunShader;
	static LLGLSLShader*				sMoonShader;
	static LLPointer<LLViewerTexture> 	sCloudNoiseTexture;
	static LLPointer<LLImageRaw>		sCloudNoiseRawImage;
};

#endif // LL_DRAWPOOLWLSKY_H

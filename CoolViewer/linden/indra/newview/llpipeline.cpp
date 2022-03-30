/**
 * @file llpipeline.cpp
 * @brief Rendering pipeline.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llpipeline.h"

#include "imageids.h"
#include "llaudioengine.h"			// For sound beacons
#include "llcubemap.h"
#include "llfasttimer.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolavatar.h"
#include "lldrawpoolbump.h"
#include "lldrawpoolground.h"
#include "lldrawpoolterrain.h"
#include "lldrawpooltree.h"
#include "lldrawpoolwater.h"
#include "lldrawpoolwlsky.h"
#include "llenvironment.h"
#include "llenvsettings.h"
#include "llface.h"
#include "llfeaturemanager.h"
#include "hbfloatersoundslist.h"
#include "llfloaterstats.h"
#include "llfloatertelehub.h"
#include "llhudmanager.h"
#include "llhudtext.h"
#include "llmeshrepository.h"
#include "llpanelface.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llstartup.h"
#include "lltexturecache.h"
#include "lltexturefetch.h"
#include "lltracker.h"
#include "lltool.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewerjoystick.h"
#include "llviewermediafocus.h"
#include "llviewerobjectlist.h"
#include "llvieweroctree.h"			// For ll_create_cube_vb()
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvocache.h"
#include "llvopartgroup.h"
#include "llvosurfacepatch.h"
#include "llvotree.h"
#include "llvovolume.h"
#include "llvowater.h"
#include "llvowlsky.h"
#include "llworld.h"

// Set to 1 to debug deferred shaders uniforms setting
#define DEBUG_OPTIMIZED_UNIFORMS 0

// Set to 0 to re-enable occlusions when rendering water reflections (causes
// reflections ghosting).
#define REFLECTION_RENDER_NOT_OCCLUDED 1

LLPipeline gPipeline;
const LLMatrix4* gGLLastMatrix = NULL;
bool gShiftFrame = false;

// Cached settings
bool LLPipeline::RenderAvatarVP;
bool LLPipeline::WindLightUseAtmosShaders;
bool LLPipeline::RenderDeferred;
F32 LLPipeline::RenderDeferredSunWash;
F32 LLPipeline::RenderDeferredDisplayGamma;
U32 LLPipeline::RenderFSAASamples;
U32 LLPipeline::RenderResolutionDivisor;
S32 LLPipeline::RenderShadowDetail;
bool LLPipeline::RenderDeferredSSAO;
F32 LLPipeline::RenderShadowResolutionScale;
S32 LLPipeline::RenderLightingDetail;
bool LLPipeline::RenderDelayCreation;
bool LLPipeline::RenderAnimateRes;
bool LLPipeline::RenderSpotLightsInNondeferred;
LLColor4 LLPipeline::PreviewAmbientColor;
LLColor4 LLPipeline::PreviewDiffuse0;
LLColor4 LLPipeline::PreviewSpecular0;
LLColor4 LLPipeline::PreviewDiffuse1;
LLColor4 LLPipeline::PreviewSpecular1;
LLColor4 LLPipeline::PreviewDiffuse2;
LLColor4 LLPipeline::PreviewSpecular2;
LLVector3 LLPipeline::PreviewDirection0;
LLVector3 LLPipeline::PreviewDirection1;
LLVector3 LLPipeline::PreviewDirection2;
bool LLPipeline::RenderGlow;
F32 LLPipeline::RenderGlowMinLuminance;
F32 LLPipeline::RenderGlowMaxExtractAlpha;
F32 LLPipeline::RenderGlowWarmthAmount;
LLVector3 LLPipeline::RenderGlowLumWeights;
LLVector3 LLPipeline::RenderGlowWarmthWeights;
U32 LLPipeline::RenderGlowResolutionPow;
U32 LLPipeline::RenderGlowIterations;
F32 LLPipeline::RenderGlowWidth;
F32 LLPipeline::RenderGlowStrength;
bool LLPipeline::RenderDepthOfField;
bool LLPipeline::RenderDepthOfFieldInEditMode;
F32 LLPipeline::CameraFocusTransitionTime;
F32 LLPipeline::CameraFNumber;
F32 LLPipeline::CameraFocalLength;
F32 LLPipeline::CameraFieldOfView;
F32 LLPipeline::RenderShadowNoise;
F32 LLPipeline::RenderShadowBlurSize;
F32 LLPipeline::RenderSSAOScale;
U32 LLPipeline::RenderSSAOMaxScale;
F32 LLPipeline::RenderSSAOFactor;
LLVector3 LLPipeline::RenderSSAOEffect;
F32 LLPipeline::RenderShadowBiasError;
F32 LLPipeline::RenderShadowOffset;
F32 LLPipeline::RenderShadowOffsetNoSSAO;
F32 LLPipeline::RenderShadowBias;
F32 LLPipeline::RenderSpotShadowOffset;
F32 LLPipeline::RenderSpotShadowBias;
LLVector3 LLPipeline::RenderShadowGaussian;
F32 LLPipeline::RenderShadowBlurDistFactor;
bool LLPipeline::RenderDeferredAtmospheric;
bool LLPipeline::RenderWaterReflections;
S32 LLPipeline::RenderReflectionDetail;
LLVector3 LLPipeline::RenderShadowClipPlanes;
LLVector3 LLPipeline::RenderShadowOrthoClipPlanes;
F32 LLPipeline::RenderFarClip;
LLVector3 LLPipeline::RenderShadowSplitExponent;
F32 LLPipeline::RenderShadowErrorCutoff;
F32 LLPipeline::RenderShadowFOVCutoff;
bool LLPipeline::CameraOffset;
F32 LLPipeline::CameraMaxCoF;
F32 LLPipeline::CameraDoFResScale;
U32 LLPipeline::RenderAutoHideGeometryMemoryLimit;
F32 LLPipeline::RenderAutoHideSurfaceAreaLimit;
bool LLPipeline::sRenderScriptedBeacons = false;
bool LLPipeline::sRenderScriptedTouchBeacons = false;
bool LLPipeline::sRenderPhysicalBeacons = false;
bool LLPipeline::sRenderPermanentBeacons = false;
bool LLPipeline::sRenderCharacterBeacons = false;
bool LLPipeline::sRenderSoundBeacons = false;
bool LLPipeline::sRenderInvisibleSoundBeacons = false;
bool LLPipeline::sRenderParticleBeacons = false;
bool LLPipeline::sRenderMOAPBeacons = false;
bool LLPipeline::sRenderHighlight = true;
bool LLPipeline::sRenderBeacons = false;
bool LLPipeline::sRenderAttachments = false;
bool LLPipeline::sRenderingHUDs = false;
U32	LLPipeline::sRenderByOwner = 0;
U32 LLPipeline::DebugBeaconLineWidth;
LLRender::eTexIndex LLPipeline::sRenderHighlightTextureChannel = LLRender::DIFFUSE_MAP;

constexpr F32 LIGHT_FADE_TIME = 0.2f;
constexpr U32 AUX_VB_MASK = LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_TEXCOORD1;

// *HACK: counter for rendering a fixed number of frames after toggling to full
// screen at login, to work around DEV-5361
static S32 sDelayedVBOEnable = 0;

static LLDrawable* sRenderSpotLight = NULL;

static LLStaticHashedString sDelta("delta");
static LLStaticHashedString sDistFactor("dist_factor");
static LLStaticHashedString sKern("kern");
static LLStaticHashedString sKernScale("kern_scale");

std::string gPoolNames[] =
{
	// Correspond to LLDrawpool enum render type
	"NONE",
	"POOL_GROUND",
	"POOL_TERRAIN,"
	"POOL_SIMPLE",
	"POOL_FULLBRIGHT",
	"POOL_BUMP",
	"POOL_MATERIALS",
	"POOL_TREE",
	"POOL_ALPHA_MASK",
	"POOL_FULLBRIGHT_ALPHA_MASK",
	"POOL_SKY",
	"POOL_WL_SKY",
	"POOL_GRASS",
	"POOL_INVISIBLE",
	"POOL_AVATAR",
	"POOL_PUPPET",
	"POOL_VOIDWATER",
	"POOL_WATER",
	"POOL_GLOW",
	"POOL_ALPHA"
};

bool LLPipeline::sFreezeTime = false;
bool LLPipeline::sPickAvatar = true;
bool LLPipeline::sDynamicLOD = true;
bool LLPipeline::sShowHUDAttachments = true;
bool LLPipeline::sRenderBeaconsFloaterOpen = false;
bool LLPipeline::sDelayVBUpdate = true;
bool LLPipeline::sAutoMaskAlphaDeferred = true;
bool LLPipeline::sAutoMaskAlphaNonDeferred = false;
bool LLPipeline::sRenderBump = true;
bool LLPipeline::sBakeSunlight = false;
bool LLPipeline::sNoAlpha = false;
bool LLPipeline::sUseFarClip = true;
bool LLPipeline::sShadowRender = false;
bool LLPipeline::sWaterReflections = false;
bool LLPipeline::sCanRenderGlow = false;
bool LLPipeline::sReflectionRender = false;
bool LLPipeline::sImpostorRender = false;
bool LLPipeline::sImpostorRenderAlphaDepthPass = false;
bool LLPipeline::sUnderWaterRender = false;
bool LLPipeline::sTextureBindTest = false;
bool LLPipeline::sRenderFrameTest = false;
bool LLPipeline::sRenderAttachedLights = true;
bool LLPipeline::sRenderAttachedParticles = true;
bool LLPipeline::sRenderDeferred = false;
bool LLPipeline::sRenderTransparentWater = true;
bool LLPipeline::sRenderWaterCull = false;
bool LLPipeline::sMemAllocationThrottled = false;
bool LLPipeline::sRenderWater = true;
S32 LLPipeline::sUseOcclusion = 0;
S32 LLPipeline::sVisibleLightCount = 0;

LLCullResult* LLPipeline::sCull = NULL;

static const LLMatrix4a TRANS_MAT(LLVector4a(0.5f, 0.f, 0.f, 0.f),
								  LLVector4a(0.f, 0.5f, 0.f, 0.f),
								  LLVector4a(0.f, 0.f, 0.5f, 0.f),
								  LLVector4a(0.5f, 0.5f, 0.5f, 1.f));

// Utility functions only used here

static LLMatrix4a look_proj(const LLVector3& pos_in, const LLVector3& dir_in,
							const LLVector3& up_in)
{
	const LLVector4a pos(pos_in.mV[VX], pos_in.mV[VY], pos_in.mV[VZ], 1.f);
	LLVector4a dir(dir_in.mV[VX], dir_in.mV[VY], dir_in.mV[VZ]);
	const LLVector4a up(up_in.mV[VX], up_in.mV[VY], up_in.mV[VZ]);

	LLVector4a left_norm;
	left_norm.setCross3(dir, up);
	left_norm.normalize3fast();
	LLVector4a up_norm;
	up_norm.setCross3(left_norm, dir);
	up_norm.normalize3fast();
	LLVector4a& dir_norm = dir;
	dir.normalize3fast();

	LLVector4a left_dot;
	left_dot.setAllDot3(left_norm, pos);
	left_dot.negate();
	LLVector4a up_dot;
	up_dot.setAllDot3(up_norm, pos);
	up_dot.negate();
	LLVector4a dir_dot;
	dir_dot.setAllDot3(dir_norm, pos);

	dir_norm.negate();

	LLMatrix4a ret;
	ret.setRow<0>(left_norm);
	ret.setRow<1>(up_norm);
	ret.setRow<2>(dir_norm);
	ret.setRow<3>(LLVector4a(0.f, 0.f, 0.f, 1.f));

	ret.getRow<0>().copyComponent<3>(left_dot);
	ret.getRow<1>().copyComponent<3>(up_dot);
	ret.getRow<2>().copyComponent<3>(dir_dot);

	ret.transpose();

	return ret;
}

static bool addDeferredAttachments(LLRenderTarget& target)
{
	return target.addColorAttachment(GL_SRGB8_ALPHA8) &&	// specular
		   target.addColorAttachment(GL_RGB10_A2);			// normal + z
}

LLPipeline::LLPipeline()
:	mBackfaceCull(false),
	mNeedsDrawStats(false),
	mBatchCount(0),
	mMatrixOpCount(0),
	mTextureMatrixOps(0),
	mMaxBatchSize(0),
	mMinBatchSize(0),
	mTrianglesDrawn(0),
	mNumVisibleNodes(0),
	mInitialized(false),
	mVertexShadersLoaded(-1),
	mRenderDebugFeatureMask(0),
	mRenderDebugMask(0),
	mOldRenderDebugMask(0),
	mMeshDirtyQueryObject(0),
	mGroupQ1Locked(false),
	mGroupQ2Locked(false),
	mResetVertexBuffers(false),
	mLastRebuildPool(NULL),
	mAlphaPool(NULL),
	mSkyPool(NULL),
	mTerrainPool(NULL),
	mWaterPool(NULL),
	mGroundPool(NULL),
	mSimplePool(NULL),
	mGrassPool(NULL),
	mAlphaMaskPool(NULL),
	mFullbrightAlphaMaskPool(NULL),
	mFullbrightPool(NULL),
	mInvisiblePool(NULL),
	mGlowPool(NULL),
	mBumpPool(NULL),
	mMaterialsPool(NULL),
	mWLSkyPool(NULL),
	mLightMask(0),
	mLightMovingMask(0),
	mLightingDetail(0),
	mScreenWidth(0),
	mScreenHeight(0),
	mNoiseMap(0),
	mTrueNoiseMap(0),
	mLightFunc(0),
	mIsSunUp(true)
{
}

void LLPipeline::connectRefreshCachedSettingsSafe(const char* name)
{
	LLPointer<LLControlVariable> cvp = gSavedSettings.getControl(name);
	if (cvp.isNull())
	{
		llwarns << "Global setting name not found: " << name << llendl;
		return;
	}
	cvp->getSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));
}

void LLPipeline::createAuxVBs()
{
	mCubeVB = ll_create_cube_vb(LLVertexBuffer::MAP_VERTEX,
								GL_STATIC_DRAW_ARB);

	mDeferredVB = new LLVertexBuffer(AUX_VB_MASK, 0);
	mDeferredVB->allocateBuffer(8, 0, true);

	mGlowCombineVB = new LLVertexBuffer(AUX_VB_MASK, GL_STREAM_DRAW_ARB);
	mGlowCombineVB->allocateBuffer(3, 0, true);

	LLStrider<LLVector3> v;
	LLStrider<LLVector2> uv1;
	if (!mGlowCombineVB->getVertexStrider(v) ||
		!mGlowCombineVB->getTexCoord0Strider(uv1))
	{
		llwarns << "Could not initialize mGlowCombineVB striders !" << llendl;
		return;
	}

	uv1[0].clear();
	uv1[1].set(0.f, 2.f);
	uv1[2].set(2.f, 0.f);

	v[0].set(-1.f, -1.f, 0.f);
	v[1].set(-1.f, 3.f, 0.f);
	v[2].set(3.f, -1.f, 0.f);

	mGlowCombineVB->flush();
}

void LLPipeline::init()
{
	// The following three lines used to be in llappviewer.cpp, in
	// settings_to_globals(). HB
	sRenderBump = gSavedSettings.getBool("RenderObjectBump");
	sRenderDeferred = sRenderBump && gSavedSettings.getBool("RenderDeferred");
	LLRenderTarget::sUseFBO = sRenderDeferred;

	refreshCachedSettings();

	RenderFSAASamples = gSavedSettings.getU32("RenderFSAASamples");

	gOctreeMaxCapacity = gSavedSettings.getU32("OctreeMaxNodeCapacity");
	gOctreeMinSize = gSavedSettings.getF32("OctreeMinimumNodeSize");

	sDynamicLOD = gSavedSettings.getBool("RenderDynamicLOD");

	LLVertexBuffer::sUseVAO = gSavedSettings.getBool("RenderUseVAO");
	LLVertexBuffer::sUseStreamDraw =
		gSavedSettings.getBool("RenderUseStreamVBO");
	LLVertexBuffer::sPreferStreamDraw =
		gSavedSettings.getBool("RenderPreferStreamDraw");

	sRenderAttachedLights = gSavedSettings.getBool("RenderAttachedLights");
	sRenderAttachedParticles =
		gSavedSettings.getBool("RenderAttachedParticles");
	sAutoMaskAlphaDeferred =
		gSavedSettings.getBool("RenderAutoMaskAlphaDeferred");
	sAutoMaskAlphaNonDeferred =
		gSavedSettings.getBool("RenderAutoMaskAlphaNonDeferred");

	if (gFeatureManager.isFeatureAvailable("RenderCompressTextures") &&
		gGLManager.mHasVertexBufferObject)
	{
		LLImageGL::sCompressTextures =
			gSavedSettings.getBool("RenderCompressTextures");
		LLImageGL::sCompressThreshold =
			gSavedSettings.getU32("RenderCompressThreshold");
	}

	mInitialized = true;

	// Create render pass pools
	getPool(LLDrawPool::POOL_ALPHA);
	getPool(LLDrawPool::POOL_SIMPLE);
	getPool(LLDrawPool::POOL_ALPHA_MASK);
	getPool(LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK);
	getPool(LLDrawPool::POOL_GRASS);
	getPool(LLDrawPool::POOL_FULLBRIGHT);
	getPool(LLDrawPool::POOL_INVISIBLE);
	getPool(LLDrawPool::POOL_BUMP);
	getPool(LLDrawPool::POOL_MATERIALS);
	getPool(LLDrawPool::POOL_GLOW);

	mTrianglesDrawnStat.reset();
	resetFrameStats();

	setAllRenderTypes();	// All rendering types start enabled

	mRenderDebugFeatureMask = 0xffffffff; // All debugging features on
	mRenderDebugMask = 0;	// All debug starts off

	// Do not turn on ground when this is set (Mac Books with intel 950s need
	// this)
	if (!gSavedSettings.getBool("RenderGround"))
	{
		toggleRenderType(RENDER_TYPE_GROUND);
	}

	mOldRenderDebugMask = mRenderDebugMask;

	mBackfaceCull = true;

	for (U32 i = 0; i < 2; ++i)
	{
		mSpotLightFade[i] = 1.f;
	}

	createAuxVBs();

	setLightingDetail(-1);

	// Enable features

	// Note: this will set mVertexShadersLoaded to 1 if basic shaders get
	// successfully loaded, or to -1 on failure.
	gViewerShaderMgrp->setShaders();

	stop_glerror();

	if (!gSavedSettings.getBool("SkipStaticVectorSizing"))
	{
		// Reserve some space in permanent vectors to avoid fragmentation,
		// based on the statistics we got for real sessions.
		// By setting SkipStaticVectorSizing to true (and restarting the
		// viewer), you may skip this pre-sizing of vectors so to verify (via
		// "Advanced" -> "Consoles" -> "Info to Debug Console" ->
		// "Memory Stats") what capacity is naturally reached during a session
		// and check it against the capacity reserved below (this is how I
		// determined the suitable values). HB
		mMovedList.reserve(1024);
		mMovedBridge.reserve(1024);
		mGroupQ1.reserve(8192);
		mGroupQ2.reserve(512);
		mMeshDirtyGroup.reserve(2048);
		mShiftList.reserve(65536);
	}

	// Register settings callbacks

	connectRefreshCachedSettingsSafe("RenderAutoMaskAlphaDeferred");
	connectRefreshCachedSettingsSafe("RenderAutoMaskAlphaNonDeferred");
	connectRefreshCachedSettingsSafe("RenderUseFarClip");
	connectRefreshCachedSettingsSafe("RenderDelayVBUpdate");
	connectRefreshCachedSettingsSafe("UseOcclusion");
	connectRefreshCachedSettingsSafe("RenderAvatarVP");
	connectRefreshCachedSettingsSafe("WindLightUseAtmosShaders");
	connectRefreshCachedSettingsSafe("RenderDeferred");
	connectRefreshCachedSettingsSafe("RenderDeferredSunWash");
	connectRefreshCachedSettingsSafe("RenderDeferredDisplayGamma");
	connectRefreshCachedSettingsSafe("RenderFSAASamples");
	connectRefreshCachedSettingsSafe("RenderResolutionDivisor");
	connectRefreshCachedSettingsSafe("RenderShadowDetail");
	connectRefreshCachedSettingsSafe("RenderDeferredSSAO");
	connectRefreshCachedSettingsSafe("RenderShadowResolutionScale");
	connectRefreshCachedSettingsSafe("RenderLightingDetail");
	connectRefreshCachedSettingsSafe("RenderDelayCreation");
	connectRefreshCachedSettingsSafe("RenderAnimateRes");
	connectRefreshCachedSettingsSafe("RenderSpotLightsInNondeferred");
	connectRefreshCachedSettingsSafe("PreviewAmbientColor");
	connectRefreshCachedSettingsSafe("PreviewDiffuse0");
	connectRefreshCachedSettingsSafe("PreviewSpecular0");
	connectRefreshCachedSettingsSafe("PreviewDiffuse1");
	connectRefreshCachedSettingsSafe("PreviewSpecular1");
	connectRefreshCachedSettingsSafe("PreviewDiffuse2");
	connectRefreshCachedSettingsSafe("PreviewSpecular2");
	connectRefreshCachedSettingsSafe("PreviewDirection0");
	connectRefreshCachedSettingsSafe("PreviewDirection1");
	connectRefreshCachedSettingsSafe("PreviewDirection2");
	connectRefreshCachedSettingsSafe("RenderGlow");
	connectRefreshCachedSettingsSafe("RenderGlowMinLuminance");
	connectRefreshCachedSettingsSafe("RenderGlowMaxExtractAlpha");
	connectRefreshCachedSettingsSafe("RenderGlowWarmthAmount");
	connectRefreshCachedSettingsSafe("RenderGlowLumWeights");
	connectRefreshCachedSettingsSafe("RenderGlowWarmthWeights");
	connectRefreshCachedSettingsSafe("RenderGlowResolutionPow");
	connectRefreshCachedSettingsSafe("RenderGlowIterations");
	connectRefreshCachedSettingsSafe("RenderGlowWidth");
	connectRefreshCachedSettingsSafe("RenderGlowStrength");
	connectRefreshCachedSettingsSafe("RenderDepthOfField");
	connectRefreshCachedSettingsSafe("RenderDepthOfFieldInEditMode");
	connectRefreshCachedSettingsSafe("CameraFocusTransitionTime");
	connectRefreshCachedSettingsSafe("CameraFNumber");
	connectRefreshCachedSettingsSafe("CameraFocalLength");
	connectRefreshCachedSettingsSafe("CameraFieldOfView");
	connectRefreshCachedSettingsSafe("RenderShadowNoise");
	connectRefreshCachedSettingsSafe("RenderShadowBlurSize");
	connectRefreshCachedSettingsSafe("RenderSSAOScale");
	connectRefreshCachedSettingsSafe("RenderSSAOMaxScale");
	connectRefreshCachedSettingsSafe("RenderSSAOFactor");
	connectRefreshCachedSettingsSafe("RenderSSAOEffect");
	connectRefreshCachedSettingsSafe("RenderShadowBiasError");
	connectRefreshCachedSettingsSafe("RenderShadowOffset");
	connectRefreshCachedSettingsSafe("RenderShadowOffsetNoSSAO");
	connectRefreshCachedSettingsSafe("RenderShadowBias");
	connectRefreshCachedSettingsSafe("RenderSpotShadowOffset");
	connectRefreshCachedSettingsSafe("RenderSpotShadowBias");
	connectRefreshCachedSettingsSafe("RenderShadowGaussian");
	connectRefreshCachedSettingsSafe("RenderShadowBlurDistFactor");
	connectRefreshCachedSettingsSafe("RenderDeferredAtmospheric");
	connectRefreshCachedSettingsSafe("RenderWaterReflections");
	connectRefreshCachedSettingsSafe("RenderReflectionDetail");
	connectRefreshCachedSettingsSafe("RenderShadowClipPlanes");
	connectRefreshCachedSettingsSafe("RenderShadowOrthoClipPlanes");
	connectRefreshCachedSettingsSafe("RenderFarClip");
	connectRefreshCachedSettingsSafe("RenderShadowSplitExponent");
	connectRefreshCachedSettingsSafe("RenderShadowErrorCutoff");
	connectRefreshCachedSettingsSafe("RenderShadowFOVCutoff");
	connectRefreshCachedSettingsSafe("CameraOffset");
	connectRefreshCachedSettingsSafe("CameraMaxCoF");
	connectRefreshCachedSettingsSafe("CameraDoFResScale");
	connectRefreshCachedSettingsSafe("RenderAutoHideGeometryMemoryLimit");
	connectRefreshCachedSettingsSafe("RenderAutoHideSurfaceAreaLimit");
	connectRefreshCachedSettingsSafe("RenderWater");
	connectRefreshCachedSettingsSafe("RenderWaterCull");

	// Beacons stuff
	connectRefreshCachedSettingsSafe("scriptsbeacon");
	connectRefreshCachedSettingsSafe("scripttouchbeacon");
	connectRefreshCachedSettingsSafe("physicalbeacon");
	connectRefreshCachedSettingsSafe("permanentbeacon");
	connectRefreshCachedSettingsSafe("characterbeacon");
	connectRefreshCachedSettingsSafe("soundsbeacon");
	connectRefreshCachedSettingsSafe("invisiblesoundsbeacon");
	connectRefreshCachedSettingsSafe("particlesbeacon");
	connectRefreshCachedSettingsSafe("moapbeacon");
	connectRefreshCachedSettingsSafe("renderhighlights");
	connectRefreshCachedSettingsSafe("renderbeacons");
	connectRefreshCachedSettingsSafe("renderattachment");
	connectRefreshCachedSettingsSafe("renderbyowner");
	connectRefreshCachedSettingsSafe("DebugBeaconLineWidth");
}

void LLPipeline::cleanup()
{
	mGroupQ1.clear();
	mGroupSaveQ1.clear();
	mGroupQ2.clear();

	for (pool_set_t::iterator iter = mPools.begin(),
							  end = mPools.end();
		 iter != end; )
	{
		pool_set_t::iterator curiter = iter++;
		LLDrawPool* poolp = *curiter;

		if (!poolp)	// Paranoia
		{
			llwarns << "Found a NULL pool !" << llendl;
			continue;
		}

		if (poolp->isFacePool())
		{
			LLFacePool* face_pool = (LLFacePool*)poolp;
			if (face_pool->mReferences.empty())
			{
				mPools.erase(curiter);
				removeFromQuickLookup(poolp);
				delete poolp;
			}
		}
		else
		{
			mPools.erase(curiter);
			removeFromQuickLookup(poolp);
			delete poolp;
		}
	}

	if (!mTerrainPools.empty())
	{
		llwarns << "Terrain Pools not cleaned up" << llendl;
	}
	if (!mTreePools.empty())
	{
		llwarns << "Tree Pools not cleaned up" << llendl;
	}

	delete mAlphaPool;
	mAlphaPool = NULL;
	delete mSkyPool;
	mSkyPool = NULL;
	delete mTerrainPool;
	mTerrainPool = NULL;
	delete mWaterPool;
	mWaterPool = NULL;
	delete mGroundPool;
	mGroundPool = NULL;
	delete mSimplePool;
	mSimplePool = NULL;
	delete mFullbrightPool;
	mFullbrightPool = NULL;
	delete mInvisiblePool;
	mInvisiblePool = NULL;
	delete mGlowPool;
	mGlowPool = NULL;
	delete mBumpPool;
	mBumpPool = NULL;
#if 0	// Do not delete WL sky pool it was handled above in the for loop
	delete mWLSkyPool;
#endif
	mWLSkyPool = NULL;

	releaseGLBuffers();

	mFaceSelectImagep = NULL;

	mMovedList.clear();
	mMovedBridge.clear();

	mMeshDirtyGroup.clear();

	mShiftList.clear();

	mInitialized = false;

	mDeferredVB = mGlowCombineVB = mCubeVB = NULL;
}

void LLPipeline::dumpStats()
{
	llinfos << "mMovedList vector capacity reached: " << mMovedList.capacity()
			<< " - mMovedBridge vector capacity reached: "
			<< mMovedBridge.capacity()
			<< " - mShiftList vector capacity reached: "
			<< mShiftList.capacity()
			<< " - mGroupQ1 vector capacity reached: " << mGroupQ1.capacity()
			<< " - mGroupQ2 vector capacity reached: " << mGroupQ2.capacity()
			<< " - mMeshDirtyGroup vector capacity reached: "
			<< mMeshDirtyGroup.capacity() << llendl;
}

void LLPipeline::destroyGL()
{
	unloadShaders();
	mHighlightFaces.clear();

	resetDrawOrders();

	resetVertexBuffers();

	releaseGLBuffers();

	if (LLVertexBuffer::sEnableVBOs && !LLStartUp::isLoggedIn())
	{
		// Render 30 frames after switching to work around DEV-5361
		sDelayedVBOEnable = 30;
		LLVertexBuffer::sEnableVBOs = false;
	}

	if (mMeshDirtyQueryObject)
	{
		glDeleteQueriesARB(1, &mMeshDirtyQueryObject);
		mMeshDirtyQueryObject = 0;
	}
	stop_glerror();
}

//static
void LLPipeline::throttleNewMemoryAllocation(bool disable)
{
	sMemAllocationThrottled = disable;
}

void LLPipeline::resizeShadowTexture()
{
	gResizeShadowTexture = false;
	releaseShadowTargets();
	allocateShadowBuffer(mScreenWidth, mScreenHeight);
}

void LLPipeline::resizeScreenTexture()
{
	static U32 res_divisor = 0;

	LL_FAST_TIMER(FTM_RESIZE_SCREEN_TEXTURE);

	gResizeScreenTexture = false;

	if (canUseVertexShaders())
	{
		GLuint res_x = gViewerWindowp->getWindowDisplayWidth();
		GLuint res_y = gViewerWindowp->getWindowDisplayHeight();
		if (res_x != mScreen.getWidth() || res_y != mScreen.getHeight() ||
			res_divisor != RenderResolutionDivisor)
		{
			res_divisor = RenderResolutionDivisor;
			releaseScreenBuffers();
			allocateScreenBuffer(res_x, res_y);
		}
	}
}

void LLPipeline::allocatePhysicsBuffer()
{
	GLuint res_x = gViewerWindowp->getWindowDisplayWidth();
	GLuint res_y = gViewerWindowp->getWindowDisplayHeight();

	if (mPhysicsDisplay.getWidth() != res_x ||
		mPhysicsDisplay.getHeight() != res_y)
	{
		mPhysicsDisplay.allocate(res_x, res_y, GL_RGBA, true, false,
								 LLTexUnit::TT_RECT_TEXTURE, false);
	}
}

void LLPipeline::allocateScreenBuffer(U32 res_x, U32 res_y)
{
	refreshCachedSettings();

	U32 samples = RenderFSAASamples;

	// Try to allocate screen buffers at requested resolution and samples:
	// - on failure, shrink number of samples and try again
	// - if not multisampled, shrink resolution and try again (favor X
	//   resolution over Y)
	// Make sure to call "releaseScreenBuffers" after each failure to cleanup
	// the partially loaded state

	if (!allocateScreenBuffer(res_x, res_y, samples))
	{
		releaseScreenBuffers();
		// Reduce number of samples
		while (samples > 0)
		{
			samples /= 2;
			if (allocateScreenBuffer(res_x, res_y, samples))
			{
				return;	// success
			}
			releaseScreenBuffers();
		}

		samples = 0;

		// Reduce resolution
		while (res_y > 0 && res_x > 0)
		{
			res_y /= 2;
			if (allocateScreenBuffer(res_x, res_y, samples))
			{
				return;
			}
			releaseScreenBuffers();

			res_x /= 2;
			if (allocateScreenBuffer(res_x, res_y, samples))
			{
				return;
			}
			releaseScreenBuffers();
		}

		llwarns << "Unable to allocate screen buffer at any resolution !"
				<< llendl;
	}
}

bool LLPipeline::allocateScreenBuffer(U32 res_x, U32 res_y, U32 samples)
{
	refreshCachedSettings();

	U32 res_mod = RenderResolutionDivisor;
	if (res_mod > 1 && res_mod < res_x && res_mod < res_y)
	{
//MK
		// *HACK: avoids issues and cheating when drawing cloud spheres around
		// the avatar and RenderResolutionDivisor is larger than 1
		if (res_mod < 256 && gRLenabled && gRLInterface.mVisionRestricted)
		{
			res_mod = 256;
		}
//mk
		res_x /= res_mod;
		res_y /= res_mod;
	}

	// Remember these dimensions
	mScreenWidth = res_x;
	mScreenHeight = res_y;

	if (sRenderDeferred)
	{
		// Set this flag in case we crash while resizing window or allocating
		// space for deferred rendering targets
		gSavedSettings.setBool("RenderInitError", true);
		gAppViewerp->saveGlobalSettings();

		constexpr U32 occlusion_divisor = 3;

		// Allocate deferred rendering color buffers
		if (!mDeferredScreen.allocate(res_x, res_y, GL_SRGB8_ALPHA8, true, true,
									  LLTexUnit::TT_RECT_TEXTURE, false,
									  samples))
		{
			return false;
		}
		if (!mDeferredDepth.allocate(res_x, res_y, 0, true, false,
									 LLTexUnit::TT_RECT_TEXTURE, false,
									 samples))
		{
			return false;
		}
		if (!mOcclusionDepth.allocate(res_x / occlusion_divisor,
									  res_y / occlusion_divisor, 0, true, false,
									  LLTexUnit::TT_RECT_TEXTURE, false,
									  samples))
		{
			return false;
		}
		if (!addDeferredAttachments(mDeferredScreen))
		{
			return false;
		}

		GLuint screen_format = GL_RGBA16;
		if (gGLManager.mIsAMD)
		{
			static LLCachedControl<bool> use_rgba16(gSavedSettings,
													"RenderUseRGBA16ATI");
			if (!use_rgba16 || gGLManager.mGLVersion < 4.f)
			{
				screen_format = GL_RGBA12;
			}
		}
		else if (gGLManager.mIsNVIDIA && gGLManager.mGLVersion < 4.f)
		{
			screen_format = GL_RGBA16F_ARB;
		}
		if (!mScreen.allocate(res_x, res_y, screen_format, false, false,
							  LLTexUnit::TT_RECT_TEXTURE, false, samples))
		{
			return false;
		}

		if (samples > 0)
		{
			if (!mFXAABuffer.allocate(res_x, res_y, GL_RGBA, false, false,
									  LLTexUnit::TT_TEXTURE, false, samples))
			{
				return false;
			}
		}
		else
		{
			mFXAABuffer.release();
		}

		if (RenderShadowDetail > 0 || RenderDeferredSSAO ||
			RenderDepthOfField || samples > 0)
		{
			// Only need mDeferredLight for shadows or SSAO or DOF or FXAA
			if (!mDeferredLight.allocate(res_x, res_y, GL_RGBA, false, false,
										 LLTexUnit::TT_RECT_TEXTURE, false))
			{
				return false;
			}
		}
		else
		{
			mDeferredLight.release();
		}

		allocateShadowBuffer(res_x, res_y);

		// Do not disable shaders on next session
		gSavedSettings.setBool("RenderInitError", false);
		gAppViewerp->saveGlobalSettings();
	}
	else
	{
		mDeferredLight.release();

		releaseShadowTargets();

		mFXAABuffer.release();
		mScreen.release();
		// Make sure to release any render targets that share a depth buffer
		// with mDeferredScreen first:
		mDeferredScreen.release();
		mDeferredDepth.release();
		mOcclusionDepth.release();

		if (!mScreen.allocate(res_x, res_y, GL_RGBA, true, true,
							  LLTexUnit::TT_RECT_TEXTURE, false))
		{
			return false;
		}
	}

	if (sRenderDeferred)
	{
		// Share depth buffer between deferred targets
		mDeferredScreen.shareDepthBuffer(mScreen);
	}

	gGL.getTexUnit(0)->disable();

	stop_glerror();

	return true;
}

// Must be even to avoid a stripe in the horizontal shadow blur
LL_INLINE static U32 blur_happy_size(U32 x, F32 scale)
{
	return (U32((F32)x * scale) + 16) & ~0xF;
}

bool LLPipeline::allocateShadowBuffer(U32 res_x, U32 res_y)
{
	refreshCachedSettings();

	if (!sRenderDeferred)
	{
		return true;
	}

	constexpr U32 occlusion_divisor = 3;
	F32 scale = RenderShadowResolutionScale;

	if (RenderShadowDetail > 0)
	{
		// Allocate 4 sun shadow maps

		U32 sun_shadow_map_width = blur_happy_size(res_x, scale);
		U32 sun_shadow_map_height = blur_happy_size(res_y, scale);
		for (U32 i = 0; i < 4; ++i)
		{
			if (!mShadow[i].allocate(sun_shadow_map_width,
									 sun_shadow_map_height, 0, true, false,
									 LLTexUnit::TT_TEXTURE))
			{
				return false;
			}
			if (!mShadowOcclusion[i].allocate(sun_shadow_map_width /
											  occlusion_divisor,
											  sun_shadow_map_height /
											  occlusion_divisor,
											  0, true, false,
											  LLTexUnit::TT_TEXTURE))
			{
				return false;
			}
		}
	}
	else
	{
		for (U32 i = 0; i < 4; ++i)
		{
			releaseShadowTarget(i);
		}
	}

	if (RenderShadowDetail > 1)
	{
		// Allocate two spot shadow maps
		U32 size = res_x * scale;
		for (U32 i = 4; i < 6; ++i)
		{
			if (!mShadow[i].allocate(size, size, 0, true, false))
			{
				return false;
			}
			if (!mShadowOcclusion[i].allocate(size / occlusion_divisor,
											  size / occlusion_divisor,
											  0, true, false))
			{
				return false;
			}
		}
	}
	else
	{
		for (U32 i = 4; i < 6; ++i)
		{
			releaseShadowTarget(i);
		}
	}

	return true;
}

//static
void LLPipeline::updateRenderDeferred()
{
	sRenderDeferred = RenderDeferred && sRenderBump && RenderAvatarVP &&
					  !gUseWireframe && LLRenderTarget::sUseFBO &&
					  WindLightUseAtmosShaders &&
					  gFeatureManager.isFeatureAvailable("RenderDeferred");
#if 1	// *TODO: check that this still could happen at all...
	if (!sRenderDeferred && LLRenderTarget::sUseFBO)
	{
		// This case should never happen, but some odd startup state can cause
		// it to occur.
		LLRenderTarget::sUseFBO = false;
	}
#endif
}

//static
void LLPipeline::refreshCachedSettings()
{
	static LLCachedControl<bool> scriptseacon(gSavedSettings, "scriptsbeacon");
	sRenderScriptedBeacons = scriptseacon;

	static LLCachedControl<bool> scripttouchbeacon(gSavedSettings,
												   "scripttouchbeacon");
	sRenderScriptedTouchBeacons = scripttouchbeacon;

	static LLCachedControl<bool> physbeacon(gSavedSettings, "physicalbeacon");
	sRenderPhysicalBeacons = physbeacon;

	static LLCachedControl<bool> permbeacon(gSavedSettings, "permanentbeacon");
	sRenderPermanentBeacons = permbeacon;

	static LLCachedControl<bool> charbeacon(gSavedSettings, "characterbeacon");
	sRenderCharacterBeacons = charbeacon;

	static LLCachedControl<bool> soundsbeacon(gSavedSettings, "soundsbeacon");
	sRenderSoundBeacons = soundsbeacon;

	static LLCachedControl<bool> invisisoundsbeacon(gSavedSettings,
													"invisiblesoundsbeacon");
	sRenderInvisibleSoundBeacons = invisisoundsbeacon;

	static LLCachedControl<bool> particlesbeacon(gSavedSettings,
												 "particlesbeacon");
	sRenderParticleBeacons = particlesbeacon;

	static LLCachedControl<bool> moapbeacon(gSavedSettings, "moapbeacon");
	sRenderMOAPBeacons = moapbeacon;

	static LLCachedControl<bool> renderbeacons(gSavedSettings,
											   "renderbeacons");
	sRenderBeacons = renderbeacons;

	static LLCachedControl<bool> renderhighlights(gSavedSettings,
												  "renderhighlights");
	sRenderHighlight = renderhighlights;

	static LLCachedControl<bool> renderattachment(gSavedSettings,
												  "renderattachment");
	sRenderAttachments = renderattachment;

	static LLCachedControl<U32> renderbyowner(gSavedSettings, "renderbyowner");
	sRenderByOwner = renderbyowner;

	static LLCachedControl<U32> beacon_width(gSavedSettings,
											 "DebugBeaconLineWidth");
	DebugBeaconLineWidth = beacon_width;

	static LLCachedControl<bool> mask_alpha_def(gSavedSettings,
												"RenderAutoMaskAlphaDeferred");
	sAutoMaskAlphaDeferred = mask_alpha_def;

	static LLCachedControl<bool> mask_alpha(gSavedSettings,
											"RenderAutoMaskAlphaNonDeferred");
	sAutoMaskAlphaNonDeferred = mask_alpha;

	static LLCachedControl<bool> use_far_clip(gSavedSettings,
											  "RenderUseFarClip");
	sUseFarClip = use_far_clip;

	static LLCachedControl<bool> delay_vb_update(gSavedSettings,
												 "RenderDelayVBUpdate");
	sDelayVBUpdate = delay_vb_update;

	static LLCachedControl<bool> use_occlusion(gSavedSettings, "UseOcclusion");
	sUseOcclusion = (use_occlusion && !gUseWireframe &&
					 gFeatureManager.isFeatureAvailable("UseOcclusion") &&
					 gGLManager.mHasOcclusionQuery) ? 2 : 0;

	static LLCachedControl<bool> render_water(gSavedSettings, "RenderWater");
	sRenderWater = render_water;

	static LLCachedControl<bool> avatar_vp(gSavedSettings, "RenderAvatarVP");
	RenderAvatarVP = avatar_vp;

	static LLCachedControl<bool> use_atmos_shaders(gSavedSettings,
												   "WindLightUseAtmosShaders");
	WindLightUseAtmosShaders = use_atmos_shaders;

	static LLCachedControl<bool> render_deferred(gSavedSettings,
												 "RenderDeferred");
	RenderDeferred = render_deferred;

	// Test this after RenderDeferred...
	static LLCachedControl<U32> cull_water(gSavedSettings, "RenderWaterCull");
	switch ((U32)cull_water)
	{
		case 0:
			sRenderWaterCull = false;
			break;

		case 1:
			sRenderWaterCull = gUseNewShaders && RenderDeferred;
			break;

		case 2:
			sRenderWaterCull = gUseNewShaders;
			break;

		default:
			sRenderWaterCull = true;
	}

	static LLCachedControl<F32> sun_wash(gSavedSettings,
										 "RenderDeferredSunWash");
	RenderDeferredSunWash = sun_wash;

	static LLCachedControl<F32> gamma(gSavedSettings,
									  "RenderDeferredDisplayGamma");
	RenderDeferredDisplayGamma = gamma > 0.1f ? gamma : 2.2f;

	static LLCachedControl<U32> fsaa_samp(gSavedSettings, "RenderFSAASamples");
	RenderFSAASamples = fsaa_samp;

	static LLCachedControl<U32> res_divisor(gSavedSettings,
											"RenderResolutionDivisor");
	RenderResolutionDivisor = llmax((U32)res_divisor, 1U);

	static LLCachedControl<S32> shadow_detail(gSavedSettings,
											  "RenderShadowDetail");
	RenderShadowDetail = shadow_detail;

	static LLCachedControl<bool> deferred_ssao(gSavedSettings,
											   "RenderDeferredSSAO");
	RenderDeferredSSAO = deferred_ssao;

	static LLCachedControl<F32> shadow_res_scl(gSavedSettings,
											   "RenderShadowResolutionScale");
	RenderShadowResolutionScale = llmax((F32)shadow_res_scl, 0.f);

	static LLCachedControl<S32> lighting_details(gSavedSettings,
												 "RenderLightingDetail");
	RenderLightingDetail = lighting_details;

	static LLCachedControl<bool> delay_creation(gSavedSettings,
												"RenderDelayCreation");
	RenderDelayCreation = delay_creation;

	static LLCachedControl<bool> anim_res(gSavedSettings, "RenderAnimateRes");
	RenderAnimateRes = anim_res;

	static LLCachedControl<bool> spot_lights_nd(gSavedSettings,
												"RenderSpotLightsInNondeferred");
	RenderSpotLightsInNondeferred = spot_lights_nd;

	static LLCachedControl<LLColor4> preview_amb_col(gSavedSettings,
													 "PreviewAmbientColor");
	PreviewAmbientColor = preview_amb_col;

	static LLCachedControl<LLColor4> preview_diff0(gSavedSettings,
												   "PreviewDiffuse0");
	PreviewDiffuse0 = preview_diff0;

	static LLCachedControl<LLColor4> preview_spec0(gSavedSettings,
												   "PreviewSpecular0");
	PreviewSpecular0 = preview_spec0;

	static LLCachedControl<LLVector3> preview_dir0(gSavedSettings,
												   "PreviewDirection0");
	PreviewDirection0 = preview_dir0;
	PreviewDirection0.normalize();

	static LLCachedControl<LLColor4> preview_diff1(gSavedSettings,
												   "PreviewDiffuse1");
	PreviewDiffuse1 = preview_diff1;

	static LLCachedControl<LLColor4> preview_spec1(gSavedSettings,
												   "PreviewSpecular1");
	PreviewSpecular1 = preview_spec1;

	static LLCachedControl<LLVector3> preview_dir1(gSavedSettings,
												   "PreviewDirection1");
	PreviewDirection1 = preview_dir1;
	PreviewDirection1.normalize();

	static LLCachedControl<LLColor4> preview_diff2(gSavedSettings,
												   "PreviewDiffuse2");
	PreviewDiffuse2 = preview_diff2;

	static LLCachedControl<LLColor4> preview_spec2(gSavedSettings,
												   "PreviewSpecular2");
	PreviewSpecular2 = preview_spec2;

	static LLCachedControl<LLVector3> preview_dir2(gSavedSettings,
												   "PreviewDirection2");
	PreviewDirection2 = preview_dir2;
	PreviewDirection2.normalize();

	static LLCachedControl<bool> render_glow(gSavedSettings, "RenderGlow");
	RenderGlow = sCanRenderGlow && render_glow;

	static LLCachedControl<F32> glow_min_lum(gSavedSettings,
											 "RenderGlowMinLuminance");
	RenderGlowMinLuminance = llmax((F32)glow_min_lum, 0.f);

	static LLCachedControl<F32> glow_max_alpha(gSavedSettings,
											   "RenderGlowMaxExtractAlpha");
	RenderGlowMaxExtractAlpha = glow_max_alpha;

	static LLCachedControl<F32> glow_warmth_a(gSavedSettings,
											  "RenderGlowWarmthAmount");
	RenderGlowWarmthAmount = glow_warmth_a;

	static LLCachedControl<LLVector3> glow_lum_w(gSavedSettings,
												 "RenderGlowLumWeights");
	RenderGlowLumWeights = glow_lum_w;

	static LLCachedControl<LLVector3> glow_warmth_w(gSavedSettings,
													"RenderGlowWarmthWeights");
	RenderGlowWarmthWeights = glow_warmth_w;

	static LLCachedControl<U32> glow_res_pow(gSavedSettings,
											 "RenderGlowResolutionPow");
	RenderGlowResolutionPow = glow_res_pow;

	static LLCachedControl<U32> glow_iterations(gSavedSettings,
												"RenderGlowIterations");
	RenderGlowIterations = glow_iterations;

	static LLCachedControl<F32> glow_width(gSavedSettings, "RenderGlowWidth");
	RenderGlowWidth = glow_width;

	static LLCachedControl<F32> glow_strength(gSavedSettings,
											  "RenderGlowStrength");
	RenderGlowStrength = llmax(0.f, (F32)glow_strength);

	static LLCachedControl<bool> depth_of_field(gSavedSettings,
												"RenderDepthOfField");
	RenderDepthOfField = depth_of_field;

	static LLCachedControl<bool> dof_in_edit(gSavedSettings,
											 "RenderDepthOfFieldInEditMode");
	RenderDepthOfFieldInEditMode = dof_in_edit;

	static LLCachedControl<F32> cam_trans_time(gSavedSettings,
											   "CameraFocusTransitionTime");
	CameraFocusTransitionTime = cam_trans_time;

	static LLCachedControl<F32> camera_fnum(gSavedSettings, "CameraFNumber");
	CameraFNumber = camera_fnum;

	static LLCachedControl<F32> camera_default_focal(gSavedSettings,
													 "CameraFocalLength");
	CameraFocalLength = camera_default_focal;

	static LLCachedControl<F32> camera_field(gSavedSettings,
											 "CameraFieldOfView");
	CameraFieldOfView = camera_field;

	static LLCachedControl<F32> shadow_noise(gSavedSettings,
											 "RenderShadowNoise");
	RenderShadowNoise = shadow_noise;

	static LLCachedControl<F32> shadow_blur_size(gSavedSettings,
												 "RenderShadowBlurSize");
	RenderShadowBlurSize = shadow_blur_size;

	static LLCachedControl<F32> ssao_scale(gSavedSettings, "RenderSSAOScale");
	RenderSSAOScale = ssao_scale;

	static LLCachedControl<U32> ssao_max_scale(gSavedSettings,
											   "RenderSSAOMaxScale");
	RenderSSAOMaxScale = ssao_max_scale;

	static LLCachedControl<F32> ssao_fact(gSavedSettings, "RenderSSAOFactor");
	RenderSSAOFactor = ssao_fact;

	static LLCachedControl<LLVector3> ssao_effect(gSavedSettings,
												  "RenderSSAOEffect");
	RenderSSAOEffect = ssao_effect;

	static LLCachedControl<F32> shadow_bias_error(gSavedSettings,
												  "RenderShadowBiasError");
	RenderShadowBiasError = shadow_bias_error;

	static LLCachedControl<F32> shadow_offset(gSavedSettings,
											  "RenderShadowOffset");
	RenderShadowOffset = shadow_offset;

	static LLCachedControl<F32> shadow_off_no_ssao(gSavedSettings,
												   "RenderShadowOffsetNoSSAO");
	RenderShadowOffsetNoSSAO = shadow_off_no_ssao;

	static LLCachedControl<F32> shadow_bias(gSavedSettings,
											"RenderShadowBias");
	RenderShadowBias = shadow_bias;

	static LLCachedControl<F32> spot_shadow_offset(gSavedSettings,
												   "RenderSpotShadowOffset");
	RenderSpotShadowOffset = spot_shadow_offset;

	static LLCachedControl<F32> spot_shadow_bias(gSavedSettings,
												 "RenderSpotShadowBias");
	RenderSpotShadowBias = spot_shadow_bias;

	static LLCachedControl<LLVector3> shadow_gaussian(gSavedSettings,
													  "RenderShadowGaussian");
	RenderShadowGaussian = shadow_gaussian;

	static LLCachedControl<F32> shadow_blur_dist(gSavedSettings,
												 "RenderShadowBlurDistFactor");
	RenderShadowBlurDistFactor = shadow_blur_dist;

	static LLCachedControl<bool> deferred_atmos(gSavedSettings,
												"RenderDeferredAtmospheric");
	RenderDeferredAtmospheric = deferred_atmos;

	static LLCachedControl<bool> transp_water(gSavedSettings,
											  "RenderTransparentWater");
	sRenderTransparentWater = transp_water;

	static LLCachedControl<bool> water_reflection(gSavedSettings,
												  "RenderWaterReflections");
	RenderWaterReflections = sWaterReflections && sRenderTransparentWater &&
							 water_reflection;

	static LLCachedControl<S32> reflection_detail(gSavedSettings,
												  "RenderReflectionDetail");
	RenderReflectionDetail = reflection_detail;

	static LLCachedControl<LLVector3> shadow_planes(gSavedSettings,
													"RenderShadowClipPlanes");
	RenderShadowClipPlanes = shadow_planes;

	static LLCachedControl<LLVector3> shadow_ortho(gSavedSettings,
												   "RenderShadowOrthoClipPlanes");
	RenderShadowOrthoClipPlanes = shadow_ortho;

	static LLCachedControl<F32> far_clip(gSavedSettings, "RenderFarClip");
	RenderFarClip = far_clip;

	static LLCachedControl<LLVector3> shadow_split_exp(gSavedSettings,
													   "RenderShadowSplitExponent");
	RenderShadowSplitExponent = shadow_split_exp;

	static LLCachedControl<F32> shadow_error_cutoff(gSavedSettings,
													"RenderShadowErrorCutoff");
	RenderShadowErrorCutoff = shadow_error_cutoff;

	static LLCachedControl<F32> shadow_fov_cutoff(gSavedSettings,
												  "RenderShadowFOVCutoff");
	RenderShadowFOVCutoff = llmin((F32)shadow_fov_cutoff, 1.4f);

	static LLCachedControl<bool> camera_offset(gSavedSettings, "CameraOffset");
	CameraOffset = camera_offset;

	static LLCachedControl<F32> camera_max_cof(gSavedSettings, "CameraMaxCoF");
	CameraMaxCoF = camera_max_cof;

	static LLCachedControl<F32> camera_dof_scale(gSavedSettings,
												 "CameraDoFResScale");
	CameraDoFResScale = camera_dof_scale;

	static LLCachedControl<U32> auto_hide_mem(gSavedSettings,
											  "RenderAutoHideGeometryMemoryLimit");
	RenderAutoHideGeometryMemoryLimit = auto_hide_mem;

	static LLCachedControl<F32> auto_hide_area(gSavedSettings,
											   "RenderAutoHideSurfaceAreaLimit");
	RenderAutoHideSurfaceAreaLimit = auto_hide_area;

	sRenderSpotLight = NULL;

	updateRenderDeferred();
}

void LLPipeline::releaseGLBuffers()
{
	if (mNoiseMap)
	{
		LLImageGL::deleteTextures(1, &mNoiseMap);
		mNoiseMap = 0;
	}

	if (mTrueNoiseMap)
	{
		LLImageGL::deleteTextures(1, &mTrueNoiseMap);
		mTrueNoiseMap = 0;
	}

	releaseLUTBuffers();

	mWaterRef.release();
	mWaterDis.release();
	mHighlight.release();
#if LL_USE_DYNAMIC_TEX_FBO
	if (!gGLManager.mIsAMD)
	{
		mBake.release();
	}
#endif

	for (U32 i = 0; i < 3; ++i)
	{
		mGlow[i].release();
	}

	releaseScreenBuffers();

	gBumpImageList.destroyGL();
	LLVOAvatar::resetImpostors();
}

void LLPipeline::releaseLUTBuffers()
{
	if (mLightFunc)
	{
		LLImageGL::deleteTextures(1, &mLightFunc);
		mLightFunc = 0;
	}
}

void LLPipeline::releaseScreenBuffers()
{
	releaseShadowTargets();

	mScreen.release();
	mFXAABuffer.release();
	mPhysicsDisplay.release();
	mDeferredScreen.release();
	mDeferredDepth.release();
	mDeferredLight.release();
	mOcclusionDepth.release();
}

void LLPipeline::releaseShadowTarget(U32 index)
{
	mShadow[index].release();
	mShadowOcclusion[index].release();
}

void LLPipeline::releaseShadowTargets()
{
	for (U32 i = 0; i < 6; ++i)
	{
		mShadow[i].release();
		mShadowOcclusion[i].release();
	}
}

void LLPipeline::createGLBuffers()
{
	updateRenderDeferred();

	if (sWaterReflections)
	{
		// Water reflection texture
		U32 res = llmax(gSavedSettings.getU32("RenderWaterRefResolution"),
						512U);

		// Set up SRGB targets if we are doing deferred-path reflection
		// rendering
		mWaterRef.allocate(res, res, GL_RGBA, true, false);
		mWaterDis.allocate(res, res, GL_RGBA, true, false,
						   LLTexUnit::TT_TEXTURE);
	}

#if LL_USE_DYNAMIC_TEX_FBO
	if (!gGLManager.mIsAMD)
	{
		// Use FBO for bake tex
		mBake.allocate(512, 512, GL_RGBA, TRUE, FALSE, LLTexUnit::TT_TEXTURE,
					   true);
	}
#endif

	mHighlight.allocate(256, 256, GL_RGBA, false, false);

	GLuint res_x = gViewerWindowp->getWindowDisplayWidth();
	GLuint res_y = gViewerWindowp->getWindowDisplayHeight();

	// Screen space glow buffers
	U32 glow_pow = gSavedSettings.getU32("RenderGlowResolutionPow");
	// Limited between 16 and 512
	const U32 glow_res = 1 << llclamp(glow_pow, 4U, 9U);

	for (U32 i = 0; i < 3; ++i)
	{
		mGlow[i].allocate(512, glow_res, GL_RGBA, false, false);
	}

	allocateScreenBuffer(res_x, res_y);

	if (sRenderDeferred)
	{
		if (!mNoiseMap)
		{
			constexpr U32 noise_res = 128;
			LLVector3 noise[noise_res * noise_res];

			F32 scaler = gSavedSettings.getF32("RenderDeferredNoise") / 100.f;
			for (U32 i = 0; i < noise_res * noise_res; ++i)
			{
				noise[i].set(ll_frand() - 0.5f, ll_frand() - 0.5f, 0.f);
				noise[i].normalize();
				noise[i].mV[2] = ll_frand() * scaler + 1.f - scaler * 0.5f;
			}

			LLImageGL::generateTextures(1, &mNoiseMap);

			LLTexUnit* unit0 = gGL.getTexUnit(0);
			unit0->bindManual(LLTexUnit::TT_TEXTURE, mNoiseMap);
			LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE),
									  0, GL_RGB16F_ARB, noise_res, noise_res,
									  GL_RGB, GL_FLOAT, noise, false);
			unit0->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		}

		if (!mTrueNoiseMap)
		{
			constexpr U32 noise_res = 128;
			F32 noise[noise_res * noise_res * 3];
			for (U32 i = 0; i < noise_res * noise_res * 3; ++i)
			{
				noise[i] = ll_frand() * 2.f - 1.f;
			}

			LLImageGL::generateTextures(1, &mTrueNoiseMap);
			LLTexUnit* unit0 = gGL.getTexUnit(0);
			unit0->bindManual(LLTexUnit::TT_TEXTURE, mTrueNoiseMap);
			LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE),
									  0, GL_RGB16F_ARB, noise_res, noise_res,
									  GL_RGB, GL_FLOAT, noise, false);
			unit0->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		}

		createLUTBuffers();
	}

	gBumpImageList.restoreGL();

	stop_glerror();
}

void LLPipeline::createLUTBuffers()
{
	if (!sRenderDeferred || mLightFunc)
	{
		return;
	}

	static LLCachedControl<U32> light_res_x(gSavedSettings,
											"RenderSpecularResX");
	static LLCachedControl<U32> light_res_y(gSavedSettings,
											"RenderSpecularResY");
	static LLCachedControl<F32> spec_exp(gSavedSettings,
										 "RenderSpecularExponent");

	F32* ls = new F32[light_res_x * light_res_y];
	// Calculate the (normalized) blinn-phong specular lookup texture (with a
	// few tweaks).
	for (U32 y = 0; y < light_res_y; ++y)
	{
		for (U32 x = 0; x < light_res_x; ++x)
		{
			ls[y * light_res_x + x] = 0.f;
			F32 sa = (F32)x / (F32)(light_res_x - 1);
			F32 spec = (F32)y / (F32)(light_res_y - 1);
			F32 n = spec * spec * spec_exp;

			// Nothing special here, just your typical blinn-phong term
			spec = powf(sa, n);

			// Apply our normalization function.
			// Note: This is the full equation that applies the full
			// normalization curve, not an approximation. This is fine, given
			// we only need to create our LUT once per buffer initialization.
			// The only trade off is we have a really low dynamic range. This
			// means we have to account for things not being able to exceed 0
			// to 1 in our shaders.
			spec *= (n + 2.f) * (n + 4.f) /
					(8.f * F_PI * (powf(2.f, -0.5f * n) + n));
			// Since we use R16F, we no longer have a dynamic range issue we
			// need to work around here. Though some older drivers may not like
			// this, newer drivers should not have this problem.
			ls[y * light_res_x + x] = spec;
		}
	}

#if LL_DARWIN
	// Work around for limited precision with 10.6.8 and older drivers
	constexpr U32 pix_format = GL_R32F;
#else
	constexpr U32 pix_format = GL_R16F;
#endif
	LLImageGL::generateTextures(1, &mLightFunc);
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->bindManual(LLTexUnit::TT_TEXTURE, mLightFunc);
	LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE),
							  0, pix_format, light_res_x, light_res_y,
							  GL_RED, GL_FLOAT, ls, false);
	unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
	unit0->setTextureFilteringOption(LLTexUnit::TFO_TRILINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	delete[] ls;
}

void LLPipeline::restoreGL()
{
	if (mVertexShadersLoaded == 0)
	{
		gViewerShaderMgrp->setShaders();
	}

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* region = *iter;
		for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			part->restoreGL();
		}
	}
}

bool LLPipeline::canUseWindLightShaders() const
{
	return gWLSkyProgram.mProgramObject &&
		   gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_WINDLIGHT) > 1;
}

bool LLPipeline::canUseWindLightShadersOnObjects() const
{
	return canUseWindLightShaders() &&
		   gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT) > 0;
}

void LLPipeline::unloadShaders()
{
	gViewerShaderMgrp->unloadShaders();
	if (mVertexShadersLoaded != -1)
	{
		mVertexShadersLoaded = 0;
	}
}

S32 LLPipeline::getMaxLightingDetail() const
{
#if 0
	if (mShaderLevel[SHADER_OBJECT] >=
			LLDrawPoolSimple::SHADER_LEVEL_LOCAL_LIGHTS)
	{
		return 3;
	}
#endif
	return 1;
}

S32 LLPipeline::setLightingDetail(S32 level)
{
	refreshCachedSettings();

	if (level < 0)
	{
		level = RenderLightingDetail;
	}
	level = llclamp(level, 0, getMaxLightingDetail());
	mLightingDetail = level;

	return mLightingDetail;
}

class LLOctreeDirtyTexture final : public OctreeTraveler
{
public:
	LLOctreeDirtyTexture(const LLViewerTextureList::dirty_list_t& textures)
	:	mTextures(textures)
	{
	}

	void visit(const OctreeNode* node) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)node->getListener(0);
		if (!group) return;

		if (!group->hasState(LLSpatialGroup::GEOM_DIRTY) && !group->isEmpty())
		{
			LLViewerTextureList::dirty_list_t::const_iterator textures_end =
				mTextures.end();
			for (LLSpatialGroup::draw_map_t::const_iterator
					i = group->mDrawMap.begin(), end = group->mDrawMap.end();
				 i != end; ++i)
			{
				for (LLSpatialGroup::drawmap_elem_t::const_iterator
						j = i->second.begin(), end2 = i->second.end();
					 j != end2; ++j)
				{
					LLDrawInfo* params = *j;
					LLViewerFetchedTexture* tex =
						LLViewerTextureManager::staticCastToFetchedTexture(params->mTexture);
					if (tex && mTextures.find(tex) != textures_end)
					{
						group->setState(LLSpatialGroup::GEOM_DIRTY);
					}
				}
			}
		}

		for (LLSpatialGroup::bridge_list_t::iterator
				i = group->mBridgeList.begin(), end = group->mBridgeList.end();
			 i != end; ++i)
		{
			LLSpatialBridge* bridge = *i;
			traverse(bridge->mOctree);
		}
	}

public:
	const LLViewerTextureList::dirty_list_t& mTextures;
};

// Called when a texture changes # of channels (causes faces to move to alpha
// pool)
void LLPipeline::dirtyPoolObjectTextures(const LLViewerTextureList::dirty_list_t& textures)
{
	// *TODO: This is inefficient and causes frame spikes; need a better way to
	//		  do this. Most of the time is spent in dirty.traverse.

	for (pool_set_t::iterator iter = mPools.begin(), end = mPools.end();
		 iter != end; ++iter)
	{
		LLDrawPool* poolp = *iter;
		// Only LLDrawPoolTerrain was using dirtyTextures() so I de-virtualized
		// the latter and added the isPoolTerrain() virtual, which is tested
		// instead of isFacePool() with a useless call to an empty virtual
		// dirtyTextures() mesthod for all other face pools... HB
		if (poolp->isPoolTerrain())
		{
			((LLDrawPoolTerrain*)poolp)->dirtyTextures(textures);
		}
	}

	LLOctreeDirtyTexture dirty(textures);
	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* region = *iter;
		for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			dirty.traverse(part->mOctree);
		}
	}
}

LLDrawPool* LLPipeline::findPool(U32 type, LLViewerTexture* tex0)
{
	LLDrawPool* poolp = NULL;
	switch (type)
	{
		case LLDrawPool::POOL_SIMPLE:
			poolp = mSimplePool;
			break;

		case LLDrawPool::POOL_GRASS:
			poolp = mGrassPool;
			break;

		case LLDrawPool::POOL_ALPHA_MASK:
			poolp = mAlphaMaskPool;
			break;

		case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK:
			poolp = mFullbrightAlphaMaskPool;
			break;

		case LLDrawPool::POOL_FULLBRIGHT:
			poolp = mFullbrightPool;
			break;

		case LLDrawPool::POOL_INVISIBLE:
			poolp = mInvisiblePool;
			break;

		case LLDrawPool::POOL_GLOW:
			poolp = mGlowPool;
			break;

		case LLDrawPool::POOL_TREE:
			poolp = get_ptr_in_map(mTreePools, (uintptr_t)tex0);
			break;

		case LLDrawPool::POOL_TERRAIN:
			poolp = get_ptr_in_map(mTerrainPools, (uintptr_t)tex0);
			break;

		case LLDrawPool::POOL_BUMP:
			poolp = mBumpPool;
			break;

		case LLDrawPool::POOL_MATERIALS:
			poolp = mMaterialsPool;
			break;

		case LLDrawPool::POOL_ALPHA:
			poolp = mAlphaPool;
			break;

		case LLDrawPool::POOL_AVATAR:
		case LLDrawPool::POOL_PUPPET:
			break; // Do nothing

		case LLDrawPool::POOL_SKY:
			poolp = mSkyPool;
			break;

		case LLDrawPool::POOL_WATER:
			poolp = mWaterPool;
			break;

		case LLDrawPool::POOL_GROUND:
			poolp = mGroundPool;
			break;

		case LLDrawPool::POOL_WL_SKY:
			poolp = mWLSkyPool;
			break;

		default:
			llerrs << "Invalid Pool Type: " << type << llendl;
	}

	return poolp;
}

LLDrawPool* LLPipeline::getPool(U32 type, LLViewerTexture* tex0)
{
	LLDrawPool* poolp = findPool(type, tex0);
	if (poolp)
	{
		return poolp;
	}
	poolp = LLDrawPool::createPool(type, tex0);
	addPool(poolp);
	return poolp;
}

//static
LLDrawPool* LLPipeline::getPoolFromTE(const LLTextureEntry* te,
									  LLViewerTexture* imagep)
{
	U32 type = getPoolTypeFromTE(te, imagep);
	return gPipeline.getPool(type, imagep);
}

//static
U32 LLPipeline::getPoolTypeFromTE(const LLTextureEntry* te,
								  LLViewerTexture* imagep)
{
	if (!te || !imagep)
	{
		return 0;
	}

	LLMaterial* mat = te->getMaterialParams().get();

	bool color_alpha = te->getColor().mV[3] < 0.999f;
	bool alpha = color_alpha;
	if (!alpha && imagep)
	{
		S32 components = imagep->getComponents();
		alpha = components == 2 ||
				(components == 4 &&
				 imagep->getType() != LLViewerTexture::MEDIA_TEXTURE);
	}
	if (alpha && mat)
	{
		if (mat->getDiffuseAlphaMode() == 1)
		{
			// Material's alpha mode is set to blend. Toss it into the alpha
			// draw pool.
			return LLDrawPool::POOL_ALPHA;
		}
		// 0 -> Material alpha mode set to none, never go to alpha pool
		// 3 -> Material alpha mode set to emissive, never go to alpha pool
		// other -> Material alpha mode set to "mask", go to alpha pool if
		// fullbright, or material alpha mode is set to none, mask or
		// emissive. Toss it into the opaque material draw pool.
		alpha = color_alpha;	// Use the pool matching the te alpha
	}
	if (alpha)
	{
		return LLDrawPool::POOL_ALPHA;
	}

	if ((te->getBumpmap() || te->getShiny()) &&
		(!mat || mat->getNormalID().isNull()))
	{
		return LLDrawPool::POOL_BUMP;
	}

	if (mat)
	{
		return LLDrawPool::POOL_MATERIALS;
	}

	return LLDrawPool::POOL_SIMPLE;
}

void LLPipeline::addPool(LLDrawPool* new_poolp)
{
	mPools.insert(new_poolp);
	addToQuickLookup(new_poolp);
}

void LLPipeline::allocDrawable(LLViewerObject* vobj)
{
	LLDrawable* drawable = new LLDrawable(vobj);
	vobj->mDrawable = drawable;

	// Encompass completely sheared objects by taking the most extreme
	// point possible (<1.0, 1.0, 0.5>)
	drawable->setRadius(LLVector3(1, 1, 0.5f).scaleVec(vobj->getScale()).length());
	if (vobj->isOrphaned())
	{
		drawable->setState(LLDrawable::FORCE_INVISIBLE);
	}
	drawable->updateXform(true);
}

void LLPipeline::unlinkDrawable(LLDrawable* drawable)
{
	LL_FAST_TIMER(FTM_UNLINK);

	// Make sure the drawable does not get deleted before we are done
	LLPointer<LLDrawable> drawablep = drawable;

	// Based on flags, remove the drawable from the queues that it is on.
	if (drawablep->isState(LLDrawable::ON_MOVE_LIST))
	{
		LL_FAST_TIMER(FTM_REMOVE_FROM_MOVE_LIST);
		for (U32 i = 0, count = mMovedList.size(); i < count; ++i)
		{
			if (mMovedList[i] == drawablep)
			{
				if (i < count - 1)
				{
					mMovedList[i] = std::move(mMovedList.back());
				}
				mMovedList.pop_back();
				break;
			}
		}
	}

	LLSpatialGroup* group = drawablep->getSpatialGroup();
	if (group && group->getSpatialPartition())
	{
		LL_FAST_TIMER(FTM_REMOVE_FROM_SPATIAL_PARTITION);
		if (!group->getSpatialPartition()->remove(drawablep, group))
		{
			llwarns << "Could not remove object from spatial group" << llendl;
			llassert(false);
		}
	}

	{
		LL_FAST_TIMER(FTM_REMOVE_FROM_LIGHT_SET);
		mLights.erase(drawablep);

		for (light_set_t::iterator iter = mNearbyLights.begin(),
								   end = mNearbyLights.end();
			 iter != end; ++iter)
		{
			if (iter->drawable == drawablep)
			{
				mNearbyLights.erase(iter);
				break;
			}
		}
	}

	for (U32 i = 0; i < 2; ++i)
	{
		if (mShadowSpotLight[i] == drawablep)
		{
			mShadowSpotLight[i] = NULL;
		}

		if (mTargetShadowSpotLight[i] == drawablep)
		{
			mTargetShadowSpotLight[i] = NULL;
		}
	}
}

U32 LLPipeline::addObject(LLViewerObject* vobj)
{
	if (RenderDelayCreation)
	{
		mCreateQ.emplace_back(vobj);
	}
	else
	{
		createObject(vobj);
	}

	return 1;
}

void LLPipeline::createObjects(F32 max_dtime)
{
	LL_FAST_TIMER(FTM_PIPELINE_CREATE);
#if 1
	LLTimer update_timer;
	while (!mCreateQ.empty() && update_timer.getElapsedTimeF32() < max_dtime)
	{
		LLViewerObject* vobj = mCreateQ.front();
		if (vobj && !vobj->isDead())
		{
			createObject(vobj);
		}
		mCreateQ.pop_front();
	}
#else
	for (LLViewerObject::vobj_list_t::iterator iter = mCreateQ.begin();
		 iter != mCreateQ.end(); ++iter)
	{
		createObject(*iter);
	}
	mCreateQ.clear();
#endif
}

void LLPipeline::createObject(LLViewerObject* vobj)
{
	LLDrawable* drawablep = vobj->mDrawable;
	if (!drawablep)
	{
		drawablep = vobj->createDrawable();
		llassert(drawablep);
	}
	else
	{
		llerrs << "Redundant drawable creation !" << llendl;
	}

	LLViewerObject* parent = (LLViewerObject*)vobj->getParent();
	if (parent)
	{
		// LLPipeline::addObject 1
		vobj->setDrawableParent(parent->mDrawable);
	}
	else
	{
		// LLPipeline::addObject 2
		vobj->setDrawableParent(NULL);
	}

	markRebuild(drawablep, LLDrawable::REBUILD_ALL, true);

	if (RenderAnimateRes && drawablep->getVOVolume())
	{
		// Fun animated res
		drawablep->updateXform(true);
		drawablep->clearState(LLDrawable::MOVE_UNDAMPED);
		drawablep->setScale(LLVector3::zero);
		drawablep->makeActive();
	}
}

void LLPipeline::resetFrameStats()
{
	static LLCachedControl<bool> render_info(gSavedSettings,
											 "DebugShowRenderInfo");
	if (render_info)
	{
		mNeedsDrawStats = true;
	}
	else if (mNeedsDrawStats && !LLFloaterStats::findInstance())
	{
		mNeedsDrawStats = false;
	}
	if (mNeedsDrawStats)
	{
		mTrianglesDrawnStat.addValue((F32)mTrianglesDrawn / 1000.f);
		mTrianglesDrawn = 0;
	}

	if (mOldRenderDebugMask != mRenderDebugMask)
	{
		gObjectList.clearDebugText();
		mOldRenderDebugMask = mRenderDebugMask;
	}
}

// External functions for asynchronous updating
void LLPipeline::updateMoveDampedAsync(LLDrawable* drawablep)
{
	if (sFreezeTime)
	{
		return;
	}
	if (!drawablep)
	{
		llerrs << "Called with NULL drawable" << llendl;
		return;
	}
	if (drawablep->isState(LLDrawable::EARLY_MOVE))
	{
		return;
	}

	// Update drawable now
	drawablep->clearState(LLDrawable::MOVE_UNDAMPED); // Force to DAMPED
	drawablep->updateMove(); // Returns done
	// Flag says we already did an undamped move this frame:
	drawablep->setState(LLDrawable::EARLY_MOVE);
	// Put on move list so that EARLY_MOVE gets cleared
	if (!drawablep->isState(LLDrawable::ON_MOVE_LIST))
	{
		mMovedList.emplace_back(drawablep);
		drawablep->setState(LLDrawable::ON_MOVE_LIST);
	}
}

void LLPipeline::updateMoveNormalAsync(LLDrawable* drawablep)
{
	if (sFreezeTime)
	{
		return;
	}
	if (!drawablep)
	{
		llerrs << "Called with NULL drawable" << llendl;
	}
	if (drawablep->isState(LLDrawable::EARLY_MOVE))
	{
		return;
	}

	// Update drawable now
	drawablep->setState(LLDrawable::MOVE_UNDAMPED); // Force to UNDAMPED
	drawablep->updateMove();
	// Flag says we already did an undamped move this frame:
	drawablep->setState(LLDrawable::EARLY_MOVE);
	// Put on move list so that EARLY_MOVE gets cleared
	if (!drawablep->isState(LLDrawable::ON_MOVE_LIST))
	{
		mMovedList.emplace_back(drawablep);
		drawablep->setState(LLDrawable::ON_MOVE_LIST);
	}
}

void LLPipeline::updateMovedList(LLDrawable::draw_vec_t& moved_list)
{
	LL_TRACY_TIMER(TRC_MOVED_LIST);

	for (U32 i = 0, count = moved_list.size(); i < count; )
	{
		LLDrawable* drawablep = moved_list[i];
		bool done = true;
		if (!drawablep->isDead() &&
			!drawablep->isState(LLDrawable::EARLY_MOVE))
		{
			done = drawablep->updateMove();
		}
		drawablep->clearState(LLDrawable::EARLY_MOVE |
							  LLDrawable::MOVE_UNDAMPED);
		if (done)
		{
			if (drawablep->isRoot() && !drawablep->isState(LLDrawable::ACTIVE))
			{
				drawablep->makeStatic();
			}
			drawablep->clearState(LLDrawable::ON_MOVE_LIST);
			if (drawablep->isState(LLDrawable::ANIMATED_CHILD))
			{
				// Will likely not receive any future world matrix updates;
				// this keeps attachments from getting stuck in space and
				// falling off your avatar
				drawablep->clearState(LLDrawable::ANIMATED_CHILD);
				markRebuild(drawablep, LLDrawable::REBUILD_VOLUME, true);
				LLViewerObject* vobj = drawablep->getVObj().get();
				if (vobj)
				{
					vobj->dirtySpatialGroup(true);
				}
			}
			if (i < --count)
			{
				moved_list[i] = std::move(moved_list.back());
			}
			moved_list.pop_back();
		}
		else
		{
			++i;
		}
	}
}

void LLPipeline::updateMove(bool balance_vo_cache)
{
	LL_FAST_TIMER(FTM_UPDATE_MOVE);

	if (sFreezeTime)
	{
		return;
	}

	for (LLDrawable::draw_set_t::iterator iter = mRetexturedList.begin(),
										  end = mRetexturedList.end();
		 iter != end; ++iter)
	{
		LLDrawable* drawablep = *iter;
		if (drawablep && !drawablep->isDead())
		{
			drawablep->updateTexture();
		}
	}
	mRetexturedList.clear();

	updateMovedList(mMovedList);

	// Balance octrees
	{
		LL_FAST_TIMER(FTM_OCTREE_BALANCE);

		for (LLWorld::region_list_t::const_iterator
				iter = gWorld.getRegionList().begin(),
				end = gWorld.getRegionList().end();
			 iter != end; ++iter)
		{
			LLViewerRegion* region = *iter;
			for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
			{
				LLSpatialPartition* part = region->getSpatialPartition(i);
				// None of the partitions under PARTITION_VO_CACHE can be NULL
				part->mOctree->balance();
			}

			if (balance_vo_cache)
			{
				// Balance the VO Cache tree
				LLVOCachePartition* vo_part = region->getVOCachePartition();
				// PARTITION_VO_CACHE partition cannot be NULL
				vo_part->mOctree->balance();
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// Culling and occlusion testing
/////////////////////////////////////////////////////////////////////////////

//static
F32 LLPipeline::calcPixelArea(LLVector3 center, LLVector3 size,
							  LLCamera& camera)
{
	LLVector3 lookAt = center - camera.getOrigin();
	F32 dist = lookAt.length();

	// Ramp down distance for nearby objects; shrink dist by dist / 16.
	if (dist < 16.f)
	{
		dist /= 16.f;
		dist *= dist;
		dist *= 16.f;
	}
	if (dist <= 0.f)
	{
		dist = F32_MIN;
	}

	// Get area of circle around node
	F32 app_angle = atanf(size.length() / dist);
	F32 radius = app_angle * LLDrawable::sCurPixelAngle;
	return radius * radius * F_PI;
}

//static
F32 LLPipeline::calcPixelArea(const LLVector4a& center, const LLVector4a& size,
							  LLCamera& camera)
{
	LLVector4a origin;
	origin.load3(camera.getOrigin().mV);

	LLVector4a lookAt;
	lookAt.setSub(center, origin);
	F32 dist = lookAt.getLength3().getF32();

	// Ramp down distance for nearby objects.
	// Shrink dist by dist/16.
	if (dist < 16.f)
	{
		dist *= 0.0625f; // 1/16
		dist *= dist;
		dist *= 16.f;
	}
	if (dist <= 0.f)
	{
		dist = F32_MIN;
	}

	// Get area of circle around node
	F32 app_angle = atanf(size.getLength3().getF32() / dist);
	F32 radius = app_angle * LLDrawable::sCurPixelAngle;
	return radius * radius * F_PI;
}

void LLPipeline::grabReferences(LLCullResult& result)
{
	sCull = &result;
}

void LLPipeline::clearReferences()
{
	sCull = NULL;
	mGroupSaveQ1.clear();
}

void check_references(LLSpatialGroup* group, LLDrawable* drawable)
{
	for (LLSpatialGroup::element_iter i = group->getDataBegin();
		 i != group->getDataEnd(); ++i)
	{
		if (drawable == (LLDrawable*)(*i)->getDrawable())
		{
			llerrs << "LLDrawable deleted while actively reference by LLPipeline."
				   << llendl;
		}
	}
}

void check_references(LLDrawable* drawable, LLFace* face)
{
	for (S32 i = 0, count = drawable->getNumFaces(); i < count; ++i)
	{
		if (drawable->getFace(i) == face)
		{
			llerrs << "LLFace deleted while actively referenced by LLPipeline."
				   << llendl;
		}
	}
}

void check_references(LLSpatialGroup* group, LLFace* face)
{
	for (LLSpatialGroup::element_iter i = group->getDataBegin();
		 i != group->getDataEnd(); ++i)
	{
		LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
		if (drawable)
		{
			check_references(drawable, face);
		}
	}
}

void LLPipeline::checkReferences(LLFace* face)
{
#if 0	// Disabled in (for ?) LL's EEP viewer...
	if (sCull)
	{
		for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups();
			 iter != sCull->endVisibleGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, face);
		}

		for (LLCullResult::sg_iterator iter = sCull->beginAlphaGroups();
			 iter != sCull->endAlphaGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, face);
		}

		for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups();
			 iter != sCull->endDrawableGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, face);
		}

		for (LLCullResult::drawable_iterator iter = sCull->beginVisibleList();
			 iter != sCull->endVisibleList(); ++iter)
		{
			LLDrawable* drawable = *iter;
			check_references(drawable, face);
		}
	}
#endif
}

void LLPipeline::checkReferences(LLDrawable* drawable)
{
#if 0	// Disabled in (for ?) LL's EEP viewer...
	if (sCull)
	{
		for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups();
			 iter != sCull->endVisibleGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, drawable);
		}

		for (LLCullResult::sg_iterator iter = sCull->beginAlphaGroups();
			 iter != sCull->endAlphaGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, drawable);
		}

		for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups();
			 iter != sCull->endDrawableGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, drawable);
		}

		for (LLCullResult::drawable_iterator iter = sCull->beginVisibleList();
			 iter != sCull->endVisibleList(); ++iter)
		{
			if (drawable == *iter)
			{
				llerrs << "LLDrawable deleted while actively referenced by LLPipeline."
					   << llendl;
			}
		}
	}
#endif
}

void check_references(LLSpatialGroup* group, LLDrawInfo* draw_info)
{
	for (LLSpatialGroup::draw_map_t::const_iterator
			i = group->mDrawMap.begin(), end = group->mDrawMap.end();
		 i != end; ++i)
	{
		const LLSpatialGroup::drawmap_elem_t& draw_vec = i->second;
		for (LLSpatialGroup::drawmap_elem_t::const_iterator
				j = draw_vec.begin(), end2 = draw_vec.end();
			 j != end2; ++j)
		{
			LLDrawInfo* params = *j;
			if (params == draw_info)
			{
				llerrs << "LLDrawInfo deleted while actively referenced by LLPipeline."
					   << llendl;
			}
		}
	}
}

void LLPipeline::checkReferences(LLDrawInfo* draw_info)
{
#if 0	// Disabled in (for ?) LL's EEP viewer...
	if (sCull)
	{
		for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups();
			 iter != sCull->endVisibleGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, draw_info);
		}

		for (LLCullResult::sg_iterator iter = sCull->beginAlphaGroups();
			 iter != sCull->endAlphaGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, draw_info);
		}

		for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups();
			 iter != sCull->endDrawableGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, draw_info);
		}
	}
#endif
}

void LLPipeline::checkReferences(LLSpatialGroup* group)
{
#if 0	// Disabled in (for ?) LL's EEP viewer...
	if (sCull)
	{
		for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups();
			 iter != sCull->endVisibleGroups(); ++iter)
		{
			if (group == *iter)
			{
				llerrs << "LLSpatialGroup deleted while actively referenced by LLPipeline."
					   << llendl;
			}
		}

		for (LLCullResult::sg_iterator iter = sCull->beginAlphaGroups();
			 iter != sCull->endAlphaGroups(); ++iter)
		{
			if (group == *iter)
			{
				llerrs << "LLSpatialGroup deleted while actively referenced by LLPipeline."
					   << llendl;
			}
		}

		for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups();
			 iter != sCull->endDrawableGroups(); ++iter)
		{
			if (group == *iter)
			{
				llerrs << "LLSpatialGroup deleted while actively referenced by LLPipeline."
					   << llendl;
			}
		}
	}
#endif
}

bool LLPipeline::visibleObjectsInFrustum(LLCamera& camera)
{
	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* region = *iter;

		for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			if (hasRenderType(part->mDrawableType) &&
				part->visibleObjectsInFrustum(camera))
			{
				return true;
			}
		}
	}

	return false;
}

bool LLPipeline::getVisibleExtents(LLCamera& camera, LLVector3& min,
								   LLVector3& max)
{
	constexpr F32 max_val = 65536.f;
	constexpr F32 min_val = -65536.f;
	min.set(max_val, max_val, max_val);
	max.set(min_val, min_val, min_val);

	LLViewerCamera::eCameraID saved_camera_id = LLViewerCamera::sCurCameraID;
	LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

	bool res = true;

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* region = *iter;

		for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			if (hasRenderType(part->mDrawableType) &&
				!part->getVisibleExtents(camera, min, max))
			{
				res = false;
			}
		}
	}

	LLViewerCamera::sCurCameraID = saved_camera_id;

	return res;
}

void LLPipeline::updateCull(LLCamera& camera, LLCullResult& result,
							S32 water_clip, LLPlane* planep,
							bool hud_attachments)
{
	LL_FAST_TIMER(FTM_CULL);

	static LLCachedControl<bool> old_culling(gSavedSettings,
											 "RenderUseOldCulling");
	bool use_new_culling = gUseNewShaders && !old_culling;

	grabReferences(result);

	sCull->clear();

	bool to_texture = sUseOcclusion > 1 && canUseVertexShaders();
	if (to_texture && !use_new_culling)
	{
		to_texture = !hasRenderType(RENDER_TYPE_HUD) && RenderGlow &&
					 LLViewerCamera::sCurCameraID ==
						LLViewerCamera::CAMERA_WORLD;
	}
	if (to_texture)
	{
		if (sRenderDeferred && sUseOcclusion > 1)
		{
			mOcclusionDepth.bindTarget();
		}
		else
		{
			mScreen.bindTarget();
		}
	}

	if (sUseOcclusion > 1)
	{
		gGL.setColorMask(false, false);
	}

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(gGLLastProjection);
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGLLastMatrix = NULL;
	gGL.loadMatrix(gGLLastModelView);

	LLGLDisable blend(GL_BLEND);
	LLGLDisable test(GL_ALPHA_TEST);
	LLGLDisable stencil(use_new_culling ? 0 : GL_STENCIL_TEST);
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	// Setup a clip plane in projection matrix for reflection renders (prevents
	// flickering from occlusion culling)
	LLPlane plane;
	if (!use_new_culling)
	{
		if (planep)
		{
			plane = *planep;
		}
		else if (water_clip < 0)
		{
			// Camera is above water, clip plane points up
			LLViewerRegion* regionp = gAgent.getRegion();
			F32 height = regionp ? regionp->getWaterHeight() : 0.f;
			plane.setVec(LLVector3::z_axis, -height);
		}
		else if (water_clip > 0)
		{
			// Camera is below water, clip plane points down
			LLViewerRegion* regionp = gAgent.getRegion();
			F32 height = regionp ? regionp->getWaterHeight() : 0.f;
			plane.setVec(LLVector3::z_axis_neg, height);
		}
	}
	const LLMatrix4a& modelview = gGLLastModelView;
	const LLMatrix4a& proj = gGLLastProjection;
	bool enable_clip = !use_new_culling && water_clip && sReflectionRender;
	LLGLUserClipPlane clip(plane, modelview, proj, enable_clip);

	LLGLDepthTest depth(GL_TRUE, GL_FALSE);

	bool bound_shader = false;
	if (canUseVertexShaders() && !LLGLSLShader::sCurBoundShader)
	{
		// If no shader is currently bound, use the occlusion shader instead of
		// fixed function if we can (shadow render uses a special shader that
		// clamps to clip planes)
		bound_shader = true;
		gOcclusionCubeProgram.bind();
	}

	if (sUseOcclusion > 1)
	{
		if (mCubeVB.isNull())
		{
			sUseOcclusion = 0;
			llwarns << "No available Cube VB, disabling occlusion" << llendl;
		}
		else
		{
			mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
		}
	}

	if (use_new_culling && !sReflectionRender)
	{
		camera.disableUserClipPlane();
	}

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* region = *iter;

		if (!use_new_culling)
		{
			if (water_clip != 0)
			{
				LLPlane plane(LLVector3(0.f, 0.f, (F32)-water_clip),
										(F32)water_clip *
										region->getWaterHeight());
				camera.setUserClipPlane(plane);
			}
			else
			{
				camera.disableUserClipPlane();
			}
		}

		for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			if (hasRenderType(part->mDrawableType) ||
				(!hud_attachments && i == LLViewerRegion::PARTITION_BRIDGE))
			{
				part->cull(camera);
			}
		}

		// Scan the VO Cache tree
		LLVOCachePartition* vo_part = region->getVOCachePartition();
		if (vo_part)
		{
#if REFLECTION_RENDER_NOT_OCCLUDED
			bool do_occlusion_cull = sUseOcclusion > 1 && water_clip < 0 &&
									 (!use_new_culling || !sReflectionRender);
#else
			bool do_occlusion_cull = sUseOcclusion > 1 && water_clip < 0;
#endif
			vo_part->cull(camera, do_occlusion_cull);
		}
	}

	if (bound_shader)
	{
		gOcclusionCubeProgram.unbind();
	}

	if (!use_new_culling)
	{
		camera.disableUserClipPlane();
	}

	if (hasRenderType(RENDER_TYPE_SKY) &&
	    gSky.mVOSkyp.notNull() && gSky.mVOSkyp->mDrawable.notNull())
	{
		gSky.mVOSkyp->mDrawable->setVisible(camera);
		sCull->pushDrawable(gSky.mVOSkyp->mDrawable);
		gSky.updateCull();
	}

	bool can_use_wl_shaders = canUseWindLightShaders();

	if (!can_use_wl_shaders && !sWaterReflections &&
		hasRenderType(RENDER_TYPE_GROUND) && gSky.mVOGroundp.notNull() &&
		gSky.mVOGroundp->mDrawable.notNull())
	{
		gSky.mVOGroundp->mDrawable->setVisible(camera);
		sCull->pushDrawable(gSky.mVOGroundp->mDrawable);
	}

	if (can_use_wl_shaders &&
		hasRenderType(RENDER_TYPE_WL_SKY) &&
		gSky.mVOWLSkyp.notNull() && gSky.mVOWLSkyp->mDrawable.notNull())
	{
		gSky.mVOWLSkyp->mDrawable->setVisible(camera);
		sCull->pushDrawable(gSky.mVOWLSkyp->mDrawable);
	}

	if (sRenderWaterCull && !sReflectionRender &&
		(hasRenderType(RENDER_TYPE_WATER) ||
		 hasRenderType(RENDER_TYPE_VOIDWATER)))
	{
		gWorld.precullWaterObjects(camera, sCull);
	}

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	if (sUseOcclusion > 1)
	{
		gGL.setColorMask(true, false);
	}

	if (to_texture)
	{
		if (sRenderDeferred && sUseOcclusion > 1)
		{
			mOcclusionDepth.flush();
		}
		else
		{
			mScreen.flush();
		}
	}

	stop_glerror();
}

void LLPipeline::markNotCulled(LLSpatialGroup* group, LLCamera& camera)
{
	if (group->isEmpty())
	{
		return;
	}

	group->setVisible();

	if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD)
	{
		group->updateDistance(camera);
	}

	if (!group->getSpatialPartition()->mRenderByGroup)
	{
		// Render by drawable
		sCull->pushDrawableGroup(group);
	}
	else
	{
		// Render by group
		sCull->pushVisibleGroup(group);
	}

	++mNumVisibleNodes;
}

void LLPipeline::markOccluder(LLSpatialGroup* group)
{
	if (sUseOcclusion > 1 && group &&
		!group->isOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION))
	{
		LLSpatialGroup* parent = group->getParent();
		if (!parent || !parent->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			// Only mark top most occluders as active occlusion
			sCull->pushOcclusionGroup(group);
			group->setOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);

			if (parent &&
				!parent->isOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION) &&
				parent->getElementCount() == 0 && parent->needsUpdate())
			{
				sCull->pushOcclusionGroup(group);
				parent->setOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);
			}
		}
	}
}

void LLPipeline::downsampleDepthBuffer(LLRenderTarget& source,
									   LLRenderTarget& dest,
									   LLRenderTarget* scratch_space)
{
	LLGLSLShader* last_shader = LLGLSLShader::sCurBoundShaderPtr;
	LLGLSLShader* shader = NULL;

	if (scratch_space)
	{
		U32 bits = GL_DEPTH_BUFFER_BIT;
#if LL_NEW_DEPTH_STENCIL
		if (source.hasStencil() && dest.hasStencil())
		{
			bits |= GL_STENCIL_BUFFER_BIT;
		}
#endif
		scratch_space->copyContents(source, 0, 0,
									source.getWidth(), source.getHeight(),
									0, 0, scratch_space->getWidth(),
									scratch_space->getHeight(), bits,
									GL_NEAREST);
	}

	dest.bindTarget();
	dest.clear(GL_DEPTH_BUFFER_BIT);

	LLStrider<LLVector3> vert; 
	if (mDeferredVB.isNull() || !mDeferredVB->getVertexStrider(vert))
	{
		return;
	}
	vert[0].set(-1.f, 1.f, 0.f);
	vert[1].set(-1.f, -3.f, 0.f);
	vert[2].set(3.f, 1.f, 0.f);

	if (source.getUsage() == LLTexUnit::TT_RECT_TEXTURE)
	{
		shader = &gDownsampleDepthRectProgram;
		shader->bind();
		shader->uniform2f(sDelta, 1.f, 1.f);
		shader->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
						  source.getWidth(), source.getHeight());
	}
	else
	{
		shader = &gDownsampleDepthProgram;
		shader->bind();
		shader->uniform2f(sDelta, 1.f / source.getWidth(),
						  1.f / source.getHeight());
		shader->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, 1.f, 1.f);
	}

	gGL.getTexUnit(0)->bind(scratch_space ? scratch_space : &source, true);

	{
		LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);
		mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
		mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	}

	dest.flush();

	if (last_shader)
	{
		last_shader->bind();
	}
	else
	{
		shader->unbind();
	}
}

void LLPipeline::doOcclusion(LLCamera& camera, LLRenderTarget& source,
							 LLRenderTarget& dest,
							 LLRenderTarget* scratch_space)
{
	downsampleDepthBuffer(source, dest, scratch_space);
	dest.bindTarget();
	doOcclusion(camera);
	dest.flush();
}

void LLPipeline::doOcclusion(LLCamera& camera)
{
	if (sUseOcclusion > 1 && !LLSpatialPartition::sTeleportRequested &&
		(sCull->hasOcclusionGroups() ||
		 LLVOCachePartition::sNeedsOcclusionCheck))
	{
		LLVertexBuffer::unbind();

		if (hasRenderDebugMask(RENDER_DEBUG_OCCLUSION))
		{
			gGL.setColorMask(true, false, false, false);
		}
		else
		{
			gGL.setColorMask(false, false);
		}
		LLGLDisable blend(GL_BLEND);
		LLGLDisable test(GL_ALPHA_TEST);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);

		LLGLDisable cull(GL_CULL_FACE);

		bool bind_shader = LLGLSLShader::sCurBoundShader == 0;
		if (bind_shader)
		{
			if (sShadowRender)
			{
				gDeferredShadowCubeProgram.bind();
			}
			else
			{
				gOcclusionCubeProgram.bind();
			}
		}

		if (mCubeVB.isNull())
		{
			sUseOcclusion = 0;
			llwarns << "No available Cube VB, disabling occlusion" << llendl;
		}
		else
		{
			mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);

			for (LLCullResult::sg_iterator iter = sCull->beginOcclusionGroups();
				 iter != sCull->endOcclusionGroups(); ++iter)
			{
				LLSpatialGroup* group = *iter;
				group->doOcclusion(&camera);
				group->clearOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);
			}
		}

		if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD)
		{
			// Apply occlusion culling to object cache tree
			for (LLWorld::region_list_t::const_iterator
					iter = gWorld.getRegionList().begin(),
					end = gWorld.getRegionList().end();
				 iter != end; ++iter)
			{
				LLVOCachePartition* vo_part = (*iter)->getVOCachePartition();
				if (vo_part)
				{
					vo_part->processOccluders(&camera);
				}
			}
		}

		if (bind_shader)
		{
			if (sShadowRender)
			{
				gDeferredShadowCubeProgram.unbind();
			}
			else
			{
				gOcclusionCubeProgram.unbind();
			}
		}

		gGL.setColorMask(true, false);
	}
}

bool LLPipeline::updateDrawableGeom(LLDrawable* drawablep, bool priority)
{
	bool update_complete = drawablep->updateGeometry(priority);
	if (update_complete)
	{
		drawablep->setState(LLDrawable::BUILT);
	}
	return update_complete;
}

void LLPipeline::updateGL()
{
	{
		LL_FAST_TIMER(FTM_UPDATE_GL);
		while (!LLGLUpdate::sGLQ.empty())
		{
			LLGLUpdate* glu = LLGLUpdate::sGLQ.front();
			glu->updateGL();
			glu->mInQ = false;
			LLGLUpdate::sGLQ.pop_front();
		}
	}

	{
		// Seed VBO Pools
		LL_FAST_TIMER(FTM_SEED_VBO_POOLS);
		LLVertexBuffer::seedPools();
	}
}

// Iterates through all groups on the priority build queues and remove all the
// groups that do not correspond to HUD objects.
void LLPipeline::clearRebuildGroups()
{
	mGroupQ1Locked = true;
	for (S32 i = 0, count = mGroupQ1.size(); i < count; )
	{
		LLSpatialGroup* group = mGroupQ1[i].get();
		if (!group || !group->isHUDGroup())
		{
			// Not a HUD object, so clear the build state
			group->clearState(LLSpatialGroup::IN_BUILD_Q1);
			if (i < --count)
			{
				mGroupQ1[i] = std::move(mGroupQ1.back());
			}
			mGroupQ1.pop_back();
		}
		else
		{
			++i;
		}
	}
	mGroupQ1Locked = false;

	mGroupQ2Locked = true;
	for (S32 i = 0, count = mGroupQ2.size(); i < count; )
	{
		LLSpatialGroup* group = mGroupQ2[i].get();
		if (!group || !group->isHUDGroup())
		{
			// Not a HUD object, so clear the build state
			group->clearState(LLSpatialGroup::IN_BUILD_Q2);
			if (i < --count)
			{
				mGroupQ2[i] = std::move(mGroupQ2.back());
			}
			mGroupQ2.pop_back();
		}
		else
		{
			++i;
		}
	}
	mGroupQ2Locked = false;
}

void LLPipeline::clearRebuildDrawables()
{
	// Clear all drawables on the priority build queue,
	for (LLDrawable::draw_list_t::iterator iter = mBuildQ1.begin(),
										   end = mBuildQ1.end();
		 iter != end; ++iter)
	{
		LLDrawable* drawablep = *iter;
		if (drawablep && !drawablep->isDead())
		{
			drawablep->clearState(LLDrawable::IN_REBUILD_Q2);
			drawablep->clearState(LLDrawable::IN_REBUILD_Q1);
		}
	}
	mBuildQ1.clear();

	// Clear drawables on the non-priority build queue
	for (LLDrawable::draw_list_t::iterator iter = mBuildQ2.begin(),
										   end = mBuildQ2.end();
		 iter != end; ++iter)
	{
		LLDrawable* drawablep = *iter;
		if (drawablep && !drawablep->isDead())
		{
			drawablep->clearState(LLDrawable::IN_REBUILD_Q2);
		}
	}
	mBuildQ2.clear();

	// Clear all moving bridges
	for (LLDrawable::draw_vec_t::iterator iter = mMovedBridge.begin(),
										  end = mMovedBridge.end();
		 iter != end; ++iter)
	{
		LLDrawable* drawablep = *iter;
		if (drawablep)
		{
			drawablep->clearState(LLDrawable::EARLY_MOVE |
								  LLDrawable::MOVE_UNDAMPED |
								  LLDrawable::ON_MOVE_LIST |
								  LLDrawable::ANIMATED_CHILD);
		}
	}
	mMovedBridge.clear();

	// Clear all moving drawables
	for (LLDrawable::draw_vec_t::iterator iter = mMovedList.begin(),
										  end = mMovedList.end();
		 iter != end; ++iter)
	{
		LLDrawable* drawablep = *iter;
		if (drawablep)
		{
			drawablep->clearState(LLDrawable::EARLY_MOVE |
								  LLDrawable::MOVE_UNDAMPED |
								  LLDrawable::ON_MOVE_LIST |
								  LLDrawable::ANIMATED_CHILD);
		}
	}
	mMovedList.clear();
}

void LLPipeline::rebuildPriorityGroups()
{
	LL_FAST_TIMER(FTM_REBUILD_PRIORITY_GROUPS);
	LLTimer update_timer;

	{
		LL_FAST_TIMER(FTM_REBUILD_MESH);
		gMeshRepo.notifyLoadedMeshes();
	}

	mGroupQ1Locked = true;
	// Iterate through all drawables on the priority build queue
	for (LLSpatialGroup::sg_vector_t::iterator iter = mGroupQ1.begin();
		 iter != mGroupQ1.end(); ++iter)
	{
		LLSpatialGroup* group = *iter;
		group->rebuildGeom();
		group->clearState(LLSpatialGroup::IN_BUILD_Q1);
	}

	mGroupSaveQ1.clear();
	mGroupSaveQ1.swap(mGroupQ1);	// This clears mGroupQ1
	mGroupQ1Locked = false;
}

void LLPipeline::rebuildGroups()
{
	if (mGroupQ2.empty())
	{
		return;
	}

	LL_FAST_TIMER(FTM_REBUILD_GROUPS);
	mGroupQ2Locked = true;
	// Iterate through some drawables on the non-priority build queue
	S32 size = (S32)mGroupQ2.size();
	S32 min_count = llclamp((S32)((F32)(size * size) / 4096 * 0.25f), 1, size);

	S32 count = 0;

	std::sort(mGroupQ2.begin(), mGroupQ2.end(),
			  LLSpatialGroup::CompareUpdateUrgency());

	LLSpatialGroup::sg_vector_t::iterator last_iter = mGroupQ2.begin();

	for (LLSpatialGroup::sg_vector_t::iterator iter = mGroupQ2.begin(),
											   end = mGroupQ2.end();
		 iter != end && count <= min_count; ++iter)
	{
		LLSpatialGroup* group = *iter;
		last_iter = iter;

		if (!group->isDead())
		{
			group->rebuildGeom();

			if (group->getSpatialPartition()->mRenderByGroup)
			{
				++count;
			}
		}

		group->clearState(LLSpatialGroup::IN_BUILD_Q2);
	}

	mGroupQ2.erase(mGroupQ2.begin(), ++last_iter);

	mGroupQ2Locked = false;

	updateMovedList(mMovedBridge);
}

void LLPipeline::updateGeom(F32 max_dtime)
{
	LLTimer update_timer;

	LL_FAST_TIMER(FTM_GEO_UPDATE);

	if (sDelayedVBOEnable > 0)
	{
		if (--sDelayedVBOEnable <= 0)
		{
			resetVertexBuffers();
			LLVertexBuffer::sEnableVBOs = true;
		}
	}

	// Notify various object types to reset internal cost metrics, etc.
	// for now, only LLVOVolume does this to throttle LOD changes
	LLVOVolume::preUpdateGeom();

	LLPointer<LLDrawable> drawablep;
	// Iterate through all drawables on the priority build queue,
	for (LLDrawable::draw_list_t::iterator iter = mBuildQ1.begin(),
										   end = mBuildQ1.end();
		 iter != end; )
	{
		LLDrawable::draw_list_t::iterator curiter = iter++;
		drawablep = *curiter;
		bool remove = drawablep.isNull() || drawablep->isDead();
		if (!remove)
		{
			if (drawablep->isState(LLDrawable::IN_REBUILD_Q2))
			{
				drawablep->clearState(LLDrawable::IN_REBUILD_Q2);
				LLDrawable::draw_list_t::iterator find =
					std::find(mBuildQ2.begin(), mBuildQ2.end(), drawablep);
				if (find != mBuildQ2.end())
				{
					mBuildQ2.erase(find);
				}
			}

			remove = updateDrawableGeom(drawablep, true);
		}
		if (remove)
		{
			if (drawablep)
			{
				drawablep->clearState(LLDrawable::IN_REBUILD_Q1);
			}
			mBuildQ1.erase(curiter);
		}
	}

	// Iterate through some drawables on the non-priority build queue
	S32 min_count = 16;
	F32 size = (F32)mBuildQ2.size();
	if (size > 1024.f)
	{
		min_count = (S32)llclamp(size * size / 4096.f, 16.f, size);
	}

	S32 count = 0;

	max_dtime = llmax(update_timer.getElapsedTimeF32() + 0.001f, max_dtime);
	LLSpatialGroup* last_group = NULL;
	LLSpatialBridge* last_bridge = NULL;

	for (LLDrawable::draw_list_t::iterator iter = mBuildQ2.begin(),
										   end = mBuildQ2.end();
		 iter != end; )
	{
		LLDrawable::draw_list_t::iterator curiter = iter++;
		drawablep = *curiter;
		bool remove = drawablep.isNull() || drawablep->isDead();
		if (!remove)
		{
			LLSpatialBridge* bridge =
				drawablep->isRoot() ? drawablep->getSpatialBridge()
									: drawablep->getParent()->getSpatialBridge();

			if (drawablep->getSpatialGroup() != last_group &&
				(!last_bridge || bridge != last_bridge) && count > min_count &&
				update_timer.getElapsedTimeF32() >= max_dtime)
			{
				break;
			}
			// Make sure updates do not stop in the middle of a spatial group
			// to avoid thrashing (objects are enqueued by group)
			last_group = drawablep->getSpatialGroup();
			last_bridge = bridge;
			++count;

			remove = updateDrawableGeom(drawablep, false);
		}
		if (remove)
		{
			if (drawablep)
			{
				drawablep->clearState(LLDrawable::IN_REBUILD_Q2);
			}
			mBuildQ2.erase(curiter);
		}
	}

	updateMovedList(mMovedBridge);
}

void LLPipeline::markVisible(LLDrawable* drawablep, LLCamera& camera)
{
	if (drawablep && !drawablep->isDead())
	{
		if (drawablep->isSpatialBridge())
		{
			const LLDrawable* root = ((LLSpatialBridge*)drawablep)->mDrawable;
			llassert(root); // Trying to catch a bad assumption
			if (root && root->getVObj()->isAttachment())
			{
				LLDrawable* rootparent = root->getParent();
				if (rootparent) // this IS sometimes NULL
				{
					LLViewerObject* vobj = rootparent->getVObj();
					llassert(vobj);	// trying to catch a bad assumption
					if (vobj)		// this test may not be needed, see above
					{
						LLVOAvatar* av = vobj->asAvatar();
						if (av &&
							(av->isImpostor() || av->isInMuteList() ||
//MK
							 (gRLenabled && av->isRLVMuted()) ||
//mk
							 (av->getVisualMuteSettings() == LLVOAvatar::AV_DO_NOT_RENDER &&
							  !av->needsImpostorUpdate())))
						{
							return;
						}
					}
				}
			}
			sCull->pushBridge((LLSpatialBridge*)drawablep);
		}
		else
		{
			sCull->pushDrawable(drawablep);
		}

		drawablep->setVisible(camera);
	}
}

void LLPipeline::markMoved(LLDrawable* drawablep, bool damped_motion)
{
	if (!drawablep)
	{
		llwarns_once << "Sending NULL drawable to moved list !" << llendl;
		return;
	}

	if (drawablep->isDead())
	{
		llwarns << "Marking NULL or dead drawable moved !" << llendl;
		return;
	}

	if (drawablep->getParent())
	{
		// Ensure that parent drawables are moved first
		markMoved(drawablep->getParent(), damped_motion);
	}

	if (!drawablep->isState(LLDrawable::ON_MOVE_LIST))
	{
		if (drawablep->isSpatialBridge())
		{
			mMovedBridge.emplace_back(drawablep);
		}
		else
		{
			mMovedList.emplace_back(drawablep);
		}
		drawablep->setState(LLDrawable::ON_MOVE_LIST);
	}
	if (!damped_motion)
	{
		// UNDAMPED trumps DAMPED
		drawablep->setState(LLDrawable::MOVE_UNDAMPED);
	}
	else if (drawablep->isState(LLDrawable::MOVE_UNDAMPED))
	{
		drawablep->clearState(LLDrawable::MOVE_UNDAMPED);
	}
}

void LLPipeline::markShift(LLDrawable* drawablep)
{
	if (!drawablep || drawablep->isDead())
	{
		return;
	}

	if (!drawablep->isState(LLDrawable::ON_SHIFT_LIST))
	{
		LLViewerObject* vobj = drawablep->getVObj().get();
		if (vobj)
		{
			vobj->setChanged(LLXform::SHIFTED | LLXform::SILHOUETTE);
		}
		if (drawablep->getParent())
		{
			markShift(drawablep->getParent());
		}
		mShiftList.emplace_back(drawablep);
		drawablep->setState(LLDrawable::ON_SHIFT_LIST);
	}
}

void LLPipeline::shiftObjects(const LLVector3& offset)
{
	glClear(GL_DEPTH_BUFFER_BIT);
	gDepthDirty = true;

	LLVector4a offseta;
	offseta.load3(offset.mV);

	{
		LL_FAST_TIMER(FTM_SHIFT_DRAWABLE);
		for (U32 i = 0, count = mShiftList.size(); i < count; ++i)
		{
			LLDrawable* drawablep = mShiftList[i].get();
			if (drawablep && !drawablep->isDead())
			{
				drawablep->shiftPos(offseta);
				drawablep->clearState(LLDrawable::ON_SHIFT_LIST);
			}
		}
		mShiftList.clear();
	}

	{
		LL_FAST_TIMER(FTM_SHIFT_OCTREE);
		for (LLWorld::region_list_t::const_iterator
				iter = gWorld.getRegionList().begin(),
				end = gWorld.getRegionList().end();
			 iter != end; ++iter)
		{
			LLViewerRegion* region = *iter;
			for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
			{
				LLSpatialPartition* part = region->getSpatialPartition(i);
				// None of the partitions under PARTITION_VO_CACHE can be NULL
				part->shift(offseta);
			}
		}
	}

	{
		LL_FAST_TIMER(FTM_SHIFT_HUD);
		LLHUDText::shiftAll(offset);
	}

	display_update_camera();
}

void LLPipeline::markTextured(LLDrawable* drawablep)
{
	if (drawablep && !drawablep->isDead())
	{
		mRetexturedList.emplace(drawablep);
	}
}

void LLPipeline::markGLRebuild(LLGLUpdate* glu)
{
	if (glu && !glu->mInQ)
	{
		LLGLUpdate::sGLQ.push_back(glu);
		glu->mInQ = true;
	}
}

void LLPipeline::markPartitionMove(LLDrawable* drawablep)
{
	if (!drawablep->isState(LLDrawable::PARTITION_MOVE) &&
		!drawablep->getPositionGroup().equals3(LLVector4a::getZero()))
	{
		drawablep->setState(LLDrawable::PARTITION_MOVE);
		mPartitionQ.emplace_back(drawablep);
	}
}

void LLPipeline::processPartitionQ()
{
	LL_FAST_TIMER(FTM_PROCESS_PARTITIONQ);
	for (U32 i = 0, count = mPartitionQ.size(); i < count; ++i)
	{
		LLDrawable* drawablep = mPartitionQ[i].get();
		if (!drawablep) continue;	// Paranoia

		if (!drawablep->isDead())
		{
			drawablep->updateBinRadius();
			drawablep->movePartition();
		}
		drawablep->clearState(LLDrawable::PARTITION_MOVE);
	}
	mPartitionQ.clear();
}

void LLPipeline::markMeshDirty(LLSpatialGroup* group)
{
	mMeshDirtyGroup.emplace_back(group);
}

void LLPipeline::markRebuild(LLSpatialGroup* group, bool priority)
{
	if (group && !group->isDead() && group->getSpatialPartition())
	{
		if (group->getSpatialPartition()->mPartitionType ==
				LLViewerRegion::PARTITION_HUD)
		{
			priority = true;
		}

		if (priority)
		{
			if (!group->hasState(LLSpatialGroup::IN_BUILD_Q1))
			{
				llassert_always(!mGroupQ1Locked);

				mGroupQ1.emplace_back(group);
				group->setState(LLSpatialGroup::IN_BUILD_Q1);

				if (group->hasState(LLSpatialGroup::IN_BUILD_Q2))
				{
					for (S32 i = 0, count = mGroupQ2.size(); i < count; ++i)
					{
						if (mGroupQ2[i].get() == group)
						{
							if (i < count - 1)
							{
								mGroupQ2[i] = std::move(mGroupQ2.back());
							}
							mGroupQ2.pop_back();
							break;
						}
					}
					group->clearState(LLSpatialGroup::IN_BUILD_Q2);
				}
			}
		}
		else if (!group->hasState(LLSpatialGroup::IN_BUILD_Q2 |
								  LLSpatialGroup::IN_BUILD_Q1))
		{
			llassert_always(!mGroupQ2Locked);
			mGroupQ2.emplace_back(group);
			group->setState(LLSpatialGroup::IN_BUILD_Q2);
		}
	}
}

void LLPipeline::markRebuild(LLDrawable* drawablep,
							 LLDrawable::EDrawableFlags flag,
							 bool priority)
{
	if (drawablep && !drawablep->isDead())
	{
		if (!drawablep->isState(LLDrawable::BUILT))
		{
			priority = true;
		}
		if (priority)
		{
			if (!drawablep->isState(LLDrawable::IN_REBUILD_Q1))
			{
				mBuildQ1.emplace_back(drawablep);
				// Mark drawable as being in priority queue
				drawablep->setState(LLDrawable::IN_REBUILD_Q1);
			}
		}
		else if (!drawablep->isState(LLDrawable::IN_REBUILD_Q2))
		{
			mBuildQ2.emplace_back(drawablep);
			// Need flag here because it is just a list
			drawablep->setState(LLDrawable::IN_REBUILD_Q2);
		}
		if (flag & (LLDrawable::REBUILD_VOLUME | LLDrawable::REBUILD_POSITION))
		{
			LLViewerObject* vobj = drawablep->getVObj().get();
			if (vobj)
			{
				vobj->setChanged(LLXform::SILHOUETTE);
			}
		}
		drawablep->setState(flag);
	}
}

void LLPipeline::stateSort(LLCamera& camera, LLCullResult& result)
{
	if (hasAnyRenderType(RENDER_TYPE_AVATAR,
						 RENDER_TYPE_PUPPET,
						 RENDER_TYPE_GROUND,
						 RENDER_TYPE_TERRAIN,
						 RENDER_TYPE_TREE,
						 RENDER_TYPE_SKY,
						 RENDER_TYPE_VOIDWATER,
						 RENDER_TYPE_WATER,
						 END_RENDER_TYPES))
	{
		// Clear faces from face pools
		LL_FAST_TIMER(FTM_RESET_DRAWORDER);
		resetDrawOrders();
	}

	LL_FAST_TIMER(FTM_STATESORT);

#if 0
	LLVertexBuffer::unbind();
#endif

	grabReferences(result);

	for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups();
		 iter != sCull->endDrawableGroups(); ++iter)
	{
		LLSpatialGroup* group = *iter;
		group->checkOcclusion();
		if (sUseOcclusion > 1 &&
			group->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			markOccluder(group);
		}
		else
		{
			group->setVisible();
			for (LLSpatialGroup::element_iter i = group->getDataBegin();
				 i != group->getDataEnd(); ++i)
			{
				markVisible((LLDrawable*)(*i)->getDrawable(), camera);
			}

			if (!sDelayVBUpdate)
			{
				// Rebuild mesh as soon as we know it is visible
				group->rebuildMesh();
			}
		}
	}

	if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD)
	{
		bool fov_changed = gViewerCamera.isDefaultFOVChanged();
		LLSpatialGroup* last_group = NULL;
		for (LLCullResult::bridge_iterator i = sCull->beginVisibleBridge();
			 i != sCull->endVisibleBridge(); ++i)
		{
			LLCullResult::bridge_iterator cur_iter = i;
			LLSpatialBridge* bridge = *cur_iter;
			if (!bridge) continue;	// Paranoia

			LLSpatialGroup* group = bridge->getSpatialGroup();

			if (last_group == NULL)
			{
				last_group = group;
			}

			if (!bridge->isDead() && group &&
				!group->isOcclusionState(LLSpatialGroup::OCCLUDED))
			{
				stateSort(bridge, camera, fov_changed);
			}

			if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
				last_group != group && last_group->changeLOD())
			{
				last_group->mLastUpdateDistance = last_group->mDistance;
			}

			last_group = group;
		}

		if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
			last_group && last_group->changeLOD())
		{
			last_group->mLastUpdateDistance = last_group->mDistance;
		}
	}

	for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups();
		 iter != sCull->endVisibleGroups(); ++iter)
	{
		LLSpatialGroup* group = *iter;
		if (!group) continue;	// Paranoia

		group->checkOcclusion();
		if (sUseOcclusion > 1 &&
			group->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			markOccluder(group);
		}
		else
		{
			group->setVisible();
			stateSort(group, camera);

			if (!sDelayVBUpdate)
			{
				// Rebuild mesh as soon as we know it is visible
				group->rebuildMesh();
			}
		}
	}

	{
		LL_FAST_TIMER(FTM_STATESORT_DRAWABLE);
		for (LLCullResult::drawable_iterator iter = sCull->beginVisibleList();
			 iter != sCull->endVisibleList(); ++iter)
		{
			LLDrawable* drawablep = *iter;
			if (drawablep && !drawablep->isDead())
			{
				stateSort(drawablep, camera);
			}
		}
	}

	postSort(camera);
}

void LLPipeline::stateSort(LLSpatialGroup* group, LLCamera& camera)
{
	if (group->changeLOD())
	{
		for (LLSpatialGroup::element_iter i = group->getDataBegin();
			 i != group->getDataEnd(); ++i)
		{
			stateSort((LLDrawable*)(*i)->getDrawable(), camera);
		}

		if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD)
		{
			// Avoid redundant stateSort calls
			group->mLastUpdateDistance = group->mDistance;
		}
	}
}

void LLPipeline::stateSort(LLSpatialBridge* bridge, LLCamera& camera,
						   bool fov_changed)
{
	if (fov_changed || bridge->getSpatialGroup()->changeLOD())
	{
		// false = do not force update
		bridge->updateDistance(camera, false);
	}
}

void LLPipeline::stateSort(LLDrawable* drawablep, LLCamera& camera)
{
	if (!drawablep || drawablep->isDead() ||
		!hasRenderType(drawablep->getRenderType()))
	{
		return;
	}

	// SL-11353: ignore our own geometry when rendering spotlight shadowmaps...
	if (sRenderSpotLight && drawablep == sRenderSpotLight)
	{
		return;
	}

	LLViewerObject* vobj = drawablep->getVObj().get();
	if (gSelectMgr.mHideSelectedObjects && vobj && vobj->isSelected() &&
//MK
		(!gRLenabled || !gRLInterface.mContainsEdit))
//mk
	{
		return;
	}

	if (drawablep->isAvatar())
	{
		// Do not draw avatars beyond render distance or if we do not have a
		// spatial group.
		if (drawablep->getSpatialGroup() == NULL ||
			drawablep->getSpatialGroup()->mDistance > LLVOAvatar::sRenderDistance)
		{
			return;
		}

		LLVOAvatar* avatarp = (LLVOAvatar*)vobj;
		if (avatarp && !avatarp->isVisible())
		{
			return;
		}
	}

	if (hasRenderType(drawablep->mRenderType) &&
		!drawablep->isState(LLDrawable::INVISIBLE |
							LLDrawable::FORCE_INVISIBLE))
	{
		drawablep->setVisible(camera, NULL, false);
	}

	if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD)
	{
#if 0	// isVisible() check here is redundant, if it was not visible, it would
		// not be here
		if (drawablep->isVisible())
#endif
		{
			if (!drawablep->isActive())
			{
				// false = do not force update = false;
				drawablep->updateDistance(camera, false);
			}
			else if (drawablep->isAvatar())
			{
				// Calls vobj->updateLOD() which calls
				// LLVOAvatar::updateVisibility()
				drawablep->updateDistance(camera, false);
			}
		}
	}

	if (!drawablep->getVOVolume())
	{
		for (LLDrawable::face_list_t::iterator
				iter = drawablep->mFaces.begin(),
				end = drawablep->mFaces.end();
			 iter != end; ++iter)
		{
			LLFace* facep = *iter;

			if (facep->hasGeometry())
			{
				if (facep->getPool())
				{
					facep->getPool()->enqueue(facep);
				}
				else
				{
					break;
				}
			}
		}
	}
}

void forAllDrawables(LLCullResult::sg_iterator begin,
					 LLCullResult::sg_iterator end,
					 void (*func)(LLDrawable*))
{
	for (LLCullResult::sg_iterator i = begin; i != end; ++i)
	{
		for (LLSpatialGroup::element_iter j = (*i)->getDataBegin(),
										  end2 = (*i)->getDataEnd();
			 j != end2; ++j)
		{
			if ((*j)->hasDrawable())
			{
				func((LLDrawable*)(*j)->getDrawable());
			}
		}
	}
}

void LLPipeline::forAllVisibleDrawables(void (*func)(LLDrawable*))
{
	forAllDrawables(sCull->beginDrawableGroups(),
					sCull->endDrawableGroups(), func);
	forAllDrawables(sCull->beginVisibleGroups(),
					sCull->endVisibleGroups(), func);
}

//static
U32 LLPipeline::highlightable(const LLViewerObject* vobj)
{
	if (!vobj || vobj->isAvatar()) return 0;
	if (sRenderByOwner == 1 && !vobj->permYouOwner()) return 0;
	if (sRenderByOwner == 2 && vobj->permYouOwner()) return 0;
	LLViewerObject* parentp = (LLViewerObject*)vobj->getParent();
	if (!parentp) return 1;
	if (!sRenderAttachments) return 0;

	// Attachments can be highlighted but are not marked with beacons since
	// it would mark the avatar itself... But, we highlight all the
	// primitives of the attachments instead of just the root primitive
	// (which could be buried into the avatar or be too small to be
	// visible).
	if (parentp->isAvatar())
	{
		return 2;
	}
	LLViewerObject* rootp = (LLViewerObject*)vobj->getRoot();
	if (rootp && rootp->isAvatar())
	{
		return 2;
	}

	return 0;
}

// Function for creating scripted beacons
void renderScriptedBeacons(LLDrawable* drawablep)
{
	LLViewerObject* vobj = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(vobj);
	if (type != 0 && vobj->flagScripted())
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "",
									   LLColor4(1.f, 0.f, 0.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderScriptedTouchBeacons(LLDrawable* drawablep)
{
	LLViewerObject* vobj = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(vobj);
	if (type != 0 && vobj->flagScripted() && vobj->flagHandleTouch())
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "",
									   LLColor4(1.f, 0.f, 0.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderPhysicalBeacons(LLDrawable* drawablep)
{
	LLViewerObject* vobj = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(vobj);
	if (type != 0 && vobj->flagUsePhysics())
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "",
									   LLColor4(0.f, 1.f, 0.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderPermanentBeacons(LLDrawable* drawablep)
{
	LLViewerObject* vobj = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(vobj);
	if (type != 0 && vobj->flagObjectPermanent())
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "",
									   LLColor4(0.f, 1.f, 1.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderCharacterBeacons(LLDrawable* drawablep)
{
	LLViewerObject* vobj = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(vobj);
	if (type != 0 && vobj->flagCharacter())
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "",
									   LLColor4(0.5f, 0.5f, 0.5f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderSoundBeacons(LLDrawable* drawablep)
{
	// Look for attachments, objects, etc.
	LLViewerObject* vobj = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(vobj);
	if (type != 0 && vobj->isAudioSource())
	{
		if ((LLPipeline::sRenderBeacons ||
			 !LLPipeline::sRenderInvisibleSoundBeacons) && type != 2)
		{
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "",
									   LLColor4(1.f, 1.f, 0.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderParticleBeacons(LLDrawable* drawablep)
{
	// Look for attachments, objects, etc.
	LLViewerObject* vobj = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(vobj);
	if (type != 0 && vobj->isParticleSource())
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "",
									   LLColor4(0.5f, 0.5f, 1.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderMOAPBeacons(LLDrawable* drawablep)
{
	LLViewerObject* vobj = drawablep->getVObj().get();
	if (!vobj || vobj->isAvatar()) return;

	U32 type = LLPipeline::highlightable(vobj);
	if (type == 0) return;

	bool beacon = false;
	U8 tecount = vobj->getNumTEs();
	for (S32 x = 0; x < tecount; ++x)
	{
		LLTextureEntry* tep = vobj->getTE(x);
		if (tep && tep->hasMedia())
		{
			beacon = true;
			break;
		}
	}
	if (beacon)
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "",
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void LLPipeline::postSort(LLCamera& camera)
{
	LL_FAST_TIMER(FTM_STATESORT_POSTSORT);

	// Rebuild drawable geometry
	for (LLCullResult::sg_iterator i = sCull->beginDrawableGroups();
		 i != sCull->endDrawableGroups(); ++i)
	{
		LLSpatialGroup* group = *i;
		if (group->isDead())
		{
			continue;
		}
		if (!sUseOcclusion ||
			!group->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			group->rebuildGeom();
		}
	}

	// Rebuild groups
	sCull->assertDrawMapsEmpty();

	rebuildPriorityGroups();

	// Build render map
	bool has_type_pass_alpha = hasRenderType(RENDER_TYPE_PASS_ALPHA);
	bool has_alpha_type = hasRenderType(LLDrawPool::POOL_ALPHA);
	bool is_world_camera =
		LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD;
	LLVector4a bounds;
	for (LLCullResult::sg_iterator i = sCull->beginVisibleGroups();
		 i != sCull->endVisibleGroups(); ++i)
	{
		LLSpatialGroup* group = *i;
		if (group->isDead())
		{
			continue;
		}
		if (sUseOcclusion &&
			group->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			continue;
		}

		if (RenderAutoHideGeometryMemoryLimit > 0 &&
			group->mGeometryBytes >
				RenderAutoHideGeometryMemoryLimit * 1048576)
		{
			continue;
		}

		if (RenderAutoHideSurfaceAreaLimit > 0.f &&
			group->mSurfaceArea >
				RenderAutoHideSurfaceAreaLimit * group->mObjectBoxSize)
		{
			continue;
		}

		if (group->hasState(LLSpatialGroup::NEW_DRAWINFO) &&
			group->hasState(LLSpatialGroup::GEOM_DIRTY))
		{
			// No way this group is going to be drawable without a rebuild
			group->rebuildGeom();
		}

		for (LLSpatialGroup::draw_map_t::iterator j = group->mDrawMap.begin(),
												  end = group->mDrawMap.end();
			 j != end; ++j)
		{
			LLSpatialGroup::drawmap_elem_t& src_vec = j->second;
			if (!hasRenderType(j->first))
			{
				continue;
			}

			for (LLSpatialGroup::drawmap_elem_t::iterator k = src_vec.begin(),
														  kend = src_vec.end();
				 k != kend; ++k)
			{
				sCull->pushDrawInfo(j->first, *k);
			}
		}

		if (has_type_pass_alpha &&
			group->mDrawMap.count(LLRenderPass::PASS_ALPHA))
		{
			// Store alpha groups for sorting
			if (is_world_camera)
			{
				LLSpatialBridge* bridge =
					group->getSpatialPartition()->asBridge();
				if (bridge)
				{
					LLCamera trans_camera =
						bridge->transformCamera(camera);
					group->updateDistance(trans_camera);
				}
				else
				{
					group->updateDistance(camera);
				}
			}
			if (has_alpha_type)
			{
				sCull->pushAlphaGroup(group);
			}
		}
	}

	// Flush particle VB
	if (LLVOPartGroup::sVB.notNull())
	{
		LLVOPartGroup::sVB->flush();
	}

	// Pack vertex buffers for groups that chose to delay their updates
	for (U32 i = 0, count = mMeshDirtyGroup.size(); i < count; ++i)
	{
		LLSpatialGroup* group = mMeshDirtyGroup[i].get();
		if (group)
		{
			group->rebuildMesh();
		}
	}
	mMeshDirtyGroup.clear();

	if (!sShadowRender)
	{
		std::sort(sCull->beginAlphaGroups(), sCull->endAlphaGroups(),
				  LLSpatialGroup::CompareDepthGreater());
	}

	// This is the position for the sounds list floater beacon:
	LLVector3d selected_pos = HBFloaterSoundsList::selectedLocation();

	// Only render if the flag is set. The flag is only set if we are in edit
	// mode or the toggle is set in the menus.
	static LLCachedControl<bool> beacons_always_on(gSavedSettings,
												   "BeaconAlwaysOn");
	if ((sRenderBeaconsFloaterOpen || beacons_always_on) &&
//MK
		!(gRLenabled &&
		  (gRLInterface.mContainsEdit || gRLInterface.mVisionRestricted)) &&
//mk
		!sShadowRender)
	{
		if (sRenderScriptedTouchBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderScriptedTouchBeacons);
		}
		else if (sRenderScriptedBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderScriptedBeacons);
		}

		if (sRenderPhysicalBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderPhysicalBeacons);
		}

		if (sRenderPermanentBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderPermanentBeacons);
		}

		if (sRenderCharacterBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderCharacterBeacons);
		}

		if (sRenderSoundBeacons && gAudiop)
		{
			if (sRenderInvisibleSoundBeacons && sRenderBeacons)
			{
				static const LLColor4 semi_yellow(1.f, 1.f, 0.f, 0.5f);
				static const LLColor4 semi_white(1.f, 1.f, 0.f, 0.5f);
				// Walk all sound sources and render out beacons for them.
				// Note, this is not done in the ForAllVisibleDrawables
				// function, because some are not visible.
				for (LLAudioEngine::source_map_t::const_iterator
						iter = gAudiop->mAllSources.begin(),
						end = gAudiop->mAllSources.end();
					 iter != end; ++iter)
				{
					const LLAudioSource* sourcep = iter->second;
					if (!sourcep) continue;	// Paranoia

					// Verify source owner and match against renderbyowner
					const LLUUID& owner_id = sourcep->getOwnerID();
					if ((sRenderByOwner == 1 && owner_id != gAgentID) ||
						(sRenderByOwner == 2 && owner_id == gAgentID))
					{
						continue;
					}

					LLVector3d pos_global = sourcep->getPositionGlobal();
					if (selected_pos.isExactlyZero() ||
						pos_global != selected_pos)
					{
						LLVector3 pos =
							gAgent.getPosAgentFromGlobal(pos_global);
						gObjectList.addDebugBeacon(pos, "", semi_yellow,
												   semi_white,
												   DebugBeaconLineWidth);
					}
				}
			}
			// Now deal with highlights for all those seeable sound sources
			forAllVisibleDrawables(renderSoundBeacons);
		}

		if (sRenderParticleBeacons)
		{
			forAllVisibleDrawables(renderParticleBeacons);
		}

		if (sRenderMOAPBeacons)
		{
			forAllVisibleDrawables(renderMOAPBeacons);
		}
	}

	// Render the sound beacon for the sounds list floater, if needed.
	if (!selected_pos.isExactlyZero())
	{
		gObjectList.addDebugBeacon(gAgent.getPosAgentFromGlobal(selected_pos),
								   "",
								   // Oranger yellow than sound normal beacons
								   LLColor4(1.f, 0.8f, 0.f, 0.5f),
								   LLColor4(1.f, 1.f, 1.f, 0.5f),
								   DebugBeaconLineWidth);
	}

	// If managing your telehub, draw beacons at telehub and currently selected
	// spawnpoint.
	if (LLFloaterTelehub::renderBeacons())
	{
		LLFloaterTelehub::addBeacons();
	}

	if (!sShadowRender)
	{
		mSelectedFaces.clear();

		sRenderHighlightTextureChannel =
			LLPanelFace::getTextureChannelToEdit();

		// Draw face highlights for selected faces.
		if (gSelectMgr.getTEMode())
		{
			struct f final : public LLSelectedTEFunctor
			{
				bool apply(LLViewerObject* object, S32 te) override
				{
					LLDrawable* drawable = object->mDrawable;
					if (drawable)
					{
						LLFace* facep = drawable->getFace(te);
						if (facep)
						{
							gPipeline.mSelectedFaces.push_back(facep);
						}
					}
					return true;
				}
			} func;
			gSelectMgr.getSelection()->applyToTEs(&func);
		}
	}
}

void render_hud_elements()
{
	gPipeline.disableLights();

	LLGLDisable fog(GL_FOG);
	LLGLSUIDefault gls_ui;

	LLGLEnable stencil(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS, 255, 0xFFFFFFFF);
	glStencilMask(0xFFFFFFFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	gGL.color4f(1.f, 1.f, 1.f, 1.f);

	gUIProgram.bind();

	LLGLDepthTest depth(GL_TRUE, GL_FALSE);

	if (!LLPipeline::sReflectionRender &&
		gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
	{
		LLGLEnable multisample(LLPipeline::RenderFSAASamples > 0 ? GL_MULTISAMPLE_ARB
																 : 0);
		// For HUD version in render_ui_3d()
		gViewerWindowp->renderSelections(false, false, false);

		// Draw the tracking overlays
		gTracker.render3D();

//MK
		if (!gRLenabled || !gRLInterface.mVisionRestricted)
//mk
		{
			// Show the property lines
			gWorld.renderPropertyLines();
			gViewerParcelMgr.render();
			gViewerParcelMgr.renderParcelCollision();
		}

		// Render name tags and hover texts.
		LLHUDObject::renderAll();
	}
	else if (gForceRenderLandFence)
	{
		// This is only set when not rendering the UI, for parcel snapshots
		gViewerParcelMgr.render();
	}
	else if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
	{
		LLHUDText::renderAllHUD();
	}

	gUIProgram.unbind();

	gGL.flush();
}

void LLPipeline::renderHighlights()
{
	S32 selected_count = mSelectedFaces.size();
	S32 highlighted_count = mHighlightFaces.size();

	if (!hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_SELECTED) ||
		(!selected_count && !highlighted_count))
	{
		// Nothing to draw
		return;
	}

	LLGLSPipelineAlpha gls_pipeline_alpha;
	LLGLEnable color_mat(GL_COLOR_MATERIAL);
	disableLights();

	bool shader_interface =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_INTERFACE) > 0;

	std::vector<LLFace*>::iterator sel_start, sel_end;
	if (selected_count)
	{
		sel_start = mSelectedFaces.begin();
		sel_end = mSelectedFaces.end();
	}

	if (highlighted_count)
	{
		// Beacons face highlights
		if (shader_interface)
		{
			gHighlightProgram.bind();
		}
		for (S32 i = 0; i < highlighted_count; ++i)
		{
			LLFace* facep = mHighlightFaces[i];
			if (facep && !facep->getDrawable()->isDead())
			{
				if (!selected_count ||
					// Exclude selected faces from beacon highlights
					std::find(sel_start, sel_end, facep) == sel_end)
				{
					facep->renderSelected(LLViewerTexture::sNullImagep,
										  LLColor4(1.f, 0.f, 0.f, .5f));
				}
			}
			else if (gDebugGL)
			{
				llwarns << "Bad face in beacons highlights" << llendl;
			}
		}
		if (shader_interface)
		{
			gHighlightProgram.unbind();
		}
		mHighlightFaces.clear();
	}

	if (selected_count)
	{
		// Selection image initialization if needed
		if (!mFaceSelectImagep)
		{
			mFaceSelectImagep =
				LLViewerTextureManager::getFetchedTexture(IMG_FACE_SELECT);
		}
		// Make sure the selection image gets downloaded and decoded
		mFaceSelectImagep->addTextureStats((F32)MAX_IMAGE_AREA);

		// Use the color matching the channel we are editing
		LLColor4 color;
		LLRender::eTexIndex active_channel = sRenderHighlightTextureChannel;
		switch (active_channel)
		{
			case LLRender::NORMAL_MAP:
				color = LLColor4(1.f, .5f, .5f, .5f);
				break;

			case LLRender::SPECULAR_MAP:
				color = LLColor4(0.f, .3f, 1.f, .8f);
				break;

			case LLRender::DIFFUSE_MAP:
			default:
				color = LLColor4(1.f, 1.f, 1.f, .5f);
		}

		LLGLSLShader* prev_shader = NULL;

		for (S32 i = 0; i < selected_count; ++i)
		{
			LLFace* facep = mSelectedFaces[i];
			if (facep && !facep->getDrawable()->isDead())
			{
				LLMaterial* mat = NULL;
				if (sRenderDeferred && active_channel != LLRender::DIFFUSE_MAP)
				{
					// Fetch the material info, if any
					const LLTextureEntry* te = facep->getTextureEntry();
					if (te)
					{
						mat = te->getMaterialParams().get();
					}
				}
				if (shader_interface)
				{
					// Default to diffuse map highlighting
					LLGLSLShader* new_shader = &gHighlightProgram;

					// Use normal or specular maps highlighting where possible
					// (i.e. material exists and got a corresponding map)
					if (active_channel == LLRender::NORMAL_MAP && mat &&
						mat->getNormalID().notNull())
					{
						new_shader = &gHighlightNormalProgram;
					}
					else if (active_channel == LLRender::SPECULAR_MAP && mat &&
							 mat->getSpecularID().notNull())
					{
						new_shader = &gHighlightSpecularProgram;
					}

					// Change the shader if not already the one in use
					if (shader_interface && new_shader != prev_shader)
					{
						if (prev_shader)
						{
							prev_shader->unbind();
						}
						new_shader->bind();
						prev_shader = new_shader;
					}
				}

				// Draw the selection on the face.
				facep->renderSelected(mFaceSelectImagep, color);
			}
			else if (gDebugGL)
			{
				llwarns << "Bad face in selection" << llendl;
			}
		}

		// Unbind the last shader, if any
		if (prev_shader)
		{
			prev_shader->unbind();
		}
	}
}

// Debug use
U32 LLPipeline::sCurRenderPoolType = 0;

void LLPipeline::renderGeom(LLCamera& camera)
{
	LL_FAST_TIMER(FTM_RENDER_GEOMETRY);

	// HACK: preserve/restore matrices around HUD render
	LLMatrix4a saved_modelview;
	LLMatrix4a saved_projection;
	bool hud_render = hasRenderType(RENDER_TYPE_HUD);
	if (hud_render)
	{
		saved_modelview = gGLModelView;
		saved_projection = gGLProjection;
	}

	///////////////////////////////////////////
	// Sync and verify GL state

	stop_glerror();

	LLVertexBuffer::unbind();

	// Do verification of GL state
	LLGLState::check(true, true);
	if (mRenderDebugMask & RENDER_DEBUG_VERIFY)
	{
		if (!verify())
		{
			llerrs << "Pipeline verification failed !" << llendl;
		}
	}

	static const std::string PipelineForceVBO = "Pipeline:ForceVBO";
	gAppViewerp->pingMainloopTimeout(&PipelineForceVBO);

	// Initialize lots of GL state to "safe" values
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->unbind(LLTexUnit::TT_TEXTURE);
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);

	LLGLSPipeline gls_pipeline;
	LLGLEnable multisample(RenderFSAASamples > 0 ? GL_MULTISAMPLE_ARB : 0);

	LLGLState gls_color_material(GL_COLOR_MATERIAL, mLightingDetail < 2);

	// Toggle backface culling for debugging
	LLGLEnable cull_face(mBackfaceCull ? GL_CULL_FACE : 0);
	// Set fog
	bool use_fog = hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_FOG);
	LLGLEnable fog_enable(use_fog &&
						  !canUseWindLightShadersOnObjects() ? GL_FOG : 0);
	gSky.updateFog(camera.getFar());
	if (!use_fog)
	{
		sUnderWaterRender = false;
	}

	if (LLViewerFetchedTexture::sDefaultImagep)
	{
		unit0->bind(LLViewerFetchedTexture::sDefaultImagep);
		LLViewerFetchedTexture::sDefaultImagep->setAddressMode(LLTexUnit::TAM_WRAP);
	}

	//////////////////////////////////////////////
	// Actually render all of the geometry

	static const std::string PipelineRenderDrawPools =
		"Pipeline:RenderDrawPools";
	gAppViewerp->pingMainloopTimeout(&PipelineRenderDrawPools);
	pool_set_t::iterator pools_end = mPools.end();
	for (pool_set_t::iterator iter = mPools.begin(); iter != pools_end; ++iter)
	{
		LLDrawPool* poolp = *iter;
		if (hasRenderType(poolp->getType()))
		{
			poolp->prerender();
		}
	}

	{
		LL_FAST_TIMER(FTM_POOLS);

		// *HACK: do not calculate local lights if we are rendering the HUD !
		// Removing this check will cause bad flickering when there are HUD
		// elements being rendered AND the user is in flycam mode. -nyx
		if (!hud_render)
		{
			calcNearbyLights(camera);
			setupHWLights(NULL);
		}

		bool occlude = sUseOcclusion > 1;
		U32 cur_type = 0;

		pool_set_t::iterator iter1 = mPools.begin();
		while (iter1 != pools_end)
		{
			LLDrawPool* poolp = *iter1;

			cur_type = poolp->getType();

			// Debug use
			sCurRenderPoolType = cur_type;

			if (occlude && cur_type >= LLDrawPool::POOL_GRASS)
			{
				occlude = false;
				gGLLastMatrix = NULL;
				gGL.loadMatrix(gGLModelView);
				LLGLSLShader::bindNoShader();
				doOcclusion(camera);
			}

			pool_set_t::iterator iter2 = iter1;
			if (hasRenderType(poolp->getType()) && poolp->getNumPasses() > 0)
			{
				LL_FAST_TIMER(FTM_POOLRENDER);

				gGLLastMatrix = NULL;
				gGL.loadMatrix(gGLModelView);

				for (S32 i = 0, passes = poolp->getNumPasses(); i < passes;
					 ++i)
				{
					LLVertexBuffer::unbind();
					poolp->beginRenderPass(i);
					for (iter2 = iter1; iter2 != pools_end; ++iter2)
					{
						LLDrawPool* p = *iter2;
						if (p->getType() != cur_type)
						{
							break;
						}

						p->render(i);
					}
					poolp->endRenderPass(i);
					LLVertexBuffer::unbind();
					if (gDebugGL && iter2 != pools_end)
					{
						std::string msg = llformat("%s pass %d",
												   gPoolNames[cur_type].c_str(),
												   i);
						LLGLState::check(true, false, msg);
					}
				}
			}
			else
			{
				// Skip all pools of this type
				for (iter2 = iter1; iter2 != pools_end; ++iter2)
				{
					LLDrawPool* p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}
				}
			}
			iter1 = iter2;
		}

		static const std::string PipelineRenderDrawPoolsEnd =
			"Pipeline:RenderDrawPoolsEnd";
		gAppViewerp->pingMainloopTimeout(&PipelineRenderDrawPoolsEnd);

		LLVertexBuffer::unbind();

		gGLLastMatrix = NULL;
		gGL.loadMatrix(gGLModelView);

		if (occlude)
		{
			occlude = false;
			gGLLastMatrix = NULL;
			gGL.loadMatrix(gGLModelView);
			LLGLSLShader::bindNoShader();
			doOcclusion(camera);
		}
	}

	stop_glerror();

	LLVertexBuffer::unbind();
	LLGLState::check();

	if (!sImpostorRender)
	{
		static const std::string PipelineRenderHighlights =
			"Pipeline:RenderHighlights";
		gAppViewerp->pingMainloopTimeout(&PipelineRenderHighlights);

		if (!sReflectionRender)
		{
			renderHighlights();
		}

		// Contains a list of the faces of beacon-targeted objects that are
		// also to be highlighted.
		mHighlightFaces.clear();

		static const std::string PipelineRenderDebug = "Pipeline:RenderDebug";
		gAppViewerp->pingMainloopTimeout(&PipelineRenderDebug);

		renderDebug();

		LLVertexBuffer::unbind();

		if (!sReflectionRender && !sRenderDeferred)
		{
			if (hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_UI))
			{
				// Render debugging beacons.
				gObjectList.renderObjectBeacons();
				gObjectList.resetObjectBeacons();
				gSky.addSunMoonBeacons();
			}
			else
			{
				// Make sure particle effects disappear
				LLHUDObject::renderAllForTimer();
			}
		}
		else
		{
			// Make sure particle effects disappear
			LLHUDObject::renderAllForTimer();
		}

		static const std::string PipelineRenderGeomEnd =
			"Pipeline:RenderGeomEnd";
		gAppViewerp->pingMainloopTimeout(&PipelineRenderGeomEnd);

		// HACK: preserve/restore matrices around HUD render
		if (hud_render)
		{
			gGLModelView = saved_modelview;
			gGLProjection = saved_projection;
		}
	}

	LLVertexBuffer::unbind();

	LLGLState::check();
}

void LLPipeline::renderGeomDeferred(LLCamera& camera)
{
	static const std::string PipelineRenderGeomDeferred =
		"Pipeline:RenderGeomDeferred";
	gAppViewerp->pingMainloopTimeout(&PipelineRenderGeomDeferred);

	LL_FAST_TIMER(FTM_RENDER_GEOMETRY);
	{
		LL_FAST_TIMER(FTM_POOLS);

		LLGLEnable cull(GL_CULL_FACE);

#if !LL_NEW_DEPTH_STENCIL
		LLGLEnable stencil(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
#endif

		pool_set_t::iterator pools_end = mPools.end();

		for (pool_set_t::iterator it = mPools.begin(); it != pools_end; ++it)
		{
			LLDrawPool* poolp = *it;
			if (hasRenderType(poolp->getType()))
			{
				poolp->prerender();
			}
		}

		LLGLEnable multisample(RenderFSAASamples > 0 ? GL_MULTISAMPLE_ARB : 0);

		LLVertexBuffer::unbind();

		LLGLState::check(true, true);

		U32 cur_type = 0;

		gGL.setColorMask(true, true);

		pool_set_t::iterator iter1 = mPools.begin();

		while (iter1 != pools_end)
		{
			LLDrawPool* poolp = *iter1;

			cur_type = poolp->getType();

			pool_set_t::iterator iter2 = iter1;
			if (hasRenderType(poolp->getType()) &&
				poolp->getNumDeferredPasses() > 0)
			{
				LL_FAST_TIMER(FTM_POOLRENDER);

				gGLLastMatrix = NULL;
				gGL.loadMatrix(gGLModelView);

				for (S32 i = 0, passes = poolp->getNumDeferredPasses();
					 i < passes; ++i)
				{
					LLVertexBuffer::unbind();
					poolp->beginDeferredPass(i);
					for (iter2 = iter1; iter2 != pools_end; ++iter2)
					{
						LLDrawPool* p = *iter2;
						if (p->getType() != cur_type)
						{
							break;
						}

						p->renderDeferred(i);
					}
					poolp->endDeferredPass(i);
					LLVertexBuffer::unbind();

					LLGLState::check();
				}
			}
			else
			{
				// Skip all pools of this type
				for (iter2 = iter1; iter2 != pools_end; ++iter2)
				{
					LLDrawPool* p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}
				}
			}
			iter1 = iter2;
		}

		gGLLastMatrix = NULL;
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.loadMatrix(gGLModelView);

		gGL.setColorMask(true, false);

		stop_glerror();
	}
}

void LLPipeline::renderGeomPostDeferred(LLCamera& camera, bool do_occlusion)
{
	LL_FAST_TIMER(FTM_POOLS);
	U32 cur_type = 0;

	LLGLEnable cull(GL_CULL_FACE);

	LLGLEnable multisample(RenderFSAASamples > 0 ? GL_MULTISAMPLE_ARB : 0);

	calcNearbyLights(camera);
	setupHWLights(NULL);

	gGL.setColorMask(true, false);

	pool_set_t::iterator iter1 = mPools.begin();
	pool_set_t::iterator pools_end = mPools.end();
	bool occlude = sUseOcclusion > 1 && do_occlusion;

	while (iter1 != pools_end)
	{
		LLDrawPool* poolp = *iter1;

		cur_type = poolp->getType();

		if (occlude && cur_type >= LLDrawPool::POOL_GRASS)
		{
			occlude = false;
			gGLLastMatrix = NULL;
			gGL.loadMatrix(gGLModelView);
			LLGLSLShader::bindNoShader();
			doOcclusion(camera, mScreen, mOcclusionDepth, &mDeferredDepth);
			gGL.setColorMask(true, false);
		}

		pool_set_t::iterator iter2 = iter1;
		if (hasRenderType(poolp->getType()) &&
			poolp->getNumPostDeferredPasses() > 0)
		{
			LL_FAST_TIMER(FTM_POOLRENDER);

			gGLLastMatrix = NULL;
			gGL.loadMatrix(gGLModelView);

			for (S32 i = 0, passes = poolp->getNumPostDeferredPasses();
				 i < passes; ++i)
			{
				LLVertexBuffer::unbind();
				poolp->beginPostDeferredPass(i);
				for (iter2 = iter1; iter2 != pools_end; ++iter2)
				{
					LLDrawPool* p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}

					p->renderPostDeferred(i);
				}
				poolp->endPostDeferredPass(i);
				LLVertexBuffer::unbind();

				LLGLState::check();
			}
		}
		else
		{
			// Skip all pools of this type
			for (iter2 = iter1; iter2 != pools_end; ++iter2)
			{
				LLDrawPool* p = *iter2;
				if (p->getType() != cur_type)
				{
					break;
				}
			}
		}
		iter1 = iter2;
	}

	gGLLastMatrix = NULL;
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.loadMatrix(gGLModelView);

	if (occlude)
	{
		LLGLSLShader::bindNoShader();
		doOcclusion(camera);
		gGLLastMatrix = NULL;
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.loadMatrix(gGLModelView);
	}

	stop_glerror();
}

void LLPipeline::renderGeomShadow(LLCamera& camera)
{
	U32 cur_type = 0;

	LLGLEnable cull(GL_CULL_FACE);

	LLVertexBuffer::unbind();

	pool_set_t::iterator iter1 = mPools.begin();
	pool_set_t::iterator pools_end = mPools.end();

	while (iter1 != pools_end)
	{
		LLDrawPool* poolp = *iter1;

		cur_type = poolp->getType();

		pool_set_t::iterator iter2 = iter1;
		if (hasRenderType(poolp->getType()) && poolp->getNumShadowPasses() > 0)
		{
			poolp->prerender();

			gGLLastMatrix = NULL;
			gGL.loadMatrix(gGLModelView);

			for (S32 i = 0, passes = poolp->getNumShadowPasses();
				 i < passes; ++i)
			{
				LLVertexBuffer::unbind();
				poolp->beginShadowPass(i);
				for (iter2 = iter1; iter2 != pools_end; ++iter2)
				{
					LLDrawPool* p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}

					p->renderShadow(i);
				}
				poolp->endShadowPass(i);
				LLVertexBuffer::unbind();

				LLGLState::check();
			}
		}
		else
		{
			// Skip all pools of this type
			for (iter2 = iter1; iter2 != pools_end; ++iter2)
			{
				LLDrawPool* p = *iter2;
				if (p->getType() != cur_type)
				{
					break;
				}
			}
		}
		iter1 = iter2;
	}

	gGLLastMatrix = NULL;
	gGL.loadMatrix(gGLModelView);

	stop_glerror();
}

void LLPipeline::addTrianglesDrawn(S32 index_count, LLRender::eGeomModes mode)
{
	if (mNeedsDrawStats)
	{
		S32 count = 0;
		if (mode == LLRender::TRIANGLE_STRIP)
		{
			count = index_count - 2;
		}
		else
		{
			count = index_count / 3;
		}

		mTrianglesDrawn += count;
		++mBatchCount;
		mMaxBatchSize = llmax(mMaxBatchSize, count);
		mMinBatchSize = llmin(mMinBatchSize, count);
	}

	if (sRenderFrameTest)
	{
		gWindowp->swapBuffers();
		ms_sleep(16);
	}
}

void LLPipeline::renderPhysicsDisplay()
{
	allocatePhysicsBuffer();

	gGL.flush();
	mPhysicsDisplay.bindTarget();
	glClearColor(0.f, 0.f, 0.f, 1.f);
	gGL.setColorMask(true, true);
	mPhysicsDisplay.clear();
	glClearColor(0.f, 0.f, 0.f, 0.f);

	gGL.setColorMask(true, false);

	gDebugProgram.bind();

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* region = *iter;
		for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			if (hasRenderType(part->mDrawableType))
			{
				part->renderPhysicsShapes();
			}
		}
	}

	gGL.flush();

	gDebugProgram.unbind();

	mPhysicsDisplay.flush();
}

void LLPipeline::renderDebug()
{
	bool hud_only = hasRenderType(RENDER_TYPE_HUD);
	bool render_blips = !hud_only && !mDebugBlips.empty();

	// If no debug feature is on and there's no blip to render, return now
	if (mRenderDebugMask == 0 && !render_blips) return;

	gGL.color4f(1.f, 1.f, 1.f, 1.f);

	gGLLastMatrix = NULL;
	gGL.loadMatrix(gGLModelView);
	gGL.setColorMask(true, false);

	if (render_blips)
	{
		// Render debug blips
		gUIProgram.bind();

		gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep, true);

		glPointSize(8.f);
		LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);

		gGL.begin(LLRender::POINTS);
		for (std::list<DebugBlip>::iterator iter = mDebugBlips.begin(),
											end = mDebugBlips.end();
			 iter != end; )
		{
			std::list<DebugBlip>::iterator curiter = iter++;
			DebugBlip& blip = *curiter;

			blip.mAge += gFrameIntervalSeconds;
			if (blip.mAge > 2.f)
			{
				mDebugBlips.erase(curiter);
			}

			blip.mPosition.mV[2] += gFrameIntervalSeconds * 2.f;

			gGL.color4fv(blip.mColor.mV);
			gGL.vertex3fv(blip.mPosition.mV);
		}
		gGL.end(true);
		glPointSize(1.f);

		gUIProgram.unbind();

		stop_glerror();
	}

	// If no debug feature is on, return now
	if (mRenderDebugMask == 0) return;

	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE, GL_LEQUAL);

	// Debug stuff.
	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* region = *iter;
		if (hud_only)
		{
			for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
			{
				LLSpatialPartition* part = region->getSpatialPartition(i);
				// None of the partitions under PARTITION_VO_CACHE can be NULL
				U32 type = part->mDrawableType;
				if (type == RENDER_TYPE_HUD ||
					type == RENDER_TYPE_HUD_PARTICLES)
				{
					part->renderDebug();
				}
			}
		}
		else
		{
			for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
			{
				LLSpatialPartition* part = region->getSpatialPartition(i);
				// None of the partitions under PARTITION_VO_CACHE can be NULL
				if (hasRenderType(part->mDrawableType))
				{
					part->renderDebug();
				}
			}
		}
	}

	for (LLCullResult::bridge_iterator i = sCull->beginVisibleBridge();
		 i != sCull->endVisibleBridge(); ++i)
	{
		LLSpatialBridge* bridge = *i;
		if (!bridge->isDead() && hasRenderType(bridge->mDrawableType))
		{
			gGL.pushMatrix();
			gGL.multMatrix(bridge->mDrawable->getRenderMatrix().getF32ptr());
			bridge->renderDebug();
			gGL.popMatrix();
		}
	}

	if (hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCCLUSION) &&
		!gVisibleSelectedGroups.empty())
	{
		// Render visible selected group occlusion geometry
		gDebugProgram.bind();
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);
		gGL.diffuseColor3f(1,0,1);
		LLVector4a fudge, size;
		for (spatial_groups_set_t::iterator
				iter = gVisibleSelectedGroups.begin(),
				end = gVisibleSelectedGroups.end();
			 iter != end; ++iter)
		{
			LLSpatialGroup* group = *iter;

			fudge.splat(0.25f); // SG_OCCLUSION_FUDGE

			const LLVector4a* bounds = group->getBounds();
			size.setAdd(fudge, bounds[1]);

			drawBox(bounds[0], size);
		}
	}

	gVisibleSelectedGroups.clear();

	gUIProgram.bind();

	if (!hud_only && gDebugRaycastParticle &&
		hasRenderDebugMask(RENDER_DEBUG_RAYCAST))
	{
		// Draw crosshairs on particle intersection
		gDebugProgram.bind();

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		LLVector3 center(gDebugRaycastParticleIntersection.getF32ptr());
		LLVector3 size(0.1f, 0.1f, 0.1f);

		LLVector3 p[6];
		p[0] = center + size.scaledVec(LLVector3(1.f, 0.f, 0.f));
		p[1] = center + size.scaledVec(LLVector3(-1.f, 0.f, 0.f));
		p[2] = center + size.scaledVec(LLVector3(0.f, 1.f, 0.f));
		p[3] = center + size.scaledVec(LLVector3(0.f, -1.f, 0.f));
		p[4] = center + size.scaledVec(LLVector3(0.f, 0.f, 1.f));
		p[5] = center + size.scaledVec(LLVector3(0.f, 0.f, -1.f));

		gGL.begin(LLRender::LINES);
		gGL.diffuseColor3f(1.f, 1.f, 0.f);
		for (U32 i = 0; i < 6; ++i)
		{
			gGL.vertex3fv(p[i].mV);
		}
		gGL.end(true);

		gDebugProgram.unbind();
		stop_glerror();
	}

	if (hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA))
	{
		LLVertexBuffer::unbind();

		LLGLEnable blend(GL_BLEND);
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);
		LLGLDisable cull(GL_CULL_FACE);

		gGL.color4f(1.f, 1.f, 1.f, 1.f);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		static const F32 colors[] =
		{
			1.f, 0.f, 0.f, 0.1f,
			0.f, 1.f, 0.f, 0.1f,
			0.f, 0.f, 1.f, 0.1f,
			1.f, 0.f, 1.f, 0.1f,

			1.f, 1.f, 0.f, 0.1f,
			0.f, 1.f, 1.f, 0.1f,
			1.f, 1.f, 1.f, 0.1f,
			1.f, 0.f, 1.f, 0.1f,
		};

		for (U32 i = 0; i < 8; ++i)
		{
			LLVector3* frust = mShadowCamera[i].mAgentFrustum;

			if (i > 3)
			{
				// Render shadow frusta as volumes
				if (mShadowFrustPoints[i - 4].empty())
				{
					continue;
				}

				gGL.color4fv(colors + (i - 4) * 4);

				gGL.begin(LLRender::TRIANGLE_STRIP);
				{
					gGL.vertex3fv(frust[0].mV);
					gGL.vertex3fv(frust[4].mV);

					gGL.vertex3fv(frust[1].mV);
					gGL.vertex3fv(frust[5].mV);

					gGL.vertex3fv(frust[2].mV);
					gGL.vertex3fv(frust[6].mV);

					gGL.vertex3fv(frust[3].mV);
					gGL.vertex3fv(frust[7].mV);

					gGL.vertex3fv(frust[0].mV);
					gGL.vertex3fv(frust[4].mV);
				}
				gGL.end();

				gGL.begin(LLRender::TRIANGLE_STRIP);
				{
					gGL.vertex3fv(frust[0].mV);
					gGL.vertex3fv(frust[1].mV);
					gGL.vertex3fv(frust[3].mV);
					gGL.vertex3fv(frust[2].mV);
				}
				gGL.end();

				gGL.begin(LLRender::TRIANGLE_STRIP);
				{
					gGL.vertex3fv(frust[4].mV);
					gGL.vertex3fv(frust[5].mV);
					gGL.vertex3fv(frust[7].mV);
					gGL.vertex3fv(frust[6].mV);
				}
				gGL.end();
			}

			if (i < 4)
			{
#if 0
				if (i == 0 || !mShadowFrustPoints[i].empty())
#endif
				{
					// Render visible point cloud
					gGL.flush();
					glPointSize(8.f);
					gGL.begin(LLRender::POINTS);

					gGL.color3fv(colors + i * 4);

					for (U32 j = 0, size = mShadowFrustPoints[i].size();
						 j < size; ++j)
					{
						gGL.vertex3fv(mShadowFrustPoints[i][j].mV);
					}
					gGL.end(true);

					glPointSize(1.f);

					LLVector3* ext = mShadowExtents[i];
					LLVector3 pos = (ext[0] + ext[1]) * 0.5f;
					LLVector3 size = (ext[1] - ext[0]) * 0.5f;
					drawBoxOutline(pos, size);

					// Render camera frustum splits as outlines
					gGL.begin(LLRender::LINES);
					{
						gGL.vertex3fv(frust[0].mV);
						gGL.vertex3fv(frust[1].mV);

						gGL.vertex3fv(frust[1].mV);
						gGL.vertex3fv(frust[2].mV);

						gGL.vertex3fv(frust[2].mV);
						gGL.vertex3fv(frust[3].mV);

						gGL.vertex3fv(frust[3].mV);
						gGL.vertex3fv(frust[0].mV);

						gGL.vertex3fv(frust[4].mV);
						gGL.vertex3fv(frust[5].mV);

						gGL.vertex3fv(frust[5].mV);
						gGL.vertex3fv(frust[6].mV);

						gGL.vertex3fv(frust[6].mV);
						gGL.vertex3fv(frust[7].mV);

						gGL.vertex3fv(frust[7].mV);
						gGL.vertex3fv(frust[4].mV);

						gGL.vertex3fv(frust[0].mV);
						gGL.vertex3fv(frust[4].mV);

						gGL.vertex3fv(frust[1].mV);
						gGL.vertex3fv(frust[5].mV);

						gGL.vertex3fv(frust[2].mV);
						gGL.vertex3fv(frust[6].mV);

						gGL.vertex3fv(frust[3].mV);
						gGL.vertex3fv(frust[7].mV);
					}
					gGL.end();
				}
			}
			gGL.flush();
#if 0
			gGL.lineWidth(16 - i * 2);
			for (LLWorld::region_list_t::const_iterator
					iter = gWorld.getRegionList().begin(),
					end = gWorld.getRegionList().end();
				 iter != end; ++iter)
			{
				LLViewerRegion* region = *iter;
				for (U32 j = 0; j < LLViewerRegion::PARTITION_VO_CACHE; ++j)
				{
					LLSpatialPartition* part = region->getSpatialPartition(j);
					// None of the partitions under PARTITION_VO_CACHE can be
					// NULL
					if (hasRenderType(part->mDrawableType))
					{
						part->renderIntersectingBBoxes(&mShadowCamera[i]);
					}
				}
			}
			gGL.flush();
			gGL.lineWidth(1.f);
#endif
		}
		stop_glerror();
	}

	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp && (mRenderDebugMask & RENDER_DEBUG_WIND_VECTORS))
	{
		regionp->mWind.renderVectors();
	}

	if (regionp && (mRenderDebugMask & RENDER_DEBUG_COMPOSITION))
	{
		// Debug composition layers
		F32 x, y;

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		gGL.begin(LLRender::POINTS);
		// Draw the composition layer for the region that I am in.
		for (x = 0; x <= 260; ++x)
		{
			for (y = 0; y <= 260; ++y)
			{
				if (x > 255 || y > 255)
				{
					gGL.color4f(1.f, 0.f, 0.f, 1.f);
				}
				else
				{
					gGL.color4f(0.f, 0.f, 1.f, 1.f);
				}
				F32 z = regionp->getCompositionXY((S32)x, (S32)y);
				z *= 5.f;
				z += 50.f;
				gGL.vertex3f(x, y, z);
			}
		}
		gGL.end();
		stop_glerror();
	}

	if (mRenderDebugMask & RENDER_DEBUG_BUILD_QUEUE)
	{
		U32 count = 0;
		U32 size = mGroupQ2.size();
		LLColor4 col;

		LLVertexBuffer::unbind();
		LLGLEnable blend(GL_BLEND);
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);
		gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep);

		gGL.pushMatrix();
		gGL.loadMatrix(gGLModelView);
		gGLLastMatrix = NULL;

		for (LLSpatialGroup::sg_vector_t::iterator iter = mGroupQ2.begin(),
												   end = mGroupQ2.end();
			 iter != end; ++iter)
		{
			LLSpatialGroup* group = *iter;
			if (group->isDead())
			{
				continue;
			}

			LLSpatialBridge* bridge = group->getSpatialPartition()->asBridge();
			if (bridge && (!bridge->mDrawable || bridge->mDrawable->isDead()))
			{
				continue;
			}

			if (bridge)
			{
				gGL.pushMatrix();
				gGL.multMatrix(bridge->mDrawable->getRenderMatrix().getF32ptr());
			}

			F32 alpha = llclamp((F32)(size - count) / size, 0.f, 1.f);

			LLVector2 c(1.f - alpha, alpha);
			c.normalize();

			++count;
			col.set(c.mV[0], c.mV[1], 0, alpha * 0.5f + 0.5f);
			group->drawObjectBox(col);

			if (bridge)
			{
				gGL.popMatrix();
			}
		}

		gGL.popMatrix();
		stop_glerror();
	}

	gGL.flush();

	gUIProgram.unbind();
}

void LLPipeline::rebuildPools()
{
	S32 max_count = mPools.size();
	pool_set_t::iterator iter1 = mPools.upper_bound(mLastRebuildPool);
	while (max_count > 0 && mPools.size() > 0) // && num_rebuilds < MAX_REBUILDS)
	{
		if (iter1 == mPools.end())
		{
			iter1 = mPools.begin();
		}
		LLDrawPool* poolp = *iter1;

		if (poolp->isDead())
		{
			mPools.erase(iter1++);
			removeFromQuickLookup(poolp);
			if (poolp == mLastRebuildPool)
			{
				mLastRebuildPool = NULL;
			}
			delete poolp;
		}
		else
		{
			mLastRebuildPool = poolp;
			++iter1;
		}
		--max_count;
	}
#if 0
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->rebuildHUD();
	}
#endif
}

void LLPipeline::addToQuickLookup(LLDrawPool* new_poolp)
{
	switch (new_poolp->getType())
	{
		case LLDrawPool::POOL_SIMPLE:
			if (mSimplePool)
			{
				llwarns << "Ignoring duplicate simple pool." << llendl;
				llassert(false);
			}
			else
			{
				mSimplePool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_ALPHA_MASK:
			if (mAlphaMaskPool)
			{
				llwarns << "Ignoring duplicate alpha mask pool." << llendl;
				llassert(false);
			}
			else
			{
				mAlphaMaskPool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK:
			if (mFullbrightAlphaMaskPool)
			{
				llwarns << "Ignoring duplicate alpha mask pool." << llendl;
				llassert(false);
			}
			else
			{
				mFullbrightAlphaMaskPool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_GRASS:
			if (mGrassPool)
			{
				llwarns << "Ignoring duplicate grass pool." << llendl;
				llassert(false);
			}
			else
			{
				mGrassPool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_FULLBRIGHT:
			if (mFullbrightPool)
			{
				llwarns << "Ignoring duplicate simple pool." << llendl;
				llassert(false);
			}
			else
			{
				mFullbrightPool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_INVISIBLE:
			if (mInvisiblePool)
			{
				llwarns << "Ignoring duplicate simple pool." << llendl;
				llassert(false);
			}
			else
			{
				mInvisiblePool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_GLOW:
			if (mGlowPool)
			{
				llwarns << "Ignoring duplicate glow pool." << llendl;
				llassert(false);
			}
			else
			{
				mGlowPool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_TREE:
			mTreePools.emplace(uintptr_t(new_poolp->getTexture()), new_poolp);
			break;

		case LLDrawPool::POOL_TERRAIN:
			mTerrainPools.emplace(uintptr_t(new_poolp->getTexture()),
								  new_poolp);
			break;

		case LLDrawPool::POOL_BUMP:
			if (mBumpPool)
			{
				llwarns << "Ignoring duplicate bump pool." << llendl;
				llassert(false);
			}
			else
			{
				mBumpPool = new_poolp;
			}
			break;

		case LLDrawPool::POOL_MATERIALS:
			if (mMaterialsPool)
			{
				llwarns << "Ignoring duplicate materials pool." << llendl;
				llassert(false);
			}
			else
			{
				mMaterialsPool = new_poolp;
			}
			break;

		case LLDrawPool::POOL_ALPHA:
			if (mAlphaPool)
			{
				llwarns << "Ignoring duplicate Alpha pool" << llendl;
				llassert(false);
			}
			else
			{
				mAlphaPool = (LLDrawPoolAlpha*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_AVATAR:
		case LLDrawPool::POOL_PUPPET:
			break; // Do nothing

		case LLDrawPool::POOL_SKY:
			if (mSkyPool)
			{
				llwarns << "Ignoring duplicate Sky pool" << llendl;
				llassert(false);
			}
			else
			{
				mSkyPool = new_poolp;
			}
			break;

		case LLDrawPool::POOL_WATER:
			if (mWaterPool)
			{
				llwarns << "Ignoring duplicate Water pool" << llendl;
				llassert(false);
			}
			else
			{
				mWaterPool = new_poolp;
			}
			break;

		case LLDrawPool::POOL_GROUND:
			if (mGroundPool)
			{
				llwarns << "Ignoring duplicate Ground Pool" << llendl;
				llassert(false);
			}
			else
			{
				mGroundPool = new_poolp;
			}
			break;

		case LLDrawPool::POOL_WL_SKY:
			if (mWLSkyPool)
			{
				llwarns << "Ignoring duplicate WLSky Pool" << llendl;
				llassert(false);
			}
			else
			{
				mWLSkyPool = new_poolp;
			}
			break;

		default:
			llerrs << "Invalid Pool Type: " << new_poolp->getType() << llendl;
	}
}

void LLPipeline::removePool(LLDrawPool* poolp)
{
	removeFromQuickLookup(poolp);
	mPools.erase(poolp);
	delete poolp;
}

void LLPipeline::removeFromQuickLookup(LLDrawPool* poolp)
{
	switch (poolp->getType())
	{
		case LLDrawPool::POOL_SIMPLE:
			llassert(mSimplePool == poolp);
			mSimplePool = NULL;
			break;

		case LLDrawPool::POOL_ALPHA_MASK:
			llassert(mAlphaMaskPool == poolp);
			mAlphaMaskPool = NULL;
			break;

		case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK:
			llassert(mFullbrightAlphaMaskPool == poolp);
			mFullbrightAlphaMaskPool = NULL;
			break;

		case LLDrawPool::POOL_GRASS:
			llassert(mGrassPool == poolp);
			mGrassPool = NULL;
			break;

		case LLDrawPool::POOL_FULLBRIGHT:
			llassert(mFullbrightPool == poolp);
			mFullbrightPool = NULL;
			break;

		case LLDrawPool::POOL_INVISIBLE:
			llassert(mInvisiblePool == poolp);
			mInvisiblePool = NULL;
			break;

		case LLDrawPool::POOL_WL_SKY:
			llassert(mWLSkyPool == poolp);
			mWLSkyPool = NULL;
			break;

		case LLDrawPool::POOL_GLOW:
			llassert(mGlowPool == poolp);
			mGlowPool = NULL;
			break;

		case LLDrawPool::POOL_TREE:
		{
#if LL_DEBUG
			bool found = mTreePools.erase((uintptr_t)poolp->getTexture());
			llassert(found);
#else
			mTreePools.erase((uintptr_t)poolp->getTexture());
#endif
			break;
		}

		case LLDrawPool::POOL_TERRAIN:
		{
#if LL_DEBUG
			bool found = mTerrainPools.erase((uintptr_t)poolp->getTexture());
			llassert(found);
#else
			mTerrainPools.erase((uintptr_t)poolp->getTexture());
#endif
			break;
		}

		case LLDrawPool::POOL_BUMP:
			llassert(poolp == mBumpPool);
			mBumpPool = NULL;
			break;

		case LLDrawPool::POOL_MATERIALS:
			llassert(poolp == mMaterialsPool);
			mMaterialsPool = NULL;
			break;

		case LLDrawPool::POOL_ALPHA:
			llassert(poolp == mAlphaPool);
			mAlphaPool = NULL;
			break;

		case LLDrawPool::POOL_AVATAR:
		case LLDrawPool::POOL_PUPPET:
			break; // Do nothing

		case LLDrawPool::POOL_SKY:
			llassert(poolp == mSkyPool);
			mSkyPool = NULL;
			break;

		case LLDrawPool::POOL_WATER:
			llassert(poolp == mWaterPool);
			mWaterPool = NULL;
			break;

		case LLDrawPool::POOL_GROUND:
			llassert(poolp == mGroundPool);
			mGroundPool = NULL;
			break;

		default:
			llerrs << "Invalid pool type: " << poolp->getType() << llendl;
			break;
	}
}

void LLPipeline::resetDrawOrders()
{
	// Iterate through all of the draw pools and rebuild them.
	for (pool_set_t::iterator iter = mPools.begin(), end = mPools.end();
		 iter != end; ++iter)
	{
		LLDrawPool* poolp = *iter;
		poolp->resetDrawOrders();
	}
}

//============================================================================
// Once-per-frame setup of hardware lights, including sun/moon, avatar
// backlight, and up to 6 local lights

void LLPipeline::setupAvatarLights(bool for_edit)
{
	LLLightState* light = gGL.getLight(1);
	if (for_edit)
	{
		static const LLColor4 white_transparent(1.f, 1.f, 1.f, 0.f);
		mHWLightColors[1] = white_transparent;

		LLMatrix4 camera_mat = gViewerCamera.getModelview();
		LLMatrix4 camera_rot(camera_mat.getMat3());
		camera_rot.invert();

		// w = 0 => directional light
		static const LLVector4 light_pos_cam(-8.f, 0.25f, 10.f, 0.f);
		LLVector4 light_pos = light_pos_cam * camera_rot;
		light_pos.normalize();

		light->setDiffuse(white_transparent);
		light->setAmbient(LLColor4::black);
		light->setSpecular(LLColor4::black);
		light->setPosition(light_pos);
		light->setConstantAttenuation(1.f);
		light->setLinearAttenuation(0.f);
		light->setQuadraticAttenuation(0.f);
		light->setSpotExponent(0.f);
		light->setSpotCutoff(180.f);
	}
	else
	{
		mHWLightColors[1] = LLColor4::black;

		light->setDiffuse(LLColor4::black);
		light->setAmbient(LLColor4::black);
		light->setSpecular(LLColor4::black);
	}
}

static F32 calc_light_dist(LLVOVolume* light, const LLVector3& cam_pos,
						   F32 max_dist)
{
	F32 inten = light->getLightIntensity();
	if (inten < .001f)
	{
		return max_dist;
	}
	F32 radius = light->getLightRadius();
	bool selected = light->isSelected();
	LLVector3 dpos = light->getRenderPosition() - cam_pos;
	F32 dist2 = dpos.lengthSquared();
	if (!selected && dist2 > (max_dist + radius) * (max_dist + radius))
	{
		return max_dist;
	}
	F32 dist = sqrtf(dist2) / inten - radius;
	if (selected)
	{
		dist -= 10000.f; // selected lights get highest priority
	}
	if (light->mDrawable.notNull() &&
		light->mDrawable->isState(LLDrawable::ACTIVE))
	{
		// Moving lights get a little higher priority (too much causes
		// artifacts)
		dist -= light->getLightRadius() * 0.25f;
	}
	return dist;
}

void LLPipeline::calcNearbyLights(LLCamera& camera)
{
	if (sReflectionRender)
	{
		return;
	}

	if (mLightingDetail >= 1)
	{
		// mNearbyLight (and all light_set_t's) are sorted such that
		// begin() == the closest light and rbegin() == the farthest light
		constexpr S32 MAX_LOCAL_LIGHTS = 6;
#if 0
 		LLVector3 cam_pos = gAgent.getCameraPositionAgent();
#else
		LLVector3 cam_pos =
			LLViewerJoystick::getInstance()->getOverrideCamera() ?
				camera.getOrigin() : gAgent.getPositionAgent();
#endif
		// Ignore entirely lights > 4 * max light rad
		F32 max_dist = LIGHT_MAX_RADIUS * 4.f;

		// UPDATE THE EXISTING NEARBY LIGHTS
		light_set_t cur_nearby_lights;
		for (light_set_t::iterator iter = mNearbyLights.begin(),
								   end = mNearbyLights.end();
			 iter != end; ++iter)
		{
			const Light* light = &(*iter);
			if (!light) continue;		// Paranoia
			LLDrawable* drawable = light->drawable;
			if (!drawable) continue;	// Paranoia

			LLVOVolume* volight = drawable->getVOVolume();
			if (!volight || !drawable->isState(LLDrawable::LIGHT))
			{
				drawable->clearState(LLDrawable::NEARBY_LIGHT);
				continue;
			}
			if (!sRenderAttachedLights && volight->isAttachment())
			{
				drawable->clearState(LLDrawable::NEARBY_LIGHT);
				continue;
			}
			if (light->fade <= -LIGHT_FADE_TIME)
			{
				drawable->clearState(LLDrawable::NEARBY_LIGHT);
				continue;
			}
			LLVOAvatar* av = volight->getAvatar();
			if (av && av->isVisuallyMuted())
			{
				drawable->clearState(LLDrawable::NEARBY_LIGHT);
				continue;
			}

			F32 dist = calc_light_dist(volight, cam_pos, max_dist);
			cur_nearby_lights.emplace(drawable, dist, light->fade);
		}
		mNearbyLights = cur_nearby_lights;

		// FIND NEW LIGHTS THAT ARE IN RANGE
		light_set_t new_nearby_lights;
		for (LLDrawable::draw_set_t::iterator iter = mLights.begin(),
											  end = mLights.end();
			 iter != end; ++iter)
		{
			LLDrawable* drawable = *iter;
			LLVOVolume* light = drawable->getVOVolume();
			if (!light || drawable->isState(LLDrawable::NEARBY_LIGHT) ||
				light->isHUDAttachment())
			{
				continue;
			}
			if (!sRenderAttachedLights && light->isAttachment())
			{
				continue;
			}
			F32 dist = calc_light_dist(light, cam_pos, max_dist);
			if (dist >= max_dist)
			{
				continue;
			}
			new_nearby_lights.emplace(drawable, dist, 0.f);
			if (new_nearby_lights.size() > (U32)MAX_LOCAL_LIGHTS)
			{
				new_nearby_lights.erase(--new_nearby_lights.end());
				const Light& last = *new_nearby_lights.rbegin();
				max_dist = last.dist;
			}
		}

		// INSERT ANY NEW LIGHTS
		for (light_set_t::iterator iter = new_nearby_lights.begin(),
								   end = new_nearby_lights.end();
			 iter != end; ++iter)
		{
			const Light* light = &(*iter);
			if (mNearbyLights.size() < (U32)MAX_LOCAL_LIGHTS)
			{
				mNearbyLights.emplace(*light);
				((LLDrawable*)light->drawable)->setState(LLDrawable::NEARBY_LIGHT);
			}
			else
			{
				// Crazy cast so that we can overwrite the fade value even
				// though gcc enforces sets as const (fade value does not
				// affect sort so this is safe)
				Light* farthest_light =
					(const_cast<Light*>(&(*(mNearbyLights.rbegin()))));
				if (light->dist >= farthest_light->dist)
				{
					break; // None of the other lights are closer
				}
				if (farthest_light->fade >= 0.f)
				{
					farthest_light->fade = -gFrameIntervalSeconds;
				}
			}
		}

		// Mark nearby lights not-removable.
		for (light_set_t::iterator iter = mNearbyLights.begin(),
								   end = mNearbyLights.end();
			 iter != end; ++iter)
		{
			const Light* light = &(*iter);
			((LLViewerOctreeEntryData*)light->drawable)->setVisible();
		}
	}
}

void LLPipeline::setupHWLights(LLDrawPool* pool)
{
	LLColor4 total_ambient;	// LLColor4::black by default, which match WL
	if (gUseNewShaders)
	{
		const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();

		// Ambient
		total_ambient = skyp->getTotalAmbient();
		gGL.setAmbientLightColor(total_ambient);

		mIsSunUp = gEnvironment.getIsSunUp();
		// Prevent underlighting from having neither lightsource facing us
		if (!mIsSunUp && !gEnvironment.getIsMoonUp())
		{
			mSunDir.set(0.f, 1.f, 0.f, 0.f);
			mMoonDir.set(0.f, 1.f, 0.f, 0.f);
			mSunDiffuse.setToBlack();
			mMoonDiffuse.setToBlack();
		}
		else
		{
			mSunDir.set(gEnvironment.getSunDirection(), 0.f);
			mMoonDir.set(gEnvironment.getMoonDirection(), 0.f);
			mSunDiffuse.set(skyp->getSunlightColor());
			mMoonDiffuse.set(skyp->getMoonlightColor());
		}
	}
	else
	{
		mIsSunUp = gSky.sunUp();
		mSunDir.set(gSky.getSunDirection(), 0.f);
		mMoonDir.set(gSky.getMoonDirection(), 0.f);
		mSunDiffuse.set(gSky.getSunDiffuseColor());
		mMoonDiffuse.set(gSky.getMoonDiffuseColor());
	}

	// Sun or Moon (All objects)
	F32 max_color = llmax(mSunDiffuse.mV[0], mSunDiffuse.mV[1],
						  mSunDiffuse.mV[2]);
	if (max_color > 1.f)
	{
		mSunDiffuse *= 1.f / max_color;
	}
	mSunDiffuse.clamp();

	max_color = llmax(mMoonDiffuse.mV[0], mMoonDiffuse.mV[1],
					  mMoonDiffuse.mV[2]);
	if (max_color > 1.f)
	{
		mMoonDiffuse *= 1.f / max_color;
	}
	mMoonDiffuse.clamp();

	LLLightState* light = gGL.getLight(0);
	light->setPosition(mIsSunUp ? mSunDir : mMoonDir);
	LLColor4 light_diffuse = mIsSunUp ? mSunDiffuse : mMoonDiffuse;
	mHWLightColors[0] = light_diffuse;
	light->setDiffuse(light_diffuse);
	if (gUseNewShaders)
	{
		light->setSunPrimary(mIsSunUp);
		light->setDiffuseB(mMoonDiffuse);
	}
	light->setAmbient(total_ambient);
	light->setSpecular(LLColor4::black);
	light->setConstantAttenuation(1.f);
	light->setLinearAttenuation(0.f);
	light->setQuadraticAttenuation(0.f);
	light->setSpotExponent(0.f);
	light->setSpotCutoff(180.f);

	// Nearby lights = LIGHT 2-7
	S32 cur_light = 2;

	mLightMovingMask = 0;

	if (mLightingDetail >= 1)
	{
		for (light_set_t::iterator iter = mNearbyLights.begin(),
								   end = mNearbyLights.end();
			 iter != end; ++iter)
		{
			LLDrawable* drawable = iter->drawable;
			LLVOVolume* vovol = drawable->getVOVolume();
			if (!vovol)
			{
				continue;
			}

			bool is_attachment = vovol->isAttachment();
			if (is_attachment && !sRenderAttachedLights)
			{
				continue;
			}

			const LLViewerObject* vobj = drawable->getVObj();
			if (vobj)
			{
				LLVOAvatar* av = is_attachment ? vobj->getAvatar()
											   : NULL;
				if (av && !av->isSelf() &&
					(av->isInMuteList() || av->isTooComplex()))
				{
					continue;
				}
			}

			if (drawable->isState(LLDrawable::ACTIVE))
			{
				mLightMovingMask |= 1 << cur_light;
			}

			// Send linear light color to shader
			LLColor4 light_color = vovol->getLightLinearColor();
			light_color.mV[3] = 0.f;

			F32 fade = iter->fade;
			if (fade < LIGHT_FADE_TIME)
			{
				constexpr F32 LIGHT_FADE_TIME_INV = 1.f / LIGHT_FADE_TIME;
				// Fade in/out light
				if (fade >= 0.f)
				{
					fade = fade * LIGHT_FADE_TIME_INV;
					((Light*)(&(*iter)))->fade += gFrameIntervalSeconds;
				}
				else
				{
					fade = 1.f + fade * LIGHT_FADE_TIME_INV;
					((Light*)(&(*iter)))->fade -= gFrameIntervalSeconds;
				}
				fade = llclamp(fade, 0.f, 1.f);
				light_color *= fade;
			}

			if (light_color.lengthSquared() < 0.001f)
			{
				continue;
			}

			F32 adjusted_radius = vovol->getLightRadius();
			if (sRenderDeferred)
			{
				 adjusted_radius *= 1.5f;
			}
			if (adjusted_radius <= 0.001f)
			{
				continue;
			}

			LLVector4 light_pos_gl(vovol->getRenderPosition(), 1.f);

			// Why this magic ?  Probably trying to match a historic behavior:
			F32 x = 3.f * (1.f + vovol->getLightFalloff(2.f));
			F32 linatten = x / adjusted_radius;

			mHWLightColors[cur_light] = light_color;
			light = gGL.getLight(cur_light);

			light->setPosition(light_pos_gl);
			light->setDiffuse(light_color);
			light->setAmbient(LLColor4::black);
			light->setConstantAttenuation(0.f);
			light->setLinearAttenuation(linatten);
			if (sRenderDeferred)
			{
				// Get falloff to match for forward deferred rendering lights
				F32 falloff = 1.f + vovol->getLightFalloff(0.5f);
				light->setQuadraticAttenuation(falloff);
			}
			else
			{
				light->setQuadraticAttenuation(0);
			}

			if (vovol->isLightSpotlight() &&	// Directional (spot-)light
				// These are only rendered as GL spotlights if we are in
				// deferred rendering mode *or* the setting forces them on:
				(sRenderDeferred || RenderSpotLightsInNondeferred))
			{
				LLQuaternion quat = vovol->getRenderRotation();
				// This matches deferred rendering's object light direction:
				LLVector3 at_axis(0.f, 0.f, -1.f);
				at_axis *= quat;

				light->setSpotDirection(at_axis);
				light->setSpotCutoff(90.f);
				light->setSpotExponent(2.f);
				LLVector3 spot_params = vovol->getSpotLightParams();
				const LLColor4 specular(0.f, 0.f, 0.f, spot_params[2]);
				light->setSpecular(specular);
			}
			else // Omnidirectional (point) light
			{
				light->setSpotExponent(0.f);
				light->setSpotCutoff(180.f);
				if (gUseNewShaders)
				{
					// We use z = 1.f as a cheap hack for the shaders to know
					// that this is omnidirectional rather than a spotlight
					light->setSpecular(LLColor4(0.f, 0.f, 1.f, 0.f));
				}
				else
				{
					// We use black (w = 1.f) instead of transparent (w = 0.f)
					// as a cheap hack for the shaders to know that this is
					// omnidirectional rather than a spotlight
					light->setSpecular(LLColor4::black);
				}
			}
			if (++cur_light >= 8)
			{
				break; // safety
			}
		}
	}
	for ( ; cur_light < 8; ++cur_light)
	{
		mHWLightColors[cur_light] = LLColor4::black;
		light = gGL.getLight(cur_light);
		if (gUseNewShaders)
		{
			light->setSunPrimary(true);
		}
		light->setDiffuse(LLColor4::black);
		light->setAmbient(LLColor4::black);
		light->setSpecular(LLColor4::black);
	}

	static LLCachedControl<bool> customize_lighting(gSavedSettings,
													"AvatarCustomizeLighting");
	if (customize_lighting && isAgentAvatarValid() &&
		gAgentAvatarp->mSpecialRenderMode == 3)
	{
		LLVector3 light_pos(gViewerCamera.getOrigin());
		LLVector4 light_pos_gl(light_pos, 1.f);

		F32 light_radius = 16.f;
		F32 x = 3.f;
		F32 linatten = x / light_radius; // % of brightness at radius

		light = gGL.getLight(2);

		LLColor4 light_color;
		if (gUseNewShaders)
		{
			light_color = LLColor4::white;
			light->setDiffuseB(light_color * 0.25f);
		}
		else
		{
			light_color = LLColor4(1.f, 1.f, 1.f, 0.f);
		}
		mHWLightColors[2] = light_color;
		light->setPosition(light_pos_gl);
		light->setDiffuse(light_color);
		if (gUseNewShaders)
		{
			light->setDiffuseB(light_color * 0.25f);
		}
		light->setAmbient(LLColor4::black);
		light->setSpecular(LLColor4::black);
		light->setQuadraticAttenuation(0.f);
		light->setConstantAttenuation(0.f);
		light->setLinearAttenuation(linatten);
		light->setSpotExponent(0.f);
		light->setSpotCutoff(180.f);
	}

	for (S32 i = 0; i < 8; ++i)
	{
		gGL.getLight(i)->disable();
	}
	mLightMask = 0;
}

void LLPipeline::enableLights(U32 mask)
{
	if (mLightingDetail == 0)
	{
		mask &= 0xf003; // sun and backlight only (and fullbright bit)
	}
	if (mLightMask != mask)
	{
		if (mask)
		{
			for (S32 i = 0; i < 8; ++i)
			{
				LLLightState* light = gGL.getLight(i);
				if (mask & (1 << i))
				{
					light->enable();
					light->setDiffuse(mHWLightColors[i]);
				}
				else
				{
					light->disable();
					light->setDiffuse(LLColor4::black);
				}
			}
		}
		mLightMask = mask;

		if (!gUseNewShaders)
		{
			gGL.setAmbientLightColor(gSky.getTotalAmbientColor());
		}
		stop_glerror();
	}
}

void LLPipeline::enableLightsStatic()
{
	U32 mask = 0x01;				// Sun
	if (mLightingDetail >= 2)
	{
		mask |= mLightMovingMask;	// Hardware moving lights
	}
	else
	{
		mask |= 0xff & ~2;			// Hardware local lights
	}
	enableLights(mask);
}

void LLPipeline::enableLightsDynamic()
{
	U32 mask = 0xff & (~2); // Local lights
	enableLights(mask);

	if (isAgentAvatarValid() && getLightingDetail() <= 0)
	{
		if (gAgentAvatarp->mSpecialRenderMode == 0)			// Normal
		{
			enableLightsAvatar();
		}
		else if (gAgentAvatarp->mSpecialRenderMode >= 1)	// Anim preview
		{
			enableLightsAvatarEdit();
		}
	}
}

void LLPipeline::enableLightsAvatar()
{
	setupAvatarLights(false);
	enableLights(0xff);		// All lights
}

void LLPipeline::enableLightsPreview()
{
	disableLights();

	gGL.setAmbientLightColor(PreviewAmbientColor);

	LLLightState* light = gGL.getLight(1);

	light->enable();
	light->setPosition(LLVector4(PreviewDirection0, 0.f));
	light->setDiffuse(PreviewDiffuse0);
	light->setAmbient(PreviewAmbientColor);
	light->setSpecular(PreviewSpecular0);
	light->setSpotExponent(0.f);
	light->setSpotCutoff(180.f);

	light = gGL.getLight(2);
	light->enable();
	light->setPosition(LLVector4(PreviewDirection1, 0.f));
	light->setDiffuse(PreviewDiffuse1);
	light->setAmbient(PreviewAmbientColor);
	light->setSpecular(PreviewSpecular1);
	light->setSpotExponent(0.f);
	light->setSpotCutoff(180.f);

	light = gGL.getLight(3);
	light->enable();
	light->setPosition(LLVector4(PreviewDirection2, 0.f));
	light->setDiffuse(PreviewDiffuse2);
	light->setAmbient(PreviewAmbientColor);
	light->setSpecular(PreviewSpecular2);
	light->setSpotExponent(0.f);
	light->setSpotCutoff(180.f);
}

void LLPipeline::enableLightsAvatarEdit()
{
	U32 mask = 0x2002;	// Avatar backlight only, set ambient
	setupAvatarLights(true);
	enableLights(mask);

	gGL.setAmbientLightColor(LLColor4(0.7f, 0.6f, 0.3f, 1.f));
}

void LLPipeline::enableLightsFullbright()
{
	U32 mask = 0x1000;	// Non-0 mask, set ambient
	enableLights(mask);
	if (!gUseNewShaders)
	{
		gGL.setAmbientLightColor(LLColor4::white);
	}
}

void LLPipeline::disableLights()
{
	enableLights(0);	// No lighting (full bright)
}

#if LL_DEBUG && 0
void LLPipeline::findReferences(LLDrawable* drawablep)
{
	if (mLights.find(drawablep) != mLights.end())
	{
		llinfos << "In mLights" << llendl;
	}
	if (std::find(mMovedList.begin(), mMovedList.end(),
				  drawablep) != mMovedList.end())
	{
		llinfos << "In mMovedList" << llendl;
	}
	if (std::find(mShiftList.begin(), mShiftList.end(),
				  drawablep) != mShiftList.end())
	{
		llinfos << "In mShiftList" << llendl;
	}
	if (mRetexturedList.find(drawablep) != mRetexturedList.end())
	{
		llinfos << "In mRetexturedList" << llendl;
	}

	if (std::find(mBuildQ1.begin(), mBuildQ1.end(),
				  drawablep) != mBuildQ1.end())
	{
		llinfos << "In mBuildQ1" << llendl;
	}
	if (std::find(mBuildQ2.begin(), mBuildQ2.end(),
				  drawablep) != mBuildQ2.end())
	{
		llinfos << "In mBuildQ2" << llendl;
	}

	S32 count;

	count = gObjectList.findReferences(drawablep);
	if (count)
	{
		llinfos << "In other drawables: " << count << " references" << llendl;
	}
}
#endif

bool LLPipeline::verify()
{
	bool ok = true;
	for (pool_set_t::iterator iter = mPools.begin(), end = mPools.end();
		 iter != end; ++iter)
	{
		LLDrawPool* poolp = *iter;
		if (!poolp->verify())
		{
			ok = false;
		}
	}
	if (!ok)
	{
		llwarns << "Pipeline verify failed !" << llendl;
	}
	return ok;
}

//////////////////////////////
// Collision detection

///////////////////////////////////////////////////////////////////////////////
/**
 *	A method to compute a ray-AABB intersection.
 *	Original code by Andrew Woo, from "Graphics Gems", Academic Press, 1990
 *	Optimized code by Pierre Terdiman, 2000 (~20-30% faster on my Celeron 500)
 *	Epsilon value added by Klaus Hartmann. (discarding it saves a few cycles only)
 *
 *	Hence this version is faster as well as more robust than the original one.
 *
 *	Should work provided:
 *	1) the integer representation of 0.f is 0x00000000
 *	2) the sign bit of the float is the most significant one
 *
 *	Report bugs: p.terdiman@codercorner.com
 *
 *	\param		aabb		[in] the axis-aligned bounding box
 *	\param		origin		[in] ray origin
 *	\param		dir			[in] ray direction
 *	\param		coord		[out] impact coordinates
 *	\return		true if ray intersects AABB
 */
///////////////////////////////////////////////////////////////////////////////
//#define RAYAABB_EPSILON 0.00001f
#define IR(x)	((U32&)x)

bool LLRayAABB(const LLVector3& center, const LLVector3& size,
			   const LLVector3& origin, const LLVector3& dir,
			   LLVector3& coord, F32 epsilon)
{
	bool inside = true;
	LLVector3 MinB = center - size;
	LLVector3 MaxB = center + size;
	LLVector3 MaxT;
	MaxT.mV[VX] = MaxT.mV[VY] = MaxT.mV[VZ] = -1.f;

	// Find candidate planes.
	for (U32 i = 0; i < 3; ++i)
	{
		if (origin.mV[i] < MinB.mV[i])
		{
			coord.mV[i]	= MinB.mV[i];
			inside = false;

			// Calculate T distances to candidate planes
			if (IR(dir.mV[i]))
			{
				MaxT.mV[i] = (MinB.mV[i] - origin.mV[i]) / dir.mV[i];
			}
		}
		else if (origin.mV[i] > MaxB.mV[i])
		{
			coord.mV[i]	= MaxB.mV[i];
			inside = false;

			// Calculate T distances to candidate planes
			if (IR(dir.mV[i]))
			{
				MaxT.mV[i] = (MaxB.mV[i] - origin.mV[i]) / dir.mV[i];
			}
		}
	}

	// Ray origin inside bounding box
	if (inside)
	{
		coord = origin;
		return true;
	}

	// Get largest of the maxT's for final choice of intersection
	U32 WhichPlane = 0;
	if (MaxT.mV[1] > MaxT.mV[WhichPlane])
	{
		WhichPlane = 1;
	}
	if (MaxT.mV[2] > MaxT.mV[WhichPlane])
	{
		WhichPlane = 2;
	}

	// Check final candidate actually inside box
	if (IR(MaxT.mV[WhichPlane]) & 0x80000000)
	{
		return false;
	}

	for (U32 i = 0; i < 3; ++i)
	{
		if (i != WhichPlane)
		{
			coord.mV[i] = origin.mV[i] + MaxT.mV[WhichPlane] * dir.mV[i];
			if (epsilon > 0)
			{
				if (coord.mV[i] < MinB.mV[i] - epsilon ||
					coord.mV[i] > MaxB.mV[i] + epsilon)
				{
					return false;
				}
			}
			else
			{
				if (coord.mV[i] < MinB.mV[i] || coord.mV[i] > MaxB.mV[i])
				{
					return false;
				}
			}
		}
	}

	return true;	// ray hits box
}

void LLPipeline::setLight(LLDrawable* drawablep, bool is_light)
{
	if (drawablep)
	{
		if (is_light)
		{
			mLights.emplace(drawablep);
			drawablep->setState(LLDrawable::LIGHT);
		}
		else
		{
			drawablep->clearState(LLDrawable::LIGHT);
			mLights.erase(drawablep);
		}
	}
}

//static
void LLPipeline::toggleRenderType(U32 type)
{
//MK
	// Force the render type to true if our vision is restricted
	if (gRLenabled &&
		(type == RENDER_TYPE_AVATAR || type == RENDER_TYPE_PUPPET) &&
		gRLInterface.mVisionRestricted)
	{
		gPipeline.mRenderTypeEnabled[type] = true;
		return;
	}
//mk
	gPipeline.mRenderTypeEnabled[type] = !gPipeline.mRenderTypeEnabled[type];
	if (type == RENDER_TYPE_WATER)
	{
		gPipeline.mRenderTypeEnabled[RENDER_TYPE_VOIDWATER] =
			!gPipeline.mRenderTypeEnabled[RENDER_TYPE_VOIDWATER];
	}
}

//static
void LLPipeline::toggleRenderTypeControl(void* data)
{
	U32 type = (U32)(intptr_t)data;
	U32 bit = 1 << type;
	if (gPipeline.hasRenderType(type))
	{
		llinfos << "Toggling render type mask " << std::hex << bit << " off"
				<< std::dec << llendl;
	}
	else
	{
		llinfos << "Toggling render type mask " << std::hex << bit << " on"
				<< std::dec << llendl;
	}
	gPipeline.toggleRenderType(type);
}

//static
bool LLPipeline::hasRenderTypeControl(void* data)
{
	U32 type = (U32)(intptr_t)data;
	return gPipeline.hasRenderType(type);
}

// Allows UI items labeled "Hide foo" instead of "Show foo"
//static
bool LLPipeline::toggleRenderTypeControlNegated(void* data)
{
	S32 type = (S32)(intptr_t)data;
	return !gPipeline.hasRenderType(type);
}

//static
void LLPipeline::toggleRenderDebug(void* data)
{
	U32 bit = (U32)(intptr_t)data;
//MK
	if (gRLenabled && gRLInterface.mVisionRestricted)
	{
		bit = 0;
	}
//mk
	if (gPipeline.hasRenderDebugMask(bit))
	{
		llinfos << "Toggling render debug mask " << std::hex << bit << " off"
				<< std::dec << llendl;
	}
	else
	{
		llinfos << "Toggling render debug mask " << std::hex << bit << " on"
				<< std::dec << llendl;
	}
	gPipeline.mRenderDebugMask ^= bit;
}

//static
bool LLPipeline::toggleRenderDebugControl(void* data)
{
	U32 bit = (U32)(intptr_t)data;
	return gPipeline.hasRenderDebugMask(bit);
}

//static
void LLPipeline::toggleRenderDebugFeature(void* data)
{
	U32 bit = (U32)(intptr_t)data;
	gPipeline.mRenderDebugFeatureMask ^= bit;
}

//static
bool LLPipeline::toggleRenderDebugFeatureControl(void* data)
{
	U32 bit = (U32)(intptr_t)data;
	return gPipeline.hasRenderDebugFeatureMask(bit);
}

//static
void LLPipeline::setRenderDebugFeatureControl(U32 bit, bool value)
{
	if (value)
	{
		gPipeline.mRenderDebugFeatureMask |= bit;
	}
	else
	{
		gPipeline.mRenderDebugFeatureMask &= !bit;
	}
}

LLVOPartGroup* LLPipeline::lineSegmentIntersectParticle(const LLVector4a& start,
														const LLVector4a& end,
														LLVector4a* intersection,
														S32* face_hit)
{
	LLVector4a local_end = end;
	LLVector4a position;
	LLDrawable* drawable = NULL;

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			wend = gWorld.getRegionList().end();
			iter != wend; ++iter)
	{
		LLViewerRegion* region = *iter;
		if (!region) continue;	// Paranoia

		LLSpatialPartition* part =
			region->getSpatialPartition(LLViewerRegion::PARTITION_PARTICLE);
		// PARTITION_PARTICLE cannot be NULL
		if (hasRenderType(part->mDrawableType))
		{
			LLDrawable* hit = part->lineSegmentIntersect(start, local_end,
														 true, false,
														 face_hit, &position);
			if (hit)
			{
				drawable = hit;
				local_end = position;
			}
		}
	}

	LLVOPartGroup* ret = NULL;
	if (drawable)
	{
		// Make sure we are returning an LLVOPartGroup
		ret = drawable->getVObj().get()->asVOPartGroup();
	}

	if (intersection)
	{
		*intersection = position;
	}

	return ret;
}

LLViewerObject* LLPipeline::lineSegmentIntersectInWorld(const LLVector4a& start,
														const LLVector4a& end,
														bool pick_transparent,
														bool pick_rigged,
														S32* face_hit,
														// intersection point
														LLVector4a* intersection,
														// texcoords of intersection
														LLVector2* tex_coord,
														// normal at intersection
														LLVector4a* normal,
														// tangent at intersection
														LLVector4a* tangent)
{
	LLDrawable* drawable = NULL;
	LLVector4a local_end = end;
	LLVector4a position;

	sPickAvatar = false;	// !LLToolMgr::getInstance()->inBuildMode();

	// Only check these non-avatar partitions in a first step
	static const U32 non_avatars[] =
	{
		LLViewerRegion::PARTITION_VOLUME,
		LLViewerRegion::PARTITION_PUPPET,
		LLViewerRegion::PARTITION_BRIDGE,
		LLViewerRegion::PARTITION_TERRAIN,
		LLViewerRegion::PARTITION_TREE,
		LLViewerRegion::PARTITION_GRASS
	};
	constexpr U32 non_avatars_count = LL_ARRAY_SIZE(non_avatars);

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			wend = gWorld.getRegionList().end();
		 iter != wend; ++iter)
	{
		LLViewerRegion* region = *iter;
		if (!region) continue;	// Paranoia

		for (U32 j = 0; j < non_avatars_count; ++j)
		{
			U32 type = non_avatars[j];
			LLSpatialPartition* part = region->getSpatialPartition(type);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			if (hasRenderType(part->mDrawableType))
			{
				LLDrawable* hit = part->lineSegmentIntersect(start, local_end,
															 pick_transparent,
															 pick_rigged,
															 face_hit,
															 &position,
															 tex_coord, normal,
															 tangent);
				if (hit)
				{
					drawable = hit;
					local_end = position;
				}
			}
		}
	}

	if (!sPickAvatar)
	{
		// Save hit info in case we need to restore due to attachment override
		LLVector4a local_normal;
		LLVector4a local_tangent;
		LLVector2 local_texcoord;
		S32 local_face_hit = -1;

		if (face_hit)
		{
			local_face_hit = *face_hit;
		}
		if (tex_coord)
		{
			local_texcoord = *tex_coord;
		}
		if (tangent)
		{
			local_tangent = *tangent;
		}
		else
		{
			local_tangent.clear();
		}
		if (normal)
		{
			local_normal = *normal;
		}
		else
		{
			local_normal.clear();
		}

		constexpr F32 ATTACHMENT_OVERRIDE_DIST = 0.1f;

		// Check against avatars
		sPickAvatar = true;
		for (LLWorld::region_list_t::const_iterator
				iter = gWorld.getRegionList().begin(),
				wend = gWorld.getRegionList().end();
			 iter != wend; ++iter)
		{
			LLViewerRegion* region = *iter;
			if (!region) continue;	// Paranoia

			LLSpatialPartition* part =
				region->getSpatialPartition(LLViewerRegion::PARTITION_AVATAR);
			// PARTITION_AVATAR cannot be NULL
			if (hasRenderType(part->mDrawableType))
			{
				LLDrawable* hit = part->lineSegmentIntersect(start, local_end,
															 pick_transparent,
															 pick_rigged,
															 face_hit,
															 &position,
															 tex_coord, normal,
															 tangent);
				if (hit)
				{
					LLVector4a delta;
					delta.setSub(position, local_end);

					if (!drawable || !drawable->getVObj()->isAttachment() ||
						delta.getLength3().getF32() > ATTACHMENT_OVERRIDE_DIST)
					{
						// Avatar overrides if previously hit drawable is not
						// an attachment or attachment is far enough away from
						// detected intersection
						drawable = hit;
						local_end = position;
					}
					else
					{
						// Prioritize attachments over avatars
						position = local_end;

						if (face_hit)
						{
							*face_hit = local_face_hit;
						}
						if (tex_coord)
						{
							*tex_coord = local_texcoord;
						}
						if (tangent)
						{
							*tangent = local_tangent;
						}
						if (normal)
						{
							*normal = local_normal;
						}
					}
				}
			}
		}
	}

	// Check all avatar nametags (silly, isn't it ?)
	for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
	{
		LLVOAvatar* av = (LLVOAvatar*)LLCharacter::sInstances[i];
		if (av && av->mNameText.notNull() &&
			av->mNameText->lineSegmentIntersect(start, local_end, position))
		{
			drawable = av->mDrawable;
			local_end = position;
		}
	}

	if (intersection)
	{
		*intersection = position;
	}

	return drawable ? drawable->getVObj().get() : NULL;
}

LLViewerObject* LLPipeline::lineSegmentIntersectInHUD(const LLVector4a& start,
													  const LLVector4a& end,
													  bool pick_transparent,
													  S32* face_hit,
													  LLVector4a* intersection,
													  LLVector2* tex_coord,
													  LLVector4a* normal,
													  LLVector4a* tangent)
{
	LLDrawable* drawable = NULL;

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			wend = gWorld.getRegionList().end();
		 iter != wend; ++iter)
	{
		LLViewerRegion* region = *iter;

		bool toggle = false;
		if (!hasRenderType(RENDER_TYPE_HUD))
		{
			toggleRenderType(RENDER_TYPE_HUD);
			toggle = true;
		}

		LLSpatialPartition* part =
			region->getSpatialPartition(LLViewerRegion::PARTITION_HUD);
		// PARTITION_HUD cannot be NULL
		LLDrawable* hit = part->lineSegmentIntersect(start, end,
													 pick_transparent, false,
													 face_hit, intersection,
													 tex_coord, normal,
													 tangent);
		if (hit)
		{
			drawable = hit;
		}

		if (toggle)
		{
			toggleRenderType(RENDER_TYPE_HUD);
		}
	}
	return drawable ? drawable->getVObj().get() : NULL;
}

LLSpatialPartition* LLPipeline::getSpatialPartition(LLViewerObject* vobj)
{
	if (vobj)
	{
		LLViewerRegion* region = vobj->getRegion();
		if (region)
		{
			return region->getSpatialPartition(vobj->getPartitionType());
		}
	}
	return NULL;
}

void LLPipeline::resetVertexBuffers(LLDrawable* drawable)
{
	if (drawable)
	{
		for (S32 i = 0, count = drawable->getNumFaces(); i < count; ++i)
		{
			LLFace* facep = drawable->getFace(i);
			if (facep)
			{
				facep->clearVertexBuffer();
			}
		}
	}
}

void LLPipeline::resetVertexBuffers()
{
	mResetVertexBuffers = true;
	updateRenderDeferred();
}

void LLPipeline::doResetVertexBuffers(bool forced)
{
	if (!mResetVertexBuffers)
	{
		return;
	}

	// Wait for teleporting to finish
	if (!forced && LLSpatialPartition::sTeleportRequested)
	{
		if (gAgent.getTeleportState() == LLAgent::TELEPORT_NONE)
		{
			// Teleporting aborted
			LLSpatialPartition::sTeleportRequested = false;
			mResetVertexBuffers = false;
		}
		return;
	}

	LL_FAST_TIMER(FTM_RESET_VB);
	mResetVertexBuffers = false;

	gGL.flush();
	glFinish();

	LLVertexBuffer::unbind();

	// Delete our utility buffers
	mDeferredVB = mGlowCombineVB = mCubeVB = NULL;

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* region = *iter;
		for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			llassert(part);
			part->resetVertexBuffers();
		}
	}

	if (LLSpatialPartition::sTeleportRequested)
	{
		LLSpatialPartition::sTeleportRequested = false;
		gWorld.clearAllVisibleObjects();
		clearRebuildDrawables();
	}

	resetDrawOrders();

	gSky.resetVertexBuffers();

	LLVOPartGroup::destroyGL();

	LLVertexBuffer::cleanupClass();

	if (LLVertexBuffer::sGLCount > 0)
	{
		llwarns << "VBO wipe failed: " << LLVertexBuffer::sGLCount
				<< " buffers remaining." << llendl;
	}

	LLVBOPool::deleteReleasedBuffers();

	sRenderBump = gSavedSettings.getBool("RenderObjectBump");
	LLVertexBuffer::sUseStreamDraw =
		gSavedSettings.getBool("RenderUseStreamVBO");
	LLVertexBuffer::sUseVAO = gSavedSettings.getBool("RenderUseVAO");
	LLVertexBuffer::sPreferStreamDraw =
		gSavedSettings.getBool("RenderPreferStreamDraw");
	LLVertexBuffer::sEnableVBOs = gSavedSettings.getBool("RenderVBOEnable");
	LLVertexBuffer::sDisableVBOMapping =
		LLVertexBuffer::sEnableVBOs &&
		gSavedSettings.getBool("RenderVBOMappingDisable");
	sBakeSunlight = gSavedSettings.getBool("RenderBakeSunlight");
	sNoAlpha = gSavedSettings.getBool("RenderNoAlpha");
	sTextureBindTest = gSavedSettings.getBool("RenderDebugTextureBind");

	updateRenderDeferred();

	LLVertexBuffer::initClass(LLVertexBuffer::sEnableVBOs,
							  LLVertexBuffer::sDisableVBOMapping);

	createAuxVBs();	// Recreate our utility buffers...

	LLVOPartGroup::restoreGL();

	// Reload the water and sky textures, depending on gUseNewShaders value
	LLDrawPoolWater::restoreGL();
	LLDrawPoolWLSky::restoreGL();
}

void LLPipeline::renderObjects(U32 type, U32 mask, bool texture,
							   bool batch_texture)
{
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
	mSimplePool->pushBatches(type, mask, texture, batch_texture);
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
}

void LLPipeline::renderMaskedObjects(U32 type, U32 mask, bool texture,
									 bool batch_texture)
{
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
	mAlphaMaskPool->pushMaskBatches(type, mask, texture, batch_texture);
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
}

void LLPipeline::renderFullbrightMaskedObjects(U32 type, U32 mask,
											   bool texture,
											   bool batch_texture)
{
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
	mFullbrightAlphaMaskPool->pushMaskBatches(type, mask, texture,
											  batch_texture);
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
}

void LLPipeline::bindScreenToTexture()
{
}

void LLPipeline::renderFinalize()
{
	LLVertexBuffer::unbind();
	LLGLState::check(true, true);

	if (gUseWireframe)
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	LLVector2 tc1;
	LLVector2 tc2((F32)(mScreen.getWidth() * 2),
				  (F32)(mScreen.getHeight() * 2));

	LL_FAST_TIMER(FTM_RENDER_BLOOM);
	gGL.color4f(1.f, 1.f, 1.f, 1.f);
	LLGLDepthTest depth(GL_FALSE);
	LLGLDisable blend(GL_BLEND);
	LLGLDisable cull(GL_CULL_FACE);

	enableLightsFullbright();

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	LLGLDisable test(GL_ALPHA_TEST);

	gGL.setColorMask(true, true);
	glClearColor(0.f, 0.f, 0.f, 0.f);

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	if (RenderGlow)
	{
		mGlow[2].bindTarget();
		mGlow[2].clear();

		gGlowExtractProgram.bind();
		gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_MIN_LUMINANCE,
									  RenderGlowMinLuminance);
		gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_MAX_EXTRACT_ALPHA,
									  RenderGlowMaxExtractAlpha);
		gGlowExtractProgram.uniform3f(LLShaderMgr::GLOW_LUM_WEIGHTS,
									  RenderGlowLumWeights.mV[0],
									  RenderGlowLumWeights.mV[1],
									  RenderGlowLumWeights.mV[2]);
		gGlowExtractProgram.uniform3f(LLShaderMgr::GLOW_WARMTH_WEIGHTS,
									  RenderGlowWarmthWeights.mV[0],
									  RenderGlowWarmthWeights.mV[1],
									  RenderGlowWarmthWeights.mV[2]);
		gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_WARMTH_AMOUNT,
									  RenderGlowWarmthAmount);
		{
			LLGLEnable blend_on(GL_BLEND);
			LLGLEnable test(GL_ALPHA_TEST);

			gGL.setSceneBlendType(LLRender::BT_ADD_WITH_ALPHA);

			if (gUseNewShaders)
			{
				mScreen.bindTexture(0, 0, LLTexUnit::TFO_POINT);
			}
			else
			{
				mScreen.bindTexture(0, 0);
			}

			gGL.color4f(1.f, 1.f, 1.f, 1.f);

			enableLightsFullbright();

			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
			gGL.vertex2f(-1.f, -1.f);

			gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
			gGL.vertex2f(-1.f, 3.f);

			gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
			gGL.vertex2f(3.f, -1.f);
 
			gGL.end();

			unit0->unbind(mScreen.getUsage());

			mGlow[2].flush();
		}

		tc1.set(0, 0);
		tc2.set(2, 2); 

		// Power of two between 1 and 1024
		const U32 glow_res = llclamp(1 << RenderGlowResolutionPow, 1, 1024);

		S32 kernel = RenderGlowIterations * 2;
		F32 delta = RenderGlowWidth / (F32)glow_res;
		// Use half the glow width if we have the res set to less than 9 so
		// that it looks almost the same in either case.
		if (RenderGlowResolutionPow < 9)
		{
			delta *= 0.5f;
		}

		gGlowProgram.bind();
		gGlowProgram.uniform1f(LLShaderMgr::GLOW_STRENGTH, RenderGlowStrength);

		for (S32 i = 0; i < kernel; ++i)
		{
			mGlow[i % 2].bindTarget();
			mGlow[i % 2].clear();

			if (i == 0)
			{
				unit0->bind(&mGlow[2]);
			}
			else
			{
				unit0->bind(&mGlow[(i - 1) % 2]);
			}

			if (i % 2 == 0)
			{
				gGlowProgram.uniform2f(LLShaderMgr::GLOW_DELTA, delta, 0);
			}
			else
			{
				gGlowProgram.uniform2f(LLShaderMgr::GLOW_DELTA, 0, delta);
			}

			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
			gGL.vertex2f(-1.f, -1.f);

			gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
			gGL.vertex2f(-1.f, 3.f);

			gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
			gGL.vertex2f(3.f, -1.f);

			gGL.end();

			mGlow[i % 2].flush();
		}

		gGlowProgram.unbind();
	}
	else	// Skip the glow ping-pong and just clear the result target
	{
		mGlow[1].bindTarget();
		mGlow[1].clear();
		mGlow[1].flush();
	}

	gViewerWindowp->setupViewport();

	tc2.set((F32)mScreen.getWidth(), (F32)mScreen.getHeight());

	gGL.flush();

	LLVertexBuffer::unbind();

	stop_glerror();

	if (sRenderDeferred)
	{
		bool dof_enabled = RenderDepthOfField &&
						   (RenderDepthOfFieldInEditMode ||
							!LLToolMgr::getInstance()->inBuildMode()) &&
						   !gViewerCamera.cameraUnderWater();

		bool multisample = RenderFSAASamples > 1 && mFXAABuffer.isComplete();

		if (dof_enabled)
		{
			LLGLSLShader* shader = &gDeferredPostProgram;
			LLGLDisable blend(GL_BLEND);

			// Depth of field focal plane calculations

			static F32 current_distance = 16.f;
			static F32 start_distance = 16.f;
			static F32 transition_time = 1.f;

			LLVector3 focus_point;
			LLViewerObject* obj =
				LLViewerMediaFocus::getInstance()->getFocusedObject();
			if (obj && obj->mDrawable && obj->isSelected())
			{
				// Focus on selected media object
				S32 face_idx = LLViewerMediaFocus::getInstance()->getFocusedFace();
				if (obj && obj->mDrawable)
				{
					LLFace* face = obj->mDrawable->getFace(face_idx);
					if (face)
					{
						focus_point = face->getPositionAgent();
					}
				}
			}
			if (focus_point.isExactlyZero())
			{
				if (LLViewerJoystick::getInstance()->getOverrideCamera())
				{
					// Focus on point under cursor
					focus_point.set(gDebugRaycastIntersection.getF32ptr());
				}
				else if (gAgent.cameraMouselook())
				{
					// Focus on point under mouselook crosshairs
					LLVector4a result;
					result.clear();
					gViewerWindowp->cursorIntersect(-1, -1, 512.f, NULL, -1,
													false, false, NULL,
													&result);
					focus_point.set(result.getF32ptr());
				}
				else
				{
					// Focus on alt-zoom target
					LLViewerRegion* region = gAgent.getRegion();
					if (region)
					{
						focus_point = LLVector3(gAgent.getFocusGlobal() -
												region->getOriginGlobal());
					}
				}
			}

			LLVector3 eye = gViewerCamera.getOrigin();
			F32 target_distance = 16.f;
			if (!focus_point.isExactlyZero())
			{
				target_distance = gViewerCamera.getAtAxis() *
								  (focus_point - eye);
			}

			if (transition_time >= 1.f &&
				fabsf(current_distance - target_distance) / current_distance >
					0.01f)
			{
				// Large shift happened, interpolate smoothly to new target
				// distance
				transition_time = 0.f;
				start_distance = current_distance;
			}
			else if (transition_time < 1.f)
			{
				// Currently in a transition, continue interpolating
				transition_time += 1.f / llmax(F32_MIN,
											   CameraFocusTransitionTime *
											   gFrameIntervalSeconds);
				transition_time = llmin(transition_time, 1.f);

				F32 t = cosf(transition_time * F_PI + F_PI) * 0.5f + 0.5f;
				current_distance = start_distance +
								   (target_distance - start_distance) * t;
			}
			else
			{
				// Small or no change, just snap to target distance
				current_distance = target_distance;
			}

			// Convert to mm
			F32 subject_distance = current_distance * 1000.f;
			F32 default_focal_length = CameraFocalLength;

			F32 fov = gViewerCamera.getView();
			const F32 default_fov = CameraFieldOfView * DEG_TO_RAD;
			F32 dv = 2.f * default_focal_length * tanf(default_fov * 0.5f);
			F32 focal_length = dv / (2.f * tanf(fov * 0.5f));

			// From wikipedia -- c = |s2-s1|/s2 * f^2/(N(S1-f))
			// where	 N = CameraFNumber
			//			 s2 = dot distance
			//			 s1 = subject distance
			//			 f = focal length
			//
			F32 blur_constant = focal_length * focal_length /
								(CameraFNumber * (subject_distance -
												  focal_length));
			blur_constant /= 1000.f; // convert to meters for shader

			F32 magnification;
			if (subject_distance == focal_length)
			{
				magnification = F32_MAX;
			}
			else
			{
				magnification = focal_length /
								(subject_distance - focal_length);
			}

			{
				// Build diffuse + bloom + CoF
				mDeferredLight.bindTarget();
				shader = &gDeferredCoFProgram;

				bindDeferredShader(*shader);

				S32 channel =
					shader->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
										  mScreen.getUsage());
				if (channel > -1)
				{
					mScreen.bindTexture(0, channel);
				}

				shader->uniform1f(LLShaderMgr::DOF_FOCAL_DISTANCE,
								  -subject_distance / 1000.f);
				shader->uniform1f(LLShaderMgr::DOF_BLUR_CONSTANT,
								  blur_constant);
				shader->uniform1f(LLShaderMgr::DOF_TAN_PIXEL_ANGLE,
								  tanf(1.f / LLDrawable::sCurPixelAngle));
				shader->uniform1f(LLShaderMgr::DOF_MAGNIFICATION,
								  magnification);
				shader->uniform1f(LLShaderMgr::DOF_MAX_COF, CameraMaxCoF);
				shader->uniform1f(LLShaderMgr::DOF_RES_SCALE,
								  CameraDoFResScale);

				gGL.begin(LLRender::TRIANGLE_STRIP);
				gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
				gGL.vertex2f(-1.f, -1.f);

				gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
				gGL.vertex2f(-1.f, 3.f);

				gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
				gGL.vertex2f(3.f, -1.f);
 
				gGL.end();

				stop_glerror();
				unbindDeferredShader(*shader);
				mDeferredLight.flush();
			}

			U32 dof_width = (U32)(mScreen.getWidth() * CameraDoFResScale);
			U32 dof_height = (U32)(mScreen.getHeight() * CameraDoFResScale);

			{
				// Perform DoF sampling at half-res (preserve alpha channel)
				mScreen.bindTarget();
				glViewport(0, 0, dof_width, dof_height);
				gGL.setColorMask(true, false);

				shader = &gDeferredPostProgram;
				bindDeferredShader(*shader);
				S32 channel =
					shader->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
										  mDeferredLight.getUsage());
				if (channel > -1)
				{
					mDeferredLight.bindTexture(0, channel);
				}

				shader->uniform1f(LLShaderMgr::DOF_MAX_COF, CameraMaxCoF);
				shader->uniform1f(LLShaderMgr::DOF_RES_SCALE,
								  CameraDoFResScale);

				gGL.begin(LLRender::TRIANGLE_STRIP);
				gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
				gGL.vertex2f(-1.f, -1.f);

				gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
				gGL.vertex2f(-1.f, 3.f);

				gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
				gGL.vertex2f(3.f, -1.f);
 
				gGL.end();

				stop_glerror();
				unbindDeferredShader(*shader);
				mScreen.flush();
				gGL.setColorMask(true, true);
			}

			{
				// Combine result based on alpha
				if (multisample)
				{
					mDeferredLight.bindTarget();
					glViewport(0, 0, mDeferredScreen.getWidth(),
							   mDeferredScreen.getHeight());
				}
				else
				{
					gViewerWindowp->setupViewport();
				}

				shader = &gDeferredDoFCombineProgram;
				bindDeferredShader(*shader);

				S32 channel =
					shader->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
										  mScreen.getUsage());
				if (channel > -1)
				{
					mScreen.bindTexture(0, channel);
					if (!gUseNewShaders)
					{
						gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
					}
				}

				shader->uniform1f(LLShaderMgr::DOF_MAX_COF, CameraMaxCoF);
				shader->uniform1f(LLShaderMgr::DOF_RES_SCALE,
								  CameraDoFResScale);
				shader->uniform1f(LLShaderMgr::DOF_WIDTH, dof_width - 1);
				shader->uniform1f(LLShaderMgr::DOF_HEIGHT, dof_height - 1);

				gGL.begin(LLRender::TRIANGLE_STRIP);
				gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
				gGL.vertex2f(-1.f, -1.f);

				gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
				gGL.vertex2f(-1.f, 3.f);

				gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
				gGL.vertex2f(3.f, -1.f);
 
				gGL.end();

				stop_glerror();
				unbindDeferredShader(*shader);

				if (multisample)
				{
					mDeferredLight.flush();
				}
			}
		}
		else
		{
			if (multisample)
			{
				mDeferredLight.bindTarget();
			}
			LLGLSLShader* shader = &gDeferredPostNoDoFProgram;

			bindDeferredShader(*shader);

			S32 channel = shader->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
												mScreen.getUsage());
			if (channel > -1)
			{
				mScreen.bindTexture(0, channel);
			}

			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
			gGL.vertex2f(-1.f, -1.f);

			gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
			gGL.vertex2f(-1.f, 3.f);

			gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
			gGL.vertex2f(3.f, -1.f);

			gGL.end();

			unbindDeferredShader(*shader);

			if (multisample)
			{
				mDeferredLight.flush();
			}
		}

		if (multisample)
		{
			// Bake out texture2D with RGBL for FXAA shader
			mFXAABuffer.bindTarget();

			S32 width = mScreen.getWidth();
			S32 height = mScreen.getHeight();
			glViewport(0, 0, width, height);

			LLGLSLShader* shader = &gGlowCombineFXAAProgram;

			shader->bind();
			shader->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, width, height);

			S32 channel = shader->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
												mDeferredLight.getUsage());
			if (channel > -1)
			{
				mDeferredLight.bindTexture(0, channel);
			}

			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.vertex2f(-1.f, -1.f);
			gGL.vertex2f(-1.f, 3.f);
			gGL.vertex2f(3.f, -1.f);
			gGL.end(true);

			shader->disableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
								   mDeferredLight.getUsage());
			shader->unbind();

			mFXAABuffer.flush();

			shader = &gFXAAProgram;
			shader->bind();

			channel = shader->enableTexture(LLShaderMgr::DIFFUSE_MAP,
											mFXAABuffer.getUsage());
			if (channel > -1)
			{
				if (gUseNewShaders)
				{
					mFXAABuffer.bindTexture(0, channel,
											LLTexUnit::TFO_BILINEAR);
				}
				else
				{
					mFXAABuffer.bindTexture(0, channel);
					gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
				}
			}

			gViewerWindowp->setupViewport();

			F32 scale_x = (F32)width / mFXAABuffer.getWidth();
			F32 scale_y = (F32)height / mFXAABuffer.getHeight();
			const F32 invertedwidth = 1.f / (F32)width;
			const F32 invertedheight = 1.f / (F32)height;
			shader->uniform2f(LLShaderMgr::FXAA_TC_SCALE, scale_x, scale_y);
			shader->uniform2f(LLShaderMgr::FXAA_RCP_SCREEN_RES,
							  invertedwidth * scale_x,
							  invertedheight * scale_y);
			shader->uniform4f(LLShaderMgr::FXAA_RCP_FRAME_OPT,
							  -0.5f * invertedwidth * scale_x,
							  -0.5f * invertedheight * scale_y,
							  0.5f * invertedwidth * scale_x,
							  0.5f * invertedheight * scale_y);
			shader->uniform4f(LLShaderMgr::FXAA_RCP_FRAME_OPT2,
							  -2.f * invertedwidth * scale_x,
							  -2.f * invertedheight * scale_y,
							  2.f * invertedwidth * scale_x,
							  2.f * invertedheight * scale_y);

			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.vertex2f(-1.f, -1.f);
			gGL.vertex2f(-1.f, 3.f);
			gGL.vertex2f(3.f, -1.f);
			gGL.end(true);
 
			shader->unbind();
		}
	}
	else
	{
		LLStrider<LLVector2> uv2;
		if (mGlowCombineVB.isNull() ||
			!mGlowCombineVB->getTexCoord1Strider(uv2))
		{
			return;
		}
		uv2[0].clear();
		uv2[1] = LLVector2(0.f, tc2.mV[1] * 2.f);
		uv2[2] = LLVector2(tc2.mV[0] * 2.f, 0.f);

		LLGLDisable blend(GL_BLEND);

		LLTexUnit* unit1 = gGL.getTexUnit(1);

		gGlowCombineProgram.bind();

		unit0->bind(&mGlow[1]);
		unit1->bind(&mScreen);

		LLGLEnable multisample(RenderFSAASamples > 0 ? GL_MULTISAMPLE_ARB : 0);

		mGlowCombineVB->setBuffer(AUX_VB_MASK);
		mGlowCombineVB->drawArrays(LLRender::TRIANGLE_STRIP, 0, 3);

		gGlowCombineProgram.unbind();
	}

	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	if (hasRenderDebugMask(RENDER_DEBUG_PHYSICS_SHAPES))
	{
		gSplatTextureRectProgram.bind();

		gGL.setColorMask(true, false);

		LLVector2 tc1;
		LLVector2 tc2((F32)gViewerWindowp->getWindowDisplayWidth() * 2,
					  (F32)gViewerWindowp->getWindowDisplayHeight() * 2);

		LLGLEnable blend(GL_BLEND);
		gGL.color4f(1.f, 1.f, 1.f, 0.75f);

		unit0->bind(&mPhysicsDisplay);

		gGL.getTexUnit(0)->bind(&mPhysicsDisplay);

		gGL.begin(LLRender::TRIANGLES);
		gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
		gGL.vertex2f(-1.f, -1.f);

		gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
		gGL.vertex2f(-1.f, 3.f);

		gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
		gGL.vertex2f(3.f, -1.f);

		gGL.end(true);

		gSplatTextureRectProgram.unbind();
	}

	if (LLRenderTarget::sUseFBO && mScreen.getFBO())
	{
		// Copy depth buffer from mScreen to framebuffer
		LLRenderTarget::copyContentsToFramebuffer(mScreen, 0, 0,
												  mScreen.getWidth(),
												  mScreen.getHeight(),
												  0, 0,
												  mScreen.getWidth(),
												  mScreen.getHeight(),
#if LL_NEW_DEPTH_STENCIL
												  GL_STENCIL_BUFFER_BIT |
#endif
												  GL_DEPTH_BUFFER_BIT,
												  GL_NEAREST);
	}

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	LLVertexBuffer::unbind();

	LLGLState::check(true, true);
}

void LLPipeline::bindDeferredShader(LLGLSLShader& shader)
{
	if (gUseNewShaders)
	{
		bindDeferredShaderEE(shader, NULL);
	}
	else
	{
		bindDeferredShaderWL(shader, 0);
	}
}

// Windlight renderer version
void LLPipeline::bindDeferredShaderWL(LLGLSLShader& shader, U32 light_index)
{
	LL_FAST_TIMER(FTM_BIND_DEFERRED);

	shader.bind();

	LLTexUnit::eTextureType usage = mDeferredScreen.getUsage();
	S32 channel = shader.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, usage);
	if (channel > -1)
	{
		mDeferredScreen.bindTexture(0, channel);
		gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_SPECULAR, usage);
	if (channel > -1)
	{
		mDeferredScreen.bindTexture(1, channel);
		gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_NORMAL, usage);
	if (channel > -1)
	{
		mDeferredScreen.bindTexture(2, channel);
		gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_DEPTH,
								   mDeferredDepth.getUsage());
	if (channel > -1)
	{
		gGL.getTexUnit(channel)->bind(&mDeferredDepth, true);

		LLMatrix4a inv_proj = gGLProjection;
		inv_proj.invert();
		shader.uniformMatrix4fv(LLShaderMgr::INVERSE_PROJECTION_MATRIX, 1,
								GL_FALSE, inv_proj.getF32ptr());
		shader.uniform4f(LLShaderMgr::VIEWPORT, (F32)gGLViewport[0],
												(F32)gGLViewport[1],
												(F32)gGLViewport[2],
												(F32)gGLViewport[3]);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_NOISE);
	if (channel > -1)
	{
		LLTexUnit* unit = gGL.getTexUnit(channel);
		unit->bindManual(LLTexUnit::TT_TEXTURE, mNoiseMap);
		unit->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_LIGHTFUNC);
	if (channel > -1)
	{
		gGL.getTexUnit(channel)->bindManual(LLTexUnit::TT_TEXTURE, mLightFunc);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_LIGHT,
								   mDeferredLight.getUsage());
	if (channel > -1)
	{
		if (light_index > 0)
		{
			mScreen.bindTexture(0, channel);
		}
		else
		{
			mDeferredLight.bindTexture(0, channel);
		}
		gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_BLOOM);
	if (channel > -1)
	{
		mGlow[1].bindTexture(0, channel);
	}

	if (shader.mFeatures.hasShadows)
	{
		for (U32 i = 0; i < 4; ++i)
		{
			channel = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i,
										   LLTexUnit::TT_TEXTURE);
			if (channel <= -1) continue;

			LLTexUnit* unit = gGL.getTexUnit(channel);
			unit->bind(&mShadow[i], true);
			unit->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
			unit->setTextureAddressMode(LLTexUnit::TAM_CLAMP);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB,
							GL_COMPARE_R_TO_TEXTURE_ARB);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
		}

		for (U32 i = 4; i < 6; ++i)
		{
			channel = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i);
			if (channel <= -1) continue;

			LLTexUnit* unit = gGL.getTexUnit(channel);
			unit->bind(&mShadow[i], true);
			unit->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
			unit->setTextureAddressMode(LLTexUnit::TAM_CLAMP);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB,
							GL_COMPARE_R_TO_TEXTURE_ARB);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
		}

		F32 mat[16 * 6];
		constexpr size_t total_bytes = sizeof(F32) * 16 * 6;
		memcpy((void*)mat, (void*)mSunShadowMatrix, total_bytes);
		shader.uniformMatrix4fv(LLShaderMgr::DEFERRED_SHADOW_MATRIX, 6,
								GL_FALSE, mat);

		shader.uniform4fv(LLShaderMgr::DEFERRED_SHADOW_CLIP, 1,
						  mSunClipPlanes.mV);
		shader.uniform2f(LLShaderMgr::DEFERRED_SHADOW_RES,
						 mShadow[0].getWidth(), mShadow[0].getHeight());
		shader.uniform2f(LLShaderMgr::DEFERRED_PROJ_SHADOW_RES,
						 mShadow[4].getWidth(), mShadow[4].getHeight());

		shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_OFFSET,
						 RenderDeferredSSAO ? RenderShadowOffset
											: RenderShadowOffsetNoSSAO);

		constexpr F32 ONEBYTHREETHOUSANDS = 1.f / 3000.f;
		F32 shadow_bias_error = RenderShadowBiasError * ONEBYTHREETHOUSANDS *
							fabsf(gViewerCamera.getOrigin().mV[2]);
		shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_BIAS,
						 RenderShadowBias + shadow_bias_error);

		shader.uniform1f(LLShaderMgr::DEFERRED_SPOT_SHADOW_OFFSET,
						 RenderSpotShadowOffset);
		shader.uniform1f(LLShaderMgr::DEFERRED_SPOT_SHADOW_BIAS,
						 RenderSpotShadowBias);
	}
#if DEBUG_OPTIMIZED_UNIFORMS
	else
	{
		if (shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW0) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_MATRIX) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_CLIP) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_RES) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_OFFSET) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_BIAS) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SPOT_SHADOW_OFFSET) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SPOT_SHADOW_BIAS) >= 0)
		{
			llwarns_once << "Shader: " << shader.mName
						 << " shall be marked as hasShadows !" << llendl;
		}
	}
#endif

	channel = shader.enableTexture(LLShaderMgr::ENVIRONMENT_MAP,
								   LLTexUnit::TT_CUBE_MAP);
	if (channel > -1)
	{
		LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
		if (cube_map)
		{
			cube_map->enableTexture(channel);
			cube_map->bind();
			const F32* m = gGLModelView.getF32ptr();

			F32 mat[] = { m[0], m[1], m[2],
						  m[4], m[5], m[6],
						  m[8], m[9], m[10] };

			shader.uniformMatrix3fv(LLShaderMgr::DEFERRED_ENV_MAT, 1, GL_TRUE,
									mat);
		}
	}

	shader.uniform1f(LLShaderMgr::DEFERRED_SUN_WASH, RenderDeferredSunWash);
	shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_NOISE, RenderShadowNoise);
	shader.uniform1f(LLShaderMgr::DEFERRED_BLUR_SIZE, RenderShadowBlurSize);

	if (shader.mFeatures.hasAmbientOcclusion)
	{
		shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_RADIUS, RenderSSAOScale);
		shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_MAX_RADIUS,
						 RenderSSAOMaxScale);
		shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_FACTOR, RenderSSAOFactor);
	}
#if DEBUG_OPTIMIZED_UNIFORMS
	else
	{
		if (shader.getUniformLocation(LLShaderMgr::DEFERRED_SSAO_RADIUS) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SSAO_MAX_RADIUS) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SSAO_FACTOR) >= 0)
		{
			llwarns_once << "Shader: " << shader.mName
						 << " shall be marked as hasAmbientOcclusion !"
						 << llendl;
		}
	}
#endif

	constexpr F32 ONETHIRD = 1.f / 3.f;
	F32 matrix_diag = (RenderSSAOEffect[0] + 2.f * RenderSSAOEffect[1]) *
					  ONETHIRD;
	F32 matrix_nondiag = (RenderSSAOEffect[0] - RenderSSAOEffect[1]) *
						 ONETHIRD;
	// This matrix scales (proj of color onto <1/rt(3),1/rt(3),1/rt(3)>) by
	// value factor, and scales remainder by saturation factor
	F32 ssao_effect_mat[] = {	matrix_diag, matrix_nondiag, matrix_nondiag,
								matrix_nondiag, matrix_diag, matrix_nondiag,
								matrix_nondiag, matrix_nondiag, matrix_diag };
	shader.uniformMatrix3fv(LLShaderMgr::DEFERRED_SSAO_EFFECT_MAT, 1, GL_FALSE,
							ssao_effect_mat);

	shader.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
					 mDeferredScreen.getWidth(), mDeferredScreen.getHeight());
	shader.uniform1f(LLShaderMgr::DEFERRED_NEAR_CLIP,
					 gViewerCamera.getNear() * 2.f);

	shader.uniform3fv(LLShaderMgr::DEFERRED_SUN_DIR, 1,
					  mTransformedSunDir.getF32ptr());

	if (shader.getUniformLocation(LLShaderMgr::DEFERRED_NORM_MATRIX) >= 0)
	{
		LLMatrix4a norm_mat = gGLModelView;
		norm_mat.invert();
		norm_mat.transpose();
		shader.uniformMatrix4fv(LLShaderMgr::DEFERRED_NORM_MATRIX, 1, GL_FALSE,
								norm_mat.getF32ptr());
	}

	stop_glerror();
}

// Extended environment renderer version
void LLPipeline::bindDeferredShaderEE(LLGLSLShader& shader,
									  LLRenderTarget* light_target)
{
	LL_FAST_TIMER(FTM_BIND_DEFERRED);

	shader.bind();

	LLTexUnit::eTextureType usage = mDeferredScreen.getUsage();
	S32 channel = shader.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, usage);
	if (channel > -1)
	{
		mDeferredScreen.bindTexture(0, channel, LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_SPECULAR, usage);
	if (channel > -1)
	{
		mDeferredScreen.bindTexture(1, channel, LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_NORMAL, usage);
	if (channel > -1)
	{
		mDeferredScreen.bindTexture(2, channel, LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_DEPTH,
								   mDeferredDepth.getUsage());
	if (channel > -1)
	{
		gGL.getTexUnit(channel)->bind(&mDeferredDepth, true);
		stop_glerror();
	}

	if (shader.getUniformLocation(LLShaderMgr::INVERSE_PROJECTION_MATRIX) != -1)
	{
		LLMatrix4a inv_proj = gGLProjection;
		inv_proj.invert();
		shader.uniformMatrix4fv(LLShaderMgr::INVERSE_PROJECTION_MATRIX, 1,
								GL_FALSE, inv_proj.getF32ptr());
	}
	if (shader.getUniformLocation(LLShaderMgr::VIEWPORT) != -1)
	{
		shader.uniform4f(LLShaderMgr::VIEWPORT, (F32)gGLViewport[0],
												(F32)gGLViewport[1],
												(F32)gGLViewport[2],
												(F32)gGLViewport[3]);
	}

	if (sReflectionRender &&
		!shader.getUniformLocation(LLShaderMgr::MODELVIEW_MATRIX))
	{
		shader.uniformMatrix4fv(LLShaderMgr::MODELVIEW_MATRIX, 1, GL_FALSE,
								mReflectionModelView.getF32ptr());
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_NOISE);
	if (channel > -1)
	{
		LLTexUnit* unit = gGL.getTexUnit(channel);
		unit->bindManual(LLTexUnit::TT_TEXTURE, mNoiseMap);
		unit->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_LIGHTFUNC);
	if (channel > -1)
	{
		gGL.getTexUnit(channel)->bindManual(LLTexUnit::TT_TEXTURE, mLightFunc);
	}

	stop_glerror();

	if (!light_target)
	{
		light_target = &mDeferredLight;
	}
	channel = shader.enableTexture(LLShaderMgr::DEFERRED_LIGHT,
								   light_target->getUsage());
	if (channel > -1)
	{
		light_target->bindTexture(0, channel, LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_BLOOM);
	if (channel > -1)
	{
		mGlow[1].bindTexture(0, channel);
	}

	stop_glerror();

	if (shader.mFeatures.hasShadows)
	{
		for (U32 i = 0; i < 4; ++i)
		{
			LLRenderTarget* shadow_target = &mShadow[i];
			if (!shadow_target) continue;

			channel = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i,
										   LLTexUnit::TT_TEXTURE);
			stop_glerror();
			if (channel <= -1) continue;

			LLTexUnit* unit = gGL.getTexUnit(channel);
			unit->bind(&mShadow[i], true);
			unit->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
			unit->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
			stop_glerror();

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB,
							GL_COMPARE_R_TO_TEXTURE_ARB);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
			stop_glerror();
		}

		for (U32 i = 4; i < 6; ++i)
		{
			channel = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i);
			stop_glerror();
			if (channel <= -1) continue;

			LLRenderTarget* shadow_target = &mShadow[i];
			if (!shadow_target) continue;

			LLTexUnit* unit = gGL.getTexUnit(channel);
			unit->bind(shadow_target, true);
			unit->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
			unit->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
			stop_glerror();

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB,
							GL_COMPARE_R_TO_TEXTURE_ARB);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
			stop_glerror();
		}

		stop_glerror();

		F32 mat[16 * 6];
		constexpr size_t total_bytes = sizeof(F32) * 16 * 6;
		memcpy((void*)mat, (void*)mSunShadowMatrix, total_bytes);
		shader.uniformMatrix4fv(LLShaderMgr::DEFERRED_SHADOW_MATRIX, 6,
								GL_FALSE, mat);

		stop_glerror();

		shader.uniform4fv(LLShaderMgr::DEFERRED_SHADOW_CLIP, 1,
						  mSunClipPlanes.mV);
		shader.uniform2f(LLShaderMgr::DEFERRED_SHADOW_RES,
						 mShadow[0].getWidth(), mShadow[0].getHeight());
		shader.uniform2f(LLShaderMgr::DEFERRED_PROJ_SHADOW_RES,
						 mShadow[4].getWidth(), mShadow[4].getHeight());

		shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_OFFSET,
						 RenderDeferredSSAO ? RenderShadowOffset
											: RenderShadowOffsetNoSSAO);

		constexpr F32 ONEBYTHREETHOUSANDS = 1.f / 3000.f;
		F32 shadow_bias_error = RenderShadowBiasError * ONEBYTHREETHOUSANDS *
							fabsf(gViewerCamera.getOrigin().mV[2]);
		shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_BIAS,
						 RenderShadowBias + shadow_bias_error);

		shader.uniform1f(LLShaderMgr::DEFERRED_SPOT_SHADOW_OFFSET,
						 RenderSpotShadowOffset);
		shader.uniform1f(LLShaderMgr::DEFERRED_SPOT_SHADOW_BIAS,
						 RenderSpotShadowBias);
	}
#if DEBUG_OPTIMIZED_UNIFORMS
	else
	{
		if (shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW0) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_MATRIX) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_CLIP) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_RES) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_OFFSET) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_BIAS) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SPOT_SHADOW_OFFSET) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SPOT_SHADOW_BIAS) >= 0)
		{
			llwarns_once << "Shader: " << shader.mName
						 << " shall be marked as hasShadows !" << llendl;
		}
	}
#endif

	channel = shader.enableTexture(LLShaderMgr::ENVIRONMENT_MAP,
								   LLTexUnit::TT_CUBE_MAP);
	if (channel > -1)
	{
		LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
		if (cube_map)
		{
			cube_map->enableTexture(channel);
			cube_map->bind();
			const F32* m = gGLModelView.getF32ptr();

			F32 mat[] = { m[0], m[1], m[2],
						  m[4], m[5], m[6],
						  m[8], m[9], m[10] };

			shader.uniformMatrix3fv(LLShaderMgr::DEFERRED_ENV_MAT, 1, GL_TRUE,
									mat);
		}
	}

	shader.uniform1f(LLShaderMgr::DEFERRED_SUN_WASH, RenderDeferredSunWash);
	shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_NOISE, RenderShadowNoise);
	shader.uniform1f(LLShaderMgr::DEFERRED_BLUR_SIZE, RenderShadowBlurSize);

	if (shader.mFeatures.hasAmbientOcclusion)
	{
		shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_RADIUS, RenderSSAOScale);
		shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_MAX_RADIUS,
						 RenderSSAOMaxScale);
		shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_FACTOR, RenderSSAOFactor);
	}
#if DEBUG_OPTIMIZED_UNIFORMS
	else
	{
		if (shader.getUniformLocation(LLShaderMgr::DEFERRED_SSAO_RADIUS) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SSAO_MAX_RADIUS) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::DEFERRED_SSAO_FACTOR) >= 0)
		{
			llwarns_once << "Shader: " << shader.mName
						 << " shall be marked as hasAmbientOcclusion !"
						 << llendl;
		}
	}
#endif

	constexpr F32 ONETHIRD = 1.f / 3.f;
	F32 matrix_diag = (RenderSSAOEffect[0] + 2.f * RenderSSAOEffect[1]) *
					  ONETHIRD;
	F32 matrix_nondiag = (RenderSSAOEffect[0] - RenderSSAOEffect[1]) *
						 ONETHIRD;
	// This matrix scales (proj of color onto <1/rt(3),1/rt(3),1/rt(3)>) by
	// value factor, and scales remainder by saturation factor
	F32 ssao_effect_mat[] = {	matrix_diag, matrix_nondiag, matrix_nondiag,
								matrix_nondiag, matrix_diag, matrix_nondiag,
								matrix_nondiag, matrix_nondiag, matrix_diag };
	shader.uniformMatrix3fv(LLShaderMgr::DEFERRED_SSAO_EFFECT_MAT, 1, GL_FALSE,
							ssao_effect_mat);

	shader.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
					 mDeferredScreen.getWidth(),
					 mDeferredScreen.getHeight());
	shader.uniform1f(LLShaderMgr::DEFERRED_NEAR_CLIP,
					 gViewerCamera.getNear() * 2.f);

	shader.uniform3fv(LLShaderMgr::DEFERRED_SUN_DIR, 1,
					  mTransformedSunDir.getF32ptr());
	shader.uniform3fv(LLShaderMgr::DEFERRED_MOON_DIR, 1,
					  mTransformedMoonDir.getF32ptr());

	if (shader.getUniformLocation(LLShaderMgr::DEFERRED_NORM_MATRIX) >= 0)
	{
		LLMatrix4a norm_mat = gGLModelView;
		norm_mat.invert();
		norm_mat.transpose();
		shader.uniformMatrix4fv(LLShaderMgr::DEFERRED_NORM_MATRIX, 1, GL_FALSE,
								norm_mat.getF32ptr());
	}

	shader.uniform4fv(LLShaderMgr::SUNLIGHT_COLOR, 1, mSunDiffuse.mV);
	shader.uniform4fv(LLShaderMgr::MOONLIGHT_COLOR, 1, mMoonDiffuse.mV);

	gEnvironment.updateShaderSkyUniforms(&shader);
}

void LLPipeline::renderDeferredLighting()
{
	if (!sCull)
	{
		return;
	}

	{
		LL_FAST_TIMER(FTM_RENDER_DEFERRED);

		{
			LLGLDepthTest depth(GL_TRUE);
			mDeferredDepth.copyContents(mDeferredScreen, 0, 0,
										mDeferredScreen.getWidth(),
										mDeferredScreen.getHeight(),
										0, 0, mDeferredDepth.getWidth(),
										mDeferredDepth.getHeight(),
										GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		}

		LLGLEnable multisample(RenderFSAASamples > 0 ? GL_MULTISAMPLE_ARB : 0);

		if (hasRenderType(RENDER_TYPE_HUD))
		{
			toggleRenderType(RENDER_TYPE_HUD);
		}

		// ATI does not seem to love actually using the stencil buffer on FBOs
		LLGLDisable stencil(GL_STENCIL_TEST);
#if 0
		glStencilFunc(GL_EQUAL, 1, 0xFFFFFFFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
#endif
		gGL.setColorMask(true, true);

		// Draw a cube around every light
		LLVertexBuffer::unbind();

		LLGLEnable cull(GL_CULL_FACE);
		LLGLEnable blend(GL_BLEND);

		LLStrider<LLVector3> vert; 
		if (mDeferredVB.isNull() || !mDeferredVB->getVertexStrider(vert))
		{
			return;
		}
		vert[0].set(-1.f, 1.f, 0.f);
		vert[1].set(-1.f, -3.f, 0.f);
		vert[2].set(3.f, 1.f, 0.f);

		setupHWLights(NULL);	// To set mSunDir/mMoonDir;
		mTransformedSunDir.loadua(mSunDir.mV);
		gGLModelView.rotate(mTransformedSunDir, mTransformedSunDir);
		mTransformedMoonDir.loadua(mMoonDir.mV);
		gGLModelView.rotate(mTransformedMoonDir, mTransformedMoonDir);

		gGL.pushMatrix();
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();
		gGL.loadIdentity();

		if (RenderDeferredSSAO || RenderShadowDetail > 0)
		{
			mDeferredLight.bindTarget();
			{
				// Paint shadow/SSAO light map (direct lighting lightmap)
				LL_FAST_TIMER(FTM_SUN_SHADOW);
				if (gUseNewShaders)
				{
					bindDeferredShaderEE(gDeferredSunProgram, &mDeferredLight);
				}
				else
				{
					bindDeferredShaderWL(gDeferredSunProgram, (U32)0);
				}
				mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
				glClearColor(1.f, 1.f, 1.f, 1.f);
				mDeferredLight.clear(GL_COLOR_BUFFER_BIT);
				glClearColor(0.f, 0.f, 0.f, 0.f);

				gDeferredSunProgram.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
											  mDeferredLight.getWidth(),
											  mDeferredLight.getHeight());

				{
					LLGLDisable blend(GL_BLEND);
					LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
					stop_glerror();
					mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);
				}

				stop_glerror();

				unbindDeferredShader(gDeferredSunProgram);
			}
			mDeferredLight.flush();
		}

		if (RenderDeferredSSAO)
		{
			// Soften direct lighting lightmap

			LL_FAST_TIMER(FTM_SOFTEN_SHADOW);

			// Blur lightmap
			mScreen.bindTarget();
			glClearColor(1.f, 1.f, 1.f, 1.f);
			mScreen.clear(GL_COLOR_BUFFER_BIT);
			glClearColor(0.f, 0.f, 0.f, 0.f);

			bindDeferredShader(gDeferredBlurLightProgram);
			mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
			LLVector3 go = RenderShadowGaussian;
			F32 blur_size = RenderShadowBlurSize;
			F32 dist_factor = RenderShadowBlurDistFactor;

			// Sample symmetrically with the middle sample falling exactly on
			// 0.0
			F32 x = 0.f;
			constexpr U32 kern_length = 4;
			LLVector3 gauss[kern_length]; // xweight, yweight, offset
			for (U32 i = 0; i < kern_length; ++i)
			{
				gauss[i].mV[0] = llgaussian(x, go.mV[0]);
				gauss[i].mV[1] = llgaussian(x, go.mV[1]);
				gauss[i].mV[2] = x;
				x += 1.f;
			}

			gDeferredBlurLightProgram.uniform2f(sDelta, 1.f, 0.f);
			gDeferredBlurLightProgram.uniform1f(sDistFactor, dist_factor);
			gDeferredBlurLightProgram.uniform3fv(sKern, kern_length,
												 gauss[0].mV);
			gDeferredBlurLightProgram.uniform1f(sKernScale,
												blur_size *
												(kern_length * 0.5f - 0.5f));

			{
				LLGLDisable blend(GL_BLEND);
				LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
				stop_glerror();
				mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);
			}

			mScreen.flush();
			stop_glerror();
			unbindDeferredShader(gDeferredBlurLightProgram);

			if (gUseNewShaders)
			{
				bindDeferredShaderEE(gDeferredBlurLightProgram, &mScreen);
			}
			else
			{
				bindDeferredShaderWL(gDeferredBlurLightProgram, 1);
			}
			mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
			mDeferredLight.bindTarget();

			gDeferredBlurLightProgram.uniform2f(sDelta, 0.f, 1.f);

			{
				LLGLDisable blend(GL_BLEND);
				LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
				stop_glerror();
				mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);
			}
			mDeferredLight.flush();
			stop_glerror();
			unbindDeferredShader(gDeferredBlurLightProgram);
		}

		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.popMatrix();

		mScreen.bindTarget();
		// Clear color buffer here; zeroing alpha (glow) is important or it
		// will accumulate against sky
		glClearColor(0.f, 0.f, 0.f, 0.f);
		mScreen.clear(GL_COLOR_BUFFER_BIT);

		if (RenderDeferredAtmospheric)
		{
			// Apply sunlight contribution

			LL_FAST_TIMER(FTM_ATMOSPHERICS);

			LLGLSLShader& soften_shader =
				sUnderWaterRender ? gDeferredSoftenWaterProgram
								  : gDeferredSoftenProgram;
			bindDeferredShader(soften_shader);

			if (gUseNewShaders)
			{
				soften_shader.uniform1i(LLShaderMgr::SUN_UP_FACTOR,
										mIsSunUp ? 1 : 0);
				soften_shader.uniform4fv(LLShaderMgr::LIGHTNORM, 1,
										 gEnvironment.getClampedLightNorm().mV);
			}

			{
				LLGLDepthTest depth(GL_FALSE);
				LLGLDisable blend(GL_BLEND);
				LLGLDisable test(GL_ALPHA_TEST);

				// Full screen blit
				gGL.pushMatrix();
				gGL.loadIdentity();
				gGL.matrixMode(LLRender::MM_PROJECTION);
				gGL.pushMatrix();
				gGL.loadIdentity();

				mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
				mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);

				gGL.popMatrix();
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				gGL.popMatrix();
			}

			stop_glerror();
			unbindDeferredShader(soften_shader);
		}

		{
			// Render non-deferred geometry (fullbright, alpha, etc)
			LLGLDisable blend(GL_BLEND);
			LLGLDisable stencil(GL_STENCIL_TEST);
			gGL.setSceneBlendType(LLRender::BT_ALPHA);

			pushRenderTypeMask();

			andRenderTypeMask(RENDER_TYPE_SKY,
							  RENDER_TYPE_CLOUDS,
							  RENDER_TYPE_WL_SKY,
							  END_RENDER_TYPES);

			renderGeomPostDeferred(gViewerCamera, false);
			popRenderTypeMask();
		}

		bool render_local = RenderLightingDetail > 0;
		if (render_local)
		{
			gGL.setSceneBlendType(LLRender::BT_ADD);
			std::list<LLVector4> fullscreen_lights, light_colors;
			LLDrawable::draw_list_t spot_lights, fullscreen_spot_lights;

			for (U32 i = 0; i < 2; ++i)
			{
				mTargetShadowSpotLight[i] = NULL;
			}

			LLVertexBuffer::unbind();

			{
				bindDeferredShader(gDeferredLightProgram);

				if (mCubeVB.notNull())
				{
					mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
				}

				const LLVector3& cam_origin = gViewerCamera.getOrigin();
				LLGLDepthTest depth(GL_TRUE, GL_FALSE);
				LLVector4a center, sa;
				LLColor3 col;
				for (LLDrawable::draw_set_t::iterator iter = mLights.begin(),
													  end = mLights.end();
					 iter != end; ++iter)
				{
					LLDrawable* drawablep = *iter;

					LLVOVolume* volume = drawablep->getVOVolume();
					if (!volume)
					{
						continue;
					}

					bool is_attachment = volume->isAttachment();
					if (is_attachment && !sRenderAttachedLights)
					{
						continue;
					}

					const LLViewerObject* vobj = drawablep->getVObj().get();
					if (vobj)
					{
						LLVOAvatar* av = is_attachment ? vobj->getAvatar()
													   : NULL;
						if (av && !av->isSelf() &&
							(av->isInMuteList() || av->isTooComplex()))
						{
							continue;
						}
					}

					const LLVector3 position = drawablep->getPositionAgent();
					if (dist_vec(position, cam_origin) >
							RenderFarClip + volume->getLightRadius())
					{
						continue;
					}

					center.load3(position.mV);
					const F32* c = center.getF32ptr();
					F32 s = volume->getLightRadius() * 1.5f;

					// Send light color to shader in linear space
					col = volume->getLightLinearColor();
					if (col.lengthSquared() < 0.001f)
					{
						continue;
					}

					if (s <= 0.001f)
					{
						continue;
					}

					sa.splat(s);
					if (gViewerCamera.AABBInFrustumNoFarClip(center, sa) == 0)
					{
						continue;
					}

					++sVisibleLightCount;

					if (cam_origin.mV[0] > c[0] + s + 0.2f ||
						cam_origin.mV[0] < c[0] - s - 0.2f ||
						cam_origin.mV[1] > c[1] + s + 0.2f ||
						cam_origin.mV[1] < c[1] - s - 0.2f ||
						cam_origin.mV[2] > c[2] + s + 0.2f ||
						cam_origin.mV[2] < c[2] - s - 0.2f)
					{
						// Draw box if camera is outside box
						if (render_local && mCubeVB.notNull())
						{
							if (volume->isLightSpotlight())
							{
								drawablep->getVOVolume()->updateSpotLightPriority();
								spot_lights.emplace_back(drawablep);
								continue;
							}

							LL_FAST_TIMER(FTM_LOCAL_LIGHTS);
							gDeferredLightProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER,
															 1, c);
							gDeferredLightProgram.uniform1f(LLShaderMgr::LIGHT_SIZE, s);
							gDeferredLightProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR,
															 1, col.mV);
							gDeferredLightProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF,
															volume->getLightFalloff(0.5f));
							gGL.syncMatrices();

							mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8,
											   get_box_fan_indices(&gViewerCamera,
																   center));
						}
					}
					else
					{
						if (volume->isLightSpotlight())
						{
							drawablep->getVOVolume()->updateSpotLightPriority();
							fullscreen_spot_lights.emplace_back(drawablep);
							continue;
						}

						gGLModelView.affineTransform(center, center);
						LLVector4 tc(center.getF32ptr());
						tc.mV[VW] = s;
						fullscreen_lights.emplace_back(tc);

						light_colors.emplace_back(col.mV[0], col.mV[1],
												  col.mV[2],
												  volume->getLightFalloff(0.5f));
					}
				}
				stop_glerror();

				// When editting appearance, we need to add the corresponding
				// light at the camera position.
				static LLCachedControl<bool> custlight(gSavedSettings,
													   "AvatarCustomizeLighting");
				if (custlight && isAgentAvatarValid() &&
					gAgentAvatarp->mSpecialRenderMode == 3)
				{
					fullscreen_lights.emplace_back(0.f, 0.f, 0.f, 15.f);
					light_colors.emplace_back(1.f, 1.f, 1.f, 0.f);
				}

				unbindDeferredShader(gDeferredLightProgram);
			}

			if (!spot_lights.empty() && mCubeVB.notNull())
			{
				LL_FAST_TIMER(FTM_PROJECTORS);

				LLGLDepthTest depth(GL_TRUE, GL_FALSE);
				bindDeferredShader(gDeferredSpotLightProgram);

				mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);

				gDeferredSpotLightProgram.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

				for (LLDrawable::draw_list_t::iterator
						iter = spot_lights.begin(), end = spot_lights.end();
					 iter != end; ++iter)
				{
					LLDrawable* drawablep = *iter;

					LLVOVolume* volume = drawablep->getVOVolume();

					LLVector4a center;
					center.load3(drawablep->getPositionAgent().mV);
					const F32* c = center.getF32ptr();
					F32 s = volume->getLightRadius() * 1.5f;

					++sVisibleLightCount;

					setupSpotLight(gDeferredSpotLightProgram, drawablep);

					LLColor3 col = volume->getLightLinearColor();
					gDeferredSpotLightProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER,
														 1, c);
					gDeferredSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_SIZE,
														s);
					gDeferredSpotLightProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR,
														 1, col.mV);
					gDeferredSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF,
														volume->getLightFalloff(0.5f));
					gGL.syncMatrices();

					mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8,
									   get_box_fan_indices(&gViewerCamera,
														   center));
				}
				gDeferredSpotLightProgram.disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
				stop_glerror();
				unbindDeferredShader(gDeferredSpotLightProgram);
			}

			// Reset mDeferredVB to fullscreen triangle
			if (!mDeferredVB->getVertexStrider(vert))
			{
				return;
			}
			vert[0].set(-1.f, 1.f, 0.f);
			vert[1].set(-1.f, -3.f, 0.f);
			vert[2].set(3.f, 1.f, 0.f);

			{
				LLGLDepthTest depth(GL_FALSE);

				// Full screen blit
				gGL.pushMatrix();
				gGL.loadIdentity();
				gGL.matrixMode(LLRender::MM_PROJECTION);
				gGL.pushMatrix();
				gGL.loadIdentity();

				constexpr U32 max_count = LL_DEFERRED_MULTI_LIGHT_COUNT;
				LLVector4 light[max_count];
				LLVector4 col[max_count];
				U32 count = 0;
				F32 far_z = 0.f;

				{
					LL_FAST_TIMER(FTM_FULLSCREEN_LIGHTS);
					while (count < max_count && !fullscreen_lights.empty())
					{
						light[count] = fullscreen_lights.front();
						fullscreen_lights.pop_front();
						col[count] = light_colors.front();
						light_colors.pop_front();
						far_z = llmin(light[count].mV[2] - light[count].mV[3],
									  far_z);

						if (++count == max_count || fullscreen_lights.empty())
						{
							U32 idx = count - 1;
							bindDeferredShader(gDeferredMultiLightProgram[idx]);
							gDeferredMultiLightProgram[idx].uniform1i(LLShaderMgr::MULTI_LIGHT_COUNT,
																	  count);
							gDeferredMultiLightProgram[idx].uniform4fv(LLShaderMgr::MULTI_LIGHT,
																	   count,
																	   (F32*)light);
							gDeferredMultiLightProgram[idx].uniform4fv(LLShaderMgr::MULTI_LIGHT_COL,
																	   count,
																	   (F32*)col);
							gDeferredMultiLightProgram[idx].uniform1f(LLShaderMgr::MULTI_LIGHT_FAR_Z,
																	  far_z);
							mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
							mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);
							stop_glerror();
							unbindDeferredShader(gDeferredMultiLightProgram[idx]);
							far_z = 0.f;
							count = 0;
						}
					}
				}

				bindDeferredShader(gDeferredMultiSpotLightProgram);

				gDeferredMultiSpotLightProgram.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

				mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);

				{
					LL_FAST_TIMER(FTM_PROJECTORS);
					for (LLDrawable::draw_list_t::iterator
							iter = fullscreen_spot_lights.begin(),
							end = fullscreen_spot_lights.end();
						 iter != end; ++iter)
					{
						LLDrawable* drawablep = *iter;

						LLVOVolume* volume = drawablep->getVOVolume();

						LLVector4a center;
						center.load3(drawablep->getPositionAgent().mV);
						F32 s = volume->getLightRadius() * 1.5f;

						++sVisibleLightCount;

						gGLModelView.affineTransform(center, center);

						setupSpotLight(gDeferredMultiSpotLightProgram,
									   drawablep);

						LLColor3 col = volume->getLightLinearColor();
						gDeferredMultiSpotLightProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER,
																  1,
																  center.getF32ptr());
						gDeferredMultiSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_SIZE,
																 s);
						gDeferredMultiSpotLightProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR,
																  1, col.mV);
						gDeferredMultiSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF,
																 volume->getLightFalloff(0.5f));
						mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);
					}
				}

				gDeferredMultiSpotLightProgram.disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
				stop_glerror();
				unbindDeferredShader(gDeferredMultiSpotLightProgram);

				gGL.popMatrix();
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				gGL.popMatrix();
			}
			gGL.setSceneBlendType(LLRender::BT_ALPHA);
		}

		gGL.setColorMask(true, true);
	}

	mScreen.flush();

	// Gamma-correct lighting

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	{
		LLGLDepthTest depth(GL_FALSE, GL_FALSE);

		LLVector2 tc1;
		LLVector2 tc2((F32)(mScreen.getWidth() * 2),
					  (F32)(mScreen.getHeight() * 2));

		mScreen.bindTarget();
		// Apply gamma correction to the frame here.
		gDeferredPostGammaCorrectProgram.bind();
		S32 channel =
			gDeferredPostGammaCorrectProgram.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
														   mScreen.getUsage());
		if (channel > -1)
		{
			if (gUseNewShaders)
			{
				mScreen.bindTexture(0, channel, LLTexUnit::TFO_POINT);
			}
			else
			{
				mScreen.bindTexture(0, channel);
				gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
			}
		}

		gDeferredPostGammaCorrectProgram.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
												   mScreen.getWidth(),
												   mScreen.getHeight());

		gDeferredPostGammaCorrectProgram.uniform1f(LLShaderMgr::DISPLAY_GAMMA,
												   1.f / RenderDeferredDisplayGamma);

		gGL.begin(LLRender::TRIANGLE_STRIP);
		gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
		gGL.vertex2f(-1.f, -1.f);

		gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
		gGL.vertex2f(-1.f, 3.f);

		gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
		gGL.vertex2f(3.f, -1.f);

		gGL.end();

		if (channel > -1)
		{
			gGL.getTexUnit(channel)->unbind(mScreen.getUsage());
		}
		gDeferredPostGammaCorrectProgram.unbind();
		mScreen.flush();
		stop_glerror();
	}

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	mScreen.bindTarget();

	{
		// Render non-deferred geometry (alpha, fullbright, glow)
		LLGLDisable blend(GL_BLEND);
		LLGLDisable stencil(GL_STENCIL_TEST);

		pushRenderTypeMask();
		andRenderTypeMask(RENDER_TYPE_ALPHA,
						  RENDER_TYPE_FULLBRIGHT,
						  RENDER_TYPE_VOLUME,
						  RENDER_TYPE_GLOW,
						  RENDER_TYPE_BUMP,
						  RENDER_TYPE_PASS_SIMPLE,
						  RENDER_TYPE_PASS_ALPHA,
						  RENDER_TYPE_PASS_ALPHA_MASK,
						  RENDER_TYPE_PASS_BUMP,
						  RENDER_TYPE_PASS_POST_BUMP,
						  RENDER_TYPE_PASS_FULLBRIGHT,
						  RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
						  RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
						  RENDER_TYPE_PASS_GLOW,
						  RENDER_TYPE_PASS_GRASS,
						  RENDER_TYPE_PASS_SHINY,
						  RENDER_TYPE_PASS_INVISIBLE,
						  RENDER_TYPE_PASS_INVISI_SHINY,
						  RENDER_TYPE_AVATAR,
						  RENDER_TYPE_PUPPET,
						  RENDER_TYPE_ALPHA_MASK,
						  RENDER_TYPE_FULLBRIGHT_ALPHA_MASK,
						  END_RENDER_TYPES);

		renderGeomPostDeferred(gViewerCamera);
		popRenderTypeMask();
	}

	{
		// Render highlights, etc.
		renderHighlights();
		mHighlightFaces.clear();

		renderDebug();

		LLVertexBuffer::unbind();

		if (hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_UI))
		{
			// Render debugging beacons.
			gObjectList.renderObjectBeacons();
			gObjectList.resetObjectBeacons();
			gSky.addSunMoonBeacons();
		}
	}

	mScreen.flush();
}

void LLPipeline::setupSpotLight(LLGLSLShader& shader, LLDrawable* drawablep)
{
	// Construct frustum
	LLVOVolume* volume = drawablep->getVOVolume();
	LLVector3 params = volume->getSpotLightParams();

	F32 fov = params.mV[0];
	F32 focus = params.mV[1];

	LLVector3 pos = drawablep->getPositionAgent();
	LLQuaternion quat = volume->getRenderRotation();
	LLVector3 scale = volume->getScale();

	// Get near clip plane
	LLVector3 at_axis(0.f, 0.f, -scale.mV[2] * 0.5f);
	at_axis *= quat;

	LLVector3 np = pos + at_axis;
	at_axis.normalize();

	// Get origin that has given fov for plane np, at_axis, and given scale
	F32 dist = scale.mV[1] * 0.5f / tanf(fov * 0.5f);

	LLVector3 origin = np - at_axis * dist;

	// Matrix from volume space to agent space
	LLMatrix4 light_mat4(quat, LLVector4(origin,1.f));
	LLMatrix4a light_mat;
	light_mat.loadu(light_mat4.getF32ptr());
	LLMatrix4a light_to_screen;
	light_to_screen.setMul(gGLModelView, light_mat);
	LLMatrix4a screen_to_light = light_to_screen;
	screen_to_light.invert();

	F32 s = volume->getLightRadius() * 1.5f;
	F32 near_clip = dist;
	F32 width = scale.mV[VX];
	F32 height = scale.mV[VY];
	F32 far_clip = s + dist - scale.mV[VZ];

	F32 fovy = fov * RAD_TO_DEG;
	F32 aspect = width / height;

	LLVector4a p1(0.f, 0.f, -(near_clip + 0.01f));
	LLVector4a p2(0.f, 0.f, -(near_clip + 1.f));

	LLVector4a screen_origin;
	screen_origin.clear();

	light_to_screen.affineTransform(p1, p1);
	light_to_screen.affineTransform(p2, p2);
	light_to_screen.affineTransform(screen_origin, screen_origin);

	LLVector4a n;
	n.setSub(p2, p1);
	n.normalize3fast();

	F32 proj_range = far_clip - near_clip;
	LLMatrix4a light_proj = gl_perspective(fovy, aspect, near_clip, far_clip);
	light_proj.setMul(TRANS_MAT, light_proj);
	screen_to_light.setMul(light_proj, screen_to_light);

	shader.uniformMatrix4fv(LLShaderMgr::PROJECTOR_MATRIX, 1, GL_FALSE,
							screen_to_light.getF32ptr());
	shader.uniform3fv(LLShaderMgr::PROJECTOR_P, 1, p1.getF32ptr());
	shader.uniform3fv(LLShaderMgr::PROJECTOR_N, 1, n.getF32ptr());
	shader.uniform3fv(LLShaderMgr::PROJECTOR_ORIGIN, 1,
					  screen_origin.getF32ptr());
	shader.uniform1f(LLShaderMgr::PROJECTOR_RANGE, proj_range);
	shader.uniform1f(LLShaderMgr::PROJECTOR_AMBIANCE, params.mV[2]);

	if (shader.mFeatures.hasShadows)
	{
		S32 s_idx = -1;
		for (U32 i = 0; i < 2; ++i)
		{
			if (mShadowSpotLight[i] == drawablep)
			{
				s_idx = i;
			}
		}

		shader.uniform1i(LLShaderMgr::PROJECTOR_SHADOW_INDEX, s_idx);

		if (s_idx >= 0)
		{
			shader.uniform1f(LLShaderMgr::PROJECTOR_SHADOW_FADE,
							 1.f - mSpotLightFade[s_idx]);
		}
		else
		{
			shader.uniform1f(LLShaderMgr::PROJECTOR_SHADOW_FADE, 1.f);
		}

		LLDrawable* potential = drawablep;
		// Determine if this is a good light for casting shadows
		F32 m_pri = volume->getSpotLightPriority();

		for (U32 i = 0; i < 2; ++i)
		{
			F32 pri = 0.f;
			LLDrawable* slightp = mTargetShadowSpotLight[i].get();
			if (slightp)
			{
				pri = slightp->getVOVolume()->getSpotLightPriority();
			}
			if (m_pri > pri)
			{
				LLDrawable* temp = mTargetShadowSpotLight[i];
				mTargetShadowSpotLight[i] = potential;
				potential = temp;
				m_pri = pri;
			}
		}
	}
#if DEBUG_OPTIMIZED_UNIFORMS
	else
	{
		if (shader.getUniformLocation(LLShaderMgr::PROJECTOR_SHADOW_INDEX) >= 0 ||
			shader.getUniformLocation(LLShaderMgr::PROJECTOR_SHADOW_FADE) >= 0)
		{
			llwarns_once << "Shader: " << shader.mName
						 << " shall be marked as hasShadows !" << llendl;
		}
	}
#endif

	LLViewerTexture* img = volume->getLightTexture();
	if (!img)
	{
		img = LLViewerFetchedTexture::sWhiteImagep;
	}

	S32 channel = shader.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);
	if (channel > -1)
	{
		if (img)
		{
			gGL.getTexUnit(channel)->bind(img);

			F32 lod_range = logf(img->getWidth()) / logf(2.f);

			shader.uniform1f(LLShaderMgr::PROJECTOR_FOCUS, focus);
			shader.uniform1f(LLShaderMgr::PROJECTOR_LOD, lod_range);
		}
	}
	stop_glerror();
}

void LLPipeline::unbindDeferredShader(LLGLSLShader& shader)
{
	LLTexUnit::eTextureType usage = mDeferredScreen.getUsage();
	shader.disableTexture(LLShaderMgr::DEFERRED_NORMAL, usage);
	shader.disableTexture(LLShaderMgr::DEFERRED_DIFFUSE, usage);
	shader.disableTexture(LLShaderMgr::DEFERRED_SPECULAR, usage);
	shader.disableTexture(LLShaderMgr::DEFERRED_DEPTH,
						  mDeferredDepth.getUsage());
	shader.disableTexture(LLShaderMgr::DEFERRED_LIGHT,
						  mDeferredLight.getUsage());
	shader.disableTexture(LLShaderMgr::DIFFUSE_MAP);
	shader.disableTexture(LLShaderMgr::DEFERRED_BLOOM);

	if (shader.mFeatures.hasShadows)
	{
		for (U32 i = 0; i < 4; ++i)
		{
			if (shader.disableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i) > -1)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB,
								GL_NONE);
			}
		}

		for (U32 i = 4; i < 6; ++i)
		{
			if (shader.disableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i) > -1)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB,
								GL_NONE);
			}
		}
	}

	shader.disableTexture(LLShaderMgr::DEFERRED_NOISE);
	shader.disableTexture(LLShaderMgr::DEFERRED_LIGHTFUNC);

	S32 channel = shader.disableTexture(LLShaderMgr::ENVIRONMENT_MAP,
										LLTexUnit::TT_CUBE_MAP);
	if (channel > -1)
	{
		LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
		if (cube_map)
		{
			cube_map->disableTexture();
		}
	}

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->unbind(LLTexUnit::TT_TEXTURE);
	unit0->activate();
	shader.unbind();
	stop_glerror();
}

void LLPipeline::generateWaterReflection()
{
	if (!gAgent.getRegion())
	{
		return;
	}

	if (!sWaterReflections || !LLDrawPoolWater::sNeedsReflectionUpdate)
	{
		if (!gViewerCamera.cameraUnderWater())
		{
			// Initial sky pass is still needed even if water reflection is not
			// rendering.
			pushRenderTypeMask();
			andRenderTypeMask(RENDER_TYPE_SKY, RENDER_TYPE_WL_SKY,
							  RENDER_TYPE_CLOUDS, END_RENDER_TYPES);
			LLCamera camera = gViewerCamera;
			camera.setFar(camera.getFar() * 0.75f);
			updateCull(camera, mSky);
			stateSort(camera, mSky);
			renderGeom(camera);
			popRenderTypeMask();
		}
		return;
	}

	bool skip_avatar_update = false;
	if (!isAgentAvatarValid() || gAgent.getCameraAnimating() ||
		gAgent.getCameraMode() != CAMERA_MODE_MOUSELOOK ||
		!LLVOAvatar::sVisibleInFirstPerson)
	{
		skip_avatar_update = true;
	}

	if (gUseNewShaders)
	{
		generateWaterReflectionEE(skip_avatar_update);
	}
	else
	{
		generateWaterReflectionWL(skip_avatar_update);
	}
}

void LLPipeline::generateWaterReflectionWL(bool skip_avatar_update)
{
	if (!skip_avatar_update)
	{
		gAgentAvatarp->updateAttachmentVisibility(CAMERA_MODE_THIRD_PERSON);
	}
	LLVertexBuffer::unbind();

	LLCamera camera = gViewerCamera;
	camera.setFar(camera.getFar() * 0.87654321f);

	sReflectionRender = true;

	pushRenderTypeMask();

#if REFLECTION_RENDER_NOT_OCCLUDED
	S32 occlusion = sUseOcclusion;
	// Disable occlusion culling for reflection map for now
	sUseOcclusion = 0;
#endif

	LLMatrix4a projection = gGLProjection;
	LLMatrix4a mat;

	stop_glerror();

	F32 height = gAgent.getRegion()->getWaterHeight();
	F32 to_clip = fabsf(camera.getOrigin().mV[2] - height);
	F32 pad = -to_clip * 0.05f; //amount to "pad" clip plane by

	// Plane params
	LLVector3 pnorm;
	F32 pd;
	LLPlane plane;
	S32 water_clip = 0;
	bool camera_is_underwater = gViewerCamera.cameraUnderWater();
	if (camera_is_underwater)
	{
		// Camera is below water, clip plane points down
		pnorm.set(0.f, 0.f, -1.f);
		pd = height;
		plane.setVec(pnorm, pd);
		water_clip = 1;
	}
	else
	{
		// Camera is above water, clip plane points up
		pnorm.set(0.f, 0.f, 1.f);
		pd = -height;
		plane.setVec(pnorm, pd);
		water_clip = -1;

		// Generate planar reflection map
		// Disable occlusion culling for reflection map for now
		S32 occlusion = sUseOcclusion;
		sUseOcclusion = 0;
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		glClearColor(0.f, 0.f, 0.f, 0.f);

		mWaterRef.bindTarget();

		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WATER0;
		gGL.setColorMask(true, true);
		mWaterRef.clear();
		gGL.setColorMask(true, false);

		mWaterRef.getViewport(gGLViewport);

		gGL.pushMatrix();

		mat.setIdentity();
		mat.getRow<2>().negate();
		mat.setTranslateAffine(LLVector3(0.f, 0.f, height * 2.f));

		LLMatrix4a current = gGLModelView;
		mat.setMul(current, mat);
		gGLModelView = mat;
		gGL.loadMatrix(mat);

		LLViewerCamera::updateFrustumPlanes(camera, false, true);

		LLVector4a origin;
		origin.clear();
		LLMatrix4a inv_mat = mat;
		inv_mat.invert();
		inv_mat.affineTransform(origin, origin);
		camera.setOrigin(origin.getF32ptr());

		glCullFace(GL_FRONT);

		if (LLDrawPoolWater::sNeedsReflectionUpdate)
		{
			// Initial sky pass (no user clip plane)
			pushRenderTypeMask();
			// Mask out everything but the sky
			andRenderTypeMask(RENDER_TYPE_SKY, RENDER_TYPE_WL_SKY,
							  RENDER_TYPE_CLOUDS, END_RENDER_TYPES);
			updateCull(camera, mSky);
			stateSort(camera, mSky);
			renderGeom(camera);
			popRenderTypeMask();

			gGL.setColorMask(true, false);
			pushRenderTypeMask();

			clearRenderTypeMask(RENDER_TYPE_WATER, RENDER_TYPE_VOIDWATER,
								RENDER_TYPE_GROUND, RENDER_TYPE_SKY,
								RENDER_TYPE_CLOUDS, END_RENDER_TYPES);

			if (RenderWaterReflections)
			{
				// Mask out selected geometry based on reflection detail
				if (RenderReflectionDetail < 3)
				{
					clearRenderTypeMask(RENDER_TYPE_PARTICLES,
										END_RENDER_TYPES);
					if (RenderReflectionDetail < 2)
					{
						clearRenderTypeMask(RENDER_TYPE_AVATAR,
											RENDER_TYPE_PUPPET,
											END_RENDER_TYPES);
						if (RenderReflectionDetail < 1)
						{
							clearRenderTypeMask(RENDER_TYPE_VOLUME,
												END_RENDER_TYPES);
						}
					}
				}

				LLGLUserClipPlane clip_plane(plane, mat, projection);
				LLGLDisable cull(GL_CULL_FACE);
				updateCull(camera, mReflectedObjects, -water_clip, &plane);
				stateSort(camera, mReflectedObjects);
			}

			if (LLDrawPoolWater::sNeedsReflectionUpdate &&
				RenderWaterReflections)
			{
				grabReferences(mReflectedObjects);
				LLGLUserClipPlane clip_plane(plane, mat, projection);
				renderGeom(camera);
			}

			popRenderTypeMask();
		}
		glCullFace(GL_BACK);
		gGL.popMatrix();
		mWaterRef.flush();
		gGLModelView = current;
		sUseOcclusion = occlusion;
	}

	camera.setOrigin(gViewerCamera.getOrigin());

	// Render distortion map
	static bool last_update = true;
	if (last_update)
	{
		camera.setFar(gViewerCamera.getFar());
		clearRenderTypeMask(RENDER_TYPE_WATER, RENDER_TYPE_VOIDWATER,
							RENDER_TYPE_GROUND, END_RENDER_TYPES);
		sUnderWaterRender = !camera_is_underwater;

		if (sUnderWaterRender)
		{
			clearRenderTypeMask(RENDER_TYPE_GROUND, RENDER_TYPE_SKY,
								RENDER_TYPE_CLOUDS, RENDER_TYPE_WL_SKY,
								END_RENDER_TYPES);
		}
		LLViewerCamera::updateFrustumPlanes(camera);

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		LLColor4& col = LLDrawPoolWater::sWaterFogColor;
		glClearColor(col.mV[0], col.mV[1], col.mV[2], 0.f);
		mWaterDis.bindTarget();
		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WATER1;

		mWaterDis.getViewport(gGLViewport);

		if (!sUnderWaterRender || LLDrawPoolWater::sNeedsReflectionUpdate)
		{
			// Clip out geometry on the same side of water as the camera
			LLPlane plane(-pnorm, -(pd + pad));

			LLGLUserClipPlane clip_plane(plane, gGLModelView, projection);
			updateCull(camera, mRefractedObjects, water_clip, &plane);
			stateSort(camera, mRefractedObjects);

			gGL.setColorMask(true, true);
			mWaterDis.clear();
			gGL.setColorMask(true, false);

			renderGeom(camera);

			gUIProgram.bind();
			gWorld.renderPropertyLines();
			gUIProgram.unbind();
		}

		mWaterDis.flush();
		sUnderWaterRender = false;
	}
	last_update = LLDrawPoolWater::sNeedsReflectionUpdate;

#if REFLECTION_RENDER_NOT_OCCLUDED
	sUseOcclusion = occlusion;
#endif
	sReflectionRender = false;

	if (!LLRenderTarget::sUseFBO)
	{
		glClear(GL_DEPTH_BUFFER_BIT);
	}
	glClearColor(0.f, 0.f, 0.f, 0.f);

	gViewerWindowp->setupViewport();
	popRenderTypeMask();
	LLDrawPoolWater::sNeedsReflectionUpdate = false;
	LLPlane npnorm(-pnorm, -pd);
	gViewerCamera.setUserClipPlane(npnorm);

	LLGLState::check();

	if (!skip_avatar_update)
	{
		gAgentAvatarp->updateAttachmentVisibility(gAgent.getCameraMode());
	}

	LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
}

void LLPipeline::generateWaterReflectionEE(bool skip_avatar_update)
{
	static LLCachedControl<bool> old_culling(gSavedSettings,
											 "RenderUseOldCulling");
	bool use_new_culling = gUseNewShaders && !old_culling;

	LLCamera camera = gViewerCamera;
	if (use_new_culling)
	{
		camera.setFar(camera.getFar() * 0.75f);
	}
	else
	{
		camera.setFar(camera.getFar() * 0.87654321f);
	}

	sReflectionRender = true;

	pushRenderTypeMask();

	stop_glerror();

#if REFLECTION_RENDER_NOT_OCCLUDED
	S32 occlusion = sUseOcclusion;
	// Disable occlusion culling for reflection map for now
	sUseOcclusion = 0;
#endif

	LLMatrix4a current = gGLModelView;
	LLMatrix4a projection = gGLProjection;

	F32 water_height = gAgent.getRegion()->getWaterHeight();
	F32 camera_height = gViewerCamera.getOrigin().mV[VZ];
	LLVector3 reflection_offset(0.f, 0.f,
								fabsf(camera_height - water_height) * 2.f);
	LLVector3 reflect_origin = gViewerCamera.getOrigin() - reflection_offset;
	const LLVector3& camera_look_at = gViewerCamera.getAtAxis();
	LLVector3 reflection_look_at(camera_look_at.mV[VX], camera_look_at.mV[VY],
								 -camera_look_at.mV[VZ]);
	LLVector3 reflect_interest_point = reflect_origin +
									   reflection_look_at * 5.f;
	camera.setOriginAndLookAt(reflect_origin, LLVector3::z_axis,
							  reflect_interest_point);

	// Plane params
	LLPlane plane;
	LLVector3 pnorm;
	F32 pd = 0.f;
	S32 water_clip = 0;
	bool camera_is_underwater = gViewerCamera.cameraUnderWater();
	if (camera_is_underwater)
	{
		// Camera is below water, clip plane points down
		pnorm.set(0.f, 0.f, -1.f);
		plane.setVec(pnorm, water_height);
		if (use_new_culling)
		{
			water_clip = -1;
		}
		else
		{
			pd = water_height;
			water_clip = 1;
		}
	}
	else
	{
		// Camera is above water, clip plane points up
		pnorm.set(0.f, 0.f, 1.f);
		plane.setVec(pnorm, -water_height);
		if (use_new_culling)
		{
			water_clip = 1;
		}
		else
		{
			pd = -water_height;
			water_clip = -1;
		}

		// Generate planar reflection map
		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WATER0;

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();

		LLMatrix4a mat;
		mat.setIdentity();
		mat.getRow<2>().negate();
		mat.setTranslateAffine(LLVector3(0.f, 0.f, water_height * 2.f));
		mat.setMul(current, mat);

		mReflectionModelView = mat;

		gGLModelView = mat;
		gGL.loadMatrix(mat);

		LLViewerCamera::updateFrustumPlanes(camera, false, true);

		LLVector4a origin;
		origin.clear();
		LLMatrix4a inv_mat = mat;
		inv_mat.invert();
		inv_mat.affineTransform(origin, origin);
		camera.setOrigin(origin.getF32ptr());

		glCullFace(GL_FRONT);

		if (LLDrawPoolWater::sNeedsReflectionUpdate)
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			glClearColor(0.f, 0.f, 0.f, 0.f);
			mWaterRef.bindTarget();

			gGL.setColorMask(true, true);
			mWaterRef.clear();
			gGL.setColorMask(true, false);
			mWaterRef.getViewport(gGLViewport);

			// Initial sky pass (no user clip plane)
			{
				// Mask out everything but the sky
				pushRenderTypeMask();
				if (RenderReflectionDetail < 3)
				{
					andRenderTypeMask(RENDER_TYPE_SKY, RENDER_TYPE_WL_SKY,
									  END_RENDER_TYPES);
				}
				else
				{
					andRenderTypeMask(RENDER_TYPE_SKY, RENDER_TYPE_WL_SKY,
									  RENDER_TYPE_CLOUDS, END_RENDER_TYPES);
				}
				updateCull(camera, mSky);
				stateSort(camera, mSky);

				renderGeom(camera);

				popRenderTypeMask();
			}

			if (RenderWaterReflections)
			{
				pushRenderTypeMask();

				clearRenderTypeMask(RENDER_TYPE_WATER, RENDER_TYPE_VOIDWATER,
									RENDER_TYPE_GROUND, RENDER_TYPE_SKY,
									RENDER_TYPE_CLOUDS, END_RENDER_TYPES);

				// Mask out selected geometry based on reflection detail
				if (RenderReflectionDetail < 3)
				{
					clearRenderTypeMask(RENDER_TYPE_PARTICLES,
										END_RENDER_TYPES);
					if (RenderReflectionDetail < 2)
					{
						clearRenderTypeMask(RENDER_TYPE_AVATAR,
											RENDER_TYPE_PUPPET,
											END_RENDER_TYPES);
						if (RenderReflectionDetail < 1)
						{
							clearRenderTypeMask(RENDER_TYPE_VOLUME,
												END_RENDER_TYPES);
						}
					}
				}

				LLGLUserClipPlane clip_plane(plane, mReflectionModelView,
											 projection);
				LLGLDisable cull(GL_CULL_FACE);
				updateCull(camera, mReflectedObjects, -water_clip, &plane);
				stateSort(camera, mReflectedObjects);
				renderGeom(camera);

				popRenderTypeMask();
			}
			mWaterRef.flush();
		}

		glCullFace(GL_BACK);
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.popMatrix();
		gGLModelView = current;
	}

	camera.setOrigin(gViewerCamera.getOrigin());

	// Render distortion map
	static bool last_update = true;
	if (last_update)
	{
		static LLCachedControl<bool> fast_edges(gSavedSettings,
												"RenderWaterFastEdges");
#if REFLECTION_RENDER_NOT_OCCLUDED
		if (fast_edges)
		{
			sUseOcclusion = occlusion;
		}
#endif
		pushRenderTypeMask();

		camera.setFar(gViewerCamera.getFar());
		clearRenderTypeMask(RENDER_TYPE_WATER, RENDER_TYPE_VOIDWATER,
							RENDER_TYPE_GROUND, END_RENDER_TYPES);

		// Intentionally inverted so that distortion map contents (objects
		// under the water when we are above it) will properly include water
		// fog effects.
		sUnderWaterRender = !camera_is_underwater;

		if (sUnderWaterRender)
		{
			clearRenderTypeMask(RENDER_TYPE_GROUND, RENDER_TYPE_SKY,
								RENDER_TYPE_CLOUDS, RENDER_TYPE_WL_SKY,
								END_RENDER_TYPES);
		}
		LLViewerCamera::updateFrustumPlanes(camera);

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		if (sUnderWaterRender || LLDrawPoolWater::sNeedsReflectionUpdate)
		{
			LLColor4& col = LLDrawPoolWater::sWaterFogColor;
			glClearColor(col.mV[0], col.mV[1], col.mV[2], 0.f);
			if (fast_edges)
			{
				// NOTE: LL's hack below cannot be applied when fast_edges is
				// true, because then occlusions are not disabled (see
				// above). HB
				LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
			}
			else
			{
				// *HACK: pretend underwater camera is the world camera to fix
				// weird visibility artifacts during distortion render (does
				// not break main render because the camera is the same
				// perspective as world camera and occlusion culling is
				// disabled for this pass).
				LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
			}

			mWaterDis.bindTarget();
			mWaterDis.getViewport(gGLViewport);

			if (!fast_edges)
			{
				gGL.setColorMask(true, true);
				mWaterDis.clear();
				gGL.setColorMask(true, false);
			}

			// Clip out geometry on the same side of water as the camera with
			// enough margin to not include the water geo itself, but not so
			// much as to clip out parts of avatars that should be seen under
			// the water in the distortion map.
			F32 water_dist;
			if (use_new_culling)
			{
				water_dist = water_height * 1.0125f;
			}
			else
			{
				F32 to_clip = fabsf(camera_height - water_height);
				// Amount to "pad" clip plane by
				F32 pad = -to_clip * 0.05f;
				water_dist = -pd - pad;
			}
			LLPlane plane(-pnorm,
						  camera_is_underwater ? -water_height : water_dist);
			LLGLUserClipPlane clip_plane(plane, current, projection);

			gGL.setColorMask(true, true);
			mWaterDis.clear();
			gGL.setColorMask(true, false);

			updateCull(camera, mRefractedObjects, water_clip, &plane);
			stateSort(camera, mRefractedObjects);

			renderGeom(camera);

			gUIProgram.bind();
			gWorld.renderPropertyLines();
			gUIProgram.unbind();

			mWaterDis.flush();
		}

		popRenderTypeMask();
	}
	last_update = LLDrawPoolWater::sNeedsReflectionUpdate;
	LLDrawPoolWater::sNeedsReflectionUpdate = false;

	popRenderTypeMask();

#if REFLECTION_RENDER_NOT_OCCLUDED
	sUseOcclusion = occlusion;
#endif
	sUnderWaterRender = false;
	sReflectionRender = false;

	if (!LLRenderTarget::sUseFBO)
	{
		glClear(GL_DEPTH_BUFFER_BIT);
	}
	glClearColor(0.f, 0.f, 0.f, 0.f);
	gViewerWindowp->setupViewport();

	LLGLState::check();

	if (!skip_avatar_update)
	{
		gAgentAvatarp->updateAttachmentVisibility(gAgent.getCameraMode());
	}

	LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
}

void LLPipeline::renderShadow(const LLMatrix4a& view, const LLMatrix4a& proj,
							  LLCamera& shadow_cam, LLCullResult &result,
							  bool use_shader, bool use_occlusion,
							  U32 target_width)
{
	LL_FAST_TIMER(FTM_SHADOW_RENDER);

	// Clip out geometry on the same side of water as the camera
	S32 occlude = sUseOcclusion;
	if (!use_occlusion)
	{
		sUseOcclusion = 0;
	}
	sShadowRender = true;

	static const U32 types[] =
	{
		LLRenderPass::PASS_SIMPLE,
		LLRenderPass::PASS_FULLBRIGHT,
		LLRenderPass::PASS_SHINY,
		LLRenderPass::PASS_BUMP,
		LLRenderPass::PASS_FULLBRIGHT_SHINY,
		LLRenderPass::PASS_MATERIAL,
		LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
		LLRenderPass::PASS_SPECMAP,
		LLRenderPass::PASS_SPECMAP_EMISSIVE,
		LLRenderPass::PASS_NORMMAP,
		LLRenderPass::PASS_NORMMAP_EMISSIVE,
		LLRenderPass::PASS_NORMSPEC,
		LLRenderPass::PASS_NORMSPEC_EMISSIVE,
	};

	LLGLEnable cull(GL_CULL_FACE);

	// Enable depth clamping if available and in use for shaders.
	U32 depth_clamp_state = 0;
	if (gGLManager.mUseDepthClamp)
	{
		// Added a debug setting to see if it makes any difference for
		// projectors with some GPU/drivers (no difference seen by me
		// for NVIDIA GPU + proprietary drivers). HB
		static LLCachedControl<bool> dclamp(gSavedSettings,
											"RenderDepthClampShadows");
		if (dclamp)
		{
			depth_clamp_state = GL_DEPTH_CLAMP;
		}
	}
	LLGLEnable depth_clamp(depth_clamp_state);

	if (use_shader)
	{
		gDeferredShadowCubeProgram.bind();
	}

	LLRenderTarget& occlusion_target =
		mShadowOcclusion[LLViewerCamera::sCurCameraID - 1];
	occlusion_target.bindTarget();
	updateCull(shadow_cam, result);
	occlusion_target.flush();

	stateSort(shadow_cam, result);

	// Generate shadow map
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(proj);
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadMatrix(view);

	stop_glerror();
	gGLLastMatrix = NULL;

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->unbind(LLTexUnit::TT_TEXTURE);

	stop_glerror();

	LLVertexBuffer::unbind();

	S32 sun_up = mIsSunUp ? 1 : 0;
	{
		if (!use_shader)
		{
			// Occlusion program is general purpose depth-only no-textures
			gOcclusionProgram.bind();
		}
		else
		{
			gDeferredShadowProgram.bind();
			if (gUseNewShaders)
			{
				gDeferredShadowProgram.uniform1i(LLShaderMgr::SUN_UP_FACTOR,
												 sun_up);
			}
		}

		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);

		// Ff not using VSM, disable color writes
		if (!gUseNewShaders || RenderShadowDetail <= 2)
		{
			gGL.setColorMask(false, false);
		}

		LL_FAST_TIMER(FTM_SHADOW_SIMPLE);
		unit0->disable();
		constexpr U32 count = sizeof(types) / sizeof(U32);
		for (U32 i = 0; i < count; ++i)
		{
			renderObjects(types[i], LLVertexBuffer::MAP_VERTEX, false);
		}
		unit0->enable(LLTexUnit::TT_TEXTURE);
		if (!use_shader)
		{
			gOcclusionProgram.unbind();
		}
		stop_glerror();
	}

	if (use_shader)
	{
		LL_TRACY_TIMER(TRC_SHADOW_GEOM);
		gDeferredShadowProgram.unbind();
		renderGeomShadow(shadow_cam);
		gDeferredShadowProgram.bind();
		if (gUseNewShaders)
		{
			gDeferredShadowProgram.uniform1i(LLShaderMgr::SUN_UP_FACTOR,
											 sun_up);
		}
	}
	else
	{
		LL_TRACY_TIMER(TRC_SHADOW_GEOM);
		renderGeomShadow(shadow_cam);
	}

	stop_glerror();

	{
		LL_FAST_TIMER(FTM_SHADOW_ALPHA);
		gDeferredShadowAlphaMaskProgram.bind();
		gDeferredShadowAlphaMaskProgram.uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH,
												  (F32)target_width);
		if (gUseNewShaders)
		{
			gDeferredShadowAlphaMaskProgram.uniform1i(LLShaderMgr::SUN_UP_FACTOR,
													  sun_up);
		}

		U32 mask =	LLVertexBuffer::MAP_VERTEX |
					LLVertexBuffer::MAP_TEXCOORD0 |
					LLVertexBuffer::MAP_COLOR |
					LLVertexBuffer::MAP_TEXTURE_INDEX;

		renderMaskedObjects(LLRenderPass::PASS_ALPHA_MASK, mask, true, true);

		if (gUseNewShaders)
		{
			gDeferredShadowAlphaMaskProgram.setMinimumAlpha(0.598f);
			renderObjects(LLRenderPass::PASS_ALPHA, mask, true, true);

			gDeferredShadowFullbrightAlphaMaskProgram.bind();
			gDeferredShadowFullbrightAlphaMaskProgram.uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH,
																(F32)target_width);
			gDeferredShadowFullbrightAlphaMaskProgram.uniform1i(LLShaderMgr::SUN_UP_FACTOR,
																sun_up);
			renderFullbrightMaskedObjects(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK,
										  mask, true, true);
		}
		else
		{
			renderMaskedObjects(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK, mask,
								true, true);
			gDeferredShadowAlphaMaskProgram.setMinimumAlpha(0.598f);
			renderObjects(LLRenderPass::PASS_ALPHA, mask, true, true);
		}

		mask = mask & ~LLVertexBuffer::MAP_TEXTURE_INDEX;

		gDeferredTreeShadowProgram.bind();
		if (gUseNewShaders)
		{
			gDeferredTreeShadowProgram.uniform1i(LLShaderMgr::SUN_UP_FACTOR,
												 sun_up);
		}
		renderMaskedObjects(LLRenderPass::PASS_NORMSPEC_MASK, mask);
		renderMaskedObjects(LLRenderPass::PASS_MATERIAL_ALPHA_MASK, mask);
		renderMaskedObjects(LLRenderPass::PASS_SPECMAP_MASK, mask);
		renderMaskedObjects(LLRenderPass::PASS_NORMMAP_MASK, mask);

		gDeferredTreeShadowProgram.setMinimumAlpha(0.598f);
		renderObjects(LLRenderPass::PASS_GRASS,
					  LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0,
					  true);
		stop_glerror();
	}

#if 0
	glCullFace(GL_BACK);
#endif

	gDeferredShadowCubeProgram.bind();
	gGLLastMatrix = NULL;
	gGL.loadMatrix(gGLModelView);

	LLRenderTarget& occlusion_source =
		mShadow[LLViewerCamera::sCurCameraID - 1];
	doOcclusion(shadow_cam, occlusion_source, occlusion_target);

	if (use_shader)
	{
		gDeferredShadowProgram.unbind();
	}

	gGL.setColorMask(true, true);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();
	gGLLastMatrix = NULL;

	sUseOcclusion = occlude;
	sShadowRender = false;
	stop_glerror();
}

// Gets point cloud of intersection of frust and min, max
bool LLPipeline::getVisiblePointCloud(LLCamera& camera,
									  LLVector3& min, LLVector3& max,
									  std::vector<LLVector3>& fp,
									  LLVector3 light_dir)
{
	LL_FAST_TIMER(FTM_VISIBLE_CLOUD);

	if (getVisibleExtents(camera, min, max))
	{
		return false;
	}

	// Get set of planes on bounding box
	LLPlane bp[] = {
		LLPlane(min, LLVector3(-1.f, 0.f, 0.f)),
		LLPlane(min, LLVector3(0.f, -1.f, 0.f)),
		LLPlane(min, LLVector3(0.f, 0.f, -1.f)),
		LLPlane(max, LLVector3(1.f, 0.f, 0.f)),
		LLPlane(max, LLVector3(0.f, 1.f, 0.f)),
		LLPlane(max, LLVector3(0.f, 0.f, 1.f))
	};

	// Potential points
	std::vector<LLVector3> pp;

	// Add corners of AABB
	pp.emplace_back(min.mV[0], min.mV[1], min.mV[2]);
	pp.emplace_back(max.mV[0], min.mV[1], min.mV[2]);
	pp.emplace_back(min.mV[0], max.mV[1], min.mV[2]);
	pp.emplace_back(max.mV[0], max.mV[1], min.mV[2]);
	pp.emplace_back(min.mV[0], min.mV[1], max.mV[2]);
	pp.emplace_back(max.mV[0], min.mV[1], max.mV[2]);
	pp.emplace_back(min.mV[0], max.mV[1], max.mV[2]);
	pp.emplace_back(max.mV[0], max.mV[1], max.mV[2]);

	// Add corners of camera frustum
	for (U32 i = 0; i < LLCamera::AGENT_FRUSTRUM_NUM; ++i)
	{
		pp.emplace_back(camera.mAgentFrustum[i]);
	}

	// bounding box line segments
	static const U32 bs[] =
	{
		0, 1,
		1, 3,
		3, 2,
		2, 0,

		4, 5,
		5, 7,
		7, 6,
		6, 4,

		0, 4,
		1, 5,
		3, 7,
		2, 6
	};

	for (U32 i = 0; i < 12; ++i)	// for each line segment in bounding box
	{
		const LLVector3& v1 = pp[bs[i * 2]];
		const LLVector3& v2 = pp[bs[i * 2 + 1]];
		LLVector3 n, line;
		// For each plane in camera frustum
		for (U32 j = 0; j < LLCamera::AGENT_PLANE_NO_USER_CLIP_NUM; ++j)
		{
			const LLPlane& cp = camera.getAgentPlane(j);
			cp.getVector3(n);

			line = v1 - v2;

			F32 d1 = line * n;
			F32 d2 = -cp.dist(v2);

			F32 t = d2 / d1;

			if (t > 0.f && t < 1.f)
			{
				pp.emplace_back(v2 + line * t);
			}
		}
	}

	// Camera frustum line segments
	static const U32 fs[] =
	{
		0, 1,
		1, 2,
		2, 3,
		3, 0,

		4, 5,
		5, 6,
		6, 7,
		7, 4,

		0, 4,
		1, 5,
		2, 6,
		3, 7
	};

	for (U32 i = 0; i < 12; ++i)
	{
		const LLVector3& v1 = pp[fs[i * 2] + 8];
		const LLVector3& v2 = pp[fs[i * 2 + 1] + 8];
		LLVector3 n, line;
		for (U32 j = 0; j < 6; ++j)
		{
			const LLPlane& cp = bp[j];
			cp.getVector3(n);

			line = v1 - v2;

			F32 d1 = line * n;
			F32 d2 = -cp.dist(v2);

			F32 t = d2 / d1;

			if (t > 0.f && t < 1.f)
			{
				pp.emplace_back(v2 + line * t);
			}
		}
	}

	LLVector3 ext[] = { min - LLVector3(0.05f, 0.05f, 0.05f),
						max + LLVector3(0.05f, 0.05f, 0.05f) };

	for (U32 i = 0, count = pp.size(); i < count; ++i)
	{
		bool found = true;

		const F32* p = pp[i].mV;

		for (U32 j = 0; j < 3; ++j)
		{
			if (p[j] < ext[0].mV[j] || p[j] > ext[1].mV[j])
			{
				found = false;
				break;
			}
		}

		for (U32 j = 0; j < LLCamera::AGENT_PLANE_NO_USER_CLIP_NUM; ++j)
		{
			const LLPlane& cp = camera.getAgentPlane(j);
			F32 dist = cp.dist(pp[i]);
			if (dist > 0.05f)
			{
				// point is above some plane, not contained
				found = false;
				break;
			}
		}
		if (found)
		{
			fp.emplace_back(pp[i]);
		}
	}

	return !fp.empty();
}

void LLPipeline::renderHighlight(const LLViewerObject* obj, F32 fade)
{
	if (obj && obj->getVolume())
	{
		for (LLViewerObject::child_list_t::const_iterator
				iter = obj->getChildren().begin(),
				end = obj->getChildren().end();
			 iter != end; ++iter)
		{
			renderHighlight(*iter, fade);
		}

		LLDrawable* drawable = obj->mDrawable;
		if (drawable)
		{
			for (S32 i = 0, count = drawable->getNumFaces(); i < count; ++i)
			{
				LLFace* face = drawable->getFace(i);
				if (face)
				{
					face->renderSelected(LLViewerTexture::sNullImagep,
										 LLColor4(1.f, 1.f, 1.f, fade));
				}
			}
		}
	}
}

void LLPipeline::generateSunShadow()
{
	if (!sRenderDeferred || RenderShadowDetail <= 0)
	{
		return;
	}

	LL_FAST_TIMER(FTM_GEN_SUN_SHADOW);

	LLViewerCamera::eCameraID saved_camera_id = LLViewerCamera::sCurCameraID;

	bool skip_avatar_update = false;
	if (!isAgentAvatarValid() || gAgent.getCameraAnimating() ||
		gAgent.getCameraMode() != CAMERA_MODE_MOUSELOOK ||
		!LLVOAvatar::sVisibleInFirstPerson)
	{
		skip_avatar_update = true;
	}

	if (!skip_avatar_update)
	{
		gAgentAvatarp->updateAttachmentVisibility(CAMERA_MODE_THIRD_PERSON);
	}

	// Store last_modelview of world camera
	LLMatrix4a last_modelview = gGLLastModelView;
	LLMatrix4a last_projection = gGLLastProjection;

	pushRenderTypeMask();
	andRenderTypeMask(RENDER_TYPE_SIMPLE,
					  RENDER_TYPE_ALPHA,
					  RENDER_TYPE_GRASS,
					  RENDER_TYPE_FULLBRIGHT,
					  RENDER_TYPE_BUMP,
					  RENDER_TYPE_VOLUME,
					  RENDER_TYPE_AVATAR,
					  RENDER_TYPE_PUPPET,
					  RENDER_TYPE_TREE,
					  RENDER_TYPE_TERRAIN,
					  RENDER_TYPE_WATER,
					  RENDER_TYPE_VOIDWATER,
					  RENDER_TYPE_PASS_ALPHA,
					  RENDER_TYPE_PASS_ALPHA_MASK,
					  RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
					  RENDER_TYPE_PASS_GRASS,
					  RENDER_TYPE_PASS_SIMPLE,
					  RENDER_TYPE_PASS_BUMP,
					  RENDER_TYPE_PASS_FULLBRIGHT,
					  RENDER_TYPE_PASS_SHINY,
					  RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
					  RENDER_TYPE_PASS_MATERIAL,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE,
					  RENDER_TYPE_PASS_SPECMAP,
					  RENDER_TYPE_PASS_SPECMAP_BLEND,
					  RENDER_TYPE_PASS_SPECMAP_MASK,
					  RENDER_TYPE_PASS_SPECMAP_EMISSIVE,
					  RENDER_TYPE_PASS_NORMMAP,
					  RENDER_TYPE_PASS_NORMMAP_BLEND,
					  RENDER_TYPE_PASS_NORMMAP_MASK,
					  RENDER_TYPE_PASS_NORMMAP_EMISSIVE,
					  RENDER_TYPE_PASS_NORMSPEC,
					  RENDER_TYPE_PASS_NORMSPEC_BLEND,
					  RENDER_TYPE_PASS_NORMSPEC_MASK,
					  RENDER_TYPE_PASS_NORMSPEC_EMISSIVE,
					  END_RENDER_TYPES);

	gGL.setColorMask(false, false);

	// Get sun view matrix

	// Store current projection/modelview matrix
	const LLMatrix4a saved_proj = gGLProjection;
	const LLMatrix4a saved_view = gGLModelView;
	LLMatrix4a inv_view(saved_view);
	inv_view.invert();

	LLMatrix4a view[6];
	LLMatrix4a proj[6];

	// Clip contains parallel split distances for 3 splits
	LLVector3 clip = RenderShadowClipPlanes;

	LLVector3 caster_dir(mIsSunUp ? mSunDir : mMoonDir);

	// Far clip on last split is minimum of camera view distance and 128
	mSunClipPlanes = LLVector4(clip, clip.mV[2] * clip.mV[2] / clip.mV[1]);

	// Put together a universal "near clip" plane for shadow frusta
	LLPlane shadow_near_clip;
	{
		LLVector3 p = gAgent.getPositionAgent();
		p += caster_dir * RenderFarClip * 2.f;
		shadow_near_clip.setVec(p, caster_dir);
	}

	LLVector3 light_dir = -caster_dir;
	light_dir.normalize();

	// Create light space camera matrix

	LLVector3 at = light_dir;

	LLVector3 up = gViewerCamera.getAtAxis();

	if (fabsf(up * light_dir) > 0.75f)
	{
		up = gViewerCamera.getUpAxis();
	}

	/*LLVector3 left = up%at;
	up = at%left;*/

	up.normalize();
	at.normalize();

	LLCamera main_camera = gViewerCamera;

	F32 near_clip = 0.f;
	{
		// Get visible point cloud
		main_camera.calcAgentFrustumPlanes(main_camera.mAgentFrustum);
		LLVector3 min, max;
		std::vector<LLVector3> fp;
		getVisiblePointCloud(main_camera, min, max, fp);
		if (fp.empty())
		{
			if (!hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA))
			{
				mShadowCamera[0] = main_camera;
				mShadowExtents[0][0] = min;
				mShadowExtents[0][1] = max;

				mShadowFrustPoints[0].clear();
				mShadowFrustPoints[1].clear();
				mShadowFrustPoints[2].clear();
				mShadowFrustPoints[3].clear();
			}
			popRenderTypeMask();

			if (!skip_avatar_update)
			{
				gAgentAvatarp->updateAttachmentVisibility(gAgent.getCameraMode());
			}

			return;
		}

		LLVector4a v;
		// Get good split distances for frustum
		for (U32 i = 0; i < fp.size(); ++i)
		{
			v.load3(fp[i].mV);
			saved_view.affineTransform(v, v);
			fp[i].set(v.getF32ptr());
		}

		min = fp[0];
		max = fp[0];

		// Get camera space bounding box
		for (U32 i = 1; i < fp.size(); ++i)
		{
			update_min_max(min, max, fp[i]);
		}

		near_clip = llclamp(-max.mV[2], 0.01f, 4.0f);
		F32 far_clip = llclamp(-min.mV[2] * 2.f, 16.0f, 512.0f);
		far_clip = llmin(far_clip, gViewerCamera.getFar());

		F32 range = far_clip - near_clip;

		LLVector3 split_exp = RenderShadowSplitExponent;

		F32 da = 1.f - llmax(fabsf(light_dir * up),
							 fabsf(light_dir * gViewerCamera.getLeftAxis()));

		da = powf(da, split_exp.mV[2]);

		F32 sxp = split_exp.mV[1] + (split_exp.mV[0] - split_exp.mV[1]) * da;

		for (U32 i = 0; i < 4; ++i)
		{
			F32 x = (F32)(i + 1) * 0.25f;
			x = powf(x, sxp);
			mSunClipPlanes.mV[i] = near_clip + range * x;
		}

		// Bump back first split for transition padding
		mSunClipPlanes.mV[0] *= 1.25f;
	}

	// Convenience array of 4 near clip plane distances
	F32 dist[] = { near_clip,
				   mSunClipPlanes.mV[0],
				   mSunClipPlanes.mV[1],
				   mSunClipPlanes.mV[2],
				   mSunClipPlanes.mV[3] };

	if (mSunDiffuse == LLColor4::black)
	{
		// Sun diffuse is totally black, shadows do not matter
		LLGLDepthTest depth(GL_TRUE);

		for (S32 j = 0; j < 4; ++j)
		{
			mShadow[j].bindTarget();
			mShadow[j].clear();
			mShadow[j].flush();
		}
	}
	else
	{
		for (S32 j = 0; j < 4; ++j)
		{
			if (!hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA))
			{
				mShadowFrustPoints[j].clear();
			}

			LLViewerCamera::sCurCameraID =
				(LLViewerCamera::eCameraID)(LLViewerCamera::CAMERA_SHADOW0 + j);

			// Restore render matrices
			gGLModelView = saved_view;
			gGLProjection = saved_proj;

			LLVector3 eye = gViewerCamera.getOrigin();

			// Camera used for shadow cull/render
			LLCamera shadow_cam;

			// Create world space camera frustum for this split
			shadow_cam = gViewerCamera;
			shadow_cam.setFar(16.f);

			LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false,
												true);

			LLVector3* frust = shadow_cam.mAgentFrustum;

			LLVector3 pn = shadow_cam.getAtAxis();

			// Construct 8 corners of split frustum section
			for (U32 i = 0; i < 4; ++i)
			{
				LLVector3 delta = frust[i + 4] - eye;
				delta += (frust[i + 4] - frust[(i + 2) % 4 + 4]) * 0.05f;
				delta.normalize();
				F32 dp = delta * pn;
				frust[i] = eye + (delta * dist[j] * 0.75f) / dp;
				frust[i + 4] = eye + (delta * dist[j + 1] * 1.25f) / dp;
			}

			shadow_cam.calcAgentFrustumPlanes(frust);
			shadow_cam.mFrustumCornerDist = 0.f;

			if (!hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA))
			{
				mShadowCamera[j] = shadow_cam;
			}

			std::vector<LLVector3> fp;
			LLVector3 min, max;
			if (!getVisiblePointCloud(shadow_cam, min, max, fp, light_dir))
			{
				// No possible shadow receivers
				if (!hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA))
				{
					mShadowExtents[j][0].clear();
					mShadowExtents[j][1].clear();
					mShadowCamera[j + 4] = shadow_cam;
				}

				mShadow[j].bindTarget();
				{
					LLGLDepthTest depth(GL_TRUE);
					mShadow[j].clear();
				}
				mShadow[j].flush();

				continue;
			}

			if (!hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA))
			{
				mShadowExtents[j][0] = min;
				mShadowExtents[j][1] = max;
				mShadowFrustPoints[j] = fp;
			}

			// Find a good origin for shadow projection
			LLVector3 origin;

			// Get a temporary view projection
			view[j] = look_proj(gViewerCamera.getOrigin(), light_dir, -up);

			std::vector<LLVector3> wpf;

			LLVector4a p;
			for (U32 i = 0; i < fp.size(); ++i)
			{
				p.load3(fp[i].mV);
				view[j].affineTransform(p, p);
				wpf.emplace_back(LLVector3(p.getF32ptr()));
			}

			min = max = wpf[0];

			for (U32 i = 0; i < fp.size(); ++i)
			{
				// Get AABB in camera space
				update_min_max(min, max, wpf[i]);
			}

			// Construct a perspective transform with perspective along y-axis
			// that contains points in wpf
			// Known:
			// - far clip plane
			// - near clip plane
			// - points in frustum
			// Find:
			// - origin

			// Get some "interesting" points of reference
			LLVector3 center = (min + max) * 0.5f;
			LLVector3 size = (max - min) * 0.5f;
			LLVector3 near_center = center;
			near_center.mV[1] += size.mV[1] * 2.f;

			// Put all points in wpf in quadrant 0, reletive to center of
			// min/max get the best fit line using least squares

			for (U32 i = 0; i < wpf.size(); ++i)
			{
				wpf[i] -= center;
				wpf[i].mV[0] = fabsf(wpf[i].mV[0]);
				wpf[i].mV[2] = fabsf(wpf[i].mV[2]);
			}

			F32 bfm = 0.f;
			F32 bfb = 0.f;
			if (!wpf.empty())
			{
				F32 sx = 0.f;
				F32 sx2 = 0.f;
				F32 sy = 0.f;
				F32 sxy = 0.f;
				for (U32 i = 0; i < wpf.size(); ++i)
				{
					sx += wpf[i].mV[0];
					sx2 += wpf[i].mV[0] * wpf[i].mV[0];
					sy += wpf[i].mV[1];
					sxy += wpf[i].mV[0] * wpf[i].mV[1];
				}

				bfm = (sy * sx - wpf.size() * sxy) /
					  (sx * sx - wpf.size() * sx2);
				bfb = (sx * sxy - sy * sx2) / (sx * sx - bfm * sx2);
			}
			if (llisnan(bfm) || llisnan(bfb))
			{
				LL_DEBUGS("Pipeline") << "NaN found. Corresponding shadow rendering aborted. Camera ID: "
									  << LLViewerCamera::sCurCameraID
									  << LL_ENDL;
				continue;
			}

			{
				// Best fit line is y = bfm * x + bfb

				// Find point that is furthest to the right of line
				F32 off_x = -1.f;
				LLVector3 lp;
				for (U32 i = 0; i < wpf.size(); ++i)
				{
					// y = bfm * x + bfb
					// x = (y - bfb) / bfm
					F32 lx = (wpf[i].mV[1] - bfb) / bfm;

					lx = wpf[i].mV[0] - lx;

					if (off_x < lx)
					{
						off_x = lx;
						lp = wpf[i];
					}
				}

				// Get line with slope bfm through lp
				// bfb = y - bfm * x
				bfb = lp.mV[1] - bfm * lp.mV[0];

				// Calculate error
				F32 shadow_error = 0.f;
				for (U32 i = 0; i < wpf.size(); ++i)
				{
					F32 lx = (wpf[i].mV[1] - bfb) / bfm;
					shadow_error += fabsf(wpf[i].mV[0] - lx);
				}

				shadow_error /= wpf.size() * size.mV[0];

				if (llisnan(shadow_error) ||
					shadow_error > RenderShadowErrorCutoff)
				{
					// Just use ortho projection
					origin.clear();
					proj[j] = gl_ortho(min.mV[0], max.mV[0], min.mV[1],
									   max.mV[1], -max.mV[2], -min.mV[2]);
				}
				else
				{
					// Origin is where line x = 0;
					origin.set(0, bfb, 0);

					F32 fovz = 1.f;
					F32 fovx = 1.f;

					LLVector3 zp;
					LLVector3 xp;

					for (U32 i = 0; i < wpf.size(); ++i)
					{
						LLVector3 atz = wpf[i] - origin;
						atz.mV[0] = 0.f;
						atz.normalize();
						if (fovz > -atz.mV[1])
						{
							zp = wpf[i];
							fovz = -atz.mV[1];
						}

						LLVector3 atx = wpf[i] - origin;
						atx.mV[2] = 0.f;
						atx.normalize();
						if (fovx > -atx.mV[1])
						{
							fovx = -atx.mV[1];
							xp = wpf[i];
						}
					}

					fovx = acosf(fovx);
					fovz = acosf(fovz);

					F32 cutoff = RenderShadowFOVCutoff;

					if (fovx < cutoff && fovz > cutoff)
					{
						// x is a good fit, but z is too big, move away from zp
						// enough so that fovz matches cutoff
						F32 d = zp.mV[2] / tanf(cutoff);
						F32 ny = zp.mV[1] + fabsf(d);

						origin.mV[1] = ny;

						fovz = fovx = 1.f;

						for (U32 i = 0; i < wpf.size(); ++i)
						{
							LLVector3 atz = wpf[i] - origin;
							atz.mV[0] = 0.f;
							atz.normalize();
							fovz = llmin(fovz, -atz.mV[1]);

							LLVector3 atx = wpf[i] - origin;
							atx.mV[2] = 0.f;
							atx.normalize();
							fovx = llmin(fovx, -atx.mV[1]);
						}

						fovx = acosf(fovx);
						fovz = acosf(fovz);
					}

					origin += center;

					F32 ynear = origin.mV[1] - max.mV[1];
					F32 yfar = origin.mV[1] - min.mV[1];

					if (ynear < 0.1f) // keep a sensible near clip plane
					{
						F32 diff = 0.1f - ynear;
						origin.mV[1] += diff;
						ynear += diff;
						yfar += diff;
					}

					if (fovx > cutoff)
					{
						// Just use ortho projection
						origin.clear();
						proj[j] = gl_ortho(min.mV[0], max.mV[0], min.mV[1],
										   max.mV[1], -max.mV[2], -min.mV[2]);
					}
					else
					{
						// Get perspective projection
						view[j].invert();

						// Translate view to origin
						LLVector4a origin_agent;
						origin_agent.load3(origin.mV);
						view[j].affineTransform(origin_agent, origin_agent);

						eye = LLVector3(origin_agent.getF32ptr());

						view[j] = look_proj(LLVector3(origin_agent.getF32ptr()),
											light_dir, -up);

						F32 fx = 1.f / tanf(fovx);
						F32 fz = 1.f / tanf(fovz);
						proj[j].setRow<0>(LLVector4a(-fx, 0.f, 0.f));
						proj[j].setRow<1>(LLVector4a(0.f,
													 (yfar + ynear) /
													 (ynear - yfar),
													 0.f, -1.f));
						proj[j].setRow<2>(LLVector4a(0.f, 0.f, -fz));
						proj[j].setRow<3>(LLVector4a(0.f,
													 2.f * yfar * ynear /
													 (ynear - yfar), 0.f));
					}
				}
			}

#if 0
			shadow_cam.setFar(128.f);
#endif
			if (llisnan(eye.mV[VX]) || llisnan(eye.mV[VY]) ||
				llisnan(eye.mV[VZ]))
			{
				LL_DEBUGS("Pipeline") << "NaN found in eye origin. Corresponding shadow rendering aborted. Camera ID: "
									  << LLViewerCamera::sCurCameraID
									  << LL_ENDL;
				continue;
			}
			shadow_cam.setOriginAndLookAt(eye, up, center);

			shadow_cam.setOrigin(0.f, 0.f, 0.f);

			gGLModelView = view[j];
			gGLProjection = proj[j];

			LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false,
												true);

			// shadow_cam.ignoreAgentFrustumPlane(LLCamera::AGENT_PLANE_NEAR);
			shadow_cam.getAgentPlane(LLCamera::AGENT_PLANE_NEAR).set(shadow_near_clip);

			gGLModelView = view[j];
			gGLProjection = proj[j];

			gGLLastModelView = mShadowModelview[j];
			gGLLastProjection = mShadowProjection[j];

			mShadowModelview[j] = view[j];
			mShadowProjection[j] = proj[j];

			mSunShadowMatrix[j].setMul(TRANS_MAT, proj[j]);
			mSunShadowMatrix[j].mulAffine(view[j]);
			mSunShadowMatrix[j].mulAffine(inv_view);

			mShadow[j].bindTarget();
			mShadow[j].getViewport(gGLViewport);
			mShadow[j].clear();

			U32 target_width = mShadow[j].getWidth();

			{
				static LLCullResult result[4];
				renderShadow(view[j], proj[j], shadow_cam, result[j],
							 true, !gUseNewShaders, target_width);
			}

			mShadow[j].flush();

			if (!hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA))
			{
				if (!gUseNewShaders)
				{
					LLViewerCamera::updateFrustumPlanes(shadow_cam, false,
														false, true);
				}
				mShadowCamera[j + 4] = shadow_cam;
			}
		}

		LLViewerCamera::sCurCameraID = saved_camera_id;
	}

	// HACK to disable projector shadows
	bool gen_shadow = RenderShadowDetail > 1;
	if (gen_shadow)
	{
		F32 fade_amt = gFrameIntervalSeconds *
					   llmax(gViewerCamera.getVelocityStat()->getCurrentPerSec(),
							 1.f);

		// Update shadow targets
		for (U32 i = 0; i < 2; ++i)
		{
			// For each current shadow
			LLViewerCamera::sCurCameraID =
				(LLViewerCamera::eCameraID)(LLViewerCamera::CAMERA_SHADOW4 + i);

			if (mShadowSpotLight[i].notNull() &&
				(mShadowSpotLight[i] == mTargetShadowSpotLight[0] ||
				mShadowSpotLight[i] == mTargetShadowSpotLight[1]))
			{
				// Keep this spotlight
				mSpotLightFade[i] = llmin(mSpotLightFade[i] + fade_amt, 1.f);
			}
			else
			{
				// Fade out this light
				mSpotLightFade[i] = llmax(mSpotLightFade[i] - fade_amt, 0.f);

				if (mSpotLightFade[i] == 0.f || mShadowSpotLight[i].isNull())
				{
					// Faded out, grab one of the pending spots (whichever one
					// is not already taken)
					if (mTargetShadowSpotLight[0] != mShadowSpotLight[(i + 1) % 2])
					{
						mShadowSpotLight[i] = mTargetShadowSpotLight[0];
					}
					else
					{
						mShadowSpotLight[i] = mTargetShadowSpotLight[1];
					}
				}
			}
		}

		for (S32 i = 0; i < 2; ++i)
		{
			gGLModelView = saved_view;
			gGLProjection = saved_proj;

			if (mShadowSpotLight[i].isNull())
			{
				continue;
			}

			LLVOVolume* volume = mShadowSpotLight[i]->getVOVolume();
			if (!volume)
			{
				mShadowSpotLight[i] = NULL;
				continue;
			}

			LLDrawable* drawable = mShadowSpotLight[i];

			LLVector3 params = volume->getSpotLightParams();
			F32 fov = params.mV[0];

			// Get agent->light space matrix (modelview)
			LLVector3 center = drawable->getPositionAgent();
			LLQuaternion quat = volume->getRenderRotation();

			// Get near clip plane
			LLVector3 scale = volume->getScale();
			LLVector3 at_axis(0.f, 0.f, -scale.mV[2] * 0.5f);
			at_axis *= quat;

			LLVector3 np = center + at_axis;
			at_axis.normalize();

			// Get origin that has given fov for plane np, at_axis, and given
			// scale
			F32 divisor = tanf(fov * 0.5f);
			// Seen happening and causing NaNs in setOrigin() below. HB
			if (divisor == 0.f) continue;
			F32 dist = (scale.mV[1] * 0.5f) / divisor;

			LLVector3 origin = np - at_axis * dist;

			LLMatrix4 mat(quat, LLVector4(origin, 1.f));

			view[i + 4].loadu(mat.getF32ptr());
			view[i + 4].invert();

			// Get perspective matrix
			F32 near_clip = dist + 0.01f;
			F32 width = scale.mV[VX];
			F32 height = scale.mV[VY];
			F32 far_clip = dist + volume->getLightRadius() * 1.5f;

			F32 fovy = fov * RAD_TO_DEG;
			F32 aspect = width / height;

			proj[i + 4] = gl_perspective(fovy, aspect, near_clip, far_clip);

			// Translate and scale from [-1, 1] to [0, 1]

			gGLModelView = view[i + 4];
			gGLProjection = proj[i + 4];

			gGLLastModelView = mShadowModelview[i + 4];
			gGLLastProjection = mShadowProjection[i + 4];

			mShadowModelview[i + 4] = view[i + 4];
			mShadowProjection[i + 4] = proj[i + 4];
			mSunShadowMatrix[i + 4].setMul(TRANS_MAT, proj[i + 4]);
			mSunShadowMatrix[i + 4].mulAffine(view[i + 4]);
			mSunShadowMatrix[i + 4].mulAffine(inv_view);

			LLCamera shadow_cam = gViewerCamera;
			shadow_cam.setFar(far_clip);
			shadow_cam.setOrigin(origin);

			LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false,
												true);

			mShadow[i + 4].bindTarget();
			mShadow[i + 4].getViewport(gGLViewport);
			mShadow[i + 4].clear();

			U32 target_width = mShadow[i + 4].getWidth();

			static LLCullResult result[2];

			LLViewerCamera::sCurCameraID =
				(LLViewerCamera::eCameraID)(LLViewerCamera::CAMERA_SHADOW0 +
											i + 4);

			sRenderSpotLight = drawable;
			renderShadow(view[i + 4], proj[i + 4], shadow_cam, result[i],
						 false, false, target_width);
			sRenderSpotLight = NULL;

			mShadow[i + 4].flush();
 		}

		LLViewerCamera::sCurCameraID = saved_camera_id;
	}
	else
	{
		// No spotlight shadows
		mShadowSpotLight[0] = mShadowSpotLight[1] = NULL;
	}

	if (!CameraOffset)
	{
		gGLModelView = saved_view;
		gGLProjection = saved_proj;
	}
	else
	{
		gGLModelView = view[1];
		gGLProjection = proj[1];
		gGL.loadMatrix(view[1]);
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.loadMatrix(proj[1]);
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
	gGL.setColorMask(true, false);

	gGLLastModelView = last_modelview;
	gGLLastProjection = last_projection;

	popRenderTypeMask();

	if (!skip_avatar_update)
	{
		gAgentAvatarp->updateAttachmentVisibility(gAgent.getCameraMode());
	}

	stop_glerror();
}

void LLPipeline::renderGroups(LLRenderPass* pass, U32 type, U32 mask,
							  bool texture)
{
	for (LLCullResult::sg_iterator i = sCull->beginVisibleGroups();
		 i != sCull->endVisibleGroups(); ++i)
	{
		LLSpatialGroup* group = *i;
		if (group && !group->isDead() &&
			(!sUseOcclusion ||
			 !group->isOcclusionState(LLSpatialGroup::OCCLUDED)) &&
			hasRenderType(group->getSpatialPartition()->mDrawableType) &&
			group->mDrawMap.find(type) != group->mDrawMap.end())
		{
			pass->renderGroup(group, type, mask, texture);
		}
	}
}

void LLPipeline::generateImpostor(LLVOAvatar* avatar)
{
	LLGLState::check(true, true);

	static LLCullResult result;
	result.clear();
	grabReferences(result);

	if (!avatar || !avatar->mDrawable)
	{
		return;
	}

	pushRenderTypeMask();

	bool visually_muted = avatar->isVisuallyMuted();
//MK
	bool vision_restricted = gRLenabled && gRLInterface.mVisionRestricted;
	if (vision_restricted)
	{
		// Render everything on impostors
		andRenderTypeMask(RENDER_TYPE_ALPHA,
						  RENDER_TYPE_FULLBRIGHT,
						  RENDER_TYPE_VOLUME,
						  RENDER_TYPE_GLOW,
						  RENDER_TYPE_BUMP,
						  RENDER_TYPE_PASS_SIMPLE,
						  RENDER_TYPE_PASS_ALPHA,
						  RENDER_TYPE_PASS_ALPHA_MASK,
						  RENDER_TYPE_PASS_BUMP,
						  RENDER_TYPE_PASS_POST_BUMP,
						  RENDER_TYPE_PASS_FULLBRIGHT,
						  RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
						  RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
						  RENDER_TYPE_PASS_GLOW,
						  RENDER_TYPE_PASS_GRASS,
						  RENDER_TYPE_PASS_SHINY,
						  RENDER_TYPE_PASS_MATERIAL,
						  RENDER_TYPE_PASS_MATERIAL_ALPHA,
						  RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK,
						  RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE,
						  RENDER_TYPE_PASS_SPECMAP,
						  RENDER_TYPE_PASS_SPECMAP_BLEND,
						  RENDER_TYPE_PASS_SPECMAP_MASK,
						  RENDER_TYPE_PASS_SPECMAP_EMISSIVE,
						  RENDER_TYPE_PASS_NORMMAP,
						  RENDER_TYPE_PASS_NORMMAP_BLEND,
						  RENDER_TYPE_PASS_NORMMAP_MASK,
						  RENDER_TYPE_PASS_NORMMAP_EMISSIVE,
						  RENDER_TYPE_PASS_NORMSPEC,
						  RENDER_TYPE_PASS_NORMSPEC_BLEND,
						  RENDER_TYPE_PASS_NORMSPEC_MASK,
						  RENDER_TYPE_PASS_NORMSPEC_EMISSIVE,
						  RENDER_TYPE_AVATAR,
						  RENDER_TYPE_PUPPET,
						  RENDER_TYPE_ALPHA_MASK,
						  RENDER_TYPE_FULLBRIGHT_ALPHA_MASK,
						  RENDER_TYPE_SIMPLE,
						  RENDER_TYPE_MATERIALS,
						  END_RENDER_TYPES);
	}
	else
//mk
	if (visually_muted)
	{
		andRenderTypeMask(RENDER_TYPE_AVATAR, RENDER_TYPE_PUPPET,
						  END_RENDER_TYPES);
	}
	else
	{
		andRenderTypeMask(RENDER_TYPE_ALPHA,
						  RENDER_TYPE_FULLBRIGHT,
						  RENDER_TYPE_VOLUME,
						  RENDER_TYPE_GLOW,
						  RENDER_TYPE_BUMP,
						  RENDER_TYPE_PASS_SIMPLE,
						  RENDER_TYPE_PASS_ALPHA,
						  RENDER_TYPE_PASS_ALPHA_MASK,
						  RENDER_TYPE_PASS_BUMP,
						  RENDER_TYPE_PASS_POST_BUMP,
						  RENDER_TYPE_PASS_FULLBRIGHT,
						  RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
						  RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
						  RENDER_TYPE_PASS_GLOW,
						  RENDER_TYPE_PASS_GRASS,
						  RENDER_TYPE_PASS_SHINY,
						  RENDER_TYPE_PASS_INVISIBLE,
						  RENDER_TYPE_PASS_INVISI_SHINY,
						  RENDER_TYPE_AVATAR,
						  RENDER_TYPE_PUPPET,
						  RENDER_TYPE_ALPHA_MASK,
						  RENDER_TYPE_FULLBRIGHT_ALPHA_MASK,
						  RENDER_TYPE_INVISIBLE,
						  RENDER_TYPE_SIMPLE,
						  END_RENDER_TYPES);
	}

	S32 occlusion = sUseOcclusion;
	sUseOcclusion = 0;
	sReflectionRender = !sRenderDeferred;
	sShadowRender = true;
	sImpostorRender = true;

	{
		LL_FAST_TIMER(FTM_IMPOSTOR_MARK_VISIBLE);
		markVisible(avatar->mDrawable, gViewerCamera);
		LLVOAvatar::sUseImpostors = false;

		for (S32 i = 0, count = avatar->mAttachedObjectsVector.size();
			 i < count; ++i)
		{
			LLViewerObject* object = avatar->mAttachedObjectsVector[i].first;
			if (object)
			{
				markVisible(object->mDrawable->getSpatialBridge(),
							gViewerCamera);
			}
		}
	}

	stateSort(gViewerCamera, result);

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	LLCamera camera = gViewerCamera;
	LLVector2 tdim;
	U32 res_y = 0;
	U32 res_x = 0;

	{
		LL_FAST_TIMER(FTM_IMPOSTOR_SETUP);
		const LLVector4a* ext = avatar->mDrawable->getSpatialExtents();
		LLVector3 pos = avatar->getRenderPosition() +
						avatar->getImpostorOffset();
		camera.lookAt(gViewerCamera.getOrigin(), pos,
					  gViewerCamera.getUpAxis());

		LLVector4a half_height;
		half_height.setSub(ext[1], ext[0]);
		half_height.mul(0.5f);

		LLVector4a left;
		left.load3(camera.getLeftAxis().mV);
		left.mul(left);
		left.normalize3fast();

		LLVector4a up;
		up.load3(camera.getUpAxis().mV);
		up.mul(up);
		up.normalize3fast();

		tdim.mV[0] = fabsf(half_height.dot3(left).getF32());
		tdim.mV[1] = fabsf(half_height.dot3(up).getF32());

		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();

		F32 distance = (pos - camera.getOrigin()).length();
		F32 fov = atanf(tdim.mV[1] / distance) * 2.f * RAD_TO_DEG;
		F32 aspect = tdim.mV[0] / tdim.mV[1];
		LLMatrix4a persp = gl_perspective(fov, aspect, .001f, 256.f);
		gGLProjection = persp;
		gGL.loadMatrix(persp);

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();
		LLMatrix4a mat;
		camera.getOpenGLTransform(mat.getF32ptr());

		mat.setMul(OGL_TO_CFR_ROT4A, mat);

		gGL.loadMatrix(mat);
		gGLModelView = mat;

		glClearColor(0.f, 0.f, 0.f, 0.f);
		gGL.setColorMask(true, true);

		// Get the number of pixels per angle
		F32 pa = gViewerWindowp->getWindowDisplayHeight() /
				 (RAD_TO_DEG * gViewerCamera.getView());

		// Get resolution based on angle width and height of impostor (double
		// desired resolution to prevent aliasing)
		res_y = llmin(nhpo2((U32)(fov * pa)), 512U);
		res_x = llmin(nhpo2((U32)(atanf(tdim.mV[0] / distance) * 2.f *
								 RAD_TO_DEG * pa)),
					  512U);

		if (!avatar->mImpostor.isComplete())
		{
			LL_FAST_TIMER(FTM_IMPOSTOR_ALLOCATE);
			if (sRenderDeferred)
			{
				avatar->mImpostor.allocate(res_x, res_y, GL_SRGB8_ALPHA8, true,
										   false);
				addDeferredAttachments(avatar->mImpostor);
			}
			else
			{
				avatar->mImpostor.allocate(res_x, res_y, GL_RGBA, true, false);
			}
			unit0->bind(&avatar->mImpostor);
			unit0->setTextureFilteringOption(LLTexUnit::TFO_POINT);
			unit0->unbind(LLTexUnit::TT_TEXTURE);
		}
		else if (res_x != avatar->mImpostor.getWidth() ||
				 res_y != avatar->mImpostor.getHeight())
		{
			LL_FAST_TIMER(FTM_IMPOSTOR_RESIZE);
			avatar->mImpostor.resize(res_x, res_y);
		}

		avatar->mImpostor.bindTarget();

		stop_glerror();
	}

	F32 old_alpha = LLDrawPoolAvatar::sMinimumAlpha;

	if (visually_muted)
	{
		// Disable alpha masking for muted avatars (get whole skin silhouette)
		LLDrawPoolAvatar::sMinimumAlpha = 0.f;
	}

	if (sRenderDeferred)
	{
		avatar->mImpostor.clear();
		renderGeomDeferred(camera);
		renderGeomPostDeferred(camera);

		// Shameless hack time: render it all again, this time writing the
		// depth values we need to generate the alpha mask below while
		// preserving the alpha-sorted color rendering from the previous pass.

		sImpostorRenderAlphaDepthPass = true;

		// Depth-only here...
		gGL.setColorMask(false, false);
		renderGeomPostDeferred(camera);

		sImpostorRenderAlphaDepthPass = false;
	}
	else
	{
		LLGLEnable scissor(GL_SCISSOR_TEST);
		glScissor(0, 0, res_x, res_y);
		avatar->mImpostor.clear();
		renderGeom(camera);

		// Shameless hack time: render it all again, this time writing the
		// depth values we need to generate the alpha mask below while
		// preserving the alpha-sorted color rendering from the previous pass.

		sImpostorRenderAlphaDepthPass = true;

		// Depth-only here...
		gGL.setColorMask(false, false);
		renderGeom(camera);

		sImpostorRenderAlphaDepthPass = false;
	}

	LLDrawPoolAvatar::sMinimumAlpha = old_alpha;

	{
		// Create alpha mask based on depth buffer (grey out if muted)
		LL_FAST_TIMER(FTM_IMPOSTOR_BACKGROUND);
		if (sRenderDeferred)
		{
			GLuint buff = GL_COLOR_ATTACHMENT0;
			glDrawBuffersARB(1, &buff);
		}

		LLGLDisable blend(vision_restricted ? 0 : GL_BLEND); // mk

		if (visually_muted)
		{
			gGL.setColorMask(true, true);
		}
		else
		{
			gGL.setColorMask(false, true);
		}

		unit0->unbind(LLTexUnit::TT_TEXTURE);

		LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_GREATER);

		gGL.flush();

		gGL.pushMatrix();
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();
		gGL.loadIdentity();

		constexpr F32 clip_plane = 0.99999f;

		gDebugProgram.bind();

		LLColor4 muted_color(avatar->getMutedAVColor());
		gGL.diffuseColor4fv(muted_color.mV);

		gGL.begin(LLRender::TRIANGLES);
		{
			gGL.vertex3f(-1.f, -1.f, clip_plane);
			gGL.vertex3f(1.f, -1.f, clip_plane);
			gGL.vertex3f(1.f, 1.f, clip_plane);
			gGL.vertex3f(-1.f, -1.f, clip_plane);
			gGL.vertex3f(1.f, 1.f, clip_plane);
			gGL.vertex3f(-1.f, 1.f, clip_plane);
		}
		gGL.end(true);

		gDebugProgram.unbind();

		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.popMatrix();

		stop_glerror();
	}

	avatar->mImpostor.flush();

	avatar->setImpostorDim(tdim);

	LLVOAvatar::sUseImpostors = LLVOAvatar::sMaxNonImpostors != 0;
	sUseOcclusion = occlusion;
	sReflectionRender = false;
	sImpostorRender = false;
	sShadowRender = false;
	popRenderTypeMask();

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	avatar->mNeedsImpostorUpdate = false;
	avatar->cacheImpostorValues();

	LLVertexBuffer::unbind();
	LLGLState::check(true, true);
}

void LLPipeline::setRenderTypeMask(U32 type, ...)
{
	va_list args;

	va_start(args, type);
	while (type < END_RENDER_TYPES)
	{
		mRenderTypeEnabled[type] = true;
		type = va_arg(args, U32);
	}
	va_end(args);

	if (type > END_RENDER_TYPES)
	{
		llerrs << "Invalid render type." << llendl;
	}
}

bool LLPipeline::hasAnyRenderType(U32 type, ...) const
{
	va_list args;
	va_start(args, type);
	while (type < END_RENDER_TYPES)
	{
		if (mRenderTypeEnabled[type])
		{
			va_end(args);
			return true;
		}
		type = va_arg(args, U32);
	}
	va_end(args);

	if (type > END_RENDER_TYPES)
	{
		llerrs << "Invalid render type." << llendl;
	}

	return false;
}

void LLPipeline::pushRenderTypeMask()
{
	mRenderTypeEnableStack.emplace((const char*)mRenderTypeEnabled,
								   sizeof(mRenderTypeEnabled));
}

void LLPipeline::popRenderTypeMask()
{
	if (mRenderTypeEnableStack.empty())
	{
		llerrs << "Depleted render type stack." << llendl;
	}

	memcpy(mRenderTypeEnabled, mRenderTypeEnableStack.top().data(),
		   sizeof(mRenderTypeEnabled));
	mRenderTypeEnableStack.pop();
}

void LLPipeline::andRenderTypeMask(U32 type, ...)
{
	bool tmp[NUM_RENDER_TYPES];
	for (U32 i = 0; i < NUM_RENDER_TYPES; ++i)
	{
		tmp[i] = false;
	}

	va_list args;
	va_start(args, type);
	while (type < END_RENDER_TYPES)
	{
		if (mRenderTypeEnabled[type])
		{
			tmp[type] = true;
		}

		type = va_arg(args, U32);
	}
	va_end(args);

	if (type > END_RENDER_TYPES)
	{
		llerrs << "Invalid render type." << llendl;
	}

	for (U32 i = 0; i < NUM_RENDER_TYPES; ++i)
	{
		mRenderTypeEnabled[i] = tmp[i];
	}
}

void LLPipeline::clearRenderTypeMask(U32 type, ...)
{
	va_list args;
	va_start(args, type);
	while (type < END_RENDER_TYPES)
	{
		mRenderTypeEnabled[type] = false;

		type = va_arg(args, U32);
	}
	va_end(args);

	if (type > END_RENDER_TYPES)
	{
		llerrs << "Invalid render type." << llendl;
	}
}

void LLPipeline::setAllRenderTypes()
{
	for (U32 i = 0; i < NUM_RENDER_TYPES; ++i)
	{
		mRenderTypeEnabled[i] = true;
	}
}

void LLPipeline::addDebugBlip(const LLVector3& position, const LLColor4& color)
{
	mDebugBlips.emplace_back(position, color);
}

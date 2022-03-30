/**
 * @file llviewerdisplay.cpp
 * @brief LLViewerDisplay class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llviewerdisplay.h"

#include "llapp.h"
#include "llcubemap.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llimagebmp.h"
#include "llimagegl.h"
#include "llrender.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldynamictexture.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolbump.h"
#include "lldrawpoolwater.h"
#include "llenvironment.h"
#include "llfeaturemanager.h"
#include "llfirstuse.h"
#include "llfloatertools.h"
#include "llgridmanager.h"			// For gIsInProductionGrid
#include "llhudmanager.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llstartup.h"
#include "lltooldraganddrop.h"
#include "lltoolfocus.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "lltracker.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvograss.h"
#include "llwlskyparammgr.h"
#include "llwlwaterparammgr.h"
#include "llworld.h"

extern LLPointer<LLViewerTexture> gStartTexture;

LLPointer<LLViewerTexture> gDisconnectedImagep = NULL;

// This is how long the sim will try to teleport you before giving up.
constexpr F32 TELEPORT_EXPIRY = 15.f;
// Additional time (in seconds) to wait per attachment
constexpr F32 TELEPORT_EXPIRY_PER_ATTACHMENT = 3.f;

// Constants used to toggle renderer back on after teleport

// Time to preload the world before raising the curtain after we've actually
// already arrived.
constexpr F32 TELEPORT_ARRIVAL_DELAY = 2.f;
// Delay to prevent teleports after starting an in-sim teleport.
constexpr F32 TELEPORT_LOCAL_DELAY = 1.f;

// Wait this long while reloading textures before we raise the curtain
constexpr F32 RESTORE_GL_TIME = 5.f;

// Globals

bool		 gTeleportDisplay = false;
LLFrameTimer gTeleportDisplayTimer;
LLFrameTimer gTeleportArrivalTimer;
F32			 gSavedDrawDistance = 0.f;
bool		 gUpdateDrawDistance = false;

bool gForceRenderLandFence = false;
bool gDisplaySwapBuffers = false;
bool gDepthDirty = false;
bool gResizeScreenTexture = false;
bool gResizeShadowTexture = false;
bool gSnapshot = false;
bool gShaderProfileFrame = false;
bool gResetVertexBuffers = false;
bool gScreenIsDirty = false;
U32 gFrameSleepTime = 0;

U32 gRecentFrameCount = 0; // number of 'recent' frames
U32 gLastFPSAverage = 0;
LLFrameTimer gRecentFPSTime;
LLFrameTimer gRecentMemoryTime;

// Rendering stuff
void render_hud_attachments();
void render_ui_3d();
void render_ui_2d();
void render_disconnected_background();

void display_startup()
{
	if (!gViewerWindowp || !gViewerWindowp->getActive() ||
		!gWindowp->getVisible() || gWindowp->getMinimized())
	{
		return;
	}

	gPipeline.updateGL();

	if (LLViewerFetchedTexture::sWhiteImagep.notNull())
	{
		LLTexUnit::sWhiteTexture =
			LLViewerFetchedTexture::sWhiteImagep->getTexName();
	}

	LLGLSDefault gls_default;

	// Required for HTML update in login screen
	static S32 frame_count = 0;

	LLGLState::check(true, true);

	if (frame_count++ > 1) // make sure we have rendered a frame first
	{
		LLViewerDynamicTexture::updateAllInstances();
	}

	LLGLState::check(true, true);

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	LLGLSUIDefault gls_ui;
	gPipeline.disableLights();

	gViewerWindowp->setup2DRender();

	gGL.color4f(1.f, 1.f, 1.f, 1.f);
	gViewerWindowp->draw();
	gGL.flush();

	LLVertexBuffer::unbind();

	LLGLState::check(true, true);

	gWindowp->swapBuffers();
	glClear(GL_DEPTH_BUFFER_BIT);
}

void display_update_camera()
{
	F32 farclip = gAgent.mDrawDistance;
#if 0
	if (gAgent.getCameraMode() == CAMERA_MODE_CUSTOMIZE_AVATAR)
	{
		// Cut draw distance in half when customizing avatar, but on the
		// viewer only.
		farclip *= 0.5f;
	}
#endif
	gViewerCamera.setFar(farclip);

	gViewerWindowp->setup3DRender();

	// Update land visibility
	gWorld.setLandFarClip(farclip);
}

// Write some stats to llinfos
void display_stats()
{
	static LLCachedControl<S32> background_yield_time(gSavedSettings,
													  "BackgroundYieldTime");
	if (!gWindowp->getVisible() ||
		(background_yield_time > 0 && !gFocusMgr.getAppHasFocus()))
	{
		// Do not keep FPS statistics while yielding cooperatively or not
		// visible.
		gRecentFrameCount = 0;
		gRecentFPSTime.reset();
	}
	static LLCachedControl<F32> fps_log_freq(gSavedSettings,
											 "FPSLogFrequency");
	if (fps_log_freq > 0.f)
	{
		F32 ellapsed = gRecentFPSTime.getElapsedTimeF32();
		if (ellapsed >= fps_log_freq)
		{
			F32 fps = gRecentFrameCount / ellapsed;
			llinfos << llformat("FPS: %.02f", fps) << llendl;
			gLastFPSAverage = (U32)fps;
			gRecentFrameCount = 0;
			gRecentFPSTime.reset();
		}
		else if (ellapsed >= 10.f)
		{
			gLastFPSAverage = (U32)gRecentFrameCount / ellapsed;
		}
	}
	static LLCachedControl<F32> mem_log_freq(gSavedSettings,
											 "MemoryLogFrequency");
	if (mem_log_freq > 0.f &&
		gRecentMemoryTime.getElapsedTimeF32() >= mem_log_freq)
	{
		gMemoryAllocated = LLMemory::getCurrentRSS();
		U32 memory = (U32)(gMemoryAllocated / 1048576UL);
		llinfos << llformat("MEMORY: %d MB", memory) << llendl;
		gRecentMemoryTime.reset();
	}
}

// Paint the display
void display(bool rebuild, F32 zoom_factor, S32 subfield, bool for_snapshot)
{
	LL_FAST_TIMER(FTM_RENDER);

	if (!gViewerWindowp) return;

	LLVBOPool::deleteReleasedBuffers();

	stop_glerror();

	if (gResizeScreenTexture)
	{
		// Skip render on frames where window has been resized
		gGL.flush();
		glClear(GL_COLOR_BUFFER_BIT);
		gWindowp->swapBuffers();
		gPipeline.resizeScreenTexture();
		return;
	}

	if (gResizeShadowTexture)
	{
		// Skip render on frames where window has been resized
		gPipeline.resizeShadowTexture();
	}

	if (LLPipeline::sRenderDeferred)
	{
		// *HACK: to make sky show up in deferred snapshots
		for_snapshot = false;
	}

	if (LLPipeline::sRenderFrameTest)
	{
		LLWorld::sendAgentPause();
	}

	gSnapshot = for_snapshot;

	LLGLSDefault gls_default;
	LLGLDepthTest gls_depth(GL_TRUE, GL_TRUE, GL_LEQUAL);

	LLVertexBuffer::unbind();

	LLGLState::check(true, true);

	gPipeline.disableLights();

	// Reset vertex buffers if needed
	gPipeline.doResetVertexBuffers();

	stop_glerror();

	// Do not draw if the window is hidden or minimized. In fact, we must
	// explicitly check the minimized state before drawing. Attempting to draw
	// into a minimized window causes a GL error. JC
	if (!gViewerWindowp->getActive() || !gWindowp->getVisible() ||
		gWindowp->getMinimized())
	{
		// Clean up memory the pools may have allocated
		if (rebuild)
		{
			gPipeline.rebuildPools();
		}

		// Avoid accumulating HUD objects while minimized
		LLHUDObject::renderAllForTimer();

		gViewerWindowp->returnEmptyPicks();
		return;
	}

	gViewerWindowp->checkSettings();

	{
		LL_FAST_TIMER(FTM_PICK);
		static const std::string DisplayPick = "Display:Pick";
		gAppViewerp->pingMainloopTimeout(&DisplayPick);
		gViewerWindowp->performPick();
	}

	static const std::string DisplayCheckStates = "Display:CheckStates";
	gAppViewerp->pingMainloopTimeout(&DisplayCheckStates);
	LLGLState::check(true, true);

	//////////////////////////////////////////////////////////
	// Logic for forcing window updates if we are in drone mode.

	// Bail out if we are in the startup state and do not want to try to render
	// the world.
	if (!LLStartUp::isLoggedIn())
	{
		static const std::string DisplayStartup = "Display:Startup";
		gAppViewerp->pingMainloopTimeout(&DisplayStartup);
		display_startup();
		gScreenIsDirty = false;
		return;
	}

	if (gShaderProfileFrame)
	{
		LLGLSLShader::initProfile();
	}

#if 0
	LLGLState::verify(false);
#endif

	/////////////////////////////////////////////////
	// Update GL Texture statistics (used for discard logic ?)

	static const std::string DisplayTextureStats = "Display:TextureStats";
	gAppViewerp->pingMainloopTimeout(&DisplayTextureStats);
	stop_glerror();

	LLImageGL::updateStats(gFrameTimeSeconds);

	static LLCachedControl<S32> render_name(gSavedSettings, "RenderName");
	static LLCachedControl<bool> hide_all_titles(gSavedSettings,
												 "RenderHideGroupTitleAll");
	LLVOAvatar::sRenderName = render_name;
	LLVOAvatar::sRenderGroupTitles = !hide_all_titles;

	gPipeline.mBackfaceCull = true;
	++gFrameCount;
	++gRecentFrameCount;
	if (gFocusMgr.getAppHasFocus())
	{
		++gForegroundFrameCount;
	}

	//////////////////////////////////////////////////////////
	// Display start screen if we are teleporting, and skip render

	if (gTeleportDisplay)
	{
		static const std::string DisplayTeleport = "Display:Teleport";
		gAppViewerp->pingMainloopTimeout(&DisplayTeleport);

		S32 attach_count = 0;
		if (isAgentAvatarValid())
		{
			attach_count = gAgentAvatarp->getNumAttachments();
		}
		F32 teleport_save_time = TELEPORT_EXPIRY +
								 TELEPORT_EXPIRY_PER_ATTACHMENT * attach_count;
		F32 teleport_elapsed = gTeleportDisplayTimer.getElapsedTimeF32();
		F32 teleport_percent = teleport_elapsed * 100.f / teleport_save_time;
		if (gAgent.getTeleportState() != LLAgent::TELEPORT_START &&
			teleport_percent > 100.f)
		{
			// Give up. Do not keep the UI locked forever.
			gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
			gAgent.setTeleportMessage("");
		}

		static LLCachedControl<bool> hide_tp_progress(gSavedSettings,
													  "HideTeleportProgress");
		bool show_tp_progress = !hide_tp_progress;
		const std::string& message = gAgent.getTeleportMessage();
		switch (gAgent.getTeleportState())
		{
			case LLAgent::TELEPORT_START:
				// Transition to REQUESTED. Viewer has sent some kind of
				// TeleportRequest to the source simulator.
				gTeleportDisplayTimer.reset();
				if (show_tp_progress)
				{
					gViewerWindowp->setShowProgress(true);
					gViewerWindowp->setProgressPercent(0);
					gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages["requesting"]);
				}
				gAgent.setTeleportState(LLAgent::TELEPORT_REQUESTED);
				break;

			case LLAgent::TELEPORT_REQUESTED:
				// Waiting for source simulator to respond
				if (show_tp_progress)
				{
					gViewerWindowp->setProgressPercent(llmin(teleport_percent,
															 37.5f));
					gViewerWindowp->setProgressString(message);
				}
				break;

			case LLAgent::TELEPORT_MOVING:
				// Viewer has received destination location from source
				// simulator
				if (show_tp_progress)
				{
					gViewerWindowp->setProgressPercent(llmin(teleport_percent,
															 75.f));
					gViewerWindowp->setProgressString(message);
				}
				break;

			case LLAgent::TELEPORT_START_ARRIVAL:
				// Transition to ARRIVING. Viewer has received avatar update,
				// etc, from destination simulator
				gTeleportArrivalTimer.reset();
				if (show_tp_progress)
				{
					gViewerWindowp->setProgressCancelButtonVisible(false);
					gViewerWindowp->setProgressPercent(75.f);
					gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages["arriving"]);
				}
				gAgent.setTeleportState(LLAgent::TELEPORT_ARRIVING);
				if (gSavedSettings.getBool("DisablePrecacheDelayAfterTP"))
				{
					LLFirstUse::useTeleport();
					gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
				}
				break;

			case LLAgent::TELEPORT_ARRIVING:
			{
				// Make the user wait while content "pre-caches"
				F32 percent = gTeleportArrivalTimer.getElapsedTimeF32() /
							  TELEPORT_ARRIVAL_DELAY;
				if (!show_tp_progress || percent > 1.f)
				{
					percent = 1.f;
					LLFirstUse::useTeleport();
					gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
				}
				if (show_tp_progress)
				{
					gViewerWindowp->setProgressCancelButtonVisible(false);
					gViewerWindowp->setProgressPercent(percent * 25.f + 75.f);
					gViewerWindowp->setProgressString(message);
				}
				break;
			}

			case LLAgent::TELEPORT_LOCAL:
				// Short delay when teleporting in the same sim (progress
				// screen active but not shown; did not fall-through from
				// TELEPORT_START)
				if (gTeleportDisplayTimer.getElapsedTimeF32() > TELEPORT_LOCAL_DELAY)
				{
					LLFirstUse::useTeleport();
					gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
				}
				break;

			case LLAgent::TELEPORT_NONE:
				// No teleport in progress
				gViewerWindowp->setShowProgress(false);
				gTeleportDisplay = false;
				gTeleportArrivalTimer.reset();
				break;

			default:
				 break;
		}
	}
    else if (gAppViewerp->logoutRequestSent())
	{
		static const std::string DisplayLogout = "Display:Logout";
		gAppViewerp->pingMainloopTimeout(&DisplayLogout);
		F32 percent_done = gLogoutTimer.getElapsedTimeF32() * 100.f /
						   gLogoutMaxTime;
		if (percent_done > 100.f)
		{
			percent_done = 100.f;
		}

		if (LLApp::isExiting())
		{
			percent_done = 100.f;
		}

		gViewerWindowp->setProgressPercent(percent_done);
	}
	else if (gRestoreGL)
	{
		static const std::string DisplayRestoreGL = "Display:RestoreGL";
		gAppViewerp->pingMainloopTimeout(&DisplayRestoreGL);
		F32 percent_done = gRestoreGLTimer.getElapsedTimeF32() * 100.f /
						   RESTORE_GL_TIME;
		if (percent_done > 100.f)
		{
			gViewerWindowp->setShowProgress(false);
			gRestoreGL = false;
		}
		else
		{
			if (LLApp::isExiting())
			{
				percent_done = 100.f;
			}

			gViewerWindowp->setProgressPercent(percent_done);
		}
	}

	// Progressively increase draw distance after TP when required and when
	// possible (enough available memory).
	if (gSavedDrawDistance > 0.f && !gAgent.teleportInProgress() &&
		!LLPipeline::sMemAllocationThrottled &&
		LLViewerTexture::sDesiredDiscardBias <= 2.5f)
	{
		static LLCachedControl<U32> speed_rez_interval(gSavedSettings,
													   "SpeedRezInterval");
		if (gTeleportArrivalTimer.getElapsedTimeF32() >= (F32)speed_rez_interval)
		{
			gTeleportArrivalTimer.reset();
			F32 current = gSavedSettings.getF32("RenderFarClip");
			if (gSavedDrawDistance > current)
			{
				current *= 2.f;
				if (current > gSavedDrawDistance)
				{
					current = gSavedDrawDistance;
				}
				gSavedSettings.setF32("RenderFarClip", current);
			}
			if (current >= gSavedDrawDistance)
			{
				gSavedDrawDistance = 0.f;
				gSavedSettings.setF32("SavedRenderFarClip", 0.f);
			}
		}
	}

	// gResetVertexBuffers is set to true when the memory gets too low, by
	// LLViewerTexture::isMemoryForTextureLow(). The reset must happen *after*
	// the new draw distance has been taken into account (so that freed objects
	// vertex buffers are cleared).
	if (gResetVertexBuffers)
	{
		gResetVertexBuffers = false;
		// The actual reset (doResetVertexBuffers()) will happen on next frame
		gPipeline.resetVertexBuffers();
	}

	// We do this here instead of inside of handleRenderFarClipChanged() in
	// llviewercontrol.cpp to ensure this is not done during rendering, which
	// would cause drawables to get destroyed while LLSpatialGroup::sNoDelete
	// is true and would therefore cause a crash.
	if (gUpdateDrawDistance)
	{
		gUpdateDrawDistance = false;
		F32 draw_distance = gSavedSettings.getF32("RenderFarClip");
		gAgent.mDrawDistance = draw_distance;
		gWorld.setLandFarClip(draw_distance);
		LLVOCacheEntry::updateSettings();
	}

	//////////////////////////
	// Prepare for the next frame

	/////////////////////////////
	// Update the camera

	static const std::string DisplayCamera = "Display:Camera";
	gAppViewerp->pingMainloopTimeout(&DisplayCamera);
	gViewerCamera.setZoomParameters(zoom_factor, subfield);
	gViewerCamera.setNear(MIN_NEAR_PLANE);

	//////////////////////////
	// Clear the next buffer (must follow dynamic texture writing since that
	// uses the frame buffer)

	if (gDisconnected)
	{
		static const std::string DisplayDisconnected = "Display:Disconnected";
		gAppViewerp->pingMainloopTimeout(&DisplayDisconnected);
		render_ui();
	}

	//////////////////////////
	// Set rendering options

	static const std::string DisplayRenderSetup = "Display:RenderSetup";
	gAppViewerp->pingMainloopTimeout(&DisplayRenderSetup);
	stop_glerror();

	///////////////////////////////////////
	// Slam lighting parameters back to our defaults.
	// Note that these are not the same as GL defaults...

	gGL.setAmbientLightColor(LLColor4::white);

	/////////////////////////////////////
	// Render
	//
	// Actually push all of our triangles to the screen.

	// Do render-to-texture stuff here
	if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_DYNAMIC_TEXTURES))
	{
		static const std::string DisplayDynamicTextures =
			"Display:DynamicTextures";
		gAppViewerp->pingMainloopTimeout(&DisplayDynamicTextures);
		LL_FAST_TIMER(FTM_UPDATE_TEXTURES);
		if (LLViewerDynamicTexture::updateAllInstances())
		{
			gGL.setColorMask(true, true);
			glClear(GL_DEPTH_BUFFER_BIT);
		}
	}

	gViewerWindowp->setupViewport();

	gPipeline.resetFrameStats();	// Reset per-frame statistics.

	if (!gDisconnected)
	{
		static const std::string DisplayUpdate = "Display:Update";
		gAppViewerp->pingMainloopTimeout(&DisplayUpdate);
		if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
		{
			// Do not draw hud objects in this frame
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
		}

		if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES))
		{
			// Do not draw hud particles in this frame
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
		}

		display_update_camera();

		// Update all the sky/atmospheric/water settings
		static bool last_override_wl = gUseNewShaders ||
									   LLEnvironment::overrideWindlight();
		bool override_wl;
		if (gUseNewShaders)
		{
			override_wl = true;
			gEnvironment.update(&gViewerCamera);
		}
		else
		{
			override_wl = LLEnvironment::overrideWindlight();
			if (last_override_wl && !override_wl)
			{
				// We just switched extended environment off, so we need to
				// restore the Windlight default textures...
				gSky.setSunTextures(LLUUID::null);
				gSky.setMoonTextures(LLUUID::null);
				gSky.setCloudNoiseTextures(LLUUID::null);
				gSky.setBloomTextures(LLUUID::null);
				LLDrawPoolWater* poolp =
					(LLDrawPoolWater*)gPipeline.getPool(LLDrawPool::POOL_WATER);
				if (poolp)
				{
					poolp->setTransparentTextures(LLUUID::null);
					poolp->setNormalMaps(LLUUID::null);
				}
			}
			static bool ran_once = false;
			if (override_wl || !ran_once)
			{
				ran_once = true;
				gEnvironment.update(&gViewerCamera);
			}
			gWLSkyParamMgr.update(&gViewerCamera);
			gWLWaterParamMgr.update(&gViewerCamera);
		}
		last_override_wl = override_wl;
		stop_glerror();

		{
			LL_FAST_TIMER(FTM_HUD_UPDATE);
			LLHUDManager::updateEffects();
			LLHUDObject::updateAll();
			stop_glerror();
		}

		{
			LL_FAST_TIMER(FTM_DISPLAY_UPDATE_GEOM);
			// 50 ms/second update time:
			const F32 max_geom_update_time = 0.05f * gFrameIntervalSeconds;
			gPipeline.createObjects(max_geom_update_time);
			gPipeline.processPartitionQ();
			gPipeline.updateGeom(max_geom_update_time);
			stop_glerror();
		}

		gPipeline.updateGL();
		stop_glerror();

		S32 water_clip = 0;
		if ((gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_ENVIRONMENT) > 1) &&
			 (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_WATER) ||
			  gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_VOIDWATER)))
		{
			if (gViewerCamera.cameraUnderWater())
			{
				water_clip = -1;
			}
			else
			{
				water_clip = 1;
			}
		}

		static const std::string DisplayCull = "Display:Cull";
		gAppViewerp->pingMainloopTimeout(&DisplayCull);

		// Increment drawable frame counter
		LLDrawable::incrementVisible();

		LLSpatialGroup::sNoDelete = true;
		if (LLViewerFetchedTexture::sWhiteImagep.notNull())
		{
			LLTexUnit::sWhiteTexture =
				LLViewerFetchedTexture::sWhiteImagep->getTexName();
		}

		S32 occlusion = LLPipeline::sUseOcclusion;
		if (gDepthDirty)
		{
			// Depth buffer is invalid, do not overwrite occlusion state
			LLPipeline::sUseOcclusion = llmin(occlusion, 1);
		}
		gDepthDirty = false;

		LLGLState::check(true, true);

		static LLCullResult result;
		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
		LLPipeline::sUnderWaterRender = gViewerCamera.cameraUnderWater();
		gPipeline.updateCull(gViewerCamera, result, water_clip);
		stop_glerror();

		LLGLState::check(true, true);

		static const std::string DisplaySwap = "Display:Swap";
		gAppViewerp->pingMainloopTimeout(&DisplaySwap);

		{
			if (gResizeScreenTexture)
			{
				gPipeline.resizeScreenTexture();
			}

			gGL.setColorMask(true, true);
			glClearColor(0.f, 0.f, 0.f, 0.f);

			LLGLState::check(true, true);

			if (!for_snapshot)
			{
				if (gFrameCount > 1)
				{
					// For some reason, ATI 4800 series will error out if you
					// try to generate a shadow before the first frame is
					// through
					gPipeline.generateSunShadow();
				}

				LLVertexBuffer::unbind();

				LLGLState::check(true, true);

				const LLMatrix4a proj = gGLProjection;
				const LLMatrix4a mod = gGLModelView;
				glViewport(0, 0, 512, 512);

				if (!LLPipeline::sMemAllocationThrottled)
				{
					LL_FAST_TIMER(FTM_IMPOSTORS_UPDATE);
					LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
					LLVOAvatar::updateImpostors();
				}

				gGLProjection = proj;
				gGLModelView = mod;
				gGL.matrixMode(LLRender::MM_PROJECTION);
				gGL.loadMatrix(proj);
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				gGL.loadMatrix(mod);
				gViewerWindowp->setupViewport();

				LLGLState::check(true, true);

			}
			glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		}

#if 0
		if (!for_snapshot)
#endif
		{
			static const std::string DisplayImagery = "Display:Imagery";
			gAppViewerp->pingMainloopTimeout(&DisplayImagery);
			gPipeline.generateWaterReflection();
			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_PHYSICS_SHAPES))
			{
				gPipeline.renderPhysicsDisplay();
			}
		}

		LLGLState::check(true, true);

		//////////////////////////////////////
		// Update images, using the image stats generated during object
		// update/culling
		//
		// Can put objects onto the retextured list.
		//
		// Doing this here gives hardware occlusion queries extra time to
		// complete
		static const std::string DisplayUpdateImages = "Display:UpdateImages";
		gAppViewerp->pingMainloopTimeout(&DisplayUpdateImages);


		{
			LL_FAST_TIMER(FTM_IMAGE_UPDATE);

			{
				LL_FAST_TIMER(FTM_IMAGE_UPDATE_CLASS);
				LLViewerTexture::updateClass(gViewerCamera.getVelocityStat()->getMean(),
											 gViewerCamera.getAngularVelocityStat()->getMean());
			}

			{
				LL_FAST_TIMER(FTM_IMAGE_UPDATE_BUMP);
				// Must be called before gTextureList version so that its
				// textures are thrown out first.
				gBumpImageList.updateImages();
			}

			{
				LL_FAST_TIMER(FTM_IMAGE_UPDATE_LIST);
				// 50 ms/second decode time
				F32 max_image_decode_time = 0.050f * gFrameIntervalSeconds;
				// Min 2ms/frame, max 5ms/frame)
				max_image_decode_time = llclamp(max_image_decode_time, 0.002f,
												0.005f);
				gTextureList.updateImages(max_image_decode_time);
			}
		}

		LLGLState::check(true, true);

		///////////////////////////////////
		// StateSort
		//
		// Responsible for taking visible objects, and adding them to the
		// appropriate draw orders. In the case of alpha objects, z-sorts them
		// first. Also creates special lists for outlines and selected face
		// rendering.

		static const std::string DisplayStateSort = "Display:StateSort";
		gAppViewerp->pingMainloopTimeout(&DisplayStateSort);
		{
			LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
			gPipeline.stateSort(gViewerCamera, result);
			if (rebuild)
			{
				// Rebuild pools
				gPipeline.rebuildPools();
			}
		}

		LLGLState::check(true, true);

		LLPipeline::sUseOcclusion = occlusion;

		{
			static const std::string DisplaySky = "Display:Sky";
			gAppViewerp->pingMainloopTimeout(&DisplaySky);
			LL_FAST_TIMER(FTM_UPDATE_SKY);
			gSky.updateSky();
		}

		if (gUseWireframe)
		{
			glClearColor(0.5f, 0.5f, 0.5f, 0.f);
			glClear(GL_COLOR_BUFFER_BIT);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}

		LLVBOPool::deleteReleasedBuffers();

		static const std::string DisplayRenderStart = "Display:RenderStart";
		gAppViewerp->pingMainloopTimeout(&DisplayRenderStart);

#if 0
		// Render frontmost floater opaque for occlusion culling purposes
		LLFloater* frontmost_floaterp = gFloaterViewp->getFrontmost();
		// assumes frontmost floater with focus is opaque
		if (frontmost_floaterp &&
			gFocusMgr.childHasKeyboardFocus(frontmost_floaterp))
		{
			gGL.matrixMode(LLRender::MM_MODELVIEW);
			gGL.pushMatrix();
			{
				gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
				gGL.loadIdentity();

				LLRect floater_rect = frontmost_floaterp->getScreenRect();
				// deflate by one pixel so rounding errors don't occlude
				// outside of floater extents
				floater_rect.stretch(-1);
				F32 window_width = (F32)gViewerWindowp->getWindowWidth();
				F32 window_height = (F32)gViewerWindowp->getWindowHeight();
				LLRectf floater_3d_rect((F32)floater_rect.mLeft / window_width,
										(F32)floater_rect.mTop / window_height,
										(F32)floater_rect.mRight / window_width,
										(F32)floater_rect.mBottom / window_height);
				floater_3d_rect.translate(-0.5f, -0.5f);
				gGL.translatef(0.f, 0.f, -gViewerCamera.getNear());
				gGL.scalef(gViewerCamera.getNear() * gViewerCamera.getAspect() /
						   sinf(gViewerCamera.getView()),
						   gViewerCamera.getNear() / sinf(gViewerCamera.getView()),
						   1.f);
				gGL.color4fv(LLColor4::white.mV);
				gGL.begin(LLRender::TRIANGLES);
				{
					gGL.vertex3f(floater_3d_rect.mLeft,
								 floater_3d_rect.mBottom, 0.f);
					gGL.vertex3f(floater_3d_rect.mLeft,
								 floater_3d_rect.mTop, 0.f);
					gGL.vertex3f(floater_3d_rect.mRight,
								 floater_3d_rect.mTop, 0.f);
					gGL.vertex3f(floater_3d_rect.mLeft,
								 floater_3d_rect.mBottom, 0.f);
					gGL.vertex3f(floater_3d_rect.mRight,
								 floater_3d_rect.mTop, 0.f);
					gGL.vertex3f(floater_3d_rect.mRight,
								 floater_3d_rect.mBottom, 0.f);
				}
				gGL.end();
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
			gGL.popMatrix();
		}
#endif

		LLPipeline::sUnderWaterRender = gViewerCamera.cameraUnderWater();

		LLGLState::check(true, true);

		gGL.setColorMask(true, true);

		if (LLPipeline::sRenderDeferred)
		{
			gPipeline.mDeferredScreen.bindTarget();
			glClearColor(1.f, 0.f, 1.f, 1.f);
			gPipeline.mDeferredScreen.clear();
		}
		else
		{
			gPipeline.mScreen.bindTarget();
			if (LLPipeline::sUnderWaterRender &&
				!gPipeline.canUseWindLightShaders())
			{
				const LLColor4& col = LLDrawPoolWater::sWaterFogColor;
				glClearColor(col.mV[0], col.mV[1], col.mV[2], 0.f);
			}
			gPipeline.mScreen.clear();
		}

		gGL.setColorMask(true, false);

		static const std::string DisplayRenderGeom = "Display:RenderGeom";
		gAppViewerp->pingMainloopTimeout(&DisplayRenderGeom);

		if (!gRestoreGL &&
			!(gAppViewerp->logoutRequestSent() &&
			  gAppViewerp->hasSavedFinalSnapshot()))
		{
			LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

			static LLCachedControl<bool> render_depth_pre_pass(gSavedSettings,
															   "RenderDepthPrePass");
			if (render_depth_pre_pass)
			{
				gGL.setColorMask(false, false);

				static const U32 types[] =
				{
					LLRenderPass::PASS_SIMPLE,
					LLRenderPass::PASS_FULLBRIGHT,
					LLRenderPass::PASS_SHINY
				};
				constexpr U32 num_types = LL_ARRAY_SIZE(types);

				gOcclusionProgram.bind();
				for (U32 i = 0; i < num_types; ++i)
				{
					gPipeline.renderObjects(types[i],
											LLVertexBuffer::MAP_VERTEX,
											false);
				}

				gOcclusionProgram.unbind();
			}

			gGL.setColorMask(true, false);
			if (LLPipeline::sRenderDeferred)
			{
				gPipeline.renderGeomDeferred(gViewerCamera);
			}
			else
			{
				gPipeline.renderGeom(gViewerCamera);
			}

			gGL.setColorMask(true, true);

			stop_glerror();

			// Store this frame's modelview matrix for use when rendering next
			// frame's occlusion queries
			gGLLastModelView = gGLModelView;
			gGLLastProjection = gGLProjection;
		}

		{
			LL_FAST_TIMER(FTM_TEXTURE_UNBIND);
			for (S32 i = 0; i < gGLManager.mNumTextureImageUnits; ++i)
			{
				// Dummy cleanup of any currently bound textures
				LLTexUnit* texunit = gGL.getTexUnit(i);
				if (texunit && texunit->getCurrType() != LLTexUnit::TT_NONE)
				{
					texunit->unbind(texunit->getCurrType());
					texunit->disable();
				}
			}
		}

		static const std::string DisplayRenderFlush = "Display:RenderFlush";
		gAppViewerp->pingMainloopTimeout(&DisplayRenderFlush);

		LLRenderTarget& rt =
			LLPipeline::sRenderDeferred ? gPipeline.mDeferredScreen
										: gPipeline.mScreen;
		rt.flush();
		if (rt.getFBO() && LLRenderTarget::sUseFBO)
		{
			LLRenderTarget::copyContentsToFramebuffer(rt, 0, 0, rt.getWidth(),
													  rt.getHeight(), 0, 0,
													  rt.getWidth(),
													  rt.getHeight(),
#if LL_NEW_DEPTH_STENCIL
													  GL_STENCIL_BUFFER_BIT |
#endif
													  GL_DEPTH_BUFFER_BIT,
													  GL_NEAREST);
		}

		if (LLPipeline::sRenderDeferred)
		{
			gPipeline.renderDeferredLighting();
		}

		LLPipeline::sUnderWaterRender = false;

		static const std::string DisplayRenderUI = "Display:RenderUI";
		gAppViewerp->pingMainloopTimeout(&DisplayRenderUI);
		if (!for_snapshot)
		{
			render_ui();
		}

		LLSpatialGroup::sNoDelete = false;
		gPipeline.clearReferences();

		gPipeline.rebuildGroups();
	}

	stop_glerror();

	if (LLPipeline::sRenderFrameTest)
	{
		LLWorld::sendAgentResume();
		LLPipeline::sRenderFrameTest = false;
	}

	display_stats();

	static const std::string DisplayDone = "Display:Done";
	gAppViewerp->pingMainloopTimeout(&DisplayDone);

	gShiftFrame = gScreenIsDirty = false;

	if (gShaderProfileFrame)
	{
		gShaderProfileFrame = false;
		LLGLSLShader::finishProfile();
	}
}

void render_hud_attachments()
{
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();

	const LLMatrix4a current_proj = gGLProjection;
	const LLMatrix4a current_mod = gGLModelView;

	F32 current_zoom = gAgent.mHUDCurZoom;
	F32 target_zoom = gAgent.getHUDTargetZoom();
	if (current_zoom != target_zoom)
	{
		// Smoothly interpolate current zoom level
		gAgent.mHUDCurZoom = lerp(current_zoom, target_zoom,
								  LLCriticalDamp::getInterpolant(0.03f));
	}

	if (LLPipeline::sShowHUDAttachments && !gDisconnected &&
		setup_hud_matrices())
	{
		LLPipeline::sRenderingHUDs = true;
		LLCamera hud_cam = gViewerCamera;
		hud_cam.setOrigin(-1.f, 0.f, 0.f);
		hud_cam.setAxes(LLVector3::x_axis, LLVector3::y_axis,
						LLVector3::z_axis);
		LLViewerCamera::updateFrustumPlanes(hud_cam, true);

		static LLCachedControl<bool> render_hud_particles(gSavedSettings,
														  "RenderHUDParticles");
		bool render_particles =
			render_hud_particles &&
			gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES);

		// Only render hud objects
		gPipeline.pushRenderTypeMask();

		// Turn off everything
		gPipeline.andRenderTypeMask(LLPipeline::END_RENDER_TYPES);
		// Turn on HUD
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
		// Turn on HUD particles
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);

		// If particles are off, turn off hud-particles as well
		if (!render_particles)
		{
			// turn back off HUD particles
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
		}

		bool has_ui =
			gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);
		if (has_ui)
		{
			gPipeline.toggleRenderDebugFeature((void*)LLPipeline::RENDER_DEBUG_FEATURE_UI);
		}

		S32 use_occlusion = LLPipeline::sUseOcclusion;
		LLPipeline::sUseOcclusion = 0;

		// Cull, sort, and render hud objects
		static LLCullResult result;
		LLSpatialGroup::sNoDelete = true;

		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
		gPipeline.updateCull(hud_cam, result, 0, NULL, true);

		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_BUMP);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_SIMPLE);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_VOLUME);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_ALPHA);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_FULLBRIGHT_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_FULLBRIGHT);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_ALPHA);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_BUMP);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_MATERIAL);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_SHINY);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_INVISIBLE);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_INVISI_SHINY);

		gPipeline.stateSort(hud_cam, result);

		gPipeline.renderGeom(hud_cam);

		LLSpatialGroup::sNoDelete = false;
#if 0
		gPipeline.clearReferences();
#endif
		render_hud_elements();

		// Restore type mask
		gPipeline.popRenderTypeMask();

		if (has_ui)
		{
			gPipeline.toggleRenderDebugFeature((void*)LLPipeline::RENDER_DEBUG_FEATURE_UI);
		}
		LLPipeline::sUseOcclusion = use_occlusion;
		LLPipeline::sRenderingHUDs = false;
	}
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	gGLProjection = current_proj;
	gGLModelView = current_mod;
}

bool setup_hud_matrices()
{
	LLRect whole_screen = gViewerWindowp->getVirtualWindowRect();

	// Apply camera zoom transform (for high res screenshots)
	F32 zoom_factor = gViewerCamera.getZoomFactor();
	S16 sub_region = gViewerCamera.getZoomSubRegion();
	if (zoom_factor > 1.f)
	{
		S32 num_horizontal_tiles = llceil(zoom_factor);
		S32 tile_width = ll_roundp((F32)gViewerWindowp->getWindowWidth() /
								   zoom_factor);
		S32 tile_height = ll_roundp((F32)gViewerWindowp->getWindowHeight() /
								    zoom_factor);
		S32 tile_y = sub_region / num_horizontal_tiles;
		S32 tile_x = sub_region - (tile_y * num_horizontal_tiles);

		whole_screen.setLeftTopAndSize(tile_x * tile_width,
									   gViewerWindowp->getWindowHeight() -
									   (tile_y * tile_height),
									   tile_width, tile_height);
	}

	return setup_hud_matrices(whole_screen);
}

bool setup_hud_matrices(const LLRect& screen_region)
{
	LLMatrix4a proj, model;
	bool result = get_hud_matrices(screen_region, proj, model);
	if (result)
	{
		// Set up transform to keep HUD objects in front of camera
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.loadMatrix(proj);
		gGLProjection = proj;

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.loadMatrix(model);
		gGLModelView = model;
	}
	return result;
}

bool get_hud_matrices(LLMatrix4a& proj, LLMatrix4a& model)
{
	LLRect whole_screen = gViewerWindowp->getVirtualWindowRect();
	return get_hud_matrices(whole_screen, proj, model);
}

bool get_hud_matrices(const LLRect& screen_region,
					  LLMatrix4a& proj, LLMatrix4a& model)
{
	if (!isAgentAvatarValid() || !gAgentAvatarp->hasHUDAttachment())
	{
		return false;
	}

	LLBBox hud_bbox = gAgentAvatarp->getHUDBBox();

	F32 aspect_ratio = gViewerCamera.getAspect();
	F32 hud_depth = llmax(1.f, hud_bbox.getExtentLocal().mV[VX] * 1.1f);
	proj = gl_ortho(-0.5f * aspect_ratio, 0.5f * aspect_ratio, -0.5f, 0.5f,
					0.f, hud_depth);
	proj.getRow<2>().copyComponent<2>(LLVector4a(-0.01f));

	F32 wwidth = (F32)gViewerWindowp->getWindowWidth();
	F32 wheight = (F32)gViewerWindowp->getWindowHeight();
	F32 scale_x = wwidth / (F32)screen_region.getWidth();
	F32 scale_y = wheight /(F32)screen_region.getHeight();
	F32 delta_x = screen_region.getCenterX() - screen_region.mLeft;
	F32 delta_y = screen_region.getCenterY() - screen_region.mBottom;
	proj.applyTranslationAffine(clamp_rescale(delta_x, 0.f, wwidth,
											  0.5f * scale_x * aspect_ratio,
											  -0.5f * scale_x * aspect_ratio),
								clamp_rescale(delta_y, 0.f, wheight,
											  0.5f * scale_y,
											  -0.5f * scale_y), 0.f);
	proj.applyScaleAffine(scale_x, scale_y, 1.f);

	model = OGL_TO_CFR_ROT4A;

	model.applyTranslationAffine(LLVector3(hud_depth * 0.5f -
										   hud_bbox.getCenterLocal().mV[VX],
										   0.f, 0.f));

	return true;
}

void render_ui(F32 zoom_factor)
{
	gGL.flush();
	{
		LL_FAST_TIMER(FTM_RENDER_UI);

		LLGLState::check();

		const LLMatrix4a saved_view = gGLModelView;
		U32 orig_matrix = gGL.getMatrixMode();

		bool not_snaphot = !gSnapshot;
		if (not_snaphot)
		{
			gGL.pushMatrix();
			gGL.loadMatrix(gGLLastModelView);
			gGLModelView = gGLLastModelView;
		}

		// Finalize scene
		gPipeline.renderFinalize();

//MK
		{
			LL_TRACY_TIMER(TRC_RLV_RENDER_LIMITS);
			// Possibly draw a big black sphere around our avatar if the camera
			// render is limited
			if (gRLenabled && !gRLInterface.mRenderLimitRenderedThisFrame &&
				!(isAgentAvatarValid() && gAgentAvatarp->isFullyLoaded()))
			{
				gRLInterface.drawRenderLimit(true);
			}
		}
//mk

		render_hud_elements();
		render_hud_attachments();

		LLGLSDefault gls_default;
		LLGLSUIDefault gls_ui;

		gPipeline.disableLights();

		{
			gGL.color4f(1.f, 1.f, 1.f, 1.f);
			if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
			{
				if (!gDisconnected)
				{
					render_ui_3d();
					LLGLState::check();
				}
				else
				{
					render_disconnected_background();
				}

				render_ui_2d();
				LLGLState::check();
			}
			gGL.flush();

			{
				gViewerWindowp->setup2DRender();
				gViewerWindowp->updateDebugText();
				gViewerWindowp->drawDebugText();
			}

			LLVertexBuffer::unbind();
		}

		if (not_snaphot)
		{
			gGL.matrixMode(orig_matrix);
			gGLModelView = saved_view;
			gGL.popMatrix();
		}
		gGL.flush();
	}

	// Do not include this in FTM_RENDER_UI, since it calls glFinish(), which
	// will draw all sorts of non-UI stuff... HB
	if (gDisplaySwapBuffers)
	{
		gWindowp->swapBuffers();
	}
	gDisplaySwapBuffers = true;
}

void renderCoordinateAxes()
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.begin(LLRender::LINES);
		gGL.color3f(1.f, 0.f, 0.f);   // i direction = X-Axis = red
		gGL.vertex3f(0.f, 0.f, 0.f);
		gGL.vertex3f(2.f, 0.f, 0.f);
		gGL.vertex3f(3.f, 0.f, 0.f);
		gGL.vertex3f(5.f, 0.f, 0.f);
		gGL.vertex3f(6.f, 0.f, 0.f);
		gGL.vertex3f(8.f, 0.f, 0.f);
		// Make an X
		gGL.vertex3f(11.f, 1.f, 1.f);
		gGL.vertex3f(11.f, -1.f, -1.f);
		gGL.vertex3f(11.f, 1.f, -1.f);
		gGL.vertex3f(11.f, -1.f, 1.f);

		gGL.color3f(0.f, 1.f, 0.f);   // j direction = Y-Axis = green
		gGL.vertex3f(0.f, 0.f, 0.f);
		gGL.vertex3f(0.f, 2.f, 0.f);
		gGL.vertex3f(0.f, 3.f, 0.f);
		gGL.vertex3f(0.f, 5.f, 0.f);
		gGL.vertex3f(0.f, 6.f, 0.f);
		gGL.vertex3f(0.f, 8.f, 0.f);
		// Make a Y
		gGL.vertex3f(1.f, 11.f, 1.f);
		gGL.vertex3f(0.f, 11.f, 0.f);
		gGL.vertex3f(-1.f, 11.f, 1.f);
		gGL.vertex3f(0.f, 11.f, 0.f);
		gGL.vertex3f(0.f, 11.f, 0.f);
		gGL.vertex3f(0.f, 11.f, -1.f);

		gGL.color3f(0.f, 0.f, 1.f);   // Z-Axis = blue
		gGL.vertex3f(0.f, 0.f, 0.f);
		gGL.vertex3f(0.f, 0.f, 2.f);
		gGL.vertex3f(0.f, 0.f, 3.f);
		gGL.vertex3f(0.f, 0.f, 5.f);
		gGL.vertex3f(0.f, 0.f, 6.f);
		gGL.vertex3f(0.f, 0.f, 8.f);
		// Make a Z
		gGL.vertex3f(-1.f, 1.f, 11.f);
		gGL.vertex3f(1.f, 1.f, 11.f);
		gGL.vertex3f(1.f, 1.f, 11.f);
		gGL.vertex3f(-1.f, -1.f, 11.f);
		gGL.vertex3f(-1.f, -1.f, 11.f);
		gGL.vertex3f(1.f, -1.f, 11.f);
	gGL.end();
}

void draw_axes()
{
	LLGLSUIDefault gls_ui;
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	// A vertical white line at origin
	LLVector3 v = gAgent.getPositionAgent();
	gGL.begin(LLRender::LINES);
		gGL.color3f(1.f, 1.f, 1.f);
		gGL.vertex3f(0.f, 0.f, 0.f);
		gGL.vertex3f(0.f, 0.f, 40.f);
	gGL.end();
	// Some coordinate axes
	gGL.pushMatrix();
		gGL.translatef(v.mV[VX], v.mV[VY], v.mV[VZ]);
		renderCoordinateAxes();
	gGL.popMatrix();
}

void render_ui_3d()
{
	LLGLSPipeline gls_pipeline;

	//////////////////////////////////////
	// Render 3D UI elements
	// NOTE: zbuffer is cleared before we get here by LLDrawPoolHUD,
	//		 so 3d elements requiring Z buffer are moved to LLDrawPoolHUD

	/////////////////////////////////////////////////////////////
	// Render 2.5D elements (2D elements in the world)
	// Stuff without z writes

	// Debugging stuff goes before the UI.

	gUIProgram.bind();

	// Coordinate axes
	static LLCachedControl<bool> show_axes(gSavedSettings, "ShowAxes");
	if (show_axes)
	{
		draw_axes();
	}

	// Non HUD call in render_hud_elements
	gViewerWindowp->renderSelections(false, false, true);

	stop_glerror();
}

// Renders 2D UI elements that overlay the world (no z compare)
void render_ui_2d()
{
	LLGLSUIDefault gls_ui;

	//  Disable wireframe mode below here, as this is HUD/menus
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// Menu overlays, HUD, etc
	gViewerWindowp->setup2DRender();

	F32 zoom_factor = gViewerCamera.getZoomFactor();
	S16 sub_region = gViewerCamera.getZoomSubRegion();

	if (zoom_factor > 1.f)
	{
		// Decompose subregion number to x and y values
		S32 pos_y = sub_region / llceil(zoom_factor);
		S32 pos_x = sub_region - pos_y * llceil(zoom_factor);
		// offset for this tile
		LLFontGL::sCurOrigin.mX -=
			ll_round((F32)gViewerWindowp->getWindowWidth() * (F32)pos_x /
					 zoom_factor);
		LLFontGL::sCurOrigin.mY -=
			ll_round((F32)gViewerWindowp->getWindowHeight() * (F32)pos_y /
					 zoom_factor);
	}

	// Render outline for HUD
	if (isAgentAvatarValid() && gAgent.mHUDCurZoom < 0.98f)
	{
		gGL.pushMatrix();
		S32 half_width = gViewerWindowp->getWindowWidth() / 2;
		S32 half_height = gViewerWindowp->getWindowHeight() / 2;
		gGL.scalef(LLUI::sGLScaleFactor.mV[0], LLUI::sGLScaleFactor.mV[1],
				   1.f);
		gGL.translatef((F32)half_width, (F32)half_height, 0.f);
		F32 zoom = gAgent.mHUDCurZoom;
		gGL.scalef(zoom, zoom, 1.f);
		gGL.color4fv(LLColor4::white.mV);
		gl_rect_2d(-half_width, half_height, half_width, -half_height, false);
		gGL.popMatrix();
	}
	gViewerWindowp->draw();

	// Reset current origin for font rendering, in case of tiling render
	LLFontGL::sCurOrigin.set(0, 0);

	stop_glerror();
}

void render_disconnected_background()
{
	gUIProgram.bind();

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	gGL.color4f(1.f, 1.f, 1.f, 1.f);
	if (!gDisconnectedImagep && gDisconnected)
	{
		std::string temp = gDirUtilp->getLindenUserDir() + LL_DIR_DELIM_STR;
		if (gIsInProductionGrid)
		{
			 temp += SCREEN_LAST_FILENAME;
		}
		else
		{
			 temp += SCREEN_LAST_BETA_FILENAME;
		}
		LLPointer<LLImageBMP> image_bmp = new LLImageBMP;
		if (!image_bmp->load(temp))
		{
			return;
		}
		llinfos << "Loaded last bitmap: " << temp << llendl;

		LLPointer<LLImageRaw> raw = new LLImageRaw;
		if (!image_bmp->decode(raw))
		{
			llwarns << "Bitmap decode failed" << llendl;
			gDisconnectedImagep = NULL;
			return;
		}

		U8* rawp = raw->getData();
		S32 npixels = (S32)image_bmp->getWidth() * (S32)image_bmp->getHeight();
		for (S32 i = 0; i < npixels; ++i)
		{
			S32 sum = 0;
			sum = *rawp + *(rawp + 1) + *(rawp + 2);
			sum /= 3;
			*rawp = ((S32)sum * 6 + *rawp) / 7;
			rawp++;
			*rawp = ((S32)sum * 6 + *rawp) / 7;
			rawp++;
			*rawp = ((S32)sum * 6 + *rawp) / 7;
			rawp++;
		}

		raw->expandToPowerOfTwo();
		gDisconnectedImagep = LLViewerTextureManager::getLocalTexture(raw.get(),
																	  false);
		gStartTexture = gDisconnectedImagep;
		unit0->unbind(LLTexUnit::TT_TEXTURE);
	}

	// Make sure the progress view always fills the entire window.
	S32 width = gViewerWindowp->getWindowWidth();
	S32 height = gViewerWindowp->getWindowHeight();

	if (gDisconnectedImagep)
	{
		LLGLSUIDefault gls_ui;
		gViewerWindowp->setup2DRender();
		gGL.pushMatrix();
		{
			// scale ui to reflect UIScaleFactor
			// this can't be done in setup2DRender because it requires a
			// pushMatrix/popMatrix pair
			const LLVector2& display_scale = gViewerWindowp->getDisplayScale();
			gGL.scalef(display_scale.mV[VX], display_scale.mV[VY], 1.f);

			unit0->bind(gDisconnectedImagep);
			gGL.color4f(1.f, 1.f, 1.f, 1.f);
			gl_rect_2d_simple_tex(width, height);
			unit0->unbind(LLTexUnit::TT_TEXTURE);
		}
		gGL.popMatrix();
	}
	gGL.flush();

	gUIProgram.unbind();
}

void display_cleanup()
{
	gDisconnectedImagep = NULL;
}

void hud_render_text(const LLWString& wstr, const LLVector3& pos_agent,
					 const LLFontGL& font, U8 style, F32 x_offset,
					 F32 y_offset, const LLColor4& color, bool orthographic)
{
	// Do cheap plane culling
	LLVector3 dir_vec = pos_agent - gViewerCamera.getOrigin();
	dir_vec /= dir_vec.length();

	if (wstr.empty() ||
		(!orthographic && dir_vec * gViewerCamera.getAtAxis() <= 0.f))
	{
		return;
	}

	LLVector3 right_axis;
	LLVector3 up_axis;
	if (orthographic)
	{
		F32 height_inv = 1.f / (F32)gViewerWindowp->getWindowHeight();
		right_axis.set(0.f, -height_inv, 0.f);
		up_axis.set(0.f, 0.f, height_inv);
	}
	else
	{
		gViewerCamera.getPixelVectors(pos_agent, up_axis, right_axis);
	}
	LLQuaternion rot;
	if (!orthographic)
	{
		rot = gViewerCamera.getQuaternion();
		rot = rot * LLQuaternion(-F_PI_BY_TWO, gViewerCamera.getYAxis());
		rot = rot * LLQuaternion(F_PI_BY_TWO, gViewerCamera.getXAxis());
	}
	else
	{
		rot = LLQuaternion(-F_PI_BY_TWO, LLVector3::z_axis);
		rot = rot * LLQuaternion(-F_PI_BY_TWO, LLVector3::y_axis);
	}
	F32 angle;
	LLVector3 axis;
	rot.getAngleAxis(&angle, axis);

	LLVector3 render_pos = pos_agent + floorf(x_offset) * right_axis +
						   floorf(y_offset) * up_axis;

	// Get the render_pos in screen space
	LLVector3 win_coord;
	LLRect viewport(gGLViewport[0], gGLViewport[1] + gGLViewport[3],
					gGLViewport[0] + gGLViewport[2], gGLViewport[1]);
	gGL.projectf(render_pos, gGLModelView, gGLProjection, viewport, win_coord);

	// Fonts all render orthographically, set up projection
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	LLUI::pushMatrix();

	gl_state_for_2d(gViewerWindowp->getWindowDisplayWidth(),
					gViewerWindowp->getWindowDisplayHeight());
	gViewerWindowp->setupViewport();

	LLUI::loadIdentity();
	gGL.loadIdentity();
	LLUI::translate(win_coord.mV[VX] / LLFontGL::sScaleX,
					win_coord.mV[VY] / LLFontGL::sScaleY,
					-2.f * win_coord.mV[VZ] + 1.f);
	F32 right_x;
	font.render(wstr, 0, 0, 0, color, LLFontGL::LEFT, LLFontGL::BASELINE,
				style, wstr.length(), 1000, &right_x);

	LLUI::popMatrix();
	gGL.popMatrix();

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
}

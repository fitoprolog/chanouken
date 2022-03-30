/**
 * @file llviewercontrol.cpp
 * @brief Viewer configuration
 * @author Richard Nelson
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

#include "llviewercontrol.h"

#include "llaudioengine.h"
#include "llavatarnamecache.h"
#include "llconsole.h"
#include "llcorehttpcommon.h"
#include "llerrorcontrol.h"
#include "llfloater.h"
#include "llgl.h"
#include "llimagegl.h"
#include "llkeyboard.h"
#include "llnotifications.h"
#include "llparcel.h"
#include "llrender.h"
#include "llspellcheck.h"
#include "llsys.h"
#include "llversionviewer.h"
#include "llvolume.h"
#include "llxmlrpctransaction.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "llappviewer.h"
#include "llavatartracker.h"
#include "llchatbar.h"
#include "lldebugview.h"
#include "lldrawpoolbump.h"
#include "lldrawpoolterrain.h"
#include "lldrawpooltree.h"
#include "llenvironment.h"
#include "llfasttimerview.h"
#include "llfeaturemanager.h"
#include "llflexibleobject.h"
#include "hbfloaterdebugtags.h"
#include "hbfloatereditenvsettings.h"
#include "hbfloatersearch.h"
#include "llfloaterstats.h"
#include "llfloaterwindlight.h"
#include "llgridmanager.h"				// For gIsInSecondLife
#include "llgroupnotify.h"
#include "llhudeffectlookat.h"
#include "llmeshrepository.h"
#include "llpanelminimap.h"
#include "llpipeline.h"
#include "llselectmgr.h"
#include "llskinningutil.h"
#include "llsky.h"
#include "llstartup.h"
#include "llstatusbar.h"
#include "lltoolbar.h"
#include "lltracker.h"
#include "llvieweraudio.h"
#include "hbviewerautomation.h"
#include "llviewerdisplay.h"
#include "llviewerjoystick.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llviewerthrottle.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvocache.h"
#include "llvoiceclient.h"
#include "llvosky.h"
#include "llvosurfacepatch.h"
#include "llvotree.h"
#include "llvovolume.h"
#include "llvowlsky.h"
#include "llwlskyparammgr.h"
#include "llworld.h"

std::map<std::string, LLControlGroup*> gSettings;
// Those two are saved at end of the session
LLControlGroup gSavedSettings("Global");
LLControlGroup gSavedPerAccountSettings("PerAccount");
// Read-only
LLControlGroup gColors("Colors");

extern bool gUpdateDrawDistance;

////////////////////////////////////////////////////////////////////////////
// Listeners

static bool handleAllowSwappingChanged(const LLSD& newvalue)
{
	LLMemoryInfo::setAllowSwapping(newvalue.asBoolean());
	return true;
}

static bool handleDebugPermissionsChanged(const LLSD& newvalue)
{
	dialog_refresh_all();
	return true;
}

#if LL_FAST_TIMERS_ENABLED
static bool handleFastTimersAlwaysEnabledChanged(const LLSD& newvalue)
{
	if (gFastTimerViewp && gFastTimerViewp->getVisible())
	{
		// Nothing to do
		return true;
	}
	if (newvalue.asBoolean())
	{
		gEnableFastTimers = true;
		llinfos << "Fast timers enabled." << llendl;
	}
	else
	{
		gEnableFastTimers = false;
		llinfos << "Fast timers disabled." << llendl;
	}
	return true;
}
#endif	// LL_FAST_TIMERS_ENABLED

static bool handleRenderCompressTexturesChanged(const LLSD& newvalue)
{
	if (gFeatureManager.isFeatureAvailable("RenderCompressTextures") &&
		gGLManager.mHasVertexBufferObject)
	{
		LLImageGL::sCompressTextures = gSavedSettings.getBool("RenderCompressTextures");
		LLImageGL::sCompressThreshold = gSavedSettings.getU32("RenderCompressThreshold");
	}
	return true;
}

static bool handleRenderClearARBBufferChanged(const LLSD& newvalue)
{
	if (!gGLManager.mIsNVIDIA) // Never disable for NVIDIA
	{
		gClearARBbuffer = newvalue.asBoolean();
	}
	return true;
}

static bool handleRenderFarClipChanged(const LLSD& newvalue)
{
	gUpdateDrawDistance = true;	// Updated in llviewerdisplay.cpp
	return true;
}

static bool handleTerrainDetailChanged(const LLSD& newvalue)
{
	LLDrawPoolTerrain::sDetailMode = newvalue.asInteger();
	return true;
}

static bool handleSetShaderChanged(const LLSD& newvalue)
{
	// Changing shader level may invalidate existing cached bump maps, as the
	// shader type determines the format of the bump map it expects - clear
	// and repopulate the bump cache
	gBumpImageList.destroyGL();
	gBumpImageList.restoreGL();

	bool old_state = LLPipeline::sRenderDeferred;
	LLPipeline::refreshCachedSettings();
	// ALM depends onto atmospheric shaders, which state might have changed
	if (gPipeline.isInit() && old_state != LLPipeline::sRenderDeferred)
	{
		gPipeline.releaseGLBuffers();
		gPipeline.createGLBuffers();
		gPipeline.resetVertexBuffers();
	}

	gViewerShaderMgrp->setShaders();
	return true;
}

static bool handleRenderLightingDetailChanged(const LLSD& newvalue)
{
	gPipeline.setLightingDetail(-1);
	gViewerShaderMgrp->setShaders();
	return true;
}

static bool handleRenderDeferredChanged(const LLSD& newvalue)
{
	LLRenderTarget::sUseFBO = newvalue.asBoolean();
	if (gPipeline.isInit())
	{
		LLPipeline::refreshCachedSettings();
		gPipeline.releaseGLBuffers();
		gPipeline.createGLBuffers();
		gPipeline.resetVertexBuffers();
		gViewerShaderMgrp->setShaders();
	}
	return true;
}

static bool handleAvatarPhysicsChanged(const LLSD& newvalue)
{
	LLVOAvatar::sAvatarPhysics = newvalue.asBoolean();
	gAgent.sendAgentSetAppearance();
	return true;
}

static bool handleBakeOnMeshUploadsChanged(const LLSD& newvalue)
{
	gAgent.setUploadedBakesLimit();
	return true;
}

static bool handleRenderTransparentWaterChanged(const LLSD& newvalue)
{
	LLPipeline::refreshCachedSettings();
	gWorld.updateWaterObjects();
	return true;
}

static bool handleMeshMaxConcurrentRequestsChanged(const LLSD& newvalue)
{
	LLMeshRepoThread::sMaxConcurrentRequests = (U32) newvalue.asInteger();
	return true;
}

static bool handleShadowsResized(const LLSD& newvalue)
{
	gResizeShadowTexture = true;
	return true;
}

static bool handleReleaseGLBufferChanged(const LLSD& newvalue)
{
	if (gPipeline.isInit())
	{
		gPipeline.releaseGLBuffers();
		gPipeline.createGLBuffers();
	}
	return true;
}

static bool handleLUTBufferChanged(const LLSD& newvalue)
{
	if (gPipeline.isInit())
	{
		gPipeline.releaseLUTBuffers();
		gPipeline.createLUTBuffers();
	}
	return true;
}

static bool handleAnisotropicChanged(const LLSD& newvalue)
{
	LLImageGL::sGlobalUseAnisotropic = newvalue.asBoolean();
	LLImageGL::dirtyTexOptions();
	return true;
}

static bool handleRenderFSAASamplesChanged(const LLSD& newvalue)
{
	handleReleaseGLBufferChanged(newvalue);
	return true;
}

static bool handleVolumeSettingsChanged(const LLSD& newvalue)
{
	LLVOVolume::updateSettings();
	return true;
}

static bool handleSkyUseClassicCloudsChanged(const LLSD& newvalue)
{
	if (!newvalue.asBoolean())
	{
		gWorld.killClouds();
	}
	return true;
}

static bool handleTerrainLODChanged(const LLSD& newvalue)
{
	LLVOSurfacePatch::sLODFactor = (F32)newvalue.asReal();
	// Square lod factor to get exponential range of [0, 4] and keep a value of
	// 1 in the middle of the detail slider for consistency with other detail
	// sliders (see panel_preferences_graphics1.xml)
	LLVOSurfacePatch::sLODFactor *= LLVOSurfacePatch::sLODFactor;
	return true;
}

static bool handleTreeSettingsChanged(const LLSD& newvalue)
{
	LLVOTree::updateSettings();
	return true;
}

static bool handleFlexLODChanged(const LLSD& newvalue)
{
	LLVolumeImplFlexible::sUpdateFactor = (F32)newvalue.asReal();
	return true;
}

static bool handleGammaChanged(const LLSD& newvalue)
{
	F32 gamma = (F32)newvalue.asReal();
	if (gWindowp && gamma != gWindowp->getGamma())
	{
		// Only save it if it changed
		if (!gWindowp->setGamma(gamma))
		{
			llwarns << "Failed to set the display gamma to " << gamma
					<< ". Restoring the default gamma." << llendl;
			gWindowp->restoreGamma();
		}
	}

	return true;
}

constexpr F32 MAX_USER_FOG_RATIO = 10.f;
constexpr F32 MIN_USER_FOG_RATIO = 0.5f;

static bool handleFogRatioChanged(const LLSD& newvalue)
{
	F32 fog_ratio = llmax(MIN_USER_FOG_RATIO,
						  llmin((F32)newvalue.asReal(), MAX_USER_FOG_RATIO));
	gSky.setFogRatio(fog_ratio);
	return true;
}

static bool handleMaxPartCountChanged(const LLSD& newvalue)
{
	LLViewerPartSim::setMaxPartCount(newvalue.asInteger());
	return true;
}

static bool handleVideoMemoryChanged(const LLSD& newvalue)
{
	// Note: not using newvalue.asInteger() because this callback is also
	// used after updating MaxBoundTexMem. HB
	gTextureList.updateMaxResidentTexMem(gSavedSettings.getS32("TextureMemory"));
	return true;
}

static bool handleBandwidthChanged(const LLSD& newvalue)
{
	gViewerThrottle.setMaxBandwidth((F32) newvalue.asReal());
	return true;
}

static bool handleDebugConsoleMaxLinesChanged(const LLSD& newvalue)
{
	if (gDebugViewp && gDebugViewp->mDebugConsolep)
	{
		gDebugViewp->mDebugConsolep->setMaxLines(newvalue.asInteger());
	}
	return true;
}

static bool handleChatConsoleMaxLinesChanged(const LLSD& newvalue)
{
	if (gConsolep)
	{
		gConsolep->setMaxLines(newvalue.asInteger());
	}
	return true;
}

static bool handleChatFontSizeChanged(const LLSD& newvalue)
{
	if (gConsolep)
	{
		gConsolep->setFontSize(newvalue.asInteger());
	}
	return true;
}

static bool handleChatPersistTimeChanged(const LLSD& newvalue)
{
	if (gConsolep)
	{
		gConsolep->setLinePersistTime((F32) newvalue.asReal());
	}
	return true;
}

static bool handleAudioVolumeChanged(const LLSD& newvalue)
{
	audio_update_volume(true);
	return true;
}

static bool handleStackMinimizedTopToBottom(const LLSD& newvalue)
{
	LLFloaterView::setStackMinimizedTopToBottom(newvalue.asBoolean());
	return true;
}

static bool handleStackMinimizedRightToLeft(const LLSD& newvalue)
{
	LLFloaterView::setStackMinimizedRightToLeft(newvalue.asBoolean());
	return true;
}

static bool handleStackScreenWidthFraction(const LLSD& newvalue)
{
	LLFloaterView::setStackScreenWidthFraction(newvalue.asInteger());
	return true;
}

static bool handleJoystickChanged(const LLSD& newvalue)
{
	LLViewerJoystick::getInstance()->setCameraNeedsUpdate(true);
	return true;
}

static bool handleAvatarOffsetChanged(const LLSD& newvalue)
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->scheduleHoverUpdate();
	}
	return true;
}

static bool handleCameraCollisionsChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gAgent.setCameraCollidePlane(LLVector4(0.f, 0.f, 0.f, 1.f));
	}
	return true;
}

static bool handleCameraChanged(const LLSD& newvalue)
{
	gAgent.setupCameraView();
	return true;
}

static bool handleTrackFocusObjectChanged(const LLSD& newvalue)
{
	gAgent.setObjectTracking(newvalue.asBoolean());
	return true;
}

static bool handlePrimMediaChanged(const LLSD& newvalue)
{
	LLVOVolume::initSharedMedia();
	return true;
}

static bool handleAudioStreamMusicChanged(const LLSD& newvalue)
{
	if (gAudiop)
	{
		if (newvalue.asBoolean())
		{
			LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
			if (parcel && !parcel->getMusicURL().empty())
			{
				// If stream is already playing, do not call this otherwise
				// music will briefly stop
				if (!gAudiop->isInternetStreamPlaying())
				{
					LLViewerParcelMedia::playStreamingMusic(parcel);
				}
			}
		}
		else
		{
			gAudiop->stopInternetStream();
		}
	}
	return true;
}

static bool handleUseNewShadersChanged(const LLSD& newvalue)
{
	if (gUseNewShaders != newvalue.asBoolean())
	{
		gUseNewShaders = newvalue.asBoolean();
		if (gViewerWindowp && gViewerShaderMgrp)
		{
			// Sadly, pausing the texture workers (in LLViewerWindow::stopGL())
			// is not enough to avoid partly loaded textures when toggling this
			// setting while logged in...
			// *TODO: implement a pausing of texture fetch *queueing* and delay
			// the renderer toggling until all *currently* queued textures have
			// been fully fetched, decoded and cached...
			gViewerWindowp->stopGL();
			gViewerShaderMgrp->unloadShaders();
			gViewerShaderMgrp->setShaders();
			gViewerWindowp->restoreGL();		
		}
	}
	return true;
}

static bool handleUseOcclusionChanged(const LLSD& newvalue)
{
	LLPipeline::sUseOcclusion = (newvalue.asBoolean() && !gUseWireframe &&
								 gFeatureManager.isFeatureAvailable("UseOcclusion") &&
								 gGLManager.mHasOcclusionQuery) ? 2 : 0;
	return true;
}

static bool handleNumpadControlChanged(const LLSD& newvalue)
{
	if (gKeyboardp)
	{
		gKeyboardp->setNumpadDistinct((LLKeyboard::e_numpad_distinct)newvalue.asInteger());
	}
	return true;
}

static bool handleWLSkyDetailChanged(const LLSD& newvalue)
{
	LLVOWLSky::updateSettings();
	return true;
}

static bool handleRenderBatchedGlyphsChanged(const LLSD& newvalue)
{
	LLFontGL::setUseBatchedRender((bool)newvalue.asBoolean());
	return true;
}

static bool handleResetVertexBuffersChanged(const LLSD&)
{
	LLVOVolume::sRenderMaxVBOSize = gSavedSettings.getU32("RenderMaxVBOSize");
	if (gPipeline.isInit())
	{
		gPipeline.resetVertexBuffers();
	}
	LLVOTree::updateSettings();

	return true;
}

static bool handleRenderObjectBumpChanged(const LLSD& newvalue)
{
	if (gPipeline.isInit())
	{
		LLPipeline::sRenderBump = newvalue.asBoolean();
		gPipeline.updateRenderDeferred();
		gPipeline.releaseGLBuffers();
		gPipeline.createGLBuffers();
		gPipeline.resetVertexBuffers();
		gViewerShaderMgrp->setShaders();
	}

	return true;
}

static bool handleRenderOptimizeMeshVertexCacheChanged(const LLSD& newvalue)
{
	bool enabled = newvalue.asBoolean();
	LLVolume::sOptimizeCache = enabled;
	return true;
}

static bool handleNoVerifySSLCertChanged(const LLSD& newvalue)
{
	LLXMLRPCTransaction::setVerifyCert(!newvalue.asBoolean());
	return true;
}

static bool handleEnableHTTP2Changed(const LLSD& newvalue)
{
	LLCore::LLHttp::gEnabledHTTP2 = newvalue.asBoolean();
	return true;
}

static bool handlePingInterpolateChanged(const LLSD& newvalue)
{
	LLViewerObject::setPingInterpolate(newvalue.asBoolean());
	return true;
}

static bool handleVelocityInterpolateChanged(const LLSD& newvalue)
{
	LLViewerObject::setVelocityInterpolate(newvalue.asBoolean());
	return true;
}

static bool handleInterpolationTimesChanged(const LLSD& newvalue)
{
	LLViewerObject::setUpdateInterpolationTimes(gSavedSettings.getF32("InterpolationTime"),
												gSavedSettings.getF32("InterpolationPhaseOut"),
												gSavedSettings.getF32("RegionCrossingInterpolationTime"));
	return true;
}

static bool handleRepartition(const LLSD&)
{
	if (gPipeline.isInit())
	{
		gOctreeMaxCapacity = gSavedSettings.getU32("OctreeMaxNodeCapacity");
		gOctreeMinSize = gSavedSettings.getF32("OctreeMinimumNodeSize");
		gObjectList.repartitionObjects();
	}
	return true;
}

static bool handleRenderDynamicLODChanged(const LLSD& newvalue)
{
	LLPipeline::sDynamicLOD = newvalue.asBoolean();
	return true;
}

static bool handleAvatarDebugSettingsChanged(const LLSD&)
{
	LLVOAvatar::updateSettings();
	return true;
}

static bool handleDisplayNamesUsageChanged(const LLSD& newvalue)
{
	LLAvatarNameCache::setUseDisplayNames((U32)newvalue.asInteger());
	LLVOAvatar::invalidateNameTags();
	gAvatarTracker.dirtyBuddies();
	return true;
}

static bool handleOmitResidentAsLastNameChanged(const LLSD& newvalue)
{
	LLAvatarName::sOmitResidentAsLastName =(bool)newvalue.asBoolean();
	LLVOAvatar::invalidateNameTags();
	gAvatarTracker.dirtyBuddies();
	return true;
}

static bool handleLegacyNamesForFriendsChanged(const LLSD& newvalue)
{
	LLAvatarName::sLegacyNamesForFriends = (bool)newvalue.asBoolean();
	gAvatarTracker.dirtyBuddies();
	return true;
}

static bool handleLegacyNamesForSpeakersChanged(const LLSD& newvalue)
{
	LLAvatarName::sLegacyNamesForSpeakers = (bool)newvalue.asBoolean();
	return true;
}

static bool handleRenderDebugGLChanged(const LLSD& newvalue)
{
	gDebugGL = newvalue.asBoolean();
	gGL.clearErrors();
	return true;
}

static bool handleRenderResolutionDivisorChanged(const LLSD&)
{
	gResizeScreenTexture = true;
	return true;
}

static bool handleDebugObjectIdChanged(const LLSD& newvalue)
{
	LLUUID obj_id;
	obj_id.set(newvalue.asString(), false);
	LLViewerObject::setDebugObjectId(obj_id);
	return true;
}

static bool handleDebugViewsChanged(const LLSD& newvalue)
{
	LLView::sDebugRects = newvalue.asBoolean();
	return true;
}

static bool handleFSFlushOnWriteChanged(const LLSD& newvalue)
{
	LLFile::sFlushOnWrite = newvalue.asBoolean();
	return true;
}

static bool handleLogFileChanged(const LLSD& newvalue)
{
	std::string log_filename = newvalue.asString();
	LLFile::remove(log_filename);
	LLError::logToFile(log_filename);
	gAppViewerp->clearLogFilename();

	return true;
}

static bool handleTextureFetchBoostWithFetchesChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gNotifications.add("TextureFetchesBoostWithFetches");
	}
	return true;
}

static bool handleTextureFetchBoostWithSpeedChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gNotifications.add("TextureFetchesBoostWithSpeed");
	}
	return true;
}

static bool handleRestrainedLoveRelaxedTempAttachChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gNotifications.add("RLVRelaxedTempAttach");
	}
	return true;
}

static bool handleRestrainedLoveAutomaticRenameItemsChanged(const LLSD& newvalue)
{
	if (!newvalue.asBoolean())
	{
		gNotifications.add("RLVNoAttachmentAutoRename");
	}
	return true;
}

bool handleHideGroupTitleChanged(const LLSD& newvalue)
{
	gAgent.setHideGroupTitle(newvalue);
	return true;
}

bool handleDebugShowRenderInfoChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gPipeline.mNeedsDrawStats = true;
	}
	else if (!LLFloaterStats::findInstance())
	{
		gPipeline.mNeedsDrawStats = false;
	}
	return true;
}

bool handleEffectColorChanged(const LLSD& newvalue)
{
	gAgent.setEffectColor(LLColor4(newvalue));
	return true;
}

bool handleVoiceClientPrefsChanged(const LLSD& newvalue)
{
	if (LLVoiceClient::sInitDone)
	{
		gVoiceClient.updateSettings();
	}
	return true;
}

static bool handleMiniMapCenterChanged(const LLSD& newvalue)
{
	LLPanelMiniMap::sMiniMapCenter = newvalue.asInteger();
	return true;
}

static bool handleMiniMapRotateChanged(const LLSD& newvalue)
{
	LLPanelMiniMap::sMiniMapRotate = newvalue.asBoolean();
	return true;
}

static bool handleToolbarButtonsChanged(const LLSD&)
{
	if (gToolBarp)
	{
		gToolBarp->layoutButtons();
	}
	return true;
}

static bool handleSpellCheckChanged(const LLSD& newvalue)
{
	LLSpellCheck::getInstance()->setSpellCheck(gSavedSettings.getBool("SpellCheck"));
	LLSpellCheck::getInstance()->setShowMisspelled(gSavedSettings.getBool("SpellCheckShow"));
	LLSpellCheck::getInstance()->setDictionary(gSavedSettings.getString("SpellCheckLanguage"));
	return true;
}

static bool handleLanguageChanged(const LLSD& newvalue)
{
	gAgent.updateLanguage();
	return true;
}

static bool handleUseOldStatusBarIconsChanged(const LLSD&)
{
	if (gStatusBarp)
	{
		gStatusBarp->setIcons();
	}
	return true;
}

static bool handleSwapShoutWhisperShortcutsChanged(const LLSD& newvalue)
{
	LLChatBar::sSwappedShortcuts = newvalue.asBoolean();
	return true;
}

static bool handleMainloopTimeoutDefaultChanged(const LLSD& newvalue)
{
	gAppViewerp->setMainloopTimeoutDefault(newvalue.asReal());
	return true;
}

static bool handleThreadedFilesystemChanged(const LLSD& newvalue)
{
	if (!newvalue.asBoolean())
	{
		// We need a threaded LFS for io_uring/IOCP support... HB
		gSavedSettings.setBool("UseIOUring", false);
	}
	return true;
}

static bool handleUseIOUringChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		// We need a threaded LFS for io_uring/IOCP support... HB
		gSavedSettings.setBool("ThreadedFilesystem", true);
	}
	return true;
}

static bool handleUISettingsChanged(const LLSD& newvalue)
{
	LLUI::refreshSettings();
	return true;
}

static bool handleMainMemoryMarginsChanged(const LLSD& newvalue)
{
	LLMemory::setSafetyMarginKB(gSavedSettings.getU32("MainMemorySafetyMargin") * 1024,
								gSavedSettings.getF32("SafetyMargin1stStepRatio"));
	return true;
}

static bool handleSearchURLChanged(const LLSD& newvalue)
{
	// Do not propagate in OpenSim, since handleSearchURLChanged() is only
	// called for SL-specific settings (the search URL in OpenSim is set
	// via simulator features, not via saved settings).
	if (!gIsInSecondLife)
	{
		HBFloaterSearch::setSearchURL(newvalue.asString());
	}
	return true;
}

static bool handleVOCacheSettingChanged(const LLSD& newvalue)
{
	LLVOCacheEntry::updateSettings();
	return true;
}

static bool handleUse360InterestListSettingChanged(const LLSD& newvalue)
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		regionp->setInterestListMode();
	}
	return true;
}

static bool handleShowPropLinesAtWaterSurfaceChanged(const LLSD&)
{
	// Force an update of the property lines
	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		(*iter)->dirtyHeights();
	}
	return true;
}

static bool handleLightshareEnabledChanged(const LLSD& newvalue)
{
	if (!newvalue.asBoolean() && LLStartUp::isLoggedIn())
	{
		gWLSkyParamMgr.processLightshareReset(true);
	}
	return true;
}

static bool handleUseLocalEnvironmentChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		gWLSkyParamMgr.setDirty();
		gWLSkyParamMgr.animate(false);
		gSavedSettings.setBool("UseParcelEnvironment", false);
	}
	else
	{
		HBFloaterLocalEnv::closeInstance();
	}
	return true;
}

static bool handleUseParcelEnvironmentChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		LLFloaterWindlight::hideInstance();
		HBFloaterLocalEnv::closeInstance();
		gWLSkyParamMgr.setDirty();
		gWLSkyParamMgr.animate(false);
		gSavedSettings.setBool("UseLocalEnvironment", false);
		gSavedSettings.setBool("UseWLEstateTime", false);
		gEnvironment.clearEnvironment(LLEnvironment::ENV_LOCAL);
		gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_PARCEL,
											LLEnvironment::TRANSITION_INSTANT);
		if (gAutomationp)
		{
			gAutomationp->onWindlightChange("parcel", "", "");
		}
	}
	return true;
}

static bool handleUseWLEstateTimeChanged(const LLSD& newvalue)
{
	std::string time;
	if (newvalue.asBoolean())
	{
		gSavedSettings.setBool("UseLocalEnvironment", false);
		time = "region";
		LLEnvironment::setRegion();
	}
	else
	{
		time = "local";
	}
	if (gAutomationp)
	{
		gAutomationp->onWindlightChange(time, "", "");
	}
	return true;
}

static bool handlePrivateLookAtChanged(const LLSD& newvalue)
{
	LLHUDEffectLookAt::updateSettings();
	return true;
}

////////////////////////////////////////////////////////////////////////////

void settings_setup_listeners()
{
	// User interface related settings
	gSavedSettings.getControl("ButtonFlashCount")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("ButtonFlashRate")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("ChatConsoleMaxLines")->getSignal()->connect(boost::bind(&handleChatConsoleMaxLinesChanged, _2));
	gSavedSettings.getControl("ChatFontSize")->getSignal()->connect(boost::bind(&handleChatFontSizeChanged, _2));
	gSavedSettings.getControl("ChatPersistTime")->getSignal()->connect(boost::bind(&handleChatPersistTimeChanged, _2));
	gSavedSettings.getControl("ColumnHeaderDropDownDelay")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("ConsoleBackgroundOpacity")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("ConsoleBoxPerMessage")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("DebugConsoleMaxLines")->getSignal()->connect(boost::bind(&handleDebugConsoleMaxLinesChanged, _2));
	gSavedSettings.getControl("DebugObjectId")->getSignal()->connect(boost::bind(&handleDebugObjectIdChanged, _2));
	gSavedSettings.getControl("DebugViews")->getSignal()->connect(boost::bind(&handleDebugViewsChanged, _2));
	gSavedSettings.getControl("DisableMessagesSpacing")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("DisplayNamesUsage")->getSignal()->connect(boost::bind(&handleDisplayNamesUsageChanged, _2));
	gSavedSettings.getControl("DisplayGamma")->getSignal()->connect(boost::bind(&handleGammaChanged, _2));
	gSavedSettings.getControl("DropShadowButton")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("DropShadowFloater")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("DropShadowTooltip")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("HTMLLinkColor")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("Language")->getSignal()->connect(boost::bind(&handleLanguageChanged, _2));
	gSavedSettings.getControl("LanguageIsPublic")->getSignal()->connect(boost::bind(&handleLanguageChanged, _2));
	gSavedSettings.getControl("LegacyNamesForFriends")->getSignal()->connect(boost::bind(&handleLegacyNamesForFriendsChanged, _2));
	gSavedSettings.getControl("LegacyNamesForSpeakers")->getSignal()->connect(boost::bind(&handleLegacyNamesForSpeakersChanged, _2));
	gSavedSettings.getControl("MenuAccessKeyTime")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("MiniMapCenter")->getSignal()->connect(boost::bind(&handleMiniMapCenterChanged, _2));
	gSavedSettings.getControl("MiniMapRotate")->getSignal()->connect(boost::bind(&handleMiniMapRotateChanged, _2));
	gSavedSettings.getControl("OmitResidentAsLastName")->getSignal()->connect(boost::bind(&handleOmitResidentAsLastNameChanged, _2));
	gSavedSettings.getControl("PieMenuLineWidth")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("ShowBuildButton")->getSignal()->connect(boost::bind(&handleToolbarButtonsChanged, _2));
	gSavedSettings.getControl("ShowChatButton")->getSignal()->connect(boost::bind(&handleToolbarButtonsChanged, _2));
	gSavedSettings.getControl("ShowFlyButton")->getSignal()->connect(boost::bind(&handleToolbarButtonsChanged, _2));
	gSavedSettings.getControl("ShowFriendsButton")->getSignal()->connect(boost::bind(&handleToolbarButtonsChanged, _2));
	gSavedSettings.getControl("ShowGroupsButton")->getSignal()->connect(boost::bind(&handleToolbarButtonsChanged, _2));
	gSavedSettings.getControl("ShowIMButton")->getSignal()->connect(boost::bind(&handleToolbarButtonsChanged, _2));
	gSavedSettings.getControl("ShowInventoryButton")->getSignal()->connect(boost::bind(&handleToolbarButtonsChanged, _2));
	gSavedSettings.getControl("ShowMapButton")->getSignal()->connect(boost::bind(&handleToolbarButtonsChanged, _2));
	gSavedSettings.getControl("ShowMiniMapButton")->getSignal()->connect(boost::bind(&handleToolbarButtonsChanged, _2));
	gSavedSettings.getControl("ShowPropLinesAtWaterSurface")->getSignal()->connect(boost::bind(&handleShowPropLinesAtWaterSurfaceChanged, _2));
	gSavedSettings.getControl("ShowRadarButton")->getSignal()->connect(boost::bind(&handleToolbarButtonsChanged, _2));
	gSavedSettings.getControl("ShowSearchButton")->getSignal()->connect(boost::bind(&handleToolbarButtonsChanged, _2));
	gSavedSettings.getControl("ShowSnapshotButton")->getSignal()->connect(boost::bind(&handleToolbarButtonsChanged, _2));
	gSavedSettings.getControl("SnapMargin")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("TabToTextFieldsOnly")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("TypeAheadTimeout")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("UseAltKeyForMenus")->getSignal()->connect(boost::bind(&handleUISettingsChanged, _2));
	gSavedSettings.getControl("StackMinimizedTopToBottom")->getSignal()->connect(boost::bind(&handleStackMinimizedTopToBottom, _2));
	gSavedSettings.getControl("StackMinimizedRightToLeft")->getSignal()->connect(boost::bind(&handleStackMinimizedRightToLeft, _2));
	gSavedSettings.getControl("StackScreenWidthFraction")->getSignal()->connect(boost::bind(&handleStackScreenWidthFraction, _2));
	gSavedSettings.getControl("SwapShoutWhisperShortcuts")->getSignal()->connect(boost::bind(&handleSwapShoutWhisperShortcutsChanged, _2));
	gSavedSettings.getControl("SystemLanguage")->getSignal()->connect(boost::bind(&handleLanguageChanged, _2));
	gSavedSettings.getControl("UseOldStatusBarIcons")->getSignal()->connect(boost::bind(&handleUseOldStatusBarIconsChanged, _2));

	// Joystick related settings
	gSavedSettings.getControl("JoystickAxis0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("JoystickAxis1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("JoystickAxis2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("JoystickAxis3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("JoystickAxis4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("JoystickAxis5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("JoystickAxis6")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale6")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone6")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisScale0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisScale1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisScale2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisScale3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisScale4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisScale5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisDeadZone0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisDeadZone1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisDeadZone2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisDeadZone3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisDeadZone4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisDeadZone5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisScale0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisScale1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisScale2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisScale3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisScale4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisScale5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisDeadZone0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisDeadZone1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisDeadZone2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisDeadZone3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisDeadZone4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisDeadZone5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("NumpadControl")->getSignal()->connect(boost::bind(&handleNumpadControlChanged, _2));

	// Avatar related settings
	gSavedSettings.getControl("AvatarOffsetZ")->getSignal()->connect(boost::bind(&handleAvatarOffsetChanged, _2));
	gSavedSettings.getControl("AvatarPhysics")->getSignal()->connect(boost::bind(&handleAvatarPhysicsChanged, _2));
	gSavedSettings.getControl("OSAllowBakeOnMeshUploads")->getSignal()->connect(boost::bind(&handleBakeOnMeshUploadsChanged, _2));

	// Camera related settings
	gSavedSettings.getControl("CameraIgnoreCollisions")->getSignal()->connect(boost::bind(&handleCameraCollisionsChanged, _2));
	gSavedSettings.getControl("CameraFrontView")->getSignal()->connect(boost::bind(&handleCameraChanged, _2));
	gSavedSettings.getControl("CameraOffsetDefault")->getSignal()->connect(boost::bind(&handleCameraChanged, _2));
	gSavedSettings.getControl("FirstPersonAvatarVisible")->getSignal()->connect(boost::bind(&handleAvatarDebugSettingsChanged, _2));
	gSavedSettings.getControl("FocusOffsetDefault")->getSignal()->connect(boost::bind(&handleCameraChanged, _2));
	gSavedSettings.getControl("FocusOffsetFrontView")->getSignal()->connect(boost::bind(&handleCameraChanged, _2));
	gSavedSettings.getControl("CameraOffsetFrontView")->getSignal()->connect(boost::bind(&handleCameraChanged, _2));
	gSavedSettings.getControl("TrackFocusObject")->getSignal()->connect(boost::bind(&handleTrackFocusObjectChanged, _2));

	// Rendering related settings
	gSavedSettings.getControl("DebugShowRenderInfo")->getSignal()->connect(boost::bind(&handleDebugShowRenderInfoChanged, _2));
	gSavedSettings.getControl("EffectColor")->getSignal()->connect(boost::bind(handleEffectColorChanged, _2));
	gSavedSettings.getControl("OctreeStaticObjectSizeFactor")->getSignal()->connect(boost::bind(&handleRepartition, _2));
	gSavedSettings.getControl("OctreeDistanceFactor")->getSignal()->connect(boost::bind(&handleRepartition, _2));
	gSavedSettings.getControl("OctreeMaxNodeCapacity")->getSignal()->connect(boost::bind(&handleRepartition, _2));
	gSavedSettings.getControl("OctreeMinimumNodeSize")->getSignal()->connect(boost::bind(&handleRepartition, _2));
	gSavedSettings.getControl("OctreeAlphaDistanceFactor")->getSignal()->connect(boost::bind(&handleRepartition, _2));
	gSavedSettings.getControl("OctreeAttachmentSizeFactor")->getSignal()->connect(boost::bind(&handleRepartition, _2));
	gSavedSettings.getControl("RenderAnimateTrees")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderAnisotropic")->getSignal()->connect(boost::bind(&handleAnisotropicChanged, _2));
	gSavedSettings.getControl("RenderAutoMaskAlphaDeferred")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderAutoMaskAlphaNonDeferred")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderAvatarCloth")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderAvatarLODFactor")->getSignal()->connect(boost::bind(&handleAvatarDebugSettingsChanged, _2));
	gSavedSettings.getControl("RenderAvatarMaxNonImpostors")->getSignal()->connect(boost::bind(&handleAvatarDebugSettingsChanged, _2));
	gSavedSettings.getControl("RenderAvatarMaxPuppets")->getSignal()->connect(boost::bind(&handleAvatarDebugSettingsChanged, _2));
	gSavedSettings.getControl("RenderAvatarPhysicsLODFactor")->getSignal()->connect(boost::bind(&handleAvatarDebugSettingsChanged, _2));
	gSavedSettings.getControl("RenderAvatarVP")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderBakeSunlight")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderBatchedGlyphs")->getSignal()->connect(boost::bind(&handleRenderBatchedGlyphsChanged, _2));
	gSavedSettings.getControl("RenderClearARBBuffer")->getSignal()->connect(boost::bind(&handleRenderClearARBBufferChanged, _2));
	gSavedSettings.getControl("RenderCompressTextures")->getSignal()->connect(boost::bind(&handleRenderCompressTexturesChanged, _2));
	gSavedSettings.getControl("RenderCompressThreshold")->getSignal()->connect(boost::bind(&handleRenderCompressTexturesChanged, _2));
	gSavedSettings.getControl("RenderDebugGL")->getSignal()->connect(boost::bind(&handleRenderDebugGLChanged, _2));
	gSavedSettings.getControl("RenderDebugTextureBind")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderDeferred")->getSignal()->connect(boost::bind(&handleRenderDeferredChanged, _2));
	gSavedSettings.getControl("RenderDeferredNoise")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _2));
	gSavedSettings.getControl("RenderDeferredSSAO")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderDepthOfField")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _2));
	gSavedSettings.getControl("RenderDynamicLOD")->getSignal()->connect(boost::bind(&handleRenderDynamicLODChanged, _2));
	gSavedSettings.getControl("RenderFarClip")->getSignal()->connect(boost::bind(&handleRenderFarClipChanged, _2));
	gSavedSettings.getControl("RenderFlexTimeFactor")->getSignal()->connect(boost::bind(&handleFlexLODChanged, _2));
	gSavedSettings.getControl("RenderFogRatio")->getSignal()->connect(boost::bind(&handleFogRatioChanged, _2));
	gSavedSettings.getControl("RenderFSAASamples")->getSignal()->connect(boost::bind(&handleRenderFSAASamplesChanged, _2));
	gSavedSettings.getControl("RenderDeferredDisplayGamma")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderGlow")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _2));
	gSavedSettings.getControl("RenderGlowResolutionPow")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _2));
	gSavedSettings.getControl("RenderHideGroupTitle")->getSignal()->connect(boost::bind(handleHideGroupTitleChanged, _2));
	gSavedSettings.getControl("RenderHideGroupTitleAll")->getSignal()->connect(boost::bind(&handleAvatarDebugSettingsChanged, _2));
	gSavedSettings.getControl("RenderLightingDetail")->getSignal()->connect(boost::bind(&handleRenderLightingDetailChanged, _2));
	gSavedSettings.getControl("RenderMaxPartCount")->getSignal()->connect(boost::bind(&handleMaxPartCountChanged, _2));
	gSavedSettings.getControl("RenderMaxTextureIndex")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderUseDepthClamp")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderMaxVBOSize")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderName")->getSignal()->connect(boost::bind(&handleAvatarDebugSettingsChanged, _2));
	gSavedSettings.getControl("RenderNoAlpha")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderObjectBump")->getSignal()->connect(boost::bind(&handleRenderObjectBumpChanged, _2));
	gSavedSettings.getControl("RenderOptimizeMeshVertexCache")->getSignal()->connect(boost::bind(&handleRenderOptimizeMeshVertexCacheChanged, _2));
	gSavedSettings.getControl("RenderPreferStreamDraw")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderResolutionDivisor")->getSignal()->connect(boost::bind(&handleRenderResolutionDivisorChanged, _2));
	gSavedSettings.getControl("RenderShadowDetail")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderShadowResolutionScale")->getSignal()->connect(boost::bind(&handleShadowsResized, _2));
	gSavedSettings.getControl("RenderSpecularExponent")->getSignal()->connect(boost::bind(&handleLUTBufferChanged, _2));
	gSavedSettings.getControl("RenderSpecularResX")->getSignal()->connect(boost::bind(&handleLUTBufferChanged, _2));
	gSavedSettings.getControl("RenderSpecularResY")->getSignal()->connect(boost::bind(&handleLUTBufferChanged, _2));
	gSavedSettings.getControl("RenderTerrainDetail")->getSignal()->connect(boost::bind(&handleTerrainDetailChanged, _2));
	gSavedSettings.getControl("RenderTerrainLODFactor")->getSignal()->connect(boost::bind(&handleTerrainLODChanged, _2));
	gSavedSettings.getControl("RenderTransparentWater")->getSignal()->connect(boost::bind(&handleRenderTransparentWaterChanged, _2));
	gSavedSettings.getControl("RenderTreeAnimationDamping")->getSignal()->connect(boost::bind(&handleTreeSettingsChanged, _2));
	gSavedSettings.getControl("RenderTreeTrunkStiffness")->getSignal()->connect(boost::bind(&handleTreeSettingsChanged, _2));
	gSavedSettings.getControl("RenderTreeWindSensitivity")->getSignal()->connect(boost::bind(&handleTreeSettingsChanged, _2));
	gSavedSettings.getControl("RenderTreeLODFactor")->getSignal()->connect(boost::bind(&handleTreeSettingsChanged, _2));
	gSavedSettings.getControl("RenderUseStreamVBO")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderUseVAO")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderVBOEnable")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderVBOMappingDisable")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderVolumeLODFactor")->getSignal()->connect(boost::bind(&handleVolumeSettingsChanged, _2));
	gSavedSettings.getControl("SkyUseClassicClouds")->getSignal()->connect(boost::bind(&handleSkyUseClassicCloudsChanged, _2));
	gSavedSettings.getControl("UseNewShaders")->getSignal()->connect(boost::bind(&handleUseNewShadersChanged, _2));
	gSavedSettings.getControl("UseOcclusion")->getSignal()->connect(boost::bind(&handleUseOcclusionChanged, _2));
	gSavedSettings.getControl("WindLightUseAtmosShaders")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("WLSkyDetail")->getSignal()->connect(boost::bind(&handleWLSkyDetailChanged, _2));

	// Network related settings
	gSavedSettings.getControl("EnableHTTP2")->getSignal()->connect(boost::bind(&handleEnableHTTP2Changed, _2));
	gSavedSettings.getControl("InterpolationTime")->getSignal()->connect(boost::bind(&handleInterpolationTimesChanged, _2));
	gSavedSettings.getControl("InterpolationPhaseOut")->getSignal()->connect(boost::bind(&handleInterpolationTimesChanged, _2));
	gSavedSettings.getControl("RegionCrossingInterpolationTime")->getSignal()->connect(boost::bind(&handleInterpolationTimesChanged, _2));
	gSavedSettings.getControl("MeshMaxConcurrentRequests")->getSignal()->connect(boost::bind(&handleMeshMaxConcurrentRequestsChanged, _2));
	gSavedSettings.getControl("NoVerifySSLCert")->getSignal()->connect(boost::bind(&handleNoVerifySSLCertChanged, _2));
	gSavedSettings.getControl("PingInterpolate")->getSignal()->connect(boost::bind(&handlePingInterpolateChanged, _2));
	gSavedSettings.getControl("SearchURL")->getSignal()->connect(boost::bind(&handleSearchURLChanged, _2));
	gSavedSettings.getControl("ThrottleBandwidthKBPS")->getSignal()->connect(boost::bind(&handleBandwidthChanged, _2));
	gSavedSettings.getControl("VelocityInterpolate")->getSignal()->connect(boost::bind(&handleVelocityInterpolateChanged, _2));

	// Obects cache related settings
	gSavedSettings.getControl("BiasedObjectRetention")->getSignal()->connect(boost::bind(&handleVOCacheSettingChanged, _2));
	gSavedSettings.getControl("NonVisibleObjectsInMemoryTime")->getSignal()->connect(boost::bind(&handleVOCacheSettingChanged, _2));
	gSavedSettings.getControl("SceneLoadMinRadius")->getSignal()->connect(boost::bind(&handleVOCacheSettingChanged, _2));
	gSavedSettings.getControl("SceneLoadFrontPixelThreshold")->getSignal()->connect(boost::bind(&handleVOCacheSettingChanged, _2));
	gSavedSettings.getControl("SceneLoadRearPixelThreshold")->getSignal()->connect(boost::bind(&handleVOCacheSettingChanged, _2));
	gSavedSettings.getControl("SceneLoadRearMaxRadiusFraction")->getSignal()->connect(boost::bind(&handleVOCacheSettingChanged, _2));
	gSavedSettings.getControl("Use360InterestList")->getSignal()->connect(boost::bind(&handleUse360InterestListSettingChanged, _2));

	// Audio and media related settings
	gSavedSettings.getControl("AudioLevelMaster")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelSFX")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelUI")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelAmbient")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelMic")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("AudioLevelMusic")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelMedia")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelVoice")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelDoppler")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelRolloff")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelUnderwaterRolloff")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelWind")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("DisableWindAudio")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("EnableStreamingMusic")->getSignal()->connect(boost::bind(&handleAudioStreamMusicChanged, _2));
	gSavedSettings.getControl("EnableStreamingMedia")->getSignal()->connect(boost::bind(&handlePrimMediaChanged, _2));
	gSavedSettings.getControl("PrimMediaMasterEnabled")->getSignal()->connect(boost::bind(&handlePrimMediaChanged, _2));
	gSavedSettings.getControl("MuteAudio")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("MuteMusic")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("MuteMedia")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("MuteVoice")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("MuteAmbient")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("MuteUI")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));

	// Voice related settings
	gSavedSettings.getControl("EnableVoiceChat")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("LipSyncEnabled")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("PTTCurrentlyEnabled")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("PushToTalkButton")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("PushToTalkToggle")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("VoiceEarLocation")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("VoiceInputAudioDevice")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("VoiceOutputAudioDevice")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));

	// Memory related settings
	gSavedSettings.getControl("AllowSwapping")->getSignal()->connect(boost::bind(&handleAllowSwappingChanged, _2));
	gSavedSettings.getControl("MainMemorySafetyMargin")->getSignal()->connect(boost::bind(&handleMainMemoryMarginsChanged, _2));
	gSavedSettings.getControl("SafetyMargin1stStepRatio")->getSignal()->connect(boost::bind(&handleMainMemoryMarginsChanged, _2));
	gSavedSettings.getControl("TextureMemory")->getSignal()->connect(boost::bind(&handleVideoMemoryChanged, _2));
	gSavedSettings.getControl("MaxBoundTexMem")->getSignal()->connect(boost::bind(&handleVideoMemoryChanged, _2));

	// Spell checking related settings
	gSavedSettings.getControl("SpellCheck")->getSignal()->connect(boost::bind(&handleSpellCheckChanged, _2));
	gSavedSettings.getControl("SpellCheckShow")->getSignal()->connect(boost::bind(&handleSpellCheckChanged, _2));
	gSavedSettings.getControl("SpellCheckLanguage")->getSignal()->connect(boost::bind(&handleSpellCheckChanged, _2));

	// Multitasking/threading related settings
	gSavedSettings.getControl("MainloopTimeoutDefault")->getSignal()->connect(boost::bind(&handleMainloopTimeoutDefaultChanged, _2));
	gSavedSettings.getControl("ThreadedFilesystem")->getSignal()->connect(boost::bind(&handleThreadedFilesystemChanged, _2));
	gSavedSettings.getControl("UseIOUring")->getSignal()->connect(boost::bind(&handleUseIOUringChanged, _2));

	// Environment related settings
	gSavedSettings.getControl("LightshareEnabled")->getSignal()->connect(boost::bind(&handleLightshareEnabledChanged, _2));
	gSavedSettings.getControl("UseLocalEnvironment")->getSignal()->connect(boost::bind(&handleUseLocalEnvironmentChanged, _2));
	gSavedSettings.getControl("UseParcelEnvironment")->getSignal()->connect(boost::bind(&handleUseParcelEnvironmentChanged, _2));
	gSavedSettings.getControl("UseWLEstateTime")->getSignal()->connect(boost::bind(&handleUseWLEstateTimeChanged, _2));

	// Privacy related settings
	gSavedSettings.getControl("PrivateLookAt")->getSignal()->connect(boost::bind(&handlePrivateLookAtChanged, _2));
	gSavedSettings.getControl("PrivateLookAtLimit")->getSignal()->connect(boost::bind(&handlePrivateLookAtChanged, _2));

	// Miscellaneous settings
	gSavedSettings.getControl("DebugPermissions")->getSignal()->connect(boost::bind(&handleDebugPermissionsChanged, _2));
#if LL_FAST_TIMERS_ENABLED
	gSavedSettings.getControl("FastTimersAlwaysEnabled")->getSignal()->connect(boost::bind(&handleFastTimersAlwaysEnabledChanged, _2));
#endif
	gSavedSettings.getControl("FSFlushOnWrite")->getSignal()->connect(boost::bind(&handleFSFlushOnWriteChanged, _2));
	gSavedSettings.getControl("UserLogFile")->getSignal()->connect(boost::bind(&handleLogFileChanged, _2));
	gSavedSettings.getControl("TextureFetchBoostWithFetches")->getSignal()->connect(boost::bind(&handleTextureFetchBoostWithFetchesChanged, _2));
	gSavedSettings.getControl("TextureFetchBoostWithSpeed")->getSignal()->connect(boost::bind(&handleTextureFetchBoostWithSpeedChanged, _2));
//MK
	gSavedSettings.getControl("RestrainedLoveRelaxedTempAttach")->getSignal()->connect(boost::bind(&handleRestrainedLoveRelaxedTempAttachChanged, _2));
	gSavedSettings.getControl("RestrainedLoveAutomaticRenameItems")->getSignal()->connect(boost::bind(&handleRestrainedLoveAutomaticRenameItemsChanged, _2));
//mk
}

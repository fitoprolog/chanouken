/**
 * @file llviewertexture.cpp
 * @brief Object which handles a received image (and associated texture(s))
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#include "llviewertexture.h"

#include "imageids.h"
#include "llgl.h"
#include "llfasttimer.h"
#include "llhost.h"
#include "llimage.h"
#include "llimagegl.h"
#include "llimagebmp.h"
#include "llimagej2c.h"
#include "llimagetga.h"
#include "llmediaentry.h"
#include "llnotifications.h"
#include "llsys.h"
#include "lltextureentry.h"
#include "lltexturemanagerbridge.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldrawpool.h"
#include "llpipeline.h"
#include "llstatusbar.h"
#include "lltexturecache.h"
#include "lltexturefetch.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"			// For gResetVertexBuffers
#include "llviewermedia.h"
#include "llviewertexturelist.h"
#include "llvovolume.h"

// statics
LLPointer<LLViewerTexture> LLViewerTexture::sNullImagep = NULL;
LLPointer<LLViewerTexture> LLViewerTexture::sCloudImagep = NULL;
LLPointer<LLViewerFetchedTexture> LLViewerFetchedTexture::sWhiteImagep = NULL;
LLPointer<LLViewerFetchedTexture> LLViewerFetchedTexture::sDefaultImagep = NULL;
LLPointer<LLViewerFetchedTexture> LLViewerFetchedTexture::sSmokeImagep = NULL;
LLPointer<LLViewerFetchedTexture> LLViewerFetchedTexture::sFlatNormalImagep = NULL;
LLPointer<LLViewerFetchedTexture> LLViewerFetchedTexture::sDefaultSunImagep = NULL;
LLPointer<LLViewerFetchedTexture> LLViewerFetchedTexture::sDefaultMoonImagep = NULL;
LLPointer<LLViewerFetchedTexture> LLViewerFetchedTexture::sDefaultCloudsImagep = NULL;
LLPointer<LLViewerFetchedTexture> LLViewerFetchedTexture::sDefaultCloudNoiseImagep = NULL;
LLPointer<LLViewerFetchedTexture> LLViewerFetchedTexture::sBloomImagep = NULL;
LLPointer<LLViewerFetchedTexture> LLViewerFetchedTexture::sOpaqueWaterImagep = NULL;
LLPointer<LLViewerFetchedTexture> LLViewerFetchedTexture::sWaterImagep = NULL;
LLPointer<LLViewerFetchedTexture> LLViewerFetchedTexture::sWaterNormapMapImagep = NULL;
LLViewerMediaTexture::media_map_t LLViewerMediaTexture::sMediaMap;

S32 LLViewerTexture::sImageCount = 0;
S32 LLViewerTexture::sRawCount = 0;
S32 LLViewerTexture::sAuxCount = 0;
LLFrameTimer LLViewerTexture::sEvaluationTimer;
F32 LLViewerTexture::sDesiredDiscardBias = 0.f;
F32 LLViewerTexture::sDesiredDiscardScale = 1.1f;
S64 LLViewerTexture::sBoundTextureMemoryInBytes = 0;
S64 LLViewerTexture::sTotalTextureMemoryInBytes = 0;
S64 LLViewerTexture::sMaxBoundTextureMemInMegaBytes = 0;
S64 LLViewerTexture::sMaxTotalTextureMemInMegaBytes = 0;
S8  LLViewerTexture::sCameraMovingDiscardBias = 0;
F32 LLViewerTexture::sCameraMovingBias = 0.f;
constexpr S32 MAX_CACHED_RAW_IMAGE_AREA = 64 * 64;
constexpr S32 MAX_CACHED_RAW_SCULPT_IMAGE_AREA = MAX_SCULPT_REZ *
												 MAX_SCULPT_REZ;
constexpr S32 MAX_CACHED_RAW_TERRAIN_IMAGE_AREA = 128 * 128;
S32 LLViewerTexture::sMinLargeImageSize = 65536; //256 * 256.
S32 LLViewerTexture::sMaxSmallImageSize = MAX_CACHED_RAW_IMAGE_AREA;
F32 LLViewerTexture::sCurrentTime = 0.f;
bool LLViewerTexture::sDontLoadVolumeTextures = false;

// -max number of levels to improve image quality by:
constexpr F32 desired_discard_bias_min = -2.f;
// max number of levels to reduce image quality by:
constexpr F32 desired_discard_bias_max = (F32)MAX_DISCARD_LEVEL;

//-----------------------------------------------------------------------------
//namespace: LLViewerTextureAccess
//-----------------------------------------------------------------------------

LLLoadedCallbackEntry::LLLoadedCallbackEntry(loaded_callback_func cb,
											 S32 discard_level,
											 bool need_imageraw,
											 void* userdata,
											 uuid_list_t* cb_list,
											 LLViewerFetchedTexture* target,
											 bool pause)
:	mCallback(cb),
	mLastUsedDiscard(MAX_DISCARD_LEVEL + 1),
	mDesiredDiscard(discard_level),
	mNeedsImageRaw(need_imageraw),
	mUserData(userdata),
	mSourceCallbackList(cb_list),
	mPaused(pause)
{
	if (mSourceCallbackList)
	{
		mSourceCallbackList->emplace(target->getID());
	}
}

void LLLoadedCallbackEntry::removeTexture(LLViewerFetchedTexture* tex)
{
	if (mSourceCallbackList)
	{
		mSourceCallbackList->erase(tex->getID());
	}
}

//static
void LLLoadedCallbackEntry::cleanUpCallbackList(uuid_list_t* cb_list)
{
	// Clear texture callbacks.
	if (cb_list && !cb_list->empty())
	{
		for (uuid_list_t::iterator it = cb_list->begin(), end = cb_list->end();
			 it != end; ++it)
		{
			LLViewerFetchedTexture* tex = gTextureList.findImage(*it);
			if (tex)
			{
				tex->deleteCallbackEntry(cb_list);
			}
		}
		cb_list->clear();
	}
}

LLViewerMediaTexture* LLViewerTextureManager::createMediaTexture(const LLUUID& media_id,
																 bool usemipmaps,
																 LLImageGL* gl_image)
{
	return new LLViewerMediaTexture(media_id, usemipmaps, gl_image);
}

LLViewerTexture*  LLViewerTextureManager::findTexture(const LLUUID& id)
{
	LLViewerTexture* tex;
	//search fetched texture list
	tex = gTextureList.findImage(id);

	// search media texture list
	if (!tex)
	{
		tex = LLViewerTextureManager::findMediaTexture(id);
	}
	return tex;
}

LLViewerMediaTexture* LLViewerTextureManager::findMediaTexture(const LLUUID& media_id)
{
	return LLViewerMediaTexture::findMediaTexture(media_id);
}

LLViewerMediaTexture* LLViewerTextureManager::getMediaTexture(const LLUUID& id,
															  bool usemipmaps,
															  LLImageGL* gl_image)
{
	LLViewerMediaTexture* tex = LLViewerMediaTexture::findMediaTexture(id);
	if (!tex)
	{
		tex = LLViewerTextureManager::createMediaTexture(id, usemipmaps, gl_image);
	}

	tex->initVirtualSize();

	return tex;
}

LLViewerFetchedTexture* LLViewerTextureManager::staticCastToFetchedTexture(LLGLTexture* tex,
																		   bool report_error)
{
	if (!tex)
	{
		return NULL;
	}

	S8 type = tex->getType();
	if (type == LLViewerTexture::FETCHED_TEXTURE ||
		type == LLViewerTexture::LOD_TEXTURE)
	{
		return (LLViewerFetchedTexture*)tex;
	}

	if (report_error)
	{
		llerrs << "Not a fetched texture type: " << type << llendl;
	}

	return NULL;
}

LLPointer<LLViewerTexture> LLViewerTextureManager::getLocalTexture(bool usemipmaps,
																   bool generate_gl_tex)
{
	LLPointer<LLViewerTexture> tex = new LLViewerTexture(usemipmaps);
	if (generate_gl_tex)
	{
		tex->generateGLTexture();
		tex->setCategory(LLGLTexture::LOCAL);
	}
	return tex;
}

LLPointer<LLViewerTexture> LLViewerTextureManager::getLocalTexture(const LLUUID& id,
																   bool usemipmaps,
																   bool generate_gl_tex)
{
	LLPointer<LLViewerTexture> tex = new LLViewerTexture(id, usemipmaps);
	if (generate_gl_tex)
	{
		tex->generateGLTexture();
		tex->setCategory(LLGLTexture::LOCAL);
	}
	return tex;
}

LLPointer<LLViewerTexture> LLViewerTextureManager::getLocalTexture(const LLImageRaw* raw,
																   bool usemipmaps)
{
	LLPointer<LLViewerTexture> tex = new LLViewerTexture(raw, usemipmaps);
	tex->setCategory(LLGLTexture::LOCAL);
	return tex;
}

LLPointer<LLViewerTexture> LLViewerTextureManager::getLocalTexture(U32 width,
																   U32 height,
																   U8 components,
																   bool usemipmaps,
																   bool generate_gl_tex)
{
	LLPointer<LLViewerTexture> tex = new LLViewerTexture(width, height,
														 components,
														 usemipmaps);
	if (generate_gl_tex)
	{
		tex->generateGLTexture();
		tex->setCategory(LLGLTexture::LOCAL);
	}
	return tex;
}

LLViewerFetchedTexture* LLViewerTextureManager::getFetchedTexture(const LLUUID& image_id,
																  FTType f_type,
																  bool usemipmaps,
																  LLGLTexture::EBoostLevel boost_priority,
																  S8 texture_type,
																  S32 internal_format,
																  U32 primary_format,
																  LLHost request_from_host)
{
	return gTextureList.getImage(image_id, f_type, usemipmaps, boost_priority,
								 texture_type, internal_format, primary_format,
								 request_from_host);
}

LLViewerFetchedTexture* LLViewerTextureManager::getFetchedTextureFromFile(const std::string& filename,
																		  bool usemipmaps,
																		  LLGLTexture::EBoostLevel boost_priority,
																		  S8 texture_type,
																		  S32 internal_format,
																		  U32 primary_format,
																		  const LLUUID& force_id)
{
	return gTextureList.getImageFromFile(filename, usemipmaps,
										 boost_priority, texture_type,
										 internal_format, primary_format,
										 force_id);
}

//static
LLViewerFetchedTexture* LLViewerTextureManager::getFetchedTextureFromUrl(const std::string& url,
																		 FTType f_type,
																		 bool usemipmaps,
																		 LLGLTexture::EBoostLevel boost_priority,
																		 S8 texture_type,
																		 S32 internal_format,
																		 U32 primary_format,
																		 const LLUUID& force_id)
{
	return gTextureList.getImageFromUrl(url, FTT_LOCAL_FILE, usemipmaps,
										boost_priority, texture_type,
										internal_format, primary_format,
										force_id);
}

LLViewerFetchedTexture* LLViewerTextureManager::getFetchedTextureFromHost(const LLUUID& image_id,
																		  FTType f_type,
																		  LLHost host)
{
	return gTextureList.getImageFromHost(image_id, f_type, host);
}

// Create a bridge to the viewer texture manager.
class LLViewerTextureManagerBridge : public LLTextureManagerBridge
{
	LLPointer<LLGLTexture> getLocalTexture(bool usemipmaps = true,
										   bool generate = true) override
	{
		return LLViewerTextureManager::getLocalTexture(usemipmaps, generate);
	}

	LLPointer<LLGLTexture> getLocalTexture(U32 width, U32 height,
										   U8 components, bool usemipmaps,
										   bool generate = true) override
	{
		return LLViewerTextureManager::getLocalTexture(width, height,
													   components, usemipmaps,
													   generate);
	}

	LLGLTexture* getFetchedTexture(const LLUUID& image_id) override
	{
		return LLViewerTextureManager::getFetchedTexture(image_id);
	}
};

void LLViewerTextureManager::init()
{
	LLPointer<LLImageRaw> raw = new LLImageRaw(1, 1, 3);
	raw->clear(0x77, 0x77, 0x77, 0xFF);
	LLViewerTexture::sNullImagep =
		LLViewerTextureManager::getLocalTexture(raw.get(), true);

	LLViewerTexture::sCloudImagep =
		LLViewerTextureManager::getFetchedTextureFromFile("cloud-particle.j2c");

#if 1
	LLPointer<LLViewerFetchedTexture> imagep =
		LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT);
	LLViewerFetchedTexture::sDefaultImagep = imagep;

	constexpr S32 dim = 128;
	LLPointer<LLImageRaw> image_raw = new LLImageRaw(dim, dim, 3);
	U8* data = image_raw->getData();
	if (!data)
	{
		return;
	}

	memset(data, 0, dim * dim * 3);

	for (S32 i = 0; i < dim; ++i)
	{
		for (S32 j = 0; j < dim; ++j)
		{
# if 0
			constexpr S32 border = 2;
			if (i < border || j < border || i >= dim - border ||
				j >= dim - border)
			{
				*data++ = 0xff;
				*data++ = 0xff;
				*data++ = 0xff;
			}
			else
# endif
			{
				*data++ = 0x7f;
				*data++ = 0x7f;
				*data++ = 0x7f;
			}
		}
	}
	imagep->createGLTexture(0, image_raw);
	// Cache the raw image
	imagep->setCachedRawImage(0, image_raw);
	image_raw = NULL;
#else
 	LLViewerFetchedTexture::sDefaultImagep =
		LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT, FTT_DEFAULT,
												  true, LLGLTexture::BOOST_UI);
#endif
	if (LLViewerFetchedTexture::sDefaultImagep)
	{
		LLViewerFetchedTexture::sDefaultImagep->setNoDelete();
		LLViewerFetchedTexture::sDefaultImagep->dontDiscard();
		LLViewerFetchedTexture::sDefaultImagep->setCategory(LLGLTexture::OTHER);
	}

 	LLViewerFetchedTexture::sSmokeImagep =
		LLViewerTextureManager::getFetchedTexture(IMG_SMOKE, FTT_DEFAULT,
												  true, LLGLTexture::BOOST_UI);
	if (LLViewerFetchedTexture::sSmokeImagep)
	{
		LLViewerFetchedTexture::sSmokeImagep->setNoDelete();
		LLViewerFetchedTexture::sSmokeImagep->dontDiscard();
	}

	LLViewerTexture::initClass();

	// Create a texture manager bridge.
	gTextureManagerBridgep = new LLViewerTextureManagerBridge;
}

void LLViewerTextureManager::cleanup()
{
	delete gTextureManagerBridgep;

	LLImageGL::sDefaultGLImagep = NULL;
	LLViewerTexture::sNullImagep = NULL;
	LLViewerTexture::sCloudImagep = NULL;
	LLViewerFetchedTexture::sDefaultImagep = NULL;
	LLViewerFetchedTexture::sSmokeImagep = NULL;
	LLViewerFetchedTexture::sWhiteImagep = NULL;
	LLViewerFetchedTexture::sFlatNormalImagep = NULL;
	LLViewerFetchedTexture::sDefaultSunImagep = NULL;
	LLViewerFetchedTexture::sDefaultMoonImagep = NULL;
	LLViewerFetchedTexture::sDefaultCloudsImagep = NULL;
	LLViewerFetchedTexture::sDefaultCloudNoiseImagep = NULL;
	LLViewerFetchedTexture::sBloomImagep = NULL;
	LLViewerFetchedTexture::sOpaqueWaterImagep = NULL;
	LLViewerFetchedTexture::sWaterImagep = NULL;
	LLViewerFetchedTexture::sWaterNormapMapImagep = NULL;

	LLViewerMediaTexture::cleanUpClass();
}

//-----------------------------------------------------------------------------
// LLViewerTexture
//-----------------------------------------------------------------------------

// static
void LLViewerTexture::initClass()
{
	LLImageGL::sDefaultGLImagep =
		LLViewerFetchedTexture::sDefaultImagep->getGLImage();
}

// Tuning params
constexpr F32 discard_bias_delta = .25f;	// Was .05f in v1
constexpr F32 discard_delta_time = 0.5f;
constexpr F32 texmem_lower_bound_scale = 0.85f;
constexpr F32 texmem_middle_bound_scale = 0.925f;

//static
bool LLViewerTexture::isMemoryForTextureLow(F32& discard)
{
	LL_FAST_TIMER(FTM_TEXTURE_MEMORY_CHECK);

	constexpr S32 MIN_FREE_TEXTURE_MEMORY = 5 * 1024;	// KB
	static F32 last_memory_check = 0.f;
	static F32 last_draw_distance_reduce = 0.f;
	static bool last_check_was_low = false;
	static bool warned = false;

	bool low_mem = false;
	U32 avail_virtual_mem_kb = LLMemory::getAvailableVirtualMemKB();
	U32 safety_margin_kb = LLMemory::getSafetyMarginKB();
	U32 threshold_margin_kb = LLMemory::getThresholdMarginKB();
	if (gMemoryCheckTimer.getElapsedTimeF32() -
			last_memory_check > 4.f * discard_delta_time)
	{
		last_memory_check = gMemoryCheckTimer.getElapsedTimeF32();
		if (avail_virtual_mem_kb < safety_margin_kb)
		{
			static LLCachedControl<bool> notify(gSavedSettings,
												"MemoryAlertsAsNotifications");
			// Avoid crashes by reducing the draw distance.
			if (last_memory_check -
					last_draw_distance_reduce > 20.f * discard_delta_time)
			{
				F32 draw_distance = gSavedSettings.getF32("RenderFarClip");
				if (draw_distance > 64.f &&
					avail_virtual_mem_kb > safety_margin_kb / 2)
				{
					draw_distance = llmax(draw_distance * 0.5f, 64.f);
					gSavedSettings.setF32("RenderFarClip", draw_distance);
					last_draw_distance_reduce = last_memory_check;
					gNotifications.add(notify ? "NotifyReducedDrawDistance"
											  :"ReducedDrawDistance");
				}
				else if (!warned)
				{
					// This is hopeless... Try reseting the camera view however
					// since caming a lot or far away causes high memory
					// consumption
					if (draw_distance > 64.f)
					{
						gSavedSettings.setF32("RenderFarClip", 64.f);
					}
					else if (draw_distance > 32.f)
					{
						// Try even harder on the second pass...
						gSavedSettings.setF32("RenderFarClip", 32.f);

						static LLCachedControl<bool> reset_vb(gSavedSettings,
															  "ResetVertexBuffersOnLowMem");
						if (reset_vb)
						{
							gResetVertexBuffers = true;
						}
					}
					gAgent.resetView();
					gNotifications.add(notify ? "NotifyMightCrashSoon"
											  : "MightCrashSoon");
					// Do not spam: warn only once and wait till the memory
					// usage decreases before warning again should it grow back
					warned = true;
				}
			}
			gBalanceObjectCache = true;
			low_mem = true;
			LL_DEBUGS("TextureMemory") << "Reporting memory low (2nd step) for textures."
									   << LL_ENDL;
		}
		else if (avail_virtual_mem_kb < threshold_margin_kb)
		{
			low_mem = true;
			LL_DEBUGS("TextureMemory") << "Reporting memory low (1st step) for textures."
									   << LL_ENDL;
		}
		if (!low_mem)
		{
			warned = false;	// Get ready for next time...
		}
		last_check_was_low = low_mem;
	}
	else
	{
		low_mem = last_check_was_low;
	}

	F32 old_discard = discard;
	if (low_mem)
	{
		F32 ratio = (F32)(threshold_margin_kb - avail_virtual_mem_kb) /
					(F32)(threshold_margin_kb - safety_margin_kb);
		F32 quantizer = (F32)((S32)(1.f / discard_bias_delta));
		discard = (F32)((S32)(desired_discard_bias_max * ratio *
							  quantizer)) / quantizer;
		llclamp(discard, 0.f, desired_discard_bias_max);
	}

	if (discard <= old_discard && gGLManager.mHasATIMemInfo)
	{
		S32 meminfo[4];
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, meminfo);

		if (meminfo[0] < MIN_FREE_TEXTURE_MEMORY)
		{
			LL_DEBUGS("TextureMemory") << "Reporting low GPU memory for textures."
									   << LL_ENDL;
			if (discard <= old_discard)
			{
				discard = old_discard + discard_bias_delta;
				llclamp(discard, 0.f, desired_discard_bias_max);
				low_mem = true;
			}
		}
	}
#if 0  // Ignore NVIDIA cards
	else if (discard <= old_discard && gGLManager.mHasNVXMemInfo)
	{
		S32 free;
		glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &free);
		if (free < MIN_FREE_TEXTURE_MEMORY)
		{
			LL_DEBUGS("TextureMemory") << "Reporting low GPU memory for textures."
									   << LL_ENDL;
			if (discard <= old_discard)
			{
				discard = old_discard + discard_bias_delta;
				llclamp(discard, 0.f, desired_discard_bias_max);
				low_mem = true;
			}
		}
	}
#endif

	return low_mem;
}

//static
void LLViewerTexture::updateClass(F32 velocity, F32 angular_velocity)
{
	sCurrentTime = gFrameTimeSeconds;

	LLViewerMediaTexture::updateClass();

	// In bytes
	sBoundTextureMemoryInBytes = LLImageGL::sBoundTextureMemoryInBytes;
	sTotalTextureMemoryInBytes = LLImageGL::sGlobalTextureMemoryInBytes;
	// In MB
	sMaxBoundTextureMemInMegaBytes = gTextureList.getMaxResidentTexMem();
	sMaxTotalTextureMemInMegaBytes = gTextureList.getMaxTotalTextureMem();

	if (gUseWireframe)
	{
		// Max out the discard level, because the wireframe mode kills object
		// culling and causes all objects and textures in FOV to load at once,
		// which can exhaust the virtual address space pretty quickly...
		sDesiredDiscardBias = desired_discard_bias_max;
		return;
	}

	bool can_decrease_discard = true;
	bool increased_discard = false;
	bool is_check_time =
		sEvaluationTimer.getElapsedTimeF32() > discard_delta_time;

	// First check wether the system memory is low or not and adjust discard
	F32 discard = sDesiredDiscardBias;
	static LLCachedControl<bool> memory_safety_check(gSavedSettings,
													 "MemorySafetyChecks");
	if (LLMemory::hasFailedAllocation())
	{
		// There has been a failure to allocate memory: the latter is either
		// too low or too fragmented !  Let's take radical measures...
		sDesiredDiscardBias = 5.f;
		if (discard < 5.f)
		{
			increased_discard = true;
		}
		can_decrease_discard = false;
		static LLCachedControl<F32> draw_distance(gSavedSettings,
												  "RenderFarClip");
		if (draw_distance > 64.f)
		{
			gSavedSettings.setF32("RenderFarClip", 64.f);
		}
		static S32 last_failure = 0;
		if ((S32)sCurrentTime - last_failure > 3)
		{
			gAgent.resetView();
		}

		// Make the memory graph flash in the status bar
		if (gStatusBarp)
		{
			gStatusBarp->setMemAllocFailure();
		}
		// Clear the error condition.
		LLMemory::resetFailedAllocation();

		// Warn the user, but do not spam them either...
		if ((S32)sCurrentTime - last_failure > 20)
		{
			gNotifications.add("GotAllocationFailure");
		}
		last_failure = (S32)sCurrentTime;

		LL_DEBUGS("TextureMemory") << "Maxing discard bias due to memory allocation failure."
								   << LL_ENDL;
	}
	else if (memory_safety_check && is_check_time &&
			 isMemoryForTextureLow(discard))
	{
		if (discard >= sDesiredDiscardBias)
		{
			can_decrease_discard = false;
			if (discard > sDesiredDiscardBias)
			{
				// Increase discard as fast as the low memory condition
				// demands it
				sDesiredDiscardBias = discard;
				increased_discard = true;
				LL_DEBUGS("TextureMemory") << "Increasing discard bias due to low memory condition."
										   << LL_ENDL;
			}
		}
	}

	if (BYTES2MEGABYTES(sBoundTextureMemoryInBytes) >=
			sMaxBoundTextureMemInMegaBytes ||
		BYTES2MEGABYTES(sTotalTextureMemoryInBytes) >=
			sMaxTotalTextureMemInMegaBytes)
	{
		// If we are using more texture memory than we should, scale up the
		// desired discard level
		if (is_check_time && !increased_discard)
		{
			LL_DEBUGS("TextureMemory") << "Increasing discard bias due to excessive texture memory use."
									   << LL_ENDL;
			can_decrease_discard = false;
			sDesiredDiscardBias += discard_bias_delta;
		}
	}

	if (is_check_time && sDesiredDiscardBias > 0.f && can_decrease_discard &&
		BYTES2MEGABYTES(sBoundTextureMemoryInBytes) <
			sMaxBoundTextureMemInMegaBytes * texmem_lower_bound_scale &&
		BYTES2MEGABYTES(sTotalTextureMemoryInBytes) <
			sMaxTotalTextureMemInMegaBytes * texmem_lower_bound_scale)
	{
		// If we are using less texture memory than we should, scale down the
		// desired discard level
		LL_DEBUGS("TextureMemory") << "Decreasing discard bias." << LL_ENDL;
		sDesiredDiscardBias -= discard_bias_delta;
	}

	if (is_check_time)
	{
		sEvaluationTimer.reset();
	}
	sDesiredDiscardBias = llclamp(sDesiredDiscardBias,
								  desired_discard_bias_min,
								  desired_discard_bias_max);

	if (sDesiredDiscardBias >= 4.f)
	{
		// Ask for immediate deletion of all old images to free memory
		gTextureList.flushOldImages();
	}

	if (LLViewerTextureList::sProgressiveLoad)
	{
		F32 camera_moving_speed = gViewerCamera.getAverageSpeed();
		F32 camera_angular_speed = gViewerCamera.getAverageAngularSpeed();
		sCameraMovingBias = llmax(0.2f * camera_moving_speed,
								  2.f * camera_angular_speed - 1);
		sCameraMovingDiscardBias = (S8)sCameraMovingBias;
	}
	else
	{
		sCameraMovingDiscardBias = 0;
	}
}

LLViewerTexture::LLViewerTexture(bool usemipmaps)
:	LLGLTexture(usemipmaps)
{
	init(true);

	mID.generate();
	++sImageCount;
}

LLViewerTexture::LLViewerTexture(const LLUUID& id, bool usemipmaps)
:	LLGLTexture(usemipmaps),
	mID(id)
{
	init(true);

	++sImageCount;
}

LLViewerTexture::LLViewerTexture(U32 width, U32 height, U8 comp, bool mipmaps)
:	LLGLTexture(width, height, comp, mipmaps)
{
	init(true);

	mID.generate();
	++sImageCount;
}

LLViewerTexture::LLViewerTexture(const LLImageRaw* raw, bool usemipmaps)
:	LLGLTexture(raw, usemipmaps)
{
	init(true);

	mID.generate();
	++sImageCount;
}

LLViewerTexture::~LLViewerTexture()
{
	cleanup();
	--sImageCount;
}

//virtual
void LLViewerTexture::init(bool firstinit)
{
	mMaxVirtualSize = 0.f;
	mMaxVirtualSizeResetInterval = 1;
	mMaxVirtualSizeResetCounter = mMaxVirtualSizeResetInterval;
	mAdditionalDecodePriority = 0.f;
	mParcelMedia = NULL;
	for (U32 i = 0; i < LLRender::NUM_TEXTURE_CHANNELS; ++i)
	{
		mNumFaces[i] = 0;
		mFaceList[i].clear();
	}
	memset(&mNumVolumes, 0,
		   sizeof(U32) * LLRender::NUM_VOLUME_TEXTURE_CHANNELS);
	mVolumeList[LLRender::LIGHT_TEX].clear();
	mVolumeList[LLRender::SCULPT_TEX].clear();
	mLastReferencedTime = mLastFaceListUpdate = mLastVolumeListUpdate =
						  gFrameTimeSeconds;
}

void LLViewerTexture::setNeedsAlphaAndPickMask(bool b)
{
	if (mGLTexturep)
	{
		mGLTexturep->setNeedsAlphaAndPickMask(b);
	}
}

//virtual
S8 LLViewerTexture::getType() const
{
	return LLViewerTexture::LOCAL_TEXTURE;
}

void LLViewerTexture::cleanup()
{
	notifyAboutMissingAsset();

	mFaceList[LLRender::DIFFUSE_MAP].clear();
	mFaceList[LLRender::NORMAL_MAP].clear();
	mFaceList[LLRender::SPECULAR_MAP].clear();
	mVolumeList[LLRender::LIGHT_TEX].clear();
	mVolumeList[LLRender::SCULPT_TEX].clear();
}

void LLViewerTexture::notifyAboutCreatingTexture()
{
	for (U32 ch = 0; ch < LLRender::NUM_TEXTURE_CHANNELS; ++ch)
	{
		for (U32 f = 0, count = mNumFaces[ch]; f < count; ++f)
		{
			mFaceList[ch][f]->notifyAboutCreatingTexture(this);
		}
	}
}

void LLViewerTexture::notifyAboutMissingAsset()
{
	for (U32 ch = 0; ch < LLRender::NUM_TEXTURE_CHANNELS; ++ch)
	{
		for (U32 f = 0, count = mNumFaces[ch]; f < count; ++f)
		{
			mFaceList[ch][f]->notifyAboutMissingAsset(this);
		}
	}
}

// virtual
void LLViewerTexture::dump()
{
	LLGLTexture::dump();
	llinfos << "LLViewerTexture  mID: " << mID << llendl;
}

void LLViewerTexture::resetLastReferencedTime()
{
	mLastReferencedTime = gFrameTimeSeconds;
}

F32 LLViewerTexture::getElapsedLastReferenceTime()
{
	return gFrameTimeSeconds - mLastReferencedTime;
}

void LLViewerTexture::setBoostLevel(S32 level)
{
	if (mBoostLevel != level)
	{
		mBoostLevel = level;
		if (mBoostLevel > LLGLTexture::BOOST_ALM)
		{
			setNoDelete();
		}
	}
}

bool LLViewerTexture::bindDefaultImage(S32 stage)
{
	if (stage < 0) return false;

	bool res = true;
	if (LLViewerFetchedTexture::sDefaultImagep.notNull() &&
		LLViewerFetchedTexture::sDefaultImagep.get() != this)
	{
		// Use default if we got it
		res = gGL.getTexUnit(stage)->bind(LLViewerFetchedTexture::sDefaultImagep);
	}
	if (!res && LLViewerTexture::sNullImagep.notNull() &&
		LLViewerTexture::sNullImagep != this)
	{
		res = gGL.getTexUnit(stage)->bind(LLViewerTexture::sNullImagep);
	}
	if (!res)
	{
		llwarns << "Failed at stage: " << stage << llendl;
	}

	// Check if there is cached raw image and switch to it if possible
	switchToCachedImage();

	return res;
}

void LLViewerTexture::addTextureStats(F32 virtual_size,
									  bool needs_gltexture) const
{
	if (mDontDiscard)
	{
		// Do not allow the scaling down of do-not-discard textures !  HB
		constexpr F32 MAX_AREA = 1024.f * 1024.f;
		virtual_size = MAX_AREA;
	}
	if (!mMaxVirtualSizeResetCounter)
	{
		// Flag to reset the values because the old values are used.
		resetMaxVirtualSizeResetCounter();
		mMaxVirtualSize = virtual_size;
		mAdditionalDecodePriority = 0.f;
		mNeedsGLTexture = needs_gltexture;
		return;
	}
	if (needs_gltexture)
	{
		mNeedsGLTexture = true;
	}
	if (virtual_size > mMaxVirtualSize)
	{
		mMaxVirtualSize = virtual_size;
	}
}

void LLViewerTexture::resetTextureStats()
{
	if (!mDontDiscard)
	{
		// Do not allow the scaling down of do-not-discard textures !  HB
		mMaxVirtualSize = 0.f;
	}
	mAdditionalDecodePriority = 0.f;
	mMaxVirtualSizeResetCounter = 0;
}

void LLViewerTexture::addFace(U32 ch, LLFace* facep)
{
	if (ch > LLRender::NUM_TEXTURE_CHANNELS)
	{
		llwarns << "Channel value out of range: " << ch << llendl;
		llassert(false);
		return;
	}

	if (mNumFaces[ch] >= mFaceList[ch].size())
	{
		mFaceList[ch].resize(2 * mNumFaces[ch] + 1);
	}
	mFaceList[ch][mNumFaces[ch]] = facep;
	facep->setIndexInTex(ch, mNumFaces[ch]);
	++mNumFaces[ch];
	mLastFaceListUpdate = gFrameTimeSeconds;
}

//virtual
void LLViewerTexture::removeFace(U32 ch, LLFace* facep)
{
	if (ch > LLRender::NUM_TEXTURE_CHANNELS)
	{
		llwarns << "Channel value out of range: " << ch << llendl;
		llassert(false);
		return;
	}

	if (mNumFaces[ch] > 1)
	{
		U32 index = facep->getIndexInTex(ch);
		if (index < (U32)mFaceList[ch].size() && index < mNumFaces[ch])
		{
			mFaceList[ch][index] = mFaceList[ch][--mNumFaces[ch]];
			mFaceList[ch][index]->setIndexInTex(ch, index);
		}
		else
		{
			llwarns << "Index out of range !" << llendl;
			llassert(false);
		}
	}
	else
	{
		mFaceList[ch].clear();
		mNumFaces[ch] = 0;
	}
	mLastFaceListUpdate = gFrameTimeSeconds;
}

S32 LLViewerTexture::getTotalNumFaces() const
{
	S32 ret = 0;

	for (U32 i = 0; i < LLRender::NUM_TEXTURE_CHANNELS; ++i)
	{
		ret += mNumFaces[i];
	}

	return ret;
}

S32 LLViewerTexture::getNumFaces(U32 ch) const
{
	llassert(ch < LLRender::NUM_TEXTURE_CHANNELS);
	return mNumFaces[ch];
}

void LLViewerTexture::reorganizeFaceList()
{
	constexpr F32 MAX_WAIT_TIME = 20.f; // seconds
	constexpr U32 MAX_EXTRA_BUFFER_SIZE = 4;

	if (gFrameTimeSeconds - mLastFaceListUpdate < MAX_WAIT_TIME)
	{
		return;
	}

	for (U32 i = 0; i < LLRender::NUM_TEXTURE_CHANNELS; ++i)
	{
		if (mNumFaces[i] + MAX_EXTRA_BUFFER_SIZE > mFaceList[i].size())
		{
			return;
		}

		mFaceList[i].erase(mFaceList[i].begin() + mNumFaces[i],
						   mFaceList[i].end());
	}

	mLastFaceListUpdate = gFrameTimeSeconds;
}

void LLViewerTexture::addVolume(U32 ch, LLVOVolume* volumep)
{
	if (mNumVolumes[ch] >= mVolumeList[ch].size())
	{
		mVolumeList[ch].resize(2 * mNumVolumes[ch] + 1);
	}
	mVolumeList[ch][mNumVolumes[ch]] = volumep;
	volumep->setIndexInTex(ch, mNumVolumes[ch]);
	++mNumVolumes[ch];
	mLastVolumeListUpdate = gFrameTimeSeconds;
}

void LLViewerTexture::removeVolume(U32 ch, LLVOVolume* volumep)
{
	if (mNumVolumes[ch] > 1)
	{
		S32 index = volumep->getIndexInTex(ch);
		llassert(index < (S32)mVolumeList[ch].size() &&
				 index < (S32)mNumVolumes[ch]);
		mVolumeList[ch][index] = mVolumeList[ch][--mNumVolumes[ch]];
		mVolumeList[ch][index]->setIndexInTex(ch, index);
	}
	else
	{
		mVolumeList[ch].clear();
		mNumVolumes[ch] = 0;
	}
	mLastVolumeListUpdate = gFrameTimeSeconds;
}

void LLViewerTexture::reorganizeVolumeList()
{
	constexpr F32 MAX_WAIT_TIME = 20.f; // seconds
	constexpr U32 MAX_EXTRA_BUFFER_SIZE = 4;

	if (gFrameTimeSeconds - mLastVolumeListUpdate < MAX_WAIT_TIME)
	{
		return;
	}

	for (U32 i = 0; i < LLRender::NUM_VOLUME_TEXTURE_CHANNELS; ++i)
	{
		if (mNumVolumes[i] + MAX_EXTRA_BUFFER_SIZE > mVolumeList[i].size())
		{
			return;
		}
	}

	mLastVolumeListUpdate = gFrameTimeSeconds;
	for (U32 i = 0; i < LLRender::NUM_VOLUME_TEXTURE_CHANNELS; ++i)
	{
		mVolumeList[i].erase(mVolumeList[i].begin() + mNumVolumes[i],
							 mVolumeList[i].end());
	}
}

//-----------------------------------------------------------------------------
// LLViewerFetchedTexture
//-----------------------------------------------------------------------------

const std::string& fttype_to_string(const FTType& fttype)
{
	static const std::string ftt_unknown("FTT_UNKNOWN");
	static const std::string ftt_default("FTT_DEFAULT");
	static const std::string ftt_server_bake("FTT_SERVER_BAKE");
	static const std::string ftt_host_bake("FTT_HOST_BAKE");
	static const std::string ftt_map_tile("FTT_MAP_TILE");
	static const std::string ftt_local_file("FTT_LOCAL_FILE");
	static const std::string ftt_error("FTT_ERROR");
	switch (fttype)
	{
		case FTT_UNKNOWN:
			return ftt_unknown;

		case FTT_DEFAULT:
			return ftt_default;

		case FTT_SERVER_BAKE:
			return ftt_server_bake;

		case FTT_HOST_BAKE:
			return ftt_host_bake;

		case FTT_MAP_TILE:
			return ftt_map_tile;

		case FTT_LOCAL_FILE:
			return ftt_local_file;
	}
	return ftt_error;
}

LLViewerFetchedTexture::LLViewerFetchedTexture(const LLUUID& id,
											   FTType f_type,
											   const LLHost& host,
											   bool usemipmaps)
:	LLViewerTexture(id, usemipmaps),
	mTargetHost(host)
{
	init(true);
	mFTType = f_type;
	generateGLTexture();
	mGLTexturep->setNeedsAlphaAndPickMask(true);
	if (!host.isInvalid())
	{
		// We must request the image from the provided host sim.
		mCanUseHTTP = false;
	}
}

LLViewerFetchedTexture::LLViewerFetchedTexture(const LLImageRaw* raw,
											   FTType f_type,
											   bool usemipmaps)
:	LLViewerTexture(raw, usemipmaps)
{
	init(true);
	mFTType = f_type;
}

LLViewerFetchedTexture::LLViewerFetchedTexture(const std::string& url,
											   FTType f_type,
											   const LLUUID& id,
											   bool usemipmaps)
:	LLViewerTexture(id, usemipmaps),
	mUrl(url)
{
	init(true);
	mFTType = f_type;
	generateGLTexture();
	mGLTexturep->setNeedsAlphaAndPickMask(true);
}

void LLViewerFetchedTexture::init(bool firstinit)
{
	mOrigWidth = 0;
	mOrigHeight = 0;
	mNeedsAux = false;
	mRequestedDiscardLevel = -1;
	mRequestedDownloadPriority = 0.f;
	mFullyLoaded = false;
	mCanUseHTTP = true;
	mSkipCache = false;
	mDesiredDiscardLevel = MAX_DISCARD_LEVEL + 1;
	mMinDesiredDiscardLevel = MAX_DISCARD_LEVEL + 1;

	mKnownDrawWidth = 0;
	mKnownDrawHeight = 0;
	mKnownDrawSizeChanged = false;

	if (firstinit)
	{
		mDecodePriority = 0.f;
		mInImageList = false;
	}

	// Only set mIsMissingAsset true when we know for certain that the database
	// does not contain this image.
	mIsMissingAsset = false;

	// When force-deleting a request before it can complete, set this as true
	// to avoid false missing asset cases.
	mWasDeleted = false;

	mLoadedCallbackDesiredDiscardLevel = S8_MAX;
	mPauseLoadedCallBacks = true;

	mNeedsCreateTexture = false;

	mIsRawImageValid = false;
	mRawDiscardLevel = INVALID_DISCARD_LEVEL;
	mMinDiscardLevel = 0;

	mHasFetcher = false;
	mIsFetching = false;
	mFetchState = 0;
	mFetchPriority = 0;
	mDownloadProgress = 0.f;
	mFetchDeltaTime = 999999.f;
	mRequestDeltaTime = 0.f;
	mForSculpt = false;
#if LL_FAST_TEX_CACHE
	mInFastCacheList = false;
#endif

	mCachedRawImage = NULL;
	mCachedRawDiscardLevel = -1;
	mCachedRawImageReady = false;

	mSavedRawImage = NULL;
	mForceToSaveRawImage  = false;
	mSaveRawImage = false;
	mSavedRawDiscardLevel = -1;
	mDesiredSavedRawDiscardLevel = -1;
	mLastReferencedSavedRawImageTime = 0.f;
	mKeptSavedRawImageTime = 0.f;
	mLastCallBackActiveTime = 0.f;

	mFTType = FTT_UNKNOWN;

	mLastPacketTime = mStopFetchingTime = gFrameTimeSeconds;
}

LLViewerFetchedTexture::~LLViewerFetchedTexture()
{
	// NOTE: gTextureFetchp can be NULL when viewer is shutting down; this is
	// due to LLWearableList is singleton and is destroyed after
	// LLAppViewer::cleanup() was called (see ticket EXT-177).
	if (mHasFetcher && gTextureFetchp)
	{
		gTextureFetchp->deleteRequest(getID());
	}
	cleanup();
}

//virtual
S8 LLViewerFetchedTexture::getType() const
{
	return LLViewerTexture::FETCHED_TEXTURE;
}

void LLViewerFetchedTexture::cleanup()
{
	for (callback_list_t::iterator iter = mLoadedCallbackList.begin();
		 iter != mLoadedCallbackList.end(); )
	{
		LLLoadedCallbackEntry *entryp = *iter++;
		// We never finished loading the image, so indicate a failure.
		// Note: this allows mLoadedCallbackUserData to be cleaned up.
		entryp->mCallback(false, this, NULL, NULL, 0, true, entryp->mUserData);
		entryp->removeTexture(this);
		delete entryp;
	}
	mLoadedCallbackList.clear();
	mNeedsAux = false;

	// Clean up image data
	destroyRawImage();
	mCachedRawImage = NULL;
	mCachedRawDiscardLevel = -1;
	mCachedRawImageReady = false;
	mSavedRawImage = NULL;
	mSavedRawDiscardLevel = -1;
}

#if LL_FAST_TEX_CACHE
// Access the fast cache
void LLViewerFetchedTexture::loadFromFastCache()
{
	if (!mInFastCacheList)
	{
		return; // no need to access the fast cache.
	}
	mInFastCacheList = false;

	mRawImage = gTextureCachep->readFromFastCache(getID(), mRawDiscardLevel);
	if (mRawImage.notNull())
	{
		mFullWidth = mRawImage->getWidth() << mRawDiscardLevel;
		mFullHeight = mRawImage->getHeight() << mRawDiscardLevel;
		setTexelsPerImage();

		if (mFullWidth > MAX_IMAGE_SIZE || mFullHeight > MAX_IMAGE_SIZE)
		{
			// Discard all oversized textures.
			llwarns << "Oversized texture " << getID()
					<< ", setting as missing." << llendl;
			destroyRawImage();
			setIsMissingAsset();
			mRawDiscardLevel = INVALID_DISCARD_LEVEL;
		}
		else
		{
			mRequestedDiscardLevel = mDesiredDiscardLevel + 1;
			mIsRawImageValid = true;
			addToCreateTexture();
		}
	}
}
#endif

void LLViewerFetchedTexture::setForSculpt()
{
	constexpr S32 MAX_INTERVAL = 8; // frames

	mForSculpt = true;
	if (isForSculptOnly() && hasGLTexture() && !getBoundRecently())
	{
		destroyGLTexture(); //sculpt image does not need gl texture.
		mTextureState = ACTIVE;
	}
	checkCachedRawSculptImage();
	setMaxVirtualSizeResetInterval(MAX_INTERVAL);
}

void LLViewerFetchedTexture::setDeletionCandidate()
{
	if (mGLTexturep.notNull() && mGLTexturep->getTexName() &&
		mTextureState == INACTIVE)
	{
		mTextureState = DELETION_CANDIDATE;
	}
}

//set the texture inactive
void LLViewerFetchedTexture::setInactive()
{
	if (mTextureState == ACTIVE &&
		mGLTexturep.notNull() && mGLTexturep->getTexName() &&
		!mGLTexturep->getBoundRecently())
	{
		mTextureState = INACTIVE;
	}
}

bool LLViewerFetchedTexture::isFullyLoaded() const
{
	// Unfortunately, the boolean "mFullyLoaded" is never updated correctly so
	// we use that logic to check if the texture is there and completely
	// downloaded
	return mFullWidth != 0 && mFullHeight != 0 && !mIsFetching && !mHasFetcher;
}

// virtual
void LLViewerFetchedTexture::dump()
{
	LLViewerTexture::dump();

	llinfos << "Dump : " << mID
			<< ", mIsMissingAsset = " << (S32)mIsMissingAsset
			<< ", mFullWidth = " << mFullWidth
			<< ", mFullHeight = " << mFullHeight
			<< ", mOrigWidth = " << mOrigWidth
			<< ", mOrigHeight = " << mOrigHeight
			<< llendl;
	llinfos << "     : "
			<< " mFullyLoaded = " << (S32)mFullyLoaded
			<< ", mFetchState = " << (S32)mFetchState
			<< ", mFetchPriority = " << (S32)mFetchPriority
			<< ", mDownloadProgress = " << (F32)mDownloadProgress
			<< llendl;
	llinfos << "     : "
			<< " mHasFetcher = " << (S32)mHasFetcher
			<< ", mIsFetching = " << (S32)mIsFetching
			<< ", mWasDeleted = " << (S32)mWasDeleted
			<< ", mBoostLevel = " << (S32)mBoostLevel
			<< llendl;
}

///////////////////////////////////////////////////////////////////////////////
// ONLY called from LLViewerFetchedTextureList
void LLViewerFetchedTexture::destroyTexture()
{
	if (mNeedsCreateTexture)
	{
		// return if in the process of generating a new texture.
		return;
	}

	destroyGLTexture();
	mFullyLoaded = false;
}

void LLViewerFetchedTexture::addToCreateTexture()
{
	bool force_update = false;
	if (getComponents() != mRawImage->getComponents())
	{
		// We've changed the number of components, so we need to move any
		// object using this pool to a different pool.
		mComponents = mRawImage->getComponents();
		mGLTexturep->setComponents(mComponents);
		force_update = true;

		for (U32 j = 0; j < LLRender::NUM_TEXTURE_CHANNELS; ++j)
		{
			U32 count = mNumFaces[j];
			U32 list_size = mFaceList[j].size();
			if (count > list_size)
			{
				llwarns_once << "Face count greater than face list size for texture channel: "
							 << j << ". Clamping down." << llendl;
				count = list_size;
			}

			for (U32 i = 0; i < count; ++i)
			{
				LLFace* facep = mFaceList[j][i];
				if (facep)
				{
					facep->dirtyTexture();
				}
			}
		}

		// discard the cached raw image and the saved raw image
		mCachedRawImageReady = false;
		mCachedRawDiscardLevel = -1;
		mCachedRawImage = NULL;
		mSavedRawDiscardLevel = -1;
		mSavedRawImage = NULL;
	}

	if (isForSculptOnly())
	{
		//just update some variables, not to create a real GL texture.
		createGLTexture(mRawDiscardLevel, mRawImage, 0, false);
		mNeedsCreateTexture = false;
		destroyRawImage();
	}
	else if (!force_update && getDiscardLevel() > -1 &&
			 getDiscardLevel() <= mRawDiscardLevel)
	{
		mNeedsCreateTexture = false;
		destroyRawImage();
	}
	else
	{
		// LLImageRaw:scale() allows for a lower memory usage but also causes
		// memory fragmentation... This is a trade off !
		static LLCachedControl<bool> rescale(gSavedSettings,
											 "TextureRescaleFetched");
		// If mRequestedDiscardLevel > mDesiredDiscardLevel, we assume the
		// required image res keeps going up, so do not scale down the over
		// qualified image. Note: scaling down image is expensensive. Do it
		// only when very necessary.
		if (rescale && !mForceToSaveRawImage &&
			mRequestedDiscardLevel <= mDesiredDiscardLevel)
		{
			S32 w = mFullWidth >> mRawDiscardLevel;
			S32 h = mFullHeight >> mRawDiscardLevel;

			// If big image, do not load extra data, scale it down to size
			// >= LLViewerTexture::sMinLargeImageSize
			if (w * h > LLViewerTexture::sMinLargeImageSize)
			{
				S32 d_level = llmin(mRequestedDiscardLevel,
									(S32)mDesiredDiscardLevel) - mRawDiscardLevel;

				if (d_level > 0)
				{
					S32 i = 0;
					while (d_level > 0 &&
						   (w >> i) * (h >> i) > LLViewerTexture::sMinLargeImageSize)
					{
						++i;
						--d_level;
					}
					if (i > 0)
					{
						mRawDiscardLevel += i;
						if (mRawDiscardLevel >= getDiscardLevel() &&
							getDiscardLevel() > 0)
						{
							mNeedsCreateTexture = false;
							destroyRawImage();
							return;
						}
						// Make a duplicate in case somebody else is using this
						// raw image:
						LLPointer<LLImageRaw> dup =
							mRawImage->scaled(w >> i, h >> i);
						if (dup.notNull())
						{
							mRawImage = std::move(dup);
						}
					}
				}
			}
		}
		mNeedsCreateTexture = true;
		gTextureList.mCreateTextureList.insert(this);
	}
	return;
}

// ONLY called from LLViewerTextureList
bool LLViewerFetchedTexture::createTexture(S32 usename)
{
	if (!mNeedsCreateTexture)
	{
		destroyRawImage();
		return false;
	}
	mNeedsCreateTexture	= false;
	if (mRawImage.isNull())
	{
		llwarns << "Trying to create texture " << mID.asString()
				<< " without raw image: aborting !" << llendl;
		destroyRawImage();
		return false;
	}
	LL_DEBUGS("ViewerTexture") << "Creating image " << mID.asString()
							   << " - discard level =  " << mRawDiscardLevel
							   << " - Size: " << mRawImage->getWidth() << "x"
							   << mRawImage->getHeight() << " pixels - "
							   << mRawImage->getDataSize() << " bytes."
							   << LL_ENDL;
	bool res = true;

	// Store original size only for locally-sourced images
	if (mUrl.compare(0, 7, "file://") == 0)
	{
		mOrigWidth = mRawImage->getWidth();
		mOrigHeight = mRawImage->getHeight();

#if 0
		if (mBoostLevel == BOOST_PREVIEW)
		{
			mRawImage->biasedScaleToPowerOfTwo(1024);
		}
		else
		{
			// Leave black border, do not scale image content
			mRawImage->expandToPowerOfTwo(MAX_IMAGE_SIZE, false);
		}
#else
		// Do not scale image content
		mRawImage->expandToPowerOfTwo(MAX_IMAGE_SIZE, false);
#endif

		mFullWidth = mRawImage->getWidth();
		mFullHeight = mRawImage->getHeight();
		setTexelsPerImage();
	}
	else
	{
		mOrigWidth = mFullWidth;
		mOrigHeight = mFullHeight;
	}

	bool size_okay = true;

	S32 raw_width = mRawImage->getWidth() << mRawDiscardLevel;
	S32 raw_height = mRawImage->getHeight() << mRawDiscardLevel;
	if (raw_width > MAX_IMAGE_SIZE || raw_height > MAX_IMAGE_SIZE)
	{
		llinfos << "Width or height is greater than " << MAX_IMAGE_SIZE
				<< ": (" << raw_width << "," << raw_height << ")" << llendl;
		size_okay = false;
	}

	if (!LLImageGL::checkSize(mRawImage->getWidth(), mRawImage->getHeight()))
	{
		// A non power-of-two image was uploaded through a non standard client
		llinfos << "Non power of two width or height: (" << mRawImage->getWidth()
				<< "," << mRawImage->getHeight() << ")" << llendl;
		size_okay = false;
	}

	if (!size_okay)
	{
		// An inappropriately-sized image was uploaded through a non standard
		// client. We treat these images as missing assets which causes them to
		// be renderd as 'missing image' and to stop requesting data.
		llwarns << "Image " << mID.asString()
				<< " does not have an acceptable size, setting as missing."
				<< llendl;
		setIsMissingAsset();
		destroyRawImage();
		return false;
	}

	if (mGLTexturep->hasExplicitFormat())
	{
		U32 format = mGLTexturep->getPrimaryFormat();
		S8 components = mRawImage->getComponents();
		if (((format == GL_RGBA && components < 4) ||
			(format == GL_RGB && components < 3)))
		{
			llwarns << "Cannot create texture " << mID
					<< ": invalid image format: " << std::hex << format
					<< std::dec << " - Number of components: " << components
					<< llendl;
			// Was expecting specific format but raw texture has insufficient
			// components for such format, using such texture would result in a
			// crash or would display wrongly. Texture might be corrupted
			// server side, so just set as missing and clear cached texture.
			setIsMissingAsset();
			destroyRawImage();
			gTextureCachep->removeFromCache(mID);
			return false;
		}
	}

	res = mGLTexturep->createGLTexture(mRawDiscardLevel, mRawImage, usename,
									   true, mBoostLevel);

	notifyAboutCreatingTexture();

	setActive();

	if (!needsToSaveRawImage())
	{
		mNeedsAux = false;
		destroyRawImage();
	}

	return res;
}

// Call with 0,0 to turn this feature off.
//virtual
void LLViewerFetchedTexture::setKnownDrawSize(S32 width, S32 height)
{
	if (mKnownDrawWidth < width || mKnownDrawHeight < height)
	{
		mKnownDrawWidth = llmax(mKnownDrawWidth, width);
		mKnownDrawHeight = llmax(mKnownDrawHeight, height);

		mKnownDrawSizeChanged = true;
		mFullyLoaded = false;
	}
	addTextureStats((F32)(mKnownDrawWidth * mKnownDrawHeight));
}

//virtual
void LLViewerFetchedTexture::processTextureStats()
{
	if (mFullyLoaded)
	{
		if (mDesiredDiscardLevel > mMinDesiredDiscardLevel)
		{
			// Need to load more
			mDesiredDiscardLevel = mMinDesiredDiscardLevel;
			mFullyLoaded = false;
		}
	}
	else
	{
		updateVirtualSize();

		static LLCachedControl<bool> textures_fullres(gSavedSettings,
													  "TextureLoadFullRes");

		if (textures_fullres)
		{
			mDesiredDiscardLevel = 0;
		}
		else if (!LLPipeline::sRenderDeferred &&
				 mBoostLevel == LLGLTexture::BOOST_ALM)
		{
			mDesiredDiscardLevel = MAX_DISCARD_LEVEL + 1;
		}
		else if (!mFullWidth || !mFullHeight)
		{
			mDesiredDiscardLevel = 	llmin(getMaxDiscardLevel(),
										  (S32)mLoadedCallbackDesiredDiscardLevel);
		}
		else
		{
			if (!mKnownDrawWidth || !mKnownDrawHeight ||
				mFullWidth <= mKnownDrawWidth || mFullHeight <= mKnownDrawHeight)
			{
				if (mFullWidth > MAX_IMAGE_SIZE_DEFAULT ||
					mFullHeight > MAX_IMAGE_SIZE_DEFAULT)
				{
					// MAX_IMAGE_SIZE_DEFAULT = 1024 and max size ever is 2048
					mDesiredDiscardLevel = 1;
				}
				else
				{
					mDesiredDiscardLevel = 0;
				}
			}
			else if (mKnownDrawSizeChanged)	// Known draw size is set
			{
				F32 ratio = llmin((F32)mFullWidth / (F32)mKnownDrawWidth,
								  (F32)mFullHeight / (F32)mKnownDrawHeight);
				mDesiredDiscardLevel = (S8)logf(ratio / F_LN2);
				mDesiredDiscardLevel = llclamp(mDesiredDiscardLevel, (S8)0,
											   (S8)getMaxDiscardLevel());
				mDesiredDiscardLevel = llmin(mDesiredDiscardLevel,
											 mMinDesiredDiscardLevel);
			}
			mKnownDrawSizeChanged = false;

			if (getDiscardLevel() >= 0 &&
				getDiscardLevel() <= mDesiredDiscardLevel)
			{
				mFullyLoaded = true;
			}
		}
	}

	if (mForceToSaveRawImage && mDesiredSavedRawDiscardLevel >= 0)
	{
		// Force to refetch the texture.
		mDesiredDiscardLevel = llmin(mDesiredDiscardLevel,
									 (S8)mDesiredSavedRawDiscardLevel);
		if (getDiscardLevel() < 0 || getDiscardLevel() > mDesiredDiscardLevel)
		{
			mFullyLoaded = false;
		}
	}
}

constexpr F32 MAX_PRIORITY_PIXEL					= 999.f;	// Pixel area
constexpr F32 PRIORITY_BOOST_LEVEL_FACTOR			= 1000.f;	// Boost level
constexpr F32 PRIORITY_DELTA_DISCARD_LEVEL_FACTOR	= 100000.f;	// Delta discard
constexpr S32 MAX_DELTA_DISCARD_LEVEL_FOR_PRIORITY	= 4;
constexpr F32 PRIORITY_ADDITIONAL_FACTOR			= 1000000.f; // Additional
constexpr S32 MAX_ADDITIONAL_LEVEL_FOR_PRIORITY		= 8;
constexpr F32 PRIORITY_BOOST_HIGH_FACTOR 			= 10000000.f; // Boost high
constexpr F32 MAX_DECODE_PRIORITY = PRIORITY_BOOST_HIGH_FACTOR +
									PRIORITY_ADDITIONAL_FACTOR *
									(MAX_ADDITIONAL_LEVEL_FOR_PRIORITY + 1) +
									PRIORITY_DELTA_DISCARD_LEVEL_FACTOR *
									(MAX_DELTA_DISCARD_LEVEL_FOR_PRIORITY + 1) +
									PRIORITY_BOOST_LEVEL_FACTOR *
									(LLGLTexture::BOOST_MAX_LEVEL - 1) +
									MAX_PRIORITY_PIXEL + 1.f;

F32 LLViewerFetchedTexture::calcDecodePriority()
{
	if (mNeedsCreateTexture)
	{
		return mDecodePriority; // No change while waiting to create
	}
	if (mFullyLoaded && !mForceToSaveRawImage)
	{
		return -1.f;	// Already loaded for static texture
	}

	S32 cur_discard = getCurrentDiscardLevelForFetching();
	bool have_all_data = cur_discard >= 0 &&
						 cur_discard <= mDesiredDiscardLevel;
	F32 pixel_priority = sqrtf(mMaxVirtualSize);

	F32 priority = 0.f;

	if (mIsMissingAsset || mWasDeleted)
	{
		priority = 0.f;
	}
	else if (mDesiredDiscardLevel >= cur_discard && cur_discard > -1)
	{
		priority = -2.f;
	}
	else if (mCachedRawDiscardLevel > -1 &&
			 mDesiredDiscardLevel >= mCachedRawDiscardLevel)
	{
		priority = -3.f;
	}
	else if (mDesiredDiscardLevel > getMaxDiscardLevel())
	{
		// Do not decode anything we do not need
		priority = -4.f;
	}
	else if (!have_all_data &&
			 (mBoostLevel == LLGLTexture::BOOST_UI ||
			  mBoostLevel == LLGLTexture::BOOST_ICON))
	{
		priority = 1.f;
	}
	else if (pixel_priority < 0.001f && !have_all_data)
	{
		// Not on screen but we might want some data
		if (mBoostLevel > BOOST_HIGH)
		{
			// Always want high boosted images
			priority = 1.f;
		}
		else
		{
			priority = -5.f; // Stop fetching
		}
	}
	else if (cur_discard < 0)
	{
		// Texture does not have any data, so we do not know the size of the
		// image, treat it like 32 * 32. Priority range = 100,000 - 500,000
		F32 desired = (F32)(logf(32.f / pixel_priority) / F_LN2);
		S32 ddiscard = MAX_DISCARD_LEVEL - (S32)desired;
		ddiscard = llclamp(ddiscard, 0, MAX_DELTA_DISCARD_LEVEL_FOR_PRIORITY);
		priority = (ddiscard + 1) * PRIORITY_DELTA_DISCARD_LEVEL_FACTOR;
		// Boost the textures without any data so far
		setAdditionalDecodePriority(0.1f);
	}
	else if (mMinDiscardLevel > 0 && cur_discard <= mMinDiscardLevel)
	{
		// Larger mips are corrupted
		priority = -6.f;
	}
	else
	{
		// Priority range = 100,000 - 500,000
		S32 desired_discard = mDesiredDiscardLevel;
		if (!isJustBound() && mCachedRawImageReady)
		{
			if (mBoostLevel < BOOST_HIGH)
			{
				// We do not have rendered this in a while, de-prioritize it
				desired_discard += 2;
			}
			else
			{
				// We do not have rendered this in the last half second, and we
				// have a cached raw image, leave the desired discard as-is
				desired_discard = cur_discard;
			}
		}

		S32 ddiscard = cur_discard - desired_discard;
		ddiscard = llclamp(ddiscard, -1, MAX_DELTA_DISCARD_LEVEL_FOR_PRIORITY);
		priority = (ddiscard + 1) * PRIORITY_DELTA_DISCARD_LEVEL_FACTOR;
	}

	// Priority Formula:
	// BOOST_HIGH  +  ADDITIONAL PRI + DELTA DISCARD + BOOST LEVEL + PIXELS
	// [10,000,000] + [1,000,000-9,000,000]  + [100,000-500,000]   + [1-20,000]  + [0-999]
	if (priority > 0.f)
	{
		bool large_enough = mCachedRawImageReady &&
							mTexelsPerImage > sMinLargeImageSize;
		if (large_enough)
		{
			// Note: to give small, low-priority textures some chance to be
			// fetched, cut the priority in half if the texture size is larger
			// than 256 * 256 and has a 64 * 64 ready.
			priority *= 0.5f;
		}

		pixel_priority = llclamp(pixel_priority, 0.f, MAX_PRIORITY_PIXEL);

		priority += pixel_priority + PRIORITY_BOOST_LEVEL_FACTOR * mBoostLevel;

		if (mBoostLevel > BOOST_HIGH)
		{
			if (mBoostLevel > BOOST_SUPER_HIGH)
			{
				// For very important textures, always grant the highest
				// priority.
				priority += PRIORITY_BOOST_HIGH_FACTOR;
			}
			else if (mCachedRawImageReady)
			{
				// Note: to give small, low-priority textures some chance to be
				// fetched, if high priority texture has a 64*64 ready, lower
				// its fetching priority.
				setAdditionalDecodePriority(0.5f);
			}
			else
			{
				priority += PRIORITY_BOOST_HIGH_FACTOR;
			}
		}

		if (mAdditionalDecodePriority > 0.f)
		{
			// Priority range += 1,000,000.f-9,000,000.f
			F32 additional = PRIORITY_ADDITIONAL_FACTOR *
							 (1.0 + mAdditionalDecodePriority *
							  MAX_ADDITIONAL_LEVEL_FOR_PRIORITY);
			if (large_enough)
			{
				// Note: to give small, low-priority textures some chance to be
				// fetched, cut the additional priority to a quarter if the
				// texture size is larger than 256 * 256 and has a 64*64 ready.
				additional *= 0.25f;
			}
			priority += additional;
		}
	}

	return priority;
}

//static
F32 LLViewerFetchedTexture::maxDecodePriority()
{
	return MAX_DECODE_PRIORITY;
}

void LLViewerFetchedTexture::setDecodePriority(F32 priority)
{
	mDecodePriority = priority;
	if (mDecodePriority < F_ALMOST_ZERO)
	{
		mStopFetchingTime = gFrameTimeSeconds;
	}
}

void LLViewerFetchedTexture::setAdditionalDecodePriority(F32 priority)
{
	priority = llclamp(priority, 0.f, 1.f);
	if (mAdditionalDecodePriority < priority)
	{
		mAdditionalDecodePriority = priority;
	}
}

void LLViewerFetchedTexture::updateVirtualSize()
{
	if (!mMaxVirtualSizeResetCounter)
	{
		addTextureStats(0.f, false);	// Reset
	}

	for (U32 ch = 0; ch < LLRender::NUM_TEXTURE_CHANNELS; ++ch)
	{
		U32 count = mNumFaces[ch];
		U32 list_size = mFaceList[ch].size();
		if (count > list_size)
		{
			llwarns_once << "Face count greater than face list size for texture channel: "
						 << ch << ". Clamping down." << llendl;
			count = list_size;
		}

		for (U32 i = 0 ; i < count; ++i)
		{
			LLFace* facep = mFaceList[ch][i];
			if (facep)
			{
				LLDrawable* drawable = facep->getDrawable();
				if (drawable && drawable->isRecentlyVisible())
				{
					addTextureStats(facep->getVirtualSize());
					setAdditionalDecodePriority(facep->getImportanceToCamera());
				}
			}
		}
	}

	if (mMaxVirtualSizeResetCounter > 0)
	{
		--mMaxVirtualSizeResetCounter;
	}
	reorganizeFaceList();
	reorganizeVolumeList();
}

S32 LLViewerFetchedTexture::getCurrentDiscardLevelForFetching()
{
	S32 current_discard = getDiscardLevel();
	if (mForceToSaveRawImage)
	{
		if (mSavedRawDiscardLevel < 0 || current_discard < 0)
		{
			current_discard = -1;
		}
		else
		{
			current_discard = llmax(current_discard, mSavedRawDiscardLevel);
		}
	}
	return current_discard;
}

bool LLViewerFetchedTexture::updateFetch()
{
	static LLCachedControl<F32> sCameraMotionThreshold(gSavedSettings,
													   "TextureCameraMotionThreshold");
	static LLCachedControl<S32> sCameraMotionBoost(gSavedSettings,
												   "TextureCameraMotionBoost");

	if (gUseWireframe && mBoostLevel < LLGLTexture::BOOST_AVATAR_BAKED_SELF)
	{
		// Do not fetch the surface textures in wireframe mode
		return false;
	}

	mFetchState = 0;
	mFetchPriority = 0;
	mFetchDeltaTime = mRequestDeltaTime = 999999.f;

	if (mNeedsCreateTexture)
	{
		// We may be fetching still (e.g. waiting on write) but do not check
		// until we have processed the raw data we have.
		return false;
	}
	if (mIsMissingAsset)
	{
		llassert_always(!mHasFetcher);
		return false; // Skip
	}
	if (!mLoadedCallbackList.empty() && mRawImage.notNull())
	{
		// Process any raw image data in callbacks before replacing
		return false;
	}
#if LL_FAST_TEX_CACHE
	if (mInFastCacheList)
	{
		return false;
	}
#endif

	S32 current_discard = getCurrentDiscardLevelForFetching();
	S32 desired_discard = getDesiredDiscardLevel();
	F32 decode_priority = llclamp(getDecodePriority(), 0.f,
								  MAX_DECODE_PRIORITY);

	if (mIsFetching)
	{
		// Sets mRawDiscardLevel, mRawImage, mAuxRawImage
		S32 fetch_discard = current_discard;

		if (mRawImage.notNull())
		{
			--sRawCount;
		}
		if (mAuxRawImage.notNull())
		{
			--sAuxCount;
		}
		bool finished = gTextureFetchp->getRequestFinished(getID(),
														   fetch_discard,
														   mRawImage,
														   mAuxRawImage,
														   mLastHttpGetStatus);
		if (mRawImage.notNull())
		{
			++sRawCount;
		}
		if (mAuxRawImage.notNull())
		{
			++sAuxCount;
		}
		if (finished)
		{
			mIsFetching = false;
			mLastPacketTime = gFrameTimeSeconds;
		}
		else
		{
			mFetchState =
				gTextureFetchp->getFetchState(mID, mDownloadProgress,
											  mRequestedDownloadPriority,
											  mFetchPriority, mFetchDeltaTime,
											  mRequestDeltaTime, mCanUseHTTP);
		}

		// We may have data ready regardless of whether or not we are finished
		// (e.g. waiting on write)
		if (mRawImage.notNull())
		{
			mRawDiscardLevel = fetch_discard;
			if (mRawImage->getDataSize() > 0 && mRawDiscardLevel >= 0 &&
				(current_discard < 0 || mRawDiscardLevel < current_discard))
			{
				mFullWidth = mRawImage->getWidth() << mRawDiscardLevel;
				mFullHeight = mRawImage->getHeight() << mRawDiscardLevel;
				setTexelsPerImage();

				if (mFullWidth > MAX_IMAGE_SIZE ||
					mFullHeight > MAX_IMAGE_SIZE)
				{
					// Discard all oversized textures.
					destroyRawImage();
					setIsMissingAsset();
					mRawDiscardLevel = INVALID_DISCARD_LEVEL;
					mIsFetching = false;
					mLastPacketTime = gFrameTimeSeconds;
				}
				else
				{
					mIsRawImageValid = true;
					addToCreateTexture();
				}

				return true;
			}
			else
			{
				// Data is ready but we do not need it (received it already
				// while the fetcher was writing to disk)
				destroyRawImage();
				return false; // done
			}
		}

		// Seconds to wait before cancelling fetching if decode_priority is 0
		constexpr F32 MAX_HOLD_TIME = 5.f;

		if (!mIsFetching)
		{
			if (decode_priority > 0 &&
				(mRawDiscardLevel < 0 ||
				 mRawDiscardLevel == INVALID_DISCARD_LEVEL))
			{
				// We finished but received no data
				S32 actual_level = getDiscardLevel();
				if (actual_level < 0)
				{
					if (!mWasDeleted && getFTType() != FTT_MAP_TILE)
					{
						llwarns << "No data received for image " << mID
								<< ", setting as missing. decode_priority = "
								<< decode_priority << " - mRawDiscardLevel = "
								<< mRawDiscardLevel << " - current_discard = "
								<< current_discard << llendl;
					}
					setIsMissingAsset();
					desired_discard = -1;
				}
				else
				{
					LL_DEBUGS("ViewerTexture") << "Texture: " << mID
											   << " - Setting min discard to "
											   << current_discard << LL_ENDL;
					if (current_discard >= 0)
					{
						mMinDiscardLevel = current_discard;
						desired_discard = current_discard;
					}
					else
					{
						mMinDiscardLevel = actual_level;
						desired_discard = actual_level;
					}
				}
				destroyRawImage();
			}
			else if (mRawImage.notNull())
			{
				// We have data, but our fetch failed to return raw data.
				// *TODO: Figure out why this is happening and fix it.
				LL_DEBUGS("ViewerTexture") << "Texture: " << mID
										   << " - We have data but fetch failed to return raw data."
										   << LL_ENDL;
				destroyRawImage();
			}
		}
		else if (decode_priority > 0.f ||
				 gFrameTimeSeconds - mStopFetchingTime > MAX_HOLD_TIME)
		{
			mStopFetchingTime = gFrameTimeSeconds;
			gTextureFetchp->updateRequestPriority(mID, decode_priority);
		}
	}

	bool make_request = true;
	if (decode_priority <= 0)
	{
		make_request = false;
	}
	else if (mDesiredDiscardLevel > getMaxDiscardLevel())
	{
		make_request = false;
	}
	else if (mNeedsCreateTexture || mIsMissingAsset)
	{
		make_request = false;
	}
	else if (current_discard >= 0 && current_discard <= mMinDiscardLevel)
	{
		make_request = false;
	}
	else if (mCachedRawImage.notNull() && mCachedRawImageReady &&
			 (current_discard < 0 || current_discard > mCachedRawDiscardLevel))
	{
		make_request = false;
		switchToCachedImage(); // Use the cached raw data first
	}
#if 0
	else if (!isJustBound() && mCachedRawImageReady)
	{
		make_request = false;
	}
#endif

	if (make_request)
	{
		if (LLViewerTextureList::sProgressiveLoad)
		{
			// Load the texture progressively.
			S32 delta_level = mBoostLevel > LLGLTexture::BOOST_ALM ? 2 : 1;
			if (current_discard < 0)
			{
				desired_discard = llmax(desired_discard,
										getMaxDiscardLevel() - delta_level);
			}
			else if (LLViewerTexture::sCameraMovingBias < sCameraMotionThreshold)
			{
				desired_discard = llmax(desired_discard,
										current_discard - sCameraMotionBoost);
			}
			else
			{
				desired_discard = llmax(desired_discard,
										current_discard - delta_level);
			}
		}

		if (mIsFetching)
		{
			if (mRequestedDiscardLevel <= desired_discard)
			{
				make_request = false;
			}
		}
		else if (current_discard >= 0 && current_discard <= desired_discard)
		{
			make_request = false;
		}
	}

	if (make_request)
	{
		mWasDeleted = false;

		S32 w = 0, h = 0, c = 0;
		if (getDiscardLevel() >= 0)
		{
			w = mGLTexturep->getWidth(0);
			h = mGLTexturep->getHeight(0);
			c = mComponents;
		}

#if 0	// Not implemented in the Cool VL Viewer
		static LLCachedControl<U32> override_discard(gSavedSettings,
													 "TextureDiscardLevel");
		if (override_discard != 0)
		{
			desired_discard = override_discard;
		}
#endif

		// Bypass texturefetch directly by pulling from LLTextureCache
		bool fetch_request_created =
			gTextureFetchp->createRequest(mFTType, mUrl, getID(),
										  getTargetHost(), decode_priority,
										  w, h, c, desired_discard, needsAux(),
										  mCanUseHTTP, mSkipCache);
		if (fetch_request_created)
		{
			mHasFetcher = mIsFetching = true;
			mRequestedDiscardLevel = desired_discard;
			mFetchState =
				gTextureFetchp->getFetchState(mID, mDownloadProgress,
											  mRequestedDownloadPriority,
											  mFetchPriority, mFetchDeltaTime,
											  mRequestDeltaTime, mCanUseHTTP);
		}

		// If createRequest() failed, we are finishing up a request for this
		// UUID, wait for it to complete
	}
	else if (mHasFetcher && !mIsFetching)
	{
		// Only delete requests that do not have received any network data for
		// a while
		constexpr F32 FETCH_IDLE_TIME = 5.f;
		if (gFrameTimeSeconds - mLastPacketTime > FETCH_IDLE_TIME)
		{
			LL_DEBUGS("ViewerTexture") << "Exceeded idle time. Deleting request for texture "
									   << mID << LL_ENDL;
			gTextureFetchp->deleteRequest(mID);
			mHasFetcher = false;
		}
	}

	if (mRawImage.isNull() && (mNeedsCreateTexture || mIsRawImageValid))
	{
		llwarns << "Incoherent fetcher state for texture " << mID
				<< ": mRawImage is NULL while mNeedsCreateTexture is "
				<< mNeedsCreateTexture << " and mIsRawImageValid is "
				<< mIsRawImageValid << llendl;
		llassert(false);
	}

	return mIsFetching;
}

void LLViewerFetchedTexture::clearFetchedResults()
{
	if (mNeedsCreateTexture || mIsFetching)
	{
		return;
	}

	cleanup();
	destroyGLTexture();

	if (getDiscardLevel() >= 0) // Sculpty texture; force to invalidate
	{
		mGLTexturep->forceToInvalidateGLTexture();
	}
}

void LLViewerFetchedTexture::requestWasDeleted()
{
	mWasDeleted = true;
	resetTextureStats();
}

void LLViewerFetchedTexture::setIsMissingAsset(bool is_missing)
{
	if (is_missing && mWasDeleted)
	{
		mWasDeleted = false;
		LL_DEBUGS("ViewerTexture") << "Fetch request for texture " << mID
								   << " was deleted in flight. Not marking as missing asset."
								   << LL_ENDL;
		return;
	}
	if (is_missing == mIsMissingAsset)
	{
		// No change
		return;
	}
	if (is_missing)
	{
		notifyAboutMissingAsset();

		if (mUrl.empty())
		{
			llwarns << mID << ": Marking image as missing" << llendl;
		}
		// It is normal to have no map tile on an empty region, but bad if
		// we are failing on a server bake texture.
		else if (getFTType() != FTT_MAP_TILE)
		{
			llwarns << mUrl << ": Marking image as missing" << llendl;
		}
		if (mHasFetcher)
		{
			gTextureFetchp->deleteRequest(mID);
			mHasFetcher = false;
			mIsFetching = false;
			mLastPacketTime = gFrameTimeSeconds;
			mFetchState = 0;
			mFetchPriority = 0;
		}
	}
	else
	{
		llinfos << mID << ": un-flagging missing asset." << llendl;
	}
	mIsMissingAsset = is_missing;
}

void LLViewerFetchedTexture::setLoadedCallback(loaded_callback_func loaded_callback,
											   S32 discard_level,
											   bool keep_imageraw,
											   bool needs_aux,
											   void* userdata,
											   uuid_list_t* src_cb_list,
											   bool pause)
{
	// Do not do ANYTHING here, just add it to the global callback list
	if (mLoadedCallbackList.empty())
	{
		// Put in list to call this->doLoadedCallbacks() periodically
		gTextureList.mCallbackList.insert(this);
		mLoadedCallbackDesiredDiscardLevel = (S8)discard_level;
	}
	else
	{
		mLoadedCallbackDesiredDiscardLevel = llmin(mLoadedCallbackDesiredDiscardLevel,
												   (S8)discard_level);
	}

	if (mPauseLoadedCallBacks)
	{
		if (!pause)
		{
			unpauseLoadedCallbacks(src_cb_list);
		}
	}
	else if (pause)
	{
		pauseLoadedCallbacks(src_cb_list);
	}

	LLLoadedCallbackEntry* entryp =
		new LLLoadedCallbackEntry(loaded_callback, discard_level,
								  keep_imageraw, userdata,
								  src_cb_list, this, pause);
	mLoadedCallbackList.push_back(entryp);

	if (needs_aux)
	{
		mNeedsAux = true;
	}
	if (keep_imageraw)
	{
		mSaveRawImage = true;
	}
	if (mNeedsAux && mAuxRawImage.isNull() && getDiscardLevel() >= 0)
	{
		// We need aux data but we have already loaded the image and it did not
		// have any. This is a common case with cached baked textures, so make
		// if an info message instead of a warning...
		llinfos_once << "No aux data available for callback for image: " << mID
					 << llendl;
	}
	mLastCallBackActiveTime = sCurrentTime;
}

void LLViewerFetchedTexture::clearCallbackEntryList()
{
	if (mLoadedCallbackList.empty())
	{
		return;
	}

	for (callback_list_t::iterator iter = mLoadedCallbackList.begin();
		 iter != mLoadedCallbackList.end(); )
	{
		LLLoadedCallbackEntry* entryp = *iter;

		// We never finished loading the image.  Indicate failure.
		// Note: this allows mLoadedCallbackUserData to be cleaned up.
		entryp->mCallback(false, this, NULL, NULL, 0, true, entryp->mUserData);
		iter = mLoadedCallbackList.erase(iter);
		delete entryp;
	}
	gTextureList.mCallbackList.erase(this);

	mLoadedCallbackDesiredDiscardLevel = S8_MAX;
	if (needsToSaveRawImage())
	{
		destroySavedRawImage();
	}

	return;
}

void LLViewerFetchedTexture::deleteCallbackEntry(const uuid_list_t* cb_list)
{
	if (mLoadedCallbackList.empty() || !cb_list)
	{
		return;
	}

	S32 desired_discard = S8_MAX;
	S32 desired_raw_discard = INVALID_DISCARD_LEVEL;
	for (callback_list_t::iterator iter = mLoadedCallbackList.begin();
		 iter != mLoadedCallbackList.end(); )
	{
		LLLoadedCallbackEntry *entryp = *iter;
		if (entryp->mSourceCallbackList == cb_list)
		{
			// We never finished loading the image. Indicate failure.
			// Note: this allows mLoadedCallbackUserData to be cleaned up.
			entryp->mCallback(false, this, NULL, NULL, 0, true,
							  entryp->mUserData);
			iter = mLoadedCallbackList.erase(iter);
			delete entryp;
		}
		else
		{
			++iter;

			desired_discard = llmin(desired_discard, entryp->mDesiredDiscard);
			if (entryp->mNeedsImageRaw)
			{
				desired_raw_discard = llmin(desired_raw_discard,
											entryp->mDesiredDiscard);
			}
		}
	}

	mLoadedCallbackDesiredDiscardLevel = desired_discard;
	if (mLoadedCallbackList.empty())
	{
		// If we have no callbacks, take us off of the image callback list.
		gTextureList.mCallbackList.erase(this);

		if (needsToSaveRawImage())
		{
			destroySavedRawImage();
		}
	}
	else if (needsToSaveRawImage() &&
			 mBoostLevel != LLGLTexture::BOOST_PREVIEW)
	{
		if (desired_raw_discard != INVALID_DISCARD_LEVEL)
		{
			mDesiredSavedRawDiscardLevel = desired_raw_discard;
		}
		else
		{
			destroySavedRawImage();
		}
	}
}

void LLViewerFetchedTexture::unpauseLoadedCallbacks(const uuid_list_t* cb_list)
{
	if (!cb_list)
	{
		mPauseLoadedCallBacks = false;
		return;
	}

	bool need_raw = false;
	for (callback_list_t::iterator iter = mLoadedCallbackList.begin(),
								   end = mLoadedCallbackList.end();
		 iter != end; ++iter)
	{
		LLLoadedCallbackEntry* entryp = *iter;
		if (entryp->mSourceCallbackList == cb_list)
		{
			entryp->mPaused = false;
			if (entryp->mNeedsImageRaw)
			{
				need_raw = true;
			}
		}
	}
	mPauseLoadedCallBacks = false;
	mLastCallBackActiveTime = sCurrentTime;
	if (need_raw)
	{
		mSaveRawImage = true;
	}
}

void LLViewerFetchedTexture::pauseLoadedCallbacks(const uuid_list_t* cb_list)
{
	if (!cb_list)
	{
		return;
	}

	bool paused = true;
	for (callback_list_t::iterator iter = mLoadedCallbackList.begin(),
								   end = mLoadedCallbackList.end();
		 iter != end; )
	{
		LLLoadedCallbackEntry* entryp = *iter++;
		if (entryp->mSourceCallbackList == cb_list)
		{
			entryp->mPaused = true;
		}
		else if (!entryp->mPaused)
		{
			paused = false;
		}
	}

	if (paused)
	{
		mPauseLoadedCallBacks = true;	// When set, loaded callback is paused.
		resetTextureStats();
		mSaveRawImage = false;
	}
}

bool LLViewerFetchedTexture::doLoadedCallbacks()
{
	constexpr F32 MAX_INACTIVE_TIME = 180.f;	// In seconds

	if (mNeedsCreateTexture)
	{
		return false;
	}
	if (mPauseLoadedCallBacks)
	{
		destroyRawImage();
		return false;				// Paused
	}

	if (!mIsFetching &&
		sCurrentTime - mLastCallBackActiveTime > MAX_INACTIVE_TIME)
	{
		clearCallbackEntryList();	// Remove all callbacks.
		return false;
	}

	bool res = false;

	if (isMissingAsset())
	{
		for (callback_list_t::iterator iter = mLoadedCallbackList.begin();
			 iter != mLoadedCallbackList.end(); )
		{
			LLLoadedCallbackEntry* entryp = *iter++;
			// We never finished loading the image. Indicate failure.
			// Note: this allows mLoadedCallbackUserData to be cleaned up.
			entryp->mCallback(false, this, NULL, NULL, 0, true,
							  entryp->mUserData);
			delete entryp;
		}
		mLoadedCallbackList.clear();

		// Remove ourself from the global list of textures with callbacks
		gTextureList.mCallbackList.erase(this);
		return false;
	}

	S32 gl_discard = getDiscardLevel();

	// If we do not have a legit GL image, set it to be lower than the worst
	// discard level
	if (gl_discard == -1)
	{
		gl_discard = MAX_DISCARD_LEVEL + 1;
	}

	//
	// Determine the quality levels of textures that we can provide to
	// callbacks and whether we need to do decompression/readback to get it.
	//

	// We can always do a readback to get a raw discard:
	S32 current_raw_discard = MAX_DISCARD_LEVEL + 1;
	// Current GL quality level:
	S32 best_raw_discard = gl_discard;
	S32 current_aux_discard = MAX_DISCARD_LEVEL + 1;
	S32 best_aux_discard = MAX_DISCARD_LEVEL + 1;

	if (mIsRawImageValid)
	{
		// If we have an existing raw image, we have a baseline for the raw and
		// auxiliary quality levels.
		best_raw_discard = llmin(best_raw_discard, mRawDiscardLevel);
		// We always decode the aux when we decode the base raw
		best_aux_discard = llmin(best_aux_discard, mRawDiscardLevel);
		current_aux_discard = llmin(current_aux_discard, best_aux_discard);
	}
	else
	{
		// We have no data at all, we need to get it. Do this by forcing the
		// best aux discard to be 0.
		best_aux_discard = 0;
	}

	// See if any of the callbacks would actually run using the data that we
	// can provide, and also determine if we need to perform any readbacks or
	// decodes.

	bool run_gl_callbacks = false;
	bool run_raw_callbacks = false;
	bool need_readback = false;

	for (callback_list_t::iterator iter = mLoadedCallbackList.begin();
		 iter != mLoadedCallbackList.end(); )
	{
		LLLoadedCallbackEntry* entryp = *iter++;

		if (entryp->mNeedsImageRaw)
		{
			if (mNeedsAux)
			{
				// Need raw and auxiliary channels
				if (entryp->mLastUsedDiscard > current_aux_discard)
				{
					// We have useful data, run the callbacks
					run_raw_callbacks = true;
				}
			}
			else if (entryp->mLastUsedDiscard > current_raw_discard)
			{
				// We have useful data, just run the callbacks
				run_raw_callbacks = true;
			}
			else if (entryp->mLastUsedDiscard > best_raw_discard)
			{
				// We can readback data, and then run the callbacks
				need_readback = true;
				run_raw_callbacks = true;
			}
		}
		// Needs just GL
		else if (entryp->mLastUsedDiscard > gl_discard)
		{
			// We have enough data, run this callback requiring GL data
			run_gl_callbacks = true;
		}
	}

	// Do a readback if required, OR start off a texture decode
	if (need_readback && getMaxDiscardLevel() > gl_discard)
	{
		// Do a readback to get the GL data into the raw image. We have GL
		// data.

		destroyRawImage();
		reloadRawImage(mLoadedCallbackDesiredDiscardLevel);
		if (mRawImage.isNull())
		{
			llwarns << "mRawImage is null. Removing callbacks."
					<< llendl;
			clearCallbackEntryList();
			mNeedsCreateTexture = mIsRawImageValid = false;
			return false;
		}
		if (mNeedsAux && mAuxRawImage.isNull())
		{
			llwarns << "mAuxRawImage is null. Removing callbacks."
					<< llendl;
			clearCallbackEntryList();
			return false;
		}
	}

	// Run raw/auxiliary data callbacks
	if (run_raw_callbacks && mIsRawImageValid &&
		mRawDiscardLevel <= getMaxDiscardLevel())
	{
		// Do callbacks which require raw image data; call each party
		// interested in the raw data.
		for (callback_list_t::iterator iter = mLoadedCallbackList.begin();
			 iter != mLoadedCallbackList.end(); )
		{
			callback_list_t::iterator curiter = iter++;
			LLLoadedCallbackEntry* entryp = *curiter;
			if (entryp->mNeedsImageRaw &&
				entryp->mLastUsedDiscard > mRawDiscardLevel)
			{
				// If we have loaded all the data there is to load or we have
				// loaded enough to satisfy the interested party, then this is
				// the last time that we are going to call them.
				mLastCallBackActiveTime = sCurrentTime;
				if (mNeedsAux && mAuxRawImage.isNull())
				{
					// This is a very common and normal case for baked
					// textures, so let's make it a llinfos instead of a
					// llwarns...
					llinfos << "Raw Image with no Aux Data for callback"
							<< llendl;
				}
				bool is_final = mRawDiscardLevel <= entryp->mDesiredDiscard;
				entryp->mLastUsedDiscard = mRawDiscardLevel;
				entryp->mCallback(true, this, mRawImage, mAuxRawImage,
								  mRawDiscardLevel, is_final,
								  entryp->mUserData);
				if (is_final)
				{
					iter = mLoadedCallbackList.erase(curiter);
					delete entryp;
				}
				res = true;
			}
		}
	}

	// Run GL callbacks
	if (run_gl_callbacks && gl_discard <= getMaxDiscardLevel())
	{
		// Call the callbacks interested in GL data.
		for (callback_list_t::iterator iter = mLoadedCallbackList.begin();
			 iter != mLoadedCallbackList.end(); )
		{
			callback_list_t::iterator curiter = iter++;
			LLLoadedCallbackEntry* entryp = *curiter;
			if (!entryp->mNeedsImageRaw &&
				entryp->mLastUsedDiscard > gl_discard)
			{
				mLastCallBackActiveTime = sCurrentTime;
				bool final = gl_discard <= entryp->mDesiredDiscard;
				entryp->mLastUsedDiscard = gl_discard;
				entryp->mCallback(true, this, NULL, NULL, gl_discard, final,
								  entryp->mUserData);
				if (final)
				{
					iter = mLoadedCallbackList.erase(curiter);
					delete entryp;
				}
				res = true;
			}
		}
	}

	// If we have no callback, take us off of the image callback list.
	if (mLoadedCallbackList.empty())
	{
		gTextureList.mCallbackList.erase(this);
	}

	// Done with any raw image data at this point (will be re-created if we
	// still have callbacks)
	destroyRawImage();

	return res;
}

//virtual
void LLViewerFetchedTexture::forceImmediateUpdate()
{
	// Only immediately update a deleted texture which is now being re-used.
	if (!isDeleted())
	{
		return;
	}
	// If already called forceImmediateUpdate()
	if (mInImageList && mDecodePriority == MAX_DECODE_PRIORITY)
	{
		return;
	}

	gTextureList.forceImmediateUpdate(this);
}

LLImageRaw* LLViewerFetchedTexture::reloadRawImage(S8 discard_level)
{
	llassert_always(mGLTexturep.notNull() && discard_level >= 0 &&
					mComponents > 0);

	if (mRawImage.notNull())
	{
		// mRawImage is in use by somebody else, do not delete it.
		return NULL;
	}

	if (mSavedRawDiscardLevel >= 0 && mSavedRawDiscardLevel <= discard_level)
	{
		if (mSavedRawDiscardLevel != discard_level)
		{
			mRawImage = new LLImageRaw(getWidth(discard_level),
									   getHeight(discard_level),
									   getComponents());
			if (mRawImage)
			{
				mRawImage->copy(getSavedRawImage());
				mRawDiscardLevel = discard_level;
			}
			else
			{
				llwarns << "Cannot create a new raw image (out of memory ?)"
						<< llendl;
				mRawImage = getSavedRawImage();
				mRawDiscardLevel = mSavedRawDiscardLevel;
			}
		}
		else
		{
			mRawImage = getSavedRawImage();
			mRawDiscardLevel = discard_level;
		}
	}
	else if (mCachedRawDiscardLevel >= discard_level)
	{
		mRawImage = mCachedRawImage;
		mRawDiscardLevel = mCachedRawDiscardLevel;
	}
	else // Cached raw image is good enough, copy it.
	{
		mRawImage = new LLImageRaw(getWidth(discard_level),
								   getHeight(discard_level),
								   getComponents());
		if (mRawImage)
		{
			mRawImage->copy(mCachedRawImage);
			mRawDiscardLevel = discard_level;
		}
		else
		{
			llwarns << "Cannot create a new raw image (out of memory ?)"
					<< llendl;
			mRawImage = mCachedRawImage;
			mRawDiscardLevel = mCachedRawDiscardLevel;
		}
	}
	mIsRawImageValid = true;
	++sRawCount;

	return mRawImage;
}

void LLViewerFetchedTexture::destroyRawImage()
{
	if (mAuxRawImage.notNull())
	{
		--sAuxCount;
		mAuxRawImage = NULL;
	}

	if (mRawImage.notNull())
	{
		--sRawCount;

		if (mIsRawImageValid)
		{
			if (needsToSaveRawImage())
			{
				saveRawImage();
			}
			setCachedRawImage();
		}
	}

	mRawImage = NULL;
	mIsRawImageValid = false;
	mRawDiscardLevel = INVALID_DISCARD_LEVEL;
}

// Use the mCachedRawImage to (re)generate the GL texture.
//virtual
void LLViewerFetchedTexture::switchToCachedImage()
{
	if (mCachedRawImage.notNull())
	{
		mRawImage = mCachedRawImage;

		if (getComponents() != mRawImage->getComponents())
		{
			// We have changed the number of components, so we need to move any
			// object using this pool to a different pool.
			mComponents = mRawImage->getComponents();
			mGLTexturep->setComponents(mComponents);
			gTextureList.dirtyImage(this);
		}

		mIsRawImageValid = true;
		mRawDiscardLevel = mCachedRawDiscardLevel;
		gTextureList.mCreateTextureList.insert(this);
		mNeedsCreateTexture = true;
	}
}

// Cache the imageraw forcefully.
//virtual
void LLViewerFetchedTexture::setCachedRawImage(S32 discard_level,
											   LLImageRaw* imageraw)
{
	if (imageraw != mRawImage.get())
	{
		mCachedRawImage = imageraw;
		mCachedRawDiscardLevel = discard_level;
		mCachedRawImageReady = true;
	}
}

void LLViewerFetchedTexture::setCachedRawImage()
{
	if (mRawImage == mCachedRawImage || !mIsRawImageValid ||
		mCachedRawImageReady)
	{
		return;
	}

	if (mCachedRawDiscardLevel < 0 ||
		mCachedRawDiscardLevel > mRawDiscardLevel)
	{
		S32 i = 0;
		S32 w = mRawImage->getWidth();
		S32 h = mRawImage->getHeight();

		S32 max_size = MAX_CACHED_RAW_IMAGE_AREA;
		if (mBoostLevel == LLGLTexture::BOOST_TERRAIN)
		{
			max_size = MAX_CACHED_RAW_TERRAIN_IMAGE_AREA;
		}
		if (mForSculpt)
		{
			max_size = MAX_CACHED_RAW_SCULPT_IMAGE_AREA;
			mCachedRawImageReady = !mRawDiscardLevel;
		}
		else
		{
			mCachedRawImageReady = !mRawDiscardLevel || w * h >= max_size;
		}

		while ((w >> i) * (h >> i) > max_size)
		{
			++i;
		}

		if (i)
		{
			if (!(w >> i) || !(h >> i))
			{
				--i;
			}
			if (mRawImage->getComponents() == 5)
			{
				llwarns << "Trying to scale an image (" << mID
						<< ") with 5 components !" << llendl;
				mIsRawImageValid = false;
				return;
			}
			// Make a duplicate in case somebody else is using this raw image:
			LLPointer<LLImageRaw> dup = mRawImage->scaled(w >> i, h >> i);
			if (dup.notNull())
			{
				mRawImage = std::move(dup);
			}
		}
		mCachedRawImage = mRawImage;
		mRawDiscardLevel += i;
		mCachedRawDiscardLevel = mRawDiscardLevel;
	}
}

void LLViewerFetchedTexture::checkCachedRawSculptImage()
{
	if (mCachedRawImageReady && mCachedRawDiscardLevel > 0)
	{
		if (getDiscardLevel() != 0)
		{
			mCachedRawImageReady = false;
		}
		else if (isForSculptOnly())
		{
			resetTextureStats(); // Do not update this image any more.
		}
	}
}

void LLViewerFetchedTexture::saveRawImage()
{
	if (mRawImage.isNull() || mRawImage == mSavedRawImage ||
		(mSavedRawDiscardLevel >= 0 &&
		 mSavedRawDiscardLevel <= mRawDiscardLevel))
	{
		return;
	}

	// This should not happen, but it did on Snowglobe 1.5. Better safe than
	// sorry...
	if (!mRawImage->getData())
	{
		llwarns << "mRawImage->getData() returns NULL" << llendl;
		return;
	}

	mSavedRawDiscardLevel = mRawDiscardLevel;
	mSavedRawImage = new LLImageRaw(mRawImage->getData(),
									mRawImage->getWidth(),
									mRawImage->getHeight(),
									mRawImage->getComponents());

	if (mForceToSaveRawImage &&
		mSavedRawDiscardLevel <= mDesiredSavedRawDiscardLevel)
	{
		mForceToSaveRawImage = false;
	}

	mLastReferencedSavedRawImageTime = sCurrentTime;
}

void LLViewerFetchedTexture::forceToSaveRawImage(S32 desired_discard, F32 kept_time)
{
	mKeptSavedRawImageTime = kept_time;
	mLastReferencedSavedRawImageTime = sCurrentTime;

	if (mSavedRawDiscardLevel > -1 && mSavedRawDiscardLevel <= desired_discard)
	{
		return; // Raw image is ready.
	}

	if (!mForceToSaveRawImage || mDesiredSavedRawDiscardLevel < 0 ||
		mDesiredSavedRawDiscardLevel > desired_discard)
	{
		mForceToSaveRawImage = true;
		mDesiredSavedRawDiscardLevel = desired_discard;

		// Copy from the cached raw image if exists.
		if (mCachedRawImage.notNull() && mRawImage.isNull() )
		{
			mRawImage = mCachedRawImage;
			mRawDiscardLevel = mCachedRawDiscardLevel;

			saveRawImage();

			mRawImage = NULL;
			mRawDiscardLevel = INVALID_DISCARD_LEVEL;
		}
	}
}

void LLViewerFetchedTexture::destroySavedRawImage()
{
	if (mLastReferencedSavedRawImageTime < mKeptSavedRawImageTime)
	{
		return; // Keep the saved raw image.
	}

	mForceToSaveRawImage = mSaveRawImage = false;

	clearCallbackEntryList();

	mSavedRawImage = NULL;
	mForceToSaveRawImage = mSaveRawImage = false;
	mSavedRawDiscardLevel = mDesiredSavedRawDiscardLevel = -1;
	mLastReferencedSavedRawImageTime = mKeptSavedRawImageTime = 0.f;
}

LLImageRaw* LLViewerFetchedTexture::getSavedRawImage()
{
	mLastReferencedSavedRawImageTime = sCurrentTime;

	return mSavedRawImage;
}

F32 LLViewerFetchedTexture::getElapsedLastReferencedSavedRawImageTime() const
{
	return sCurrentTime - mLastReferencedSavedRawImageTime;
}

// This method is a hack to allow reloading manually staled, blurry (i.e.
// corrupted in cache), or "missing" textures. HB
void LLViewerFetchedTexture::forceRefetch()
{
	// Remove the cache entry
	gTextureCachep->removeFromCache(getID());

	S32 current_discard = getDiscardLevel();
	S32 w = 0, h = 0, c = 0;
	if (current_discard >= 0)
	{
		w = getWidth(0);
		h = getHeight(0);
		c = getComponents();
	}

	if (mHasFetcher)
	{
		gTextureFetchp->deleteRequest(getID());
	}
	cleanup();
	mIsMissingAsset = mWasDeleted = false;
	mDesiredSavedRawDiscardLevel = 0;

	if (mGLTexturep)
	{
		mGLTexturep->forceToInvalidateGLTexture();
	}

	bool success = gTextureFetchp->createRequest(mFTType, mUrl, getID(),
												getTargetHost(),
												MAX_DECODE_PRIORITY, w, h, c,
												mDesiredSavedRawDiscardLevel,
												needsAux(), mCanUseHTTP,
												mSkipCache);
	if (!success)
	{
		return;
	}

	mHasFetcher = mIsFetching = true;
	gTextureList.forceImmediateUpdate(this);
	mRequestedDiscardLevel = mDesiredSavedRawDiscardLevel;

	mFetchState = gTextureFetchp->getFetchState(mID, mDownloadProgress,
												mRequestedDownloadPriority,
												mFetchPriority,
												mFetchDeltaTime,
												mRequestDeltaTime,
												mCanUseHTTP);
}

//-----------------------------------------------------------------------------
// LLViewerLODTexture
//-----------------------------------------------------------------------------

LLViewerLODTexture::LLViewerLODTexture(const LLUUID& id, FTType f_type,
									   const LLHost& host, bool usemipmaps)
:	LLViewerFetchedTexture(id, f_type, host, usemipmaps)
{
	init(true);
}

LLViewerLODTexture::LLViewerLODTexture(const std::string& url, FTType f_type,
									   const LLUUID& id, bool usemipmaps)
:	LLViewerFetchedTexture(url, f_type, id, usemipmaps)
{
	init(true);
}

void LLViewerLODTexture::init(bool firstinit)
{
	mTexelsPerImage = 64 * 64;
	mDiscardVirtualSize = 0.f;
	mCalculatedDiscardLevel = -1.f;
}

//virtual
S8 LLViewerLODTexture::getType() const
{
	return LLViewerTexture::LOD_TEXTURE;
}

// This is guaranteed to get called periodically for every texture
//virtual
void LLViewerLODTexture::processTextureStats()
{
	updateVirtualSize();

	static LLCachedControl<bool> textures_fullres(gSavedSettings,
												  "TextureLoadFullRes");
	static LLCachedControl<U32> min_vsize_discard(gSavedSettings,
												  "TextureMinVirtualSizeDiscard");
	F32 min_virtual_size = llmax((F32)min_vsize_discard, 10.f);
	if (textures_fullres)
	{
		mDesiredDiscardLevel = 0;
	}
	// Generate the request priority and render priority
	else if (mDontDiscard || !mUseMipMaps)
	{
		mDesiredDiscardLevel = 0;
		if (mFullWidth > MAX_IMAGE_SIZE_DEFAULT ||
			mFullHeight > MAX_IMAGE_SIZE_DEFAULT)
		{
			// MAX_IMAGE_SIZE_DEFAULT = 1024 and max size ever is 2048
			mDesiredDiscardLevel = 1;
		}
	}
	else if (!LLPipeline::sRenderDeferred &&
			 mBoostLevel == LLGLTexture::BOOST_ALM)
	{
		mDesiredDiscardLevel = MAX_DISCARD_LEVEL + 1;
	}
	else if (mBoostLevel < LLGLTexture::BOOST_HIGH &&
			 mMaxVirtualSize <= min_virtual_size)
	{
		// If the image has not been significantly visible in a while, we do
		// not want it
		mDesiredDiscardLevel = llmin(mMinDesiredDiscardLevel,
									 (S8)(MAX_DISCARD_LEVEL + 1));
	}
	else if (!mFullWidth || !mFullHeight)
	{
		mDesiredDiscardLevel = getMaxDiscardLevel();
	}
	else
	{
		static const F32 log_4 = logf(4.f);

		F32 discard_level = 0.f;

		// If we know the output width and height, we can force the discard
		// level to the correct value, and thus not decode more texture
		// data than we need to.
		if (mKnownDrawWidth && mKnownDrawHeight)
		{
			F32 draw_texels = (F32)llclamp(mKnownDrawWidth * mKnownDrawHeight,
										   MIN_IMAGE_AREA, MAX_IMAGE_AREA);

			// Use log_4 because we are in square-pixel space, so an image with
			// twice the width and twice the height will have mTexelsPerImage =
			// 4 * draw_size
			discard_level = logf(F32(mTexelsPerImage) / draw_texels) / log_4;
		}
		else
		{
			if (isLargeImage() && !isJustBound() &&
				mAdditionalDecodePriority < 0.3f)
			{
				// If it is a big image and not being used recently, nor close
				// to the view point, do not load hi-res data.
				mMaxVirtualSize =
					llmin(mMaxVirtualSize,
						  (F32)LLViewerTexture::sMinLargeImageSize);
			}

			if (mCalculatedDiscardLevel >= 0.f &&
				fabsf(mMaxVirtualSize - mDiscardVirtualSize) <
					mMaxVirtualSize * 0.2f)
			{
				// < 20% change in virtual size = no change in desired discard
				discard_level = mCalculatedDiscardLevel;
			}
			else
			{
				// Calculate the required scale factor of the image using pixels
				// per texel
				discard_level = logf(mTexelsPerImage / mMaxVirtualSize) /
								log_4;
				mDiscardVirtualSize = mMaxVirtualSize;
				mCalculatedDiscardLevel = discard_level;
			}
		}
		if (mBoostLevel < LLGLTexture::BOOST_SCULPTED)
		{
			discard_level += sDesiredDiscardBias;
			discard_level *= sDesiredDiscardScale; // scale
			discard_level += sCameraMovingDiscardBias;
		}
		discard_level = floorf(discard_level);

		F32 min_discard = 0.f;
		if (mFullWidth > MAX_IMAGE_SIZE_DEFAULT ||
			mFullHeight > MAX_IMAGE_SIZE_DEFAULT)
		{
			// MAX_IMAGE_SIZE_DEFAULT = 1024 and max size ever is 2048
			min_discard = 1.f;
		}

		discard_level = llclamp(discard_level, min_discard,
								(F32)MAX_DISCARD_LEVEL);

		// Cannot go higher than the max discard level
		mDesiredDiscardLevel = llmin(getMaxDiscardLevel() + 1,
									 (S32)discard_level);
		// Clamp to min desired discard
		mDesiredDiscardLevel = llmin(mMinDesiredDiscardLevel,
									 mDesiredDiscardLevel);

		// At this point we have calculated the quality level that we want, if
		// possible. Now we check to see if we have it and take the proper
		// action if we do not.

		S32 current_discard = getDiscardLevel();
		if (sDesiredDiscardBias > 0.f && current_discard >= 0 &&
			mBoostLevel < LLGLTexture::BOOST_SCULPTED)
		{
			if (!mForceToSaveRawImage &&
				desired_discard_bias_max <= sDesiredDiscardBias)
			{
				// We need to release texture memory urgently
				scaleDown();
			}
			else if (BYTES2MEGABYTES(sBoundTextureMemoryInBytes) >
						sMaxBoundTextureMemInMegaBytes * texmem_middle_bound_scale
					 && (!getBoundRecently() ||
						 mDesiredDiscardLevel >= mCachedRawDiscardLevel))
			{
				// Limit the amount of GL memory bound each frame
				scaleDown();
			}
			else if (BYTES2MEGABYTES(sTotalTextureMemoryInBytes) >
						sMaxTotalTextureMemInMegaBytes * texmem_middle_bound_scale
					 && (!getBoundRecently() ||
						 mDesiredDiscardLevel >= mCachedRawDiscardLevel))
			{
				// Only allow GL to have 2x the video card memory
				scaleDown();
			}
		}
	}

	if (mForceToSaveRawImage && mDesiredSavedRawDiscardLevel >= 0)
	{
		mDesiredDiscardLevel = llmin(mDesiredDiscardLevel,
									 (S8)mDesiredSavedRawDiscardLevel);
	}
	else if (LLPipeline::sMemAllocationThrottled)
	{
		// Release memory of large textures by decrease their resolutions.
		if (scaleDown())
		{
			mDesiredDiscardLevel = mCachedRawDiscardLevel;
		}
	}
}

bool LLViewerLODTexture::scaleDown()
{
	if (hasGLTexture() && mCachedRawDiscardLevel > getDiscardLevel())
	{
		switchToCachedImage();
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// LLViewerMediaTexture
//-----------------------------------------------------------------------------

//static
void LLViewerMediaTexture::updateClass()
{
	static LLCachedControl<U32> lazy_flush_timeout(gSavedSettings,
												   "TextureLazyFlushTimeout");
	F32 max_inactive_time = llmax((F32)(lazy_flush_timeout / 2), 5.f);

#if 0
	// force to play media.
	gSavedSettings.setBool("EnableStreamingMedia", true);
#endif

	for (media_map_t::iterator iter = sMediaMap.begin(), end = sMediaMap.end();
		 iter != end; )
	{
		media_map_t::iterator cur = iter++;
		LLViewerMediaTexture* mediap = cur->second.get();
		if (mediap && mediap->getNumRefs() == 1) // one reference by sMediaMap
		{
			// Delay some time to delete the media textures to stop endlessly
			// creating and immediately removing media texture.
			if (mediap->getElapsedLastReferenceTime() > max_inactive_time)
			{
				sMediaMap.erase(cur);
			}
		}
	}
}

//static
void LLViewerMediaTexture::removeMediaImplFromTexture(const LLUUID& media_id)
{
	LLViewerMediaTexture* media_tex = findMediaTexture(media_id);
	if (media_tex)
	{
		media_tex->invalidateMediaImpl();
	}
}

//static
void LLViewerMediaTexture::cleanUpClass()
{
	sMediaMap.clear();
}

//static
LLViewerMediaTexture* LLViewerMediaTexture::findMediaTexture(const LLUUID& media_id)
{
	media_map_t::iterator iter = sMediaMap.find(media_id);
	if (iter == sMediaMap.end())
	{
		return NULL;
	}

	LLViewerMediaTexture* media_tex = iter->second.get();
	if (media_tex)
	{
		media_tex->setMediaImpl();
		media_tex->resetLastReferencedTime();
	}

	return media_tex;
}

LLViewerMediaTexture::LLViewerMediaTexture(const LLUUID& id, bool usemipmaps,
										   LLImageGL* gl_image)
:	LLViewerTexture(id, usemipmaps),
	mMediaImplp(NULL),
	mUpdateVirtualSizeTime(0)
{
	sMediaMap[id] = this;

	mGLTexturep = gl_image;
	if (mGLTexturep.isNull())
	{
		generateGLTexture();
	}
	mGLTexturep->setAllowCompression(false);
	mGLTexturep->setNeedsAlphaAndPickMask(false);

	mIsPlaying = false;

	setMediaImpl();

	setCategory(LLGLTexture::MEDIA);

	LLViewerTexture* tex = gTextureList.findImage(mID);
	if (tex)
	{
		// This media is a parcel media for tex.
		tex->setParcelMedia(this);
	}
}

//virtual
LLViewerMediaTexture::~LLViewerMediaTexture()
{
	LLViewerTexture* tex = gTextureList.findImage(mID);
	if (tex)
	{
		// This media is a parcel media for tex.
		tex->setParcelMedia(NULL);
	}
}

void LLViewerMediaTexture::reinit(bool usemipmaps)
{
	mUseMipMaps = usemipmaps;
	resetLastReferencedTime();
	if (mGLTexturep.notNull())
	{
		mGLTexturep->setUseMipMaps(usemipmaps);
		mGLTexturep->setNeedsAlphaAndPickMask(false);
	}
}

void LLViewerMediaTexture::setUseMipMaps(bool mipmap)
{
	mUseMipMaps = mipmap;

	if (mGLTexturep.notNull())
	{
		mGLTexturep->setUseMipMaps(mipmap);
	}
}

//virtual
S8 LLViewerMediaTexture::getType() const
{
	return LLViewerTexture::MEDIA_TEXTURE;
}

void LLViewerMediaTexture::invalidateMediaImpl()
{
	mMediaImplp = NULL;
}

void LLViewerMediaTexture::setMediaImpl()
{
	if (!mMediaImplp)
	{
		mMediaImplp = LLViewerMedia::getMediaImplFromTextureID(mID);
	}
}

// Return true if all faces to reference to this media texture are found
// Note: mMediaFaceList is valid only for the current instant because it does
//       not check the face validity after the current frame.
bool LLViewerMediaTexture::findFaces()
{
	mMediaFaceList.clear();

	bool ret = true;

	LLViewerTexture* tex = gTextureList.findImage(mID);
	if (tex)
	{
		// This media is a parcel media for tex.
		for (U32 ch = 0; ch < LLRender::NUM_TEXTURE_CHANNELS; ++ch)
		{
			const ll_face_list_t* face_list = tex->getFaceList(ch);
			U32 end = tex->getNumFaces(ch);
			for (U32 i = 0; i < end; ++i)
			{
				LLFace* facep = (*face_list)[i];
				if (facep->isMediaAllowed())
				{
					mMediaFaceList.push_back((*face_list)[i]);
				}
			}
		}
	}

	if (!mMediaImplp)
	{
		return true;
	}

	// For media on a face.
	const std::list<LLVOVolume*>* obj_list = mMediaImplp->getObjectList();
	for (std::list<LLVOVolume*>::const_iterator iter = obj_list->begin(),
												end = obj_list->end();
		 iter != end; ++iter)
	{
		LLVOVolume* obj = *iter;
		if (obj->mDrawable.isNull())
		{
			ret = false;
			continue;
		}

		S32 face_id = -1;
		S32 num_faces = obj->mDrawable->getNumFaces();
		while ((face_id = obj->getFaceIndexWithMediaImpl(mMediaImplp,
														 face_id)) > -1 &&
			   face_id < num_faces)
		{
			LLFace* facep = obj->mDrawable->getFace(face_id);
			if (facep)
			{
				mMediaFaceList.push_back(facep);
			}
			else
			{
				ret = false;
			}
		}
	}

	return ret;
}

void LLViewerMediaTexture::initVirtualSize()
{
	if (mIsPlaying)
	{
		return;
	}

	findFaces();
	for (std::list<LLFace*>::iterator iter = mMediaFaceList.begin(),
									  end = mMediaFaceList.end();
		 iter != end; ++iter)
	{
		addTextureStats((*iter)->getVirtualSize());
	}
}

void LLViewerMediaTexture::addMediaToFace(LLFace* facep)
{
	if (facep)
	{
		facep->setHasMedia(true);
	}
	if (!mIsPlaying)
	{
		// No need to remove the face because the media is not playing.
		return;
	}

	switchTexture(LLRender::DIFFUSE_MAP, facep);
}

void LLViewerMediaTexture::removeMediaFromFace(LLFace* facep)
{
	if (!facep)
	{
		return;
	}
	facep->setHasMedia(false);

	if (!mIsPlaying)
	{
		// No need to remove the face because the media is not playing.
		return;
	}

	mIsPlaying = false;			// Set to remove the media from the face.
	switchTexture(LLRender::DIFFUSE_MAP, facep);
	mIsPlaying = true;			// Set the flag back.

	if (getTotalNumFaces() < 1)	// No face referencing to this media
	{
		stopPlaying();
	}
}

//virtual
void LLViewerMediaTexture::addFace(U32 ch, LLFace* facep)
{
	LLViewerTexture::addFace(ch, facep);

	const LLTextureEntry* te = facep->getTextureEntry();
	if (te && te->getID().notNull())
	{
		LLViewerTexture* tex = gTextureList.findImage(te->getID());
		if (tex)
		{
			// Increase the reference number by one for tex to avoid deleting
			// it.
			mTextureList.push_back(tex);
			return;
		}
	}

	// Check if it is a parcel media
	if (facep->getTexture() && facep->getTexture() != this &&
		facep->getTexture()->getID() == mID)
	{
		mTextureList.push_back(facep->getTexture()); // a parcel media.
		return;
	}

	if (te && te->getID().notNull()) // should have a texture
	{
		llerrs << "The face does not have a valid texture before media texture."
			   << llendl;
	}
}

//virtual
void LLViewerMediaTexture::removeFace(U32 ch, LLFace* facep)
{
	LLViewerTexture::removeFace(ch, facep);

	const LLTextureEntry* te = facep->getTextureEntry();
	if (te && te->getID().notNull())
	{
		LLViewerTexture* tex = gTextureList.findImage(te->getID());
		if (tex)
		{
			for (std::list<LLPointer<LLViewerTexture> >::iterator
					iter = mTextureList.begin();
				 iter != mTextureList.end(); ++iter)
			{
				if (*iter == tex)
				{
					// decrease the reference number for tex by one.
					mTextureList.erase(iter);
					return;
				}
			}

			// We have some trouble here: the texture of the face is changed.
			// We need to find the former texture, and remove it from the list
			// to avoid memory leaking.
			std::vector<const LLTextureEntry*> te_list;
			for (U32 ch = 0; ch < 3; ++ch)
			{
				U32 count = mNumFaces[ch];
				U32 list_size = mFaceList[ch].size();
				if (count > list_size)
				{
					llwarns_once << "Face count greater than face list size for texture channel: "
								 << ch << ". Clamping down." << llendl;
					count = list_size;
				}

				for (U32 j = 0; j < count; ++j)
				{
					// All textures are in use.
					te_list.push_back(mFaceList[ch][j]->getTextureEntry());
				}
			}

			if (te_list.empty())
			{
				mTextureList.clear();
				return;
			}
			S32 end = te_list.size();

			for (std::list< LLPointer<LLViewerTexture> >::iterator
					iter = mTextureList.begin();
				 iter != mTextureList.end(); ++iter)
			{
				S32 i = 0;
				for (i = 0; i < end; ++i)
				{
					if (te_list[i] && te_list[i]->getID() == (*iter)->getID())
					{
						// the texture is in use.
						te_list[i] = NULL;
						break;
					}
				}
				if (i == end) // No hit for this texture, remove it.
				{
					// Decrease the reference number for tex by one.
					mTextureList.erase(iter);
					return;
				}
			}
		}
	}

	// Check if it is a parcel media
	for (std::list< LLPointer<LLViewerTexture> >::iterator
			iter = mTextureList.begin();
		 iter != mTextureList.end(); ++iter)
	{
		if ((*iter)->getID() == mID)
		{
			// Decrease the reference number for tex by one.
			mTextureList.erase(iter);
			return;
		}
	}

	if (te && te->getID().notNull()) // Should have a texture
	{
		llwarns_once << "mTextureList texture reference number is corrupted !"
				  	 << llendl;
		llassert(false);
	}
}

void LLViewerMediaTexture::stopPlaying()
{
#if 0	// Do not stop the media impl playing here: this breaks non-inworld
		// media (login screen, search, and media browser).
	if (mMediaImplp)
	{
		mMediaImplp->stop();
	}
#endif
	mIsPlaying = false;
}

void LLViewerMediaTexture::switchTexture(U32 ch, LLFace* facep)
{
	if (!facep) return;

	// Check if another media is playing on this face and if this is a parcel
	// media, let the prim media win.
	LLViewerTexture* tex = facep->getTexture();
	if (tex && tex != this &&
		tex->getType() == LLViewerTexture::MEDIA_TEXTURE &&
		tex->getID() == mID)
	{
		return;
	}

	if (mIsPlaying)
	{
		// Old textures switch to the media texture
		facep->switchTexture(ch, this);
	}
	else
	{
		// Switch to old textures.
		const LLTextureEntry* te = facep->getTextureEntry();
		if (te)
		{
			LLViewerTexture* tex = NULL;
			if (te->getID().notNull())
			{
				tex = gTextureList.findImage(te->getID());
			}
			if (!tex && te->getID() != mID) // try parcel media.
			{
				tex = gTextureList.findImage(mID);
			}
			if (!tex)
			{
				tex = LLViewerFetchedTexture::sDefaultImagep;
			}
			facep->switchTexture(ch, tex);
		}
	}
}

void LLViewerMediaTexture::setPlaying(bool playing)
{
	if (!mMediaImplp)
	{
		return;
	}
	if (!playing && !mIsPlaying)
	{
		return; // Media is already off
	}

	if (playing == mIsPlaying && !mMediaImplp->isUpdated())
	{
		return; // Nothing has changed since last time.
	}

	mIsPlaying = playing;
	if (mIsPlaying)					// We are about to play this media
	{
		if (findFaces())
		{
			// About to update all faces.
			mMediaImplp->setUpdated(false);
		}

		if (mMediaFaceList.empty()) // No face pointing to this media
		{
			stopPlaying();
			return;
		}

		for (std::list<LLFace*>::iterator iter = mMediaFaceList.begin();
			 iter!= mMediaFaceList.end(); ++iter)
		{
			switchTexture(LLRender::DIFFUSE_MAP, *iter);
		}
	}
	else							// Stop playing this media
	{
		U32 ch = LLRender::DIFFUSE_MAP;
		U32 count = mNumFaces[ch];
		U32 list_size = mFaceList[ch].size();
		if (count > list_size)
		{
			llwarns_once << "Face count greater than face list size for texture channel: "
						 << ch << ". Clamping down." << llendl;
			count = list_size;
		}
		for (U32 i = count; i; --i)
		{
			// Current face could be removed in this function.
			switchTexture(ch, mFaceList[ch][i - 1]);
		}
	}
}

//virtual
F32 LLViewerMediaTexture::getMaxVirtualSize()
{
	if (LLFrameTimer::getFrameCount() == mUpdateVirtualSizeTime)
	{
		return mMaxVirtualSize;
	}
	mUpdateVirtualSizeTime = LLFrameTimer::getFrameCount();

	if (!mMaxVirtualSizeResetCounter)
	{
		addTextureStats(0.f, false); // Reset
	}

	if (mIsPlaying)		// Media is playing
	{
		for (U32 ch = 0; ch < LLRender::NUM_TEXTURE_CHANNELS; ++ch)
		{
			U32 count = mNumFaces[ch];
			U32 list_size = mFaceList[ch].size();
			if (count > list_size)
			{
				llwarns_once << "Face count greater than face list size for texture channel: "
							 << ch << ". Clamping down." << llendl;
				count = list_size;
			}
			for (U32 i = 0 ; i < count ; ++i)
			{
				LLFace* facep = mFaceList[ch][i];
				if (facep && facep->getDrawable()->isRecentlyVisible())
				{
					addTextureStats(facep->getVirtualSize());
				}
			}
		}
	}
	else				// Media is not in playing
	{
		findFaces();

		if (!mMediaFaceList.empty())
		{
			for (std::list<LLFace*>::iterator iter = mMediaFaceList.begin(),
											  end = mMediaFaceList.end();
				 iter!= end; ++iter)
			{
				LLFace* facep = *iter;
				if (facep && facep->getDrawable() &&
					facep->getDrawable()->isRecentlyVisible())
				{
					addTextureStats(facep->getVirtualSize());
				}
			}
		}
	}

	if (mMaxVirtualSizeResetCounter > 0)
	{
		--mMaxVirtualSizeResetCounter;
	}

	reorganizeFaceList();
	reorganizeVolumeList();

	return mMaxVirtualSize;
}

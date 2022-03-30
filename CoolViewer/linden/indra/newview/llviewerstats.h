/**
 * @file llviewerstats.h
 * @brief LLViewerStats class header file
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

#ifndef LL_LLVIEWERSTATS_H
#define LL_LLVIEWERSTATS_H

#include "llsingleton.h"
#include "llstat.h"

class LLSD;

enum
{
	LL_SIM_STAT_TIME_DILATION			=  0,
	LL_SIM_STAT_FPS						=  1,
	LL_SIM_STAT_PHYSFPS					=  2,
	LL_SIM_STAT_AGENTUPS				=  3,
	LL_SIM_STAT_FRAMEMS					=  4,
	LL_SIM_STAT_NETMS					=  5,
	LL_SIM_STAT_SIMOTHERMS				=  6,
	LL_SIM_STAT_SIMPHYSICSMS			=  7,
	LL_SIM_STAT_AGENTMS					=  8,
	LL_SIM_STAT_IMAGESMS				=  9,
	LL_SIM_STAT_SCRIPTMS				= 10,
	LL_SIM_STAT_NUMTASKS				= 11,
	LL_SIM_STAT_NUMTASKSACTIVE			= 12,
	LL_SIM_STAT_NUMAGENTMAIN			= 13,
	LL_SIM_STAT_NUMAGENTCHILD			= 14,
	LL_SIM_STAT_NUMSCRIPTSACTIVE		= 15,
	LL_SIM_STAT_LSLIPS					= 16,
	LL_SIM_STAT_INPPS					= 17,
	LL_SIM_STAT_OUTPPS					= 18,
	LL_SIM_STAT_PENDING_DOWNLOADS		= 19,
	LL_SIM_STAT_PENDING_UPLOADS			= 20,
	LL_SIM_STAT_VIRTUAL_SIZE_KB			= 21,
	LL_SIM_STAT_RESIDENT_SIZE_KB		= 22,
	LL_SIM_STAT_PENDING_LOCAL_UPLOADS	= 23,
	LL_SIM_STAT_TOTAL_UNACKED_BYTES		= 24,
	LL_SIM_STAT_PHYSICS_PINNED_TASKS	= 25,
	LL_SIM_STAT_PHYSICS_LOD_TASKS		= 26,
	LL_SIM_STAT_SIMPHYSICSSTEPMS		= 27,
	LL_SIM_STAT_SIMPHYSICSSHAPEMS		= 28,
	LL_SIM_STAT_SIMPHYSICSOTHERMS		= 29,
	LL_SIM_STAT_SIMPHYSICSMEMORY		= 30,
	LL_SIM_STAT_SCRIPT_EPS				= 31,
	LL_SIM_STAT_SIMSPARETIME			= 32,
	LL_SIM_STAT_SIMSLEEPTIME			= 33,
	LL_SIM_STAT_IOPUMPTIME				= 34,
	LL_SIM_STAT_PCTSCRIPTSRUN			= 35,
	LL_SIM_STAT_REGION_IDLE				= 36, // dataserver only
	LL_SIM_STAT_REGION_IDLE_POSSIBLE	= 37, // dataserver only
	LL_SIM_STAT_SIMAISTEPTIMEMS			= 38,
	LL_SIM_STAT_SKIPPEDAISILSTEPS_PS	= 39,
	LL_SIM_STAT_PCTSTEPPEDCHARACTERS	= 40
};

class LLViewerStats : public LLSingleton<LLViewerStats>
{
	friend class LLSingleton<LLViewerStats>;

protected:
	LOG_CLASS(LLViewerStats);

public:
	// If you change this, please also add a corresponding text label
	// in statTypeToText in llviewerstats.cpp
	enum EStatType
	{
		ST_VERSION = 0,
		ST_AVATAR_EDIT_SECONDS = 1,
		ST_TOOLBOX_SECONDS = 2,
		ST_CHAT_COUNT = 3,
		ST_IM_COUNT = 4,
		ST_FULLSCREEN_BOOL = 5,
		ST_RELEASE_COUNT= 6,
		ST_CREATE_COUNT = 7,
		ST_REZ_COUNT = 8,
		ST_FPS_10_SECONDS = 9,
		ST_FPS_2_SECONDS = 10,
		ST_MOUSELOOK_SECONDS = 11,
		ST_FLY_COUNT = 12,
		ST_TELEPORT_COUNT = 13,
		ST_OBJECT_DELETE_COUNT = 14,
		ST_SNAPSHOT_COUNT = 15,
		ST_UPLOAD_SOUND_COUNT = 16,
		ST_UPLOAD_TEXTURE_COUNT = 17,
		ST_EDIT_TEXTURE_COUNT = 18,
		ST_KILLED_COUNT = 19,
		ST_FRAMETIME_JITTER = 20,
		ST_FRAMETIME_SLEW = 21,
		ST_INVENTORY_TOO_LONG = 22,
		ST_WEARABLES_TOO_LONG = 23,
		ST_LOGIN_SECONDS = 24,
		ST_LOGIN_TIMEOUT_COUNT = 25,
		ST_HAS_BAD_TIMER = 26,
		ST_DOWNLOAD_FAILED = 27,
		ST_LSL_SAVE_COUNT = 28,
		ST_UPLOAD_ANIM_COUNT = 29,
		ST_FPS_8_SECONDS = 30,
		ST_SIM_FPS_20_SECONDS = 31,
		ST_PHYS_FPS_20_SECONDS = 32,
		ST_LOSS_05_SECONDS = 33,
		ST_FPS_DROP_50_RATIO = 34,
		ST_ENABLE_VBO = 35,
		ST_DELTA_BANDWIDTH = 36,
		ST_MAX_BANDWIDTH = 37,
		ST_LIGHTING_DETAIL = 38,
		ST_VISIBLE_AVATARS = 39,
		ST_SHADER_OBJECTS = 40,
		ST_SHADER_ENVIRONMENT = 41,
		ST_DRAW_DIST = 42,
		ST_CHAT_BUBBLES = 43,
		ST_SHADER_AVATAR = 44,
		ST_FRAME_SECS = 45,
		ST_UPDATE_SECS = 46,
		ST_NETWORK_SECS = 47,
		ST_IMAGE_SECS = 48,
		ST_REBUILD_SECS = 49,
		ST_RENDER_SECS = 50,
		ST_CROSSING_AVG = 51,
		ST_CROSSING_MAX = 52,
		ST_LIBXUL_WIDGET_USED = 53,	// Unused
		ST_WINDOW_WIDTH = 54,
		ST_WINDOW_HEIGHT = 55,
		ST_TEX_BAKES = 56,
		ST_TEX_REBAKES = 57,
		ST_LOGITECH_LCD = 58,		// Deprecated/never actually used

		ST_COUNT = 59
	};

	LLViewerStats();

	// Returns the latest value of given stat
	LL_INLINE F64 getStat(EStatType type) const		{ return mStats[type]; }

	// Sets the stat to value
	LL_INLINE F64 setStat(EStatType type, F64 value)
	{
		mStats[type] = value;
		return mStats[type];
	}

	// Adds value to the stat
	LL_INLINE F64 incStat(EStatType type, F64 value = 1.f)
	{
		mStats[type] += value;
		return mStats[type];
	}

	void addSample(U32 stat_id, F32 stat_value);

	void updateFrameStats(F64 time_diff);

	void addToMessage(LLSD& body) const;

	void resetStats();

	void idleUpdate();

	void updateStatistics(U32 frame_count);
	void sendStats();

	void resetAvatarStats();	// Used after TPs, in llagent.cpp

	LL_INLINE static U32 getComplexityReports()		{ return sComplexityReports; }
	LL_INLINE static U32 getTooComplexReports()		{ return sTooComplexReports; }

private:
	void avatarRenderingStats();

	static void getAvatarRenderStatsCoro(const std::string& url,
										 std::string region_name);
	static void sendAvatarRenderStatsCoro(const std::string& url,
										  std::string region_name,
										  const LLSD& data);

public:
	LLStat mKBitStat;
	LLStat mLayersKBitStat;
	LLStat mObjectKBitStat;
	LLStat mAssetKBitStat;
	LLStat mTextureKBitStat;
	LLStat mObjectsDrawnStat;
	LLStat mObjectsCulledStat;
	LLStat mObjectsTestedStat;
	LLStat mObjectsComparedStat;
	LLStat mObjectsOccludedStat;
	LLStat mFPSStat;
	LLStat mPacketsInStat;
	LLStat mPacketsLostStat;
	LLStat mPacketsOutStat;
	LLStat mPacketsLostPercentStat;
	LLStat mTexturePacketsStat;
	LLStat mActualInKBitStat;	// From the packet ring (when faking a bad connection)
	LLStat mActualOutKBitStat;	// From the packet ring (when faking a bad connection)

	// Simulator stats
	LLStat mSimTimeDilation;

	LLStat mSimFPS;
	LLStat mSimPhysicsFPS;
	LLStat mSimAgentUPS;
	LLStat mSimScriptEPS;

	LLStat mSimFrameMsec;
	LLStat mSimNetMsec;
	LLStat mSimSimOtherMsec;
	LLStat mSimSimPhysicsMsec;

	LLStat mSimSimPhysicsStepMsec;
	LLStat mSimSimPhysicsShapeUpdateMsec;
	LLStat mSimSimPhysicsOtherMsec;

	LLStat mSimSimAIStepMsec;
	LLStat mSimSimSkippedSilhouetteSteps;
	LLStat mSimSimPctSteppedCharacters;

	LLStat mSimAgentMsec;
	LLStat mSimImagesMsec;
	LLStat mSimScriptMsec;
	LLStat mSimSpareMsec;
	LLStat mSimSleepMsec;
	LLStat mSimPumpIOMsec;

	LLStat mSimMainAgents;
	LLStat mSimChildAgents;
	LLStat mSimObjects;
	LLStat mSimActiveObjects;
	LLStat mSimActiveScripts;
	LLStat mSimPctScriptsRun;

	LLStat mSimInPPS;
	LLStat mSimOutPPS;
	LLStat mSimPendingDownloads;
	LLStat mSimPendingUploads;
	LLStat mSimPendingLocalUploads;
	LLStat mSimTotalUnackedBytes;

	LLStat mPhysicsPinnedTasks;
	LLStat mPhysicsLODTasks;
	LLStat mPhysicsMemoryAllocated;
	/*
	LLStat mSimCPUUsageStat;
	LLStat mSimMemTotalStat;
	LLStat mSimMemRSSStat;
	*/

	LLStat mSimPingStat;

	LLStat mMemoryUsageStat;

private:
	F64			mStats[ST_COUNT];

	F64			mLastTimeDiff;				// used for time stat updates

	F32			mNextStatsSendingTime;
	F32			mNextAvStatsSendingTime;

	static U32	sComplexityReports;
	static U32	sTooComplexReports;
};

// Used by llviewermenu.cpp as a menu callback. Not strictly related to
// LLViewerStats, so kept as a separate function.
void output_statistics(void*);

extern LLFrameTimer gTextureTimer;
extern U32 gObjectBits;

#endif // LL_LLVIEWERSTATS_H

/** 
 * @file llagentconstants.h
 * @author James Cook, Andrew Meadows, Richard Nelson
 * @brief Shared constants through the system for agents.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LL_LLAGENTCONSTANTS_H
#define LL_LLAGENTCONSTANTS_H

#include "stdtypes.h"

constexpr U32 CONTROL_AT_POS_INDEX			= 0;
constexpr U32 CONTROL_AT_NEG_INDEX			= 1;
constexpr U32 CONTROL_LEFT_POS_INDEX		= 2;
constexpr U32 CONTROL_LEFT_NEG_INDEX		= 3;
constexpr U32 CONTROL_UP_POS_INDEX			= 4;
constexpr U32 CONTROL_UP_NEG_INDEX			= 5;
constexpr U32 CONTROL_PITCH_POS_INDEX		= 6;
constexpr U32 CONTROL_PITCH_NEG_INDEX		= 7;
constexpr U32 CONTROL_YAW_POS_INDEX			= 8;
constexpr U32 CONTROL_YAW_NEG_INDEX			= 9;
constexpr U32 CONTROL_FAST_AT_INDEX			= 10;
constexpr U32 CONTROL_FAST_LEFT_INDEX		= 11;
constexpr U32 CONTROL_FAST_UP_INDEX			= 12;
constexpr U32 CONTROL_FLY_INDEX				= 13;
constexpr U32 CONTROL_STOP_INDEX			= 14;
constexpr U32 CONTROL_FINISH_ANIM_INDEX		= 15;
constexpr U32 CONTROL_STAND_UP_INDEX		= 16;
constexpr U32 CONTROL_SIT_ON_GROUND_INDEX	= 17;
constexpr U32 CONTROL_MOUSELOOK_INDEX		= 18;
constexpr U32 CONTROL_NUDGE_AT_POS_INDEX	= 19;
constexpr U32 CONTROL_NUDGE_AT_NEG_INDEX	= 20;
constexpr U32 CONTROL_NUDGE_LEFT_POS_INDEX	= 21;
constexpr U32 CONTROL_NUDGE_LEFT_NEG_INDEX	= 22;
constexpr U32 CONTROL_NUDGE_UP_POS_INDEX	= 23;
constexpr U32 CONTROL_NUDGE_UP_NEG_INDEX	= 24;
constexpr U32 CONTROL_TURN_LEFT_INDEX		= 25;
constexpr U32 CONTROL_TURN_RIGHT_INDEX		= 26;
constexpr U32 CONTROL_AWAY_INDEX			= 27;
constexpr U32 CONTROL_LBUTTON_DOWN_INDEX	= 28;
constexpr U32 CONTROL_LBUTTON_UP_INDEX		= 29;
constexpr U32 CONTROL_ML_LBUTTON_DOWN_INDEX	= 30;
constexpr U32 CONTROL_ML_LBUTTON_UP_INDEX	= 31;
constexpr U32 TOTAL_CONTROLS				= 32;

constexpr U32 AGENT_CONTROL_AT_POS			= 0x1 << CONTROL_AT_POS_INDEX;			// 0x00000001
constexpr U32 AGENT_CONTROL_AT_NEG			= 0x1 << CONTROL_AT_NEG_INDEX;			// 0x00000002
constexpr U32 AGENT_CONTROL_LEFT_POS		= 0x1 << CONTROL_LEFT_POS_INDEX;		// 0x00000004
constexpr U32 AGENT_CONTROL_LEFT_NEG		= 0x1 << CONTROL_LEFT_NEG_INDEX;		// 0x00000008
constexpr U32 AGENT_CONTROL_UP_POS			= 0x1 << CONTROL_UP_POS_INDEX;			// 0x00000010
constexpr U32 AGENT_CONTROL_UP_NEG			= 0x1 << CONTROL_UP_NEG_INDEX;			// 0x00000020
constexpr U32 AGENT_CONTROL_PITCH_POS		= 0x1 << CONTROL_PITCH_POS_INDEX;		// 0x00000040
constexpr U32 AGENT_CONTROL_PITCH_NEG		= 0x1 << CONTROL_PITCH_NEG_INDEX;		// 0x00000080
constexpr U32 AGENT_CONTROL_YAW_POS			= 0x1 << CONTROL_YAW_POS_INDEX;			// 0x00000100
constexpr U32 AGENT_CONTROL_YAW_NEG			= 0x1 << CONTROL_YAW_NEG_INDEX;			// 0x00000200

constexpr U32 AGENT_CONTROL_FAST_AT			= 0x1 << CONTROL_FAST_AT_INDEX;			// 0x00000400
constexpr U32 AGENT_CONTROL_FAST_LEFT		= 0x1 << CONTROL_FAST_LEFT_INDEX;		// 0x00000800
constexpr U32 AGENT_CONTROL_FAST_UP			= 0x1 << CONTROL_FAST_UP_INDEX;			// 0x00001000

constexpr U32 AGENT_CONTROL_FLY				= 0x1 << CONTROL_FLY_INDEX;				// 0x00002000
constexpr U32 AGENT_CONTROL_STOP			= 0x1 << CONTROL_STOP_INDEX;			// 0x00004000
constexpr U32 AGENT_CONTROL_FINISH_ANIM		= 0x1 << CONTROL_FINISH_ANIM_INDEX;		// 0x00008000
constexpr U32 AGENT_CONTROL_STAND_UP		= 0x1 << CONTROL_STAND_UP_INDEX;		// 0x00010000
constexpr U32 AGENT_CONTROL_SIT_ON_GROUND	= 0x1 << CONTROL_SIT_ON_GROUND_INDEX;	// 0x00020000
constexpr U32 AGENT_CONTROL_MOUSELOOK		= 0x1 << CONTROL_MOUSELOOK_INDEX;		// 0x00040000

constexpr U32 AGENT_CONTROL_NUDGE_AT_POS	= 0x1 << CONTROL_NUDGE_AT_POS_INDEX;	// 0x00080000
constexpr U32 AGENT_CONTROL_NUDGE_AT_NEG	= 0x1 << CONTROL_NUDGE_AT_NEG_INDEX;	// 0x00100000
constexpr U32 AGENT_CONTROL_NUDGE_LEFT_POS	= 0x1 << CONTROL_NUDGE_LEFT_POS_INDEX;	// 0x00200000
constexpr U32 AGENT_CONTROL_NUDGE_LEFT_NEG	= 0x1 << CONTROL_NUDGE_LEFT_NEG_INDEX;	// 0x00400000
constexpr U32 AGENT_CONTROL_NUDGE_UP_POS	= 0x1 << CONTROL_NUDGE_UP_POS_INDEX;	// 0x00800000
constexpr U32 AGENT_CONTROL_NUDGE_UP_NEG	= 0x1 << CONTROL_NUDGE_UP_NEG_INDEX;	// 0x01000000
constexpr U32 AGENT_CONTROL_TURN_LEFT		= 0x1 << CONTROL_TURN_LEFT_INDEX;		// 0x02000000
constexpr U32 AGENT_CONTROL_TURN_RIGHT		= 0x1 << CONTROL_TURN_RIGHT_INDEX;		// 0x04000000

constexpr U32 AGENT_CONTROL_AWAY			= 0x1 << CONTROL_AWAY_INDEX;			// 0x08000000

constexpr U32 AGENT_CONTROL_LBUTTON_DOWN	= 0x1 << CONTROL_LBUTTON_DOWN_INDEX;	// 0x10000000
constexpr U32 AGENT_CONTROL_LBUTTON_UP		= 0x1 << CONTROL_LBUTTON_UP_INDEX;		// 0x20000000
constexpr U32 AGENT_CONTROL_ML_LBUTTON_DOWN	= 0x1 << CONTROL_ML_LBUTTON_DOWN_INDEX;	// 0x40000000
constexpr U32 AGENT_CONTROL_ML_LBUTTON_UP	= ((U32)0x1) << CONTROL_ML_LBUTTON_UP_INDEX;	// 0x80000000

constexpr U32 AGENT_CONTROL_AT 	= AGENT_CONTROL_AT_POS 
								  | AGENT_CONTROL_AT_NEG 
								  | AGENT_CONTROL_NUDGE_AT_POS 
								  | AGENT_CONTROL_NUDGE_AT_NEG;

constexpr U32 AGENT_CONTROL_LEFT = AGENT_CONTROL_LEFT_POS 
								  | AGENT_CONTROL_LEFT_NEG 
								  | AGENT_CONTROL_NUDGE_LEFT_POS 
								  | AGENT_CONTROL_NUDGE_LEFT_NEG;

constexpr U32 AGENT_CONTROL_UP 	= AGENT_CONTROL_UP_POS 
								  | AGENT_CONTROL_UP_NEG 
								  | AGENT_CONTROL_NUDGE_UP_POS 
								  | AGENT_CONTROL_NUDGE_UP_NEG;

constexpr U32 AGENT_CONTROL_HORIZONTAL = AGENT_CONTROL_AT 
										 | AGENT_CONTROL_LEFT;

constexpr U32 AGENT_CONTROL_NOT_USED_BY_LSL = AGENT_CONTROL_FLY 
											  | AGENT_CONTROL_STOP 
											  | AGENT_CONTROL_FINISH_ANIM 
											  | AGENT_CONTROL_STAND_UP 
											  | AGENT_CONTROL_SIT_ON_GROUND 
											  | AGENT_CONTROL_MOUSELOOK 
											  | AGENT_CONTROL_AWAY;

constexpr U32 AGENT_CONTROL_MOVEMENT = AGENT_CONTROL_AT 
									   | AGENT_CONTROL_LEFT 
									   | AGENT_CONTROL_UP;

constexpr U32 AGENT_CONTROL_ROTATION = AGENT_CONTROL_PITCH_POS 
									   | AGENT_CONTROL_PITCH_NEG 
									   | AGENT_CONTROL_YAW_POS 
									   | AGENT_CONTROL_YAW_NEG;

constexpr U32 AGENT_CONTROL_NUDGE = AGENT_CONTROL_NUDGE_AT_POS
									| AGENT_CONTROL_NUDGE_AT_NEG
									| AGENT_CONTROL_NUDGE_LEFT_POS
									| AGENT_CONTROL_NUDGE_LEFT_NEG;
	

// move these up so that we can hide them in "State" for object updates 
// (for now)
constexpr U32 AGENT_ATTACH_OFFSET	= 4;
constexpr U32 AGENT_ATTACH_MASK		= 0xf << AGENT_ATTACH_OFFSET;
constexpr U32 AGENT_ATTACH_CLEAR	= 0x00;

// RN: this method swaps the upper and lower nibbles to maintain backward 
// compatibility with old objects that only used the upper nibble
#define ATTACHMENT_ID_FROM_STATE(state) ((S32)((((U8)state & AGENT_ATTACH_MASK) >> 4) | (((U8)state & ~AGENT_ATTACH_MASK) << 4)))

// test state for use in testing grabbing the camera
constexpr U32 AGENT_CAMERA_OBJECT	= 0x1 << 3;

constexpr F32 MAX_ATTACHMENT_DIST	= 3.5f; // meters ?

#endif

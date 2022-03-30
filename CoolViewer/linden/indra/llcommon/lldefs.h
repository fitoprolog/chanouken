/**
 * @file lldefs.h
 * @brief Various generic constant definitions.
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

#ifndef LL_LLDEFS_H
#define LL_LLDEFS_H

#include "llpreprocessor.h"

#include "stdtypes.h"

// Often used array indices
constexpr U32 VX = 0;
constexpr U32 VY = 1;
constexpr U32 VZ = 2;
constexpr U32 VW = 3;
constexpr U32 VS = 3;

constexpr U32 VRED   = 0;
constexpr U32 VGREEN = 1;
constexpr U32 VBLUE  = 2;
constexpr U32 VALPHA = 3;

constexpr U32 INVALID_DIRECTION = 0xFFFFFFFF;
constexpr U32 EAST  = 0;
constexpr U32 NORTH = 1;
constexpr U32 WEST  = 2;
constexpr U32 SOUTH = 3;

constexpr U32 NORTHEAST = 4;
constexpr U32 NORTHWEST = 5;
constexpr U32 SOUTHWEST = 6;
constexpr U32 SOUTHEAST = 7;
constexpr U32 MIDDLE    = 8;

constexpr U8 EAST_MASK  = 0x1<<EAST;
constexpr U8 NORTH_MASK = 0x1<<NORTH;
constexpr U8 WEST_MASK  = 0x1<<WEST;
constexpr U8 SOUTH_MASK = 0x1<<SOUTH;

constexpr U8 NORTHEAST_MASK = NORTH_MASK | EAST_MASK;
constexpr U8 NORTHWEST_MASK = NORTH_MASK | WEST_MASK;
constexpr U8 SOUTHWEST_MASK = SOUTH_MASK | WEST_MASK;
constexpr U8 SOUTHEAST_MASK = SOUTH_MASK | EAST_MASK;

const U32 gDirOpposite[8] = { 2, 3, 0, 1, 6, 7, 4, 5 };
const U32 gDirAdjacent[8][2] =  {
								{ 4, 7 },
								{ 4, 5 },
								{ 5, 6 },
								{ 6, 7 },
								{ 0, 1 },
								{ 1, 2 },
								{ 2, 3 },
								{ 0, 3 }
								};

// Magnitude along the x and y axis
const S32 gDirAxes[8][2] =
{
	{ 1, 0 },	// East
	{ 0, 1 },	// North
	{ -1, 0 },	// West
	{ 0, -1 },	// South
	{ 1, 1 },	// NE
	{ -1, 1 },	// NW
	{ -1, -1 },	// SW
	{ 1, -1 },	// SE
};

const S32 gDirMasks[8] =
{
	EAST_MASK,
	NORTH_MASK,
	WEST_MASK,
	SOUTH_MASK,
	NORTHEAST_MASK,
	NORTHWEST_MASK,
	SOUTHWEST_MASK,
	SOUTHEAST_MASK
};

// Sides of a box...
//                  . Z      __.Y
//                 /|\        /|       0 = NO_SIDE
//                  |        /         1 = FRONT_SIDE   = +x
//           +------|-----------+      2 = BACK_SIDE    = -x
//          /|      |/     /   /|      3 = LEFT_SIDE    = +y
//         / |     -5-   |/   / |      4 = RIGHT_SIDE   = -y
//        /  |     /|   -3-  /  |      5 = TOP_SIDE     = +z
//       +------------------+   |      6 = BOTTOM_SIDE  = -z
//       |   |      |  /    |   |
//       | |/|      | /     | |/|
//       | 2 |    | *-------|-1--------> X
//       |/| |   -4-        |/| |
//       |   +----|---------|---+
//       |  /        /      |  /
//       | /       -6-      | /
//       |/        /        |/
//       +------------------+
constexpr U32 NO_SIDE 		= 0;
constexpr U32 FRONT_SIDE 	= 1;
constexpr U32 BACK_SIDE 	= 2;
constexpr U32 LEFT_SIDE 	= 3;
constexpr U32 RIGHT_SIDE 	= 4;
constexpr U32 TOP_SIDE 		= 5;
constexpr U32 BOTTOM_SIDE 	= 6;

constexpr U8 LL_SOUND_FLAG_NONE =         0x0;
constexpr U8 LL_SOUND_FLAG_LOOP =         1 << 0;
constexpr U8 LL_SOUND_FLAG_SYNC_MASTER =  1 << 1;
constexpr U8 LL_SOUND_FLAG_SYNC_SLAVE =   1 << 2;
constexpr U8 LL_SOUND_FLAG_SYNC_PENDING = 1 << 3;
constexpr U8 LL_SOUND_FLAG_QUEUE =        1 << 4;
constexpr U8 LL_SOUND_FLAG_STOP =         1 << 5;
constexpr U8 LL_SOUND_FLAG_SYNC_MASK = LL_SOUND_FLAG_SYNC_MASTER |
									   LL_SOUND_FLAG_SYNC_SLAVE |
									   LL_SOUND_FLAG_SYNC_PENDING;

// *NOTE: These values may be used as hard-coded numbers in scanf() variants.
// --------------
// DO NOT CHANGE.
// --------------

// Buffer size of maximum path + filename string length
constexpr U32 LL_MAX_PATH = 1024;

// For strings we send in messages
constexpr U32 STD_STRING_BUF_SIZE = 255;	// Buffer size
constexpr U32 STD_STRING_STR_LEN = 254;		// String length, \0 excluded

// *NOTE: This value is used as hard-coded numbers in scanf() variants.
// DO NOT CHANGE.
constexpr U32 MAX_STRING = STD_STRING_BUF_SIZE;	// Buffer size
 // 123.567.901.345 = 15 chars + \0 + 1 for good luck
constexpr U32 MAXADDRSTR = 17;

// C++ is our friend. . . use template functions to make life easier!

// specific LL_INLINEs for basic types
//
// defined for all:
//   llmin(a,b)
//   llmax(a,b)
//   llclamp(a,minimum,maximum)
//
// defined for F32, F64:
//   llclampf(a)     // clamps a to [0.0 .. 1.0]
//
// defined for U16, U32, U64, S16, S32, S64, :
//   llclampb(a)     // clamps a to [0 .. 255]
//

template <class LLDATATYPE>
LL_INLINE LLDATATYPE llmax(const LLDATATYPE& d1, const LLDATATYPE& d2)
{
	return d1 > d2 ? d1 : d2;
}

template <class LLDATATYPE>
LL_INLINE LLDATATYPE llmax(const LLDATATYPE& d1, const LLDATATYPE& d2,
						   const LLDATATYPE& d3)
{
	LLDATATYPE r = llmax(d1,d2);
	return llmax(r, d3);
}

template <class LLDATATYPE>
LL_INLINE LLDATATYPE llmax(const LLDATATYPE& d1, const LLDATATYPE& d2,
						   const LLDATATYPE& d3, const LLDATATYPE& d4)
{
	LLDATATYPE r1 = llmax(d1,d2);
	LLDATATYPE r2 = llmax(d3,d4);
	return llmax(r1, r2);
}

template <class LLDATATYPE>
LL_INLINE LLDATATYPE llmin(const LLDATATYPE& d1, const LLDATATYPE& d2)
{
	return d1 < d2 ? d1 : d2;
}

template <class LLDATATYPE>
LL_INLINE LLDATATYPE llmin(const LLDATATYPE& d1, const LLDATATYPE& d2,
						   const LLDATATYPE& d3)
{
	LLDATATYPE r = llmin(d1,d2);
	return r < d3 ? r : d3;
}

template <class LLDATATYPE>
LL_INLINE LLDATATYPE llmin(const LLDATATYPE& d1, const LLDATATYPE& d2,
						   const LLDATATYPE& d3, const LLDATATYPE& d4)
{
	LLDATATYPE r1 = llmin(d1,d2);
	LLDATATYPE r2 = llmin(d3,d4);
	return llmin(r1, r2);
}

template <class LLDATATYPE>
LL_INLINE LLDATATYPE llclamp(const LLDATATYPE& a, const LLDATATYPE& minval,
							 const LLDATATYPE& maxval)
{
	if (a < minval)
	{
		return minval;
	}
	if (a > maxval)
	{
		return maxval;
	}
	return a;
}

template <class LLDATATYPE>
LL_INLINE LLDATATYPE llclampf(const LLDATATYPE& a)
{
	return llmin(llmax(a, (LLDATATYPE)0), (LLDATATYPE)1);
}

template <class LLDATATYPE>
LL_INLINE LLDATATYPE llclampb(const LLDATATYPE& a)
{
	return llmin(llmax(a, (LLDATATYPE)0), (LLDATATYPE)255);
}

template <class LLDATATYPE>
LL_INLINE void llswap(LLDATATYPE& lhs, LLDATATYPE& rhs)
{
	LLDATATYPE tmp = lhs;
	lhs = rhs;
	rhs = tmp;
}

#endif // LL_LLDEFS_H

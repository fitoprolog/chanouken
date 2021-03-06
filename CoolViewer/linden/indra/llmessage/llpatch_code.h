/**
 * @file llpatch_code.h
 * @brief Function declarations for encoding and decoding patches.
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

#ifndef LL_PATCH_CODE_H
#define LL_PATCH_CODE_H

// Saves a mandatory include in sources using patch code
#include "llpatch_dct.h"

class LLBitPack;
class LLGroupHeader;
class LLPatchHeader;

void init_patch_coding(LLBitPack& bitpack);
void code_patch_group_header(LLBitPack& bitpack, LLGroupHeader* gopp);
void code_patch_header(LLBitPack& bitpack, LLPatchHeader* ph, S32* patch);
void code_end_of_data(LLBitPack& bitpack);
void code_patch(LLBitPack& bitpack, S32* patch, S32 postquant);
void end_patch_coding(LLBitPack& bitpack);

void init_patch_decoding(LLBitPack& bitpack);
void decode_patch_group_header(LLBitPack& bitpack, LLGroupHeader* gopp);
void decode_patch_header(LLBitPack& bitpack, LLPatchHeader* ph,
						 bool large_patch = false);
void decode_patch(LLBitPack& bitpack, S32* patches);

#endif

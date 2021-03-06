/** 
 * @file vorbisencode.h
 * @brief Vorbis encoding routine routine for Indra.
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

#ifndef LL_VORBISENCODE_H
#define LL_VORBISENCODE_H

constexpr S32 LLVORBISENC_NOERR						= 0; // No error
constexpr S32 LLVORBISENC_SOURCE_OPEN_ERR			= 1; // Error opening source
constexpr S32 LLVORBISENC_DEST_OPEN_ERR				= 2; // Error opening dest
constexpr S32 LLVORBISENC_WAV_FORMAT_ERR			= 3; // not a WAV
constexpr S32 LLVORBISENC_PCM_FORMAT_ERR			= 4; // not a PCM
constexpr S32 LLVORBISENC_MONO_ERR					= 5; // Ccannot do mono
constexpr S32 LLVORBISENC_STEREO_ERR				= 6; // Cannot do stereo
constexpr S32 LLVORBISENC_MULTICHANNEL_ERR			= 7; // Cannot do stereo
constexpr S32 LLVORBISENC_UNSUPPORTED_SAMPLE_RATE	= 8; // Unsupported sample rate
constexpr S32 LLVORBISENC_UNSUPPORTED_WORD_SIZE		= 9; // Unsupported word size
constexpr S32 LLVORBISENC_CLIP_TOO_LONG				= 10; // Src file too long
constexpr S32 LLVORBISENC_CHUNK_SIZE_ERR			= 11; // Wrong chunk size

constexpr F32 LLVORBIS_CLIP_MAX_TIME				= 30.f;
constexpr U8  LLVORBIS_CLIP_MAX_CHANNELS			= 2;
constexpr U32 LLVORBIS_CLIP_SAMPLE_RATE				= 44100;
constexpr U32 LLVORBIS_CLIP_MAX_SAMPLES_PER_CHANNEL	= (U32)(LLVORBIS_CLIP_MAX_TIME * LLVORBIS_CLIP_SAMPLE_RATE);
constexpr U32 LLVORBIS_CLIP_MAX_SAMPLES				= LLVORBIS_CLIP_MAX_SAMPLES_PER_CHANNEL * LLVORBIS_CLIP_MAX_CHANNELS;
constexpr size_t LLVORBIS_CLIP_MAX_SAMPLE_DATA		= LLVORBIS_CLIP_MAX_SAMPLES * 2; // 2 = 16-bit

// Treat anything this long as a bad asset. A little fudge factor at the end:
// Make that a lot of fudge factor. We're allowing 30 sec for now - 3x legal upload
constexpr size_t LLVORBIS_CLIP_REJECT_SAMPLES		= LLVORBIS_CLIP_MAX_SAMPLES * 3;
constexpr size_t LLVORBIS_CLIP_REJECT_SIZE			= LLVORBIS_CLIP_MAX_SAMPLE_DATA * 3;

S32 check_for_invalid_wav_formats(const std::string& in_fname,
								  std::string& error_msg, F32 max_duration);
S32 encode_vorbis_file(const std::string& in_fname,
					   const std::string& out_fname, F32 max_duration);

#endif


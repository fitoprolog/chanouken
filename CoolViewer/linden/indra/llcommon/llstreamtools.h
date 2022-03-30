/**
 * @file llstreamtools.h
 * @brief some helper functions for parsing legacy simstate and asset files.
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

#ifndef LL_STREAM_TOOLS_H
#define LL_STREAM_TOOLS_H

#include <iostream>
#include <string>

#include "llpreprocessor.h"

// Unless specifed otherwise these all return input_stream.good()

// Skips emptyspace and lines that start with a #
LL_COMMON_API bool skip_comments_and_emptyspace(std::istream& input_stream);

// Skips to character after the end of next keyword. A 'keyword' is defined as
// the first word on a line
LL_COMMON_API bool skip_to_end_of_next_keyword(const char* keyword,
											   std::istream& input_stream);

LL_COMMON_API bool get_line(std::string& output_string,
							std::istream& input_stream, int n);

// *TODO: move these string manipulator functions to a different file

// replaces unescaped characters with expanded equivalents from left to right
// '\\' ---> "\\"
// '\n' ---> "\n"
LL_COMMON_API void escape_string(std::string& line);

// The 'keyword' is defined as the first word on a line
// The 'value' is everything after the keyword on the same line starting at the
// first non-whitespace and ending right before the newline.
LL_COMMON_API void get_keyword_and_value(std::string& keyword,
									     std::string& value,
									     const std::string& line);

// Continue to read from the stream until you really cannot read anymore or
// until we hit the count. Some istream implementations have a max that they
// will read. Returns the number of bytes read.
LL_COMMON_API std::streamsize fullread(std::istream& istr, char* buf,
									   std::streamsize requested);

LL_COMMON_API std::istream& operator>>(std::istream& str, const char* tocheck);

// gunzip srcfile into dstfile. Returns false on error.
LL_COMMON_API bool gunzip_file(const std::string& srcfile,
							   const std::string& dstfile);

// gzip srcfile into dstfile. Returns false on error.
LL_COMMON_API bool gzip_file(const std::string& srcfile,
							 const std::string& dstfile);

#endif	// LL_STREAM_TOOLS_H

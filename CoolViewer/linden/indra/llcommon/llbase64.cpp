/**
 * @file llbase64.cpp
 * @brief Wrapper for apr base64 encoding that returns a std::string
 * @author James Cook
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "linden_common.h"

#include "apr_base64.h"

#include "llbase64.h"

//static
std::string LLBase64::encode(const char* input, size_t input_size)
{
	std::string output;
	if (input && input_size > 0)
	{
		// Estimated (maximum) length, trailing nul char included
		int b64_buffer_length = apr_base64_encode_len(input_size);
		char* b64_buffer = new char[b64_buffer_length];
		apr_base64_encode_binary(b64_buffer, (const unsigned char*)input,
								 input_size);
		output.assign(b64_buffer); // Note: buffer is nul-terminated
		delete[] b64_buffer;
	}
	return output;
}

//static
std::string LLBase64::decode(const char* input)
{
	std::string output;
	if (input)
	{
		// Estimated (maximum) length
		int b64_buffer_length = apr_base64_decode_len(input);
		unsigned char* b64_buffer = new unsigned char[b64_buffer_length];
		// Actual length
		b64_buffer_length = apr_base64_decode_binary(b64_buffer, input);
		output.assign((char*)b64_buffer, b64_buffer_length);
		delete[] b64_buffer;
	}
	return output;
}

//static
std::string LLBase64::encode_text(const char* input, size_t input_size)
{
	std::string output;
	if (input && input_size > 0)
	{
		int b64_buffer_length = apr_base64_encode_len(input_size);
		char* b64_buffer = new char[b64_buffer_length];
		apr_base64_encode(b64_buffer, input, input_size);
		output.assign(b64_buffer);
		delete[] b64_buffer;
	}
	return output;
}

//static
std::string LLBase64::decode_text(const char* input)
{
	std::string output;
	if (input)
	{
		int b64_buffer_length = apr_base64_decode_len(input);
		char* b64_buffer = new char[b64_buffer_length];
		apr_base64_decode(b64_buffer, input);
		output.assign(b64_buffer, b64_buffer_length);
		delete[] b64_buffer;
	}
	return output;
}

/**
 * @file llxmlparser.h
 * @brief LLXmlParser class definition
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

#ifndef LL_LLXMLPARSER_H
#define LL_LLXMLPARSER_H

#include "expat.h"

#include "llpreprocessor.h"

class LLXmlParser
{
protected:
	LOG_CLASS(LLXmlParser);

public:
	LLXmlParser();
	virtual ~LLXmlParser();

	// Parses entire file
	bool parseFile(const std::string& path);

	// Parses some input. Returns 0 if a fatal error is detected. The last call
	// must have is_final true. len may be zero for this call (or any other).
	S32 parse(const char* buf, int len, int is_inal);

	const char* getErrorString();

	S32 getCurrentLineNumber();

	S32 getCurrentColumnNumber();

	LL_INLINE S32 getDepth()								{ return mDepth; }

protected:
	// atts is an array of name/value pairs, terminated by 0; names and values
	// are 0 terminated.
	virtual void startElement(const char* name,
							  const char** atts)			{}

	virtual void endElement(const char* name)				{}

	// s is not 0 terminated.
	virtual void characterData(const char* s, int len)		{}

	// target and data are 0 terminated
	virtual void processingInstruction(const char* target,
									   const char* data)	{}

	// data is 0 terminated
	virtual void comment(const char* data)					{}

	virtual void startCdataSection()						{}

	virtual void endCdataSection()							{}

	// This is called for any characters in the XML document for which there is
	// no applicable handler.  This includes both characters that are part of
	// markup which is of a kind that is not reported (comments, markup
	// declarations), or characters that are part of a construct which could be
	// reported but for which no handler has been supplied. The characters are
	// passed exactly as they were in the XML document except that they will be
	// encoded in UTF-8.  Line boundaries are not normalized. Note that a byte
	// order mark character is not passed to the default handler. There are no
	// guarantees about how characters are divided between calls to the default
	// handler: for example, a comment might be split between multiple calls.
	virtual void defaultData(const char* s, int len)		{}

	// This is called for a declaration of an unparsed (NDATA) entity. The base
	// argument is whatever was set by XML_SetBase. The entity_name, system_id
	// and notation_name arguments will never be NULL. The other arguments may
	// be.
	virtual void unparsedEntityDecl(const char* entity_name, const char* base,
									const char* system_id,
									const char* public_id,
									const char* notation_name)
	{
	}

public:
	///////////////////////////////////////////////////////////////////////////
	// Pseudo-private methods. These are only used by internal callbacks.

	static void startElementHandler(void* user_data, const XML_Char* name,
									const XML_Char** atts);
	static void endElementHandler(void* user_data, const XML_Char* name);
	static void characterDataHandler(void* user_data, const XML_Char* s,
									 int len);
	static void processingInstructionHandler(void* user_data,
											 const XML_Char* target,
											 const XML_Char* data);
	static void commentHandler(void* user_data, const XML_Char* data);
	static void startCdataSectionHandler(void* user_data);
	static void endCdataSectionHandler(void* user_data);
	static void defaultDataHandler(void* user_data, const XML_Char* s,
								   int len);
	static void unparsedEntityDeclHandler(void* user_data,
										  const XML_Char* entity_name,
										  const XML_Char* base,
										  const XML_Char* system_id,
										  const XML_Char* public_id,
										  const XML_Char* notation_name);

protected:
	XML_Parser	mParser;
	std::string	mAuxErrorString;
	S32			mDepth;
};

#endif  // LL_LLXMLPARSER_H

/**
 * @file llnotecard.cpp
 * @brief LLNotecard class definition
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

#include "linden_common.h"

#include "llnotecard.h"

#include "llstreamtools.h"

LLNotecard::LLNotecard(S32 max_text)
:	mMaxText(max_text),
	mVersion(0),
	mEmbeddedVersion(0)
{
}

bool LLNotecard::importEmbeddedItemsStream(std::istream& str)
{
	// Version 1 format:
	//		LLEmbeddedItems version 1
	//		{
	//			count <number of entries being used and not deleted>
	//			{
	//				ext char index <index>
	//				<InventoryItem chunk>
	//			}
	//		}

	S32 i;
	S32 count = 0;

	str >> std::ws >> "LLEmbeddedItems version" >> mEmbeddedVersion >> "\n";
	if (str.fail())
	{
		llwarns << "Invalid Linden text file header" << llendl;
		goto import_file_failed;
	}

	if (1 != mEmbeddedVersion)
	{
		llwarns << "Invalid LLEmbeddedItems version: " << mEmbeddedVersion
				<< llendl;
		goto import_file_failed;
	}

	str >> std::ws >> "{\n";
	if (str.fail())
	{
		llwarns << "Invalid Linden text file format: missing {" << llendl;
		goto import_file_failed;
	}

	str >> std::ws >> "count " >> count >> "\n";
	if (str.fail())
	{
		llwarns << "Invalid LLEmbeddedItems count" << llendl;
		goto import_file_failed;
	}

	if ((count < 0))
	{
		llwarns << "Invalid LLEmbeddedItems count value: " << count << llendl;
		goto import_file_failed;
	}

	for(i = 0; i < count; i++)
	{
		str >> std::ws >> "{\n";
		if (str.fail())
		{
			llwarns << "Invalid LLEmbeddedItems file format: missing {"
					<< llendl;
			goto import_file_failed;
		}

		U32 index = 0;
		str >> std::ws >> "ext char index " >> index >> "\n";
		if (str.fail())
		{
			llwarns << "Invalid LLEmbeddedItems file format: missing ext char index"
					<< llendl;
			goto import_file_failed;
		}

		str >> std::ws >> "inv_item\t0\n";
		if (str.fail())
		{
			llwarns << "Invalid LLEmbeddedItems file format: missing inv_item"
					<< llendl;
			goto import_file_failed;
		}

		LLPointer<LLInventoryItem> item = new LLInventoryItem;
		if (!item->importLegacyStream(str))
		{
			llinfos << "notecard import failed" << llendl;
			goto import_file_failed;
		}
		mItems.push_back(item);

		str >> std::ws >> "}\n";
		if (str.fail())
		{
			llwarns << "Invalid LLEmbeddedItems file format: missing }"
					<< llendl;
			goto import_file_failed;
		}
	}

	str >> std::ws >> "}\n";
	if (str.fail())
	{
		llwarns << "Invalid LLEmbeddedItems file format: missing }" << llendl;
		goto import_file_failed;
	}

	return true;

import_file_failed:
	return false;
}

bool LLNotecard::importStream(std::istream& str)
{
	// Version 1 format:
	//		Linden text version 1
	//		{
	//			<EmbeddedItemList chunk>
	//			Text length
	//			<ASCII text; 0x80 | index = embedded item>
	//		}

	// Version 2 format: (NOTE: Imports identically to version 1)
	//		Linden text version 2
	//		{
	//			<EmbeddedItemList chunk>
	//			Text length
	//			<UTF8 text; FIRST_EMBEDDED_CHAR + index = embedded item>
	//		}

	str >> std::ws >> "Linden text version " >> mVersion >> "\n";
	if (str.fail())
	{
		llwarns << "Invalid Linden text file header " << llendl;
		return false;
	}

	if (1 != mVersion && 2 != mVersion)
	{
		llwarns << "Invalid Linden text file version: " << mVersion << llendl;
		return false;
	}

	str >> std::ws >> "{\n";
	if (str.fail())
	{
		llwarns << "Invalid Linden text file format" << llendl;
		return false;
	}

	if (!importEmbeddedItemsStream(str))
	{
		return false;
	}

	char line_buf[STD_STRING_BUF_SIZE];
	str.getline(line_buf, STD_STRING_BUF_SIZE);
	if (str.fail())
	{
		llwarns << "Invalid Linden text length field" << llendl;
		return false;
	}
	line_buf[STD_STRING_STR_LEN] = '\0';

	S32 text_len = 0;
	if (1 != sscanf(line_buf, "Text length %d", &text_len))
	{
		llwarns << "Invalid Linden text length field" << llendl;
		return false;
	}

	if (text_len > mMaxText || text_len < 0)
	{
		llwarns << "Invalid Linden text length: " << text_len << llendl;
		return false;
	}

	bool success = true;

	char* text = new char[text_len + 1];
	fullread(str, text, text_len);
	if (str.fail())
	{
		llwarns << "Invalid Linden text: text shorter than text length: "
				<< text_len << llendl;
		success = false;
	}
	text[text_len] = '\0';

	if (success)
	{
		// Actually set the text
		mText = std::string(text);
	}

	delete[] text;

	return success;
}

////////////////////////////////////////////////////////////////////////////

bool LLNotecard::exportEmbeddedItemsStream(std::ostream& out_stream)
{
	out_stream << "LLEmbeddedItems version 1\n";
	out_stream << "{\n";

	S32 count =  mItems.size();
	out_stream << llformat("count %d\n", count);

	for (S32 i = 0; i < count; ++i)
	{
		LLInventoryItem* item = mItems[i];
		if (item)
		{
			out_stream << "{\n";
			out_stream << llformat("ext char index %d\n", i);
			if (!item->exportLegacyStream(out_stream))
			{
				return false;
			}
			out_stream << "}\n";
		}
	}

	out_stream << "}\n";

	return true;
}

bool LLNotecard::exportStream(std::ostream& out_stream)
{
	out_stream << "Linden text version 2\n";
	out_stream << "{\n";

	if (!exportEmbeddedItemsStream(out_stream))
	{
		return false;
	}

	out_stream << llformat("Text length %d\n", mText.length());
	out_stream << mText;
	out_stream << "}\n";

	return true;
}

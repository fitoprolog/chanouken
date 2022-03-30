/**
 * @file lllogchat.cpp
 * @brief LLLogChat class implementation
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

#include "llviewerprecompiledheaders.h"

#include "lllogchat.h"

#include "lldir.h"

#include "llgridmanager.h"
#include "llviewercontrol.h"

//static
std::string LLLogChat::timestamp(bool no_date)
{
	std::string format;
	static LLCachedControl<bool> withdate(gSavedSettings, "LogTimestampDate");
	if (!no_date && withdate)
	{
		static LLCachedControl<std::string> date_fmt(gSavedSettings,
													 "ShortDateFormat");
		format = date_fmt;
		format += " ";
	}
	static LLCachedControl<bool> with_seconds(gSavedSettings,
											  "LogTimestampSeconds");
	if (with_seconds)
	{
		static LLCachedControl<std::string> long_fmt(gSavedSettings,
													 "LongTimeFormat");
		format += long_fmt;
	}
	else
	{
		static LLCachedControl<std::string> short_fmt(gSavedSettings,
													  "ShortTimeFormat");
		format += short_fmt;
	}
	return "[" + LLGridManager::getTimeStamp(time_corrected(), format) + "]";
}

//static
std::string LLLogChat::makeLogFileName(std::string filename)
{
	if (gIsInSecondLife &&
		gSavedPerAccountSettings.getBool("LogFileNameWithoutResident"))
	{
		LLStringUtil::replaceString(filename, " Resident", "");
	}

	if (gSavedPerAccountSettings.getBool("LogFileNamewithDate"))
	{
		time_t now;
		time(&now);
		char dbuffer[20];
		if (filename == "chat")
		{
			strftime(dbuffer, 20, "-%Y-%m-%d", localtime(&now));
		}
		else
		{
			strftime(dbuffer, 20, "-%Y-%m", localtime(&now));
		}
		filename += dbuffer;
	}

	filename = cleanFileName(filename);
	filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT_CHAT_LOGS,
											  filename);
	filename += ".txt";
	return filename;
}

//static
std::string LLLogChat::cleanFileName(std::string filename)
{
	std::string invalidChars = "\"\'\\/?*:<>|";
	size_t position = filename.find_first_of(invalidChars);
	while (position != std::string::npos)
	{
		filename[position] = '_';
		position = filename.find_first_of(invalidChars, position);
	}
	return filename;
}

//static
void LLLogChat::saveHistory(std::string filename, std::string line)
{
	if (!filename.size())
	{
		llwarns << "Filename is Empty !" << llendl;
		return;
	}

	LLFILE* fp = LLFile::open(LLLogChat::makeLogFileName(filename), "a");
	if (!fp)
	{
		llwarns << "Couldn't open chat history log file: " << filename
				<< llendl;
	}
	else
	{
		fprintf(fp, "%s\n", line.c_str());
		LLFile::close (fp);
	}
}

//static
void LLLogChat::loadHistory(std::string filename,
							void (*callback)(ELogLineType, std::string, void*),
							void* userdata)
{
	if (!filename.size())
	{
		return;
	}

	std::string log_filename = makeLogFileName(filename);
	LLFILE* fp = LLFile::open(log_filename, "r");
	if (!fp)
	{
		// No previous conversation with this name.
		callback(LOG_EMPTY, LLStringUtil::null, userdata);
		return;
	}
	else
	{
		callback(LOG_FILENAME, log_filename, userdata);
		U32 bsize = gSavedPerAccountSettings.getU32("LogShowHistoryMaxSize");
		// The minimum must be larger than the largest line (1024 characters
		// for the largest text line + timestamp size + resident name size).
		bsize = llmax(bsize, 2U) * 1024U;
		char* buffer = (char*)malloc((size_t)bsize * sizeof(char));
		if (!buffer)
		{
			llwarns << "Failure to allocate buffer !" << llendl;
			return;
		}
		char* bptr;
		S32 len;
		bool firstline = true;

		if (fseek(fp, 1L - (long)bsize, SEEK_END))
		{
			// File is smaller than recall size. Get it all.
			firstline = false;
			if (fseek(fp, 0, SEEK_SET))
			{
				llwarns << "Failure to seek file: " << log_filename << llendl;
				LLFile::close(fp);
				free(buffer);
				return;
			}
		}

		while (fgets(buffer, bsize, fp))
		{
			len = strlen(buffer) - 1;
			for (bptr = buffer + len;
				 (*bptr == '\n' || *bptr == '\r') && bptr > buffer; --bptr)
			{
				*bptr = '\0';
			}

			if (!firstline)
			{
				callback(LOG_LINE, std::string(buffer), userdata);
			}
			else
			{
				firstline = false;
			}
		}
		callback(LOG_END, LLStringUtil::null, userdata);

		LLFile::close(fp);
		free(buffer);
	}
}

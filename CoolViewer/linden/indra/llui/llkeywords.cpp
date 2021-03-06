/**
 * @file llkeywords.cpp
 * @brief Keyword list for LSL
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

#include "linden_common.h"

#include <iostream>
#include <fstream>
#include "boost/tokenizer.hpp"

#include "llkeywords.h"
#include "llstl.h"
#include "lltexteditor.h"

constexpr U32 KEYWORD_FILE_CURRENT_VERSION = 2;

LL_INLINE bool LLKeywordToken::isHead(const llwchar* s,
									  bool search_end_c_comment) const
{
	// strncmp is much faster than string compare
	const llwchar* t = mToken.c_str();
	S32 len = mToken.size();
	if (search_end_c_comment && len == 2 && t[0] == '/' && t[1] == '*')
	{
		// Special case for C-like */ end comment token
		return s[0] == '*' && s[1] == '/';
	}
	for (S32 i = 0; i < len; ++i)
	{
		if (s[i] != t[i])
		{
			return false;
		}
	}
	return true;
}

LLKeywords::LLKeywords()
:	mLoaded(false)
{
}

LLKeywords::~LLKeywords()
{
	std::for_each(mWordTokenMap.begin(), mWordTokenMap.end(),
				  DeletePairedPointer());
	mWordTokenMap.clear();
	std::for_each(mLineTokenList.begin(), mLineTokenList.end(),
				  DeletePointer());
	mLineTokenList.clear();
	std::for_each(mDelimiterTokenList.begin(), mDelimiterTokenList.end(),
				  DeletePointer());
	mDelimiterTokenList.clear();
}

bool LLKeywords::loadFromFile(const std::string& filename)
{
	mLoaded = false;

	////////////////////////////////////////////////////////////
	// File header

	constexpr S32 BUFFER_SIZE = 1024;
	char buffer[BUFFER_SIZE];

	llifstream file(filename.c_str());
	if (!file.is_open())
	{
		llwarns << "Unable to open file: " << filename << llendl;
		return mLoaded;
	}

	// Identifying string
	file >> buffer;
	if (strcmp(buffer, "llkeywords"))
	{
		llwarns << filename << " does not appear to be a keyword file"
				<< llendl;
		return mLoaded;
	}

	// Check file version
	file >> buffer;
	U32	version_num;
	file >> version_num;
	if (strcmp(buffer, "version") ||
		version_num != (U32)KEYWORD_FILE_CURRENT_VERSION)
	{
		llwarns << filename << " does not appear to be a version "
				<< KEYWORD_FILE_CURRENT_VERSION << " keyword file" << llendl;
		return mLoaded;
	}

	// Start of line (SOL)
	static const std::string SOL_COMMENT = ";";
	static const std::string SOL_WORD = "[word ";
	static const std::string SOL_LINE = "[line ";
	static const std::string SOL_ONE_SIDED_DELIMITER = "[one_sided_delimiter ";
	static const std::string SOL_TWO_SIDED_DELIMITER = "[two_sided_delimiter ";

	LLColor3 cur_color(1.f, 0.f, 0.f);
	LLKeywordToken::TOKEN_TYPE cur_type = LLKeywordToken::WORD;

	while (!file.eof())
	{
		buffer[0] = 0;
		file.getline(buffer, BUFFER_SIZE);
		std::string line(buffer);
		if (line.find(SOL_COMMENT) == 0)
		{
			continue;
		}
		else if (line.find(SOL_WORD) == 0)
		{
			cur_color = readColor(line.substr(SOL_WORD.size()));
			cur_type = LLKeywordToken::WORD;
			continue;
		}
		else if (line.find(SOL_LINE) == 0)
		{
			cur_color = readColor(line.substr(SOL_LINE.size()));
			cur_type = LLKeywordToken::LINE;
			continue;
		}
		else if (line.find(SOL_TWO_SIDED_DELIMITER) == 0)
		{
			cur_color = readColor(line.substr(SOL_TWO_SIDED_DELIMITER.size()));
			cur_type = LLKeywordToken::TWO_SIDED_DELIMITER;
			continue;
		}
		else if (line.find(SOL_ONE_SIDED_DELIMITER) == 0)
		{
			cur_color = readColor(line.substr(SOL_ONE_SIDED_DELIMITER.size()));
			cur_type = LLKeywordToken::ONE_SIDED_DELIMITER;
			continue;
		}

		std::string token_buffer(line);
		LLStringUtil::trim(token_buffer);

		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep_word("", " \t");
		tokenizer word_tokens(token_buffer, sep_word);
		tokenizer::iterator token_word_iter = word_tokens.begin();

		if (!token_buffer.empty() && token_word_iter != word_tokens.end())
		{
			// First word is keyword
			std::string keyword = *token_word_iter;
			LLStringUtil::trim(keyword);

			// Following words are tooltip
			std::string tool_tip;
			while (++token_word_iter != word_tokens.end())
			{
				tool_tip += *token_word_iter;
			}
			LLStringUtil::trim(tool_tip);

			if (!tool_tip.empty())
			{
				// Replace : with \n for multi-line tool tips.
				LLStringUtil::replaceChar(tool_tip, ':', '\n');
				addToken(cur_type, keyword, cur_color, tool_tip);
			}
			else
			{
				addToken(cur_type, keyword, cur_color, LLStringUtil::null);
			}
		}
	}

	file.close();

	mLoaded = true;
	return mLoaded;
}

// Add the token as described
void LLKeywords::addToken(LLKeywordToken::TOKEN_TYPE type,
						  const std::string& key_in, const LLColor3& color,
						  const std::string& tool_tip_in)
{
	LLWString key = utf8str_to_wstring(key_in);
	LLWString tool_tip = utf8str_to_wstring(tool_tip_in);
	switch(type)
	{
		case LLKeywordToken::WORD:
			mWordTokenMap[key] = new LLKeywordToken(type, color, key,
													tool_tip);
			break;

		case LLKeywordToken::LINE:
			mLineTokenList.push_front(new LLKeywordToken(type, color, key,
														 tool_tip));
			break;

		case LLKeywordToken::TWO_SIDED_DELIMITER:
		case LLKeywordToken::ONE_SIDED_DELIMITER:
			mDelimiterTokenList.push_front(new LLKeywordToken(type, color, key,
															  tool_tip));
			break;

		default:
			llassert(false);
	}
}

LLColor3 LLKeywords::readColor(const std::string& s)
{
	F32 r, g, b;
	r = g = b = 0.0f;
	if (sscanf(s.c_str(), "%f, %f, %f]", &r, &g, &b) == 3)
	{
		return LLColor3(r, g, b);
	}

	size_t i = s.rfind(']');
	if (i != std::string::npos)
	{
		std::string color_name = s.substr(0, i);
		i = color_name.rfind(' ');
		if (i != std::string::npos)
		{
			color_name = color_name.substr(i + 1);
		}
		if (LLUI::sColorsGroup &&
			LLUI::sColorsGroup->getControl(color_name.c_str()))
		{
			return LLColor3(LLUI::sColorsGroup->getColor(color_name.c_str()));
		}
	}

	llwarns << "Poorly formed color (" << s << ") in keyword file." << llendl;

	return LLColor3(r, g, b);
}

// Walk through a string, applying the rules specified by the keyword token
// list and create a list of color segments.
void LLKeywords::findSegments(std::vector<LLTextSegment*>* seg_list,
							  const LLWString& wtext,
							  const LLColor4 &default_color)
{
	std::for_each(seg_list->begin(), seg_list->end(), DeletePointer());
	seg_list->clear();

	if (wtext.empty())
	{
		return;
	}

	S32 text_len = wtext.size();

	seg_list->push_back(new LLTextSegment(LLColor3(default_color), 0,
										  text_len));

	const llwchar* base = wtext.c_str();
	const llwchar* cur = base;

	while (*cur)
	{
		if (*cur == '\n' || cur == base)
		{
			if (*cur == '\n')
			{
				++cur;
				if (!*cur || *cur == '\n')
				{
					continue;
				}
			}

			// Skip white space
			while (*cur && isspace(*cur) && *cur != '\n')
			{
				++cur;
			}
			if (!*cur || *cur == '\n')
			{
				continue;
			}

			// cur is now at the first non-whitespace character of a new line

			// Line start tokens
			{
				bool line_done = false;
				for (token_list_t::iterator iter = mLineTokenList.begin(),
											end = mLineTokenList.end();
					 iter != end; ++iter)
				{
					LLKeywordToken* cur_token = *iter;
					if (cur_token->isHead(cur))
					{
						S32 seg_start = cur - base;
						while (*cur && *cur != '\n')
						{
							// skip the rest of the line
							++cur;
						}
						S32 seg_end = cur - base;

						LLTextSegment* text_segment =
							new LLTextSegment(cur_token->getColor(), seg_start,
											  seg_end);
						text_segment->setToken(cur_token);
						insertSegment(seg_list, text_segment, text_len,
									  default_color);
						line_done = true; // To break out of second loop.
						break;
					}
				}

				if (line_done)
				{
					continue;
				}
			}
		}

		// Skip white space
		while (*cur && isspace(*cur) && *cur != '\n')
		{
			++cur;
		}

		while (*cur && *cur != '\n')
		{
			// Check against delimiters
			{
				S32 seg_start = 0;
				LLKeywordToken* cur_delimiter = NULL;
				for (token_list_t::iterator iter = mDelimiterTokenList.begin();
					 iter != mDelimiterTokenList.end(); ++iter)
				{
					LLKeywordToken* delimiter = *iter;
					if (delimiter->isHead(cur))
					{
						cur_delimiter = delimiter;
						break;
					}
				}

				if (cur_delimiter)
				{
					S32 between_delimiters = 0;
					S32 seg_end = 0;

					seg_start = cur - base;
					cur += cur_delimiter->getLength();

					if (cur_delimiter->getType() ==
							LLKeywordToken::TWO_SIDED_DELIMITER)
					{
						while (*cur && !cur_delimiter->isHead(cur, true))
						{
							// Check for an escape sequence.
							if (*cur == '\\')
							{
								// Count the number of backslashes.
								S32 num_backslashes = 0;
								while (*cur == '\\')
								{
									++num_backslashes;
									++between_delimiters;
									++cur;
								}
								// Is the next character the end delimiter ?
								if (cur_delimiter->isHead(cur, true))
								{
									// Is there was an odd number of
									// backslashes, then this delimiter does
									// not end the sequence.
									if (num_backslashes % 2 == 1)
									{
										++between_delimiters;
										++cur;
									}
									else
									{
										// This is an end delimiter.
										break;
									}
								}
							}
							else
							{
								++between_delimiters;
								++cur;
							}
						}

						if (*cur)
						{
							cur += cur_delimiter->getLength();
							seg_end = seg_start + between_delimiters +
									  2 * cur_delimiter->getLength();
						}
						else
						{
							// eof
							seg_end = seg_start + between_delimiters +
									  cur_delimiter->getLength();
						}
					}
					else
					{
						llassert(cur_delimiter->getType() ==
								 LLKeywordToken::ONE_SIDED_DELIMITER);
						// Left side is the delimiter. Right side is eol or eof
						while (*cur && *cur != '\n')
						{
							++between_delimiters;
							++cur;
						}
						seg_end = seg_start + between_delimiters +
								  cur_delimiter->getLength();
					}

					LLTextSegment* text_segment =
						new LLTextSegment(cur_delimiter->getColor(), seg_start,
										  seg_end);
					text_segment->setToken(cur_delimiter);
					insertSegment(seg_list, text_segment, text_len,
								  default_color);

					// Note: we do not increment cur, since the end of one
					// delimited seg may be immediately followed by the start
					// of another one.
					continue;
				}
			}

			// Check against words
			llwchar prev = cur > base ? *(cur - 1) : 0;
			if (!isalnum(prev) && prev != '_')
			{
				const llwchar* p = cur;
#if 0			// Works for "#define", but not for "# define", so...
				if (*p == '#')	// For preprocessor #directive tokens
				{
					++p;
				}
#endif
				while (isalnum(*p) || *p == '_')
				{
					++p;
				}
				S32 seg_len = p - cur;
				if (seg_len > 0)
				{
					LLWString word(cur, 0, seg_len);
					word_token_map_t::iterator map_iter =
						mWordTokenMap.find(word);
					if (map_iter != mWordTokenMap.end())
					{
						LLKeywordToken* cur_token = map_iter->second;
						S32 seg_start = cur - base;
						S32 seg_end = seg_start + seg_len;

						LLTextSegment* text_segment =
							new LLTextSegment(cur_token->getColor(), seg_start,
											  seg_end);
						text_segment->setToken(cur_token);
						insertSegment(seg_list, text_segment, text_len,
									  default_color);
					}
					cur += seg_len;
					continue;
				}
			}

			if (*cur && *cur != '\n')
			{
				++cur;
			}
		}
	}
}

void LLKeywords::insertSegment(std::vector<LLTextSegment*>* seg_list,
							   LLTextSegment* new_segment, S32 text_len,
							   const LLColor4& default_color)
{
	LLTextSegment* last = seg_list->back();
	S32 new_seg_end = new_segment->getEnd();

	if (new_segment->getStart() == last->getStart())
	{
		*last = *new_segment;
		delete new_segment;
	}
	else
	{
		last->setEnd(new_segment->getStart());
		seg_list->push_back(new_segment);
	}

	if (new_seg_end < text_len)
	{
		seg_list->push_back(new LLTextSegment(default_color, new_seg_end,
											  text_len));
	}
}

#if LL_DEBUG
void LLKeywords::dump()
{
	llinfos << "LLKeywords" << llendl;
	llinfos << "LLKeywords::sWordTokenMap" << llendl;
	word_token_map_t::iterator word_token_iter = mWordTokenMap.begin();
	while (word_token_iter != mWordTokenMap.end())
	{
		LLKeywordToken* word_token = word_token_iter->second;
		word_token->dump();
		++word_token_iter;
	}

	llinfos << "LLKeywords::sLineTokenList" << llendl;
	for (token_list_t::iterator iter = mLineTokenList.begin();
		 iter != mLineTokenList.end(); ++iter)
	{
		LLKeywordToken* line_token = *iter;
		line_token->dump();
	}


	llinfos << "LLKeywords::sDelimiterTokenList" << llendl;
	for (token_list_t::iterator iter = mDelimiterTokenList.begin();
		 iter != mDelimiterTokenList.end(); ++iter)
	{
		LLKeywordToken* delimiter_token = *iter;
		delimiter_token->dump();
	}
}

void LLKeywordToken::dump()
{
	llinfos << "[" << mColor.mV[VX] << ", " << mColor.mV[VY] << ", "
			<< mColor.mV[VZ] << "] [" << wstring_to_utf8str(mToken) << "]"
			<< llendl;
}

#endif  // DEBUG

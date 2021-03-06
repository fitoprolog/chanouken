/** 
 * @file llspellcheck.cpp
 * @brief LLSpellCheck class implementation
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 * 
 * Copyright (c) 2009 LordGregGreg Back, 2012 Henri Beauchamp 
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

#if LL_WINDOWS
#include "hunspell/hunspelldll.h"
#else
#include "hunspell/hunspell.hxx"
#endif

#include "llspellcheck.h"

#include "lldir.h"
#include "lldiriterator.h"
#include "llstring.h"

LLSpellCheck::LLSpellCheck()
:	mHunspell(NULL),
	mSpellCheckEnable(false),
	mShowMisspelled(false)
{
}

LLSpellCheck::~LLSpellCheck()
{
	if (mHunspell)
	{
		delete mHunspell;
		mHunspell =  NULL;
		mIgnoreList.clear();
		mSpellCheckEnable = false;
	}
}

// static
void LLSpellCheck::initClass()
{
}

std::string LLSpellCheck::getDicFullPath(const std::string& file)
{
	// Check if it exists in user dir and if not take it from app dir
	std::string path = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
													  "dictionaries", file);
	if (!LLFile::exists(path))
	{
		path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
											  "dictionaries", file);
	}

	return path;
}

// Return all *.aff files found either in the user settings or in the app
// settings directories and for which there are corresponding *.dic files
// Note that the names are returned without the .aff extension.
std::set<std::string> LLSpellCheck::getBaseDicts()
{
	std::set<std::string> names;	
	std::string name;
	std::string path = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
													  "dictionaries", "");
	{
		LLDirIterator iter(path, "*.aff");
		while (iter.next(name))
		{
			LLStringUtil::truncate(name, name.length() - 4);
			if (LLFile::exists(path + name + ".dic"))
			{
				names.emplace(name);
			}
		}
	}	// Destroys LLDirIterator iter

	path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
										  "dictionaries", "");
	{
		LLDirIterator iter(path, "*.aff");
		while (iter.next(name))
		{
			LLStringUtil::truncate(name, name.length() - 4);
			if (LLFile::exists(path + name + ".dic"))
			{
				names.emplace(name);
			}
		}
	}	// Destroys LLDirIterator iter

	return names;
}

// Return all *.dic files found either in the user settings or in the app
// settings directories and for which there is no corresponding *.aff file
std::set<std::string> LLSpellCheck::getExtraDicts()
{
	std::set<std::string> names;	
	std::string name;
	std::string root;
	std::string path = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
													  "dictionaries", "");
	{
		LLDirIterator iter(path, "*.dic");
		while (iter.next(name))
		{
			root = name;
			LLStringUtil::truncate(root, root.length() - 4);
			if (!LLFile::exists(path + root + ".aff"))
			{
				names.emplace(name);
			}
		}
	}	// Destroys LLDirIterator iter

	path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
										  "dictionaries", "");
	{
		LLDirIterator iter(path, "*.dic");
		while (iter.next(name))
		{
			root = name;
			LLStringUtil::truncate(root, root.length() - 4);
			if (!LLFile::exists(path + root + ".aff"))
			{
				names.emplace(name);
			}
		}
	}	// Destroys LLDirIterator iter

	return names;
}

void LLSpellCheck::addDictionary(const std::string& dict_name)
{
	if (mHunspell && !dict_name.empty())
	{
		std::string dict = getDicFullPath(dict_name);
		if (LLFile::exists(dict))
		{
			llinfos << "Adding additional dictionary: " << dict << llendl;
			mHunspell->add_dic(dict.c_str());
		}
	}
}

void LLSpellCheck::setDictionary(const std::string& dict_name)
{
	std::string name = dict_name;
	LLStringUtil::toLower(name);
	std::string dicaffpath = getDicFullPath(name + ".aff");
	std::string dicdicpath = dicaffpath;
	LLStringUtil::truncate(dicdicpath, dicdicpath.length() - 4);
	dicdicpath += ".dic";
	if (!LLFile::exists(dicaffpath) || !LLFile::exists(dicdicpath))
	{
		llwarns << "Cannot find the dictionary files for: "
				<< dict_name << llendl;
		return;
	}

	llinfos << "Setting new base dictionary to " << dicdicpath
			<< " with associated affix file " << dicaffpath << llendl;
	mCurrentDictName = name;
	if (mHunspell)
	{
		delete mHunspell;
	}
	mHunspell = new Hunspell(dicaffpath.c_str(), dicdicpath.c_str());

	mDictEncoding = mHunspell->get_dic_encoding();
	llinfos << "Dictionary encoding is: " << mDictEncoding << llendl;
	LLStringUtil::toLower(mDictEncoding);

	std::string fname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
													   "dictionaries",
													   "custom.dic");
	if (!LLFile::exists(fname))
	{
		llinfos << "Creating custom.dic..." << llendl;
		LLFile::mkdir(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
													 "dictionaries"));
		llofstream outfile(fname.c_str());
		if (outfile.is_open())
		{
			outfile << 1 << std::endl;
			outfile << "SL" << std::endl;
			outfile.close();
		}
		else
		{
			llwarns << "Error creating custom.dic. Cannot open file for writing."
					<< llendl;
		}
	}

	llinfos << "Adding extra *.dic dictionaries..." << llendl;
	std::set<std::string> to_install = getExtraDicts();
	for (std::set<std::string>::iterator it = to_install.begin();
		 it != to_install.end(); ++it)
	{
		addDictionary(*it);
	}

	llinfos << "Done setting the dictionaries." << llendl;
}

void LLSpellCheck::addToCustomDictionary(const std::string& word)
{
	if (!mHunspell)
	{
		return;
	}
	mHunspell->add(word.c_str());

	std::vector<std::string> word_list;
	std::string fname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
													   "dictionaries",
													   "custom.dic");

	// Read the dictionary, if it exists
	if (LLFile::exists(fname))
	{
		// Get words already there..
		llifstream infile(fname.c_str());
		if (infile.is_open())
		{
			std::string line;
			S32 line_num = 0;
			while (getline(infile, line))
			{
				if (line_num != 0)	// Skip the count of lines in the list
				{
					word_list.emplace_back(line);
				}
				++line_num;
			}
		}
		infile.close();
	}

	// Add the new word to the list
#if 0
	if (mDictEncoding == "utf-8")
	{
		word_list.emplace_back(word);
	}
	else
	{
		word_list.emplace_back(utf8_to_iso8859(word));
	}
#else
	word_list.emplace_back(word);
#endif

	llofstream outfile(fname.c_str());	
	if (outfile.is_open())
	{
		outfile << word_list.size() << std::endl;
		for (std::vector<std::string>::const_iterator it = word_list.begin();
			 it != word_list.end(); ++it)
		{
			outfile << *it << std::endl;
		}
		outfile.close();
	}
	else
	{
		llwarns << "Could not add \"" << word
				<< "\" to the custom dictionary. Error opening the file for writing."
				<< llendl;
	}
}

void LLSpellCheck::addToIgnoreList(const std::string& word)
{
	if (word.length() > 2)
	{
		std::string lc_word = word;
		LLStringUtil::toLower(lc_word);
		mIgnoreList.emplace(lc_word);
	}
}

void LLSpellCheck::addWordsToIgnoreList(const std::string& words)
{
	// Add each lexical word in "words"
	for (size_t i = 0; i < words.length(); ++i)
	{
		std::string word;
		while (i < words.length() &&
			   LLStringUtil::isPartOfLexicalWord(words[i]))
		{
			if (words[i] != '\'' ||
				(!word.empty() && i + 1 < words.length() &&
				 words[i + 1] != '\'' &&
				 LLStringUtil::isPartOfLexicalWord(words[i + 1])))
			{
				word += words[i];
			}
			++i;
		}
		if (word.length() > 2)
		{
			addToIgnoreList(word);
			LL_DEBUGS("SpellCheck") << "Added \"" << word
									<< "\" to the ignore list." << LL_ENDL;
		}
	}
}

bool LLSpellCheck::checkSpelling(const std::string& word)
{
	if (mHunspell && word.length() > 2)
	{
		std::string lc_word = word;
		LLStringUtil::toLower(lc_word);
		if (mIgnoreList.count(lc_word))
		{
			return true;
		}
#if 0
		if (mDictEncoding == "utf-8")
		{
			return mHunspell->spell(word.c_str());
		}
		else
		{
			return mHunspell->spell(utf8_to_iso8859(word).c_str());
		}
#else
		return mHunspell->spell(word.c_str());
#endif
	}

	return true;
}

S32 LLSpellCheck::getSuggestions(const std::string& word,
								 std::vector<std::string>& suggestions)
{
	suggestions.clear();

	if (mHunspell && word.length() > 2)
	{
		char** suggestion_list;	
		S32 suggestion_count = mHunspell->suggest(&suggestion_list,
												  word.c_str());	
		if (suggestion_count > 0)
		{
			bool is_utf8 = mDictEncoding == "utf-8";
			std::string word;
			for (S32 i = 0; i < suggestion_count; ++i) 
			{
				if (is_utf8)
				{
					word = suggestion_list[i];
				}
				else
				{
					word = iso8859_to_utf8(suggestion_list[i]);
				}
				suggestions.emplace_back(word);
			}
		}
		mHunspell->free_list(&suggestion_list, suggestion_count);
	}

	return suggestions.size();
}

/**
 * @file llfontregistry.h
 * @author Brad Payne
 * @brief Storage for fonts.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 *
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#ifndef LL_LLFONTREGISTRY_H
#define LL_LLFONTREGISTRY_H

#include "llpointer.h"

class LLFontGL;

typedef std::vector<std::string> string_vec_t;

class LLFontDescriptor
{
public:
	LLFontDescriptor();
	LLFontDescriptor(const std::string& name, const std::string& size,
					 U8 style = 0);	// 0 = NORMAL style
	LLFontDescriptor(const std::string& name, const std::string& size,
					 U8 style, const string_vec_t& file_names);
	LLFontDescriptor normalize() const;

	bool operator<(const LLFontDescriptor& b) const;

	bool isTemplate() const;

	LL_INLINE const std::string& getName() const			{ return mName; }
	LL_INLINE void setName(const std::string& name)			{ mName = name; }
	LL_INLINE const std::string& getSize() const			{ return mSize; }
	LL_INLINE void setSize(const std::string& size)			{ mSize = size; }

	LL_INLINE const std::vector<std::string>& getFileNames() const
	{
		return mFileNames;
	}

	LL_INLINE std::vector<std::string>& getFileNames()		{ return mFileNames; }

	LL_INLINE U8 getStyle() const							{ return mStyle; }
	LL_INLINE void setStyle(U8 style)						{ mStyle = style; }

private:
	std::string		mName;
	std::string		mSize;
	string_vec_t	mFileNames;
	U8				mStyle;
};

class LLFontRegistry
{
	friend bool init_from_xml(LLFontRegistry*, LLPointer<class LLXMLNode>);

protected:
	LOG_CLASS(LLFontRegistry);

public:
	LLFontRegistry(const string_vec_t& xui_paths,
				   bool create_gl_textures = true);
	~LLFontRegistry();

	// Load standard font info from XML file(s).
	bool parseFontInfo(const std::string& xml_filename);

	// Clear cached glyphs for all fonts.
	void reset();

	// Destroy all fonts.
	void clear();

	// GL cleanup
	void destroyGL();

	LLFontGL* getFont(const LLFontDescriptor& desc, bool normalize = true);
	const LLFontDescriptor* getMatchingFontDesc(const LLFontDescriptor& desc);
	const LLFontDescriptor* getClosestFontTemplate(const LLFontDescriptor& desc);

	bool nameToSize(const std::string& size_name, F32& size);

	void dump();

	LL_INLINE const string_vec_t& getUltimateFallbackList() const
	{
		return mUltimateFallbackList;
	}

private:
	LLFontRegistry(const LLFontRegistry& other);	// no-copy
	LLFontGL* createFont(const LLFontDescriptor& desc);

private:
	// Given a descriptor, look up specific font instantiation.
	typedef std::map<LLFontDescriptor, LLFontGL*> font_reg_map_t;
	font_reg_map_t	mFontMap;

	// Given a size name, look up the point size.
	typedef std::map<std::string, F32> font_size_map_t;
	font_size_map_t	mFontSizes;

	string_vec_t	mUltimateFallbackList;
	string_vec_t	mXUIPaths;
	bool			mCreateGLTextures;
};

#endif // LL_LLFONTREGISTRY_H

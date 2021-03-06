/**
 * @file llfontfreetype.h
 * @brief Font library wrapper
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_FONTFREETYPE_H
#define LL_FONTFREETYPE_H

#include "llfastmap.h"
#include "llfontbitmapcache.h"
#include "llimagegl.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "llstl.h"

// Hack. FT_Face is just a typedef for a pointer to a struct, but there is no
// simple forward declarations file for FreeType and the main include file is
// 200K. We forward declare the struct here. JC
struct FT_FaceRec_;
typedef struct FT_FaceRec_* LLFT_Face;

class LLFontManager
{
public:
	static void initClass();
	static void cleanupClass();

public:
	LLFontManager();
	~LLFontManager();
};

struct LLFontGlyphInfo
{
	LLFontGlyphInfo(U32 index);

	U32 mGlyphIndex;

	// Metrics
	S32 mWidth;			// In pixels
	S32 mHeight;		// In pixels
	F32 mXAdvance;		// In pixels
	F32 mYAdvance;		// In pixels

	// Information for actually rendering
	S32 mXBitmapOffset;	// Offset to the origin in the bitmap
	S32 mYBitmapOffset;	// Offset to the origin in the bitmap
	S32 mXBearing;		// Distance from baseline to left in pixels
	S32 mYBearing;		// Distance from baseline to top in pixels
	S32 mBitmapNum;		// Which bitmap in the bitmap cache contains this glyph
};

extern LLFontManager* gFontManagerp;

class LLFontFreetype : public LLRefCount
{
protected:
	LOG_CLASS(LLFontFreetype);

public:
	LLFontFreetype();
	virtual ~LLFontFreetype();

	// is_fallback should be true for fallback fonts that aren't used
	// to render directly (Unicode backup, primarily)
	bool loadFace(const std::string& filename, F32 point_size, F32 vert_dpi,
				  F32 horz_dpi, S32 components, bool is_fallback);

	typedef std::vector<LLPointer<LLFontFreetype> > font_vector_t;

	void setFallbackFonts(const font_vector_t& font);
	const font_vector_t& getFallbackFonts() const;

	// Global font metrics - in units of pixels
	F32 getLineHeight() const;
	F32 getAscenderHeight() const;
	F32 getDescenderHeight() const;


// For a lowercase "g":
//
//	------------------------------
//	                     ^     ^
//						 |     |
//				xxx x    |Ascender
//	           x   x     v     |
//	---------   xxxx-------------- Baseline
//	^		       x	       |
//  | Descender    x           |
//	v			xxxx           |LineHeight
//  -----------------------    |
//                             v
//	------------------------------

	enum
	{
		FIRST_CHAR = 32,
		NUM_CHARS = 127 - 32,
		LAST_CHAR_BASIC = 127,

		// Need full 8-bit ascii range for spanish
		NUM_CHARS_FULL = 255 - 32,
		LAST_CHAR_FULL = 255
	};

	F32 getXAdvance(llwchar wc) const;
	F32 getXAdvance(const LLFontGlyphInfo* glyph) const;

	// Get the kerning between the two characters:
	F32 getXKerning(llwchar char_left, llwchar char_right) const;
	F32 getXKerning(const LLFontGlyphInfo* left_glyph_info,
					const LLFontGlyphInfo* right_glyph_info) const;

	LLFontGlyphInfo* getGlyphInfo(llwchar wch) const;

	void reset(F32 vert_dpi, F32 horz_dpi);

	void destroyGL();

	const std::string& getName() const;

	const LLPointer<LLFontBitmapCache> getFontBitmapCache() const;

	void setStyle(U8 style);
	U8 getStyle() const;

private:
	void resetBitmapCache();
	void setSubImageLuminanceAlpha(U32 x, U32 y, U32 bitmap_num,
								   U32 width, U32 height,
								   U8* data, S32 stride = 0) const;
	// Has a glyph for this character
	bool hasGlyph(const llwchar wch) const;
	// Add a new character to the font if necessary
	LLFontGlyphInfo* addGlyph(llwchar wch) const;
	// Add a glyph from this font to the other (returns the glyph_index, NULL
	// if not found)
	LLFontGlyphInfo* addGlyphFromFont(const LLFontFreetype* fontp,
									  llwchar wch, U32 glyph_index) const;

	void renderGlyph(U32 glyph_index) const;
	void insertGlyphInfo(llwchar wch, LLFontGlyphInfo* gi) const;

private:
	F32				mPointSize;
	F32				mAscender;
	F32				mDescender;
	F32				mLineHeight;
	F32**			mKerningCache;

	mutable S32		mRenderGlyphCount;
	mutable S32		mAddGlyphCount;

	LLFT_Face		mFTFace;

	std::string		mName;

	U8				mStyle;

	bool			mIsFallback;

	// A list of fallback fonts to look for glyphs in (for Unicode chars)
	font_vector_t	mFallbackFonts;

	typedef fast_hmap<llwchar, LLFontGlyphInfo*> char_glyph_info_map_t;
	// Information about glyph location in bitmap
	mutable char_glyph_info_map_t			mCharGlyphInfoMap;

	mutable LLPointer<LLFontBitmapCache>	mFontBitmapCachep;
};

#endif // LL_FONTFREETYPE_H

/**
 * @file llkeyboard.h
 * @brief Handler for assignable key bindings
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLKEYBOARD_H
#define LL_LLKEYBOARD_H

#include <map>

#include "indra_constants.h"
#include "llpreprocessor.h"
#include "llstringtable.h"
#include "lltimer.h"

enum EKeystate
{
	KEYSTATE_DOWN,
	KEYSTATE_LEVEL,
	KEYSTATE_UP
};

typedef void (*LLKeyFunc)(EKeystate keystate);

enum EKeyboardInsertMode
{
	LL_KIM_INSERT,
	LL_KIM_OVERWRITE
};

class LLKeyBinding
{
public:
	KEY			mKey;
	MASK		mMask;
#if 0	// unused
	const char*	mName;
#endif
	LLKeyFunc	mFunction;
};

class LLWindowCallbacks;

class LLKeyboard
{
protected:
	LOG_CLASS(LLKeyboard);

public:
	typedef enum e_numpad_distinct
	{
		ND_NEVER,
		ND_NUMLOCK_OFF,
		ND_NUMLOCK_ON
	} ENumpadDistinct;

public:
	LLKeyboard();
	virtual ~LLKeyboard()							{}

	void resetKeys();

	LL_INLINE F32  getCurKeyElapsedTime()			{ return getKeyDown(mCurScanKey) ? getKeyElapsedTime(mCurScanKey) : 0.f; }
	LL_INLINE F32  getCurKeyElapsedFrameCount()		{ return getKeyDown(mCurScanKey) ? (F32)getKeyElapsedFrameCount(mCurScanKey) : 0.f; }
	LL_INLINE bool getKeyDown(KEY key)				{ return mKeyLevel[key]; }
	LL_INLINE bool getKeyRepeated(KEY key)			{ return mKeyRepeated[key]; }

	bool translateKey(U32 os_key, KEY* translated_key, MASK mask);
	U32 inverseTranslateKey(KEY translated_key);
	// Translated into "Linden" keycodes
	bool handleTranslatedKeyUp(KEY translated_key, U32 translated_mask);
	// Translated into "Linden" keycodes
	bool handleTranslatedKeyDown(KEY translated_key, U32 translated_mask);

	virtual bool handleKeyUp(U32 key, MASK mask) = 0;
	virtual bool handleKeyDown(U32 key, MASK mask) = 0;

#if LL_DARWIN
	// We only actually use this for OS X.
	virtual void handleModifier(MASK mask) = 0;
#endif

	// Asynchronously poll the control, alt, and shift keys and set the
	// appropriate internal key masks.

	virtual void resetMaskKeys() = 0;
	// scans keyboard, calls functions as necessary:
	virtual void scanKeyboard() = 0;
	// Mac must differentiate between Command = Control for keyboard events
	// and Command != Control for mouse events.
	virtual MASK currentMask(bool for_mouse_event) = 0;
	LL_INLINE virtual KEY currentKey()				{ return mCurTranslatedKey; }

	LL_INLINE EKeyboardInsertMode getInsertMode()	{ return mInsertMode; }
	void toggleInsertMode();

	// false on failure
	static bool maskFromString(const char* str, MASK* mask);
	static bool keyFromString(const char* str, KEY* key);

	static std::string stringFromKey(KEY key);

	e_numpad_distinct getNumpadDistinct()			{ return mNumpadDistinct; }
	void setNumpadDistinct(e_numpad_distinct val)	{ mNumpadDistinct = val; }

	void setCallbacks(LLWindowCallbacks* cbs)		{ mCallbacks = cbs; }

	// Returns time in seconds since key was pressed:
	F32 getKeyElapsedTime(KEY key);
	// Returns time in frames since key was pressed:
	S32 getKeyElapsedFrameCount(KEY key);

protected:
	void addKeyName(KEY key, const std::string& name);

protected:
	// Map of translations from OS keys to Linden KEYs
	std::map<U32, KEY>	mTranslateKeyMap;
	// Map of translations from Linden KEYs to OS keys
	std::map<KEY, U32>	mInvTranslateKeyMap;
	LLWindowCallbacks*	mCallbacks;

	LLTimer				mKeyLevelTimer[KEY_COUNT];		// Time since level was set
	S32					mKeyLevelFrameCount[KEY_COUNT];	// Frames since level was set
	KEY					mCurTranslatedKey;
	KEY					mCurScanKey;					// Used during the scanKeyboard()

	e_numpad_distinct	mNumpadDistinct;
	EKeyboardInsertMode	mInsertMode;

	bool				mKeyLevel[KEY_COUNT];			// Levels
	bool				mKeyRepeated[KEY_COUNT];		// Key was repeated
	bool				mKeyUp[KEY_COUNT];				// Up edge
	bool				mKeyDown[KEY_COUNT];			// Down edge

	static std::map<KEY, std::string> sKeysToNames;
	static std::map<std::string, KEY> sNamesToKeys;
};

extern LLKeyboard* gKeyboardp;

#endif

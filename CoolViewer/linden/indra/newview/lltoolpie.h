/**
 * @file lltoolpie.h
 * @brief LLToolPie class header file
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

#ifndef LL_TOOLPIE_H
#define LL_TOOLPIE_H

#include "llsafehandle.h"
#include "llsingleton.h"
#include "lluuid.h"

#include "lltool.h"
#include "llviewerwindow.h" // for LLPickInfo

class LLViewerObject;
class LLObjectSelection;

class LLToolPie : public LLTool, public LLSingleton<LLToolPie>
{
	friend class LLSingleton<LLToolPie>;

protected:
	LOG_CLASS(LLToolPie);

public:
	LLToolPie();

	virtual bool handleMouseDown(S32 x, S32 y, MASK mask);
	virtual bool handleRightMouseDown(S32 x, S32 y, MASK mask);
	virtual bool handleMouseUp(S32 x, S32 y, MASK mask);
	virtual bool handleRightMouseUp(S32 x, S32 y, MASK mask);
	virtual bool handleHover(S32 x, S32 y, MASK mask);
	virtual bool handleDoubleClick(S32 x, S32 y, MASK mask);
	virtual bool handleScrollWheel(S32 x, S32 y, S32 clicks);

	LL_INLINE virtual void render()						{}

	virtual void stopEditing();

	virtual void onMouseCaptureLost()					{}
	virtual void handleDeselect();
	virtual LLTool* getOverrideTool(MASK mask);

	LL_INLINE LLPickInfo& getPick()						{ return mPick; }
	LL_INLINE U8 getClickAction()						{ return mClickAction; }
	LL_INLINE LLViewerObject* getClickActionObject()	{ return mClickActionObject; }

	LL_INLINE LLObjectSelection* getLeftClickSelection()
	{
		return (LLObjectSelection*)mLeftClickSelection;
	}

	void resetSelection();

	static void leftMouseCallback(const LLPickInfo& pick_info);
	static void rightMouseCallback(const LLPickInfo& pick_info);

	static void selectionPropertiesReceived();


private:
	bool handleLeftClickPick();
	bool handleRightClickPick();

	bool useClickAction(MASK mask, LLViewerObject* object,
						LLViewerObject* parent);

	bool handleMediaClick(const LLPickInfo& info);
	bool handleMediaDblClick(const LLPickInfo& info);
	bool handleMediaHover(const LLPickInfo& info);

private:
	LLPointer<LLViewerObject>		mClickActionObject;
	LLSafeHandle<LLObjectSelection>	mLeftClickSelection;
	LLPickInfo						mPick;
	U8								mClickAction;
	bool							mPieMouseButtonDown;
	bool							mGrabMouseButtonDown;
};

#endif

/**
 * @file lltoolmgr.h
 * @brief LLToolMgr class header file
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

#ifndef LL_TOOLMGR_H
#define LL_TOOLMGR_H

#include "llkeyboard.h"
#include "llsingleton.h"

class LLTool;
class LLToolset;

// Key bindings for common operations
const MASK MASK_VERTICAL		= MASK_CONTROL;
const MASK MASK_SPIN			= MASK_CONTROL | MASK_SHIFT;
const MASK MASK_ZOOM			= MASK_NONE;
const MASK MASK_ORBIT			= MASK_CONTROL;
const MASK MASK_PAN				= MASK_CONTROL | MASK_SHIFT;
const MASK MASK_COPY			= MASK_SHIFT;

class LLToolMgr : public LLSingleton<LLToolMgr>
{
	friend class LLSingleton<LLToolMgr>;

protected:
	LOG_CLASS(LLToolMgr);

public:
	LLToolMgr();
	~LLToolMgr();

	// Must be called after gSavedSettings set up.
	void			initTools();

	// returns active tool, taking into account keyboard state
	LLTool*			getCurrentTool();
	// returns active tool when overrides are deactivated
	LL_INLINE LLTool* getBaseTool()				{ return mBaseTool; }

	bool			inEdit();
	void			toggleBuildMode();
	// Determines if we are in Build mode or not
	bool			inBuildMode();

	void			setTransientTool(LLTool* tool);
	void			clearTransientTool();
	LL_INLINE bool	usingTransientTool()		{ return mTransientTool != NULL; }


	void			setCurrentToolset(LLToolset* current);
	LL_INLINE LLToolset* getCurrentToolset()	{ return mCurrentToolset; }

	void			onAppFocusGained();
	void			onAppFocusLost();

	LL_INLINE void	clearSavedTool()			{ mSavedTool = NULL; }

protected:
	friend class LLToolset;	// To allow access to setCurrentTool();

	void			setCurrentTool(LLTool* tool);

	// Calls getcurrenttool() to calculate active tool and call handleSelect()
	// and handleDeselect() immediately when active tool changes
	LL_INLINE void	updateToolStatus()			{ getCurrentTool(); }

protected:
	LLTool*			mBaseTool;
	// The current tool at the time application focus was lost:
	LLTool*			mSavedTool;
	LLTool*			mTransientTool;
	LLTool*			mOverrideTool;	// Tool triggered by keyboard override
	LLTool*			mSelectedTool;	// Last known active tool
	LLToolset*		mCurrentToolset;
};

// Sets of tools for various modes
class LLToolset
{
public:
	LLToolset() : mSelectedTool(NULL)			{}

	LL_INLINE LLTool* getSelectedTool()			{ return mSelectedTool; }

	void			addTool(LLTool* tool);

	void			selectTool(LLTool* tool);
	void			selectToolByIndex(S32 index);
	void			selectFirstTool();
	void			selectNextTool();
	void			selectPrevTool();

	void			handleScrollWheel(S32 clicks);

	bool			isToolSelected(S32 index);

protected:
	LLTool*			mSelectedTool;
	typedef std::vector<LLTool*> tool_list_t;
	tool_list_t 	mToolList;
};

// Handy callbacks for switching tools
void select_tool(void *tool);

// Globals

extern LLToolset*	gBasicToolset;
extern LLToolset*	gCameraToolset;
//extern LLToolset*	gLandToolset;
extern LLToolset*	gMouselookToolset;
extern LLToolset*	gFaceEditToolset;

extern LLTool*		gToolNull;

#endif

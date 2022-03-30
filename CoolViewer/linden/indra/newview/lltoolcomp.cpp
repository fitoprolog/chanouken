/**
 * @file lltoolcomp.cpp
 * @brief Composite tools
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

#include "llviewerprecompiledheaders.h"

#include "lltoolcomp.h"

#include "llmenugl.h"			// For right-click menu hack
#include "llgl.h"

#include "llagent.h"
#include "llfloatertools.h"
#include "llmanip.h"
#include "llmaniprotate.h"
#include "llmanipscale.h"
#include "llmaniptranslate.h"
#include "llselectmgr.h"
#include "lltoolfocus.h"
#include "lltoolgrab.h"
#include "lltoolgun.h"
#include "lltoolmgr.h"
#include "lltoolplacer.h"
#include "lltoolselectrect.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewerobject.h"
#include "llviewerwindow.h"

// We use this in various places instead of NULL
static LLPointer<LLTool> sNullTool(new LLTool("null", NULL));

//-----------------------------------------------------------------------------
// LLToolComposite
//-----------------------------------------------------------------------------

//static
void LLToolComposite::setCurrentTool(LLTool* new_tool)
{
	if (mCur != new_tool)
	{
		if (mSelected)
		{
			mCur->handleDeselect();
			mCur = new_tool;
			mCur->handleSelect();
		}
		else
		{
			mCur = new_tool;
		}
	}
}

LLToolComposite::LLToolComposite(const std::string& name)
:	LLTool(name),
	mCur(sNullTool),
	mDefault(sNullTool),
	mSelected(false),
	mMouseDown(false),
	mManip(NULL),
	mSelectRect(NULL)
{
}

// Returns to the default tool
bool LLToolComposite::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = mCur->handleMouseUp(x, y, mask);
	if (handled)
	{
		setCurrentTool(mDefault);
	}
	return handled;
}

void LLToolComposite::onMouseCaptureLost()
{
	mCur->onMouseCaptureLost();
	setCurrentTool(mDefault);
}

bool LLToolComposite::isSelecting()
{
	return mCur == mSelectRect;
}

void LLToolComposite::handleSelect()
{
	if (!gSavedSettings.getBool("EditLinkedParts"))
	{
		gSelectMgr.promoteSelectionToRoot();
	}
	mCur = mDefault;
	mCur->handleSelect();
	mSelected = true;
}

//-----------------------------------------------------------------------------
// LLToolCompInspect
//-----------------------------------------------------------------------------

LLToolCompInspect::LLToolCompInspect()
:	LLToolComposite("Inspect")
{
	mSelectRect = new LLToolSelectRect(this);
	mDefault = mSelectRect;
}

LLToolCompInspect::~LLToolCompInspect()
{
	delete mSelectRect;
	mSelectRect = NULL;
}

bool LLToolCompInspect::handleMouseDown(S32 x, S32 y, MASK mask)
{
	mMouseDown = true;
	gViewerWindowp->pickAsync(x, y, mask, pickCallback);
	return true;
}

void LLToolCompInspect::pickCallback(const LLPickInfo& pick_info)
{
	LLViewerObject* hit_obj = pick_info.getObject();

	LLToolCompInspect* toolinsp = LLToolCompInspect::getInstance();
	if (!toolinsp->mMouseDown)
	{
		// fast click on object, but mouse is already up... just do select
		toolinsp->mSelectRect->handleObjectSelection(pick_info,
													 gSavedSettings.getBool("EditLinkedParts"),
													 false);
		return;
	}

	if (hit_obj)
	{
		if (gSelectMgr.getSelection()->getObjectCount())
		{
			LLEditMenuHandler::gEditMenuHandler = &gSelectMgr;
		}
		toolinsp->setCurrentTool(toolinsp->mSelectRect);
		toolinsp->mSelectRect->handlePick(pick_info);

	}
	else
	{
		toolinsp->setCurrentTool(toolinsp->mSelectRect);
		toolinsp->mSelectRect->handlePick(pick_info);
	}
}

bool LLToolCompInspect::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	return true;
}

//-----------------------------------------------------------------------------
// LLToolCompTranslate
//-----------------------------------------------------------------------------

LLToolCompTranslate::LLToolCompTranslate()
:	LLToolComposite("Move")
{
	mManip = new LLManipTranslate(this);
	mSelectRect = new LLToolSelectRect(this);

	mCur = mManip;
	mDefault = mManip;
}

LLToolCompTranslate::~LLToolCompTranslate()
{
	delete mManip;
	mManip = NULL;

	delete mSelectRect;
	mSelectRect = NULL;
}

bool LLToolCompTranslate::handleHover(S32 x, S32 y, MASK mask)
{
	if (!mCur->hasMouseCapture())
	{
		setCurrentTool(mManip);
	}
	return mCur->handleHover(x, y, mask);
}

bool LLToolCompTranslate::handleMouseDown(S32 x, S32 y, MASK mask)
{
	mMouseDown = true;
	gViewerWindowp->pickAsync(x, y, mask, pickCallback, true);
	return true;
}

void LLToolCompTranslate::pickCallback(const LLPickInfo& pick_info)
{
	LLViewerObject* hit_obj = pick_info.getObject();

	LLToolCompTranslate* tooltrans = LLToolCompTranslate::getInstance();
	tooltrans->mManip->highlightManipulators(pick_info.mMousePt.mX, pick_info.mMousePt.mY);
	if (!tooltrans->mMouseDown)
	{
		// fast click on object, but mouse is already up...just do select
		tooltrans->mSelectRect->handleObjectSelection(pick_info,
													  gSavedSettings.getBool("EditLinkedParts"),
													  false);
		return;
	}

	if (hit_obj ||
		tooltrans->mManip->getHighlightedPart() != LLManip::LL_NO_PART)
	{
		if (tooltrans->mManip->getSelection()->getObjectCount())
		{
			LLEditMenuHandler::gEditMenuHandler = &gSelectMgr;
		}

		bool can_move = tooltrans->mManip->canAffectSelection();

		if (can_move &&
			LLManip::LL_NO_PART != tooltrans->mManip->getHighlightedPart())
		{
			tooltrans->setCurrentTool(tooltrans->mManip);
			tooltrans->mManip->handleMouseDownOnPart(pick_info.mMousePt.mX,
													 pick_info.mMousePt.mY,
													 pick_info.mKeyMask);
		}
		else
		{
			tooltrans->setCurrentTool(tooltrans->mSelectRect);
			tooltrans->mSelectRect->handlePick(pick_info);

			// *TODO: add toggle to trigger old click-drag functionality
			// tooltrans->mManip->handleMouseDownOnPart(XY_part, x, y, mask);
		}
	}
	else
	{
		tooltrans->setCurrentTool(tooltrans->mSelectRect);
		tooltrans->mSelectRect->handlePick(pick_info);
	}
}

bool LLToolCompTranslate::handleMouseUp(S32 x, S32 y, MASK mask)
{
	mMouseDown = false;
	return LLToolComposite::handleMouseUp(x, y, mask);
}

LLTool* LLToolCompTranslate::getOverrideTool(MASK mask)
{
	if (mask == MASK_CONTROL)
	{
		return LLToolCompRotate::getInstance();
	}
	else if (mask == (MASK_CONTROL | MASK_SHIFT))
	{
		return LLToolCompScale::getInstance();
	}
	return LLToolComposite::getOverrideTool(mask);
}

bool LLToolCompTranslate::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (mManip->getSelection()->isEmpty() &&
		mManip->getHighlightedPart() == LLManip::LL_NO_PART)
	{
		// You should already have an object selected from the mousedown.
		// If so, show its properties
		if (gFloaterToolsp)
		{
			gFloaterToolsp->showPanel(LLFloaterTools::PANEL_CONTENTS);
		}
		return true;
	}
	// Nothing selected means the first mouse click was probably bad, so try
	// again. This also consumes the event to prevent things like double-click
	// teleport from triggering.
	return handleMouseDown(x, y, mask);
}

void LLToolCompTranslate::render()
{
	mCur->render(); // removing this will not draw the RGB arrows and guidelines

	if (mCur != mManip)
	{
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		mManip->renderGuidelines();
	}
}

//-----------------------------------------------------------------------------
// LLToolCompScale
//-----------------------------------------------------------------------------

LLToolCompScale::LLToolCompScale()
:	LLToolComposite("Stretch")
{
	mManip = new LLManipScale(this);
	mSelectRect = new LLToolSelectRect(this);

	mCur = mManip;
	mDefault = mManip;
}

LLToolCompScale::~LLToolCompScale()
{
	delete mManip;
	delete mSelectRect;
}

bool LLToolCompScale::handleHover(S32 x, S32 y, MASK mask)
{
	if (!mCur->hasMouseCapture())
	{
		setCurrentTool(mManip);
	}
	return mCur->handleHover(x, y, mask);
}

bool LLToolCompScale::handleMouseDown(S32 x, S32 y, MASK mask)
{
	mMouseDown = true;
	gViewerWindowp->pickAsync(x, y, mask, pickCallback);
	return true;
}

void LLToolCompScale::pickCallback(const LLPickInfo& pick_info)
{
	LLViewerObject* hit_obj = pick_info.getObject();

	LLToolCompScale* toolscale = LLToolCompScale::getInstance();
	toolscale->mManip->highlightManipulators(pick_info.mMousePt.mX,
											 pick_info.mMousePt.mY);
	if (!toolscale->mMouseDown)
	{
		// fast click on object, but mouse is already up... just do select
		toolscale->mSelectRect->handleObjectSelection(pick_info,
													  gSavedSettings.getBool("EditLinkedParts"),
													  false);

		return;
	}

	if (hit_obj ||
		toolscale->mManip->getHighlightedPart() != LLManip::LL_NO_PART)
	{
		if (toolscale->mManip->getSelection()->getObjectCount())
		{
			LLEditMenuHandler::gEditMenuHandler = &gSelectMgr;
		}
		if (LLManip::LL_NO_PART != toolscale->mManip->getHighlightedPart())
		{
			toolscale->setCurrentTool(toolscale->mManip);
			toolscale->mManip->handleMouseDownOnPart(pick_info.mMousePt.mX,
													 pick_info.mMousePt.mY,
													 pick_info.mKeyMask);
		}
		else
		{
			toolscale->setCurrentTool(toolscale->mSelectRect);
			toolscale->mSelectRect->handlePick(pick_info);
		}
	}
	else
	{
		toolscale->setCurrentTool(toolscale->mSelectRect);
		toolscale->mSelectRect->handlePick(pick_info);
	}
}

bool LLToolCompScale::handleMouseUp(S32 x, S32 y, MASK mask)
{
	mMouseDown = false;
	return LLToolComposite::handleMouseUp(x, y, mask);
}

LLTool* LLToolCompScale::getOverrideTool(MASK mask)
{
	if (mask == MASK_CONTROL)
	{
		return LLToolCompRotate::getInstance();
	}

	return LLToolComposite::getOverrideTool(mask);
}

bool LLToolCompScale::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (!mManip->getSelection()->isEmpty() &&
		mManip->getHighlightedPart() == LLManip::LL_NO_PART)
	{
		// You should already have an object selected from the mousedown.
		// If so, show its properties
		if (gFloaterToolsp)
		{
			gFloaterToolsp->showPanel(LLFloaterTools::PANEL_CONTENTS);
		}
		return true;
	}
	// Nothing selected means the first mouse click was probably bad, so try
	// again.
	return handleMouseDown(x, y, mask);
}

void LLToolCompScale::render()
{
	mCur->render();

	if (mCur != mManip)
	{
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		mManip->renderGuidelines();
	}
}

//-----------------------------------------------------------------------------
// LLToolCompCreate
//-----------------------------------------------------------------------------

LLToolCompCreate::LLToolCompCreate()
:	LLToolComposite("Create")
{
	mPlacer = new LLToolPlacer();
	mSelectRect = new LLToolSelectRect(this);

	mCur = mPlacer;
	mDefault = mPlacer;
	mObjectPlacedOnMouseDown = false;
}

LLToolCompCreate::~LLToolCompCreate()
{
	delete mPlacer;
	delete mSelectRect;
}

bool LLToolCompCreate::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = false;
	mMouseDown = true;

	if (mask == MASK_SHIFT || mask == MASK_CONTROL)
	{
		gViewerWindowp->pickAsync(x, y, mask, pickCallback);
		handled = true;
	}
	else
	{
		setCurrentTool(mPlacer);
		handled = mPlacer->placeObject(x, y, mask);
	}

	mObjectPlacedOnMouseDown = true;

	return handled;
}

void LLToolCompCreate::pickCallback(const LLPickInfo& pick_info)
{
	// *NOTE: We mask off shift and control, so you cannot
	// multi-select multiple objects with the create tool.
	MASK mask = (pick_info.mKeyMask & ~MASK_SHIFT);
	mask = (mask & ~MASK_CONTROL);

	LLToolCompCreate::getInstance()->setCurrentTool(LLToolCompCreate::getInstance()->mSelectRect);
	LLToolCompCreate::getInstance()->mSelectRect->handlePick(pick_info);
}

bool LLToolCompCreate::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	return handleMouseDown(x, y, mask);
}

bool LLToolCompCreate::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	if (mMouseDown && !mObjectPlacedOnMouseDown && mask != MASK_SHIFT &&
		mask != MASK_CONTROL)
	{
		setCurrentTool(mPlacer);
		handled = mPlacer->placeObject(x, y, mask);
	}

	mObjectPlacedOnMouseDown = false;
	mMouseDown = false;

	if (!handled)
	{
		handled = LLToolComposite::handleMouseUp(x, y, mask);
	}

	return handled;
}

//-----------------------------------------------------------------------------
// LLToolCompRotate
//-----------------------------------------------------------------------------

LLToolCompRotate::LLToolCompRotate()
:	LLToolComposite("Rotate")
{
	mManip = new LLManipRotate(this);
	mSelectRect = new LLToolSelectRect(this);

	mCur = mManip;
	mDefault = mManip;
}

LLToolCompRotate::~LLToolCompRotate()
{
	delete mManip;
	delete mSelectRect;
}

bool LLToolCompRotate::handleHover(S32 x, S32 y, MASK mask)
{
	if (!mCur->hasMouseCapture())
	{
		setCurrentTool(mManip);
	}
	return mCur->handleHover(x, y, mask);
}

bool LLToolCompRotate::handleMouseDown(S32 x, S32 y, MASK mask)
{
	mMouseDown = true;
	gViewerWindowp->pickAsync(x, y, mask, pickCallback);
	return true;
}

void LLToolCompRotate::pickCallback(const LLPickInfo& pick_info)
{
	LLViewerObject* hit_obj = pick_info.getObject();

	LLToolCompRotate* toolrot = LLToolCompRotate::getInstance();
	toolrot->mManip->highlightManipulators(pick_info.mMousePt.mX,
										   pick_info.mMousePt.mY);
	if (!toolrot->mMouseDown)
	{
		// fast click on object, but mouse is already up... just do select
		toolrot->mSelectRect->handleObjectSelection(pick_info,
													gSavedSettings.getBool("EditLinkedParts"),
													false);
		return;
	}

	if (hit_obj ||
		toolrot->mManip->getHighlightedPart() != LLManip::LL_NO_PART)
	{
		if (toolrot->mManip->getSelection()->getObjectCount())
		{
			LLEditMenuHandler::gEditMenuHandler = &gSelectMgr;
		}
		if (LLManip::LL_NO_PART != toolrot->mManip->getHighlightedPart())
		{
			toolrot->setCurrentTool(toolrot->mManip);
			toolrot->mManip->handleMouseDownOnPart(pick_info.mMousePt.mX,
												   pick_info.mMousePt.mY,
												   pick_info.mKeyMask);
		}
		else
		{
			toolrot->setCurrentTool(toolrot->mSelectRect);
			toolrot->mSelectRect->handlePick(pick_info);
		}
	}
	else
	{
		toolrot->setCurrentTool(toolrot->mSelectRect);
		toolrot->mSelectRect->handlePick(pick_info);
	}
}

bool LLToolCompRotate::handleMouseUp(S32 x, S32 y, MASK mask)
{
	mMouseDown = false;
	return LLToolComposite::handleMouseUp(x, y, mask);
}

LLTool* LLToolCompRotate::getOverrideTool(MASK mask)
{
	if (mask == (MASK_CONTROL | MASK_SHIFT))
	{
		return LLToolCompScale::getInstance();
	}
	return LLToolComposite::getOverrideTool(mask);
}

bool LLToolCompRotate::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (!mManip->getSelection()->isEmpty() &&
		mManip->getHighlightedPart() == LLManip::LL_NO_PART)
	{
		// You should already have an object selected from the mousedown.
		// If so, show its properties
		if (gFloaterToolsp)
		{
			gFloaterToolsp->showPanel(LLFloaterTools::PANEL_CONTENTS);
		}
		return true;
	}
	// Nothing selected means the first mouse click was probably bad, so try
	// again.
	return handleMouseDown(x, y, mask);
}

void LLToolCompRotate::render()
{
	mCur->render();

	if (mCur != mManip)
	{
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		mManip->renderGuidelines();
	}
}

//-----------------------------------------------------------------------------
// LLToolCompGun
//-----------------------------------------------------------------------------

LLToolCompGun::LLToolCompGun()
:	LLToolComposite("Mouselook")
{
	mGun = new LLToolGun(this);
	mGrab = new LLToolGrabBase(this);
	mNull = sNullTool;

	setCurrentTool(mGun);
	mDefault = mGun;
}

LLToolCompGun::~LLToolCompGun()
{
	delete mGun;
	mGun = NULL;

	delete mGrab;
	mGrab = NULL;

#if 0	// don't delete a static object
	 delete mNull;
#endif
	mNull = NULL;
}

bool LLToolCompGun::handleHover(S32 x, S32 y, MASK mask)
{
	// Note: if the tool changed, we can't delegate the current mouse event
	// after the change because tools can modify the mouse during selection and
	// deselection.
	// Instead we let the current tool handle the event and then make the change.
	// The new tool will take effect on the next frame.

	mCur->handleHover(x, y, mask);

	// If mouse button not down...
	if (!gViewerWindowp->getLeftMouseDown())
	{
		// let ALT switch from gun to grab
		if (mCur == mGun && (mask & MASK_ALT))
		{
			setCurrentTool((LLTool*)mGrab);
		}
		else if (mCur == mGrab && !(mask & MASK_ALT))
		{
			setCurrentTool((LLTool*)mGun);
			setMouseCapture(true);
		}
	}

	return true;
}

bool LLToolCompGun::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// if the left button is grabbed, don't put up the pie menu
	if (gAgent.leftButtonGrabbed())
	{
		gAgent.setControlFlags(AGENT_CONTROL_ML_LBUTTON_DOWN);
		return false;
	}

	// On mousedown, start grabbing
	gGrabTransientTool = this;
	LLToolMgr::getInstance()->getCurrentToolset()->selectTool((LLTool*)mGrab);

	return LLToolGrab::getInstance()->handleMouseDown(x, y, mask);
}

bool LLToolCompGun::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	// if the left button is grabbed, don't put up the pie menu
	if (gAgent.leftButtonGrabbed())
	{
		gAgent.setControlFlags(AGENT_CONTROL_ML_LBUTTON_DOWN);
		return false;
	}

	// On mousedown, start grabbing
	gGrabTransientTool = this;
	LLToolMgr::getInstance()->getCurrentToolset()->selectTool((LLTool*)mGrab);

	return LLToolGrab::getInstance()->handleDoubleClick(x, y, mask);
}

bool LLToolCompGun::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
#if 0	// JC - suppress context menu 8/29/2002

	// On right mouse, go through some convoluted steps to make the build menu
	// appear.
	setCurrentTool((LLTool*)mNull);

	// This should return false, meaning the context menu will be shown.
	return false;
#else
	// Returning true suppresses the context menu
	return true;
#endif
}

bool LLToolCompGun::handleMouseUp(S32 x, S32 y, MASK mask)
{
	gAgent.setControlFlags(AGENT_CONTROL_ML_LBUTTON_UP);
	setCurrentTool((LLTool*)mGun);
	return true;
}

void LLToolCompGun::onMouseCaptureLost()
{
	if (mComposite)
	{
		mComposite->onMouseCaptureLost();
		return;
	}
	mCur->onMouseCaptureLost();
}

void LLToolCompGun::handleSelect()
{
	LLToolComposite::handleSelect();
	setMouseCapture(true);
}

void LLToolCompGun::handleDeselect()
{
	LLToolComposite::handleDeselect();
	setMouseCapture(false);
}

bool LLToolCompGun::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (clicks > 0)
	{
		gAgent.changeCameraToDefault();
	}
	return true;
}

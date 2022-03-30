/**
 * @file lltoolgun.cpp
 * @brief LLToolGun class implementation
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

#include "lltoolgun.h"

#include "llfontgl.h"
#include "lllocale.h"
#include "llui.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llhudmanager.h"
#include "llsky.h"
#include "lltoolmgr.h"
#include "lltoolgrab.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llviewercamera.h"
#include "llviewerwindow.h"

LLToolGun::LLToolGun(LLToolComposite* composite)
:	LLTool("gun", composite),
	mIsSelected(false)
{
}

void LLToolGun::handleSelect()
{
	if (gViewerWindowp)
	{
		gViewerWindowp->hideCursor();
		gViewerWindowp->moveCursorToCenter();
		gWindowp->setMouseClipping(true);
	}
	mIsSelected = true;
}

void LLToolGun::handleDeselect()
{
	if (gViewerWindowp)
	{
		gViewerWindowp->moveCursorToCenter();
		gViewerWindowp->showCursor();
		gWindowp->setMouseClipping(false);
	}
	mIsSelected = false;
}

bool LLToolGun::handleMouseDown(S32 x, S32 y, MASK mask)
{
	gGrabTransientTool = this;

	LLToolMgr* mgr = LLToolMgr::getInstance();
	mgr->getCurrentToolset()->selectTool(LLToolGrab::getInstance());

	return LLToolGrab::getInstance()->handleMouseDown(x, y, mask);
}

bool LLToolGun::handleHover(S32 x, S32 y, MASK mask)
{
	if (!gViewerWindowp) return false;

	if (mIsSelected && gAgent.cameraMouselook())
	{
		constexpr F32 NOMINAL_MOUSE_SENSITIVITY = 0.0025f;

		static LLCachedControl<F32> sensitivity(gSavedSettings,
												"MouseSensitivity");
		F32 mouse_sensitivity = clamp_rescale((F32)sensitivity,
											  0.f, 15.f, 0.5f, 2.75f)
								* NOMINAL_MOUSE_SENSITIVITY;

		// Move the view with the mouse

		// Get mouse movement delta
		S32 dx = -gViewerWindowp->getCurrentMouseDX();
		S32 dy = -gViewerWindowp->getCurrentMouseDY();

		if (dx != 0 || dy != 0)
		{
			// ...actually moved off center
			static LLCachedControl<bool> invert_mouse(gSavedSettings,
													  "InvertMouse");
			if (invert_mouse)
			{
				gAgent.pitch(mouse_sensitivity * -dy);
			}
			else
			{
				gAgent.pitch(mouse_sensitivity * dy);
			}
			LLVector3 skyward = gAgent.getReferenceUpVector();
			gAgent.rotate(mouse_sensitivity * dx, skyward.mV[VX],
						  skyward.mV[VY], skyward.mV[VZ]);

			gViewerWindowp->moveCursorToCenter();
			gViewerWindowp->hideCursor();
		}

		LL_DEBUGS("UserInput") << "hover handled by LLToolGun (mouselook)"
							   << LL_ENDL;
	}
	else
	{
		LL_DEBUGS("UserInput") << "hover handled by LLToolGun (not mouselook)"
							   << LL_ENDL;
	}

	// *HACK to avoid assert: error checking system makes sure that the cursor
	// is set during every handleHover. This is actually a no-op since the
	// cursor is hidden.
	gViewerWindowp->setCursor(UI_CURSOR_ARROW);

	return true;
}

void LLToolGun::draw()
{
	if (!gViewerWindowp) return;

	static LLUIImagePtr crosshair = LLUI::getUIImage("UIImgCrosshairsUUID");
	if (crosshair.isNull())
	{
		llerrs << "Missing cross-hair image: verify the viewer installation !"
			   << llendl;
	}
	static S32 image_width = crosshair->getWidth();
	static S32 image_height = crosshair->getHeight();

	static LLCachedControl<bool> show_crosshairs(gSavedSettings,
												 "ShowCrosshairs");
	if (show_crosshairs)
	{
		crosshair->draw((gViewerWindowp->getWindowWidth() - image_width) / 2,
						(gViewerWindowp->getWindowHeight() -
						 image_height) / 2);
	}
}

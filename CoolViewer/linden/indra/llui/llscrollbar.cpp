/**
 * @file llscrollbar.cpp
 * @brief Scrollbar UI widget
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

#include "linden_common.h"

#include "llscrollbar.h"

#include "llbutton.h"
#include "llcontrol.h"
#include "llcriticaldamp.h"
#include "llkeyboard.h"
#include "llrender.h"
#include "lltimer.h"
#include "llwindow.h"
#include "llcolor3.h"

LLScrollbar::LLScrollbar(const std::string& name, LLRect rect,
						 LLScrollbar::ORIENTATION orientation,
						 S32 doc_size, S32 doc_pos, S32 page_size,
						 void (*change_callback)(S32, LLScrollbar*, void*),
						 void* callback_user_data, S32 step_size)
:	LLUICtrl(name, rect, true, NULL, NULL),
	mChangeCallback(change_callback),
	mCallbackUserData(callback_user_data),
	mOrientation(orientation),
	mDocSize(doc_size),
	mDocPos(doc_pos),
	mPageSize(page_size),
	mStepSize(step_size),
	mDocChanged(false),
	mDragStartX(0),
	mDragStartY(0),
	mHoverGlowStrength(0.15f),
	mCurGlowStrength(0.f),
	mTrackColor(LLUI::sScrollbarTrackColor),
	mThumbColor(LLUI::sScrollbarThumbColor),
	mHighlightColor(LLUI::sDefaultHighlightLight),
	mShadowColor(LLUI::sDefaultShadowLight),
	mOnScrollEndCallback(NULL),
	mOnScrollEndData(NULL)
{
	setTabStop(false);
	updateThumbRect();

	// Page up and page down buttons
	LLRect line_up_rect;
	std::string line_up_img;
	std::string line_up_selected_img;
	std::string line_down_img;
	std::string line_down_selected_img;

	LLRect line_down_rect;

	if (LLScrollbar::VERTICAL == mOrientation)
	{
		line_up_rect.setLeftTopAndSize(0, getRect().getHeight(),
									   SCROLLBAR_SIZE, SCROLLBAR_SIZE);
		line_up_img = "UIImgBtnScrollUpOutUUID";
		line_up_selected_img = "UIImgBtnScrollUpInUUID";

		line_down_rect.setOriginAndSize(0, 0, SCROLLBAR_SIZE, SCROLLBAR_SIZE);
		line_down_img = "UIImgBtnScrollDownOutUUID";
		line_down_selected_img = "UIImgBtnScrollDownInUUID";
	}
	else
	{
		// Horizontal
		line_up_rect.setOriginAndSize(0, 0, SCROLLBAR_SIZE, SCROLLBAR_SIZE);
		line_up_img = "UIImgBtnScrollLeftOutUUID";
		line_up_selected_img = "UIImgBtnScrollLeftInUUID";

		line_down_rect.setOriginAndSize(getRect().getWidth() - SCROLLBAR_SIZE,
										0, SCROLLBAR_SIZE, SCROLLBAR_SIZE);
		line_down_img = "UIImgBtnScrollRightOutUUID";
		line_down_selected_img = "UIImgBtnScrollRightInUUID";
	}

	LLButton* line_up_btn = new LLButton("Line Up", line_up_rect, line_up_img,
										 line_up_selected_img, NULL,
										 &LLScrollbar::onLineUpBtnPressed,
										 this, LLFontGL::getFontSansSerif());
	if (LLScrollbar::VERTICAL == mOrientation)
	{
		line_up_btn->setFollowsRight();
		line_up_btn->setFollowsTop();
	}
	else
	{
		// horizontal
		line_up_btn->setFollowsLeft();
		line_up_btn->setFollowsBottom();
	}
	line_up_btn->setHeldDownCallback(&LLScrollbar::onLineUpBtnPressed);
	line_up_btn->setTabStop(false);
	line_up_btn->setScaleImage(true);

	addChild(line_up_btn);

	LLButton* line_down_btn =
		new LLButton("Line Down", line_down_rect, line_down_img,
					 line_down_selected_img, NULL,
					 &LLScrollbar::onLineDownBtnPressed, this,
					 LLFontGL::getFontSansSerif());
	line_down_btn->setFollowsRight();
	line_down_btn->setFollowsBottom();
	line_down_btn->setHeldDownCallback(&LLScrollbar::onLineDownBtnPressed);
	line_down_btn->setTabStop(false);
	line_down_btn->setScaleImage(true);
	addChild(line_down_btn);
}

LLScrollbar::~LLScrollbar()
{
	// Children buttons killed by parent class
}

void LLScrollbar::setDocParams(S32 size, S32 pos)
{
	mDocSize = size;
	setDocPos(pos);
	mDocChanged = true;

	updateThumbRect();
}

void LLScrollbar::setDocPos(S32 pos, bool update_thumb)
{
	pos = llclamp(pos, 0, getDocPosMax());
	if (pos != mDocPos)
	{
		mDocPos = pos;
		mDocChanged = true;

		if (mChangeCallback)
		{
			mChangeCallback(mDocPos, this, mCallbackUserData);
		}

		if (update_thumb)
		{
			updateThumbRect();
		}
	}
}

void LLScrollbar::setDocSize(S32 size)
{
	if (size != mDocSize)
	{
		mDocSize = size;
		setDocPos(mDocPos);
		mDocChanged = true;

		updateThumbRect();
	}
}

void LLScrollbar::setPageSize(S32 page_size)
{
	if (page_size != mPageSize)
	{
		mPageSize = page_size;
		setDocPos(mDocPos);
		mDocChanged = true;

		updateThumbRect();
	}
}

void LLScrollbar::updateThumbRect()
{
	constexpr S32 THUMB_MIN_LENGTH = 16;

	S32 window_length =
		mOrientation == LLScrollbar::HORIZONTAL ? getRect().getWidth()
												: getRect().getHeight();
	S32 thumb_bg_length = llmax(0, window_length - 2 * SCROLLBAR_SIZE);
	S32 visible_lines = llmin(mDocSize, mPageSize);
	S32 thumb_length =
		mDocSize ? llmin(llmax(visible_lines * thumb_bg_length / mDocSize,
							   THUMB_MIN_LENGTH),
						 thumb_bg_length)
				 : thumb_bg_length;

	S32 variable_lines = mDocSize - visible_lines;

	if (mOrientation == LLScrollbar::VERTICAL)
	{
		S32 thumb_start_max = thumb_bg_length + SCROLLBAR_SIZE;
		S32 thumb_start_min = SCROLLBAR_SIZE + THUMB_MIN_LENGTH;
		S32 thumb_start =
			variable_lines ? llmin(llmax(thumb_start_max - mDocPos *
										 (thumb_bg_length - thumb_length) /
										  variable_lines, thumb_start_min),
								   thumb_start_max)
						   : thumb_start_max;

		mThumbRect.mLeft =  0;
		mThumbRect.mTop = thumb_start;
		mThumbRect.mRight = SCROLLBAR_SIZE;
		mThumbRect.mBottom = thumb_start - thumb_length;
	}
	else
	{
		// Horizontal
		S32 thumb_start_max = thumb_bg_length + SCROLLBAR_SIZE - thumb_length;
		S32 thumb_start_min = SCROLLBAR_SIZE;
		S32 thumb_start =
			variable_lines ? llmin(llmax(thumb_start_min + mDocPos *
										 (thumb_bg_length - thumb_length) /
										 variable_lines, thumb_start_min),
								   thumb_start_max)
						   : thumb_start_min;

		mThumbRect.mLeft = thumb_start;
		mThumbRect.mTop = SCROLLBAR_SIZE;
		mThumbRect.mRight = thumb_start + thumb_length;
		mThumbRect.mBottom = 0;
	}

	if (mOnScrollEndCallback && mOnScrollEndData && mDocPos == getDocPosMax())
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}
}

bool LLScrollbar::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Check children first
	if (LLView::childrenHandleMouseDown(x, y, mask) == NULL)
	{
		if (mThumbRect.pointInRect(x, y))
		{
			// Start dragging the thumb. No handler needed for focus lost since
			// this clas has no state that depends on it.
			gFocusMgr.setMouseCapture(this);
			mDragStartX = x;
			mDragStartY = y;
			mOrigRect.mTop = mThumbRect.mTop;
			mOrigRect.mBottom = mThumbRect.mBottom;
			mOrigRect.mLeft = mThumbRect.mLeft;
			mOrigRect.mRight = mThumbRect.mRight;
			mLastDelta = 0;
		}
		else
		{
			if ((LLScrollbar::VERTICAL == mOrientation &&
				 mThumbRect.mTop < y) ||
				(LLScrollbar::HORIZONTAL == mOrientation &&
				 x < mThumbRect.mLeft))
			{
				// Page up
				pageUp(0);
			}
			else
			if ((LLScrollbar::VERTICAL == mOrientation &&
				 y < mThumbRect.mBottom) ||
				(LLScrollbar::HORIZONTAL == mOrientation &&
				 mThumbRect.mRight < x))
			{
				// Page down
				pageDown(0);
			}
		}
	}

	return true;
}

bool LLScrollbar::handleHover(S32 x, S32 y, MASK mask)
{
	// Note: we do not bother sending the event to the children (the arrow
	// buttons) because they will capture the mouse whenever they need hover
	// events.

	bool handled = false;
	if (hasMouseCapture())
	{
		S32 height = getRect().getHeight();
		S32 width = getRect().getWidth();

		if (VERTICAL == mOrientation)
		{
			S32 delta_pixels = y - mDragStartY;
			if (mOrigRect.mBottom + delta_pixels < SCROLLBAR_SIZE)
			{
				delta_pixels = SCROLLBAR_SIZE - mOrigRect.mBottom - 1;
			}
			else if (mOrigRect.mTop + delta_pixels > height - SCROLLBAR_SIZE)
			{
				delta_pixels = height - SCROLLBAR_SIZE - mOrigRect.mTop + 1;
			}

			mThumbRect.mTop = mOrigRect.mTop + delta_pixels;
			mThumbRect.mBottom = mOrigRect.mBottom + delta_pixels;

			S32 thumb_length = mThumbRect.getHeight();
			S32 thumb_track_length = height - 2 * SCROLLBAR_SIZE;

			if (delta_pixels != mLastDelta || mDocChanged)
			{
				// Note: delta_pixels increases as you go up. mDocPos increases
				// down (line 0 is at the top of the page).
				S32 usable_track_length = thumb_track_length - thumb_length;
				if (0 < usable_track_length)
				{
					S32 variable_lines = getDocPosMax();
					S32 pos = mThumbRect.mTop;
					F32 ratio = F32(pos - SCROLLBAR_SIZE - thumb_length) /
								usable_track_length;

					S32 new_pos = llclamp(S32(variable_lines - ratio *
											  variable_lines + 0.5f),
										  0, variable_lines);
					// Note: we do not call updateThumbRect() here. Instead we
					// let the thumb and the document go slightly out of sync
					// (less than a line's worth) to make the thumb feel
					// responsive.
					changeLine(new_pos - mDocPos, false);
				}
			}

			mLastDelta = delta_pixels;
		}
		else	// Horizontal
		{
			S32 delta_pixels = x - mDragStartX;

			if (mOrigRect.mLeft + delta_pixels < SCROLLBAR_SIZE)
			{
				delta_pixels = SCROLLBAR_SIZE - mOrigRect.mLeft - 1;
			}
			else if (mOrigRect.mRight + delta_pixels > width - SCROLLBAR_SIZE)
			{
				delta_pixels = width - SCROLLBAR_SIZE - mOrigRect.mRight + 1;
			}

			mThumbRect.mLeft = mOrigRect.mLeft + delta_pixels;
			mThumbRect.mRight = mOrigRect.mRight + delta_pixels;

			S32 thumb_length = mThumbRect.getWidth();
			S32 thumb_track_length = width - 2 * SCROLLBAR_SIZE;

			if (delta_pixels != mLastDelta || mDocChanged)
			{
				// Note: delta_pixels increases as you go up. mDocPos increases
				// down (line 0 is at the top of the page).
				S32 usable_track_length = thumb_track_length - thumb_length;
				if (0 < usable_track_length)
				{
					S32 variable_lines = getDocPosMax();
					S32 pos = mThumbRect.mLeft;
					F32 ratio = F32(pos - SCROLLBAR_SIZE) / usable_track_length;

					S32 new_pos = llclamp(S32(ratio * variable_lines + 0.5f),
										  0, variable_lines);

					// Note: we do not call updateThumbRect() here. Instead we
					// let the thumb and the document go slightly out of sync
					// (less than a line's worth) to make the thumb feel
					// responsive.
					changeLine(new_pos - mDocPos, false);
				}
			}

			mLastDelta = delta_pixels;
		}

		gWindowp->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("UserInput") << "hover handled by " << getName()
							   << " (active)" << LL_ENDL;
		handled = true;
	}
	else
	{
		handled = childrenHandleMouseUp(x, y, mask) != NULL;
	}

	// Opaque
	if (!handled)
	{
		gWindowp->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("UserInput") << "hover handled by " << getName()
							   << " (inactive)"  << LL_ENDL;
		handled = true;
	}

	mDocChanged = false;

	return handled;
}

bool LLScrollbar::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	changeLine(clicks * mStepSize, true);
	return true;
}

bool LLScrollbar::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
									EDragAndDropType cargo_type,
									void *cargo_data, EAcceptance *accept,
									std::string &tooltip_msg)
{
#if 0	// Enable this to get drag and drop to control scrollbars
	if (!drop)
	{
		// TODO: refactor this
		S32 variable_lines = getDocPosMax();
		S32 pos = VERTICAL == mOrientation ? y : x;
		S32 thumb_length =
			VERTICAL == mOrientation ? mThumbRect.getHeight()
									: mThumbRect.getWidth();
		S32 thumb_track_length =
			VERTICAL == mOrientation ? getRect().getHeight() - 2 * SCROLLBAR_SIZE
									 : getRect().getWidth() - 2 * SCROLLBAR_SIZE;
		S32 usable_track_length = thumb_track_length - thumb_length;
		F32 ratio =
			VERTICAL == mOrientation ? F32(pos - SCROLLBAR_SIZE -
										   thumb_length) / usable_track_length
									 : F32(pos - SCROLLBAR_SIZE) /
									   usable_track_length;
		S32 new_pos =
			VERTICAL == mOrientation ? llclamp(S32(variable_lines -
												   ratio * variable_lines +
												   0.5f), 0, variable_lines)
									 : llclamp(S32(ratio * variable_lines +
												   0.5f),
											   0, variable_lines);
		changeLine(new_pos - mDocPos, true);
	}
	return true;
#else
	return false;
#endif
}

bool LLScrollbar::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = false;
	if (hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(NULL);
		handled = true;
	}
	else
	{
		// Opaque, so don't just check children
		handled = LLView::handleMouseUp(x, y, mask);
	}

	return handled;
}

void LLScrollbar::reshape(S32 width, S32 height, bool called_from_parent)
{
	if (width == getRect().getWidth() && height == getRect().getHeight())
	{
		return;
	}
	LLView::reshape(width, height, called_from_parent);
	LLButton* up_button = getChild<LLButton>("Line Up");
	LLButton* down_button = getChild<LLButton>("Line Down");

	if (mOrientation == VERTICAL)
	{
		up_button->reshape(up_button->getRect().getWidth(),
						   llmin(getRect().getHeight() / 2, SCROLLBAR_SIZE));
		down_button->reshape(down_button->getRect().getWidth(),
							 llmin(getRect().getHeight() / 2, SCROLLBAR_SIZE));
		up_button->setOrigin(up_button->getRect().mLeft,
							 getRect().getHeight() - up_button->getRect().getHeight());
	}
	else
	{
		up_button->reshape(llmin(getRect().getWidth() / 2, SCROLLBAR_SIZE),
						   up_button->getRect().getHeight());
		down_button->reshape(llmin(getRect().getWidth() / 2, SCROLLBAR_SIZE),
							 down_button->getRect().getHeight());
		down_button->setOrigin(getRect().getWidth() - down_button->getRect().getWidth(),
							   down_button->getRect().mBottom);
	}
	updateThumbRect();
}

void LLScrollbar::draw()
{
	if (!getRect().isValid())
	{
		return;
	}

	S32 local_mouse_x;
	S32 local_mouse_y;
	LLUI::getCursorPositionLocal(this, &local_mouse_x, &local_mouse_y);
	bool other_captor = gFocusMgr.getMouseCapture() &&
						gFocusMgr.getMouseCapture() != this;
	bool hovered = getEnabled() && !other_captor &&
				   (hasMouseCapture() ||
					mThumbRect.pointInRect(local_mouse_x, local_mouse_y));
	if (hovered)
	{
		mCurGlowStrength = lerp(mCurGlowStrength, mHoverGlowStrength,
								LLCriticalDamp::getInterpolant(0.05f));
	}
	else
	{
		mCurGlowStrength = lerp(mCurGlowStrength, 0.f,
								LLCriticalDamp::getInterpolant(0.05f));
	}

	// Background
	if (mOrientation == HORIZONTAL)
	{
		LLUIImage::sRoundedSquare->drawSolid(SCROLLBAR_SIZE, 0,
											 getRect().getWidth() -
											 2 * SCROLLBAR_SIZE,
											 getRect().getHeight(),
											 mTrackColor);
	}
	else
	{
		LLUIImage::sRoundedSquare->drawSolid(0, SCROLLBAR_SIZE,
											 getRect().getWidth(),
											 getRect().getHeight() -
											 2 * SCROLLBAR_SIZE,
											 mTrackColor);
	}

	// Thumb
	LLRect outline_rect = mThumbRect;
	outline_rect.stretch(2);

	if (gFocusMgr.getKeyboardFocus() == this)
	{
		LLUIImage::sRoundedSquare->draw(outline_rect,
										gFocusMgr.getFocusColor());
	}

	LLUIImage::sRoundedSquare->draw(mThumbRect, mThumbColor);
	if (mCurGlowStrength > 0.01f)
	{
		gGL.setSceneBlendType(LLRender::BT_ADD_WITH_ALPHA);
		LLUIImage::sRoundedSquare->drawSolid(mThumbRect,
											 LLColor4(1.f, 1.f, 1.f,
													  mCurGlowStrength));
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
	}

	bool was_scrolled_to_bottom = getDocPos() == getDocPosMax();
	if (mOnScrollEndCallback && was_scrolled_to_bottom)
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}

	// Draw children
	LLView::draw();
}

void LLScrollbar::changeLine(S32 delta, bool update_thumb)
{
	setDocPos(mDocPos + delta, update_thumb);
}

void LLScrollbar::setValue(const LLSD& value)
{
	setDocPos((S32) value.asInteger());
}

bool LLScrollbar::handleKeyHere(KEY key, MASK mask)
{
	bool handled = false;

	switch (key)
	{
		case KEY_HOME:
			setDocPos(0);
			handled = true;
			break;

		case KEY_END:
			setDocPos(getDocPosMax());
			handled = true;
			break;

		case KEY_DOWN:
			setDocPos(getDocPos() + mStepSize);
			handled = true;
			break;

		case KEY_UP:
			setDocPos(getDocPos() - mStepSize);
			handled = true;
			break;

		case KEY_PAGE_DOWN:
			pageDown(1);
			break;

		case KEY_PAGE_UP:
			pageUp(1);
			break;
	}

	return handled;
}

void LLScrollbar::pageUp(S32 overlap)
{
	if (mDocSize > mPageSize)
	{
		changeLine(-(mPageSize - overlap), true);
	}
}

void LLScrollbar::pageDown(S32 overlap)
{
	if (mDocSize > mPageSize)
	{
		changeLine(mPageSize - overlap, true);
	}
}

//static
void LLScrollbar::onLineUpBtnPressed(void* userdata)
{
	LLScrollbar* self = (LLScrollbar*)userdata;

	self->changeLine(- self->mStepSize, true);
}

//static
void LLScrollbar::onLineDownBtnPressed(void* userdata)
{
	LLScrollbar* self = (LLScrollbar*)userdata;
	self->changeLine(self->mStepSize, true);
}

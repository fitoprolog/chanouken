/**
 * @file lltoolcomp.h
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

#ifndef LL_TOOLCOMP_H
#define LL_TOOLCOMP_H

#include "llsingleton.h"

#include "lltool.h"

class LLManip;
class LLPickInfo;
class LLTextBox;
class LLToolGun;
class LLToolGrabBase;
class LLToolPlacer;
class LLToolSelect;
class LLToolSelectRect;
class LLView;

class LLToolComposite : public LLTool
{
public:
	LLToolComposite(const std::string& name);

	// Sets the current tool:
    virtual bool handleMouseDown(S32 x, S32 y, MASK mask) = 0;
	// Returns to the default tool:
    virtual bool handleMouseUp(S32 x, S32 y, MASK mask);
	virtual bool handleDoubleClick(S32 x, S32 y, MASK mask) = 0;

	// Map virtual functions to the currently active internal tool:
    LL_INLINE virtual bool handleHover(S32 x, S32 y, MASK mask)
	{
		return mCur->handleHover(x, y, mask);
	}

	LL_INLINE virtual bool handleScrollWheel(S32 x, S32 y, S32 clicks)
	{
		return mCur->handleScrollWheel(x, y, clicks);
	}

	LL_INLINE virtual bool handleRightMouseDown(S32 x, S32 y, MASK mask)
	{
		return mCur->handleRightMouseDown(x, y, mask);
	}

	LL_INLINE virtual LLViewerObject* getEditingObject()	{ return mCur->getEditingObject(); }
	LL_INLINE virtual LLVector3d getEditingPointGlobal()	{ return mCur->getEditingPointGlobal(); }
	LL_INLINE virtual bool isEditing()						{ return mCur->isEditing(); }
	LL_INLINE virtual void stopEditing()					{ mCur->stopEditing(); mCur = mDefault; }

	LL_INLINE virtual bool clipMouseWhenDown()				{ return mCur->clipMouseWhenDown(); }

	virtual void handleSelect();
	LL_INLINE virtual void handleDeselect()					{ mCur->handleDeselect(); mCur = mDefault; mSelected = false; }

	LL_INLINE virtual void render()							{ mCur->render(); }
	LL_INLINE virtual void draw()							{ mCur->draw(); }

	LL_INLINE virtual bool handleKey(KEY key, MASK mask)	{ return mCur->handleKey(key, mask); }

	virtual void onMouseCaptureLost();

	LL_INLINE virtual void screenPointToLocal(S32 screen_x, S32 screen_y,
											  S32* local_x, S32* local_y) const
	{
		mCur->screenPointToLocal(screen_x, screen_y, local_x, local_y);
	}

	LL_INLINE virtual void localPointToScreen(S32 local_x, S32 local_y,
											  S32* screen_x,
											  S32* screen_y) const
	{
		mCur->localPointToScreen(local_x, local_y, screen_x, screen_y);
	}

	bool isSelecting();

protected:
	void setCurrentTool(LLTool* new_tool);
	LL_INLINE LLTool* getCurrentTool()						{ return mCur; }

	// In hover handler, call this to auto-switch tools:
	void setToolFromMask(MASK mask, LLTool* normal);

protected:
	// The tool to which we are delegating:
	LLTool*						mCur;
	LLTool*						mDefault;
	LLManip*					mManip;
	LLToolSelectRect*			mSelectRect;
	bool						mSelected;
	bool						mMouseDown;

public:
	static const std::string	sNameComp;
};

class LLToolCompInspect final : public LLToolComposite,
								public LLSingleton<LLToolCompInspect>
{
	friend class LLSingleton<LLToolCompInspect>;

protected:
	LOG_CLASS(LLToolCompInspect);

public:
	LLToolCompInspect();
	~LLToolCompInspect() override;

	// Overridden from LLToolComposite
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleDoubleClick(S32 x, S32 y, MASK mask) override;

	static void pickCallback(const LLPickInfo& pick_info);
};

class LLToolCompTranslate final : public LLToolComposite,
								  public LLSingleton<LLToolCompTranslate>
{
	friend class LLSingleton<LLToolCompTranslate>;

protected:
	LOG_CLASS(LLToolCompTranslate);

public:
	LLToolCompTranslate();
	~LLToolCompTranslate() override;

	// Overridden from LLToolComposite
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	// Returns to the default tool:
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	void render() override;

	LLTool* getOverrideTool(MASK mask) override;

	static void pickCallback(const LLPickInfo& pick_info);
};

class LLToolCompScale final : public LLToolComposite,
							  public LLSingleton<LLToolCompScale>
{
	friend class LLSingleton<LLToolCompScale>;

protected:
	LOG_CLASS(LLToolCompScale);

public:
	LLToolCompScale();
	~LLToolCompScale() override;

	// Overridden from LLToolComposite
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
	// Returns to the default tool:
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	void render() override;

	LLTool* getOverrideTool(MASK mask) override;

	static void pickCallback(const LLPickInfo& pick_info);
};

class LLToolCompRotate final : public LLToolComposite,
							   public LLSingleton<LLToolCompRotate>
{
	friend class LLSingleton<LLToolCompRotate>;

protected:
	LOG_CLASS(LLToolCompRotate);

public:
	LLToolCompRotate();
	~LLToolCompRotate() override;

	// Overridden from LLToolComposite
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	void render() override;

	LLTool* getOverrideTool(MASK mask) override;

	static void pickCallback(const LLPickInfo& pick_info);
};

class LLToolCompCreate final : public LLToolComposite,
							   public LLSingleton<LLToolCompCreate>
{
	friend class LLSingleton<LLToolCompCreate>;

protected:
	LOG_CLASS(LLToolCompCreate);

public:
	LLToolCompCreate();
	~LLToolCompCreate() override;

	// Overridden from LLToolComposite
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	static void pickCallback(const LLPickInfo& pick_info);

protected:
	LLToolPlacer*	mPlacer;
	bool			mObjectPlacedOnMouseDown;
};

class LLToolCompGun final : public LLToolComposite,
							public LLSingleton<LLToolCompGun>
{
	friend class LLSingleton<LLToolCompGun>;

protected:
	LOG_CLASS(LLToolCompGun);

public:
	LLToolCompGun();
	~LLToolCompGun() override;

	// Overridden from LLToolComposite
    bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	void onMouseCaptureLost() override;
	void handleSelect() override;
	void handleDeselect() override;
	LL_INLINE LLTool* getOverrideTool(MASK mask) override	{ return NULL; }

protected:
	LLToolGun*		mGun;
	LLToolGrabBase*	mGrab;
	LLTool*			mNull;
};

#endif  // LL_TOOLCOMP_H

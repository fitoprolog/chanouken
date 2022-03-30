/**
* @file lleditmenuhandler.h
* @authors Aaron Yonas, James Cook
*
* $LicenseInfo:firstyear=2006&license=viewergpl$
*
* Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LLEDITMENUHANDLER_H
#define LLEDITMENUHANDLER_H

#include "llpreprocessor.h"

// Interface used by menu system for plug-in hotkey/menu handling
class LLEditMenuHandler
{
public:
	// This is needed even though this is just an interface class.
	virtual ~LLEditMenuHandler() = default;

	LL_INLINE virtual void undo()					{}
	LL_INLINE virtual bool canUndo() const			{ return false; }

	LL_INLINE virtual void redo()					{}
	LL_INLINE virtual bool canRedo() const			{ return false; }

	LL_INLINE virtual void cut()					{}
	LL_INLINE virtual bool canCut() const			{ return false; }

	LL_INLINE virtual void copy()					{}
	LL_INLINE virtual bool canCopy() const			{ return false; }

	LL_INLINE virtual void paste()					{}
	LL_INLINE virtual bool canPaste() const			{ return false; }

	// "delete" is a keyword
	LL_INLINE virtual void doDelete()				{}
	LL_INLINE virtual bool canDoDelete() const		{ return false; }

	LL_INLINE virtual void selectAll()				{}
	LL_INLINE virtual bool canSelectAll() const		{ return false; }

	LL_INLINE virtual void deselect()				{}
	LL_INLINE virtual bool canDeselect() const		{ return false; }

	LL_INLINE virtual void duplicate()				{}
	LL_INLINE virtual bool canDuplicate() const		{ return false; }

public:
	// TODO: Instead of being a public data member, it would be better to hide
	// it altogether and have a "set" method and then a bunch of static
	// versions of the cut, copy, paste methods, etc that operate on the
	// current global instance. That would drastically simplify the existing
	// code that accesses this global variable by putting all the null checks
	// in the one implementation of those static methods. -MG
	static LLEditMenuHandler* gEditMenuHandler;
};

#endif

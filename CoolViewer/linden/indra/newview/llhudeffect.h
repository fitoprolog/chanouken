/** 
 * @file llhudeffect.h
 * @brief LLHUDEffect class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLHUDEFFECT_H
#define LL_LLHUDEFFECT_H

#include "llhudobject.h"

#include "llcolor4u.h"
#include "llframetimer.h"
#include "lluuid.h"

constexpr F32 LL_HUD_DUR_SHORT = 1.f;

class LLMessageSystem;


class LLHUDEffect : public LLHUDObject
{
	friend class LLHUDManager;

public:
	void setNeedsSendToSim(bool b)			{ mNeedsSendToSim = b; }
	bool getNeedsSendToSim() const			{ return mNeedsSendToSim; }
	void setOriginatedHere(bool b)			{ mOriginatedHere = b; }
	bool getOriginatedHere() const			{ return mOriginatedHere; }

	void setDuration(F32 duration)			{ mDuration = duration; }
	void setColor(const LLColor4U& color)	{ mColor = color; }
	void setID(const LLUUID& id)			{ mID = id; }
	const LLUUID& getID() const				{ return mID; }

	bool isDead() const override			{ return mDead; }

protected:
	LLHUDEffect(U8 type);
	~LLHUDEffect() override = default;

	void render() override;

	virtual void packData(LLMessageSystem* mesgsys);
	virtual void unpackData(LLMessageSystem* mesgsys, S32 blocknum);
	virtual void update()					{}

	static void getIDType(LLMessageSystem* mesgsys, S32 blocknum, LLUUID& uuid,
						  U8& type);

protected:
	LLUUID		mID;
	F32			mDuration;
	LLColor4U	mColor;

	bool		mNeedsSendToSim;
	bool		mOriginatedHere;
};

#endif // LL_LLHUDEFFECT_H

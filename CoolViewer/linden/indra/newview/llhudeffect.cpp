/**
 * @file llhudeffect.cpp
 * @brief LLHUDEffect class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llhudeffect.h"

#include "llmessage.h"

#include "llagent.h"

LLHUDEffect::LLHUDEffect(U8 type)
:	LLHUDObject(type),
	mDuration(1.f),
	mNeedsSendToSim(false),
	mOriginatedHere(false)
{
}

void LLHUDEffect::render()
{
	llerrs << "Never call this!" << llendl;
}

void LLHUDEffect::packData(LLMessageSystem* mesgsys)
{
	mesgsys->addUUIDFast(_PREHASH_ID, mID);
	mesgsys->addUUIDFast(_PREHASH_AgentID, gAgentID);
	mesgsys->addU8Fast(_PREHASH_Type, mType);
	mesgsys->addF32Fast(_PREHASH_Duration, mDuration);
	mesgsys->addBinaryData(_PREHASH_Color, mColor.mV, 4);
}

void LLHUDEffect::unpackData(LLMessageSystem* mesgsys, S32 blocknum)
{
	mesgsys->getUUIDFast(_PREHASH_Effect, _PREHASH_ID, mID, blocknum);
	mesgsys->getU8Fast(_PREHASH_Effect, _PREHASH_Type, mType, blocknum);
	mesgsys->getF32Fast(_PREHASH_Effect, _PREHASH_Duration, mDuration, blocknum);
	mesgsys->getBinaryDataFast(_PREHASH_Effect,_PREHASH_Color, mColor.mV, 4, blocknum);
}

//static
void LLHUDEffect::getIDType(LLMessageSystem* mesgsys, S32 blocknum, LLUUID& id,
							U8& type)
{
	mesgsys->getUUIDFast(_PREHASH_Effect, _PREHASH_ID, id, blocknum);
	mesgsys->getU8Fast(_PREHASH_Effect, _PREHASH_Type, type, blocknum);
}

/**
* @file llpathfindingcharacter.h
* @brief Definition of a pathfinding character that contains various properties required for havok pathfinding.
*
* $LicenseInfo:firstyear=2012&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2012, Linden Research, Inc.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation;
* version 2.1 of the License only.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
*/
#ifndef LL_LLPATHFINDINGCHARACTER_H
#define LL_LLPATHFINDINGCHARACTER_H

#include <string>

#include "llpathfindingobject.h"

class LLSD;

class LLPathfindingCharacter : public LLPathfindingObject
{
protected:
	LOG_CLASS(LLPathfindingCharacter);

public:
	LLPathfindingCharacter(const LLUUID& id, const LLSD& char_data);
	LLPathfindingCharacter(const LLPathfindingCharacter& obj);

	LLPathfindingCharacter& operator=(const LLPathfindingCharacter& obj);

	LL_INLINE LLPathfindingCharacter* asCharacter() override
	{
		return this;
	}

	LL_INLINE const LLPathfindingCharacter* asCharacter() const override
	{
		return this;
	}

	LL_INLINE F32 getCPUTime() const		{ return mCPUTime; }

	LL_INLINE bool isHorizontal() const		{ return mIsHorizontal; }
	LL_INLINE F32 getLength() const			{ return mLength; }
	LL_INLINE F32 getRadius() const			{ return mRadius; }

private:
	void parseCharacterData(const LLSD& char_data);

private:
	F32  mCPUTime;
	F32  mLength;
	F32  mRadius;
	bool mIsHorizontal;
};

#endif // LL_LLPATHFINDINGCHARACTER_H

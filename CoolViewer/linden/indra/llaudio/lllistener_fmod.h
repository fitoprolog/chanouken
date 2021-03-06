/**
 * @file lllistener_fmod.h
 * @brief Description of LISTENER class abstracting the audio support as an
 * FMOD implementation
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

#ifndef LL_LISTENER_FMOD_H
#define LL_LISTENER_FMOD_H

#include "lllistener.h"

// Stubs
namespace FMOD
{
	class System;
}

class LLListener_FMOD final : public LLListener
{
public:
	LLListener_FMOD(FMOD::System* system);

	void translate(const LLVector3& offset) override;
	void setPosition(const LLVector3& pos) override;
	void setVelocity(const LLVector3& vel) override;
	void orient(const LLVector3& up, const LLVector3& at) override;
	void setDopplerFactor(F32 factor) override;
	void setRolloffFactor(F32 factor) override;

	void commitDeferredChanges() override;

protected:
	 FMOD::System*	mSystem;
};

#endif	// LL_LISTENER_FMOD_H

/**
 * @file llwatchdog.h
 * @brief The LLWatchdog class declaration
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_LLWATCHDOG_H
#define LL_LLWATCHDOG_H

#include "boost/function.hpp"

#include "llsingleton.h"
#include "lltimer.h"

class LLMutex;
class LLWatchdogTimerThread;

class LL_COMMON_API LLWatchdogTimeout
{
public:
	LLWatchdogTimeout();
	~LLWatchdogTimeout();

	bool isAlive() const;
	void reset();
	LL_INLINE void start()         				{ start(NULL); }
	void stop();

	void start(const std::string* state);
	void setTimeout(F32 d);
	void ping(const std::string* state);
	LL_INLINE const std::string* getState()		{ return mPingState; }

private:
	LLTimer				mTimer;
	F32					mTimeout;
	const std::string*	mPingState;
};

class LL_COMMON_API LLWatchdog : public LLSingleton<LLWatchdog>
{
	friend class LLSingleton<LLWatchdog>;

protected:
	LOG_CLASS(LLWatchdog);

public:
	LLWatchdog();

	// Add an entry to the watchdog.
	void add(LLWatchdogTimeout* e);
	void remove(LLWatchdogTimeout* e);

	typedef boost::function<void (void)> killer_event_callback_t;

	void init(killer_event_callback_t func = NULL);
	void run();
	void cleanup();

private:
	void lockThread();
	void unlockThread();

private:
	LLMutex*				mSuspectsAccessMutex;
	LLWatchdogTimerThread*	mTimer;
	U64						mLastClockCount;

	typedef std::set<LLWatchdogTimeout*> suspects_registry_t;
	suspects_registry_t		mSuspects;

	killer_event_callback_t	mKillerCallback;
};

#endif // LL_LLWATCHDOG_H

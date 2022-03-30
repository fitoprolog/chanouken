/**
 * @file llerror.h
 * @date   December 2006
 * @brief error message system
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

#ifndef LL_LLERROR_H
#define LL_LLERROR_H

#include <sstream>
#include <typeinfo>

#include "stdtypes.h"

#include "llpreprocessor.h"

const int LL_ERR_NOERR = 0;

// NOTE: in the Cool VL Viewer, the error logging scheme was both simplified
// and optimized. There was little to no point to tagged info, warning and
// error messages (LL_INFOS("Tag"), LL_WARNS("Tag"), LL_ERRS("Tag")) since
// all info and warning messages are normally always logged, unless the user
// fiddles with the app_settings/logcontrol.xml file to explicitely disable
// them (but what for ???...). LL_ERRS was totally useless, since encountering
// any such message always involved a voluntary crash (which should not be
// prevented either, since the code would not be able to handle the error
// condition that lead to it). This lets LL_DEBUGS, but LL's viewer did not
// even allow to enable those messages via the UI and you had to recourse to
// editing manually the app_settings/logcontrol.xml file... I therefore kept
// only llinfos, llwarns, llerrs and LL_DEBUGS("Tag"), and I added a way to
// enable/disable LL_DEBUGS from a floater ("Advanced" -> "Consoles" ->
// "Debug tags"; see also the indra/newview/hbfloaterdebugtags.* files).
// I also got rid of broad/narrow tags (dual tags), since they were used only
// in a couple of places and did not really bring anything but slowing down the
// logging routine with one more test to perform.
// I added llinfos_once and llwarns_once (which replace LL_INFOS_ONCE("Tag")
// and LL_WARNS_ONCE("Tag"), respectively) and kept LL_DEBUGS_ONCE("Tag").
// Finally, I added a repeating messages filter to keep the viewer from being
// slowed down when a log message is repeated over and over dozens or hundreds
// of times per second: repeated messages are counted, and only printed twice
// (the first time they occur, then a second time, with the repeats count
// appended, when another, different message is logged). HB

/* Error Logging Facility

	Information for most users:

	Code can log messages with constructions like this:

		llinfos << "request to fizzbip agent " << agent_id
				<< " denied due to timeout" << llendl;

	Messages can be logged to one of four increasing levels of concern, using
	one of four "streams":

		LL_DEBUGS("Tag")	- debug messages that are not shown unless "Tag"
							  is active.
		llinfos				- informational messages.
		llwarns				- warning messages that signal an unexpected
							  occurrence (that could be or not the sign of an
							  actual problem).
		llerrs				- error messages that are major, unrecoverable
							  failures.
	The later (llerrs) automatically crashes the process after the message is
	logged.

	Note that these "streams" are actually #define magic. Rules for use:
		* they cannot be used as normal streams, only to start a message
		* messages written to them MUST be terminated with llendl or LL_ENDL
		* between the opening and closing, the << operator is indeed
		  writing onto a std::ostream, so all conversions and stream
		  formating are available

	These messages are automatically logged with function name, and (if
	enabled) file and line of the message (note: existing messages that already
	include the function name do not get name printed twice).

	If you have a class, adding LOG_CLASS line to the declaration will cause
	all messages emitted from member functions (normal and static) to be tagged
	with the proper class name as well as the function name:

		class LLFoo
		{
		protected:
			LOG_CLASS(LLFoo);

		public:
			...
		};

		void LLFoo::doSomething(int i)
		{
			if (i > 100)
			{
				llwarns << "called with a big value for i: " << i << llendl;
			}
			...
		}

	will result in messages like:

		WARNING: LLFoo::doSomething: called with a big value for i: 283


	You may also use this construct if you need to do computation in the middle
	of a message (most useful with debug messages):
		LL_DEBUGS ("AgentGesture") << "the agent " << agend_id;
		switch (f)
		{
			case FOP_SHRUGS:	LL_CONT << "shrugs";			break;
			case FOP_TAPS:		LL_CONT << "points at " << who;	break;
			case FOP_SAYS:		LL_CONT << "says " << message;	break;
		}
		LL_CONT << " for " << t << " seconds" << LL_ENDL;

	Such computation is done only when the message is actually logged.


	Which messages are logged and which are suppressed can be controled at run
	time from the live file logcontrol.xml based on function, class and/or
	source file.

	Lastly, logging is now very efficient in both compiled code and execution
	when skipped. There is no need to wrap messages, even debugging ones, in
	#if LL_DEBUG constructs. LL_DEBUGS("StringTag") messages are compiled into
	all builds, even releases. Which means you can use them to help debug even
	when deployed to a real grid.
*/

namespace LLError
{
	enum ELevel
	{
		LEVEL_DEBUG = 0,
		LEVEL_INFO = 1,
		LEVEL_WARN = 2,
		LEVEL_ERROR = 3,
		// Not really a level: used to indicate that no messages should be
		// logged.
		LEVEL_NONE = 4
	};

	// Macro support
	// The classes CallSite and Log are used by the logging macros below.
	// They are not intended for general use.

	class CallSite;

	class LL_COMMON_API Log
	{
	public:
		static bool shouldLog(CallSite&);
		static std::ostringstream* out();
		static void flush(const std::ostringstream& out, const CallSite& site);

		// When false, skip all LL_DEBUGS checks, for speed.
		static bool sDebugMessages;
		// When true, abort() on llerrs instead of crashing.
		static bool sIsBeingDebugged;
	};

	class LL_COMMON_API CallSite
	{
		friend class Log;

	public:
		// Represents a specific place in the code where a message is logged
		// This is public because it is used by the macros below. It is not
		// intended for public use. The constructor is never inlined since it
		// is called only once per static call site in the code and inlining it
		// would just consume needlessly CPU instruction cache lines... HB
		LL_NO_INLINE CallSite(ELevel level, const char* file, S32 line,
							  const std::type_info& class_info,
							  const char* function, const char* tag,
							  bool print_once);

		// This member function needs to be in-line for efficiency
		LL_INLINE bool shouldLog()
		{
			return mCached ? mShouldLog : Log::shouldLog(*this);
		}

		LL_INLINE void invalidate()					{ mCached = false; }

	private:
		// These describe the call site and never change
		const ELevel			mLevel;
		const S32				mLine;
		char*					mFile;
		const std::type_info&   mClassInfo;
		const char* const		mFunction;
		const char* const		mTag;
		const bool				mPrintOnce;

		// These implement a cache of the call to shouldLog()
		bool					mCached;
		bool					mShouldLog;
	};

	// Used to indicate the end of a message
	class End {};
	LL_INLINE std::ostream& operator<<(std::ostream& s, const End&)
	{
		return s;
	}

	// Used to indicate no class info known for logging
	class LL_COMMON_API NoClassInfo {};

	LL_COMMON_API std::string className(const std::type_info& type);

	LL_COMMON_API bool isAvailable();
}

#ifdef LL_DEBUG
# define llassert(func)			llassert_always(func)
#else
# define llassert(func)
#endif
#define llassert_always(func)	if (LL_UNLIKELY(!(func))) llerrs << "ASSERT (" << #func << ")" << llendl;

/*
	Class type information for logging
 */

// Declares class to tag logged messages with. See top of file for example of
// how to use this
#define LOG_CLASS(s)		typedef s _LL_CLASS_TO_LOG

// Outside a class declaration, or in class without LOG_CLASS(), this typedef
// causes the messages to not be associated with any class.
typedef LLError::NoClassInfo _LL_CLASS_TO_LOG;

//
// Error Logging Macros. See top of file for common usage.
//

#define lllog(level, Tag, once) \
	do { \
		static LLError::CallSite _site(level, __FILE__, __LINE__, typeid(_LL_CLASS_TO_LOG), __FUNCTION__, Tag, once); \
		if (LL_LIKELY(_site.shouldLog())) \
		{ \
			std::ostringstream _out; \
			(_out)

#define llendl \
			LLError::End(); \
			LLError::Log::flush(_out, _site); \
		} \
	} while (false)

#define llinfos				lllog(LLError::LEVEL_INFO, NULL, false)
#define llinfos_once		lllog(LLError::LEVEL_INFO, NULL, true)
#define llwarns				lllog(LLError::LEVEL_WARN, NULL, false)
#define llwarns_once		lllog(LLError::LEVEL_WARN, NULL, true)
#define llerrs				lllog(LLError::LEVEL_ERROR, NULL, false)
#define llcont				(_out)

// Macros for debugging with the passing of a string tag. Note that we test for
// a special static variable (sDebugMessages) before calling shouldLog(), which
// allows to switch off all debug messages logging at once if/when needed. HB

#define lllog_debug(Tag, once) \
	do { \
		static LLError::CallSite _site(LLError::LEVEL_DEBUG, __FILE__, __LINE__, typeid(_LL_CLASS_TO_LOG), __FUNCTION__, Tag, once); \
		if (LL_UNLIKELY(LLError::Log::sDebugMessages && _site.shouldLog())) \
		{ \
			std::ostringstream _out; \
			(_out)

#define LL_DEBUGS(Tag)		lllog_debug(Tag, false)
#define LL_DEBUGS_ONCE(Tag)	lllog_debug(Tag, true)
#define LL_ENDL				llendl
#define LL_CONT				(_out)

#endif // LL_LLERROR_H

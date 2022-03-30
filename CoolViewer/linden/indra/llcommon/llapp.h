/**
 * @file llapp.h
 * @brief Declaration of the LLApp class.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLAPP_H
#define LL_LLAPP_H

#include <map>
#if LL_LINUX
// glibc 2.16 does declare siginfo_t
# define GLIBC_VERSION (__GLIBC__ * 100 + __GLIBC_MINOR__)
# if GLIBC_VERSION >= 216
#  include <signal.h>
# else
typedef struct siginfo siginfo_t;
# endif
# undef GLIBC_VERSION
#endif

#include "llatomic.h"
#include "llsd.h"

// Forward declarations
class LLErrorThread;
class LLLiveFile;

typedef void (*LLAppErrorHandler)();
typedef void (*LLAppChildCallback)(int pid, bool exited, int status);

#if !LL_WINDOWS
extern const S32 LL_SMACKDOWN_SIGNAL;
extern const S32 LL_HEARTBEAT_SIGNAL;

// Clear all of the signal handlers (which we want to do for the child process
// when we fork
void clear_signals();

class LLChildInfo
{
public:
	LLChildInfo()
	:	mGotSigChild(false),
		mCallback(NULL)
	{
	}

public:
	LLAppChildCallback	mCallback;
	bool				mGotSigChild;
};
#endif

class LL_COMMON_API LLApp
{
	friend class LLErrorThread;
#if !LL_WINDOWS
	friend void default_unix_signal_handler(int, siginfo_t*, void*);
#endif

protected:
	LOG_CLASS(LLApp);

	LLApp(LLErrorThread* error_thread);

	void commonCtor();

	/**
	  * @brief This method is called once a frame to do once a frame tasks.
	  */
	void stepFrame();

public:
	typedef enum e_app_status
	{
		APP_STATUS_STOPPED,		// No longer running - tells the error thread it can exit
		APP_STATUS_RUNNING,		// Running - the default status
		APP_STATUS_QUITTING,	// Quitting - threads should listen for this and clean up
		APP_STATUS_ERROR		// Fatal error occurred - tells the error thread to run
	} EAppStatus;

	LLApp();
	virtual ~LLApp();

	/**
	 * @brief Returns the static app instance if one was created.
	 */
	LL_INLINE static LLApp* getInstance()	{ return sApplication; }

	/** @name Runtime options */
	//@{
	/**
	 * @brief Enumeration to specify option priorities in highest to
	 * lowest order.
	 */
	enum OptionPriority
	{
		PRIORITY_RUNTIME_OVERRIDE,
		PRIORITY_COMMAND_LINE,
		PRIORITY_SPECIFIC_CONFIGURATION,
		PRIORITY_GENERAL_CONFIGURATION,
		PRIORITY_DEFAULT,
		PRIORITY_COUNT
	};

	/**
	 * @brief Get the application option at the highest priority.
	 *
	 * If the return value is undefined, the option does not exist.
	 * @param name The name of the option.
	 * @return Returns the option data.
	 */
	LLSD getOption(const std::string& name) const;

	/**
	 * @brief Parse command line options and insert them into
	 * application command line options.
	 *
	 * The name inserted into the option will have leading option identifiers
	 * (a minus or double minus) stripped. All optionswith values will be
	 * stored as a string, while all options without values will be stored as
	 * true.
	 * @param argc The argc passed into main().
	 * @param argv The argv passed into main().
	 * @return Returns true if the parse succeeded.
	 */
	bool parseCommandOptions(int argc, char** argv);

	/**
	 * @brief Set the options at the specified priority.
	 *
	 * This function completely replaces the options at the priority level with
	 * the data specified. This function will make sure level and data might be
     * valid before doing the replace.
	 * @param level The priority level of the data.
	 * @param data The data to set.
	 * @return Returns true if the option was set.
	 */
	bool setOptionData(OptionPriority level, LLSD data);

	/**
	 * @brief Get the option data at the specified priority.
	 *
	 * This method is probably not so useful except when merging information.
	 * @param level The priority level of the data.
	 * @return Returns The data (if any) at the level priority.
	 */
	LLSD getOptionData(OptionPriority level);
	//@}

	//
	// Main application logic
	//

	enum InitState
	{
		INIT_OK,		// Initialization OK
		INIT_OK_EXIT,	// Initialization OK, but do exit immediately
		INIT_FAILED		// Initialization failed
	};

	virtual InitState init() = 0; // Override to do application initialization

	// cleanup()
	//
	// It's currently assumed that the cleanup() method will only get
	// called from the main thread or the error handling thread, as it will
	// likely do thread shutdown, among other things.
	//
	virtual bool cleanup() = 0;	// Override to do application cleanup

	// mainLoop()
	//
	// Runs the application main loop. It's assumed that when you exit this
	// method, the application is in one of the cleanup states, either QUITTING
	// or ERROR.
	//
	 // Override for the application main loop. Needs to at least gracefully
	// notice the QUITTING state and exit.
	virtual bool mainLoop() = 0;

	//
	// Application status
	//

	// Sets status to QUITTING, the app is now shutting down
	static void setQuitting();
	// Sets status to STOPPED, the app is done running and should exit
	LL_INLINE static void setStopped()		{ sStatus = APP_STATUS_STOPPED; }
	// Sets status to ERROR, the error handler should run
	static void setError();

	LL_INLINE static bool isStopped()		{ return sStatus == APP_STATUS_STOPPED; }
	LL_INLINE static bool isRunning()		{ return sStatus == APP_STATUS_RUNNING; }
	LL_INLINE static bool isQuitting()		{ return sStatus == APP_STATUS_QUITTING; }
	LL_INLINE static bool isError()			{ return sStatus == APP_STATUS_ERROR; }
	// Either quitting or error (app is exiting, cleanly or not)
	LL_INLINE static bool isExiting()		{ return sStatus >= APP_STATUS_QUITTING; }
#if !LL_WINDOWS
	static U32 getSigChildCount();
	static void incSigChildCount();
#endif
	static int getPid();

	//
	// Error handling methods
	//

	LL_INLINE static void setErrorHandler(LLAppErrorHandler handler)
	{
		sErrorHandler = handler;
	}

	LL_INLINE static void setSyncErrorHandler(LLAppErrorHandler handler)
	{
		sSyncErrorHandler = handler;
	}

#if !LL_WINDOWS
	//
	// Child process handling (Unix only for now)
	//
	// Set a callback to be run on exit of a child process
	// WARNING!  This callback is run from the signal handler due to the extreme crappiness of
	// Linux threading requiring waitpid() to be called from the thread that spawned the process.
	// At some point I will make this more behaved, but I'm not going to fix this right now - djs
	void setChildCallback(pid_t pid, LLAppChildCallback callback);

    // The child callback to run if no specific handler is set
	void setDefaultChildCallback(LLAppChildCallback callback);

    // Fork and do the proper signal handling/error handling mojo
	// WARNING: You need to make sure your signal handling callback is correct after
	// you fork, because not all threads are duplicated when you fork!
	pid_t fork();
#endif

protected:
	// Use this to change the application status.
	LL_INLINE static void setStatus(EAppStatus status)
	{
		sStatus = status;
	}

private:
	void startErrorThread();

	// Do platform-specific error-handling setup (signals, structured
	// exceptions).
	void setupErrorHandling();

	// Run shortly after we detect an error, ran in the relatively robust
	// context of the LLErrorThread - preferred.
	static void runErrorHandler();
	// Run IMMEDIATELY when we get an error, ran in the context of the
	// faulting thread.
	static void runSyncErrorHandler();

private:
	// *NOTE: On Windows, we need a routine to reset the structured exception
	// handler when some evil driver has taken it over for their own purposes
	typedef int(*signal_handler_func)(int signum);
	static LLAppErrorHandler	sErrorHandler;
	static LLAppErrorHandler	sSyncErrorHandler;

	// Default application threads
	// Waits for app to go to status ERROR, then runs the error callback
	LLErrorThread*				mThreadErrorp;

	// The application options.
	LLSD						mOptions;

	// The static application instance if it was created.
	static LLApp*				sApplication;

protected:
	// Reflects current application status
	static EAppStatus			sStatus;

#if !LL_WINDOWS
	// Number of SIGCHLDs received.
	static LLAtomicU32*			sSigChildCount;

	// Map key is a PID
	typedef std::map<pid_t, LLChildInfo> child_map;
	static child_map			sChildMap;

	static LLAppChildCallback	sDefaultChildCallback;
#endif

	// Set while the error thread is running
	static bool			sErrorThreadRunning;

public:
	// Contains all command-line options and arguments in a map
	typedef std::map<std::string, std::string> string_map;
	string_map					mOptionMap;

	static bool			sLogInSignal;
};

#endif // LL_LLAPP_H

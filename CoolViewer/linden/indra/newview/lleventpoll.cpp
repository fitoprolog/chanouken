/**
 * @file lleventpoll.cpp
 * @brief Implementation of the LLEventPoll class.
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

#include "llviewerprecompiledheaders.h"

#include "lleventpoll.h"

#include "llcorehttputil.h"
#include "lleventfilter.h"
#include "llhost.h"
#include "llsdserialize.h"
#include "lltrans.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llgridmanager.h"		// For gIsInSecondLife
#include "llstatusbar.h"
#include "llviewerregion.h"

///////////////////////////////////////////////////////////////////////////////
// LLEventPollImpl class

// We will wait RETRY_SECONDS + (error_count * RETRY_SECONDS_INC) before
// retrying after an error. This means we attempt to recover relatively quickly
// but back off giving more time to recover until we finally give up after
// MAX_EVENT_POLL_HTTP_ERRORS attempts.

// Half of a normal timeout.
constexpr F32 EVENT_POLL_ERROR_RETRY_SECONDS = 15.f;
constexpr F32 EVENT_POLL_ERROR_RETRY_SECONDS_INC = 5.f;
// 5 minutes, by the above rules.
constexpr S32 MAX_EVENT_POLL_HTTP_ERRORS = 10;

class LLEventPollImpl : public LLRefCount
{
protected:
	LOG_CLASS(LLEventPollImpl);

public:
	LLEventPollImpl(const std::string& region_name, const LLHost& sender);

	void start(const std::string& url);
	void stop();

	LL_INLINE S32 getCounter() const				{ return mCounter; }
	LL_INLINE const std::string& getRegionName()	{ return mRegionName; }

private:
	~LLEventPollImpl();

	static void eventPollCoro(std::string url,
							  LLPointer<LLEventPollImpl> impl);

	static void handleMessage(const LLSD& content,
							  const std::string& sender_ip);

private:
	LLCore::HttpRequest::policy_t					mHttpPolicy;
	LLCore::HttpOptions::ptr_t						mHttpOptions;
	LLCore::HttpHeaders::ptr_t						mHttpHeaders;
	LLCoreHttpUtil::HttpCoroutineAdapter::wptr_t	mAdapter;
	S32												mCounter;
	std::string										mSenderIp;
	std::string										mRegionName;
	std::string										mPollURL;
	bool											mDone;

	static S32										sNextCounter;
};

S32 LLEventPollImpl::sNextCounter = 1;

LLEventPollImpl::LLEventPollImpl(const std::string& region_name,
								 const LLHost& sender)
:	mDone(false),
	mCounter(sNextCounter++),
	mRegionName(region_name),
	mSenderIp(sender.getIPandPort()),
	// NOTE: by using these instead of omitting the corresponding
	// postAndSuspend() parameters, we avoid seeing such classes constructed
	// and destroyed at each loop...
	mHttpOptions(new LLCore::HttpOptions),
	mHttpHeaders(new LLCore::HttpHeaders)
{
	LLAppCoreHttp& app_core_http = gAppViewerp->getAppCoreHttp();
	// NOTE: be sure to use this policy, or to set the timeout to what it used
	// to be before changing it; using too large a viewer-side timeout would
	// cause to recieve bogus timeout responses from the server (especially in
	// SL, where 502 replies may come in disguise of 499 or 500 HTTP errors)...
	// HB
	mHttpPolicy = app_core_http.getPolicy(LLAppCoreHttp::AP_LONG_POLL);
	if (!gIsInSecondLife)
	{
		// In OpenSim, wait for the server to timeout on us (will report a 502
		// error), while in SL, we now timeout viewer-side (in libcurl) before
		// the server would send us a bogus HTTP error (502 error report HTML
		// page disguised with a 499 or 500 error code in the header) on its
		// own timeout... HB
		mHttpOptions->setTransferTimeout(90);
		mHttpOptions->setRetries(0);
	}

	if (mRegionName.empty())
	{
#if 0	// This can lead to long hiccups or "pauses" when the host is unknown
		// and the DNS resolves too slowly...
		mRegionName = sender.getHostName();
#else
		mRegionName = sender.getIPString();
#endif
	}

	llinfos << "Event poll <" << mCounter << "> initialized for region \""
			<< mRegionName << "\" with sender IP: " << mSenderIp << llendl;
}

LLEventPollImpl::~LLEventPollImpl()
{
	LL_DEBUGS("EventPoll") << "Destroying event poll <" << mCounter << ">"
						   << LL_ENDL;
	mHttpOptions.reset();
	mHttpHeaders.reset();
}

void LLEventPollImpl::start(const std::string& url)
{
	mPollURL = url;
	if (!url.empty())
	{
		llinfos	<< "Starting event poll <" << mCounter << "> - Region: "
				<< mRegionName << " - URL: " << mPollURL << llendl;
		std::string coroname =
			LLCoros::getInstance()->launch("LLEventPollImpl::eventPollCoro",
										   boost::bind(&LLEventPollImpl::eventPollCoro,
													   url, this));
		LL_DEBUGS("EventPoll") << "Coroutine name event for poll <"
							   << mCounter << ">: " << coroname << LL_ENDL;
	}
}

void LLEventPollImpl::stop()
{
	mDone = true;

	LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t adapter = mAdapter.lock();
	if (adapter)
	{
		llinfos << "Requesting stop for event poll <" << mCounter
				<< "> - Region: " << mRegionName << " - URL: " << mPollURL
				<< llendl;
		// Cancel the yielding operation if any.
		adapter->cancelSuspendedOperation();
	}
	else
	{
		LL_DEBUGS("EventPoll") << "Event poll <" << mCounter
							   << "> - Region: " << mRegionName
							   << " - Already stopped. No action taken."
							   << LL_ENDL;
	}
}

//static
void LLEventPollImpl::handleMessage(const LLSD& content,
									const std::string& sender_ip)
{
	std::string	msg_name = content["message"];
	LLSD message;
	message["sender"] = sender_ip;
	if (content.has("body"))
	{
		message["body"] = content["body"];
	}
	else
	{
		llwarns << "Message '" << msg_name << "' without a body." << llendl;
	}
	LLMessageSystem::dispatch(msg_name, message);
}

//static
void LLEventPollImpl::eventPollCoro(std::string url,
									LLPointer<LLEventPollImpl> impl)
{
	// Hold a LLPointer of our impl on the coroutine stack, so to avoid the
	// impl destruction before the exit of the coroutine.
	LLPointer<LLEventPollImpl> self = impl;

	LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t
		adapter(new LLCoreHttpUtil::HttpCoroutineAdapter("EventPoller",
														 self->mHttpPolicy));
	self->mAdapter = adapter;

	std::string sender_ip = self->mSenderIp;

	LL_DEBUGS("EventPoll") << "Event poll <" << self->mCounter
						   << "> entering coroutine. Region: "
						   << self->mRegionName << LL_ENDL;

	// Continually poll for a server update until we have been terminated
	S32 error_count = 0;
 	LLSD acknowledge;
	while (!self->mDone && !gDisconnected)
	{
		LLSD request;
		request["ack"] = acknowledge;
		request["done"] = false;

		LL_DEBUGS("EventPoll") << "Event poll <" << self->mCounter
							   << "> posting and yielding. Region: "
							   << self->mRegionName << LL_ENDL;
		LLSD result = adapter->postAndSuspend(url, request, self->mHttpOptions,
											  self->mHttpHeaders);

		if (gDisconnected)
		{
			llinfos << "Viewer disconnected. Dropping stale event message."
					<< llendl;
			break;
		}

		LLViewerRegion* region = gAgent.getRegion();
		bool is_agent_region = region &&
							   region->getHost().getIPandPort() == sender_ip;

		LLCore::HttpStatus status =
			LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
		if (!status)
		{
			if (status == gStatusTimeout)
			{
				// A standard timeout response: we get this when there are no
				// events.
				LL_DEBUGS("EventPoll") << "All is very quiet on target server "
									   << url << " - Region: "
									   << self->mRegionName << LL_ENDL;
				error_count = 0;
				continue;
			}
			else if (status == gStatusCancelled || status == gStatusNotFound)
			{
				// Event polling for this server has been cancelled. In some
				// cases the server gets ahead of the viewer and will return a
				// 404 error (not found) before the cancel event comes back in
				// the queue
				llinfos << "Cancelling coroutine for event poll <"
						<< self->mCounter << "> - Region: "
						<< self->mRegionName << llendl;
				break;
			}
			else if (!status.isHttpStatus())
			{
				// Some libcurl error (other than gStatusTimeout) or LLCore
				// error (other than gStatusCancelled) was returned. This is
				// unlikely to be recoverable...
				llwarns << "Critical error from poll request returned from libraries. Cancelling coroutine for event poll <"
						<< self->mCounter << "> - Region: "
						<< self->mRegionName << llendl;
				break;
			}
			else if (!gIsInSecondLife && status == gStatusBadGateway)
			{
				// Error 502 is seen on OpenSim grids when the server times
				// out (because there was no event to report). Treat as timeout
				// and restart.
				LL_DEBUGS("EventPoll") << "Event poll <" << self->mCounter
									   << "> - Region: " << self->mRegionName
									   << " - Error " << status.toTerseString()
									   << ": " << status.getMessage()
									   << " - Ignored and treated as a timeout."
									   << LL_ENDL;
				error_count = 0;
				continue;
			}

			LL_DEBUGS("EventPoll") << "Event poll <" << self->mCounter
								   << "> - Region: " << self->mRegionName
								   << " - Error " << status.toTerseString()
								   << ": " << status.getMessage();
			const LLSD& http_results =
				result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
			if (http_results.has("error_body"))
			{
				std::string body = http_results["error_body"].asString();
				LL_CONT << " Returned body:\n" << body;
			}
			LL_CONT << LL_ENDL;

			S32 max_retries = MAX_EVENT_POLL_HTTP_ERRORS;
			if (is_agent_region)
			{
				max_retries *= 2;
				llwarns << "Event poll <" << self->mCounter
						<< "> (agent region). Error: " << status.toTerseString()
						<< ": " << status.getMessage() << llendl;
				if (gStatusBarp)
				{
					gStatusBarp->incFailedEventPolls();
				}
			}

			if (error_count < max_retries)
			{
				// An unanticipated error has been received from our poll 
				// request. Calculate a timeout and wait for it to expire
				// (sleep) before trying again. The sleep time is increased by
				// EVENT_POLL_ERROR_RETRY_SECONDS_INC seconds for each
				// consecutive error until MAX_EVENT_POLL_HTTP_ERRORS is
				// reached.
				F32 wait = EVENT_POLL_ERROR_RETRY_SECONDS +
						   error_count * EVENT_POLL_ERROR_RETRY_SECONDS_INC;
				llwarns << "Event poll <" << self->mCounter
						<< "> - Region: " << self->mRegionName
						<< " - Retrying in " << wait
						<< " seconds; error count is now " << ++error_count
						<< llendl;

				llcoro::suspendUntilTimeout(wait);

				LL_DEBUGS("EventPoll") << "Event poll <" << self->mCounter
									   << "> about to retry request. Region: "
									   << self->mRegionName << LL_ENDL;
				continue;
			}

			// At this point we have given up and the viewer will not receive
			// HTTP messages from the simulator. IMs, teleports, about land,
			// selecting land, region crossing and more will all fail. They are
			// essentially disconnected from the region even though some things
			// may still work. Since things would not get better until they
			// relog we force a disconnect now.
			if (is_agent_region)
			{
				llwarns << "Forcing disconnect due to stalled agent region event poll."
						<< llendl;
				gAppViewerp->forceDisconnect(LLTrans::getString("AgentLostConnection"));
			}
			else
			{
				llwarns << "Stalled region event poll. Giving up for region: "
						<< self->mRegionName << llendl;
			}
			self->mDone = true;
			break;
		}
		else if (is_agent_region && gStatusBarp)
		{
			gStatusBarp->resetFailedEventPolls();
		}

		error_count = 0;

		if (!result.isMap() || !result.get("events") || !result.get("id"))
		{
			llwarns << "Event poll <" << self->mCounter
					<< "> - Region: " << self->mRegionName
					<< " - Received without event or id key: "
					<< LLSDXMLStreamer(result) << llendl;
			continue;
		}

		acknowledge = result["id"];
		const LLSD& events = result["events"];
		if (acknowledge.isUndefined())
		{
			llwarns << "Event poll <" << self->mCounter
					<< "> - Region: " << self->mRegionName
					<< " - Id undefined !" << llendl;
		}
		else
		{
			LL_DEBUGS("EventPoll") << "Event poll <" << self->mCounter
								   << "> - Region: " << self->mRegionName
								   << " - " << events.size() << " event(s):\n"
								   << LLSDXMLStreamer(acknowledge) << LL_ENDL;
		}

		for (LLSD::array_const_iterator it = events.beginArray(),
										end = events.endArray();
			 it != end; ++it)
		{
			if (it->has("message"))
			{
				handleMessage(*it, self->mSenderIp);
			}
		}
	}

	LL_DEBUGS("EventPoll") << "Event poll <" << self->mCounter
						   << "> - Region: " << self->mRegionName
						   << " - Leaving coroutine." << LL_ENDL;
}

///////////////////////////////////////////////////////////////////////////////
// LLEventPoll class

LLEventPoll::LLEventPoll(const std::string& region_name,
						 const std::string& poll_url, const LLHost& sender)
:	mImpl(new LLEventPollImpl(region_name, sender))
{
	mImpl->start(poll_url);
	LL_DEBUGS("EventPoll") << "Event poll <" << mImpl->getCounter()
						   << "> registered - Region: "
						   << mImpl->getRegionName() << LL_ENDL;
}

LLEventPoll::~LLEventPoll()
{
	S32 counter = mImpl->getCounter();
	std::string region_name = mImpl->getRegionName();
	mImpl->stop();
	mImpl = NULL;
	LL_DEBUGS("EventPoll") << "Event poll <" << counter
						   << "> unregistered - Region: " << region_name
						   << LL_ENDL;
}

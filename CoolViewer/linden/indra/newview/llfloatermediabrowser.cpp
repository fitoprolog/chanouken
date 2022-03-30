/**
 * @file llfloaterhtmlhelp.cpp
 * @brief HTML Help floater - uses embedded web browser control
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

#include "llfloatermediabrowser.h"

#include "llcombobox.h"
#include "llhttpconstants.h"
#include "llpluginclassmedia.h"
#include "llsdutil.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llurlhistory.h"
#include "llwindow.h"

#include "llcommandhandler.h"
#include "llviewercontrol.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llweb.h"
#include "roles_constants.h"

LLFloaterMediaBrowser::LLFloaterMediaBrowser(const LLSD& media_data)
:	mBrowser(NULL),
	mBackButton(NULL),
	mForwardButton(NULL),
	mReloadButton(NULL),
	mRewindButton(NULL),
	mPlayButton(NULL),
	mPauseButton(NULL),
	mStopButton(NULL),
	mSeekButton(NULL),
	mGoButton(NULL),
	mCloseButton(NULL),
	mBrowserButton(NULL),
	mAssignButton(NULL),
	mAddressCombo(NULL),
	mLoadingText(NULL)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_media_browser.xml");
}

bool LLFloaterMediaBrowser::postBuild()
{
	// Note: we use the "build dummy widget if missing" version of getChild<T>
	// so that all pointers are non-NULL and warnings are issued in the log
	// about missing UI elements. All the UI elements are considered mandatory.

	mBrowser = getChild<LLMediaCtrl>("browser");
	mBrowser->addObserver(this);

	mAddressCombo = getChild<LLComboBox>("address");
	mAddressCombo->setCommitCallback(onEnterAddress);
	mAddressCombo->setCallbackUserData(this);

	mBackButton = getChild<LLButton>("back");
	mBackButton->setClickedCallback(onClickBack, this);

	mForwardButton = getChild<LLButton>("forward");
	mForwardButton->setClickedCallback(onClickForward, this);

	mReloadButton = getChild<LLButton>("reload");
	mReloadButton->setClickedCallback(onClickRefresh, this);

	mRewindButton = getChild<LLButton>("rewind");
	mRewindButton->setClickedCallback(onClickRewind, this);

	mPlayButton = getChild<LLButton>("play");
	mPlayButton->setClickedCallback(onClickPlay, this);

	mPauseButton = getChild<LLButton>("pause");
	mPauseButton->setClickedCallback(onClickPlay, this);

	mStopButton = getChild<LLButton>("stop");
	mStopButton->setClickedCallback(onClickStop, this);

	mSeekButton = getChild<LLButton>("seek");
	mSeekButton->setClickedCallback(onClickSeek, this);

	mGoButton = getChild<LLButton>("go");
	mGoButton->setClickedCallback(onClickGo, this);

	mCloseButton = getChild<LLButton>("close");
	mCloseButton->setClickedCallback(onClickClose, this);

	mBrowserButton = getChild<LLButton>("open_browser");
	mBrowserButton->setClickedCallback(onClickOpenWebBrowser, this);

	mAssignButton = getChild<LLButton>("assign");
	mAssignButton->setClickedCallback(onClickAssign, this);

	mLoadingText = getChild<LLTextBox>("loading");

	buildURLHistory();

	return true;
}

void LLFloaterMediaBrowser::geometryChanged(S32 x, S32 y, S32 width,
											S32 height)
{
	// Make sure the layout of the browser control is updated, so this
	// calculation is correct.
	LLLayoutStack::updateClass();

	// TODO: need to adjust size and constrain position to make sure floaters
	// aren't moved outside the window view, etc.
	LLCoordWindow window_size;
	gWindowp->getSize(&window_size);

	// Adjust width and height for the size of the chrome on the Media Browser
	// window.
	width += getRect().getWidth() - mBrowser->getRect().getWidth();
	height += getRect().getHeight() - mBrowser->getRect().getHeight();

	LLRect geom;
	geom.setOriginAndSize(x, window_size.mY - (y + height), width, height);

	LL_DEBUGS("MediaBrowser") << "geometry change: " << geom << LL_ENDL;

	userSetShape(geom);
}

void LLFloaterMediaBrowser::draw()
{
	if (!mBrowser)
	{
		// There's something *very* wrong: abort
		llwarns_once << "Incomplete floater media browser !" << llendl;
		LLFloater::draw();
		return;
	}

	mBackButton->setEnabled(mBrowser->canNavigateBack());
	mForwardButton->setEnabled(mBrowser->canNavigateForward());

	bool can_go = !mAddressCombo->getValue().asString().empty();
	can_go &= !mBrowser->isTrusted();
	mGoButton->setEnabled(can_go);

	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		bool can_change = LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
																	   GP_LAND_CHANGE_MEDIA);
		mAssignButton->setVisible(can_change);
		bool not_empty = !mAddressCombo->getValue().asString().empty();
		mAssignButton->setEnabled(not_empty);
	}

	bool show_time_controls = false;
	bool media_playing = false;
	LLPluginClassMedia* media_plugin = mBrowser->getMediaPlugin();
	if (media_plugin)
	{
		show_time_controls = media_plugin->pluginSupportsMediaTime();
		media_playing = media_plugin->getStatus() == LLPluginClassMediaOwner::MEDIA_PLAYING;
	}

	mRewindButton->setVisible(show_time_controls);
	mPlayButton->setVisible(show_time_controls && !media_playing);
	mPlayButton->setEnabled(!media_playing);
	mPauseButton->setVisible(show_time_controls && media_playing);
	mStopButton->setVisible(show_time_controls);
	mStopButton->setEnabled(media_playing);
	mSeekButton->setVisible(show_time_controls);

	LLFloater::draw();
}

void LLFloaterMediaBrowser::buildURLHistory()
{
	LLCtrlListInterface* url_list = mAddressCombo->getListInterface();
	if (url_list)
	{
		url_list->operateOnAll(LLCtrlListInterface::OP_DELETE);
	}

	// Get all of the entries in the "browser" collection
	LLSD browser_history = LLURLHistory::getURLHistory("browser");

	for(LLSD::array_iterator iter_history = browser_history.beginArray(),
							 end_history = browser_history.endArray();
		iter_history != end_history; ++iter_history)
	{
		std::string url = (*iter_history).asString();
		if (! url.empty())
		{
			url_list->addSimpleElement(url);
		}
	}

	// Initialize URL history in the plugin
	LLPluginClassMedia* plugin = mBrowser->getMediaPlugin();
	if (plugin)
	{
		plugin->initializeUrlHistory(browser_history);
	}
}

std::string LLFloaterMediaBrowser::getSupportURL()
{
	return getString("support_page_url");
}

void LLFloaterMediaBrowser::onClose(bool app_quitting)
{
	if (mBrowser)
	{
		mBrowser->remObserver(this);
		if (mBrowser->getMediaSource())
		{
			mBrowser->getMediaSource()->cancelMimeTypeProbe();
		}
	}

	//setVisible(false);
	destroy();
}

void LLFloaterMediaBrowser::handleMediaEvent(LLPluginClassMedia* self,
											 EMediaEvent event)
{
	if (event == MEDIA_EVENT_LOCATION_CHANGED)
	{
		setCurrentURL(self->getLocation());
		mAddressCombo->setVisible(false);
		mLoadingText->setVisible(true);
	}
	else if (event == MEDIA_EVENT_NAVIGATE_COMPLETE)
	{
		// This is the event these flags are sent with.
		mBackButton->setEnabled(self->getHistoryBackAvailable());
		mForwardButton->setEnabled(self->getHistoryForwardAvailable());
		mAddressCombo->setVisible(true);
		mLoadingText->setVisible(false);
	}
	else if (event == MEDIA_EVENT_CLOSE_REQUEST)
	{
		// The browser instance wants its window closed.
		close();
	}
	else if (event == MEDIA_EVENT_GEOMETRY_CHANGE)
	{
		geometryChanged(self->getGeometryX(), self->getGeometryY(),
						self->getGeometryWidth(), self->getGeometryHeight());
	}
}

void LLFloaterMediaBrowser::setCurrentURL(const std::string& url)
{
	mCurrentURL = url;
	// redirects will navigate momentarily to about:blank: don't add to history
	if (mCurrentURL != "about:blank")
	{
		mAddressCombo->remove(mCurrentURL);
		mAddressCombo->add(mCurrentURL, ADD_SORTED);
		mAddressCombo->selectByValue(mCurrentURL);

		// Serialize url history
		LLURLHistory::removeURL("browser", mCurrentURL);
		LLURLHistory::addURL("browser", mCurrentURL);
	}

	mBackButton->setEnabled(mBrowser->canNavigateBack());
	mForwardButton->setEnabled(mBrowser->canNavigateForward());
	mReloadButton->setEnabled(true);
}

LLFloaterMediaBrowser* LLFloaterMediaBrowser::showInstance(const LLSD& media_url,
														   bool trusted)
{
	LLFloaterMediaBrowser* floaterp = LLUISingleton<LLFloaterMediaBrowser,
													VisibilityPolicy<LLFloater> >::showInstance(media_url);

	floaterp->openMedia(media_url.asString(), trusted);
	return floaterp;
}

//static
void LLFloaterMediaBrowser::onEnterAddress(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (self)
	{
		self->mBrowser->navigateTo(self->mAddressCombo->getValue().asString());
	}
}

//static
void LLFloaterMediaBrowser::onClickRefresh(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (self)
	{
		self->mAddressCombo->remove(0);
		std::string url = self->mCurrentURL;
		// Force a reload by changing the page first
		self->mBrowser->navigateTo("about:blank");
		self->mBrowser->navigateTo(url);
	}
}

//static
void LLFloaterMediaBrowser::onClickForward(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (self)
	{
		self->mBrowser->navigateForward();
	}
}

//static
void LLFloaterMediaBrowser::onClickBack(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (self)
	{
		self->mBrowser->navigateBack();
	}
}

//static
void LLFloaterMediaBrowser::onClickGo(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;

	self->mBrowser->navigateTo(self->mAddressCombo->getValue().asString());
}

//static
void LLFloaterMediaBrowser::onClickClose(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterMediaBrowser::onClickOpenWebBrowser(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (self)
	{
		// NOTE: we favour the URL in the combo box, because in case of a page
		// loading failure (SSL handshake failures, for example), mCurrentURL
		// contains about:blank or another URL than the failed page URL...
		std::string url = self->mAddressCombo->getValue().asString();
		if (url.empty())
		{
			url = self->mCurrentURL;
		}
		if (url.empty())
		{
			url = self->mBrowser->getHomePageUrl();
		}
		LLWeb::loadURLExternal(url);
	}
}

void LLFloaterMediaBrowser::onClickAssign(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (!self) return;

	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (!parcel)
	{
		return;
	}

	std::string media_url = self->mAddressCombo->getValue().asString();
	LLStringUtil::trim(media_url);

	if (parcel->getMediaType() != HTTP_CONTENT_TEXT_HTML)
	{
		parcel->setMediaURL(media_url);
		parcel->setMediaCurrentURL(media_url);
		parcel->setMediaType(HTTP_CONTENT_TEXT_HTML);
		gViewerParcelMgr.sendParcelPropertiesUpdate(parcel, true);
		LLViewerParcelMedia::sendMediaNavigateMessage(media_url);
		LLViewerParcelMedia::stop();
		// LLViewerParcelMedia::update(parcel);
	}
	LLViewerParcelMedia::sendMediaNavigateMessage(media_url);
}

//static
void LLFloaterMediaBrowser::onClickRewind(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;

	if (self && self->mBrowser->getMediaPlugin())
	{
		self->mBrowser->getMediaPlugin()->start(-2.0f);
	}
}

//static
void LLFloaterMediaBrowser::onClickPlay(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (!self) return;

	LLPluginClassMedia* plugin = self->mBrowser->getMediaPlugin();
	if (plugin)
	{
		if (plugin->getStatus() == LLPluginClassMediaOwner::MEDIA_PLAYING)
		{
			plugin->pause();
		}
		else
		{
			plugin->start();
		}
	}
}

//static
void LLFloaterMediaBrowser::onClickStop(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;

	if (self && self->mBrowser->getMediaPlugin())
	{
		self->mBrowser->getMediaPlugin()->stop();
	}
}

//static
void LLFloaterMediaBrowser::onClickSeek(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;

	if (self && self->mBrowser->getMediaPlugin())
	{
		self->mBrowser->getMediaPlugin()->start(2.0f);
	}
}

void LLFloaterMediaBrowser::openMedia(const std::string& media_url,
									  bool trusted)
{
	openMedia(media_url, "", trusted);
}

void LLFloaterMediaBrowser::openMedia(const std::string& media_url,
									  const std::string& target,
									  bool trusted)
{
	mBrowser->setHomePageUrl(media_url);
	mBrowser->setTarget(target);
	mBrowser->setTrusted(trusted);
	mAddressCombo->setEnabled(!trusted);
	mGoButton->setEnabled(!trusted);
	mAddressCombo->setVisible(false);
	mLoadingText->setVisible(true);
	mBrowser->navigateTo(media_url);
	setCurrentURL(media_url);
}

////////////////////////////////////////////////////////////////////////////////
//

LLViewerHtmlHelp gViewerHtmlHelp;

// Support for secondlife:///app/help/{TOPIC} SLapps
// NOTE: TOPIC is ignored (it's pretty dumb anyway: only pre and post login
// topics are used in LL's v3 viewer).
class LLHelpHandler : public LLCommandHandler
{
public:
	// requests will be throttled from a non-trusted browser
	LLHelpHandler() : LLCommandHandler("help", UNTRUSTED_THROTTLE) {}

	bool handle(const LLSD& params, const LLSD& query_map, LLMediaCtrl* web)
	{
		gViewerHtmlHelp.show();
		return true;
	}
};
LLHelpHandler gHelpHandler;

LLViewerHtmlHelp::LLViewerHtmlHelp()
{
	LLUI::setHtmlHelp(this);
}

LLViewerHtmlHelp::~LLViewerHtmlHelp()
{
	LLUI::setHtmlHelp(NULL);
}

void LLViewerHtmlHelp::show()
{
	show("");
}

void LLViewerHtmlHelp::show(std::string url)
{
	LLFloaterMediaBrowser* floater_html = LLFloaterMediaBrowser::getInstance();
	floater_html->setVisible(false);

	if (url.empty())
	{
		url = floater_html->getSupportURL();
	}

	if (gSavedSettings.getBool("UseExternalBrowser"))
	{
		LLSD notificationData;
		notificationData["url"] = url;

		gNotifications.add("ClickOpenF1Help", notificationData, LLSD(),
						   onClickF1HelpLoadURL);
		floater_html->close();
	}
	else
	{
		// don't wait, just do it
		floater_html->setVisible(true);
		floater_html->openMedia(url);
	}
}

//static
bool LLViewerHtmlHelp::onClickF1HelpLoadURL(const LLSD& notification,
											const LLSD& response)
{
	LLFloaterMediaBrowser* floater_html = LLFloaterMediaBrowser::getInstance();
	floater_html->setVisible(false);
	std::string url = floater_html->getSupportURL();
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLWeb::loadURL(url);
	}
	floater_html->close();
	return false;
}

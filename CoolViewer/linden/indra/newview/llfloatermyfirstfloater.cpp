/**
 * @file llFloaterMyFirstFloater.cpp
 * @brief LLFloaterMyFirstFloater class implementation
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

/**
 * Actually the "Chat History" floater.
 * Should be llFloaterMyFirstFloaterhistory, not llFloaterMyFirstFloater.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloatermyfirstfloater.h"

#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llconsole.h"
#include "llstylemap.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llchatbar.h"
#include "llfloateractivespeakers.h"
#include "llfloaterchat.h"
#include "llfloatermute.h"
#include "llfloaterscriptdebug.h"
#include "lllogchat.h"
#include "llmutelist.h"
#include "llfloaterchatterbox.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewergesture.h"			// For triggering gestures
#include "llviewermenu.h"
#include "llviewertexteditor.h"
#include "llvoavatarself.h"
#include "llslurl.h"
#include "llstatusbar.h"
#include "hbviewerautomation.h"
#include "llweb.h"

static std::set<std::string> sHighlightWords;

//
// Helper functions
//

extern LLColor4 get_agent_chat_color(const LLChat& chat);
extern LLColor4 get_text_color(const LLChat& chat);
extern void add_timestamped_line(LLViewerTextEditor* edit, LLChat chat,const LLColor4& color);
extern void log_chat_text(const LLChat& chat);
extern bool make_words_list();

//
// Member Functions
//
LLFloaterMyFirstFloater::LLFloaterMyFirstFloater(const LLSD& seed)
  :	LLFloater("chat floater", "FloaterMyFirstFloaterRect", "", RESIZE_YES, 440, 100,
      DRAG_ON_TOP, MINIMIZE_NO, CLOSE_YES),
  mChatBarPanel(NULL),
  mSpeakerPanel(NULL),
  mToggleActiveSpeakersBtn(NULL),
  mHistoryWithoutMutes(NULL),
  mHistoryWithMutes(NULL),
  mFocused(false)
{
  std::string xml_file;
  if (gSavedSettings.getBool("UseOldChatHistory"))
  {
    xml_file = "floater_chat_history2.xml";
  }
  else
  {
    xml_file = "floater_chat_history.xml";
    mFactoryMap["chat_panel"] = LLCallbackMap(createChatPanel, this);
  }
  mFactoryMap["active_speakers_panel"] = LLCallbackMap(createSpeakersPanel,
      this);

  // false so to not automatically open singleton floaters (as result of
  // getInstance())
  LLUICtrlFactory::getInstance()->buildFloater(this, xml_file,
      &getFactoryMap(), false);
}

//virtual
bool LLFloaterMyFirstFloater::postBuild()
{
  if (mChatBarPanel)
  {
    mChatBarPanel->setGestureCombo(getChild<LLComboBox>("Gesture",
          true, false));
  }

  childSetCommitCallback("show mutes", onClickToggleShowMute, this);

  mHistoryWithoutMutes = getChild<LLViewerTextEditor>("Chat History Editor");
  mHistoryWithoutMutes->setPreserveSegments(true);
  mHistoryWithMutes = getChild<LLViewerTextEditor>("Chat History Editor with mute");
  mHistoryWithMutes->setPreserveSegments(true);
  mHistoryWithMutes->setVisible(false);

  mToggleActiveSpeakersBtn = getChild<LLButton>("toggle_active_speakers_btn");
  mToggleActiveSpeakersBtn->setClickedCallback(onClickToggleActiveSpeakers,
      this);

  return true;
}

//virtual
void LLFloaterMyFirstFloater::setVisible(bool visible)
{
  LLFloater::setVisible(visible);
  gSavedSettings.setBool("ShowChatHistory", visible);
}

//virtual
void LLFloaterMyFirstFloater::draw()
{
  bool active_speakers_panel = mSpeakerPanel && mSpeakerPanel->getVisible();
  mToggleActiveSpeakersBtn->setValue(active_speakers_panel);
  if (active_speakers_panel)
  {
    mSpeakerPanel->refreshSpeakers();
  }

  if (mChatBarPanel)
  {
    mChatBarPanel->refresh();
  }

  LLFloater::draw();
}

//virtual
void LLFloaterMyFirstFloater::onClose(bool app_quitting)
{
  if (!app_quitting)
  {
    gSavedSettings.setBool("ShowChatHistory", false);
  }
  setVisible(false);
  mFocused = false;
}

//virtual
void LLFloaterMyFirstFloater::onVisibilityChange(bool new_visibility)
{
  // Hide the chat overlay when our history is visible.
  updateConsoleVisibility();

  // stop chat history tab from flashing when it appears
  if (new_visibility)
  {
    LLFloaterChatterBox::getInstance()->setFloaterFlashing(this, false);
  }

  LLFloater::onVisibilityChange(new_visibility);
}

//virtual
void LLFloaterMyFirstFloater::setMinimized(bool minimized)
{
  LLFloater::setMinimized(minimized);
  updateConsoleVisibility();
}

//virtual
void LLFloaterMyFirstFloater::onFocusReceived()
{
  // This keeps track of the panel focus, independently of the keyboard
  // focus (which might get stolen by the main chat bar). Also, we don't
  // register a focused event if the chat floater got its own chat bar
  // (in which case the latter will actually receive the keyboard focus).
  if (!mChatBarPanel)
  {
    mFocused = true;
  }
}

//virtual
void LLFloaterMyFirstFloater::onFocusLost()
{
  mFocused = false;
}

void LLFloaterMyFirstFloater::updateConsoleVisibility()
{
  if (!gConsolep) return;

  // Determine whether we should show console due to not being visible
  gConsolep->setVisible(isMinimized() ||	// are we minimized ?
      // are we not in part of UI being drawn ?
      !isInVisibleChain() ||
      // are we hosted in a minimized floater ?
      (getHost() && getHost()->isMinimized()));
}

//static
void LLFloaterMyFirstFloater::addChatHistory(LLChat& chat, bool log_to_file)
{
  LLFloaterMyFirstFloater* self = LLFloaterMyFirstFloater::getInstance(LLSD());

  static LLCachedControl<bool> log_chat(gSavedPerAccountSettings, "LogChat");
  if (log_to_file && log_chat)
  {
    log_chat_text(chat);
  }

  LLColor4 color;
  if (log_to_file)
  {
    color = get_text_color(chat);
  }
  else
  {
    color = LLColor4::grey;	// Recap from log file.
  }

  if (chat.mChatType == CHAT_TYPE_DEBUG_MSG)
  {
    LLFloaterScriptDebug::addScriptLine(chat.mText, chat.mFromName,
        color, chat.mFromID);
    if (!gSavedSettings.getBool("ScriptErrorsAsChat"))
    {
      return;
    }
  }

  // Could flash the chat button in the status bar here. JC

  self->mHistoryWithoutMutes->setParseHTML(true);
  self->mHistoryWithMutes->setParseHTML(true);

  if (!chat.mMuted)
  {
    add_timestamped_line(self->mHistoryWithoutMutes, chat, color);
    add_timestamped_line(self->mHistoryWithMutes, chat, color);
  }
  else
  {
    // Desaturate muted chat
    LLColor4 muted_color = lerp(color, LLColor4::grey, 0.5f);
    add_timestamped_line(self->mHistoryWithMutes, chat, muted_color);
  }

  // Add objects as transient speakers that can be muted
  if (chat.mSourceType == CHAT_SOURCE_OBJECT)
  {
    self->mSpeakerPanel->setSpeaker(chat.mFromID, chat.mFromName,
        LLSpeaker::STATUS_NOT_IN_CHANNEL,
        LLSpeaker::SPEAKER_OBJECT,
        chat.mOwnerID);
  }

  // Start tab flashing on incoming text from other users (ignoring system
  // text, etc)
  if (!self->isInVisibleChain() && chat.mSourceType == CHAT_SOURCE_AGENT)
  {
    LLFloaterChatterBox::getInstance()->setFloaterFlashing(self, true);
  }
}

//static
void LLFloaterMyFirstFloater::setHistoryCursorAndScrollToEnd()
{
  LLFloaterMyFirstFloater* self = LLFloaterMyFirstFloater::getInstance(LLSD());
  if (!self) return;

  if (self->mHistoryWithoutMutes)
  {
    self->mHistoryWithoutMutes->setCursorAndScrollToEnd();
  }
  if (self->mHistoryWithMutes)
  {
    self->mHistoryWithMutes->setCursorAndScrollToEnd();
  }
}

//static
void LLFloaterMyFirstFloater::onClickMute(void* data)
{
  LLFloaterMyFirstFloater* self = (LLFloaterMyFirstFloater*)data;

  LLComboBox*	chatter_combo = self->getChild<LLComboBox>("chatter combobox");

  const std::string& name = chatter_combo->getSimple();
  LLUUID id = chatter_combo->getCurrentID();

  if (name.empty()) return;

  LLMute mute(id);
  mute.setFromDisplayName(name);
  if (LLMuteList::add(mute))
  {
    LLFloaterMute::selectMute(mute.mID);
  }
}

//static
void LLFloaterMyFirstFloater::onClickToggleShowMute(LLUICtrl* ctrl, void* data)
{
  LLFloaterMyFirstFloater* self = (LLFloaterMyFirstFloater*)data;
  LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
  if (!check || !self || !self->mHistoryWithoutMutes ||
      !self->mHistoryWithMutes)
  {
    return;
  }

  if (check->get())
  {
    self->mHistoryWithoutMutes->setVisible(false);
    self->mHistoryWithMutes->setVisible(true);
    self->mHistoryWithMutes->setCursorAndScrollToEnd();
  }
  else
  {
    self->mHistoryWithMutes->setVisible(false);
    self->mHistoryWithoutMutes->setVisible(true);
    self->mHistoryWithoutMutes->setCursorAndScrollToEnd();
  }
}

// Put a line of chat in all the right places
//static
void LLFloaterMyFirstFloater::addChat(LLChat& chat, bool from_im, bool local_agent)
{
  LLFloaterMyFirstFloater* self = LLFloaterMyFirstFloater::getInstance(LLSD());
  if (!self) return;

  //MK
  if (gRLenabled && chat.mText == "")
  {
    // In case crunchEmote() returned an empty string, just abort.
    return;
  }
  //mk

  LLColor4 text_color = get_text_color(chat);

  bool no_script_debug = chat.mChatType == CHAT_TYPE_DEBUG_MSG &&
    !gSavedSettings.getBool("ScriptErrorsAsChat");

  if (!no_script_debug && !local_agent && gConsolep && !chat.mMuted)
  {
    if (chat.mSourceType == CHAT_SOURCE_SYSTEM)
    {
      text_color = gSavedSettings.getColor("SystemChatColor");
    }
    else if (from_im)
    {
      text_color = gSavedSettings.getColor("IMChatColor");
    }
    // We display anything if it is not an IM. If it's an IM, check pref.
    if (!from_im || gSavedSettings.getBool("IMInChatConsole"))
    {
      gConsolep->addConsoleLine(chat.mText, text_color);
    }
  }

  static LLCachedControl<bool> log_im(gSavedPerAccountSettings, "LogChatIM");
  if (from_im && log_im)
  {
    log_chat_text(chat);
  }

  if (from_im)
  {
    if (gSavedSettings.getBool("IMInChatHistory"))
    {
      addChatHistory(chat, false);
    }
  }
  else
  {
    addChatHistory(chat, true);
  }

  resolveSLURLs(chat);
}

//static
void LLFloaterMyFirstFloater::resolveSLURLs(const LLChat& chat)
{
  LLFloaterMyFirstFloater* self = LLFloaterMyFirstFloater::findInstance(LLSD());
  if (!self) return;

  // SLURLs resolving: fetch the Ids associated with avatar/group/experience
  // name SLURLs present in the text.
  uuid_list_t agent_ids = LLSLURL::findSLURLs(chat.mText);
  if (agent_ids.empty()) return;

  // Add to the existing list of pending Ids
  self->mPendingIds.insert(agent_ids.begin(), agent_ids.end());

  // Launch the SLURLs resolution. Note that the substituteSLURL() callback
  // will be invoked immediately for names already in cache. That's why we
  // needed to push the untranslated SLURLs in the chat first (together with
  // the fact that doing so, gets the SLURLs auto-parsed and puts a link
  // segment on them in the text editor, segment link that will be preserved
  // when the SLURL will be replaced with the corresponding name).
  LLSLURL::resolveSLURLs();
}

//static
void LLFloaterMyFirstFloater::substituteSLURL(const LLUUID& id, const std::string& slurl,
    const std::string& substitute)
{
  LLFloaterMyFirstFloater* self = LLFloaterMyFirstFloater::findInstance(LLSD());
  if (self && self->mPendingIds.count(id))
  {
    self->mHistoryWithoutMutes->replaceTextAll(slurl, substitute, true);
    self->mHistoryWithoutMutes->setEnabled(false);
    self->mHistoryWithMutes->replaceTextAll(slurl, substitute, true);
    self->mHistoryWithMutes->setEnabled(false);
    if (gConsolep)
    {
      gConsolep->replaceAllText(slurl, substitute, true);
    }
  }
}

//static
void LLFloaterMyFirstFloater::substitutionDone(const LLUUID& id)
{
  LLFloaterMyFirstFloater* self = LLFloaterMyFirstFloater::findInstance(LLSD());
  if (self)
  {
    self->mPendingIds.erase(id);
  }
}
//static
void LLFloaterMyFirstFloater::loadHistory()
{
  LLLogChat::loadHistory(std::string("chat"), &chatFromLogFile,
      (void*)LLFloaterMyFirstFloater::getInstance(LLSD()));
}

//static
void LLFloaterMyFirstFloater::chatFromLogFile(LLLogChat::ELogLineType type,
    std::string line,
    void* userdata)
{
  switch (type)
  {
    case LLLogChat::LOG_EMPTY:
    case LLLogChat::LOG_END:
      // *TODO: nice message from XML file here
      break;

    case LLLogChat::LOG_LINE:
      {
        LLChat chat;
        chat.mText = line;
        addChatHistory(chat, false);
        break;
      }

    default:
      // nothing
      break;
  }
}

//static
void* LLFloaterMyFirstFloater::createSpeakersPanel(void* data)
{
  LLFloaterMyFirstFloater* self = (LLFloaterMyFirstFloater*)data;
  self->mSpeakerPanel =
    new LLPanelActiveSpeakers(LLLocalSpeakerMgr::getInstance(), true);
  return self->mSpeakerPanel;
}

//static
void* LLFloaterMyFirstFloater::createChatPanel(void* data)
{
  LLFloaterMyFirstFloater* self = (LLFloaterMyFirstFloater*)data;
  self->mChatBarPanel = new LLChatBar("floating_chat_bar");
  return self->mChatBarPanel;
}

//static
void LLFloaterMyFirstFloater::onClickToggleActiveSpeakers(void* userdata)
{
  LLFloaterMyFirstFloater* self = (LLFloaterMyFirstFloater*)userdata;
  //MK
  if (gRLenabled && gRLInterface.mContainsShownames)
  {
    if (!self->mSpeakerPanel->getVisible()) return;
  }
  //mk
  self->mSpeakerPanel->setVisible(!self->mSpeakerPanel->getVisible());
}

//static
bool LLFloaterMyFirstFloater::visible(LLFloater* instance, const LLSD& key)
{
  return VisibilityPolicy<LLFloater>::visible(instance, key);
}

//static
void LLFloaterMyFirstFloater::show(LLFloater* instance, const LLSD& key)
{
  VisibilityPolicy<LLFloater>::show(instance, key);
}

//static
void LLFloaterMyFirstFloater::hide(LLFloater* instance, const LLSD& key)
{
  if (instance->getHost())
  {
    LLFloaterChatterBox::hideInstance();
  }
  else
  {
    VisibilityPolicy<LLFloater>::hide(instance, key);
  }
}

//static
void LLFloaterMyFirstFloater::focus()
{
  LLFloaterMyFirstFloater* self = (LLFloaterMyFirstFloater*)findInstance();
  if (self)
  {
    self->setFocus(true);
  }
}

//static
bool LLFloaterMyFirstFloater::isFocused()
{
  LLFloaterMyFirstFloater* self = (LLFloaterMyFirstFloater*)findInstance();
  return self && self->mFocused;
}

//static
bool LLFloaterMyFirstFloater::isOwnNameInText(const std::string& text_line)
{
  if (!isAgentAvatarValid())
  {
    return false;
  }
  const std::string separators(" .,;:'!?*-()[]\"");
  bool flag;
  char before, after;
  size_t index = 0, larger_index, length = 0;
  std::set<std::string>::iterator it;
  std::string name;
  std::string text = " " + text_line + " ";
  LLStringUtil::toLower(text);

  if (make_words_list())
  {
    name = "Highlights words list changed to: ";
    flag = false;
    for (it = sHighlightWords.begin(); it != sHighlightWords.end(); ++it)
    {
      if (flag)
      {
        name += ", ";
      }
      name += *it;
      flag = true;
    }
    llinfos << name << llendl;
  }

  do
  {
    flag = false;
    larger_index = 0;
    for (it = sHighlightWords.begin(); it != sHighlightWords.end(); ++it)
    {
      name = *it;
      index = text.find(name);
      if (index != std::string::npos)
      {
        flag = true;
        before = text.at(index - 1);
        after = text.at(index + name.length());
        if (separators.find(before) != std::string::npos &&
            separators.find(after) != std::string::npos)
        {
          return true;
        }
        if (index >= larger_index)
        {
          larger_index = index;
          length = name.length();
        }
      }
    }
    if (flag)
    {
      text = " " + text.substr(index + length);
    }
  }
  while (flag);

  return false;
}

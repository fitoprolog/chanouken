/**
 * @file llappviewer.cpp
 * @brief The LLAppViewer class definitions
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

#include "llviewerprecompiledheaders.h"

#include "boost/make_shared.hpp"

#if LL_WINDOWS
# include <share.h>					// For _SH_DENYWR in initMarkerFile
#else
# include <sys/file.h>				// For initMarkerFile support
#endif

#include "cef/dullahan.h"			// For HB_DULLAHAN_EXTENDED

#include "llappviewer.h"

#include "llalertdialog.h"
#include "llapp.h"
#include "llassetstorage.h"
#include "llaudioengine.h"
#include "llbase64.h"
#include "llbutton.h"
#include "llcachename.h"
#include "llcallbacklist.h"
#include "llcombobox.h"
#include "llcommandlineparser.h"
#include "llconsole.h"
#include "llcoproceduremanager.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "lldiskcache.h"
#include "llerrorcontrol.h"
#include "llexperiencecache.h"
#include "lleventtimer.h"
#include "llevents.h"
#include "hbfileselector.h"
#include "llfontfreetype.h"
#include "llgl.h"
#include "llimagej2c.h"
#include "llimageworker.h"
#include "llinitdestroyclass.h"
#include "llkeyframemotion.h"
#include "lllfsthread.h"
#include "llmd5.h"
#include "llmimetypes.h"
#include "llnotifications.h"
#include "llparcel.h"
#include "llpolymesh.h"
#include "llprimitive.h"
#include "llproxy.h"
#include "llpumpio.h"
#include "llrender.h"
#include "llsdserialize.h"
#include "llsettingstype.h"
#include "llspellcheck.h"
#include "llsys.h"
#include "lltexteditor.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "lluitrans.h"
#include "llurlhistory.h"
#include "lluserauth.h"
#include "llversionviewer.h"
#include "llvertexbuffer.h"
#include "llvolume.h"
#include "llvolumemgr.h"
#include "llwindow.h"
#include "llwatchdog.h"
#if LL_WINDOWS
# include "llwindowwin32.h"			// For gIgnoreHiDPIEvents
#endif
#include "llworkerthread.h"
#include "llxfermanager.h"
#include "llxmlrpctransaction.h"
#include "llxorcipher.h"

#include "llagent.h"
#include "llagentpilot.h"
#if LL_LINUX
# include "llappviewerlinux.h"		// For pumpGlib()
#endif
#include "llavatartracker.h"
#include "lldebugview.h"
#include "lldrawpoolbump.h"
#include "lleventnotifier.h"
#include "llenvironment.h"
#include "llfasttimerview.h"		// For HBTracyProfiler
#include "llfeaturemanager.h"
#include "llflexibleobject.h"
#include "hbfloaterareasearch.h"
#include "hbfloaterbump.h"
#include "llfloaterim.h"
#include "llfloaterinventory.h"
#include "llfloaterjoystick.h"
#include "llfloatersnapshot.h"
#include "llfloaterstats.h"
#include "llfolderview.h"
#include "llgesturemgr.h"
#include "llgridmanager.h"
#include "llgroupmgr.h"
#include "llgroupnotify.h"
#include "llhoverview.h"
#include "llhudeffectlookat.h"
#include "llhudeffectspiral.h"
#include "llhudmanager.h"
#include "llimmgr.h"
#include "lllocalbitmaps.h"
#include "llmeshrepository.h"
#include "llmutelist.h"
#include "llnotify.h"
#include "llpanelworldmap.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llskinningutil.h"
#include "llsky.h"
#include "llslurl.h"
#include "llstartup.h"
#include "llstatusbar.h"
#include "llsurface.h"
#include "lltexturecache.h"
#include "lltexturefetch.h"
#include "lltoolbar.h"
#include "lltoolmgr.h"
#include "lltracker.h"
#include "llurldispatcher.h"
#include "llvieweraudio.h"
#include "hbviewerautomation.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewerjoystick.h"
#include "llviewerkeyboard.h"
#include "llviewermedia.h"
#include "llviewermediafocus.h"
#include "llviewermenu.h"
#include "llviewermessage.h"		// For send_agent_update()
#include "llviewerobjectlist.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llviewerpartsim.h"
#include "llviewershadermgr.h"
#include "llviewerstats.h"
#include "llviewertextureanim.h"
#include "llviewertexturelist.h"
#include "llviewerthrottle.h"
#include "llviewerwindow.h"
#include "llvlmanager.h"
#include "llvoavatarself.h"
#include "llvocache.h"
#include "llvoicechannel.h"
#include "llvopartgroup.h"
#include "llvosky.h"
#include "llvosurfacepatch.h"
#include "llvotree.h"
#include "llvovolume.h"
#include "llvowlsky.h"
#include "llweb.h"
#include "llwlskyparammgr.h"
#include "llwlwaterparammgr.h"
#include "llworld.h"
#include "llworldmap.h"

#if LL_JEMALLOC
# include "jemalloc/jemalloc.h"

// We configure four general purpose arenas, plus the ones we will add for the
// textures, the vertex buffers and the volumes/meshes. We also use transparent
// huge pages, activate the background thread for memory purging, and disable
// profiling by default. Note: you may override these settings with an exported
// MALLOC_CONF environment variable; in particular, Debug and RelWithDebInfo
// builds are linked with a jemalloc build supporting profiling, and you may
// for example, enable memory leaks detection with:
// export MALLOC_CONF="prof_leak:true,prof_final:true"
# if LL_DEBUG || LL_NO_FORCE_INLINE
const char* malloc_conf = "narenas:4,thp:always,background_thread:true,prof:false";
# else	// Release build: no profiling option available in jemalloc.
const char* malloc_conf = "narenas:4,thp:always,background_thread:true";
# endif
#endif

// Global variables

// The single viewer app.
LLAppViewer* gAppViewerp = NULL;

// Thread objects.
LLTextureCache* gTextureCachep = NULL;
LLImageDecodeThread* gImageDecodeThreadp = NULL;
LLTextureFetch* gTextureFetchp = NULL;

std::string gLastRunVersion;
std::string gCurrentVersion;
std::string gViewerVersion;

F32 gSimLastTime; // Used in LLAppViewer::init and LLViewerStats::sendStats()
F32 gSimFrames;

std::string gSecondLife;
std::string gWindowTitle;

eLastExecEvent gLastExecEvent = LAST_EXEC_NORMAL;

LLSD gDebugInfo;

LLPumpIO* gServicePumpIOp = NULL;

S32 gExitCode = LLAppViewer::EXIT_OK;

U32	gFrameCount = 0;
// Number of frames that app window was in foreground:
U32 gForegroundFrameCount = 0;
U64 gFrameTime = 0;
F32 gFrameTimeSeconds = 0.f;
F32 gFrameIntervalSeconds = 0.f;
F32 gFPSClamped = 30.f;	// Pretend we start at target rate.
// Time between adjacent checks to network for packets:
F32 gFrameDTClamped = 0.f;
// gStartTime is "private", used only to calculate gFrameTimeSeconds:
U64	gStartTime = 0;

LLTimer gRenderStartTime;
LLFrameTimer gForegroundTime;
LLTimer gLogoutTimer;
// This will be cut short by the LogoutReply msg:
constexpr F32 LOGOUT_REQUEST_TIME = 6.f;
F32 gLogoutMaxTime = LOGOUT_REQUEST_TIME;

bool gDisconnected = false;

// Minimap scale in pixels per region

// Used to restore texture state after a mode switch
LLFrameTimer gRestoreGLTimer;
bool gRestoreGL = false;
bool gUseWireframe = false;

// Set to true only while the fast timer view is opened
bool gEnableFastTimers = false;

// Memory checks
LLFrameTimer gMemoryCheckTimer;
// Updated in display_stats() in llviewerdisplay.cpp:
U64 gMemoryAllocated = 0;

bool gBalanceObjectCache = true;

std::string gLastVersionChannel;

LLVector3 gWindVec(3.0, 3.0, 0.0);
LLVector3 gRelativeWindVec(0.0, 0.0, 0.0);

U32 gPacketsIn = 0;

bool gAllowTapTapHoldRun = true;
bool gShowObjectUpdates = false;

bool gAcceptTOS = false;
bool gAcceptCriticalMessage = false;

bool gAvatarMovedOnLogin = false;
bool gLogoutInProgress = false;

unsigned char gMACAddress[MAC_ADDRESS_BYTES];

const std::string PREVIOUS_LOG("CoolVLViewer.old");
const std::string CURRENT_LOG("CoolVLViewer.log");
const char* TEMP_LOG_FMT = "CoolVLViewer_%d.log";

// We must keep the same MARKER_FILE_NAME as for other viewers so to be able to
// detect multiple instances... But we use the contents of this file to
// distinguish our marker from others' (see anotherInstanceRunning() and
// initMarkerFile())
const std::string MARKER_FILE_NAME("SecondLife.exec_marker");
// Use custom marker files to avoid being attributed other viewers' crashes !
const std::string ERROR_MARKER_FILE_NAME("CoolVLViewer.error_marker");
const std::string LLERROR_MARKER_FILE_NAME("CoolVLViewer.llerror_marker");
const std::string LOGOUT_MARKER_FILE_NAME("CoolVLViewer.logout_marker");
static bool sLLErrorActivated = false;
static bool sDoDisconnect = false;
static bool sLoggingOut = false;

// Plugin presence
bool gHasGstreamer = false;

// Static members.
const std::string LLAppViewer::sGlobalSettingsName = "Global";
const std::string LLAppViewer::sPerAccountSettingsName = "PerAccount";

///////////////////////////////////////////////////////////////////////////////
// LLControlGroupCLP class
// Uses the command line parser to configure an LLControlGroup
///////////////////////////////////////////////////////////////////////////////
class LLControlGroupCLP : public LLCommandLineParser
{
  protected:
    LOG_CLASS(LLControlGroupCLP);

  public:
    /**
     * @brief Configure the command line parser according the given config file.
     *
     * @param config_filename The name of the XML based LLSD config file.
     * @param clp A reference to the command line parser object to configure.
     *
     * *FIX:Mani Specify config file format.
     */
    void configure(const std::string& config_filename, LLControlGroup* group);

  private:
    static void setControlValueCB(const LLCommandLineParser::token_vector_t& value,
        const std::string& opt_name,
        LLControlGroup* ctrl_group);
};

//static
void LLControlGroupCLP::setControlValueCB(const LLCommandLineParser::token_vector_t& value,
    const std::string& opt_name,
    LLControlGroup* ctrl_group)
{
  // *FIXME: do sematic conversion here ?  LLSD(ImplString) is no good for
  // doing string to type conversion for... booleans, compound types ?...
  LLControlVariable* ctrl = ctrl_group->getControl(opt_name.c_str());
  if (!ctrl)
  {
    llwarns << "Command Line option mapping '" << opt_name
      << "' not found !  Ignoring." << llendl;
    return;
  }

  if (ctrl->type() == TYPE_BOOLEAN)
  {
    if (value.size() > 1)
    {
      llwarns << "Ignoring extra tokens." << llendl;
    }
    if (value.size() > 0)
    {
      // There is a token. Check the string for true/false/1/0 etc.
      bool result = false;
      bool gotSet = LLStringUtil::convertToBool(value[0], result);
      if (gotSet)
      {
        ctrl->setValue(LLSD(result), false);
      }
    }
    else
    {
      ctrl->setValue(LLSD(true), false);
    }
  }
  // For the default types, let llsd do the conversion.
  else if (value.size() > 1 && ctrl->isType(TYPE_LLSD))
  {
    // Assume it is an array...
    LLSD llsd_array;
    for (U32 i = 0; i < value.size(); ++i)
    {
      LLSD llsd_value;
      llsd_value.assign(LLSD::String(value[i]));
      llsd_array.set(i, llsd_value);
    }
    ctrl->setValue(llsd_array, false);
  }
  else if (value.size() > 0)
  {
    if (value.size() > 1)
    {
      llwarns << "Ignoring extra tokens mapped to the setting: "
        << opt_name << "." << llendl;
    }
    LLSD llsd_value;
    llsd_value.assign(LLSD::String(value[0]));
    ctrl->setValue(llsd_value, false);
  }
}

// This method reads the LLSD based config file, and uses it to set members of
// a control group.
void LLControlGroupCLP::configure(const std::string& config_filename,
    LLControlGroup* ctrl_group)
{
  llifstream input_stream(config_filename.c_str(),
      std::ios::in | std::ios::binary);
  if (!input_stream.is_open())
  {
    llwarns << "Could not open: " << config_filename << llendl;
    return;
  }
  LLSD config;
  LLSDSerialize::fromXML(config, input_stream);
  for (LLSD::map_iterator it = config.beginMap(); it != config.endMap();
      ++it)
  {
    LLSD::String long_name = it->first;
    LLSD option_params = it->second;

    std::string desc("n/a");
    if (option_params.has("desc"))
    {
      desc = option_params["desc"].asString();
    }

    std::string short_name = LLStringUtil::null;
    if (option_params.has("short"))
    {
      short_name = option_params["short"].asString();
    }

    unsigned int token_count = 0;
    if (option_params.has("count"))
    {
      token_count = option_params["count"].asInteger();
    }

    bool composing = false;
    if (option_params.has("compose"))
    {
      composing = option_params["compose"].asBoolean();
    }

    bool positional = false;
    if (option_params.has("positional"))
    {
      positional = option_params["positional"].asBoolean();
    }

    bool last_option = false;
    if (option_params.has("last_option"))
    {
      last_option = option_params["last_option"].asBoolean();
    }

    boost::function1<void, const token_vector_t&> callback;
    if (ctrl_group && option_params.has("map-to"))
    {
      std::string ctrl_name = option_params["map-to"].asString();
      callback = boost::bind(&LLControlGroupCLP::setControlValueCB, _1,
          ctrl_name, ctrl_group);
    }

    addOptionDesc(long_name, callback, token_count, desc, short_name,
        composing, positional, last_option);
  }
}

///////////////////////////////////////////////////////////////////////////////
// LLFrameStatsTimer class
// This class is an LLFrameTimer that can be created with an elapsed time that
// starts counting up from the given value rather than 0.0.
// Otherwise it behaves the same way as LLFrameTimer.
///////////////////////////////////////////////////////////////////////////////
class LLFrameStatsTimer : public LLFrameTimer
{
  public:
    LLFrameStatsTimer(F64 elapsed_already = 0.0)
      :	LLFrameTimer()
    {
      mStartTime -= elapsed_already;
    }
};

//----------------------------------------------------------------------------
// File scope definitons

std::string gLoginPage;
std::vector<std::string> gLoginURIs;
static std::string gHelperURI;

// Show only appropriate debug controls in settings editor
static void hide_useless_settings()
{
  LLControlVariable* control = NULL;

#if !LL_FAST_TIMERS_ENABLED
  control = gSavedSettings.getControl("FastTimersAlwaysEnabled");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
#endif

#if !LL_FAST_TEX_CACHE
  control = gSavedSettings.getControl("FastCacheFetchEnabled");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
#endif

#if LL_PENDING_MESH_REQUEST_SORTING
  control = gSavedSettings.getControl("DelayPendingMeshFetchesOnTP");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
#endif

  // Not used under Linux
#if LL_LINUX
  control = gSavedSettings.getControl("RenderHiDPI");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
#else
  control = gSavedSettings.getControl("MainThreadCPUAffinity");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
#endif

#if LL_WINDOWS
  control = gSavedSettings.getControl("WatchdogEnabledWindows");
  if (control)
  {
    control->setHiddenFromUser(false);
  }
  control = gSavedSettings.getControl("ShowConsoleWindow");
  if (control)
  {
    control->setHiddenFromUser(false);
  }
  control = gSavedSettings.getControl("IgnoreHiDPIEvents");
  if (control)
  {
    control->setHiddenFromUser(false);
  }
#else
  control = gSavedSettings.getControl("WatchdogEnabled");
  if (control)
  {
    control->setHiddenFromUser(false);
  }
#endif

#if LL_DARWIN
  control = gSavedSettings.getControl("UseIOUring");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
#elif LL_LINUX
  LLOSInfo* osinfop = LLOSInfo::getInstance();
  U32 major = osinfop->getKernelVersionMajor();
  U32 minor = osinfop->getKernelVersionMinor();
  if (major < 5 || (major == 5 && minor < 6))
  {
    control = gSavedSettings.getControl("UseIOUring");
    if (control)
    {
      llinfos << "Cannot use io_uring on this kernel; version too old: "
        << major << "." << minor << llendl;
      control->setValue(LLSD::Boolean(false));
      control->setHiddenFromUser(true);
    }
  }
#endif

#if !LL_LINUX
  control = gSavedPerAccountSettings.getControl("LuaAcceptDbusCommands");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
#endif

  // There is no reliable way to find out the available virtual address space
  // under macOS neither any way to force-release to the OS the memory freed
  // by the viewer... Memory safety checks cannot work properly in these
  // conditions.
#if LL_DARWIN && !LL_JEMALLOC
  control = gSavedSettings.getControl("MemorySafetyChecks");
  if (control)
  {
    control->setValue(false);
    control->setHiddenFromUser(true);
  }
  control = gSavedSettings.getControl("MainMemorySafetyMargin");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
  control = gSavedSettings.getControl("SafetyMargin1stStepRatio");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
  control = gSavedSettings.getControl("AllowSwapping");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
#endif

#if !LL_LINUX || !LL_FMOD
  control = gSavedSettings.getControl("FMODDisableALSA");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
  control = gSavedSettings.getControl("FMODDisablePulseAudio");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
#endif
#if !LL_FMOD
  control = gSavedSettings.getControl("AudioDisableFMOD");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
#endif
#if !LL_OPENAL
  control = gSavedSettings.getControl("AudioDisableOpenAL");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
#endif

#ifndef HB_DULLAHAN_EXTENDED
  control = gSavedSettings.getControl("CEFPreferredFont");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
  control = gSavedSettings.getControl("CEFMinimumFontSize");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
  control = gSavedSettings.getControl("CEFDefaultFontSize");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
  control = gSavedSettings.getControl("CEFRemoteFonts");
  if (control)
  {
    control->setHiddenFromUser(true);
  }
#endif

  // Check plugins existence for this particular build/installation

  std::string plugin_file =
    gDirUtilp->getLLPluginFilename("media_plugin_cef");
  if (!LLFile::isfile(plugin_file))
  {
    llwarns << "No web browser plugin found !" << llendl;
  }

  plugin_file = gDirUtilp->getLLPluginFilename("media_plugin_gstreamer");
  gHasGstreamer = LLFile::isfile(plugin_file);
  if (!gHasGstreamer)
  {
    llwarns << "No streaming media plugin found !" << llendl;
  }
}

// Deals with settings that must be passed to already constructed classes or
// affected to global variables.
static void settings_to_globals()
{
  gButtonHPad			= gSavedSettings.getS32("ButtonHPad");
  gButtonVPad			= gSavedSettings.getS32("ButtonVPad");
  gBtnHeightSmall		= gSavedSettings.getS32("ButtonHeightSmall");
  gBtnHeight			= gSavedSettings.getS32("ButtonHeight");
  gMenuBarHeight		= gSavedSettings.getS32("MenuBarHeight");
  gStatusBarHeight	= gSavedSettings.getS32("StatusBarHeight");
  // Not used under Linux
#if !LL_LINUX
  gHiDPISupport		= gSavedSettings.getBool("RenderHiDPI");
#endif
#if LL_WINDOWS
  gIgnoreHiDPIEvents	= gSavedSettings.getBool("IgnoreHiDPIEvents");
#endif

  // For HTML parsing in text boxes.
  LLTextEditor::setLinksColor(gSavedSettings.getColor4("HTMLLinkColor"));

  LLRender::sGLCoreProfile = gSavedSettings.getBool("RenderGLCoreProfile");

  gClearARBbuffer = gGLManager.mIsNVIDIA ||
    gSavedSettings.getBool("RenderClearARBBuffer");

  gFocusMgr.setFocusColor(gColors.getColor("FocusColor"));

  LLFloaterView::setStackMinimizedTopToBottom(gSavedSettings.getBool("StackMinimizedTopToBottom"));
  LLFloaterView::setStackMinimizedRightToLeft(gSavedSettings.getBool("StackMinimizedRightToLeft"));
  LLFloaterView::setStackScreenWidthFraction(gSavedSettings.getU32("StackScreenWidthFraction"));

  LLSurface::setTextureSize(gSavedSettings.getU32("RegionTextureSize"));

  LLImageGL::sGlobalUseAnisotropic = gSavedSettings.getBool("RenderAnisotropic");

  // clamp auto-open time to some minimum usable value
  LLFolderView::sAutoOpenTime =
    llmax(0.25f, gSavedSettings.getF32("FolderAutoOpenDelay"));
  LLToolBar::sInventoryAutoOpenTime =
    gSavedSettings.getF32("InventoryAutoOpenDelay");

  LLMemoryInfo::setAllowSwapping(gSavedSettings.getBool("AllowSwapping"));
  LLMemory::setSafetyMarginKB(gSavedSettings.getU32("MainMemorySafetyMargin") *
      1024,
      gSavedSettings.getF32("SafetyMargin1stStepRatio"));

  LLFile::sFlushOnWrite = gSavedSettings.getBool("FSFlushOnWrite");

  gAgent.mHideGroupTitle = gSavedSettings.getBool("RenderHideGroupTitle");

  gDebugWindowProc = gSavedSettings.getBool("DebugWindowProc");
  gAllowTapTapHoldRun = gSavedSettings.getBool("AllowTapTapHoldRun");
  gShowObjectUpdates = gSavedSettings.getBool("ShowObjectUpdates");
  LLPanelWorldMap::sMapScale = gSavedSettings.getF32("MapScale");
  LLHoverView::sShowHoverTips = gSavedSettings.getBool("ShowHoverTips");
  LLAvatarName::sLegacyNamesForFriends =
    gSavedSettings.getBool("LegacyNamesForFriends");
  LLAvatarName::sLegacyNamesForSpeakers =
    gSavedSettings.getBool("LegacyNamesForSpeakers");

  // Setup the spell checker
  LLSpellCheck* spchk = LLSpellCheck::getInstance();
  spchk->setSpellCheck(gSavedSettings.getBool("SpellCheck"));
  spchk->setShowMisspelled(gSavedSettings.getBool("SpellCheckShow"));
  spchk->setDictionary(gSavedSettings.getString("SpellCheckLanguage"));

  LLVolume::sOptimizeCache =
    gSavedSettings.getBool("RenderOptimizeMeshVertexCache");

  LLVOSurfacePatch::sLODFactor =
    gSavedSettings.getF32("RenderTerrainLODFactor");
  // Square lod factor to get exponential range of [1, 4]
  LLVOSurfacePatch::sLODFactor *= LLVOSurfacePatch::sLODFactor;

  gDebugGL = gSavedSettings.getBool("RenderDebugGL");
  gUseNewShaders = gSavedSettings.getBool("UseNewShaders");

  LLHUDEffectLookAt::updateSettings();
}

class LLUITranslationBridge final : public LLTranslationBridge
{
  public:
    std::string getString(const std::string& xml_desc) override
    {
      return LLTrans::getString(xml_desc);
    }
};

//----------------------------------------------------------------------------
// LLAppViewer class
//----------------------------------------------------------------------------

LLAppViewer::LLAppViewer()
  :	mMarkerFile(NULL),
  mOwnsLogoutMarkerFile(false),
  mNumSessions(0),
  mPurgeCache(false),
  mPurgeOnExit(false),
  mSecondInstance(false),
  mIsOurViewer(false),
  mSameBranchViewer(false),
  mIsSiblingViewer(false),
  mSavedFinalSnapshot(false),
  mSavePerAccountSettings(false),
  mQuitRequested(false),
  mLogoutRequestSent(false),
  mLastAgentControlFlags(0),
  mLastAgentForceUpdate(0),
  mMainloopTimeoutDefault(20.f),
  mMainloopTimeout(NULL),
  mAgentRegionLastAlive(false)
{
  if (gAppViewerp)
  {
    llerrs << "An instance of LLAppViewer already exists !" << llendl;
  }

  gAppViewerp = this;
}

LLAppViewer::~LLAppViewer()
{
  destroyMainloopTimeout();

  // If we got to this destructor somehow, the app did not hang.
  removeMarkerFile();

  gAppViewerp = NULL;
}

// Start of the application
//
// IMPORTANT !  Do NOT put anything that will write into the log files during
// normal startup until AFTER we run the "program crashed last time" error
// handler below.
LLApp::InitState LLAppViewer::init()
{
  // Reserve some memory space that will get freed on crash.
  LLMemory::initClass();

  // Initialize the translation bridge for LLWearableType...
  LLTranslationBridge::ptr_t trans =
    boost::make_shared<LLUITranslationBridge>();
  LLWearableType::initClass(trans);
  // ... and LLSettingsType
  LLSettingsType::initClass(trans);

  // Initialize SSE2 math
  LLVector4a::initClass();

  // Initialize particle index pool
  LLVOPartGroup::initClass();

  // Need to do this initialization before we do anything else, since
  // anything that touches files should really go through the lldir API
  gDirUtilp->initAppDirs("SecondLife");
  // set skin search path to default, will be overridden later: this
  // allows simple skinned file lookups to work
  gDirUtilp->setSkinFolder("default");

  initLogging();

  // OK to write stuff to logs now, we have now crash reported if necessary

  // This sets LLError::Log::sIsBeingDebugged appropriatley to abort()
  // instead of crashing when encountering an llerrs while being debugged.
  if (gAppViewerp->beingDebugged())
  {
    llinfos << "Running under a debugger. llerrs will cause abort() instead of a crash."
      << llendl;
  }

  InitState init_state = initConfiguration();
  // Bail if init failed/aborted
  // Rename the log if possible
  renameLog(false);
  if (init_state != INIT_OK)
  {
    return init_state;
  }

  hide_useless_settings();

  if (!gSavedSettings.getBool("SkipStaticVectorSizing"))
  {
    // These are not true intialization routines, but rather memory
    // reserving functions to avoid (as much as possible) fragmentation
    // by making enough room for a few static/permanent std::vectors that
    // would otherwise slowly grow over time and might end up in the middle
    // of freed memory blocks after a TP if they were not large enough at
    // the start of the session.
    // By setting SkipStaticVectorSizing to true (and restarting the
    // viewer), you may skip this initialization so to verify (via
    // "Advanced" -> "Consoles" -> "Info to Debug Console" ->
    // "Memory Stats") what capacity is naturally reached during a session
    // and check it against the capacity reserved in the following
    // functions (this is how I determined the suitable values).
    LLCharacter::initClass();
    LLMotionController::initClass();
    LLVolumeImplFlexible::initClass();
    LLViewerTextureAnim::initClass();
  }

  // Set the max heap size.
  initMaxHeapSize();

  // Initialize the private memory pool for volumes
  LLVolumeFace::initClass();

  // Initialize the LLCore::Http libcurl library. Should be called before
  // consumers.
  mAppCoreHttp.init();
  llinfos << "LLCore::Http initialized. libcurl version is: "
    << LLCore::LLHttp::getCURLVersion() << llendl;

  // Set LLXMLRPCTransaction parameters
  LLXMLRPCTransaction::setVerifyCert(!gSavedSettings.getBool("NoVerifySSLCert"));

  // Avatar name cache and preferences
  U32 maxreq = llclamp(gSavedSettings.getU32("AvatarNameCacheMaxRequests"),
      4U, 32U);
  LLAvatarNameCache::setMaximumRequests(maxreq);
  llinfos << "LLAvatarNameCache maximum simultaneous requests set to: "
    << maxreq << llendl;
  LLAvatarNameCache::setUseDisplayNames(gSavedSettings.getU32("DisplayNamesUsage"));
  LLAvatarName::sOmitResidentAsLastName =
    gSavedSettings.getBool("OmitResidentAsLastName");

  initThreads();

  writeSystemInfo();

  // Build a string representing the advertized name and version number.
  gCurrentVersion =
    llformat("%s %d.%d.%d.%d",
        gSavedSettings.getString("VersionChannelName").c_str(),
        LL_VERSION_MAJOR, LL_VERSION_MINOR, LL_VERSION_BRANCH,
        LL_VERSION_RELEASE);

  llinfos << "J2C Engine is: " << LLImageJ2C::getEngineInfo() << llendl;

  /////////////////////////////////////////////////
  // OS-specific login dialogs
  /////////////////////////////////////////////////

  // Track number of times that app has run
  mNumSessions = gSavedSettings.getS32("NumSessions");
  gSavedSettings.setS32("NumSessions", ++mNumSessions);

  gSavedSettings.setString("HelpLastVisitedURL",
      gSavedSettings.getString("HelpHomeURL"));

  if (gSavedSettings.getBool("VerboseLogs"))
  {
    LLError::setPrintLocation(true);
  }

  // Load art UUID information; do not require these strings to be declared
  // in code.
  std::string colors_base_filename =
    gDirUtilp->findSkinnedFilename("colors_base.xml");
  LL_DEBUGS("AppInit") << "Loading base colors from " << colors_base_filename
    << LL_ENDL;
  gColors.loadFromFileLegacy(colors_base_filename, false, TYPE_COL4U);

  // Load overrides from user colors file, if any
  std::string user_colors_filename =
    gDirUtilp->findSkinnedFilename("colors.xml");
  if (!user_colors_filename.empty())
  {
    llinfos << "Loading user colors from " << user_colors_filename
      << llendl;
    if (gColors.loadFromFileLegacy(user_colors_filename, false,
          TYPE_COL4U) == 0)
    {
      llwarns << "Failed to load user colors from "
        << user_colors_filename << llendl;
    }
  }

  // Widget construction depends on LLUI being initialized
  LLUI::initClass(&gSavedSettings, &gSavedSettings, &gColors,
      LLUIImageList::getInstance(), ui_audio_callback,
      &LLUI::sGLScaleFactor);
  LLWeb::initClass();			  // do this after LLUI

  LLTextEditor::setURLCallbacks(&LLWeb::loadURL,
      &LLURLDispatcher::dispatchFromTextEditor,
      &LLURLDispatcher::dispatchFromTextEditor);

  // Update paths with correct language set
  LLUICtrlFactory::getInstance()->setupPaths();

  // Now that settings and colors are loaded, we can call this:
  gSelectMgr.initClass();

  /////////////////////////////////////////////////
  // Load settings files

  LLGroupMgr::parseRoleActions("role_actions.xml");

  LLAgent::parseTeleportMessages("teleport_strings.xml");

  // Load MIME type -> media impl mappings
  LLMIMETypes::parseMIMETypes(std::string("mime_types.xml"));

  if (gSavedSettings.getBool("SaveFileSelectorPaths"))
  {
    // Load the file selector default paths
    HBFileSelector::loadDefaultPaths("selector_paths.xml");
  }

  // Copy settings to globals and already constructed classes.
  settings_to_globals();

  // Setup settings listeners
  settings_setup_listeners();

  //MK
  RLInterface::init();
  //mk

  LLFontGL::setUseBatchedRender(gSavedSettings.getBool("RenderBatchedGlyphs"));

  // Do any necessary setup for accepting incoming SLURLs and Lua commands
  // from apps
  initAppMessagesHandler();

  if (!initHardwareTest())
  {
    // Early out from user choice.
    return INIT_FAILED;
  }

  // Derive an "unique" serial number out of the system disks serial numbers
  // (Windows, Linux) or out of the hardware serial (macOS).
  mSerialNumber = generateSerialNumber();
  // Hash it so that we can store it into the user settings, without
  // disclosing it (just in case the settings file and hashed passwords would
  // get stolen by someone ill-intentioned; of course, this does not offer
  // any protection against someone gaining full access to the computer since
  // they can easily get/compute the serial number in this case). HB
  LLMD5 serial_hash((unsigned char*)mSerialNumber.c_str());
  char md5serial[MD5HEX_STR_BYTES + 1];
  serial_hash.hex_digest(md5serial);
  // Check to see if the serial number changed since last session (could be
  // the case should there be a changed disk or should the settings be ported
  // to another computer); if so, clear any saved MAC since the latter was
  // encrypted with the serial number... HB
  std::string saved_hash = gSavedSettings.getString("SerialNumberHash");
  if (!saved_hash.empty() &&
      strncmp(md5serial, saved_hash.c_str(), MD5HEX_STR_BYTES))
  {
    llwarns << "Detected unique serial number change: clearing the saved settings depending on it."
      << llendl;
    gSavedSettings.setString("SavedMACAddress", "");
  }
  gSavedSettings.setString("SerialNumberHash", md5serial);

  // Always fetch the Ethernet MAC address, needed both for login and
  // password load.
  // Since the MAC address changes with the network I/F and we do not want to
  // loose our saved passwords each time we switch I/F on our computer (e.g.
  // from the Ethernet port to a Wi-Fi connection or vice versa), we store
  // the currrent MAC address the first time we get one, and then reuse that
  // same address on the subsequent sessions (the address being stored in the
  // saved settings, it is kept the same for all avatar accounts used from
  // this computer account, which is exactly what we want to happen).
  // To avoid (or at least make much less easy and likely) potential security
  // issues (such as in case of the stealing of both the user settings and
  // the saved grids login files by a third person for reuse on their own
  // computer), the MAC address is encrypted (using our unique computer Id):
  // of course, if a person could get access to your computer account, they
  // likely can get/deduce/compute that computer Id... But they can just as
  // well and in fact more easily, get access to the network I/F MAC address,
  // so... HB
  bool saved_mac_ok = false;
  std::string saved_mac = gSavedSettings.getString("SavedMACAddress");
  if (saved_mac.size() > MAC_ADDRESS_BYTES)
  {
    saved_mac = LLBase64::decode(saved_mac);
    if (saved_mac.size() == MAC_ADDRESS_BYTES)
    {
      LLXORCipher cipher(mSerialNumber);
      if (cipher.decrypt(saved_mac, (U8*)gMACAddress))
      {
        saved_mac_ok = true;
        llinfos << "Got the MAC address from the user settings."
          << llendl;
      }
    }
  }
  if (!saved_mac_ok)
  {
    // Get the actual and current network I/F MAC address
    LLUUID::getNodeID(gMACAddress);

    // Try and save it in settings, encrypted with our unique serial number
    saved_mac.assign((const char*)gMACAddress, MAC_ADDRESS_BYTES);
    LLXORCipher cipher(mSerialNumber);
    if (cipher.encrypt(saved_mac))
    {
      saved_mac =  LLBase64::encode(saved_mac);
      llinfos << "Saved the current MAC address into the user settings."
        << llendl;
    }
    else
    {
      llwarns << "Could not encrypt the MAC address to store it into the user settings."
        << llendl;
      saved_mac.clear();
    }
    gSavedSettings.setString("SavedMACAddress", saved_mac);
  }

  // *Note: this is where gViewerStats used to be created.

  // Initialize the cache, and gracefully handle initialization errors
  if (!initCache())
  {
    std::ostringstream msg;
    msg << gSecondLife << " is unable to access a file that it needs.\n\n"
      << "This can be because you somehow have multiple copies running, "
      << "or your system incorrectly thinks a file is open. "
      << "If this message persists, restart your computer and try again. "
      << "If it continues to persist, you may need to completely uninstall "
      << gSecondLife << " and reinstall it.";
    OSMessageBox(msg.str());
    return INIT_FAILED;
  }

  // Initialize the window

  if (!initWindow())
  {
    OSMessageBox(gNotifications.getGlobalString("UnsupportedGLRequirements"));
    return INIT_FAILED;
  }

  // initWindow also initializes the Feature List, so now we can initialize
  // this global.
  LLCubeMap::sUseCubeMaps = gFeatureManager.isFeatureAvailable("RenderCubeMap");

  // Call all self-registered classes
  LLInitClassList::getInstance()->fireCallbacks();

  // SJB: Needs to happen after initWindow(), not sure why but related to
  // fonts
  LLFolderViewItem::initClass();

  gGLManager.getGLInfo(gDebugInfo);
  gGLManager.printGLInfoString();

  // Load key settings
  bind_keyboard_functions();

  // Load Default bindings
  if (!gViewerKeyboard.loadBindings(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
          "keys.ini")))
  {
    llerrs << "Unable to open keys.ini" << llendl;
  }
  // Load Custom bindings (override defaults)
  gViewerKeyboard.loadBindings(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
        "custom_keys.ini"));

  // Without SSE2 support we will crash almost immediately, warn here.
  LLCPUInfo* cpuinfo = LLCPUInfo::getInstance();
  if (!cpuinfo->hasSSE2())
  {
    // Cannot use an alert here since we are exiting and all hell breaks
    // lose.
    OSMessageBox(gNotifications.getGlobalString("UnsupportedCPUSSE2"));
    return INIT_FAILED;
  }

  // Alert the user if they are using unsupported hardware
  if (!gSavedSettings.getBool("AlertedUnsupportedHardware"))
  {
    bool unsupported = false;
    LLSD args;
    std::string minSpecs;

    // Get cpu data from xml
    std::stringstream min_cpu_string(gNotifications.getGlobalString("UnsupportedCPUAmount"));
    S32 min_cpu = 0;
    min_cpu_string >> min_cpu;

    // Get RAM data from XML
    std::stringstream min_ram_str(gNotifications.getGlobalString("UnsupportedRAMAmount"));
    U64 min_ram = 0;
    min_ram_str >> min_ram;
    min_ram = min_ram * 1024;

    if (!gFeatureManager.isGPUSupported() &&
        gFeatureManager.getGPUClass() != GPU_CLASS_UNKNOWN)
    {
      minSpecs += gNotifications.getGlobalString("UnsupportedGPU");
      minSpecs += "\n";
      unsupported = true;
    }
    if (cpuinfo->getMHz() < min_cpu)
    {
      minSpecs += gNotifications.getGlobalString("UnsupportedCPU");
      minSpecs += "\n";
      unsupported = true;
    }
    if (LLMemoryInfo::getPhysicalMemoryKB() < min_ram)
    {
      minSpecs += gNotifications.getGlobalString("UnsupportedRAM");
      minSpecs += "\n";
      unsupported = true;
    }

    if (gFeatureManager.getGPUClass() == GPU_CLASS_UNKNOWN)
    {
      gNotifications.add("UnknownGPU");
    }

    if (unsupported)
    {
      if (!gSavedSettings.controlExists("WarnUnsupportedHardware") ||
          gSavedSettings.getBool("WarnUnsupportedHardware"))
      {
        args["MINSPECS"] = minSpecs;
        gNotifications.add("UnsupportedHardware", args);
      }
    }
  }

  // Save the graphics card
  gDebugInfo["GraphicsCard"] = gFeatureManager.getGPUString();

  // Save the current version to the prefs file
  gSavedSettings.setString("LastRunVersion", gCurrentVersion);

  // Initialize the constant data for the login authentication. HB
  char hashed_mac_string[MD5HEX_STR_SIZE];
  LLMD5 hashed_mac;
  hashed_mac.update(gMACAddress, MAC_ADDRESS_BYTES);
  hashed_mac.finalize();
  hashed_mac.hex_digest(hashed_mac_string);
  const LLOSInfo* osinfo = LLOSInfo::getInstance();
  gUserAuth.init(osinfo->getOSVersionString(), osinfo->getOSStringSimple(),
      gCurrentVersion,
      gSavedSettings.getString("VersionChannelName"),
      mSerialNumber, hashed_mac_string);

  gSimLastTime = gRenderStartTime.getElapsedTimeF32();
  gSimFrames = (F32)gFrameCount;

  LLViewerJoystick::getInstance()->init(false);

  gViewerParcelMgr.initClass();

  LLViewerMedia::initClass();
  llinfos << "Viewer media initialized." << llendl;

  // Tell the coprocedure manager how to discover and store the pool sizes
  LLCoprocedureManager::getInstance()->setPropertyMethods(getSettingU32,
      setSettingU32);

  std::string lua_script = gSavedSettings.getString("LuaAutomationScript");
  if (!lua_script.empty() && gDirUtilp)
  {
    lua_script = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
        lua_script);
    if (LLFile::exists(lua_script))
    {
      HBViewerAutomation::start(lua_script);
    }
  }

  return INIT_OK;
}

// Loads	up the initial grid	choice from:
//	- hard coded defaults...
//	- command line settings...
//	- persisted settings...
void LLAppViewer::initGridChoice()
{
  // Get the grid choice specified via the command line.
  std::string	grid_choice	= gSavedSettings.getString("CmdLineGridChoice");
  LLGridManager* gm = LLGridManager::getInstance();

  // Load last server choice by default, ignored if the command line grid
  // choice has been set
  if (grid_choice.empty())
  {
    EGridInfo server = (EGridInfo)gSavedSettings.getS32("ServerChoice");
    if (server == GRID_INFO_OTHER)
    {
      grid_choice = gSavedSettings.getString("CustomServer");
    }
    else if (server != GRID_INFO_NONE)
    {
      gm->setGridChoice(server);
      return;
    }
    else
    {
      gm->setGridChoice(DEFAULT_GRID_CHOICE);
      return;
    }
  }
  // Note: this call is no op when string is empty:
  gm->setGridChoice(grid_choice);
}

//virtual
bool LLAppViewer::initAppMessagesHandler()
{
  // Does nothing unless subclassed
  return false;
}

//virtual
bool LLAppViewer::sendURLToOtherInstance(const std::string& url)
{
  // Does nothing unless subclassed
  return false;
}

//static
U32 LLAppViewer::getSettingU32(const std::string& name)
{
  if (gSavedSettings.getControl(name.c_str()))
  {
    return gSavedSettings.getU32(name.c_str());
  }
  return 0;
}

//static
void LLAppViewer::setSettingU32(const std::string& name, U32 value)
{
  if (gSavedSettings.getControl(name.c_str()))
  {
    gSavedSettings.setU32(name.c_str(), value);
  }
}

// Sets the max heap size.
void LLAppViewer::initMaxHeapSize()
{
  U32 max_mem, max_phys;
  LLMemoryInfo::getMaxMemoryKB(max_phys, max_mem);
  LLMemory::initMaxHeapSizeKB(max_mem);
}

void LLAppViewer::checkMemory()
{
  constexpr F32 MEMORY_CHECK_INTERVAL = 1.f;		// in seconds
  constexpr F32 MEMORY_TRIM_INTERVAL = 3.f;		// in seconds
  constexpr F32 MEMORY_TRIM_LONG_INTERVAL = 60.f;	// in seconds
  static F32 last_check = 0.f;
  static F32 last_trim = 0.f;
  static LLCachedControl<bool> memory_safety_check(gSavedSettings,
      "MemorySafetyChecks");

  LL_FAST_TIMER(FTM_MEMORY_CHECK);

  F32 elapsed = gMemoryCheckTimer.getElapsedTimeF32();
  if (memory_safety_check)
  {
    F32 check_interval = MEMORY_CHECK_INTERVAL;
    // Check memory more often the closer it is to get exhausted... For a
    // discard bias of 0 the check interval is unchanged (1 second), and
    // for a discard bias of 5 the interval is divided by 4 (0.25 seconds).
    check_interval /= 0.6f * LLViewerTexture::sDesiredDiscardBias + 1.f;
    if (elapsed - last_check > check_interval)
    {
      // We never reset gMemoryCheckTimer because it is used elsewhere:
      // just keep track of the last time we checked memory instead.
      last_check = elapsed;

      // Update the memory availability info, trimming the heap from the
      // freed memory every MEMORY_TRIM_INTERVAL seconds; we do not do
      // the latter each time because it is time-consuming with some
      // memory managers (Linux' standard one, for example) and also
      // because a slower trimming of freed memory provides a welcomed
      // hysteresis that compensates for the texture fetching thread
      // inertia to the discard level changes.
      bool trim = elapsed - last_trim >= MEMORY_TRIM_INTERVAL;
      LLMemory::updateMemoryInfo(trim);
      if (trim)
      {
        last_trim = elapsed;
      }

      // Check for low memory condition
      bool is_low = LLMemory::getAvailableVirtualMemKB() <
        LLMemory::getSafetyMarginKB();

      // Do not spam, logging the memory info only after a heap trim
      if (is_low && last_trim == elapsed)
      {
        LLMemory::logMemoryInfo();
      }

      LLPipeline::throttleNewMemoryAllocation(is_low);
    }
  }
  // Even when safety checks are off, we still trim memory down every minute
  else if (elapsed - last_check > MEMORY_TRIM_LONG_INTERVAL)
  {
    // We never reset gMemoryCheckTimer because it is used elsewhere: just
    // keep track of the last time we checked memory instead.
    LLMemory::updateMemoryInfo(true);
    last_check = elapsed;
  }
}

void LLAppViewer::idleAFKCheck(bool force_afk)
{
  static LLCachedControl<U32> afk_timeout(gSavedSettings, "AFKTimeout");
  U32 timeout = afk_timeout;
  if (timeout > 0 && timeout < 30)
  {
    timeout = 30;
  }

  // Check idle timers
  if (timeout > 0 &&
      !gAgent.getAFK() && !gAgent.getBusy() && !gAgent.getAutoReply() &&
      (force_afk || gAwayTriggerTimer.getElapsedTimeF32() > (F32)timeout))
  {
    U32 away_action = gSavedSettings.getU32("AwayAction");
    switch (away_action)
    {
      case 0:
        gAgent.setAFK();
        break;

      case 1:
        gAgent.setBusy();
        break;

      default:
        gAgent.setAutoReply();
    }
  }
}

void LLAppViewer::frame(LLEventPump& mainloop)
{
  static LLViewerJoystick* joystick = LLViewerJoystick::getInstance();
  // As we do not (yet) send data on the mainloop LLEventPump that varies
  // with each frame, no need to instantiate a new LLSD event object each
  // time. Obviously, if that changes, just instantiate the LLSD at the
  // point of posting.
  static LLSD new_frame;

  LL_FAST_TIMER(FTM_FRAME);

  // Check memory availability information
  checkMemory();

  static const std::string MainGatherInput = "Main:GatherInput";
  pingMainloopTimeout(&MainGatherInput);

#if LL_LINUX
  // Pump glib events to avoid starvation for DBus servicing.
  LLAppViewerLinux::pumpGlib();
#endif

  if (gWindowp)
  {
    LL_FAST_TIMER(FTM_MESSAGES);
    if (!restoreErrorTrap())
    {
      llwarns << " Someone took over my signal/exception handler !"
        << llendl;
    }

    gWindowp->gatherInput();
  }

  if (!LLApp::isExiting())
  {
    static const std::string MainJoystickKeyboard =
      "Main:JoystickKeyboard";
    pingMainloopTimeout(&MainJoystickKeyboard);

    // Scan keyboard for movement keys. Command keys and typing are handled
    // by windows callbacks. Do not do this until we are done initializing.
    if (gViewerWindowp && gKeyboardp && gWindowp->getVisible() &&
        gViewerWindowp->getActive() && !gWindowp->getMinimized() &&
        LLStartUp::isLoggedIn() && !gViewerWindowp->getShowProgress() &&
        !gFocusMgr.focusLocked())
    {
      joystick->scanJoystick();
      gKeyboardp->scanKeyboard();
    }

    // Update state based on messages, user input, object idle.
    {
      // *TODO: Remove. Messages should not be stalling for 20+ seconds !
      pauseMainloopTimeout();

      LL_FAST_TIMER(FTM_IDLE);
      idle();

      {
        static const std::string MainServicePump =
          "Main:ServicePump";
        pingMainloopTimeout(&MainServicePump);

        LL_FAST_TIMER(FTM_PUMP);
        {
          LL_FAST_TIMER(FTM_PUMP_EVENT);

          // Canonical per-frame event
          mainloop.post(new_frame);

          // Give listeners a chance to run
          llcoro::suspend();
        }

        {
          LL_FAST_TIMER(FTM_PUMP_SERVICE);
          gServicePumpIOp->pump();
          {
            LL_FAST_TIMER(FTM_SERVICE_CALLBACK);
            gServicePumpIOp->callback();
          }
        }
      }

      resumeMainloopTimeout();
    }

    if (sDoDisconnect && LLStartUp::isLoggedIn())
    {
      pauseMainloopTimeout();
      saveFinalSnapshot();
      disconnectViewer();
      resumeMainloopTimeout();
    }

    // Render scene.
    if (!LLApp::isExiting())
    {
      static const std::string MainDisplay = "Main:Display";
      pingMainloopTimeout(&MainDisplay);
      gRLInterface.mRenderLimitRenderedThisFrame = false;
      display();
      static const std::string MainSnapshot = "Main:Snapshot";
      pingMainloopTimeout(&MainSnapshot);
      LLFloaterSnapshot::update(); // take snapshots
    }
  }

#if LL_LINUX && !LL_CALL_SLURL_DISPATCHER_IN_CALLBACK
  const std::string& url = getReceivedSLURL();
  if (!url.empty())
  {
    LLMediaCtrl* web = NULL;
    LLURLDispatcher::dispatch(url, "clicked", web, false);
    clearReceivedSLURL();
  }
#endif

  static const std::string MainSleep = "Main:Sleep";
  pingMainloopTimeout(&MainSleep);

  pauseMainloopTimeout();

  // Sleep and run background threads
  {
    LL_FAST_TIMER(FTM_SLEEP);

    if (gDisconnected)
    {
      // Always sleep 10ms per frame after a spurious disconnection to
      // avoid excessive CPU and GPU usage while just rendering the UI...
      gFrameSleepTime = 10;
    }
    else if (LLStartUp::isLoggedIn())
    {
      // Reset at each frame once logged in and not yet disconnected
      gFrameSleepTime = 0;
    }

    // Yield cooperatively when not running as foreground window
    static LLCachedControl<U32> background_yield_time(gSavedSettings,
        "BackgroundYieldTime");
    if (background_yield_time > 0 &&
        (!gFocusMgr.getAppHasFocus() ||
         (gWindowp && !gWindowp->getVisible())))
    {
      if (background_yield_time > 1000)
      {
        llwarns << "Out or range BackgroundYieldTime setting; resetting to default (40ms)."
          << llendl;
        gSavedSettings.setU32("BackgroundYieldTime", 40);
      }
      if (!gFrameSleepTime)
      {
        gFrameSleepTime = background_yield_time;
      }
      // Also pause worker threads during this wait period
      gTextureCachep->pause();
      gImageDecodeThreadp->pause();
    }

    // Do not sleep when a reshape() occurred (gScreenIsDirty == true)
    // to avoid excessive flicker during window resizing. HB
    if (!gScreenIsDirty && gFrameSleepTime)
    {
      ms_sleep(gFrameSleepTime);
    }

    S32 work_pending = 0;
    S32 io_pending = 0;
    F32 max_time = llmin(gFrameIntervalSeconds * 10.f, 1.f);
    {
      LL_FAST_TIMER(FTM_TEXTURE_CACHE);
      // Unpauses the texture cache thread
      work_pending = gTextureCachep->update(max_time);
    }
    {
      LL_FAST_TIMER(FTM_DECODE);
      // Unpauses the image thread
      work_pending += gImageDecodeThreadp->update(max_time);
    }
    {
      LL_FAST_TIMER(FTM_FETCH);
      // Unpauses the texture fetch thread
      work_pending += gTextureFetchp->update(max_time);
    }
    {
      LL_FAST_TIMER(FTM_LFS);
      io_pending += LLLFSThread::updateClass(1);
    }

    gMeshRepo.update();

    // Pause texture fetching threads if nothing to process
    if (!work_pending)
    {
      pauseTextureFetch();
    }
    // Pause file threads if nothing to process
    if (!io_pending)
    {
      LLLFSThread::sLocal->pause();
    }
    else if (io_pending > 1000)
    {
      // Give the file system some time to catch up
      ms_sleep(llmin(io_pending / 100, 50));
    }

    resumeMainloopTimeout();

    static const std::string MainEnd = "Main:End";
    pingMainloopTimeout(&MainEnd);
  }
}

// Runs the main loop until time to quit. NOTE: for macOS, this method returns
// at each frame, while for Linux and Windows, it only returns on shutdown.
//#Chanouken
bool LLAppViewer::mainLoop()
{
  static bool init_needed = true;
  if (init_needed)
  {
    init_needed = false;

    mMainloopTimeout = new LLWatchdogTimeout();

    // Create IO Pump
    gServicePumpIOp = new LLPumpIO(gAPRPoolp);

    // Note: this is where gLocalSpeakerMgr and gActiveSpeakerMgr used to
    // be instantiated.

    LLVoiceChannel::initClass();
    LLVoiceClient::init(gServicePumpIOp);

    LLViewerJoystick::getInstance()->setNeedsReset(true);
  }

  LLEventPump& mainloop(LLEventPumps::getInstance()->obtain("mainloop"));

#if LL_DARWIN
  if (!LLApp::isExiting())
#else
    while (!LLApp::isExiting())
#endif
    {
#if LL_FAST_TIMERS_ENABLED
      // Must be outside of any timer instances
      LLFastTimer::enabledFastTimers(gEnableFastTimers);
      LLFastTimer::reset();
#endif
      frame(mainloop);
#if TRACY_ENABLE
      FrameMark
#endif
    }
#if LL_DARWIN
  else
#endif
  {
    // Save snapshot for next time, if we made it through initialization
    if (LLStartUp::isLoggedIn())
    {
      saveFinalSnapshot();
    }

    delete gServicePumpIOp;
    gServicePumpIOp = NULL;

    destroyMainloopTimeout();

    llinfos << "Exiting main_loop" << llendl;
  }

#if LL_DARWIN
  return LLApp::isExiting();
#else
  return true;
#endif
}

bool LLAppViewer::cleanup()
{
  llinfos << "Cleaning up..." << llendl;

#if TRACY_ENABLE
  // Let any profiler run to allow examining data after the session
  HBTracyProfiler::detach();
#endif

  if (gWindowp && gSavedSettings.getBool("MinimizeOnClose"))
  {
    llinfos << "Minimizing the viewer windows." << llendl;
    gWindowp->minimize();
  }

  HBViewerAutomation::cleanup();

  HBFloaterBump::cleanup();

  // Ditch LLVOAvatarSelf instance
  gAgentAvatarp = NULL;
  llinfos << "LLVOAvatarSelf destroyed" << llendl;

  LLVoiceClient::terminate();
  llinfos << "LLVoiceClient terminated" << llendl;

  disconnectViewer();
  llinfos << "Viewer disconnected" << llendl;

  // Give any remaining SLPlugin instances a chance to exit cleanly.
  LLPluginProcessParent::shutdown();

  // Cleanup the environment class now, since it uses a pump on experiences
  gEnvironment.cleanupClass();

  // Let some time to coroutines to notice and exit
  llinfos << "Pumping 'mainloop' to let coroutines shut down..." << llendl;
  LLEventPump& mainloop(LLEventPumps::getInstance()->obtain("mainloop"));
  LLSD frame_llsd;
  gLogoutTimer.reset();	// Let's reuse an existing timer...
  bool first_try = true;
  LLCoros* coros = LLCoros::getInstance();
  while (coros->hasActiveCoroutines())
  {
    mainloop.post(frame_llsd);

    // Give listeners a chance to run
    llcoro::suspend();

    if (gLogoutTimer.getElapsedTimeF64() > 1.0)
    {
      if (first_try)
      {
        first_try = false;
        // Abort remaining suspended HTTP operations
        LLCoreHttpUtil::HttpCoroutineAdapter::cleanup();
        // And retry...
        gLogoutTimer.reset();
        continue;
      }
      break;
    }
  }
  coros->printActiveCoroutines();

#if 1	// This should not be necessary any more (reset() is now called in
  // ~LLEventPumps()), but it does not hurt, so just in case... HB
  // Workaround for DEV-35406 crash on shutdown
  LLEventPumps::getInstance()->reset();
  llinfos << "LLEventPumps reset" << llendl;
#endif

  LLEventPumps::getInstance()->clear();
  llinfos << "LLEventPumps cleared" << llendl;

  // Flag all elements as needing to be destroyed immediately to ensure
  // shutdown order
  LLMortician::setZealous(true);
  llinfos << "LLMortician::setZealous() called" << llendl;

  // Note, we do this early in case of a crash when cleaning up the UI or
  // threads. This way, user settings and data get saved despite the crash...
  llinfos << "Saving data..." << llendl;

  // Quitting with "Remember login credentials" turned off should always
  // stomp your saved password, whether or not you successfully logged in.
  if (!mIsSiblingViewer && !gSavedSettings.getBool("RememberLogin"))
  {
    gSavedSettings.setString("HashedPassword", "");
  }

  gSavedSettings.setString("VersionChannelName", LL_CHANNEL);
  saveGlobalSettings();

  // PerAccountSettingsFile is empty if the user never logged on.
  std::string filename = gSavedSettings.getString("PerAccountSettingsFile");
  if (filename.empty())
  {
    llinfos << "Not saving per-account settings; no account name yet."
      << llendl;
  }
  else if (!mSavePerAccountSettings)
  {
    llinfos << "Not saving per-account settings; last login was not successful."
      << llendl;
  }
  else
  {
    //MK
    if (gRLenabled)
    {
      gRLInterface.refreshTPflag(false);
    }
    // Do this even if !gRLenabled
    gRLInterface.validateLastStandingLoc();
    //mk
    // Store the time of our current logoff
    gSavedPerAccountSettings.setU32("LastLogoff", time_corrected());
    gSavedPerAccountSettings.saveToFile(filename);
  }

  llinfos << "All user settings saved" << llendl;

  if (gSavedSettings.getBool("SaveFileSelectorPaths"))
  {
    // Save the file selector default paths
    HBFileSelector::saveDefaultPaths("selector_paths.xml");
    llinfos << "selector_paths.xml saved" << llendl;
  }

  // Save URL history file
  LLURLHistory::saveFile("url_history.xml");
  llinfos << "url_history.xml saved" << llendl;

  // Save mute list if needed.
  LLMuteList::cache();

  display_cleanup();
  gStartTexture = NULL;
  llinfos << "Display cleaned up" << llendl;

  LLError::logToFixedBuffer(NULL);
  llinfos << "Stopped logging to fixed buffer" << llendl;

  // Shut down mesh streamer
  gMeshRepo.shutdown();
  llinfos << "Mesh repository shut down" << llendl;

  // Must clean up texture references before viewer window is destroyed.
  LLHUDManager::updateEffects();
  LLHUDObject::updateAll();
  LLHUDManager::cleanupEffects();
  LLHUDObject::cleanupHUDObjects();
  llinfos << "HUD objects cleaned up" << llendl;

  LLKeyframeDataCache::clear();

  LLHUDManager::cleanupClass();
  llinfos << "HUD manager shut down" << llendl;

  LLLocalBitmap::cleanupClass();
  llinfos << "Local bitmaps cleaned up" << llendl;

  delete gAssetStoragep;
  gAssetStoragep = NULL;
  llinfos << "Asset storage deleted" << llendl;

  LLPolyMesh::freeAllMeshes();
  llinfos << "All polymeshes freed" << llendl;

  LLAvatarNameCache::cleanupClass();
  delete gCacheNamep;
  gCacheNamep = NULL;
  llinfos << "Name cache cleaned up" << llendl;

  // Note: this is where gLocalSpeakerMgr and gActiveSpeakerMgr used to be
  // deleted.

  gWorldMap.reset(); // Release any images
  llinfos << "World map images cleared" << llendl;

  LLStartUp::shutdownAudioEngine();
  llinfos << "Audio engine shut down" << llendl;

  // Note: this is where gFeatureManager used to be deleted.

  // Patch up settings for next time. Must do this before we delete the
  // viewer window, such that we can suck rectangle information out of it.
  cleanupSavedSettings();
  llinfos << "Settings patched up" << llendl;

  // Wait for any pending IO
  llinfos << "Checking for pending IO..." << llendl;
  while (true)
  {
    S32 pending = LLLFSThread::updateClass(0);
    if (!pending)
    {
      break;
    }
    llinfos << "Waiting for pending IO to finish: " << pending << llendl;
    ms_sleep(100);
  }
  llinfos << "No more pending IO" << llendl;

  if (!mSecondInstance)
  {
    // Delete some of the files left around in the cache.
    // But only do this if no other instance is running !
    removeCacheFiles("*.wav");
    removeCacheFiles("*.tmp");
    removeCacheFiles("*.lso");
    removeCacheFiles("*.out");
    removeCacheFiles("*.dsf");
    removeCacheFiles("*.bodypart");
    removeCacheFiles("*.clothing");
    llinfos << "Temporary cache files removed" << llendl;
  }

  // Destroy the UI
  if (gViewerWindowp)
  {
    gViewerWindowp->shutdownViews();
    llinfos << "Shut down views" << llendl;
  }

  // Cleanup inventory after the UI since it will delete any remaining
  // observers (Deleted observers should have already removed themselves)
  gInventory.cleanupInventory();
  llinfos << "Inventory cleaned up" << llendl;

  // Clean up selections in selections manager after UI is destroyed, as UI
  // may be observing them. Also, clean up before GL is shut down because we
  // might be holding onto objects with texture references
  gSelectMgr.clearSelections();
  llinfos << "Selections cleaned up" << llendl;

  // Shut down OpenGL
  if (gViewerWindowp)
  {
    gViewerWindowp->shutdownGL();

    // Destroy window, and make sure we're not fullscreen
    // This may generate window reshape and activation events.
    // Therefore must do this before destroying the message system.
    delete gViewerWindowp;
    gViewerWindowp = NULL;
    llinfos << "ViewerWindow deleted" << llendl;
  }

  // Viewer UI relies on keyboard so keep it aound until viewer UI is gone
  delete gKeyboardp;
  gKeyboardp = NULL;
  llinfos << "Keyboard handler destroyed" << llendl;

  // Turn off Space Navigator and similar devices
  LLViewerJoystick::getInstance()->terminate();
  llinfos << "Joystick handler terminated" << llendl;

  LLViewerObject::cleanupVOClasses();
  llinfos << "Viewer objects cleaned up" << llendl;

  LLAvatarAppearance::cleanupClass();
  llinfos << "Avatar appearance cleaned up" << llendl;

  // *FIX: This is handled in LLAppViewerWin32::cleanup().
  // I'm keeping the comment to remember its order in cleanup, in case of
  // unforseen dependency.
  //#if LL_WINDOWS
  //	gDXHardware.cleanup();
  //#endif // LL_WINDOWS

  LLVolumeMgr::cleanupClass();

  gViewerParcelMgr.cleanupClass();

  // *Note: this is where gViewerStats used to be deleted.

  LLFollowCamMgr::cleanupClass();
  llinfos << "LLFollowCamMgr cleaned up" << llendl;

  LLPanelWorldMap::cleanupClass();
  llinfos << "LLPanelWorldMap cleaned up" << llendl;

  LLFolderViewItem::cleanupClass();
  llinfos << "LLFolderViewItem cleaned up" << llendl;

  LLUI::cleanupClass();
  llinfos << "LLUI cleaned up" << llendl;

  // Must do this (again) after all panels have been deleted because panels
  // that have persistent rects save their rects on delete.
  saveGlobalSettings();
  llinfos << "User settings saved again to update closed floaters rects"
    << llendl;

  LLMuteList::shutDownClass();

  removeMarkerFile(); // Any crashes from here on we will just have to ignore
  llinfos << "Removed marker files" << llendl;

  writeDebugInfo();

  llinfos << "Shutting down Threads..." << llendl;

  // Stop the plugin read thread if it is running.
  LLPluginProcessParent::setUseReadThread(false);

  // Let threads finish
  gLogoutTimer.reset();	// Let's reuse an existing timer...
  while (true)
  {
    S32 pending = 0;
    // Un-pause the cache worker, image worker and texture fetcher threads
    pending += gTextureCachep->update(1.f);
    pending += gImageDecodeThreadp->update(1.f);
    pending += gTextureFetchp->update(1.f);
    pending += LLLFSThread::updateClass(0);
    if (pending == 0)
    {
      break;
    }
    if (gLogoutTimer.getElapsedTimeF64() >= 5.0)
    {
      llwarns << "Quitting with pending background tasks." << llendl;
      break;
    }
  }

  // Delete workers first: shutdown all worker threads before deleting them
  // in case of co-dependencies.
  mAppCoreHttp.requestStop();
  gTextureFetchp->shutdown();
  gTextureCachep->shutdown();
  gImageDecodeThreadp->shutdown();

  gTextureFetchp->shutDownTextureCacheThread();
  gTextureFetchp->shutDownImageDecodeThread();
  llinfos << "Threads shut down, cleaning up threads..." << llendl;

  end_messaging_system();
  llinfos << "Message system deleted." << llendl;

  // LLCore::Http libcurl library
  mAppCoreHttp.cleanup();
  llinfos << "LLCore HTTP cleaned up." << llendl;

  // MUST happen AFTER mAppCoreHttp.cleanup();
  delete gTextureCachep;
  gTextureCachep = NULL;
  delete gTextureFetchp;
  gTextureFetchp = NULL;
  delete gImageDecodeThreadp;
  gImageDecodeThreadp = NULL;
  llinfos << "Image caching/fetching/decoding threads destroyed."
    << llendl;

  // Note: LLViewerMedia::cleanupClass() has to be put before
  // gTextureList.shutdown() because some new image might be generated
  // during cleaning up media. --bao
  LLViewerMediaFocus::cleanupClass();
  LLViewerMedia::cleanupClass();
  LLViewerParcelMedia::cleanupClass();
  llinfos << "Media classes cleaned up." << llendl;

  // shutdown again in case a callback added something
  gTextureList.shutdown();
  LLUIImageList::getInstance()->cleanUp();

  // This should eventually be done in LLAppViewer
  LLImage::cleanupClass();
  LLLFSThread::cleanupClass();
  llinfos << "LFS thread finished" << llendl;

  // This must be done *after* the texture cache is stopped
  if (mPurgeOnExit && gDirUtilp)
  {
    llinfos << "Purging all cache files on exit..." << llendl;
    LLDirIterator::deleteFilesInDir(gDirUtilp->getCacheDir());
    llinfos << "Cache files purged" << llendl;
  }

  // Cleanup settings last in case other clases reference them
  gSavedSettings.cleanup();
  gColors.cleanup();

  LLWatchdog::getInstance()->cleanup();

  LLProxy::cleanupClass();
  llinfos << "LLProxy cleaned up." << llendl;

  LLCore::LLHttp::cleanup();
  llinfos << "LLCoreHttp cleaned up." << llendl;

  LLWearableType::cleanupClass();
  llinfos << "Wearable types cleaned up." << llendl;

  if (gAvatarAppDictp)
  {
    delete gAvatarAppDictp;
    llinfos << "Avatar appearance dictionnary cleaned up." << llendl;
  }

  LLSettingsType::cleanupClass();
  llinfos << "Settings types cleaned up." << llendl;

#if LL_UUID_ALIGMENT_STATS
  llinfos << "Number of created LLUUIDs per address alignment:\n";
  U64 total = 0;
  for (U32 i = 0; i < 8; ++i)
  {
    total += LLUUID::sAlignmentCounts[i];
  }
  for (U32 i = 0; i < 8; ++i)
  {
    U64 number = LLUUID::sAlignmentCounts[i];
    llcont << "  - " << i << ": " << number << " ("
      << F32(1000L * number / total) * 0.1f << "%)\n";
  }
  llcont << llendl;
#endif

  LLMemory::cleanupClass();

  llinfos << "Goodbye." << llendl;

  // This is needed to ensure that the log file is properly flushed,
  // especially under Linux (there is apparently a destructors ordering
  // issue that prevents it to flush and close naturally otherwise)...
  LLError::logToFile("");

  // Rename the log if needed
  renameLog(true);

  return true;
}

// A callback for llerrs to call during the watchdog error.
void watchdog_llerrs_callback(const std::string& error_string)
{
  std::ostringstream msg;
  msg << gSecondLife << " is unresponsive.\n\n"
    << "This is most likely the graphics driver that got stuck\n"
    << "in an infinite loop. Either (by likeliness order):\n"
    << " - there is a bug in the graphics driver;\n"
    << " - your PC hardware was overclocked beyond its limits;\n"
    << " - your PC hardware overheated;\n"
    << " - the power supply of your PC cannot stand the load;\n"
    << " - your PC hardware is defective.\n\n"
    << "This error is unrecoverable and the watchdog will now\n"
    << "voluntarily crash the viewer so to exit the infinite\n"
    << "loop and abort the session.";
  OSMessageBox(msg.str());

  sLLErrorActivated = true;

#if LL_WINDOWS
  RaiseException(0, 0, 0, 0);
#else
  raise(SIGQUIT);
#endif
}

// A callback for the watchdog to call.
void watchdog_killer_callback()
{
  LLError::setFatalFunction(watchdog_llerrs_callback);
  llerrs << "Watchdog killer event" << llendl;
}

void LLAppViewer::initThreads()
{
  // Do not set affinity if a first Cool VL Viewer instance is already
  // running... This would be detrimental to both instances.
  if (!mIsSiblingViewer)
  {
    // Set the CPU affinity for the main thread; the affinity for all child
    // threads will be set to the complementary of this affinity, so that
    // they run on other cores than the main thread. This call must be done
    // before we start any LLThread if we want the corresponding affinity
    // set on them. When MainThreadCPUAffinity is 0 (or, for now, under
    // Darwin and Windows) this call is a no-operation and no affinity is
    // set for any threads. HB
    U32 cpu_mask = gSavedSettings.getU32("MainThreadCPUAffinity");
    LLCPUInfo::setMainThreadCPUAffinifty(cpu_mask);
  }

#if !LL_DEBUG	// Never enable the watchdog during debug sessions...
# if LL_WINDOWS
  if (gSavedSettings.getBool("WatchdogEnabledWindows"))
# else
    if (gSavedSettings.getBool("WatchdogEnabled"))
# endif
    {
      LLWatchdog::getInstance()->init(watchdog_killer_callback);
    }
#endif

  bool threaded_fs = gSavedSettings.getBool("ThreadedFilesystem");
#if LL_DARWIN
  constexpr bool use_io_uring = false;
#else
  bool use_io_uring = gSavedSettings.getBool("UseIOUring");
  if (use_io_uring)
  {
    llinfos << "Enabling io_uring for FS operations." << llendl;
    // NOTE: when io_uring/IOCP is in use, we need the LFS to be
    // threaded ! HB
    if (!threaded_fs)
    {
      threaded_fs = true;
      gSavedSettings.setBool("ThreadedFilesystem", true);
    }
  }
#endif
  if (threaded_fs)
  {
    llinfos << "Enabling threaded FS operations." << llendl;
  }
  LLLFSThread::initClass(threaded_fs, use_io_uring);

  // Image decoding
  U32 decode_threads = gSavedSettings.getU32("NumImageDecodeThreads");
  gImageDecodeThreadp = new LLImageDecodeThread(decode_threads);
  gTextureCachep = new LLTextureCache(threaded_fs);
  gTextureFetchp = new LLTextureFetch(gTextureCachep, gImageDecodeThreadp);
  LLImage::initClass();

  // Mesh streaming and caching
  gMeshRepo.init();
}

//static
void LLAppViewer::errorCallback(const std::string& error_string)
{
  // Since this is a volontary (controlled) "crash" due to the lack of a
  // fallback path in the viewer code for an unexpected/unhandled situation,
  // let's at least try and quit elegantly, notifying properly the user and
  // logging out cleanly whenever possible !
  static bool called_once = false;
  if (!called_once)
  {
    // Do not try this twice, in case another llerrs would get triggered
    // during OSMessageBox() or sendLogoutRequest() !
    called_once = true;

    OSMessageBox(error_string, "Unrecoverable error");

    // If we have a region, make some attempt to send a logout request
    // first. This prevents the halfway-logged-in avatar from hanging
    // around inworld for a couple minutes.
    if (gAgent.getRegion())
    {
      gAppViewerp->sendLogoutRequest();
    }

    // Let some time for the user to read the message box, in case it would
    // get force-closed together with the application, by the OS.
    ms_sleep(5000);
  }

  // Set the ErrorActivated global so we know to create a marker file
  sLLErrorActivated = true;
  LLError::crashAndLoop(error_string);
}

// Sets up logging defaults for the viewer
void LLAppViewer::initLogging()
{
  std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
      "");
  LLError::initForApplication(filename);
  LLError::setFatalFunction(errorCallback);

  // Setup our temporary log file name.
#if LL_WINDOWS
  S32 pid = _getpid();
#else
  S32 pid = getpid();
#endif
  filename = llformat(TEMP_LOG_FMT, pid);
  mLogFileName = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, filename);
  // Set the log file
  LLError::logToFile(mLogFileName);

  llinfos << "Viewer process id is: " << pid
    << ". Logging to temporary file: " << mLogFileName << llendl;
}

void LLAppViewer::renameLog(bool on_exit)
{
  if (mLogFileName.empty())
  {
    llinfos << "User-supplied log file name. Not renaming it." << llendl;
    return;
  }

  std::string old_log_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
      PREVIOUS_LOG);
  if (mSecondInstance && mIsSiblingViewer)
  {
    if (on_exit)
    {
      // Remove the last PREVIOUS_LOG log file, if any.
      LLFile::remove(old_log_file);
      // Rename our log as PREVIOUS_LOG
      LLFile::rename(mLogFileName, old_log_file);
#if 0		// Log file already closed at this point: do not restart logging !
      LLError::setLogFileName(old_log_file);
      llinfos << "Renamed log file '" << mLogFileName << "' into '"
        << old_log_file << "'" << llendl;
#endif
    }
  }
#if LL_WINDOWS	// Windows cannot rename a file which is in use... Bleh !
  else if (on_exit)
#else
  else if (!on_exit)
#endif
  {
    // Remove the last PREVIOUS_LOG log file, if any.
    LLFile::remove(old_log_file);
    std::string log_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
        CURRENT_LOG);
    // Rename the last CURRENT_LOG log file to PREVIOUS_LOG, if any
    if (LLFile::exists(log_file))
    {
      LLFile::rename(log_file, old_log_file);
    }
    // Rename our log as CURRENT_LOG
    LLFile::rename(mLogFileName, log_file);
#if !LL_WINDOWS
    LLError::setLogFileName(log_file);
    llinfos << "Renamed log file '" << mLogFileName << "' into '"
      << log_file << "'" << llendl;
    mLogFileName = log_file;
#endif
  }
}

bool LLAppViewer::loadSettingsFromDirectory(const std::string& location_key,
    bool set_defaults)
{
  // Find and vet the location key.
  if (!mSettingsLocationList.has(location_key))
  {
    llerrs << "Requested unknown location: " << location_key << llendl;
  }

  LLSD location = mSettingsLocationList.get(location_key);

  if (!location.has("PathIndex"))
  {
    llerrs << "Settings location is missing PathIndex value. Settings cannot be loaded."
      << llendl;
  }
  ELLPath path_index = (ELLPath)(location.get("PathIndex").asInteger());
  if (path_index <= LL_PATH_NONE || path_index >= LL_PATH_LAST)
  {
    llerrs << "Out of range path index in app_settings/settings_files.xml"
      << llendl;
  }

  // Iterate through the locations list of files.
  LLSD files = location.get("Files");
  for (LLSD::map_iterator it = files.beginMap(); it != files.endMap(); ++it)
  {
    std::string settings_group = it->first;
    llinfos << "Attempting to load settings for the group "
      << settings_group << " - from location " << location_key
      << llendl;

    if (gSettings.find(settings_group) == gSettings.end())
    {
      llwarns << "No matching settings group for name " << settings_group
        << llendl;
      continue;
    }

    LLSD file = it->second;

    std::string full_settings_path;
    if (file.has("NameFromSetting"))
    {
      std::string custom_settings = file.get("NameFromSetting");
      // *NOTE: Regardless of the group currently being lodaed,
      // this setting is always read from the Global settings.
      if (gSettings[sGlobalSettingsName]->controlExists(custom_settings.c_str()))
      {
        full_settings_path =
          gSettings[sGlobalSettingsName]->getString(custom_settings.c_str());
      }
    }

    if (full_settings_path.empty())
    {
      std::string file_name = file.get("Name");
      full_settings_path = gDirUtilp->getExpandedFilename(path_index,
          file_name);
    }

    S32 requirement = 0;
    if (file.has("Requirement"))
    {
      requirement = file.get("Requirement").asInteger();
    }

    if (!gSettings[settings_group]->loadFromFile(full_settings_path,
          set_defaults))
    {
      if (requirement == 1)
      {
        llwarns << "Error: Cannot load required settings file from: "
          << full_settings_path << llendl;
        return false;
      }
      llwarns << "Cannot load " << full_settings_path
        << " - No settings found." << llendl;
    }
    else
    {
      llinfos << "Loaded settings file " << full_settings_path << llendl;
    }
  }

  return true;
}

void LLAppViewer::saveGlobalSettings()
{
  // Do not fight over the global settings between Cool VL Viewer instances
  // pertaining to the same branch (other viewers and viewers of a different
  // branch got a different settings file name, so we do not care).
  if (!mSameBranchViewer)
  {
    gSavedSettings.saveToFile(gSavedSettings.getString("ClientSettingsFile"));
  }
}

std::string LLAppViewer::getSettingsFilename(const std::string& location_key,
    const std::string& file)
{
  if (mSettingsLocationList.has(location_key))
  {
    LLSD location = mSettingsLocationList.get(location_key);
    if (location.has("Files"))
    {
      LLSD files = location.get("Files");
      if (files.has(file) && files[file].has("Name"))
      {
        return files.get(file).get("Name").asString();
      }
    }
  }
  return std::string();
}

LLApp::InitState LLAppViewer::initConfiguration()
{
  // Set up internal pointers
  gSettings[sGlobalSettingsName] = &gSavedSettings;
  gSettings[sPerAccountSettingsName] = &gSavedPerAccountSettings;

  // Load settings files list
  std::string filename =
    gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
        "settings_files.xml");
  LLControlGroup settings_control("SettingsFiles");
  llinfos << "Loading settings file list " << filename << llendl;
  if (!settings_control.loadFromFile(filename))
  {
    llwarns << "Cannot load default configuration file '" << filename
      << "'. Aborting." << llendl;
    return INIT_FAILED;
  }

  mSettingsLocationList = settings_control.getLLSD("Locations");

  // The settings and command line parsing have a fragile order-of-operation:
  // - load defaults from app_settings
  // - set procedural settings values
  // - read command line settings
  // - selectively apply settings needed to load user settings.
  // - load overrides from user_settings
  // - apply command line settings (to override the overrides)
  // - load per account settings (happens in llstartup.cpp)

  // - load defaults
  bool set_defaults = true;
  if (!loadSettingsFromDirectory("Default", set_defaults))
  {
    std::ostringstream msg;
    msg << gSecondLife << " could not load its default settings file. \n"
      << "The installation may be corrupted. \n";
    OSMessageBox(msg.str());

    return INIT_FAILED;
  }

  // Set procedural settings
  gSavedSettings.setString("ClientSettingsFile",
      gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
        getSettingsFilename("User",
          "Global")));

  // Read command line settings.
  filename = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
      "cmd_line.xml");
  LLControlGroupCLP clp;
  clp.configure(filename, &gSavedSettings);

  if (!initParseCommandLine(clp))
  {
    llwarns << "Error parsing command line options.	Command	Line options ignored."
      << llendl;

    llinfos	<< "Command	line usage:\n" << clp << llendl;

    std::ostringstream msg;
    msg << "An error was found while parsing the command line. Please see:\n"
      << "http://wiki.secondlife.com/wiki/Client_parameters&oldid=878593\n"
      << "or use the --help option to list the available options.\n \n"
      << "Error: " << clp.getErrorMessage();
    OSMessageBox(msg.str());

    return INIT_FAILED;
  }

  // Selectively apply settings

  // If the user has specified an alternate settings file name, load it now
  // before loading the user_settings/settings.xml
  if (clp.hasOption("settings"))
  {
    filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
        clp.getOption("settings")[0]);
    gSavedSettings.setString("ClientSettingsFile", filename);
    llinfos	<< "Using command line specified user settings filename: "
      << filename << llendl;
  }

  // Load overrides from user_settings
  loadSettingsFromDirectory("User");

  // Apply command line settings
  clp.notify();

  // Handle initialization from settings.

  // Start up the debugging console before handling other options.
#if !LL_DEBUG	// always show the console for debug builds
  if (gSavedSettings.getBool("ShowConsoleWindow"))
#endif
  {
    initConsole();
  }

  if (clp.hasOption("help"))
  {
    std::ostringstream msg;
    msg << "Command	line usage:\n" << clp;
    OSMessageBox(msg.str().c_str(), gSecondLife);

    return INIT_OK_EXIT;
  }

  //////////////////////////
  // Apply settings...

  if (clp.hasOption("set"))
  {
    const LLCommandLineParser::token_vector_t& set_values =
      clp.getOption("set");
    if (0x1 & set_values.size())
    {
      llwarns << "Invalid '--set' parameter count." << llendl;
    }
    else
    {
      for (LLCommandLineParser::token_vector_t::const_iterator
          it = set_values.begin(); it != set_values.end(); ++it)
      {
        const std::string& name = *it;
        const std::string& value = *(++it);
        LLControlVariable* c =
          gSettings[sGlobalSettingsName]->getControl(name.c_str());
        if (c)
        {
          c->setValue(value.c_str(), false);
        }
        else
        {
          llwarns << "'--set' specified with unknown setting: '"
            << name << "'." << llendl;
        }
      }
    }
  }

  initGridChoice();

  // Handle slurl use.

  // *NOTE: the command line parser parses tokens and is setup to bail after
  // parsing the '--url' option or the first option specified without an
  // '--option' flag (or any other option that uses the 'last_option'
  // setting. See LLControlGroupCLP::configure())

  if (clp.hasOption("url"))
  {
    std::string url = clp.getOption("url")[0];
    LLSLURL slurl(url);
    LLStartUp::setStartSLURL(slurl);
    if (slurl.getType() == LLSLURL::LOCATION)
    {
      LLGridManager::getInstance()->setGridChoice(slurl.getGrid());
    }
  }

  std::string skin_name = gSavedSettings.getString("SkinCurrent");
  if (skin_name.empty())
  {
    skin_name = "default";
  }
  else
  {
    std::string skin_colors =
      gDirUtilp->getExpandedFilename(LL_PATH_SKINS, skin_name,
          "colors_base.xml");
    if (skin_name != "default" && !LLFile::exists(skin_colors))
    {
      llwarns << "Invalid skin '" << skin_name
        << "', switching to the default skin." << llendl;
      skin_name = "default";
      gSavedSettings.setString("SkinCurrent", skin_name);
    }
  }
  gDirUtilp->setSkinFolder(skin_name);

  setMainloopTimeoutDefault(gSavedSettings.getF32("MainloopTimeoutDefault"));

  // XUI:translate
  gSecondLife.assign(LL_CHANNEL);
  // Build a string representing the true viewer name and version number.
  gViewerVersion = llformat("%s %d.%d.%d.%d", gSecondLife.c_str(),
      LL_VERSION_MAJOR, LL_VERSION_MINOR,
      LL_VERSION_BRANCH, LL_VERSION_RELEASE);

  // Display splash screen. Must be after above check for previous crash as
  // this dialog is always frontmost.
  LLSplashScreen::show();
  LLSplashScreen::update("Loading " + gSecondLife + "...");

  LLVolumeMgr::initClass();

  // Note: this is where we used to initialize gFeatureManagerp.

  gStartTime = LLTimer::totalTime();

  //
  // Set the name of the window
  //
#if LL_DEBUG
  gWindowTitle = gSecondLife + " [DEBUG]";
#else
  gWindowTitle = gSecondLife;
#endif
  LLStringUtil::truncate(gWindowTitle, 255);

  // Check for another instance of the app running
  mSecondInstance = anotherInstanceRunning();

  // If we received a SLURL and it is not a login command, hand it off to the
  // existing instance, if any.
  LLSLURL slurl = LLStartUp::getStartSLURL();
  if (slurl.isValid() && slurl.getAppCmd() != "login" && mSecondInstance &&
      sendURLToOtherInstance(slurl.getSLURLString()))
  {
    llinfos << "Sent SLURL '" << slurl.getSLURLString()
      << "' to the already running viewer instance. Exiting."
      << llendl;
    // Let's consider we are also using the same viewer to avoid
    // overwriting the log with our (mostly irrelevant) log.
    mIsSiblingViewer = true;
    renameLog(true);	// Rename the log now
    // Successfully handed off URL to existing instance; let's exit now.
    return INIT_OK_EXIT;
  }

  if (mSecondInstance)
  {
    if (gSavedSettings.getBool("AllowMultipleViewers"))
    {
      // This is the second instance of SL. Turn off voice support, but
      // make sure the setting is *not* persisted.
      LLControlVariable* disable_voice =
        gSavedSettings.getControl("CmdLineDisableVoice");
      if (disable_voice)
      {
        disable_voice->setValue(LLSD(true), false);
      }
    }
    else
    {
      std::ostringstream msg;
      msg << "A viewer instance is already running and " << gSecondLife
        << "\nwas configured to refuse to run as a second instance.\n"
        << "You may change that preference if you wish...";
      OSMessageBox(msg.str(), gSecondLife);
      return INIT_FAILED;
    }
  }
  else
  {
    // This is the first instance: check for stale/crash markers and
    // initialize the instance marker.
    initMarkerFile();
  }

  // Need to do this here because we must have initialized global settings
  // first
  std::string next_login_loc = gSavedSettings.getString("NextLoginLocation");
  if (next_login_loc.length())
  {
    LLStartUp::setStartSLURL(LLSLURL(next_login_loc));
  }

  gLastRunVersion = gSavedSettings.getString("LastRunVersion");

  return INIT_OK; // Config was successful.
}

bool LLAppViewer::initWindow()
{
  llinfos << "Initializing window..." << llendl;

  // Always start windowed
  S32 x0 = gSavedSettings.getS32("WindowX");
  S32 y0 = gSavedSettings.getS32("WindowY");
  // Sadly, these have been historically stored as signed integers, where it
  // does not make any sense...
  S32 signed_width = gSavedSettings.getS32("WindowWidth");
  U32 width = signed_width >= 0 ? (U32)signed_width : 0;
  S32 signed_height = gSavedSettings.getS32("WindowHeight");
  U32 height = signed_height >= 0 ? (U32)signed_height : 0;
  bool ignore_depth = gSavedSettings.getBool("IgnorePixelDepth");
  gViewerWindowp = new LLViewerWindow(gWindowTitle, x0, y0, width, height,
      false, ignore_depth);

  // Hide the splash screen
  LLSplashScreen::hide();

  // This happens when we do not have the minimum OpenGL requirements.
  if (!LLViewerShaderMgr::sInitialized)
  {
    return false;
  }

  if (gSavedSettings.getBool("FullScreen"))
  {
    // Request to go full screen... which will be delayed until login
    gViewerWindowp->toggleFullscreen();
  }

  if (gSavedSettings.getBool("WindowMaximized"))
  {
    gWindowp->maximize();
    gWindowp->setNativeAspectRatio(gSavedSettings.getF32("FullScreenAspectRatio"));
  }

  // Initialize the environment classes
  llinfos << "Initializing environment classes..." << llendl;
  gWLSkyParamMgr.initClass();
  gWLWaterParamMgr.initClass();
  gEnvironment.initClass();

  //
  // Initialize GL stuff
  //

  // Set this flag in case we crash while initializing GL
  gSavedSettings.setBool("RenderInitError", true);
  saveGlobalSettings();

  llinfos << "Initializing the render pipeline..." << llendl;
  gPipeline.init();
  llinfos << "Render pipeline initialized." << llendl;

  gViewerWindowp->initGLDefaults();

  gSavedSettings.setBool("RenderInitError", false);
  saveGlobalSettings();

  LLTrans::parseStrings("strings.xml");
  LLUITrans::parseStrings("ui_strings.xml");
  // Set error messages for LLXMLRPCTransaction, now that the strings are
  // parsed.
  LLXMLRPCTransaction::setMessages(LLTrans::getString("server_is_down"),
      LLTrans::getString("dns_not_resolving"),
      LLTrans::getString("cert_not_verified"),
      LLTrans::getString("connect_error"));

  // Show watch cursor
  gViewerWindowp->setCursor(UI_CURSOR_WAIT);

  // Finish view initialization
  gViewerWindowp->initBase();

  // We can now (potentially) enable this.
  LLView::sDebugRects = gSavedSettings.getBool("DebugViews");

  llinfos << "Window initialization done." << llendl;
  return true;
}

void LLAppViewer::writeDebugInfo(bool log_interesting_info)
{
  // Since the debug_info.log is mostly useless (see the comment below), and
  // is overwritten without care by any other viewer running instances, I
  // made it optional (and OFF by default) in the Cool VL Viewer. HB
  if (gSavedSettings.getBool("WriteDebugInfo"))
  {
    std::string filename =
      gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "debug_info.log");
    llinfos << "Opening debug file " << filename << llendl;
    llofstream out_file(filename.c_str());
    if (out_file.is_open())
    {
      LLSDSerialize::toPrettyXML(gDebugInfo, out_file);
      out_file.close();
    }
  }
  if (!log_interesting_info)
  {
    return;
  }
  // Really, this is the only interesting info that has not already been
  // logged (or is hard to find in the log)... HB
  if (gDebugInfo.has("MainloopTimeoutState"))
  {
    llinfos << "Mainloop timeout state: "
      << gDebugInfo["MainloopTimeoutState"].asString() << llendl;
  }
  llinfos << "Grid: " << gDebugInfo["GridName"].asString() << llendl;
  // This info may exist on crashes, and is interesting...
  if (gDebugInfo.has("CurrentLocationX"))
  {
    llinfos << "Agent position: " << gDebugInfo["CurrentRegion"].asString()
      << " " << (S32)gDebugInfo["CurrentLocationX"].asReal()
      << "," << (S32)gDebugInfo["CurrentLocationY"].asReal()
      << "," << (S32)gDebugInfo["CurrentLocationZ"].asReal()
      << llendl;
    if (gDebugInfo.has("ParcelMusicURL"))
    {
      llinfos << "Parcel music URL: "
        << gDebugInfo["ParcelMusicURL"].asString() << llendl;
    }
    if (gDebugInfo.has("ParcelMediaURL"))
    {
      llinfos << "Parcel media URL: "
        << gDebugInfo["ParcelMediaURL"].asString() << llendl;
    }
  }
}

void LLAppViewer::cleanupSavedSettings()
{
  gSavedSettings.setBool("FlyBtnState", false);
  gSavedSettings.setBool("BuildBtnState", false);

  gSavedSettings.setBool("DebugWindowProc", gDebugWindowProc);

  gSavedSettings.setBool("AllowTapTapHoldRun", gAllowTapTapHoldRun);
  gSavedSettings.setBool("ShowObjectUpdates", gShowObjectUpdates);

  if (gDebugViewp && gDebugViewp->mDebugConsolep)
  {
    gSavedSettings.setBool("ShowDebugConsole",
        gDebugViewp->mDebugConsolep->getVisible());
  }

  // Save window position if not fullscreen as we do not track it in
  // callbacks
  if (gWindowp)
  {
    bool fullscreen = gWindowp->getFullscreen();
    bool maximized = gWindowp->getMaximized();
    if (!fullscreen && !maximized)
    {
      LLCoordScreen window_pos;
      if (gWindowp->getPosition(&window_pos))
      {
        gSavedSettings.setS32("WindowX", window_pos.mX);
        gSavedSettings.setS32("WindowY", window_pos.mY);
      }
    }
  }

  gSavedSettings.setF32("MapScale", LLPanelWorldMap::sMapScale);
  gSavedSettings.setBool("ShowHoverTips", LLHoverView::sShowHoverTips);

  // Some things are cached in LLAgent.
  if (gAgent.mInitialized)
  {
    gSavedSettings.setF32("RenderFarClip", gAgent.mDrawDistance);
  }
}

void LLAppViewer::removeCacheFiles(const char* file_mask)
{
  LLDirIterator::deleteFilesInDir(gDirUtilp->getCacheDir(), file_mask);
}

void LLAppViewer::writeSystemInfo()
{
  gDebugInfo["SLLog"] = LLError::logFileName();

  gDebugInfo["ClientInfo"]["Name"] =
    gSavedSettings.getString("VersionChannelName");
  gDebugInfo["ClientInfo"]["MajorVersion"] = LL_VERSION_MAJOR;
  gDebugInfo["ClientInfo"]["MinorVersion"] = LL_VERSION_MINOR;
  gDebugInfo["ClientInfo"]["PatchVersion"] = LL_VERSION_BRANCH;
  gDebugInfo["ClientInfo"]["BuildVersion"] = LL_VERSION_RELEASE;

  gDebugInfo["CRTFilename"] = gDirUtilp->getCRTFile();

  LLCPUInfo* cpuinfo = LLCPUInfo::getInstance();
  gDebugInfo["CPUInfo"]["CPUString"] = cpuinfo->getCPUString();
  gDebugInfo["CPUInfo"]["CPUFamily"] = cpuinfo->getFamily();
  gDebugInfo["CPUInfo"]["CPUMhz"] = cpuinfo->getMHz();
  gDebugInfo["CPUInfo"]["CPUSSE2"] = cpuinfo->hasSSE2();

  gDebugInfo["RAMInfo"]["Physical"] =
    (LLSD::Integer)LLMemoryInfo::getPhysicalMemoryKB();
  gDebugInfo["RAMInfo"]["Allocated"] =
    (LLSD::Integer)(gMemoryAllocated >> 10); // MB -> KB
  gDebugInfo["OSInfo"] = LLOSInfo::getInstance()->getOSStringSimple();

  // The user is not logged on yet, but record the current grid choice login url
  // which may have been the intended grid.
  gDebugInfo["GridName"] = LLGridManager::getInstance()->getGridLabel();

  // *FIX:Mani - move this ddown in llappviewerwin32
#if LL_WINDOWS
  DWORD thread_id = GetCurrentThreadId();
  gDebugInfo["MainloopThreadID"] = (S32)thread_id;
#endif

  // "CrashNotHandled" is set here, while things are running well. If the
  // crash is handled by LLAppViewer::handleViewerCrash, i.e. not a freeze,
  // then the value of "CrashNotHandled" will be set to false.
  gDebugInfo["CrashNotHandled"] = (LLSD::Boolean)true;

  // Dump some debugging info
  llinfos << gSecondLife << " version " << LL_VERSION_MAJOR << "."
    << LL_VERSION_MINOR << "." << LL_VERSION_BRANCH << "."
    << LL_VERSION_RELEASE << llendl;

  // Dump the local time and time zone
  time_t now;
  time(&now);
  char tbuffer[256];
  strftime(tbuffer, 256, "%Y-%m-%dT%H:%M:%S %Z", localtime(&now));
  llinfos << "Local time: " << tbuffer << llendl;

  // Query some system information
  llinfos << "CPU info:\n" << LLCPUInfo::getInstance()->getInfo() << llendl;
  llinfos << "Memory info:\n" << LLMemoryInfo::getInfo() << llendl;
  llinfos << "OS: " << LLOSInfo::getInstance()->getOSStringSimple()
    << llendl;
  llinfos << "OS info: " << LLOSInfo::getInstance()->getOSString() << llendl;

  llinfos << "CPU single-core benchmarking..." << llendl;
  LLCPUInfo::benchmarkFactor();

  writeDebugInfo(false); // Save out debug_info.log early, in case of crash.
}

//static
void LLAppViewer::handleSyncViewerCrash()
{
  // Call to pure virtual, handled by platform specific llappviewer instance.
  gAppViewerp->handleSyncCrashTrace();
}

//static
void LLAppViewer::handleViewerCrash()
{
  // Free our reserved memory space before dumping the stack trace
  LLMemory::cleanupClass();

  llinfos << "Handle viewer crash entry." << llendl;

  LLMemory::logMemoryInfo();

  if (gAppViewerp->beingDebugged())
  {
    // This will drop us into the debugger (if not already done).
    abort();
  }

  // Make sure the watchdog gets turned off...
  gAppViewerp->destroyMainloopTimeout();

  // We already do this in writeSystemInfo(), but we do it again here to make
  // *sure* we have a version to check against no matter what
  gDebugInfo["ClientInfo"]["Name"] =
    gSavedSettings.getString("VersionChannelName");
  gDebugInfo["ClientInfo"]["MajorVersion"] = LL_VERSION_MAJOR;
  gDebugInfo["ClientInfo"]["MinorVersion"] = LL_VERSION_MINOR;
  gDebugInfo["ClientInfo"]["PatchVersion"] = LL_VERSION_BRANCH;
  gDebugInfo["ClientInfo"]["BuildVersion"] = LL_VERSION_RELEASE;

  gDebugInfo["GridName"] = LLGridManager::getInstance()->getGridLabel();

  LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
  if (parcel && parcel->getMusicURL()[0])
  {
    gDebugInfo["ParcelMusicURL"] = parcel->getMusicURL();
  }
  if (parcel && !parcel->getMediaURL().empty())
  {
    gDebugInfo["ParcelMediaURL"] = parcel->getMediaURL();
  }

  gDebugInfo["SettingsFilename"] =
    gSavedSettings.getString("ClientSettingsFile");
  gDebugInfo["CRTFilename"] = gDirUtilp->getCRTFile();
  gDebugInfo["ViewerExePath"] = gDirUtilp->getExecutablePathAndName();
  gDebugInfo["CurrentPath"] = gDirUtilp->getCurPath();
  gDebugInfo["SessionLength"] = F32(LLFrameTimer::getElapsedSeconds());
  gDebugInfo["StartupState"] = LLStartUp::getStartupStateString();
  gDebugInfo["RAMInfo"]["Allocated"] =
    (LLSD::Integer)(LLMemory::getCurrentRSS() >> 10);
  gDebugInfo["FirstLogin"] = (LLSD::Boolean)gAgent.isFirstLogin();
  gDebugInfo["FirstRunThisInstall"] =
    gSavedSettings.getBool("FirstRunThisInstall");

  if (gLogoutInProgress)
  {
    gDebugInfo["LastExecEvent"] = LAST_EXEC_LOGOUT_CRASH;
  }
  else
  {
    gDebugInfo["LastExecEvent"] =
      sLLErrorActivated ? LAST_EXEC_LLERROR_CRASH
      : LAST_EXEC_OTHER_CRASH;
  }

  LLViewerRegion* regionp = gAgent.getRegion();
  if (regionp)
  {
    gDebugInfo["CurrentSimHost"] = regionp->getSimHostName();
    gDebugInfo["CurrentRegion"] = regionp->getName();

    const LLVector3& loc = gAgent.getPositionAgent();
    gDebugInfo["CurrentLocationX"] = loc.mV[0];
    gDebugInfo["CurrentLocationY"] = loc.mV[1];
    gDebugInfo["CurrentLocationZ"] = loc.mV[2];
  }

  if (gAppViewerp->mMainloopTimeout)
  {
    gDebugInfo["MainloopTimeoutState"] =
      *gAppViewerp->mMainloopTimeout->getState();
  }

  // The crash is being handled here so set this value to false.
  gDebugInfo["CrashNotHandled"] = (LLSD::Boolean)false;

  // Write out the crash status file. Use marker file style setup, as that is
  // the simplest, especially since we are already in a crash situation.
  if (gDirUtilp)
  {
    std::string filename;
    if (sLLErrorActivated)
    {
      filename = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
          LLERROR_MARKER_FILE_NAME);
    }
    else
    {
      filename = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
          ERROR_MARKER_FILE_NAME);
    }
    llinfos << "Creating crash marker file " << filename << llendl;
    LLFile marker_file(filename, "w");
    if (marker_file)
    {
      gAppViewerp->stampMarkerFile(&marker_file); 
      llinfos << "Created marker file " << filename << llendl;
    }
    else
    {
      llwarns << "Cannot create marker file " << filename << llendl;
    }
  }

  if (gMessageSystemp)
  {
    gMessageSystemp->getCircuitInfo(gDebugInfo["CircuitInfo"]);
    gMessageSystemp->stopLogging();

    std::ostringstream stats;
    gMessageSystemp->summarizeLogs(stats);
    gDebugInfo["MessageSystemStats"] = stats.str();
  }

  gWorld.getInfo(gDebugInfo);

  // Close the debug file
  gAppViewerp->writeDebugInfo();

  // Remove the marker file, since we will spawn a process that would
  // otherwise keep it locked
  if (gDebugInfo["LastExecEvent"].asInteger() == LAST_EXEC_LOGOUT_CRASH)
  {
    gAppViewerp->removeMarkerFile(true);
  }
  else
  {
    gAppViewerp->removeMarkerFile(false);
  }

  // This is needed to ensure that the log file is properly flushed,
  // especially under Linux (there is apparently a destructors ordering
  // issue that prevents it to flush and close naturally otherwise)...
  LLError::logToFile("");
}

bool LLAppViewer::anotherInstanceRunning()
{
  // We create a marker file when the program starts and remove the file when
  // it finishes. If the file is currently locked, that means another process
  // is already running.

  std::string marker_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
      MARKER_FILE_NAME);
  LL_DEBUGS("MarkerFile") << "Checking marker file for lock..." << LL_ENDL;

  // Freeze case checks
  if (LLFile::exists(marker_file))
  {
    static bool checked = false;
    if (!checked)
    {
      // Do not check again since the file will contain gViewerVersion
      // after a successful run of initMarkerFile()
      checked = true;
      // File exists, look at what is inside...
      mIsOurViewer = isOurMarkerFile(marker_file);
      checkSiblingMarkerFile(marker_file);
    }

    // Try opening with appending permissions (*should* fail if locked):
    // Using append to avoid wiping the file contents on success (since we
    // will need to examine that contents later on in initMarkerFile()). HB
    LLFile outfile(marker_file, "a");
    if (!outfile)
    {
      // Another instance is running. Skip the rest of these operations.
      llinfos << "Cannot open marker file for writing." << llendl;
      return true;
    }
    LL_DEBUGS("MarkerFile") << "Could open the marker file for writing."
      << LL_ENDL;
    // Try acquiring an exclusive lock (shall fail if locked).
    if (!outfile.lock(true))
    {
      llinfos << "Marker file is locked by another instance." << llendl;
      return true;
    }
#if LL_WINDOWS
    // Ensure the OS immediately releases the lock we just acquired
    outfile.unlock();
#endif
  }
  LL_DEBUGS("MarkerFile") << "Marker file does not pertain to a running instance."
    << LL_ENDL;
  // No other instance running
  return false;
}

void LLAppViewer::initMarkerFile()
{
  // We have got 5 things to check for here:
  // - Other viewer running (SecondLife.exec_marker present, locked)
  // - Freeze (SecondLife.exec_marker present, not locked)
  // - LLError crash (llerror_marker present)
  // - Unexpected crash (error_marker present)
  // - Crash or freeze after logout (logout_marker present)
  // These checks should also remove all but the exec_marker file if they
  // currently exist

  mMarkerFileName = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
      MARKER_FILE_NAME);
  LL_DEBUGS("MarkerFile") << "Checking marker file for lock..." << LL_ENDL;

  // LLError/Error checks. Only one of these should ever happen at a time.
  std::string logout_marker_file =
    gDirUtilp->getExpandedFilename(LL_PATH_LOGS, LOGOUT_MARKER_FILE_NAME);
  std::string llerror_marker_file =
    gDirUtilp->getExpandedFilename(LL_PATH_LOGS, LLERROR_MARKER_FILE_NAME);
  std::string error_marker_file =
    gDirUtilp->getExpandedFilename(LL_PATH_LOGS, ERROR_MARKER_FILE_NAME);

  std::string diagnosis;
  if (!mSecondInstance && LLFile::exists(mMarkerFileName))
  {
    if (mIsOurViewer)
    {
      gLastExecEvent = LAST_EXEC_FROZE;
      diagnosis = "Last session froze unexpectedly";
    }
    else
    {
      llinfos << "An execution marker file has been found but is not ours: another viewer froze..."
        << llendl;
    }
  }
  if (LLFile::exists(logout_marker_file))
  {
    if (isOurMarkerFile(logout_marker_file))
    {
      gLastExecEvent = LAST_EXEC_LOGOUT_FROZE;
      diagnosis = "Last session froze after logout";
    }
    else
    {
      llinfos << "A logout marker file has been found but is not ours: another viewer crashed after logout..."
        << llendl;
    }
    LLFile::remove(logout_marker_file);
  }
  if (LLFile::exists(llerror_marker_file))
  {
    if (isOurMarkerFile(llerror_marker_file))
    {
      if (gLastExecEvent == LAST_EXEC_LOGOUT_FROZE)
      {
        gLastExecEvent = LAST_EXEC_LOGOUT_CRASH;
        diagnosis = "Last session crashed on a LLError after logout";
      }
      else
      {
        gLastExecEvent = LAST_EXEC_LLERROR_CRASH;
        diagnosis = "Last session crashed on a LLError";
      }
    }
    else
    {
      llinfos << "An LLError marker file has been found but is not ours: another viewer crashed..."
        << llendl;
    }
    LLFile::remove(llerror_marker_file);
  }
  if (LLFile::exists(error_marker_file))
  {
    if (isOurMarkerFile(error_marker_file))
    {
      if (gLastExecEvent == LAST_EXEC_LOGOUT_FROZE)
      {
        gLastExecEvent = LAST_EXEC_LOGOUT_CRASH;
        diagnosis = "Last session crashed unexpectedly after logout";
      }
      else if (gLastExecEvent != LAST_EXEC_LOGOUT_CRASH)
      {
        gLastExecEvent = LAST_EXEC_OTHER_CRASH;
        diagnosis = "Last session crashed unexpectedly";
      }
    }
    else
    {
      llinfos << "An unexpected error marker file has been found but is not ours: another viewer crashed..."
        << llendl;
    }
    LLFile::remove(error_marker_file);
  }

  if (!diagnosis.empty())
  {
    llwarns << diagnosis << llendl;
  }

  // No new markers if another instance is running.
  if (mSecondInstance)
  {
    return;
  }

  // Create the marker file for this execution & lock it
  mMarkerFile = new LLFile(mMarkerFileName, "w");
  if (mMarkerFile->getStream())
  {
    LL_DEBUGS("MarkerFile") << "Marker file created." << LL_ENDL;
  }
  else
  {
    delete mMarkerFile;
    mMarkerFile = NULL;
    llwarns << "Failed to create marker file." << llendl;
    mSecondInstance = true; // lock means that instance is running.
    return;
  }
  if (!mMarkerFile->lock())
  {
    delete mMarkerFile;
    mMarkerFile = NULL;
    llwarns << "Marker file cannot be locked." << llendl;
    return;
  }
  // Windows is stupid: you cannot change the contents of a locked file, when
  // you own a shared lock on it, and you cannot read it from another process
  // if you take an exclusive lock on it. This is unlike POSIX systems where
  // a write-locked file can still be written by the lock holder and read by
  // everyone... HB
#if LL_WINDOWS
  LL_DEBUGS("MarkerFile") << "Marker file unlocked." << LL_ENDL;
  mMarkerFile->unlock();
#else
  LL_DEBUGS("MarkerFile") << "Marker file locked." << LL_ENDL;
#endif
  LL_DEBUGS("MarkerFile") << "Stamping marker file: " << mMarkerFileName
    << LL_ENDL;
  stampMarkerFile(mMarkerFile);
#if LL_WINDOWS
  mMarkerFile->lock();
  LL_DEBUGS("MarkerFile") << "Marker file locked." << LL_ENDL;
#endif
}

// Stamp the marker file as pertaining to our viewer
void LLAppViewer::stampMarkerFile(LLFile* marker_file)
{
  if (marker_file->getStream())
  {
    // Stamp the marker file as pertaining to our viewer
    marker_file->write((const U8*)gViewerVersion.c_str(),
        strlen(gViewerVersion.c_str()));
    marker_file->flush();
  }
}

bool LLAppViewer::isOurMarkerFile(std::string& filename)
{
  char buff[256];	// Must be able to hold gViewerVersion.c_str()
  LLFile infile(filename, "rb");
  S32 n = infile.read((U8*)buff, strlen(gViewerVersion.c_str()));
  buff[n] = '\0';
  bool is_ours = strcmp(buff, gViewerVersion.c_str()) == 0;
  LL_DEBUGS("MarkerFile") << "Marker file " << filename
    << (is_ours ? " is ours." : " is not ours.")
    << LL_ENDL;
  return is_ours;
}

void LLAppViewer::checkSiblingMarkerFile(std::string& filename)
{
  char buff[256];	// Must be able to hold gViewerVersion.c_str()
  LLFile infile(filename, "rb");
  S32 n = infile.read((U8*)buff, 255);
  std::string viewer(buff, n);
  size_t i = gViewerVersion.rfind('.');
  if (i != std::string::npos && viewer.rfind('.') == i)
  {
    mSameBranchViewer = viewer.substr(0, i) == gViewerVersion.substr(0, i);
  }
  mIsSiblingViewer = viewer.find(gSecondLife) == 0;
  if (mSameBranchViewer)
  {
    llinfos << "Another " << gViewerVersion.substr(0, i)
      << " instance is already running." << llendl;
  }
  else if (mIsSiblingViewer)
  {
    llinfos << "Another " << gSecondLife << " instance is already running."
      << llendl;
  }
}

void LLAppViewer::removeMarkerFile(bool leave_logout_marker)
{
  if (mMarkerFile)
  {
    LL_DEBUGS("MarkerFile") << "Removing main marker file: "
      << mMarkerFileName << LL_ENDL;
    delete mMarkerFile;
    mMarkerFile = NULL;
    LLFile::remove(mMarkerFileName);
  }
  if (mOwnsLogoutMarkerFile && !leave_logout_marker)
  {
    LL_DEBUGS("MarkerFile") << "Removing logout marker file: "
      << mLogoutMarkerFileName << LL_ENDL;
    LLFile::remove(mLogoutMarkerFileName);
    mOwnsLogoutMarkerFile = false;
  }
}

void LLAppViewer::forceQuit()
{
  llinfos << "Quitting..." << llendl;
  LLApp::setQuitting();
}

void LLAppViewer::requestQuit()
{
  llinfos << "Quitting..." << llendl;

  LLViewerRegion* region = gAgent.getRegion();
  if (!region || !LLStartUp::isLoggedIn())
  {
    // If we have a region, make some attempt to send a logout request
    // first. This prevents the halfway-logged-in avatar from hanging
    // around inworld for a couple minutes.
    if (region)
    {
      sendLogoutRequest();
    }

    // Quit immediately
    forceQuit();
    return;
  }

  LLHUDEffectSpiral* effectp =
    (LLHUDEffectSpiral*)LLHUDManager::createEffect(LLHUDObject::LL_HUD_EFFECT_POINT);
  effectp->setPositionGlobal(gAgent.getPositionGlobal());
  effectp->setColor(LLColor4U(gAgent.getEffectColor()));
  LLHUDManager::sendEffects();
  effectp->markDead() ;//remove it.

  // Attempt to close all floaters that might be/ editing things.
  if (gFloaterViewp)
  {
    // Application is quitting
    gFloaterViewp->closeAllChildren(true);
  }

  LLViewerStats::getInstance()->sendStats();

  gLogoutTimer.reset();
  mQuitRequested = true;
}

static bool finish_quit(const LLSD& notification, const LLSD& response)
{
  if (LLNotification::getSelectedOption(notification, response) == 0)
  {
    // Some OpenSim grids can really be annoying and spuriuously trigger
    // "You have been disconnected" dialogs on normal logouts... Let's flag
    // that it is a normal logout. NOTE: do NOT flag it with sDoDisconnect
    // or LLApp::setQuitting() (the first would make the viewer wait
    // forever, and the second would cause a truncated logout process). HB
    sLoggingOut = true;
    gAppViewerp->requestQuit();
  }
  return false;
}
static LLNotificationFunctorRegistration finish_quit_reg1("ConfirmQuit",
    finish_quit);
static LLNotificationFunctorRegistration finish_quit_reg2("ConfirmQuitNotifications",
    finish_quit);

void LLAppViewer::userQuit()
{
  llinfos << "Quitting..." << llendl;

  if (gDisconnected || !gViewerWindowp ||
      !gViewerWindowp->getProgressView() ||
      gViewerWindowp->getProgressView()->getVisible())
  {
    requestQuit();
  }
  else if (LLNotifyBox::getNotifyBoxCount() +
      LLGroupNotifyBox::getGroupNotifyBoxCount() > 0)
  {
    if (!LLNotifyBox::areNotificationsShown())
    {
      LLNotifyBox::setShowNotifications(true);
    }
    gNotifications.add("ConfirmQuitNotifications");
  }
  else
  {
    gNotifications.add("ConfirmQuit");
  }
}

static bool finish_early_exit(const LLSD& notification, const LLSD& response)
{
  gAppViewerp->forceQuit();
  return false;
}

void LLAppViewer::earlyExit(const std::string& name, const LLSD& substitutions)
{
  llwarns << "app_early_exit: " << name << llendl;
  sDoDisconnect = true;
  gNotifications.add(name, substitutions, LLSD(), finish_early_exit);
}

void LLAppViewer::forceExit()
{
  LLSplashScreen::update("Shutting down...");
  ms_sleep(1000);
  removeMarkerFile();
  LLSplashScreen::hide();
  // *FIXME: this kind of exit hardly seems appropriate - Mani.
  exit(-1);	// -1 is the code we use for "application init failed".
}

void LLAppViewer::abortQuit()
{
  llinfos << "abortQuit()" << llendl;
  mQuitRequested = false;
}

bool LLAppViewer::initCache()
{
  mPurgeCache = false;
  bool read_only = mIsSiblingViewer;	// If same cache directory in use

  gTextureCachep->setReadOnly(read_only);
  LLVOCache::getInstance()->setReadOnly(read_only);

  // Get the maximum cache size from the debug settings and clamp it
  constexpr S64 MB = 1024L * 1024L;
  S64 cache_size = (S64)(gSavedSettings.getU32("CacheSize")) * MB;
  constexpr S64 MAX_CACHE_SIZE = 10240L * MB;
  cache_size = llmin(cache_size, MAX_CACHE_SIZE);
  // Percent of the cache to reserve to assets
  F64 assets_percent = gSavedSettings.getF32("AssetsCachePercentOfTotal");
  // Assets cache maximum size
  U64 assets_cache_size = (U64)((F64)cache_size * assets_percent / 100.0);
  // Give textures the rest of the cache less 5% for objects cache and
  // inventory.
  S64 texture_cache_size =  100 * cache_size / 95 - (S64)assets_cache_size;

  if (!read_only)
  {
    // Purge cache if user requested it
    if (gSavedSettings.getBool("PurgeCacheOnStartup") ||
        gSavedSettings.getBool("PurgeCacheOnNextStartup"))
    {
      gSavedSettings.setBool("PurgeCacheOnNextStartup", false);
      mPurgeCache = true;
    }

    // Setup and verify the cache location
    std::string cache_location = gSavedSettings.getString("CacheLocation");
    std::string new_loc = gSavedSettings.getString("NewCacheLocation");
    if (new_loc != cache_location)
    {
      LLSplashScreen::update("Clearing cache...");
      gDirUtilp->setCacheDir(gSavedSettings.getString("CacheLocation"));
      purgeCache(); // Purge old cache
      mPurgeCache = false;
      gSavedSettings.setString("CacheLocation", new_loc);
    }
  }

  if (!gDirUtilp->setCacheDir(gSavedSettings.getString("CacheLocation")))
  {
    llwarns << "Unable to set cache location" << llendl;
    gSavedSettings.setString("CacheLocation", "");
  }

  if (mPurgeCache && !read_only)
  {
    LLSplashScreen::update("Clearing cache...");
    purgeCache();
  }

  LLSplashScreen::update("Initializing texture cache...");
  S64 extra = gTextureCachep->initCache(LL_PATH_CACHE, texture_cache_size);
  texture_cache_size -= extra;

#if 0	// We now initialize the object cache after login, so that it can take
  // the grid name into account and keep cached regions on a per-grid
  // basis. HB
  U32 cache_size = gSavedSettings.getU32("CacheNumberOfRegionsForObjects");
  LLVOCache::getInstance()->initCache(LL_PATH_CACHE, cache_size);
#endif

  LLSplashScreen::update("Initializing asset cache...");
  LLDiskCache::init(assets_cache_size, read_only);
  if (!read_only)
  {
    if (gSavedSettings.getBool("ClearAssetsCache"))
    {
      LLSplashScreen::update("Clearing the asset cache...");
      llinfos << "Clearing the cached assets, on user request." << llendl;
      gSavedSettings.setBool("ClearAssetsCache", false);
      LLDiskCache::clear();
    }
    else
    {
      LLDiskCache::threadedPurge();
    }
  }

  return true;
}

void LLAppViewer::purgeCache()
{
  llinfos << "Clearing all caches..." << llendl;
  gTextureCachep->purgeCache(LL_PATH_CACHE);
  LLVOCache::getInstance()->removeCache(LL_PATH_CACHE);
  LLDiskCache::clear();
  LLDirIterator::deleteFilesInDir(gDirUtilp->getCacheDir());
}

// Callback from a dialog indicating user was logged out.
bool finish_disconnect(const LLSD& notification, const LLSD& response)
{
  if (LLNotification::getSelectedOption(notification, response) == 1)
  {
    gAppViewerp->forceQuit();
  }
  return false;
}

// Callback from an early disconnect dialog, force an exit
bool finish_forced_disconnect(const LLSD& notification, const LLSD& response)
{
  gAppViewerp->forceQuit();
  return false;
}

void LLAppViewer::forceDisconnect(const std::string& mesg)
{
  if (sDoDisconnect)
  {
    // Already popped up one of these dialogs, do not do this again.
    return;
  }
  sDoDisconnect = true;

  if (!gIsInSecondLife && sLoggingOut)
  {
    // In OpenSim, we may get here while logging out normally (see the
    // comment in finish_quit())... So just confirm that we are indeed
    // quitting ! HB
    gAppViewerp->forceQuit();
    return;
  }

  // Translate the message if possible
  std::string big_reason = LLAgent::sTeleportErrorMessages[mesg];
  if (big_reason.size() == 0)
  {
    big_reason = mesg;
  }
  // Tell the user what happened
  LLSD args;
  if (LLStartUp::isLoggedIn())
  {
    args["MESSAGE"] = big_reason;
    gNotifications.add("YouHaveBeenLoggedOut", args, LLSD(),
        finish_disconnect);
    gExitCode = EXIT_FORCE_LOGGED_OUT;
  }
  else
  {
    args["ERROR_MESSAGE"] = big_reason;
    gNotifications.add("ErrorMessage", args, LLSD(),
        finish_forced_disconnect);
    gExitCode = EXIT_LOGIN_FAILED;
  }
}

void LLAppViewer::badNetworkHandler()
{
  // Dump the packet
  gMessageSystemp->dumpPacketToLog();

  // Flush all of our caches on exit in the case of disconnect due to invalid
  // packets.
  mPurgeOnExit = true;

  std::ostringstream message;
  message << gSecondLife << " has detected mangled\n"
    << "network data indicative of a bad upstream network\n"
    << "connection or an incompatibility between the viewer\n"
    << "and the grid you are connected to.\n"
    << "If the problem persists, please report it on the\n"
    << "support forum at: http://sldev.free.fr/forum/";
  forceDisconnect(message.str());
}

// This routine may get called more than once during the shutdown process.
// This can happen because we need to get the screenshot before the window
// is destroyed.
void LLAppViewer::saveFinalSnapshot()
{
  if (gViewerWindowp && !mSavedFinalSnapshot)
  {
    gViewerWindowp->setCursor(UI_CURSOR_WAIT);
    // Do not animate, need immediate switch:
    gAgent.changeCameraToThirdPerson(false);
    gSavedSettings.setBool("ShowParcelOwners", false);
    gSavedSettings.setBool("RenderHUDInSnapshot", false);
    idle(false);

    std::string snap_filename = gDirUtilp->getLindenUserDir();
    snap_filename += LL_DIR_DELIM_STR;
    if (gIsInProductionGrid)
    {
      snap_filename += SCREEN_LAST_FILENAME;
    }
    else
    {
      snap_filename += SCREEN_LAST_BETA_FILENAME;
    }
    // Use full pixel dimensions of viewer window (do not post-scale
    // dimensions)
    gViewerWindowp->saveSnapshot(snap_filename,
        gViewerWindowp->getWindowDisplayWidth(),
        gViewerWindowp->getWindowDisplayHeight(),
        false, true);
    mSavedFinalSnapshot = true;
  }
}

void LLAppViewer::loadNameCache()
{
  std::string prefix;
  if (!gIsInSecondLife)
  {
    prefix = LLGridManager::getInstance()->getGridLabel() + "_";
  }

  // Display names cache
  std::string filename =
    gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
        prefix + "avatar_name_cache.xml");
  llifstream name_cache_stream(filename.c_str());
  if (name_cache_stream.is_open())
  {
    if (!LLAvatarNameCache::importFile(name_cache_stream))
    {
      llwarns << "Removing invalid name cache file." << llendl;
      name_cache_stream.close();
      LLFile::remove(filename);
    }
  }

  if (!gCacheNamep) return;

  filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
      prefix + "name.cache");
  llifstream cache_file(filename.c_str());
  if (cache_file.is_open())
  {
    gCacheNamep->importFile(cache_file);
  }
}

void LLAppViewer::saveNameCache()
{
  std::string prefix;
  if (!gIsInSecondLife)
  {
    prefix = LLGridManager::getInstance()->getGridLabel() + "_";
  }

  // Display names cache
  std::string filename =
    gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
        prefix + "avatar_name_cache.xml");
  llofstream name_cache_stream(filename.c_str());
  if (name_cache_stream.is_open())
  {
    LLAvatarNameCache::exportFile(name_cache_stream);
  }
  else
  {
    llwarns << "Could not open file '" << filename << "' for writing."
      << llendl;
  }

  if (!gCacheNamep) return;

  filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
      prefix + "name.cache");
  llofstream cache_file(filename.c_str());
  if (cache_file.is_open())
  {
    gCacheNamep->exportFile(cache_file);
  }
  else
  {
    llwarns << "Could not open file '" << filename << "' for writing."
      << llendl;
  }
}

void LLAppViewer::saveExperienceCache()
{
  std::string prefix;
  if (!gIsInSecondLife)
  {
    prefix = LLGridManager::getInstance()->getGridLabel() + "_";
  }

  std::string filename;
  filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
      prefix + "experience_cache.xml");
  llinfos << "Saving: " << filename << llendl;
  llofstream cache_stream(filename.c_str());
  if (cache_stream.is_open())
  {
    LLExperienceCache::getInstance()->exportFile(cache_stream);
  }
  else
  {
    llwarns << "Could not open file '" << filename << "' for writing."
      << llendl;
  }
}

void LLAppViewer::loadExperienceCache()
{
  std::string prefix;
  if (!gIsInSecondLife)
  {
    prefix = LLGridManager::getInstance()->getGridLabel() + "_";
  }

  std::string filename;
  filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
      prefix + "experience_cache.xml");
  llifstream cache_stream(filename.c_str());
  if (cache_stream.is_open())
  {
    llinfos << "Loading: " << filename << llendl;
    LLExperienceCache::getInstance()->importFile(cache_stream);
  }
}

// Called every time the window is not doing anything. Receive packets, update
// statistics, and schedule a redisplay.
void LLAppViewer::idle(bool run_rlv_maintenance)
{
  static const std::string MainIdle = "Main:Idle";
  pingMainloopTimeout(&MainIdle);

  // Update frame timers
  static LLTimer idle_timer;

  //MK
  if (run_rlv_maintenance)
  {
    LL_FAST_TIMER(FTM_RLV);
    if (gRLenabled && LLStartUp::isLoggedIn() &&
        gViewerWindowp && !gViewerWindowp->getShowProgress())
    {
      // Do some RLV maintenance (garbage collector, etc)
      gRLInterface.idleTasks();
    }
  }
  //mk

  LLFrameTimer::updateFrameTime();
  LLFrameTimer::updateFrameCount();
  LLEventTimer::updateClass();
  LLCriticalDamp::updateInterpolants();
  LLMortician::updateClass();

  F32 dt_raw = idle_timer.getElapsedTimeAndResetF32();

  // Cap out-of-control frame times. Too low because in menus, swapping,
  // debugger, etc. Too high because idle called with no objects in view,
  // etc.
  constexpr F32 MIN_FRAME_RATE = 1.f;
  constexpr F32 MAX_FRAME_RATE = 200.f;

  F32 frame_rate_clamped = 1.f / dt_raw;
  frame_rate_clamped = llclamp(frame_rate_clamped, MIN_FRAME_RATE,
      MAX_FRAME_RATE);
  gFrameDTClamped = 1.f / frame_rate_clamped;

  // Global frame timer. Smoothly weight toward current frame
  gFPSClamped = (frame_rate_clamped + 4.f * gFPSClamped) / 5.f;

  static LLCachedControl<F32> qas(gSavedSettings, "QuitAfterSeconds");
  if (qas > 0.f)
  {
    if (gRenderStartTime.getElapsedTimeF32() > qas)
    {
      gAppViewerp->forceQuit();
    }
  }

  // Must wait until both have avatar object and mute list, so poll here.
  // Auto-accepted inventory items may require the avatar object to build a
  // correct name. Likewise, inventory offers from muted avatars require the
  // mute list to properly mute.
  static bool ims_requested = false;
  if (!ims_requested && LLMuteList::isLoaded() && isAgentAvatarValid())
  {
    ims_requested = LLIMMgr::requestOfflineMessages();
  }

  ///////////////////////////////////
  // Special case idle if still starting up

  if (!LLStartUp::isLoggedIn())
  {
    // Skip rest if idle startup returns false (essentially, no world yet)
    if (!LLStartUp::idleStartup())
    {
      return;
    }
  }

  F32 yaw = 0.f;				// radians

  if (!gDisconnected)
  {
    LL_FAST_TIMER(FTM_NETWORK);

#if LL_CURL_BUG
    // HACK: to work around libcurl bugs that sometimes cause the HTTP
    // pipeline  to return corrupted data... The idea of this hack is to
    // temporarily turn off pipelining when we detect an issue, and
    // automatically turn it back on a minute later, with fresh pipelined
    // connections, once the old ones have been closed. This call renables
    // HTTP pipelining after a delay has elasped. HB
    mAppCoreHttp.checkPipelinedTempOff();
#endif

#if 0	// Not used
    // Update spaceserver timeinfo
    gWorld.setSpaceTimeUSec(gWorld.getSpaceTimeUSec() +
        (U32)(dt_raw * SEC_TO_MICROSEC));
#endif

    //////////////////////////////////////
    // Update simulator agent state

    static LLCachedControl<bool> rotate_right(gSavedSettings,
        "RotateRight");
    if (rotate_right)
    {
      gAgent.moveYaw(-1.f);
    }

    // Handle automatic walking towards points
    gAgentPilot.updateTarget();
    gAgentPilot.autoPilot(&yaw);

    // When appropriate, update agent location to the simulator.
    constexpr F32 AFUPS = 1.f / AGENT_FORCE_UPDATES_PER_SECOND;
    constexpr F32 AUPS = 1.f / AGENT_UPDATES_PER_SECOND;
    static LLFrameTimer agent_update_timer;
    F32 agent_update_time = agent_update_timer.getElapsedTimeF32();
    F32 agent_force_upd_time = mLastAgentForceUpdate + agent_update_time;
    bool force_upd = gAgent.controlFlagsDirty() ||
      mLastAgentControlFlags != gAgent.getControlFlags() ||
      agent_force_upd_time > AFUPS;

    if (force_upd || agent_update_time > AUPS)
    {
      // Send avatar and camera info
      mLastAgentControlFlags = gAgent.getControlFlags();
      mLastAgentForceUpdate = force_upd ? 0 : agent_force_upd_time;
      send_agent_update(force_upd);
      agent_update_timer.reset();
    }

    //////////////////////////////////////
    // Manage statistics

    LLViewerStats* viewerstats = LLViewerStats::getInstance();
    viewerstats->idleUpdate();

    // Print the object debugging stats
    static LLFrameTimer object_debug_timer;
    if (object_debug_timer.getElapsedTimeF32() > 5.f)
    {
      object_debug_timer.reset();
      if (gObjectList.mNumDeadObjectUpdates)
      {
        llinfos << "Dead object updates: "
          << gObjectList.mNumDeadObjectUpdates << llendl;
        gObjectList.mNumDeadObjectUpdates = 0;
      }
      if (gObjectList.mNumUnknownUpdates)
      {
        llinfos << "Unknown object updates: "
          << gObjectList.mNumUnknownUpdates << llendl;
        gObjectList.mNumUnknownUpdates = 0;
      }
    }

    ////////////////////////////////////////////////
    // Network processing
    //
    // NOTE: Starting at this point, we may still have pointers to "dead"
    // objects floating throughout the various object lists.

    idleNameCache();
    idleNetwork();

    // Check for away from keyboard, kick idle agents.
    idleAFKCheck();

    // Update statistics for this frame
    viewerstats->updateStatistics(gFrameCount);
  }

  ////////////////////////////////////////
  // Handle the regular UI idle callbacks as well as hover callbacks

  {
    LL_FAST_TIMER(FTM_IDLE_CB);

    // Do event notifications if necessary. Yes, we may want to move this
    // elsewhere.
    gEventNotifier.update();

    gIdleCallbacks.callFunctions();
    gInventory.idleNotifyObservers();
    gAvatarTracker.idleNotifyObservers();
  }

  if (gDisconnected)
  {
    return;
  }

  gViewerWindowp->handlePerFrameHover();

  ///////////////////////////////////////
  // Agent and camera movement

  LLCoordGL current_mouse = gViewerWindowp->getCurrentMouse();

  {
    // After agent and camera moved, figure out if we need to
    // deselect objects.
    gSelectMgr.deselectAllIfTooFar();

  }

  {
    // Handle pending gesture processing
    gGestureManager.update();

    gAgent.updateAgentPosition(gFrameDTClamped, yaw, current_mouse.mX,
        current_mouse.mY);
  }

  {
    // Actually "object update"
    LL_FAST_TIMER(FTM_OBJECTLIST_UPDATE);

    if (!(logoutRequestSent() && hasSavedFinalSnapshot()))
    {
      gObjectList.update();
    }
  }

  //////////////////////////////////////
  // Deletes objects...
  // Has to be done after doing idleUpdates (which can kill objects)
#if 0
  if (gAgent.notTPingFar())
#endif
  {
    LL_FAST_TIMER(FTM_CLEANUP);
    gObjectList.cleanDeadObjects();
#if LL_DEBUG && 0
    LLDrawable::cleanupDeadDrawables();
#endif
  }

  {
    LL_FAST_TIMER(FTM_AREASEARCH_UPDATE);

    // Send background requests for the area search is needed
    HBFloaterAreaSearch::idleUpdate();
  }

  // After this point, in theory we should never see a dead object in the
  // various object/drawable lists.

  //////////////////////////////////////
  // Update/send HUD effects
  //
  // At this point, HUD effects may clean up some references to dead objects.

  {
    gSelectMgr.updateEffects();
    LLHUDManager::cleanupEffects();
    LLHUDManager::sendEffects();
  }

  stop_glerror();

  ////////////////////////////////////////
  // Unpack layer data that we have received

  {
    LL_FAST_TIMER(FTM_NETWORK);
    gVLManager.unpackData();
  }

  /////////////////////////
  // Update surfaces, and surface textures as well.

  gWorld.updateVisibilities();
  {
    constexpr F32 max_region_update_time = .001f; // 1ms
    LL_FAST_TIMER(FTM_REGION_UPDATE);
    gWorld.updateRegions(max_region_update_time);
  }

  /////////////////////////
  // Update weather effects

  gWorld.updateClouds(gFrameDTClamped);

  if (!gUseNewShaders)
  {
    // Moves sun, moon, and planets:
    gSky.propagateHeavenlyBodies(gFrameDTClamped);
  }

  // Update wind vector
  LLVector3 wind_position_region;
  static LLVector3 average_wind;

  // Puts agent's local coords into wind_position:
  LLViewerRegion* regionp =
    gWorld.resolveRegionGlobal(wind_position_region,
        gAgent.getPositionGlobal());
  if (regionp)
  {
    gWindVec = regionp->mWind.getVelocity(wind_position_region);

    // Compute average wind and use to drive motion of water

    F32 cloud_density =
      regionp->mCloudLayer.getDensityRegion(wind_position_region);
    gSky.setCloudDensityAtAgent(cloud_density);

    average_wind = regionp->mWind.getAverage();
    gSky.setWind(average_wind);
  }
  else
  {
    gWindVec.set(0.f, 0.f, 0.f);
  }
  stop_glerror();

  //////////////////////////////////////
  // Sort and cull in the new renderer are moved to pipeline.cpp
  // Here, particles are updated and drawables are moved.

  {
    LL_FAST_TIMER(FTM_WORLD_UPDATE);

    // Do not loose time to balance the object cache at every frame: only
    // do it once every 100 frames.
    gPipeline.updateMove(gBalanceObjectCache && gFrameCount % 100 == 0 &&
        // Do not perform the following during TPs to
        // avoid race conditions that cause crashes.
        !gAgent.notTPingFar());

    gViewerPartSim.updateSimulation();
  }
  stop_glerror();

  if (LLViewerJoystick::getInstance()->getOverrideCamera())
  {
    LLViewerJoystick::getInstance()->moveFlycam();
  }
  else
  {
    if (LLToolMgr::getInstance()->inBuildMode())
    {
      LLViewerJoystick::getInstance()->moveObjects();
    }

    gAgent.updateCamera();
  }

  // Update media focus
  LLViewerMediaFocus::getInstance()->update();

  // Objects and camera should be in sync, do LOD calculations now
  {
    LL_FAST_TIMER(FTM_LOD_UPDATE);
    gObjectList.updateApparentAngles();
  }

  {
    LL_FAST_TIMER(FTM_AUDIO_UPDATE);

    if (gAudiop)
    {
      audio_update_volume(false);
      audio_update_listener();
      audio_update_wind(false);

      // This line actually commits the changes we have made to source
      // positions, etc.
      constexpr F32 max_audio_decode_time = 0.002f; // 2 ms decode time
      gAudiop->idle(max_audio_decode_time);
    }
  }

  // Handle shutdown process, for example, wait for floaters to close, send
  // quit message, forcibly quit if it has taken too long
  if (mQuitRequested)
  {
    idleShutdown();
  }
}

void LLAppViewer::idleShutdown()
{
  // Wait for all modal alerts to get resolved
  if (LLModalDialog::activeCount() > 0)
  {
    return;
  }

  // Close IM interface
  if (gIMMgrp)
  {
    gIMMgrp->disconnectAllSessions();
  }

  // Wait for all floaters to get resolved
  if (gFloaterViewp && !gFloaterViewp->allChildrenClosed())
  {
    return;
  }

  static bool saved_snapshot = false;
  if (!saved_snapshot)
  {
    saved_snapshot = true;
    saveFinalSnapshot();
    return;
  }

  constexpr F32 SHUTDOWN_UPLOAD_SAVE_TIME = 5.f;

  S32 uploads = gAssetStoragep ? gAssetStoragep->getNumPendingUploads() : 0;
  if (uploads > 0 &&
      gLogoutTimer.getElapsedTimeF32() < SHUTDOWN_UPLOAD_SAVE_TIME &&
      !logoutRequestSent())
  {
    static S32 total_uploads = 0;
    // Sometimes total upload count can change during logout.
    total_uploads = llmax(total_uploads, uploads);
    gViewerWindowp->setShowProgress(true);
    S32 finished_uploads = total_uploads - uploads;
    F32 percent = 100.f * finished_uploads / total_uploads;
    gViewerWindowp->setProgressPercent(percent);
    gViewerWindowp->setProgressString("Saving final data...");
    return;
  }

  // All floaters are closed. Tell server we want to quit.
  if (!logoutRequestSent())
  {
    sendLogoutRequest();

    // Wait for a LogoutReply message
    gViewerWindowp->setShowProgress(true);
    gViewerWindowp->setProgressPercent(100.f);
    gViewerWindowp->setProgressString("Logging out...");
    return;
  }

  // Make sure that we quit if we have not received a reply from the server.
  if (logoutRequestSent() &&
      gLogoutTimer.getElapsedTimeF32() > gLogoutMaxTime)
  {
    forceQuit();
  }
}

void LLAppViewer::sendLogoutRequest()
{
  LLMessageSystem* msg = gMessageSystemp;
  if (msg && !mLogoutRequestSent)
  {
    mLogoutRequestSent = true;

    msg->newMessageFast(_PREHASH_LogoutRequest);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
    msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
    gAgent.sendReliableMessage();

    gLogoutTimer.reset();
    gLogoutMaxTime = LOGOUT_REQUEST_TIME;

    if (LLVoiceClient::sInitDone)
    {
      gVoiceClient.leaveChannel();
    }

    // Set internal status variables and marker files
    gLogoutInProgress = true;
    mLogoutMarkerFileName =
      gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
          LOGOUT_MARKER_FILE_NAME);

    LLFile outfile(mLogoutMarkerFileName, "w");
    mOwnsLogoutMarkerFile = bool(outfile);
    if (mOwnsLogoutMarkerFile)
    {
      stampMarkerFile(&outfile);
      llinfos << "Created logout marker file " << mLogoutMarkerFileName
        << llendl;
    }
    else
    {
      llwarns << "Cannot create logout marker file "
        << mLogoutMarkerFileName << llendl;
    }
  }
}

void LLAppViewer::idleNameCache()
{
  // Neither old nor new name cache can function before agent has a region
  LLViewerRegion* regionp = gAgent.getRegion();
  if (!regionp || !gCacheNamep) return;

  // Deal with any queued name requests and replies.
  gCacheNamep->processPending();

  // Cannot run the new cache until we have the list of capabilities
  // for the agent region, and can therefore decide whether to use
  // display names or fall back to the old name system.
  if (!regionp->capabilitiesReceived()) return;

  LLAvatarNameCache::idle();
}

//
// Handle messages, and all message related stuff
//

#define TIME_THROTTLE_MESSAGES

#ifdef TIME_THROTTLE_MESSAGES
#define CHECK_MESSAGES_DEFAULT_MAX_TIME .020f // 50 ms = 50 fps (just for messages!)
static F32 CheckMessagesMaxTime = CHECK_MESSAGES_DEFAULT_MAX_TIME;
#endif

void LLAppViewer::idleNetwork()
{
  if (gDisconnected)
  {
    return;
  }

  static const std::string idleNetwork = "idleNetwork";
  pingMainloopTimeout(&idleNetwork);

  // Disable the next queued simulator, if any.
  LLWorld::idleDisableQueuedSim();

  gObjectList.mNumNewObjects = 0;
  S32 total_decoded = 0;

  static LLCachedControl<bool> speed_test(gSavedSettings, "SpeedTest");
  if (!speed_test)
  {
    LL_FAST_TIMER(FTM_IDLE_NETWORK); // Decode

    LLTimer check_message_timer;
    // Read all available packets from network
    const S64 frame_count = gFrameCount;  // U32->S64
    F32 total_time = 0.f;
    {	// Scope-guard for LockMessageChecker
#if LL_USE_FIBER_AWARE_MUTEX
      LockMessageChecker lmc(gMessageSystemp);
      while (lmc.checkAllMessages(frame_count, gServicePumpIOp))
#else
        while (gMessageSystemp->checkAllMessages(frame_count,
              gServicePumpIOp))
#endif
        {
          if (sDoDisconnect)
          {
            // We are disconnecting, do not process any more messages
            // from the server. We are usually disconnecting due to
            // either network corruption or a server going down, so
            // this is OK.
            break;
          }

          ++gPacketsIn;

          if (++total_decoded > MESSAGE_MAX_PER_FRAME)
          {
            break;
          }

#ifdef TIME_THROTTLE_MESSAGES
          // Prevent slow packets from completely destroying the frame
          // rate. This usually happens due to clumps of avatars taking
          // huge amount of network processing time (which needs to be
          // fixed, but this is a good limit anyway).
          total_time = check_message_timer.getElapsedTimeF32();
          if (total_time >= CheckMessagesMaxTime)
          {
            break;
          }
#endif
        }

      // Handle per-frame message system processing.
      static LLCachedControl<F32> collect_time(gSavedSettings,
          "AckCollectTime");
#if LL_USE_FIBER_AWARE_MUTEX
      lmc.processAcks(collect_time);
#else
      gMessageSystemp->processAcks(collect_time);
#endif
    }	// End of scope for LockMessageChecker

#ifdef TIME_THROTTLE_MESSAGES
    if (total_time >= CheckMessagesMaxTime)
    {
      // Increase CheckMessagesMaxTime so that we will eventually catch
      // up. 3.5% ~= x2 in 20 frames, ~8x in 60 frames
      CheckMessagesMaxTime *= 1.035f;
    }
    else
    {
      // Reset CheckMessagesMaxTime to default value
      CheckMessagesMaxTime = CHECK_MESSAGES_DEFAULT_MAX_TIME;
    }
#endif

    // We want to clear the control after sending out all necessary agent
    // updates
    gAgent.resetControlFlags();

    // Decode enqueued messages...
    S32 remaining_possible_decodes = MESSAGE_MAX_PER_FRAME - total_decoded;

    if (remaining_possible_decodes <= 0)
    {
      llinfos << "Maxed out number of messages per frame at "
        << MESSAGE_MAX_PER_FRAME << llendl;
    }
  }
  gObjectList.mNumNewObjectsStat.addValue(gObjectList.mNumNewObjects);

  // Retransmit unacknowledged packets.
  if (gXferManagerp)
  {
    gXferManagerp->retransmitUnackedPackets();
  }
  if (gAssetStoragep)
  {
    gAssetStoragep->checkForTimeouts();
  }
  gViewerThrottle.updateDynamicThrottle();

  // Check that the circuit between the viewer and the agent's current
  // region is still alive
  LLViewerRegion* agent_region = gAgent.getRegion();
  if (agent_region && LLStartUp::isLoggedIn())
  {
    const LLUUID& this_region_id = agent_region->getRegionID();
    bool this_region_alive = agent_region->isAlive();
    if (mAgentRegionLastAlive && !this_region_alive &&	// newly dead
        mAgentRegionLastID == this_region_id)			// same region
    {
      forceDisconnect(LLTrans::getString("AgentLostConnection"));
    }
    mAgentRegionLastID = this_region_id;
    mAgentRegionLastAlive = this_region_alive;
  }
}

void LLAppViewer::disconnectViewer()
{
  if (gDisconnected)
  {
    return;
  }

  // Cleanup after quitting.

  // Save snapshot for next time, if we made it through initialization

  llinfos << "Disconnecting viewer !" << llendl;

  // Dump our frame statistics

  // Dump memory the statistics
  LLMemory::logMemoryInfo();

  // Remember if we were flying
  gSavedSettings.setBool("FlyingAtExit", gAgent.getFlying());

  // Un-minimize all windows so they do not get saved minimized
  if (gFloaterViewp)
  {
    gFloaterViewp->restoreAll();

    std::list<LLFloater*> floaters_to_close;
    std::string name;
    for (LLView::child_list_const_iter_t
        it = gFloaterViewp->getChildList()->begin();
        it != gFloaterViewp->getChildList()->end(); ++it)
    {
      LLView* viewp = *it;
      if (!viewp) continue;	// Paranoia

      LLFloater* floaterp = viewp->asFloater();
      if (floaterp)
      {
        // The following names are defined in the XUI files:
        //	floater_image_preview.xml
        //	floater_sound_preview.xml
        //	floater_animation_preview.xml
        name = floaterp->getName();
        if (name == "Image Preview"	|| name == "Sound Preview" ||
            name == "Animation Preview")
        {
          floaters_to_close.push_back(floaterp);
        }
      }
    }

    while (!floaters_to_close.empty())
    {
      LLFloater* floaterp = floaters_to_close.front();
      floaters_to_close.pop_front();
      floaterp->close();
    }
  }

  gSelectMgr.deselectAll();

  // Save inventory if appropriate
  if (gInventory.isInventoryUsable())	// Paranoia
  {
    gInventory.cache(gInventory.getRootFolderID(), gAgentID);
    gInventory.cache(gInventory.getLibraryRootFolderID(),
        gInventory.getLibraryOwnerID());
  }

  saveNameCache();

  if (LLExperienceCache::instanceExists())
  {
    saveExperienceCache();
    LLExperienceCache::getInstance()->cleanup();
  }

  if (LLCoprocedureManager::instanceExists())
  {
    LLCoprocedureManager::getInstance()->cleanup();
  }

  // Close all inventory floaters
  LLFloaterInventory::cleanup();

  // Also writes cached agent settings to gSavedSettings
  gAgent.cleanup();

  // Make gWorld cleanly shut down.
  gWorld.cleanupClass();

  LLVOCache::deleteSingleton();

  // Call all self-registered classes
  llinfos << "Firing LLDestroyClassList callbacks..." << llendl;
  LLDestroyClassList::getInstance()->fireCallbacks();

  delete gXferManagerp;
  gXferManagerp = NULL;
  llinfos << "Transfer manager destroyed." << llendl;

  LLDiskCache::shutdown();

  gDisconnected = true;
}

void LLAppViewer::initMainloopTimeout(const std::string* state, F32 secs)
{
  if (!mMainloopTimeout)
  {
    mMainloopTimeout = new LLWatchdogTimeout();
    resumeMainloopTimeout(state, secs);
  }
}

void LLAppViewer::destroyMainloopTimeout()
{
  if (mMainloopTimeout)
  {
    delete mMainloopTimeout;
    mMainloopTimeout = NULL;
  }
}

void LLAppViewer::resumeMainloopTimeout(const std::string* state, F32 secs)
{
  if (mMainloopTimeout)
  {
    if (secs < 0.f)
    {
      secs = mMainloopTimeoutDefault;
    }

    mMainloopTimeout->setTimeout(secs);
    mMainloopTimeout->start(state);
  }
}

void LLAppViewer::pauseMainloopTimeout()
{
  if (mMainloopTimeout)
  {
    mMainloopTimeout->stop();
  }
}

void LLAppViewer::pingMainloopTimeout(const std::string* state, F32 secs)
{
  if (mMainloopTimeout)
  {
    if (secs < 0.f)
    {
      secs = mMainloopTimeoutDefault;
    }

    mMainloopTimeout->setTimeout(secs);
    mMainloopTimeout->ping(state);
  }
}

void LLAppViewer::handleLoginComplete()
{
  static const std::string MainloopInit = "Mainloop Init";
  initMainloopTimeout(&MainloopInit);

  // Store some data to DebugInfo in case of a freeze.
  gDebugInfo["ClientInfo"]["Name"] =
    gSavedSettings.getString("VersionChannelName");
  gDebugInfo["ClientInfo"]["MajorVersion"] = LL_VERSION_MAJOR;
  gDebugInfo["ClientInfo"]["MinorVersion"] = LL_VERSION_MINOR;
  gDebugInfo["ClientInfo"]["PatchVersion"] = LL_VERSION_BRANCH;
  gDebugInfo["ClientInfo"]["BuildVersion"] = LL_VERSION_RELEASE;

  gDebugInfo["GridName"] = LLGridManager::getInstance()->getGridLabel();

  LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
  if (parcel && parcel->getMusicURL()[0])
  {
    gDebugInfo["ParcelMusicURL"] = parcel->getMusicURL();
  }
  if (parcel && !parcel->getMediaURL().empty())
  {
    gDebugInfo["ParcelMediaURL"] = parcel->getMediaURL();
  }

  gDebugInfo["SettingsFilename"] =
    gSavedSettings.getString("ClientSettingsFile");
  gDebugInfo["CRTFilename"] = gDirUtilp->getCRTFile();
  gDebugInfo["ViewerExePath"] = gDirUtilp->getExecutablePathAndName();
  gDebugInfo["CurrentPath"] = gDirUtilp->getCurPath();

  LLViewerRegion* regionp = gAgent.getRegion();
  if (regionp)
  {
    gDebugInfo["CurrentSimHost"] = regionp->getSimHostName();
    gDebugInfo["CurrentRegion"] = regionp->getName();
  }

  if (gAppViewerp->mMainloopTimeout)
  {
    gDebugInfo["MainloopTimeoutState"] = *mMainloopTimeout->getState();
  }
  writeDebugInfo(false);

  mSavePerAccountSettings = true;
}

//static
void LLAppViewer::pauseTextureFetch()
{
  gTextureFetchp->pause();
  gImageDecodeThreadp->pause();
  gTextureCachep->pause();
}

//static
void LLAppViewer::updateTextureFetch()
{
  gTextureCachep->update(1.f);		// Un-pauses the texture cache thread
  gImageDecodeThreadp->update(1.f);	// Un-pauses the image thread
  gTextureFetchp->update(1.f);		// Un-pauses the texture fetch thread
  gTextureList.updateImages(0.1f);
}

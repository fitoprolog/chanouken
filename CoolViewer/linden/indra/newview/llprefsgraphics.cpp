/**
 * @file llprefsgraphics.cpp
 * @brief Graphics preferences for the preferences floater
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include <regex>

#include "llprefsgraphics.h"

#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "lllocale.h"
#include "llnotifications.h"
#include "llpanel.h"
#include "llradiogroup.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llstartup.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llwindow.h"

#include "llfeaturemanager.h"
#include "llfloaterpreference.h"
#include "llpanellogin.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llsky.h"
#include "llstartup.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"		// For send_agent_update()
#include "llviewerobjectlist.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvosky.h"
#include "llvotree.h"
#include "llvovolume.h"
#include "llworld.h"

class LLPrefsGraphicsImpl final : public LLPanel
{
	friend class LLPreferenceCore;

public:
	LLPrefsGraphicsImpl();
	~LLPrefsGraphicsImpl() override;

	bool postBuild() override;

	void draw() override;

	void refresh() override;	// Refresh enable/disable

	void apply();				// Apply the changed values.
	void applyResolution();
	void applyWindowSize();
	void cancel();

private:
	void setCloudsMaxAlt();
	void initWindowSizeControls();
	bool extractSizeFromString(const std::string& instr, U32& width,
							   U32& height);

	void refreshEnabledState();

	static void onTabChanged(void* user_data, bool);

	// When the quality radio buttons are changed
	static void onChangeQuality(LLUICtrl* ctrl, void* data);
	// When the custom settings box is clicked
	static void onChangeCustom(LLUICtrl* ctrl, void* data);

	static void onOpenHelp(void* data);
	static void onCommitAutoDetectAspect(LLUICtrl* ctrl, void* data);
	static void onKeystrokeAspectRatio(LLLineEditor* caller, void* user_data);
	static void onSelectAspectRatio(LLUICtrl*, void*);
	static void onCommitWindowedMode(LLUICtrl* ctrl, void* data);
	static void onApplyResolution(LLUICtrl* ctrl, void* data);
	static void updateSliderText(LLUICtrl* ctrl, void* user_data);
	static void updateMeterText(LLUICtrl* ctrl, void* user_data);
	static void onClassicClouds(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckBox(LLUICtrl*, void* user_data);
	static void onCommitOcclusion(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckBoxAvatarPhysics(LLUICtrl* ctrl, void* user_data);
	static void onCommitMaxNonImpostors(LLUICtrl* ctrl, void* user_data);

	// Callback for defaults
	static void setHardwareDefaults(void* user_data);

	// Helper method
	static void fractionFromDecimal(F32 decimal_val, S32& numerator,
									S32& denominator);

private:
	typedef boost::signals2::connection connection_t;
	connection_t	mCommitSignal;

	LLTabContainer*	mTabContainer;

	// Aspect ratio sliders and boxes
	LLComboBox*		mCtrlFullScreen;		// Fullscreen resolution
	LLCheckBoxCtrl*	mCtrlAutoDetectAspect;	// Auto-detect aspect ratio
	LLComboBox*		mCtrlAspectRatio;		// User provided aspect ratio

	LLCheckBoxCtrl*	mCtrlWindowed;			// Windowed mode
	LLComboBox*		mCtrlWindowSize;		// Window size for windowed mode
	LLCheckBoxCtrl*	mCtrlBenchmark;			// Benchmark GPU

	/// Performance radio group
	LLSliderCtrl*	mCtrlSliderQuality;

	// Performance sliders and boxes
	LLSliderCtrl*	mCtrlDrawDistance;		// Draw distance slider
	LLSliderCtrl*	mCtrlLODFactor;			// LOD for volume objects
	LLSliderCtrl*	mCtrlFlexFactor;		// Timeslice for flexible objects
	LLSliderCtrl*	mCtrlTreeFactor;		// Control tree cutoff distance
	LLSliderCtrl*	mCtrlAvatarFactor;		// LOD for avatars
	LLSliderCtrl*	mCtrlTerrainFactor;		// LOD for terrain
	LLSliderCtrl*	mCtrlSkyFactor;			// LOD for terrain
	LLSliderCtrl*	mCtrlMaxParticle;		// Max Particle
	LLSliderCtrl*	mCtrlPostProcess;		// Max Particle

	LLCheckBoxCtrl*	mCtrlBumpShiny;
	LLCheckBoxCtrl*	mCtrlTransparentWater;
	LLCheckBoxCtrl*	mCtrlReflections;
	LLCheckBoxCtrl*	mCtrlWindLight;
	LLCheckBoxCtrl*	mCtrlGlow;
	LLCheckBoxCtrl*	mCtrlAvatarVP;
	LLCheckBoxCtrl*	mCtrlLightingDetail;
	LLCheckBoxCtrl*	mCtrlDeferredEnable;
	LLCheckBoxCtrl*	mCtrlAvatarCloth;
	LLCheckBoxCtrl*	mCtrlClassicClouds;

	LLSpinCtrl*		mRenderGlowStrength;
	LLSpinCtrl*		mSpinCloudsAltitude;

	LLComboBox*		mComboReflectionDetail;
	LLComboBox*		mComboRenderShadowDetail;

	LLTextBox*		mAspectRatioLabel1;
	LLTextBox*		mDisplayResLabel;
	LLTextBox*		mFullScreenInfo;
	LLTextBox*		mWindowSizeLabel;

	LLTextBox*		mDrawDistanceMeterText1;
	LLTextBox*		mDrawDistanceMeterText2;

	LLTextBox*		mLODFactorText;
	LLTextBox*		mFlexFactorText;
	LLTextBox*		mTreeFactorText;
	LLTextBox*		mAvatarFactorText;
	LLTextBox*		mTerrainFactorText;
	LLTextBox*		mSkyFactorText;
	LLTextBox*		mPostProcessText;
	LLTextBox*		mClassicCloudsText;

	// GPU features sub-tab
	LLCheckBoxCtrl*	mCtrlOcclusion;
	LLComboBox*		mCtrlWaterOcclusion;

	// Avatar rendering sub-tab
	LLSliderCtrl*	mCtrlMaxNonImpostors;
	LLSliderCtrl*	mCtrlMaximumComplexity;
	LLSliderCtrl*	mCtrlSurfaceAreaLimit;
	LLSliderCtrl*	mCtrlGeometryBytesLimit;

	F32				mAspectRatio;

	// Performance value holders for cancel
	S32				mQualityPerformance;

	S32				mReflectionDetail;
	S32				mAvatarMode;
	S32				mClassicCloudsAvgAlt;
	S32				mLightingDetail;
	S32				mTerrainDetail;
	S32				mRenderShadowDetail;
	F32				mRenderFarClip;
	F32				mPrimLOD;
	F32				mMeshLODBoost;
	F32				mFlexLOD;
	F32				mTreeLOD;
	F32				mAvatarLOD;
	F32				mTerrainLOD;
	S32				mSkyLOD;
	S32				mParticleCount;
	S32				mPostProcess;
	F32				mFogRatio;
	F32				mGlowStrength;
	bool			mGlow;

	// Renderer settings sub-tab
	U32				mFSAASamples;
	F32				mGamma;
	S32				mVideoCardMem;
	U32				mRenderCompressThreshold;

	bool			mFirstRun;

	bool			mFSAutoDetectAspect;
#if !LL_LINUX	// Irrelevant setting for Linux
	bool			mRenderHiDPI;
#endif
	bool			mBumpShiny;
	bool			mRenderDeferred;
	bool			mWindLight;
	bool			mReflections;
	bool			mAvatarVP;
	bool			mAvatarCloth;
	bool			mUseNewShaders;
	bool			mUseClassicClouds;
	bool			mRenderTransparentWater;
	bool			mCanDoObjectBump;
	bool			mCanDoReflections;
	bool			mCanDoWindlight;
	bool			mCanDoSkinning;
	bool			mCanDoCloth;
	bool			mCanDoDeferred;

	// GPU features sub-tab
	U32 			mRenderWaterCull;
	bool			mUseVBO;
	bool			mUseStreamVBO;
	bool			mUseAniso;
	bool			mCompressTextures;

	// Avatars rendering sub-tab
	bool			mAvatarPhysics;
	bool			mAlwaysRenderFriends;
	bool			mColoredJellyDolls;
	U32				mNonImpostors;
	U32				mNonImpostorsPuppets;
	U32				mRenderAvatarMaxComplexity;
	F32				mRenderAutoMuteSurfaceAreaLimit;
	U32				mRenderAutoMuteMemoryLimit;
	F32				mRenderAvatarPhysicsLODFactor;
};

constexpr S32 ASPECT_RATIO_STR_LEN = 100;

LLPrefsGraphicsImpl::LLPrefsGraphicsImpl()
:	mFirstRun(true)
{
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_preferences_graphics.xml");
}

bool LLPrefsGraphicsImpl::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("graphics_tabs");
	LLPanel* tab = mTabContainer->getChild<LLPanel>("Renderer settings");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("GPU features");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("Avatars rendering");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);

	// Setup graphic card capabilities
	bool cubemap = gGLManager.mHasCubeMap && LLCubeMap::sUseCubeMaps;
	mCanDoObjectBump = cubemap &&
					   gFeatureManager.isFeatureAvailable("RenderObjectBump");
	mCanDoWindlight =
		gFeatureManager.isFeatureAvailable("WindLightUseAtmosShaders");
	mCanDoReflections =
		cubemap &&
		gFeatureManager.isFeatureAvailable("RenderWaterReflections");
	mCanDoSkinning = gFeatureManager.isFeatureAvailable("RenderAvatarVP");
	mCanDoCloth = mCanDoSkinning &&
				  gFeatureManager.isFeatureAvailable("RenderAvatarCloth");
	mCanDoDeferred = mCanDoObjectBump && mCanDoWindlight && mCanDoSkinning &&
					 gGLManager.mHasFramebufferObject &&
					 gFeatureManager.isFeatureAvailable("RenderDeferred");

	// Return to default values
	childSetAction("Defaults", setHardwareDefaults, this);

	// Help button
	childSetAction("GraphicsPreferencesHelpButton", onOpenHelp, this);

	// Resolution

	// Radio set for fullscreen size
	mCtrlWindowed = getChild<LLCheckBoxCtrl>("windowed mode");
	mCtrlWindowed->setCommitCallback(onCommitWindowedMode);
	mCtrlWindowed->setCallbackUserData(this);

	mAspectRatioLabel1 = getChild<LLTextBox>("AspectRatioLabel1");
	mFullScreenInfo = getChild<LLTextBox>("FullScreenInfo");
	mDisplayResLabel = getChild<LLTextBox>("DisplayResLabel");

	S32 num_resolutions = 0;
	LLWindow::LLWindowResolution* resolutions =
		gWindowp->getSupportedResolutions(num_resolutions);

	S32 fullscreen_mode = num_resolutions - 1;

	mCtrlFullScreen = getChild<LLComboBox>("fullscreen combo");

	LLUIString resolution_label = getString("resolution_format");

	for (S32 i = 0; i < num_resolutions; ++i)
	{
		resolution_label.setArg("[RES_X]",
								llformat("%d", resolutions[i].mWidth));
		resolution_label.setArg("[RES_Y]",
								llformat("%d", resolutions[i].mHeight));
		mCtrlFullScreen->add(resolution_label, ADD_BOTTOM);
	}

	bool full_screen;
	S32 width, height;
	gViewerWindowp->getTargetWindow(full_screen, width, height);
	if (full_screen)
	{
		fullscreen_mode = 0; // Fefaults to 800x600
		for (S32 i = 0; i < num_resolutions; ++i)
		{
			if (width == resolutions[i].mWidth &&
				height == resolutions[i].mHeight)
			{
				fullscreen_mode = i;
			}
		}
		mCtrlFullScreen->setCurrentByIndex(fullscreen_mode);
		mCtrlWindowed->set(false);
		mCtrlFullScreen->setVisible(true);
	}
	else
	{
		// Set to windowed mode
		mCtrlWindowed->set(true);
		mCtrlFullScreen->setCurrentByIndex(0);
		mCtrlFullScreen->setVisible(false);
	}

	initWindowSizeControls();

	if (gSavedSettings.getBool("FullScreenAutoDetectAspectRatio"))
	{
		mAspectRatio = gViewerWindowp->getDisplayAspectRatio();
	}
	else
	{
		mAspectRatio = gSavedSettings.getF32("FullScreenAspectRatio");
	}

	S32 numerator = 0;
	S32 denominator = 0;
	fractionFromDecimal(mAspectRatio, numerator, denominator);

	LLUIString aspect_ratio_text = getString("aspect_ratio_text");
	if (numerator != 0)
	{
		aspect_ratio_text.setArg("[NUM]", llformat("%d",  numerator));
		aspect_ratio_text.setArg("[DEN]", llformat("%d",  denominator));
	}
	else
	{
		aspect_ratio_text = llformat("%.3f", mAspectRatio);
	}

	mCtrlAspectRatio = getChild<LLComboBox>("aspect_ratio");
	mCtrlAspectRatio->setTextEntryCallback(onKeystrokeAspectRatio);
	mCtrlAspectRatio->setCommitCallback(onSelectAspectRatio);
	mCtrlAspectRatio->setCallbackUserData(this);
	// Add default aspect ratios
	mCtrlAspectRatio->add(aspect_ratio_text, &mAspectRatio, ADD_TOP);
	mCtrlAspectRatio->setCurrentByIndex(0);

	mCtrlAutoDetectAspect = getChild<LLCheckBoxCtrl>("aspect_auto_detect");
	mCtrlAutoDetectAspect->setCommitCallback(onCommitAutoDetectAspect);
	mCtrlAutoDetectAspect->setCallbackUserData(this);

#if LL_LINUX
	// HiDPI (Retina) mode for macOS or UI scaling under Windows 10. Irrelevant
	// to Linux.
	childHide("hi_dpi_check");
#endif

	// Radio performance box
	mCtrlSliderQuality = getChild<LLSliderCtrl>("QualityPerformanceSelection");
	mCtrlSliderQuality->setSliderMouseUpCallback(onChangeQuality);
	mCtrlSliderQuality->setCallbackUserData(this);

	mCtrlBenchmark = getChild<LLCheckBoxCtrl>("benchmark_gpu_check");

	// Bump and shiny
	mCtrlBumpShiny = getChild<LLCheckBoxCtrl>("BumpShiny");
	mCtrlBumpShiny->setCommitCallback(onCommitCheckBox);
	mCtrlBumpShiny->setCallbackUserData(this);

	// WindLight
	mCtrlWindLight = getChild<LLCheckBoxCtrl>("WindLightUseAtmosShaders");
	mCtrlWindLight->setCommitCallback(onCommitCheckBox);
	mCtrlWindLight->setCallbackUserData(this);

	// Glow
	mCtrlGlow = getChild<LLCheckBoxCtrl>("RenderGlowCheck");
	mCtrlGlow->setCommitCallback(onCommitCheckBox);
	mCtrlGlow->setCallbackUserData(this);
	mRenderGlowStrength = getChild<LLSpinCtrl>("glow_strength");

	// Water opacity or reflections
	mCtrlTransparentWater = getChild<LLCheckBoxCtrl>("TransparentWaterCheck");
	mCtrlTransparentWater->setCommitCallback(onCommitCheckBox);
	mCtrlTransparentWater->setCallbackUserData(this);
	mCtrlReflections = getChild<LLCheckBoxCtrl>("Reflections");
	mCtrlReflections->setCommitCallback(onCommitCheckBox);
	mCtrlReflections->setCallbackUserData(this);
	mComboReflectionDetail = getChild<LLComboBox>("ReflectionDetailCombo");
	mComboReflectionDetail->setCurrentByIndex(gSavedSettings.getS32("RenderReflectionDetail"));

	// Check box for lighting detail
	mCtrlLightingDetail = getChild<LLCheckBoxCtrl>("LightingDetailCheck");

	// Avatar shaders
	mCtrlAvatarVP = getChild<LLCheckBoxCtrl>("AvatarVertexProgram");
	mCtrlAvatarVP->setCommitCallback(onCommitCheckBox);
	mCtrlAvatarVP->setCallbackUserData(this);
	mCtrlAvatarCloth = getChild<LLCheckBoxCtrl>("AvatarCloth");

	// Deferred rendering
	mCtrlDeferredEnable = getChild<LLCheckBoxCtrl>("RenderDeferred");
	mCtrlDeferredEnable->setCommitCallback(onCommitCheckBox);
	mCtrlDeferredEnable->setCallbackUserData(this);

	mComboRenderShadowDetail = getChild<LLComboBox>("RenderShadowDetailCombo");
	mComboRenderShadowDetail->setCurrentByIndex(gSavedSettings.getS32("RenderShadowDetail"));

	// Object detail slider
	mCtrlDrawDistance = getChild<LLSliderCtrl>("DrawDistance");
	mDrawDistanceMeterText1 = getChild<LLTextBox>("DrawDistanceMeterText1");
	mDrawDistanceMeterText2 = getChild<LLTextBox>("DrawDistanceMeterText2");
	mCtrlDrawDistance->setCommitCallback(updateMeterText);
	mCtrlDrawDistance->setCallbackUserData(this);
	updateMeterText(mCtrlDrawDistance, (void*)this);

	// Object detail slider
	mCtrlLODFactor = getChild<LLSliderCtrl>("ObjectMeshDetail");
	mLODFactorText = getChild<LLTextBox>("ObjectMeshDetailText");
	mCtrlLODFactor->setCommitCallback(updateSliderText);
	mCtrlLODFactor->setCallbackUserData(mLODFactorText);

	// Flex object detail slider
	mCtrlFlexFactor = getChild<LLSliderCtrl>("FlexibleMeshDetail");
	mFlexFactorText = getChild<LLTextBox>("FlexibleMeshDetailText");
	mCtrlFlexFactor->setCommitCallback(updateSliderText);
	mCtrlFlexFactor->setCallbackUserData(mFlexFactorText);

	// Tree detail slider
	mCtrlTreeFactor = getChild<LLSliderCtrl>("TreeMeshDetail");
	mTreeFactorText = getChild<LLTextBox>("TreeMeshDetailText");
	mCtrlTreeFactor->setCommitCallback(updateSliderText);
	mCtrlTreeFactor->setCallbackUserData(mTreeFactorText);

	// Avatar detail slider
	mCtrlAvatarFactor = getChild<LLSliderCtrl>("AvatarMeshDetail");
	mAvatarFactorText = getChild<LLTextBox>("AvatarMeshDetailText");
	mCtrlAvatarFactor->setCommitCallback(updateSliderText);
	mCtrlAvatarFactor->setCallbackUserData(mAvatarFactorText);

	// Terrain detail slider
	mCtrlTerrainFactor = getChild<LLSliderCtrl>("TerrainMeshDetail");
	mTerrainFactorText = getChild<LLTextBox>("TerrainMeshDetailText");
	mCtrlTerrainFactor->setCommitCallback(updateSliderText);
	mCtrlTerrainFactor->setCallbackUserData(mTerrainFactorText);

	// Sky detail slider
	mCtrlSkyFactor = getChild<LLSliderCtrl>("SkyMeshDetail");
	mSkyFactorText = getChild<LLTextBox>("SkyMeshDetailText");
	mCtrlSkyFactor->setCommitCallback(updateSliderText);
	mCtrlSkyFactor->setCallbackUserData(mSkyFactorText);

	// Classic clouds
	mCtrlClassicClouds = getChild<LLCheckBoxCtrl>("ClassicClouds");
	mCtrlClassicClouds->setCommitCallback(onClassicClouds);
	mCtrlClassicClouds->setCallbackUserData(this);
	mClassicCloudsText = getChild<LLTextBox>("ClassicCloudsText");
	mSpinCloudsAltitude = getChild<LLSpinCtrl>("CloudsAltitude");
	LLControlVariable* controlp =
		gSavedSettings.getControl("ClassicCloudsMaxAlt");
	if (!controlp)
	{
		llerrs << "ClassicCloudsMaxAlt debug setting is missing !" << llendl;
	}
	mCommitSignal =
		controlp->getSignal()->connect(boost::bind(&LLPrefsGraphicsImpl::setCloudsMaxAlt,
												   this));
	setCloudsMaxAlt();

	// Particle detail slider
	mCtrlMaxParticle = getChild<LLSliderCtrl>("MaxParticleCount");

	// Glow detail slider
	mCtrlPostProcess = getChild<LLSliderCtrl>("RenderPostProcess");
	mPostProcessText = getChild<LLTextBox>("PostProcessText");
	mCtrlPostProcess->setCommitCallback(updateSliderText);
	mCtrlPostProcess->setCallbackUserData(mPostProcessText);

	// GPU features sub-tab:
	childSetCommitCallback("vbo", onCommitCheckBox, this);
	mCtrlOcclusion = getChild<LLCheckBoxCtrl>("occlusion");
	mCtrlOcclusion->setCommitCallback(onCommitOcclusion);
	mCtrlOcclusion->setCallbackUserData(this);
	mCtrlWaterOcclusion = getChild<LLComboBox>("water_occlusion");
	mCtrlWaterOcclusion->setCurrentByIndex(gSavedSettings.getU32("RenderWaterCull"));
	childSetCommitCallback("texture_compression", onCommitCheckBox, this);
	childSetVisible("after_restart", LLStartUp::isLoggedIn());

	// Avatars rendering sub-tab:
	std::string off_text = getString("off_text");

	mCtrlMaxNonImpostors = getChild<LLSliderCtrl>("MaxNonImpostors");
	mCtrlMaxNonImpostors->setOffLimit(off_text, 0.f);
	mCtrlMaxNonImpostors->setCommitCallback(onCommitMaxNonImpostors);
	mCtrlMaxNonImpostors->setCallbackUserData(this);

	mCtrlMaximumComplexity = getChild<LLSliderCtrl>("MaximumComplexity");
	mCtrlMaximumComplexity->setOffLimit(off_text, 0.f);

	mCtrlSurfaceAreaLimit = getChild<LLSliderCtrl>("SurfaceAreaLimit");
	mCtrlSurfaceAreaLimit->setOffLimit(off_text, 0.f);

	mCtrlGeometryBytesLimit = getChild<LLSliderCtrl>("GeometryBytesLimit");
	mCtrlGeometryBytesLimit->setOffLimit(off_text, 0.f);

	LLSliderCtrl* slider = getChild<LLSliderCtrl>("MaxPuppetAvatars");
	slider->setOffLimit(off_text, 0.f);

	childSetCommitCallback("AvatarPhysics", onCommitCheckBoxAvatarPhysics,
						   this);

	bool show_rgba16 = gGLManager.mIsAMD && gGLManager.mGLVersion >= 4.f;
	childSetVisible("rgba16_text", show_rgba16);
	childSetVisible("rgba16_check", show_rgba16);

	refresh();

	return true;
}

void LLPrefsGraphicsImpl::initWindowSizeControls()
{
	// Window size
	mWindowSizeLabel = getChild<LLTextBox>("WindowSizeLabel");
	mCtrlWindowSize = getChild<LLComboBox>("windowsize combo");

	// Look to see if current window size matches existing window sizes, if so
	// then just set the selection value...
	const U32 height = gViewerWindowp->getWindowDisplayHeight();
	const U32 width = gViewerWindowp->getWindowDisplayWidth();
	for (S32 i = 0; i < mCtrlWindowSize->getItemCount(); ++i)
	{
		U32 height_test = 0;
		U32 width_test = 0;
		mCtrlWindowSize->setCurrentByIndex(i);
		if (extractSizeFromString(mCtrlWindowSize->getValue().asString(),
								  width_test, height_test))
		{
			if (height_test == height && width_test == width)
			{
				return;
			}
		}
	}
	// ...otherwise, add a new entry with the current window height/width.
	LLUIString resolution_label = getString("resolution_format");
	resolution_label.setArg("[RES_X]", llformat("%d", width));
	resolution_label.setArg("[RES_Y]", llformat("%d", height));
	mCtrlWindowSize->add(resolution_label, ADD_TOP);
	mCtrlWindowSize->setCurrentByIndex(0);
}

LLPrefsGraphicsImpl::~LLPrefsGraphicsImpl()
{
	mCommitSignal.disconnect();

#if 0
	// Clean up user data
	for (S32 i = 0; i < mCtrlAspectRatio->getItemCount(); ++i)
	{
		mCtrlAspectRatio->setCurrentByIndex(i);
	}
	for (S32 i = 0; i < mCtrlWindowSize->getItemCount(); ++i)
	{
		mCtrlWindowSize->setCurrentByIndex(i);
	}
#endif
}

void LLPrefsGraphicsImpl::setCloudsMaxAlt()
{
	F32 max_alt = gSavedSettings.getU32("ClassicCloudsMaxAlt");
	mSpinCloudsAltitude->setMinValue(-max_alt);
	mSpinCloudsAltitude->setMaxValue(max_alt);
}

void LLPrefsGraphicsImpl::draw()
{
	if (mFirstRun)
	{
		mFirstRun = false;
		mTabContainer->selectTab(gSavedSettings.getS32("LastGraphicsPrefTab"));
	}
	LLPanel::draw();
}

void LLPrefsGraphicsImpl::refresh()
{
	mFSAutoDetectAspect =
		gSavedSettings.getBool("FullScreenAutoDetectAspectRatio");
#if !LL_LINUX	// Irrelevant setting for Linux
	mRenderHiDPI = gSavedSettings.getBool("RenderHiDPI");
#endif

	mQualityPerformance = gSavedSettings.getU32("RenderQualityPerformance");

	F32 bw = gFeatureManager.getGPUMemoryBandwidth();
	if (bw > 0.f)
	{
		mCtrlBenchmark->setToolTip(getString("tool_tip_bench"));
		mCtrlBenchmark->setToolTipArg("[BW]", llformat("%d", (S32)bw));
	}

	// Shaders settings
	mBumpShiny = mCanDoObjectBump &&
				 gSavedSettings.getBool("RenderObjectBump");
	mWindLight = mCanDoWindlight &&
				 gSavedSettings.getBool("WindLightUseAtmosShaders");
	mReflections = mCanDoReflections &&
				   gSavedSettings.getBool("RenderWaterReflections");
	mAvatarVP = mCanDoSkinning && gSavedSettings.getBool("RenderAvatarVP");
	mAvatarCloth = mCanDoCloth && gSavedSettings.getBool("RenderAvatarCloth");
	mFogRatio = gSavedSettings.getF32("RenderFogRatio");
	mUseNewShaders = gSavedSettings.getBool("UseNewShaders");

	// Water
	mReflectionDetail = gSavedSettings.getS32("RenderReflectionDetail");
	mRenderTransparentWater = gSavedSettings.getBool("RenderTransparentWater");

	// Draw distance
	mRenderFarClip = gSavedSettings.getF32("RenderFarClip");

	// Sliders and their text boxes
	mPrimLOD = gSavedSettings.getF32("RenderVolumeLODFactor");
	mMeshLODBoost = gSavedSettings.getF32("MeshLODBoostFactor");
	mFlexLOD = gSavedSettings.getF32("RenderFlexTimeFactor");
	mTreeLOD = gSavedSettings.getF32("RenderTreeLODFactor");
	mAvatarLOD = gSavedSettings.getF32("RenderAvatarLODFactor");
	mTerrainLOD = gSavedSettings.getF32("RenderTerrainLODFactor");
	mSkyLOD = gSavedSettings.getU32("WLSkyDetail");
	mParticleCount = gSavedSettings.getS32("RenderMaxPartCount");
	mPostProcess = gSavedSettings.getU32("RenderGlowResolutionPow");

	// Classic clouds
	mUseClassicClouds = gSavedSettings.getBool("SkyUseClassicClouds");
	mClassicCloudsAvgAlt = gSavedSettings.getS32("ClassicCloudsAvgAlt");

	// Lighting and terrain radios
	mGlow = gSavedSettings.getBool("RenderGlow");
	mGlowStrength = gSavedSettings.getF32("RenderGlowStrength");
	mLightingDetail = gSavedSettings.getS32("RenderLightingDetail");
	mRenderDeferred = gSavedSettings.getBool("RenderDeferred");
	mRenderShadowDetail = gSavedSettings.getS32("RenderShadowDetail");
	mCtrlDeferredEnable->setValue(mRenderDeferred);
	mTerrainDetail = gSavedSettings.getS32("RenderTerrainDetail");

	// Slider text boxes
	updateSliderText(mCtrlLODFactor, mLODFactorText);
	updateSliderText(mCtrlFlexFactor, mFlexFactorText);
	updateSliderText(mCtrlTreeFactor, mTreeFactorText);
	updateSliderText(mCtrlAvatarFactor, mAvatarFactorText);
	updateSliderText(mCtrlTerrainFactor, mTerrainFactorText);
	updateSliderText(mCtrlPostProcess, mPostProcessText);
	updateSliderText(mCtrlSkyFactor, mSkyFactorText);

	// GPU features sub-tab
	mUseVBO = gSavedSettings.getBool("RenderVBOEnable");
	mUseStreamVBO = gSavedSettings.getBool("RenderUseStreamVBO");
	mUseAniso = gSavedSettings.getBool("RenderAnisotropic");
	mRenderWaterCull = gSavedSettings.getU32("RenderWaterCull");
	mFSAASamples = gSavedSettings.getU32("RenderFSAASamples");
	mGamma = gSavedSettings.getF32("DisplayGamma");
	mVideoCardMem = gSavedSettings.getS32("TextureMemory");
	mCompressTextures = gSavedSettings.getBool("RenderCompressTextures");
	mRenderCompressThreshold =
		gSavedSettings.getU32("RenderCompressThreshold");
	childSetValue("fsaa", (LLSD::Integer)mFSAASamples);

	// Avatars rendering sub-tab
	mNonImpostors = gSavedSettings.getU32("RenderAvatarMaxNonImpostors");
	mNonImpostorsPuppets = gSavedSettings.getU32("RenderAvatarMaxPuppets");
	mAvatarPhysics = gSavedSettings.getBool("AvatarPhysics");
	mAlwaysRenderFriends = gSavedSettings.getBool("AlwaysRenderFriends");
	mColoredJellyDolls = gSavedSettings.getBool("ColoredJellyDolls");
	mRenderAvatarPhysicsLODFactor =
		gSavedSettings.getF32("RenderAvatarPhysicsLODFactor");
	mRenderAvatarMaxComplexity =
		gSavedSettings.getU32("RenderAvatarMaxComplexity");
	mRenderAutoMuteSurfaceAreaLimit =
		gSavedSettings.getF32("RenderAutoMuteSurfaceAreaLimit");
	mRenderAutoMuteMemoryLimit =
		gSavedSettings.getU32("RenderAutoMuteMemoryLimit");

	refreshEnabledState();

//MK
	// If unable to change Windlight settings, make sure the Windlight shaders
	// check box is ticked and disabled
	if (gRLenabled && gRLInterface.mContainsSetenv)
	{
		mWindLight = true;
		gSavedSettings.setBool("WindLightUseAtmosShaders", mWindLight);
		mCtrlWindLight->setValue(mWindLight);
		mCtrlWindLight->setEnabled(false);
	}
//mk
}

void LLPrefsGraphicsImpl::refreshEnabledState()
{
	// Windowed/full-screen modes UI elements visibility
	bool is_full_screen = !mCtrlWindowed->get();
	mDisplayResLabel->setVisible(is_full_screen);
	mCtrlFullScreen->setVisible(is_full_screen);
	mCtrlAspectRatio->setVisible(is_full_screen);
	mAspectRatioLabel1->setVisible(is_full_screen);
	mCtrlAutoDetectAspect->setVisible(is_full_screen);
	mCtrlWindowSize->setVisible(!is_full_screen);
	mFullScreenInfo->setVisible(!is_full_screen);
	mWindowSizeLabel->setVisible(!is_full_screen);

	// Bump & shiny
	if (!mCanDoObjectBump)
	{
		// Turn off bump & shiny
		mCtrlBumpShiny->setEnabled(false);
		mCtrlBumpShiny->setValue(false);
	}
	bool bumps = mCtrlBumpShiny->get();

	// Glow
	if (LLPipeline::sCanRenderGlow || !LLStartUp::isLoggedIn())
	{
		mCtrlGlow->setEnabled(true);
		mRenderGlowStrength->setEnabled(mCtrlGlow->get());
	}
	else
	{
		mCtrlGlow->setEnabled(false);
		mRenderGlowStrength->setEnabled(false);
	}

	// Reflections
	bool reflections = mCanDoReflections && mCtrlTransparentWater->get();
	mCtrlReflections->setEnabled(reflections);
	if (!reflections)
	{
		mCtrlReflections->setValue(false);
	}
	else
	{
		reflections = mCtrlReflections->get();
	}
	// Reflection details
	mComboReflectionDetail->setEnabled(reflections);

	bool windlight = mCanDoWindlight;
	mCtrlWindLight->setEnabled(windlight);
	if (!windlight)
	{
		mCtrlWindLight->setValue(false);
	}
	else
	{
		windlight = mCtrlWindLight->get();
	}

	// Sky detail depend on Windlight Atmospheric shaders
	mCtrlSkyFactor->setEnabled(windlight);
	mSkyFactorText->setEnabled(windlight);

	// Classic clouds
	bool clouds = mCtrlClassicClouds->get();
	mSpinCloudsAltitude->setEnabled(clouds);

	// Avatar Mode
	bool skinning = mCanDoSkinning &&
					(!LLStartUp::isLoggedIn() ||
					 gViewerShaderMgrp->mMaxAvatarShaderLevel > 0);
	mCtrlAvatarVP->setEnabled(skinning);
	if (!skinning)
	{
		mCtrlAvatarVP->setValue(false);
	}
	else
	{
		skinning = mCtrlAvatarVP->get();
	}

	bool cloth = mCanDoCloth && skinning;
	// Avatar cloth
	mCtrlAvatarCloth->setEnabled(cloth);
	if (!cloth)
	{
		mCtrlAvatarCloth->setValue(false);
	}

	// Deferred rendering
	bool deferred = mCanDoDeferred && bumps && windlight && skinning;
	mCtrlDeferredEnable->setEnabled(deferred);
	if (!deferred)
	{
		mCtrlDeferredEnable->setValue(false);
	}
	mComboRenderShadowDetail->setEnabled(mCtrlDeferredEnable->get());

	// Visibility of settings depending on Windlight shaders
	mCtrlDeferredEnable->setVisible(windlight);
	childSetVisible("fog", !windlight);
	deferred &= mCtrlDeferredEnable->get();
	mComboRenderShadowDetail->setVisible(deferred);
	childSetVisible("no_alm_text", !deferred);
	childSetToolTip("no_alm_text",
					windlight ? getString("tool_tip_no_deferred")
							  : getString("tool_tip_no_windilght"));

	// GPU features sub-tab
	S32 min_tex_mem = LLViewerTextureList::getMinVideoRamSetting();
	S32 max_tex_mem = LLViewerTextureList::getMaxVideoRamSetting(true);
	childSetMinValue("GrapicsCardTextureMemory", min_tex_mem);
	childSetMaxValue("GrapicsCardTextureMemory", max_tex_mem);

	bool vbo_ok = true;
	if (!gFeatureManager.isFeatureAvailable("RenderVBOEnable") ||
		!gGLManager.mHasVertexBufferObject)
	{
		childSetEnabled("vbo", false);
		vbo_ok = false;
		gSavedSettings.setBool("RenderVBOEnable", false);
		gSavedSettings.setBool("RenderUseStreamVBO", false);
	}
	childSetVisible("stream_vbo", vbo_ok && childGetValue("vbo"));
	childSetEnabled("stream_vbo",
					gFeatureManager.isFeatureAvailable("RenderUseStreamVBO"));

	if (!gFeatureManager.isFeatureAvailable("RenderCompressTextures") ||
		!gGLManager.mHasVertexBufferObject)
	{
		childSetEnabled("texture_compression", false);
	}

	if (!gFeatureManager.isFeatureAvailable("UseOcclusion"))
	{
		mCtrlOcclusion->setEnabled(false);
		mCtrlWaterOcclusion->setEnabled(false);
	}

	// Texture compression settings.
	bool compress = gSavedSettings.getBool("RenderCompressTextures");
	childSetEnabled("compress_throttle", compress);
	childSetEnabled("pixels_text", compress);

	// Avatars rendering sub-tab
	bool impostors = mCtrlMaxNonImpostors->getValue().asInteger() > 0;
	mCtrlMaximumComplexity->setEnabled(impostors);
	mCtrlSurfaceAreaLimit->setEnabled(impostors);
	mCtrlGeometryBytesLimit->setEnabled(impostors);
	childSetEnabled("AvatarPhysicsLOD",
					gSavedSettings.getBool("AvatarPhysics"));
}

void LLPrefsGraphicsImpl::cancel()
{
	gSavedSettings.setBool("FullScreenAutoDetectAspectRatio", mFSAutoDetectAspect);
#if !LL_LINUX	// Irrelevant setting for Linux
	gSavedSettings.setBool("RenderHiDPI", mRenderHiDPI);
#endif
	gSavedSettings.setF32("FullScreenAspectRatio", mAspectRatio);

	gSavedSettings.setU32("RenderQualityPerformance", mQualityPerformance);

	gSavedSettings.setBool("RenderObjectBump", mBumpShiny);
	gSavedSettings.setBool("WindLightUseAtmosShaders", mWindLight);
	gSavedSettings.setBool("RenderWaterReflections", mReflections);
	gSavedSettings.setBool("RenderAvatarVP", mAvatarVP);
	gSavedSettings.setBool("RenderAvatarCloth", mAvatarCloth);

	gSavedSettings.setS32("RenderReflectionDetail", mReflectionDetail);
	gSavedSettings.setBool("RenderTransparentWater", mRenderTransparentWater);

	gSavedSettings.setBool("SkyUseClassicClouds", mUseClassicClouds);
	gSavedSettings.setS32("ClassicCloudsAvgAlt", mClassicCloudsAvgAlt);

	gSavedSettings.setBool("RenderDeferred", mRenderDeferred);
	gSavedSettings.setS32("RenderShadowDetail", mRenderShadowDetail);
	gSavedSettings.setBool("RenderGlow", mGlow);
	gSavedSettings.setF32("RenderGlowStrength", mGlowStrength);
	gSavedSettings.setS32("RenderLightingDetail", mLightingDetail);
	gSavedSettings.setF32("RenderFogRatio", mFogRatio);
	gSavedSettings.setBool("UseNewShaders", mUseNewShaders);

	gSavedSettings.setS32("RenderTerrainDetail", mTerrainDetail);

	gSavedSettings.setF32("RenderFarClip", mRenderFarClip);
	gSavedSettings.setF32("RenderVolumeLODFactor", mPrimLOD);
	gSavedSettings.setF32("MeshLODBoostFactor", mMeshLODBoost);
	gSavedSettings.setF32("RenderFlexTimeFactor", mFlexLOD);
	gSavedSettings.setF32("RenderTreeLODFactor", mTreeLOD);
	gSavedSettings.setF32("RenderAvatarLODFactor", mAvatarLOD);
	gSavedSettings.setF32("RenderTerrainLODFactor", mTerrainLOD);
	gSavedSettings.setU32("WLSkyDetail", mSkyLOD);
	gSavedSettings.setS32("RenderMaxPartCount", mParticleCount);
	gSavedSettings.setU32("RenderGlowResolutionPow", mPostProcess);

	// GPU features sub-tab
	gSavedSettings.setBool("RenderVBOEnable", mUseVBO);
	gSavedSettings.setBool("RenderUseStreamVBO", mUseStreamVBO);
	gSavedSettings.setBool("RenderAnisotropic", mUseAniso);
	gSavedSettings.setU32("RenderWaterCull", mRenderWaterCull);
	gSavedSettings.setU32("RenderFSAASamples", mFSAASamples);
	gSavedSettings.setF32("DisplayGamma", mGamma);
	gSavedSettings.setS32("TextureMemory", mVideoCardMem);
	gSavedSettings.setBool("RenderCompressTextures", mCompressTextures);
	gSavedSettings.setU32("RenderCompressThreshold", mRenderCompressThreshold);

	// Avatars rendering sub-tab
	gSavedSettings.setU32("RenderAvatarMaxNonImpostors", mNonImpostors);
	gSavedSettings.setU32("RenderAvatarMaxPuppets", mNonImpostorsPuppets);
	gSavedSettings.setBool("AvatarPhysics", mAvatarPhysics);
	gSavedSettings.setBool("AlwaysRenderFriends", mAlwaysRenderFriends);
	gSavedSettings.setBool("ColoredJellyDolls", mColoredJellyDolls);
	gSavedSettings.setF32("RenderAvatarPhysicsLODFactor", mRenderAvatarPhysicsLODFactor);
	gSavedSettings.setU32("RenderAvatarMaxComplexity", mRenderAvatarMaxComplexity);
	gSavedSettings.setF32("RenderAutoMuteSurfaceAreaLimit", mRenderAutoMuteSurfaceAreaLimit);
	gSavedSettings.setU32("RenderAutoMuteMemoryLimit", mRenderAutoMuteMemoryLimit);
}

void LLPrefsGraphicsImpl::apply()
{
	applyResolution();

	// Only set window size if we are not in fullscreen mode
	if (mCtrlWindowed->get())
	{
		applyWindowSize();
	}
}

void LLPrefsGraphicsImpl::onChangeQuality(LLUICtrl* ctrl, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	LLSliderCtrl* sldr = (LLSliderCtrl*)ctrl;
	if (self && sldr)
	{
		U32 set = (U32)sldr->getValueF32();
		gFeatureManager.setGraphicsLevel(set, true);

		self->refreshEnabledState();
		self->refresh();
		self->applyResolution();
	}
}

void LLPrefsGraphicsImpl::applyResolution()
{
	gGL.flush();
	char aspect_ratio_text[ASPECT_RATIO_STR_LEN];
	if (mCtrlAspectRatio->getCurrentIndex() == -1)
	{
		// Cannot pass const char* from c_str() into strtok
		strncpy(aspect_ratio_text, mCtrlAspectRatio->getSimple().c_str(),
				sizeof(aspect_ratio_text) -1);
		aspect_ratio_text[sizeof(aspect_ratio_text) -1] = '\0';
		char* element = strtok(aspect_ratio_text, ":/\\");
		if (!element)
		{
			mAspectRatio = 0.f; // Will be clamped later
		}
		else
		{
			LLLocale locale(LLLocale::USER_LOCALE);
			mAspectRatio = (F32)atof(element);
		}

		// Look for denominator
		element = strtok(NULL, ":/\\");
		if (element)
		{
			LLLocale locale(LLLocale::USER_LOCALE);
			F32 denominator = (F32)atof(element);
			if (denominator > 0.f)
			{
				mAspectRatio /= denominator;
			}
		}
	}
	else
	{
		mAspectRatio = (F32)mCtrlAspectRatio->getValue().asReal();
	}

	// Presumably, user entered a non-numeric value if mAspectRatio == 0.f
	if (mAspectRatio != 0.f)
	{
		mAspectRatio = llclamp(mAspectRatio, 0.2f, 5.f);
		gSavedSettings.setF32("FullScreenAspectRatio", mAspectRatio);
	}

	// Screen resolution
	S32 num_resolutions;
	LLWindow::LLWindowResolution* resolutions =
		gWindowp->getSupportedResolutions(num_resolutions);
	U32 res_idx = mCtrlFullScreen->getCurrentIndex();
	gSavedSettings.setS32("FullScreenWidth", resolutions[res_idx].mWidth);
	gSavedSettings.setS32("FullScreenHeight", resolutions[res_idx].mHeight);

	gViewerWindowp->requestResolutionUpdate(!mCtrlWindowed->get());

	send_agent_update(true);

	bool change_display = false;
	bool restart_display = false;
	bool after_restart = false;
	bool logged_in = LLStartUp::isLoggedIn();

	// GPU features sub-tab
	if (gSavedSettings.getU32("RenderFSAASamples") != mFSAASamples)
	{
		after_restart = true;
	}

	if (gSavedSettings.getBool("RenderAnisotropic") != mUseAniso)
	{
		restart_display = true;
	}

#if !LL_LINUX	// Irrelevant setting for Linux
	if (gSavedSettings.getBool("RenderHiDPI") != mRenderHiDPI)
	{
		after_restart = true;
	}
#endif

	if (change_display || restart_display)
	{
		if (change_display)
		{
			LLCoordScreen size;
			gWindowp->getSize(&size);
			bool disable_vsync = gSavedSettings.getBool("DisableVerticalSync");
			gViewerWindowp->changeDisplaySettings(gWindowp->getFullscreen(),
												  size, disable_vsync,
												  logged_in);
		}
		else
		{
			gViewerWindowp->restartDisplay(logged_in);
		}
		if (!logged_in && LLPanelLogin::getInstance())
		{
			// Force a refresh of the fonts and GL images
			LLPanelLogin::getInstance()->refresh();
		}
	}

	if (after_restart)
	{
		gNotifications.add("InEffectAfterRestart");
	}

	// Update enable/disable
	refresh();
}

// Extract from strings of the form "<width> x <height>", e.g. "640 x 480".
bool LLPrefsGraphicsImpl::extractSizeFromString(const std::string& instr,
												U32& width, U32& height)
{
	try
	{
		std::cmatch what;
		const std::regex expression("([0-9]+) x ([0-9]+)");
		if (std::regex_match(instr.c_str(), what, expression))
		{
			width = atoi(what[1].first);
			height = atoi(what[2].first);
			return true;
		}
	}
	catch (std::regex_error& e)
	{
		llwarns << "Regex error: " << e.what() << llendl;
	}

	width = height = 0;
	return false;
}

void LLPrefsGraphicsImpl::applyWindowSize()
{
	if (!mCtrlWindowSize->getVisible() ||
		mCtrlWindowSize->getCurrentIndex() == -1)
	{
		return;
	}
	U32 width = 0;
	U32 height = 0;
	if (extractSizeFromString(mCtrlWindowSize->getValue().asString().c_str(),
							  width, height))
	{
		LLViewerWindow::movieSize(width, height);
	}
}

//static
void LLPrefsGraphicsImpl::onTabChanged(void* user_data, bool)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)user_data;
	if (self && self->mTabContainer)
	{
		gSavedSettings.setS32("LastGraphicsPrefTab",
							  self->mTabContainer->getCurrentPanelIndex());
	}
}

//static
void LLPrefsGraphicsImpl::onOpenHelp(void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (!self) return;

	LLFloater* parentp = gFloaterViewp->getParentFloater(self);
	if (!parentp) return;

	gNotifications.add(parentp->contextualNotification("GraphicsPreferencesHelp"));
}

//static
void LLPrefsGraphicsImpl::onApplyResolution(LLUICtrl* ctrl, void*)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)ctrl;
	if (self)
	{
		self->applyResolution();
	}
}

//static
void LLPrefsGraphicsImpl::onCommitWindowedMode(LLUICtrl*, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (self)
	{
		self->refresh();
	}
}

//static
void LLPrefsGraphicsImpl::onCommitAutoDetectAspect(LLUICtrl* ctrl, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (!self || !ctrl) return;

	bool auto_detect = ((LLCheckBoxCtrl*)ctrl)->get();
	if (auto_detect)
	{
		S32 numerator = 0;
		S32 denominator = 0;

		// Clear any aspect ratio override
		gWindowp->setNativeAspectRatio(0.f);
		fractionFromDecimal(gWindowp->getNativeAspectRatio(), numerator,
							denominator);
		std::string aspect;
		if (numerator != 0)
		{
			aspect = llformat("%d:%d", numerator, denominator);
		}
		else
		{
			aspect = llformat("%.3f", gWindowp->getNativeAspectRatio());
		}

		self->mCtrlAspectRatio->setLabel(aspect);

		F32 ratio = gWindowp->getNativeAspectRatio();
		gSavedSettings.setF32("FullScreenAspectRatio", ratio);
	}
}

//static
void LLPrefsGraphicsImpl::onKeystrokeAspectRatio(LLLineEditor*, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (self)
	{
		self->mCtrlAutoDetectAspect->set(false);
	}
}

//static
void LLPrefsGraphicsImpl::onSelectAspectRatio(LLUICtrl*, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	if (self)
	{
		self->mCtrlAutoDetectAspect->set(false);
	}
}

//static
void LLPrefsGraphicsImpl::fractionFromDecimal(F32 decimal_val, S32& numerator,
										 S32& denominator)
{
	numerator = 0;
	denominator = 0;
	for (F32 test_denominator = 1.f; test_denominator < 30.f;
		 test_denominator += 1.f)
	{
		if (fmodf((decimal_val * test_denominator) + 0.01f, 1.f) < 0.02f)
		{
			numerator = ll_round(decimal_val * test_denominator);
			denominator = ll_round(test_denominator);
			break;
		}
	}
}

//static
void LLPrefsGraphicsImpl::onCommitCheckBox(LLUICtrl*, void* user_data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)user_data;
	if (self)
	{
		self->refreshEnabledState();
	}
}

//static
void LLPrefsGraphicsImpl::onCommitCheckBoxAvatarPhysics(LLUICtrl* ctrl,
														void* user_data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	bool enabled = check->get();
	self->childSetEnabled("AvatarPhysicsLOD", enabled);
}

//static
void LLPrefsGraphicsImpl::onCommitOcclusion(LLUICtrl* ctrl, void* user_data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	bool enabled = check->get();
	self->mCtrlWaterOcclusion->setEnabled(enabled);
}

//static
void LLPrefsGraphicsImpl::onCommitMaxNonImpostors(LLUICtrl* ctrl,
												  void* user_data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)user_data;
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	if (!self || !slider) return;

	bool enabled = slider->getValue().asInteger() > 0;
	self->mCtrlMaximumComplexity->setEnabled(enabled);
	self->mCtrlSurfaceAreaLimit->setEnabled(enabled);
	self->mCtrlGeometryBytesLimit->setEnabled(enabled);
}

void LLPrefsGraphicsImpl::setHardwareDefaults(void* user_data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)user_data;
	if (self)
	{
		gFeatureManager.applyRecommendedSettings();
		self->refreshEnabledState();
		self->refresh();
	}
}

void LLPrefsGraphicsImpl::updateSliderText(LLUICtrl* ctrl, void* data)
{
	// Get our UI widgets
	LLTextBox* text_box = (LLTextBox*)data;
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	if (!text_box || !slider)
	{
		return;
	}

	// Get range and points when text should change
	F32 range = slider->getMaxValue() - slider->getMinValue();
	llassert(range > 0);
	F32 midPoint = slider->getMinValue() + range / 3.f;
	F32 highPoint = slider->getMinValue() + (2.f / 3.f) * range;

	// Choose the right text
	if (slider->getValueF32() < midPoint)
	{
		text_box->setText("Low");
	}
	else if (slider->getValueF32() < highPoint)
	{
		text_box->setText("Mid");
	}
	else
	{
		text_box->setText("High");
	}
}

void LLPrefsGraphicsImpl::updateMeterText(LLUICtrl* ctrl, void* data)
{
	// Get our UI widgets
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	if (self && ctrl)
	{
		// Toggle the two text boxes based on whether we have 1 or two digits
		F32 val = slider->getValueF32();
		bool two_digits = val < 100;
		self->mDrawDistanceMeterText1->setVisible(two_digits);
		self->mDrawDistanceMeterText2->setVisible(!two_digits);
	}
}

void LLPrefsGraphicsImpl::onClassicClouds(LLUICtrl* ctrl, void* data)
{
	LLPrefsGraphicsImpl* self = (LLPrefsGraphicsImpl*)data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		self->mSpinCloudsAltitude->setEnabled(check->get());
	}
}

//-----------------------------------------------------------------------------

LLPrefsGraphics::LLPrefsGraphics()
:	impl(*new LLPrefsGraphicsImpl())
{
}

LLPrefsGraphics::~LLPrefsGraphics()
{
	delete &impl;
}

void LLPrefsGraphics::apply()
{
	impl.apply();
}

void LLPrefsGraphics::cancel()
{
	impl.cancel();
}

LLPanel* LLPrefsGraphics::getPanel()
{
	return &impl;
}

/**
 * @file lltexlayer.cpp
 * @brief A texture layer. Used for avatars.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#include "linden_common.h"

#include "lltexlayer.h"

#include "imageids.h"
#include "llavatarappearance.h"
#include "llcrc.h"
#include "lldir.h"
#include "llimagej2c.h"
#include "llimagetga.h"
#include "llrenderutils.h"
#include "lltexlayerparams.h"
#include "lltexturemanagerbridge.h"
#include "hbtracy.h"
#include "llvertexbuffer.h"
#include "llviewervisualparam.h"
#include "llwearable.h"
#include "llwearabledata.h"

bool LLTexLayerSet::sHasCaches = false;
// In SL, face wrinkles cannot be baked any more (the SSB code was not fixed
// to support them), while in OpenSim, the viewer still bakes its own textures
// and we got the fix for face wrinkles. This variable is set in llstartup.cpp.
//static
bool LLTexLayerSet::sAllowFaceWrinkles = true;

// Global
LLTexLayerStaticImageList gTexLayerStaticImageList;

using namespace LLAvatarAppearanceDefines;

// Runway consolidate
extern std::string self_av_string();

class LLTexLayerInfo
{
	friend class LLTexLayer;
	friend class LLTexLayerTemplate;
	friend class LLTexLayerInterface;
public:
	LLTexLayerInfo();
	~LLTexLayerInfo();

	bool parseXml(LLXmlTreeNode* node);
	bool createVisualParams(LLAvatarAppearance* appearance);
	LL_INLINE bool isUserSettable()				{ return mLocalTexture != -1; }
	LL_INLINE S32 getLocalTexture() const		{ return mLocalTexture; }
	LL_INLINE bool getOnlyAlpha() const			{ return mUseLocalTextureAlphaOnly; }
	LL_INLINE std::string getName() const		{ return mName;	}

private:
	LLTexLayerInterface::ERenderPass	mRenderPass;

	std::string							mGlobalColor;
	LLColor4							mFixedColor;

	S32									mLocalTexture;
	std::string							mStaticImageFileName;
	bool								mStaticImageIsMask;

	// Don't use masking. Just write RGBA into buffer
	bool								mWriteAllChannels;

	// Ignore RGB channels from the input texture. Use alpha as a mask
	bool								mUseLocalTextureAlphaOnly;

	bool								mIsVisibilityMask;

	std::string							mName;

	typedef std::vector<std::pair<std::string, bool> > morph_name_list_t;
	morph_name_list_t					mMorphNameList;
	param_color_info_list_t				mParamColorInfoList;
	param_alpha_info_list_t				mParamAlphaInfoList;
};

//-----------------------------------------------------------------------------
// LLTexLayerSetBuffer
// The composite image that a LLViewerTexLayerSet writes to.
// Each LLViewerTexLayerSet has one.
//-----------------------------------------------------------------------------

LLTexLayerSetBuffer::LLTexLayerSetBuffer(LLTexLayerSet* const owner)
:	mTexLayerSet(owner)
{
}

void LLTexLayerSetBuffer::pushProjection() const
{
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.ortho(0.f, getCompositeWidth(), 0.f, getCompositeHeight(), -1.f, 1.f);

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();
}

void LLTexLayerSetBuffer::popProjection() const
{
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();
}

//virtual
void LLTexLayerSetBuffer::preRenderTexLayerSet()
{
	// Set up an ortho projection
	pushProjection();
}

//virtual
void LLTexLayerSetBuffer::postRenderTexLayerSet(bool success)
{
	popProjection();
}

bool LLTexLayerSetBuffer::renderTexLayerSet()
{
	// Default color mask for tex layer render
	gGL.setColorMask(true, true);

	bool success = true;

	gAlphaMaskProgram.bind();
	gAlphaMaskProgram.setMinimumAlpha(0.004f);

	LLVertexBuffer::unbind();

	// Composite the color data
	LLGLSUIDefault gls_ui;
	success &= mTexLayerSet->render(getCompositeOriginX(), getCompositeOriginY(),
									getCompositeWidth(), getCompositeHeight());
	midRenderTexLayerSet(success);

	gAlphaMaskProgram.unbind();

	LLVertexBuffer::unbind();

	// Reset GL state
	gGL.setColorMask(true, true);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	return success;
}

//-----------------------------------------------------------------------------
// LLTexLayerSetInfo
// An ordered set of texture layers that get composited into a single texture.
//-----------------------------------------------------------------------------

LLTexLayerSetInfo::LLTexLayerSetInfo()
:	mBodyRegion(""),
	mWidth(512),
	mHeight(512),
	mClearAlpha(true)
{
}

LLTexLayerSetInfo::~LLTexLayerSetInfo()
{
	std::for_each(mLayerInfoList.begin(), mLayerInfoList.end(),
				  DeletePointer());
	mLayerInfoList.clear();
}

bool LLTexLayerSetInfo::parseXml(LLXmlTreeNode* node)
{
	llassert(node->hasName("layer_set"));
	if (!node->hasName("layer_set"))
	{
		return false;
	}

	// body_region
	static LLStdStringHandle body_region_string = LLXmlTree::addAttributeString("body_region");
	if (!node->getFastAttributeString(body_region_string, mBodyRegion))
	{
		llwarns << "<layer_set> is missing body_region attribute" << llendl;
		return false;
	}

	// width, height
	static LLStdStringHandle width_string = LLXmlTree::addAttributeString("width");
	if (!node->getFastAttributeS32(width_string, mWidth))
	{
		return false;
	}

	static LLStdStringHandle height_string = LLXmlTree::addAttributeString("height");
	if (!node->getFastAttributeS32(height_string, mHeight))
	{
		return false;
	}

	// Optional alpha component to apply after all compositing is complete.
	static LLStdStringHandle alpha_tga_file_string = LLXmlTree::addAttributeString("alpha_tga_file");
	node->getFastAttributeString(alpha_tga_file_string, mStaticAlphaFileName);

	static LLStdStringHandle clear_alpha_string = LLXmlTree::addAttributeString("clear_alpha");
	node->getFastAttributeBool(clear_alpha_string, mClearAlpha);

	// <layer>
	for (LLXmlTreeNode* child = node->getChildByName("layer");
		 child; child = node->getNextNamedChild())
	{
		LLTexLayerInfo* info = new LLTexLayerInfo();
		if (!info->parseXml(child))
		{
			delete info;
			return false;
		}
		mLayerInfoList.push_back(info);
	}
	return true;
}

// creates visual params without generating layersets or layers
void LLTexLayerSetInfo::createVisualParams(LLAvatarAppearance* appearance)
{
	//layer_info_list_t		mLayerInfoList;
	for (layer_info_list_t::iterator layer_iter = mLayerInfoList.begin(),
									 end = mLayerInfoList.end();
		 layer_iter != end; ++layer_iter)
	{
		LLTexLayerInfo* layer_info = *layer_iter;
		layer_info->createVisualParams(appearance);
	}
}

//-----------------------------------------------------------------------------
// LLTexLayerSet
// An ordered set of texture layers that get composited into a single texture.
//-----------------------------------------------------------------------------

LLTexLayerSet::LLTexLayerSet(LLAvatarAppearance* const appearance)
:	mAvatarAppearance(appearance),
	mIsVisible(true),
	mBakedTexIndex(LLAvatarAppearanceDefines::BAKED_HEAD),
	mInfo(NULL)
{
}

//virtual
LLTexLayerSet::~LLTexLayerSet()
{
	deleteCaches();
	std::for_each(mLayerList.begin(), mLayerList.end(), DeletePointer());
	mLayerList.clear();
	std::for_each(mMaskLayerList.begin(), mMaskLayerList.end(),
				  DeletePointer());
	mMaskLayerList.clear();
}

bool LLTexLayerSet::setInfo(const LLTexLayerSetInfo* info)
{
	llassert(mInfo == NULL);
	mInfo = info;
	//mID = info->mID; // No ID

	mLayerList.reserve(info->mLayerInfoList.size());
	for (LLTexLayerSetInfo::layer_info_list_t::const_iterator
			iter = info->mLayerInfoList.begin(),
			end = info->mLayerInfoList.end();
		 iter != end; ++iter)
	{
		LLTexLayerInterface* layer = NULL;
		if ((*iter)->isUserSettable())
		{
			layer = new LLTexLayerTemplate(this, getAvatarAppearance());
		}
		else
		{
			layer = new LLTexLayer(this);
		}
		// This is the first time this layer (of either type) is being created
		// - make sure you add the parameters to the avatar appearance.
		if (!layer->setInfo(*iter, NULL))
		{
			mInfo = NULL;
			return false;
		}
		if (!layer->isVisibilityMask())
		{
			mLayerList.push_back(layer);
		}
		else
		{
			mMaskLayerList.push_back(layer);
		}
	}

	requestUpdate();

	return true;
}

void LLTexLayerSet::deleteCaches()
{
	for (layer_list_t::iterator iter = mLayerList.begin();
		 iter != mLayerList.end(); ++iter)
	{
		LLTexLayerInterface* layer = *iter;
		layer->deleteCaches();
	}
	for (layer_list_t::iterator iter = mMaskLayerList.begin();
		 iter != mMaskLayerList.end(); ++iter)
	{
		LLTexLayerInterface* layer = *iter;
		layer->deleteCaches();
	}
}

bool LLTexLayerSet::render(S32 x, S32 y, S32 width, S32 height)
{
	mIsVisible = true;
	if (mMaskLayerList.size() > 0)
	{
		for (layer_list_t::iterator iter = mMaskLayerList.begin(),
									end = mMaskLayerList.end();
			 iter != end; ++iter)
		{
			LLTexLayerInterface* layer = *iter;
			if (layer->isInvisibleAlphaMask())
			{
				mIsVisible = false;
			}
		}
	}

	LLGLSUIDefault gls_ui;
	LLGLDepthTest gls_depth(GL_FALSE, GL_FALSE);
	gGL.setColorMask(true, true);

	// Clear buffer area to ensure we don't pick up UI elements
	{
		LLGLDisable no_alpha(GL_ALPHA_TEST);

		gAlphaMaskProgram.setMinimumAlpha(0.f);

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f(0.f, 0.f, 0.f, 1.f);

		gl_rect_2d_simple(width, height);

		gAlphaMaskProgram.setMinimumAlpha(0.004f);
	}

	bool success = true;

	if (mIsVisible)
	{
		// Composite color layers
		for (layer_list_t::iterator iter = mLayerList.begin(),
									end = mLayerList.end();
			 iter != end; ++iter)
		{
			LLTexLayerInterface* layer = *iter;
			if (layer->getRenderPass() == LLTexLayer::RP_COLOR ||
				(sAllowFaceWrinkles &&
				 layer->getRenderPass() == LLTexLayer::RP_BUMP))
			{
				success &= layer->render(x, y, width, height);
			}
		}

		renderAlphaMaskTextures(x, y, width, height, false);
	}
	else
	{
		gGL.flush();

		gGL.setSceneBlendType(LLRender::BT_REPLACE);
		LLGLDisable no_alpha(GL_ALPHA_TEST);

		gAlphaMaskProgram.setMinimumAlpha(0.f);

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f(0.f, 0.f, 0.f, 0.f);

		gl_rect_2d_simple(width, height);
		gGL.setSceneBlendType(LLRender::BT_ALPHA);

		gAlphaMaskProgram.setMinimumAlpha(0.004f);
	}

	stop_glerror();

	return success;
}

bool LLTexLayerSet::isBodyRegion(const std::string& region) const
{
	return mInfo->mBodyRegion == region;
}

const std::string LLTexLayerSet::getBodyRegionName() const
{
	return mInfo->mBodyRegion;
}

//virtual
void LLTexLayerSet::asLLSD(LLSD& sd) const
{
	sd["visible"] = LLSD::Boolean(isVisible());
	LLSD layer_list_sd;
	for (layer_list_t::const_iterator layer_iter = mLayerList.begin(),
									  layer_end  = mLayerList.end();
		 layer_iter != layer_end; ++layer_iter)
	{
		LLSD layer_sd;
#if 0
		LLTexLayerInterface* layer = *layer_iter;
		if (layer)
		{
			layer->asLLSD(layer_sd);
		}
#endif
		layer_list_sd.append(layer_sd);
	}
	LLSD mask_list_sd;
	LLSD info_sd;
	sd["layers"] = layer_list_sd;
	sd["masks"] = mask_list_sd;
	sd["info"] = info_sd;
}

void LLTexLayerSet::destroyComposite()
{
	if (mComposite)
	{
		mComposite = NULL;
	}
}

LLTexLayerSetBuffer* LLTexLayerSet::getComposite()
{
	if (!mComposite)
	{
		createComposite();
	}
	return mComposite;
}

const LLTexLayerSetBuffer* LLTexLayerSet::getComposite() const
{
	return mComposite;
}

void LLTexLayerSet::gatherMorphMaskAlpha(U8* data, S32 origin_x, S32 origin_y,
										 S32 width, S32 height)
{
	LL_TRACY_TIMER(TRC_GATHER_MORPH_MASK_ALPHA);

	memset(data, 255, width * height);

	for (layer_list_t::iterator iter = mLayerList.begin(),
								end = mLayerList.end();
		 iter != end; ++iter)
	{
		LLTexLayerInterface* layer = *iter;
		layer->gatherAlphaMasks(data, origin_x, origin_y, width, height);
	}

	// Set alpha back to that of our alpha masks.
	renderAlphaMaskTextures(origin_x, origin_y, width, height, true);
}

void LLTexLayerSet::renderAlphaMaskTextures(S32 x, S32 y, S32 width,
											S32 height, bool forceClear)
{
	LL_TRACY_TIMER(TRC_RENDER_ALPHA_MASK_TEXTURES);

	const LLTexLayerSetInfo* info = getInfo();

	gGL.setColorMask(false, true);
	gGL.setSceneBlendType(LLRender::BT_REPLACE);

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	// (Optionally) replace alpha with a single component image from a tga file.
	if (!info->mStaticAlphaFileName.empty())
	{
		{
			LLGLTexture* tex;
			tex = gTexLayerStaticImageList.getTexture(info->mStaticAlphaFileName,
													  true);
			if (tex)
			{
				LLGLSUIDefault gls_ui;
				unit0->bind(tex);
				gl_rect_2d_simple_tex(width, height);
			}
		}
	}
	else if (forceClear || info->mClearAlpha || mMaskLayerList.size() > 0)
	{
		// Set the alpha channel to one (clean up after previous blending)
		LLGLDisable no_alpha(GL_ALPHA_TEST);

		gAlphaMaskProgram.setMinimumAlpha(0.f);

		unit0->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f(0.f, 0.f, 0.f, 1.f);

		gl_rect_2d_simple(width, height);

		gAlphaMaskProgram.setMinimumAlpha(0.004f);
	}

	// (Optional) Mask out part of the baked texture with alpha masks; will
	// still have an effect even if mClearAlpha is set or the alpha component
	// was replaced.
	if (mMaskLayerList.size() > 0)
	{
		gGL.setSceneBlendType(LLRender::BT_MULT_ALPHA);
		for (layer_list_t::iterator iter = mMaskLayerList.begin(),
									end = mMaskLayerList.end();
			 iter != end; ++iter)
		{
			LLTexLayerInterface* layer = *iter;
			layer->blendAlphaTexture(x, y, width, height);
		}

	}

	unit0->unbind(LLTexUnit::TT_TEXTURE);

	gGL.setColorMask(true, true);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

void LLTexLayerSet::applyMorphMask(U8* tex_data, S32 width, S32 height,
								   S32 num_components)
{
	mAvatarAppearance->applyMorphMask(tex_data, width, height, num_components,
									  mBakedTexIndex);
}

bool LLTexLayerSet::isMorphValid() const
{
	for (layer_list_t::const_iterator iter = mLayerList.begin(),
									  end = mLayerList.end();
		 iter != end; ++iter)
	{
		const LLTexLayerInterface* layer = *iter;
		if (layer && !layer->isMorphValid())
		{
			return false;
		}
	}
	return true;
}

void LLTexLayerSet::invalidateMorphMasks()
{
	for (layer_list_t::iterator iter = mLayerList.begin(),
								end = mLayerList.end();
		 iter != end; ++iter)
	{
		LLTexLayerInterface* layer = *iter;
		if (layer)
		{
			layer->invalidateMorphMasks();
		}
	}
}

//-----------------------------------------------------------------------------
// LLTexLayerInfo
//-----------------------------------------------------------------------------
LLTexLayerInfo::LLTexLayerInfo()
:	mWriteAllChannels(false),
	mRenderPass(LLTexLayer::RP_COLOR),
	mFixedColor(0.f, 0.f, 0.f, 0.f),
	mLocalTexture(-1),
	mStaticImageIsMask(false),
	mUseLocalTextureAlphaOnly(false),
	mIsVisibilityMask(false)
{
}

LLTexLayerInfo::~LLTexLayerInfo()
{
	std::for_each(mParamColorInfoList.begin(), mParamColorInfoList.end(),
				  DeletePointer());
	mParamColorInfoList.clear();
	std::for_each(mParamAlphaInfoList.begin(), mParamAlphaInfoList.end(),
				  DeletePointer());
	mParamAlphaInfoList.clear();
}

bool LLTexLayerInfo::parseXml(LLXmlTreeNode* node)
{
	llassert(node->hasName("layer"));

	// name attribute
	static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
	if (!node->getFastAttributeString(name_string, mName))
	{
		return false;
	}

	static LLStdStringHandle write_all_channels_string = LLXmlTree::addAttributeString("write_all_channels");
	node->getFastAttributeBool(write_all_channels_string, mWriteAllChannels);

	std::string render_pass_name;
	static LLStdStringHandle render_pass_string = LLXmlTree::addAttributeString("render_pass");
	if (node->getFastAttributeString(render_pass_string, render_pass_name))
	{
		if (render_pass_name == "bump")
		{
			mRenderPass = LLTexLayer::RP_BUMP;
		}
	}

	// Note: layers can have either a "global_color" attrib, a "fixed_color"
	// attrib, or a <param_color> child.
	// global color attribute (optional)
	static LLStdStringHandle global_color_string = LLXmlTree::addAttributeString("global_color");
	node->getFastAttributeString(global_color_string, mGlobalColor);

	// Visibility mask (optional)
	bool is_visibility;
	static LLStdStringHandle visibility_mask_string = LLXmlTree::addAttributeString("visibility_mask");
	if (node->getFastAttributeBool(visibility_mask_string, is_visibility))
	{
		mIsVisibilityMask = is_visibility;
	}

	// color attribute (optional)
	LLColor4U color4u;
	static LLStdStringHandle fixed_color_string = LLXmlTree::addAttributeString("fixed_color");
	if (node->getFastAttributeColor4U(fixed_color_string, color4u))
	{
		mFixedColor.set(color4u);
	}

		// <texture> optional sub-element
	for (LLXmlTreeNode* texture_node = node->getChildByName("texture");
		 texture_node; texture_node = node->getNextNamedChild())
	{
		std::string local_texture_name;
		static LLStdStringHandle tga_file_string = LLXmlTree::addAttributeString("tga_file");
		static LLStdStringHandle local_texture_string = LLXmlTree::addAttributeString("local_texture");
		static LLStdStringHandle file_is_mask_string = LLXmlTree::addAttributeString("file_is_mask");
		static LLStdStringHandle local_texture_alpha_only_string = LLXmlTree::addAttributeString("local_texture_alpha_only");
		if (texture_node->getFastAttributeString(tga_file_string, mStaticImageFileName))
		{
			texture_node->getFastAttributeBool(file_is_mask_string,
											   mStaticImageIsMask);
		}
		else if (texture_node->getFastAttributeString(local_texture_string,
													  local_texture_name))
		{
			texture_node->getFastAttributeBool(local_texture_alpha_only_string,
											   mUseLocalTextureAlphaOnly);

			/* if ("upper_shirt" == local_texture_name)
				mLocalTexture = TEX_UPPER_SHIRT; */
			mLocalTexture = TEX_NUM_INDICES;
			for (LLAvatarAppearanceDictionary::Textures::const_iterator
					iter = gAvatarAppDictp->getTextures().begin(),
					end = gAvatarAppDictp->getTextures().end();
				 iter != end; ++iter)
			{
				const LLAvatarAppearanceDictionary::TextureEntry* texture_dict = iter->second;
				if (local_texture_name == texture_dict->mName)
				{
					mLocalTexture = iter->first;
					break;
				}
			}
			if (mLocalTexture == TEX_NUM_INDICES)
			{
				llwarns << "<texture> element has invalid local_texture attribute: "
						<< mName << " " << local_texture_name << llendl;
				return false;
			}
		}
		else
		{
			llwarns << "<texture> element is missing a required attribute: "
					<< mName << llendl;
			return false;
		}
	}

	for (LLXmlTreeNode* maskNode = node->getChildByName("morph_mask");
		 maskNode; maskNode = node->getNextNamedChild())
	{
		std::string morph_name;
		static LLStdStringHandle morph_name_string =
			LLXmlTree::addAttributeString("morph_name");
		if (maskNode->getFastAttributeString(morph_name_string, morph_name))
		{
			bool invert = false;
			static LLStdStringHandle invert_string =
				LLXmlTree::addAttributeString("invert");
			maskNode->getFastAttributeBool(invert_string, invert);
			mMorphNameList.push_back(std::pair<std::string, bool>(morph_name,
																  invert));
		}
	}

	// <param> optional sub-element (color or alpha params)
	for (LLXmlTreeNode* child = node->getChildByName("param");
		 child; child = node->getNextNamedChild())
	{
		if (child->getChildByName("param_color"))
		{
			// <param><param_color/></param>
			LLTexLayerParamColorInfo* info = new LLTexLayerParamColorInfo();
			if (!info->parseXml(child))
			{
				delete info;
				return false;
			}
			mParamColorInfoList.push_back(info);
		}
		else if (child->getChildByName("param_alpha"))
		{
			// <param><param_alpha/></param>
			LLTexLayerParamAlphaInfo* info = new LLTexLayerParamAlphaInfo();
			if (!info->parseXml(child))
			{
				delete info;
				return false;
			}
 			mParamAlphaInfoList.push_back(info);
		}
	}

	return true;
}

bool LLTexLayerInfo::createVisualParams(LLAvatarAppearance* appearance)
{
	bool success = true;
	for (param_color_info_list_t::iterator
			color_info_iter = mParamColorInfoList.begin(),
			end = mParamColorInfoList.end();
		 color_info_iter != end; ++color_info_iter)
	{
		LLTexLayerParamColorInfo* color_info = *color_info_iter;
		LLTexLayerParamColor* param_color = new LLTexLayerParamColor(appearance);
		if (!param_color->setInfo(color_info, true))
		{
			llwarns << "NULL TexLayer Color Param could not be added to visual param list. Deleting."
					<< llendl;
			delete param_color;
			success = false;
		}
	}

	for (param_alpha_info_list_t::iterator
			alpha_info_iter = mParamAlphaInfoList.begin(),
			end = mParamAlphaInfoList.end();
		 alpha_info_iter != end; ++alpha_info_iter)
	{
		LLTexLayerParamAlphaInfo* alpha_info = *alpha_info_iter;
		LLTexLayerParamAlpha* param_alpha = new LLTexLayerParamAlpha(appearance);
		if (!param_alpha->setInfo(alpha_info, true))
		{
			llwarns << "NULL TexLayer Alpha Param could not be added to visual param list. Deleting."
					<< llendl;
			delete param_alpha;
			success = false;
		}
	}

	return success;
}

LLTexLayerInterface::LLTexLayerInterface(LLTexLayerSet* const layer_set)
:	mTexLayerSet(layer_set),
	mInfo(NULL),
	mMorphMasksValid(false),
	mHasMorph(false)
{
}

LLTexLayerInterface::LLTexLayerInterface(const LLTexLayerInterface& layer,
										 LLWearable* wearable)
:	mTexLayerSet(layer.mTexLayerSet),
	mInfo(NULL)
{
	// don't add visual params for cloned layers
	setInfo(layer.getInfo(), wearable);

	mHasMorph = layer.mHasMorph;
}

// This sets mInfo and calls initialization functions
bool LLTexLayerInterface::setInfo(const LLTexLayerInfo* info,
								  LLWearable* wearable)
{
	// setInfo should only be called once. Code is not robust enough to handle
	// redefinition of a texlayer.
	// Not a critical warning, but could be useful for debugging later issues. -Nyx
	if (mInfo)
	{
		llwarns << "mInfo != NULL" << llendl;
	}
	mInfo = info;
	//mID = info->mID; // No ID

	mParamColorList.reserve(mInfo->mParamColorInfoList.size());
	for (param_color_info_list_t::const_iterator
			iter = mInfo->mParamColorInfoList.begin(),
			end = mInfo->mParamColorInfoList.end();
		 iter != end; ++iter)
	{
		LLTexLayerParamColor* param_color;
		if (!wearable)
		{
			param_color = new LLTexLayerParamColor(this);
			if (!param_color->setInfo(*iter, true))
			{
				mInfo = NULL;
				return false;
			}
		}
		else
		{
			param_color = (LLTexLayerParamColor*)wearable->getVisualParam((*iter)->getID());
			if (!param_color)
			{
				mInfo = NULL;
				return false;
			}
		}
		mParamColorList.push_back(param_color);
	}

	mParamAlphaList.reserve(mInfo->mParamAlphaInfoList.size());
	for (param_alpha_info_list_t::const_iterator
			iter = mInfo->mParamAlphaInfoList.begin(),
			end = mInfo->mParamAlphaInfoList.end();
		 iter != end; ++iter)
	{
		LLTexLayerParamAlpha* param_alpha;
		if (!wearable)
		{
			param_alpha = new LLTexLayerParamAlpha(this);
			if (!param_alpha->setInfo(*iter, true))
			{
				mInfo = NULL;
				return false;
			}
		}
		else
		{
			param_alpha =
				(LLTexLayerParamAlpha*)wearable->getVisualParam((*iter)->getID());
			if (!param_alpha)
			{
				mInfo = NULL;
				return false;
			}
		}
		mParamAlphaList.push_back(param_alpha);
	}

	return true;
}

//virtual
void LLTexLayerInterface::requestUpdate()
{
	mTexLayerSet->requestUpdate();
}

const std::string& LLTexLayerInterface::getName() const
{
	return mInfo->mName;
}

ETextureIndex LLTexLayerInterface::getLocalTextureIndex() const
{
	return (ETextureIndex)mInfo->mLocalTexture;
}

LLWearableType::EType LLTexLayerInterface::getWearableType() const
{
	ETextureIndex te = getLocalTextureIndex();
	if (TEX_INVALID == te)
	{
		LLWearableType::EType type = LLWearableType::WT_INVALID;
		LLWearableType::EType new_type;
		for (param_color_list_t::const_iterator
				color_iter = mParamColorList.begin(),
				color_end = mParamColorList.end();
			 color_iter != color_end; ++color_iter)
		{
			LLTexLayerParamColor* param = *color_iter;
			if (param)
			{
				new_type = (LLWearableType::EType)param->getWearableType();
				if (new_type != LLWearableType::WT_INVALID && new_type != type)
				{
					if (type != LLWearableType::WT_INVALID)
					{
						return LLWearableType::WT_INVALID;
					}
					type = new_type;
				}
			}
		}

		for (param_alpha_list_t::const_iterator
				alpha_iter = mParamAlphaList.begin(),
				alpha_end = mParamAlphaList.end();
			 alpha_iter != alpha_end; alpha_iter++)
		{
			LLTexLayerParamAlpha* param = *alpha_iter;
			if (param)
			{
				new_type = (LLWearableType::EType)param->getWearableType();
				if (new_type != LLWearableType::WT_INVALID && new_type != type)
				{
					if (type != LLWearableType::WT_INVALID)
					{
						return LLWearableType::WT_INVALID;
					}
					type = new_type;
				}
			}
		}

		return type;
	}
	return LLAvatarAppearanceDictionary::getTEWearableType(te);
}

LLTexLayerInterface::ERenderPass LLTexLayerInterface::getRenderPass() const
{
	return mInfo->mRenderPass;
}

const std::string& LLTexLayerInterface::getGlobalColor() const
{
	return mInfo->mGlobalColor;
}

bool LLTexLayerInterface::isVisibilityMask() const
{
	return mInfo->mIsVisibilityMask;
}

void LLTexLayerInterface::invalidateMorphMasks()
{
	mMorphMasksValid = false;
}

LLViewerVisualParam* LLTexLayerInterface::getVisualParamPtr(S32 index) const
{
	LLViewerVisualParam *result = NULL;
	for (param_color_list_t::const_iterator color_iter = mParamColorList.begin(),
											end = mParamColorList.end();
		 color_iter != end && !result; ++color_iter)
	{
		if ((*color_iter)->getID() == index)
		{
			result = *color_iter;
		}
	}
	for (param_alpha_list_t::const_iterator alpha_iter = mParamAlphaList.begin(),
											end = mParamAlphaList.end();
		 alpha_iter != end && !result; ++alpha_iter)
	{
		if ((*alpha_iter)->getID() == index)
		{
			result = *alpha_iter;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
// LLTexLayer
// A single texture layer, consisting of:
//		* color, consisting of either
//			* one or more color parameters (weighted colors)
//			* a reference to a global color
//			* a fixed color with non-zero alpha
//			* opaque white (the default)
//		* (optional) a texture defined by either
//			* a GUID
//			* a texture entry index (TE)
//		* (optional) one or more alpha parameters (weighted alpha textures)
//-----------------------------------------------------------------------------
LLTexLayer::LLTexLayer(LLTexLayerSet* const layer_set)
:	LLTexLayerInterface(layer_set),
	mLocalTextureObject(NULL)
{
}

LLTexLayer::LLTexLayer(const LLTexLayer &layer, LLWearable* wearable)
:	LLTexLayerInterface(layer, wearable),
	mLocalTextureObject(NULL)
{
}

LLTexLayer::LLTexLayer(const LLTexLayerTemplate& layer_template,
					   LLLocalTextureObject* lto,
					   LLWearable* wearable) :
	LLTexLayerInterface(layer_template, wearable),
	mLocalTextureObject(lto)
{
}

LLTexLayer::~LLTexLayer()
{
#if 0	// mParamAlphaList and mParamColorList are LLViewerVisualParams and get
		// deleted with ~LLCharacter()

	std::for_each(mParamAlphaList.begin(), mParamAlphaList.end(),
				  DeletePointer());
	mParamAlphaList.clear();
	std::for_each(mParamColorList.begin(), mParamColorList.end(),
				  DeletePointer());
	mParamColorList.clear();
#endif

	for (alpha_cache_t::iterator iter = mAlphaCache.begin(),
								 end = mAlphaCache.end();
		 iter != end; ++iter)
	{
		U8* alpha_data = iter->second;
		delete[] alpha_data;
	}
}

void LLTexLayer::asLLSD(LLSD& sd) const
{
	// *TODO: Finish
	sd["id"] = getUUID();
}

bool LLTexLayer::setInfo(const LLTexLayerInfo* info, LLWearable* wearable)
{
	return LLTexLayerInterface::setInfo(info, wearable);
}

//static
void LLTexLayer::calculateTexLayerColor(const param_color_list_t& param_list,
										LLColor4& net_color)
{
	for (param_color_list_t::const_iterator iter = param_list.begin(),
											end = param_list.end();
		 iter != end; ++iter)
	{
		const LLTexLayerParamColor* param = *iter;
		LLColor4 param_net = param->getNetColor();
		const LLTexLayerParamColorInfo* info = (LLTexLayerParamColorInfo*)param->getInfo();
		switch (info->getOperation())
		{
			case LLTexLayerParamColor::OP_ADD:
				net_color += param_net;
				break;
			case LLTexLayerParamColor::OP_MULTIPLY:
				net_color = net_color * param_net;
				break;
			case LLTexLayerParamColor::OP_BLEND:
				net_color = lerp(net_color, param_net, param->getWeight());
				break;
			default:
				llassert(0);
				break;
		}
	}
	net_color.clamp();
}

//virtual
void LLTexLayer::deleteCaches()
{
	// Only need to delete caches for alpha params. Color params don't hold
	// extra memory
	for (param_alpha_list_t::iterator iter = mParamAlphaList.begin(),
									  end = mParamAlphaList.end();
		 iter != end; ++iter)
	{
		LLTexLayerParamAlpha* param = *iter;
		param->deleteCaches();
	}
}

bool LLTexLayer::render(S32 x, S32 y, S32 width, S32 height)
{
	LLGLEnable color_mat(GL_COLOR_MATERIAL);

	// *TODO: Is this correct?
	//gPipeline.disableLights();

	LLColor4 net_color;
	bool color_specified = findNetColor(&net_color);
	if (mTexLayerSet->getAvatarAppearance()->mIsDummy)
	{
		color_specified = true;
		net_color = LLAvatarAppearance::getDummyColor();
	}

	bool success = true;

	// If you can't see the layer, don't render it.
	if (is_approx_zero(net_color.mV[VW]))
	{
		return success;
	}

	bool alpha_mask_specified = false;
	param_alpha_list_t::const_iterator iter = mParamAlphaList.begin();
	if (iter != mParamAlphaList.end())
	{
		// If we have alpha masks, but we're skipping all of them, skip the
		// whole layer. However, we can't do this optimization if we have
		// morph masks that need updating.
#if 0
		if (!mHasMorph)
		{
			bool skip_layer = true;
			while (iter != mParamAlphaList.end())
			{
				const LLTexLayerParamAlpha* param = *iter;

				if (!param->getSkip())
				{
					skip_layer = false;
					break;
				}

				++iter;
			}
			if (skip_layer)
			{
				return success;
			}
		}
#endif

		renderMorphMasks(x, y, width, height, net_color, true);
		alpha_mask_specified = true;
		gGL.blendFunc(LLRender::BF_DEST_ALPHA,
					  LLRender::BF_ONE_MINUS_DEST_ALPHA);
	}

	bool use_alpha_only = getInfo()->mUseLocalTextureAlphaOnly;

	gGL.color4fv(net_color.mV);

	if (getInfo()->mWriteAllChannels)
	{
		gGL.setSceneBlendType(LLRender::BT_REPLACE);
	}
	else if (use_alpha_only && LLTexLayerSet::sAllowFaceWrinkles)
	{
		// Use the alpha channel only
		gGL.setColorMask(false, true);
	}

	if (getInfo()->mLocalTexture != -1 && !use_alpha_only)
	{
		{
			LLGLTexture* tex = NULL;
			if (mLocalTextureObject && mLocalTextureObject->getImage())
			{
				tex = mLocalTextureObject->getImage();
				if (mLocalTextureObject->getID() == IMG_DEFAULT_AVATAR)
				{
					tex = NULL;
				}
			}
			else
			{
				llinfos << "lto not defined or image not defined: "
						<< getInfo()->getLocalTexture() << " lto: "
						<< mLocalTextureObject << llendl;
			}
#if 0
			if (mTexLayerSet->getAvatarAppearance()->getLocalTextureGL((ETextureIndex)getInfo()->mLocalTexture,
																	   &image_gl))
#endif
			{
				if (tex)
				{
					bool no_alpha_test = getInfo()->mWriteAllChannels;
					LLGLDisable alpha_test(no_alpha_test ? GL_ALPHA_TEST : 0);
					if (no_alpha_test)
					{
						gAlphaMaskProgram.setMinimumAlpha(0.f);
					}

					LLTexUnit::eTextureAddressMode old_mode = tex->getAddressMode();

					LLTexUnit* unit0 = gGL.getTexUnit(0);
					unit0->bind(tex);
					unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);

					gl_rect_2d_simple_tex(width, height);

					unit0->setTextureAddressMode(old_mode);
					unit0->unbind(LLTexUnit::TT_TEXTURE);
					if (no_alpha_test)
					{
						gAlphaMaskProgram.setMinimumAlpha(0.004f);
					}
				}
			}
#if 0
			else
			{
				success = false;
			}
#endif
		}
	}

	if (!getInfo()->mStaticImageFileName.empty())
	{
		LLGLTexture* tex =
			gTexLayerStaticImageList.getTexture(getInfo()->mStaticImageFileName,
													getInfo()->mStaticImageIsMask);
		if (tex)
		{
			LLTexUnit* unit0 = gGL.getTexUnit(0);
			unit0->bind(tex);
			gl_rect_2d_simple_tex(width, height);
			unit0->unbind(LLTexUnit::TT_TEXTURE);
		}
		else
		{
			success = false;
		}
	}

	if (color_specified && getInfo()->mStaticImageFileName.empty() &&
		(getInfo()->mLocalTexture == -1 || use_alpha_only))
	{
		LLGLDisable no_alpha(GL_ALPHA_TEST);

		gAlphaMaskProgram.setMinimumAlpha(0.000f);

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4fv(net_color.mV);
		gl_rect_2d_simple(width, height);

		gAlphaMaskProgram.setMinimumAlpha(0.004f);
	}

	if (alpha_mask_specified || getInfo()->mWriteAllChannels)
	{
		// Restore standard blend func value
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
	}

	if (use_alpha_only && LLTexLayerSet::sAllowFaceWrinkles)
	{
		// Restore color + alpha mode.
		gGL.setColorMask(true, true);
	}

	stop_glerror();

	if (!success)
	{
		llinfos << "Partial render for: " << getInfo()->mName << llendl;
	}
	return success;
}

const U8* LLTexLayer::getAlphaData() const
{
	LLCRC alpha_mask_crc;
	const LLUUID& uuid = getUUID();
	alpha_mask_crc.update((U8*)(&uuid.mData), UUID_BYTES);

	for (param_alpha_list_t::const_iterator
			iter = mParamAlphaList.begin(),
			end = mParamAlphaList.end(); iter != end; ++iter)
	{
		const LLTexLayerParamAlpha* param = *iter;
		// MULTI-WEARABLE: verify visual parameters used here
		F32 param_weight = param->getWeight();
		alpha_mask_crc.update((U8*)&param_weight, sizeof(F32));
	}

	U32 cache_index = alpha_mask_crc.getCRC();

	alpha_cache_t::const_iterator iter2 = mAlphaCache.find(cache_index);
	return (iter2 == mAlphaCache.end()) ? 0 : iter2->second;
}

bool LLTexLayer::findNetColor(LLColor4* net_color) const
{
	// Color is either:
	//	* one or more color parameters (weighted colors, which may make use of
	//    a global color or fixed color)
	//	* a reference to a global color
	//	* a fixed color with non-zero alpha
	//	* opaque white (the default)

	if (!mParamColorList.empty())
	{
		if (!getGlobalColor().empty())
		{
			net_color->set(mTexLayerSet->getAvatarAppearance()->getGlobalColor(getInfo()->mGlobalColor));
		}
		else if (getInfo()->mFixedColor.mV[VW])
		{
			net_color->set(getInfo()->mFixedColor);
		}
		else
		{
			net_color->set(0.f, 0.f, 0.f, 0.f);
		}

		calculateTexLayerColor(mParamColorList, *net_color);
		return true;
	}

	if (!getGlobalColor().empty())
	{
		net_color->set(mTexLayerSet->getAvatarAppearance()->getGlobalColor(getGlobalColor()));
		return true;
	}

	if (getInfo()->mFixedColor.mV[VW])
	{
		net_color->set(getInfo()->mFixedColor);
		return true;
	}

	net_color->setToWhite();

	return false; // No need to draw a separate colored polygon
}

bool LLTexLayer::blendAlphaTexture(S32 x, S32 y, S32 width, S32 height)
{
	bool success = true;

	if (!getInfo()->mStaticImageFileName.empty())
	{
		LLGLTexture* tex =
			gTexLayerStaticImageList.getTexture(getInfo()->mStaticImageFileName,
												getInfo()->mStaticImageIsMask);
		if (tex)
		{
			LLGLSNoAlphaTest gls_no_alpha_test;

			gAlphaMaskProgram.setMinimumAlpha(0.f);

			LLTexUnit* unit0 = gGL.getTexUnit(0);
			unit0->bind(tex);
			gl_rect_2d_simple_tex(width, height);
			unit0->unbind(LLTexUnit::TT_TEXTURE);

			gAlphaMaskProgram.setMinimumAlpha(0.004f);
		}
		else
		{
			success = false;
		}
	}
	else if (getInfo()->mLocalTexture >= 0 &&
			 getInfo()->mLocalTexture < TEX_NUM_INDICES)
	{
		LLGLTexture* tex = mLocalTextureObject->getImage();
		if (tex)
		{
			LLGLSNoAlphaTest gls_no_alpha_test;

			gAlphaMaskProgram.setMinimumAlpha(0.f);

			LLTexUnit* unit0 = gGL.getTexUnit(0);
			unit0->bind(tex);
			gl_rect_2d_simple_tex(width, height);
			unit0->unbind(LLTexUnit::TT_TEXTURE);
			success = true;

			gAlphaMaskProgram.setMinimumAlpha(0.004f);
		}
	}

	return success;
}

//virtual
void LLTexLayer::gatherAlphaMasks(U8* data, S32 originX, S32 originY,
								  S32 width, S32 height)
{
	addAlphaMask(data, originX, originY, width, height);
}

void LLTexLayer::renderMorphMasks(S32 x, S32 y, S32 width, S32 height,
								  const LLColor4& layer_color,
								  bool force_render)
{
	LL_TRACY_TIMER(TRC_RENDER_MORPH_MASKS);

	if (!force_render && !hasMorph())
	{
		LL_DEBUGS("TexLayer") << "Skipping renderMorphMasks for " << getUUID()
							  << LL_ENDL;
		return;
	}

	bool success = true;

	llassert(!mParamAlphaList.empty());

	gAlphaMaskProgram.setMinimumAlpha(0.f);

	gGL.setColorMask(false, true);

	LLTexLayerParamAlpha* first_param = *mParamAlphaList.begin();
	// Note: if the first param is a mulitply, multiply against the current
	// buffer's alpha
	if (!first_param || !first_param->getMultiplyBlend())
	{
		LLGLDisable no_alpha(GL_ALPHA_TEST);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		// Clear the alpha
		gGL.setSceneBlendType(LLRender::BT_REPLACE);

		gGL.color4f(0.f, 0.f, 0.f, 0.f);
		gl_rect_2d_simple(width, height);
	}

	// Accumulate alphas
	LLGLSNoAlphaTest gls_no_alpha_test;
	gGL.color4f(1.f, 1.f, 1.f, 1.f);
	for (param_alpha_list_t::iterator iter = mParamAlphaList.begin(),
									  end = mParamAlphaList.end();
		 iter != end; ++iter)
	{
		LLTexLayerParamAlpha* param = *iter;
		success &= param->render(x, y, width, height);
		if (!success && !force_render)
		{
			LL_DEBUGS("TexLayer") << "Failed to render param "
								  << param->getID()
								  << ", skipping morph mask." << LL_ENDL;
			return;
		}
	}

	// Approximates a min() function
	gGL.setSceneBlendType(LLRender::BT_MULT_ALPHA);

	// Accumulate the alpha component of the texture
	if (getInfo()->mLocalTexture != -1)
	{
		LLGLTexture* tex = mLocalTextureObject->getImage();
		if (tex && tex->getComponents() == 4)
		{
			LLGLSNoAlphaTest gls_no_alpha_test;
			LLTexUnit::eTextureAddressMode old_mode = tex->getAddressMode();

			LLTexUnit* unit0 = gGL.getTexUnit(0);
			unit0->bind(tex);
			unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);

			gl_rect_2d_simple_tex(width, height);

			unit0->setTextureAddressMode(old_mode);
			unit0->unbind(LLTexUnit::TT_TEXTURE);
		}
	}

	if (!getInfo()->mStaticImageFileName.empty() &&
		getInfo()->mStaticImageIsMask)
	{
		LLGLTexture* tex;
		tex = gTexLayerStaticImageList.getTexture(getInfo()->mStaticImageFileName,
												  getInfo()->mStaticImageIsMask);
		if (tex)
		{
			if (tex->getComponents() == 4 || tex->getComponents() == 1)
			{
				LLGLSNoAlphaTest gls_no_alpha_test;
				LLTexUnit* unit0 = gGL.getTexUnit(0);
				unit0->bind(tex);
				gl_rect_2d_simple_tex(width, height);
				unit0->unbind(LLTexUnit::TT_TEXTURE);
			}
			else
			{
				llwarns << "Expected 1 or 4 components. Skipping rendering of "
						<< getInfo()->mStaticImageFileName << " that got "
						<< tex->getComponents() << " components." << llendl;
			}
		}
	}

	// Draw a rectangle with the layer color to multiply the alpha by that
	// color's alpha.
	// Note: we're still using gGL.blendFunc(GL_DST_ALPHA, GL_ZERO);
	if (!is_approx_equal(layer_color.mV[VW], 1.f))
	{
		LLGLDisable no_alpha(GL_ALPHA_TEST);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4fv(layer_color.mV);
		gl_rect_2d_simple(width, height);
	}

	gAlphaMaskProgram.setMinimumAlpha(0.004f);

	LLGLSUIDefault gls_ui;

	gGL.setColorMask(true, true);

	if (hasMorph() && success)
	{
		LLCRC alpha_mask_crc;
		const LLUUID& uuid = getUUID();
		alpha_mask_crc.update((U8*)(&uuid.mData), UUID_BYTES);

		for (param_alpha_list_t::const_iterator iter = mParamAlphaList.begin(),
												end = mParamAlphaList.end();
			 iter != end; ++iter)
		{
			const LLTexLayerParamAlpha* param = *iter;
			F32 param_weight = param->getWeight();
			alpha_mask_crc.update((U8*)&param_weight, sizeof(F32));
		}

		U32 cache_index = alpha_mask_crc.getCRC();
		U8* alpha_data = get_ptr_in_map(mAlphaCache, cache_index);
		if (!alpha_data)
		{
			// Clear out a slot if we have filled our cache
			S32 max_cache_entries =
				getTexLayerSet()->getAvatarAppearance()->isSelf() ? 4 : 1;
			while ((S32)mAlphaCache.size() >= max_cache_entries)
			{
				// Arbitrarily grab the first entry:
				alpha_cache_t::iterator iter2 = mAlphaCache.begin();
				alpha_data = iter2->second;
				delete [] alpha_data;
				mAlphaCache.hmap_erase(iter2);
			}
			alpha_data = new U8[width * height];
			if (gGLManager.mIsIntel)	// Work-around for broken Intel drivers
			{
				U8* buffer = new U8[width * height * 4];
				glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
							 buffer);
				for (U32 i = 0, count = width * height; i < count; ++i)
				{
					alpha_data[i] = buffer[i * 4 + 3];
				}
				delete[] buffer;
			}
			else	// Working drivers
			{
				glReadPixels(x, y, width, height, GL_ALPHA, GL_UNSIGNED_BYTE,
							 alpha_data);
			}
			mAlphaCache[cache_index] = alpha_data;
		}

		getTexLayerSet()->getAvatarAppearance()->dirtyMesh();

		mMorphMasksValid = true;
		getTexLayerSet()->applyMorphMask(alpha_data, width, height, 1);
	}
}

void LLTexLayer::addAlphaMask(U8* data, S32 originX, S32 originY, S32 width,
							  S32 height)
{
	LL_TRACY_TIMER(TRC_ADD_ALPHA_MASK);

	S32 size = width * height;
	const U8* alphaData = getAlphaData();
	if (!alphaData && hasAlphaParams())
	{
		LLColor4 net_color;
		findNetColor(&net_color);
		// *TODO: eliminate need for layer morph mask valid flag
		invalidateMorphMasks();
		renderMorphMasks(originX, originY, width, height, net_color, false);
		alphaData = getAlphaData();
	}
	if (alphaData)
	{
		for (S32 i = 0; i < size; ++i)
		{
			U8 curAlpha = data[i];
			U16 resultAlpha = curAlpha;
			resultAlpha *= (U16)alphaData[i] + 1;
			resultAlpha = resultAlpha >> 8;
			data[i] = (U8)resultAlpha;
		}
	}
}

//virtual
bool LLTexLayer::isInvisibleAlphaMask() const
{
	return mLocalTextureObject && mLocalTextureObject->getID() == IMG_INVISIBLE;
}

LLUUID LLTexLayer::getUUID() const
{
	LLUUID uuid;
	if (getInfo()->mLocalTexture != -1)
	{
		LLGLTexture* tex = mLocalTextureObject->getImage();
		if (tex)
		{
			uuid = mLocalTextureObject->getID();
		}
	}
	if (!getInfo()->mStaticImageFileName.empty())
	{
		LLGLTexture* tex;
		tex = gTexLayerStaticImageList.getTexture(getInfo()->mStaticImageFileName,
												  getInfo()->mStaticImageIsMask);
		if (tex)
		{
			uuid = tex->getID();
		}
	}
	return uuid;
}

//-----------------------------------------------------------------------------
// LLTexLayerTemplate
// A single texture layer, consisting of:
//		* color, consisting of either
//			* one or more color parameters (weighted colors)
//			* a reference to a global color
//			* a fixed color with non-zero alpha
//			* opaque white (the default)
//		* (optional) a texture defined by either
//			* a GUID
//			* a texture entry index (TE)
//		* (optional) one or more alpha parameters (weighted alpha textures)
//-----------------------------------------------------------------------------
LLTexLayerTemplate::LLTexLayerTemplate(LLTexLayerSet* layer_set,
									   LLAvatarAppearance* const appearance)
:	LLTexLayerInterface(layer_set),
	mAvatarAppearance(appearance)
{
}

LLTexLayerTemplate::LLTexLayerTemplate(const LLTexLayerTemplate& layer)
:	LLTexLayerInterface(layer),
	mAvatarAppearance(layer.getAvatarAppearance())
{
}

//virtual
bool LLTexLayerTemplate::setInfo(const LLTexLayerInfo* info,
								 LLWearable* wearable)
{
	return LLTexLayerInterface::setInfo(info, wearable);
}

U32 LLTexLayerTemplate::updateWearableCache() const
{
	mWearableCache.clear();

	LLWearableType::EType wearable_type = getWearableType();
	if (LLWearableType::WT_INVALID == wearable_type)
	{
		// this isn't a cloneable layer
		return 0;
	}
	U32 num_wearables = getAvatarAppearance()->getWearableData()->getWearableCount(wearable_type);
	U32 added = 0;
	for (U32 i = 0; i < num_wearables; ++i)
	{
		LLWearable* wearable;
		wearable = getAvatarAppearance()->getWearableData()->getWearable(wearable_type,
																		 i);
		if (!wearable)
		{
			continue;
		}
		mWearableCache.push_back(wearable);
		++added;
	}
	return added;
}

LLTexLayer* LLTexLayerTemplate::getLayer(U32 i) const
{
	if (mWearableCache.size() <= i)
	{
		return NULL;
	}
	LLWearable* wearable = mWearableCache[i];
	LLLocalTextureObject* lto = NULL;
	LLTexLayer* layer = NULL;
	if (wearable)
	{
		 lto = wearable->getLocalTextureObject(mInfo->mLocalTexture);
	}
	if (lto)
	{
		layer = lto->getTexLayer(getName());
	}
	return layer;
}

//virtual
bool LLTexLayerTemplate::render(S32 x, S32 y, S32 width, S32 height)
{
	if (!mInfo)
	{
		return false;
	}

	bool success = true;
	updateWearableCache();
	for (wearable_cache_t::const_iterator iter = mWearableCache.begin(),
										  end = mWearableCache.end();
		 iter!= end; ++iter)
	{
		LLWearable* wearable = *iter;
		LLLocalTextureObject* lto = NULL;
		LLTexLayer* layer = NULL;
		if (wearable)
		{
			lto = wearable->getLocalTextureObject(mInfo->mLocalTexture);
		}
		if (lto)
		{
			layer = lto->getTexLayer(getName());
		}
		if (layer)
		{
			wearable->writeToAvatar(mAvatarAppearance);
			layer->setLTO(lto);
			success &= layer->render(x,y,width,height);
		}
	}

	return success;
}

// Multiplies a single alpha texture against the frame buffer
//virtual
bool LLTexLayerTemplate::blendAlphaTexture(S32 x, S32 y, S32 width, S32 height)
{
	bool success = true;
	U32 num_wearables = updateWearableCache();
	for (U32 i = 0; i < num_wearables; ++i)
	{
		LLTexLayer* layer = getLayer(i);
		if (layer)
		{
			success &= layer->blendAlphaTexture(x, y, width, height);
		}
	}
	return success;
}

//virtual
void LLTexLayerTemplate::gatherAlphaMasks(U8* data, S32 originX, S32 originY,
										  S32 width, S32 height)
{
	U32 num_wearables = updateWearableCache();
	for (U32 i = 0; i < num_wearables; ++i)
	{
		LLTexLayer* layer = getLayer(i);
		if (layer)
		{
			layer->addAlphaMask(data, originX, originY, width, height);
		}
	}
}

//virtual
void LLTexLayerTemplate::setHasMorph(bool newval)
{
	mHasMorph = newval;
	U32 num_wearables = updateWearableCache();
	for (U32 i = 0; i < num_wearables; ++i)
	{
		LLTexLayer* layer = getLayer(i);
		if (layer)
		{
			layer->setHasMorph(newval);
		}
	}
}

//virtual
void LLTexLayerTemplate::deleteCaches()
{
	U32 num_wearables = updateWearableCache();
	for (U32 i = 0; i < num_wearables; ++i)
	{
		LLTexLayer *layer = getLayer(i);
		if (layer)
		{
			layer->deleteCaches();
		}
	}
}

//virtual
bool LLTexLayerTemplate::isInvisibleAlphaMask() const
{
	U32 num_wearables = updateWearableCache();
	for (U32 i = 0; i < num_wearables; ++i)
	{
		LLTexLayer *layer = getLayer(i);
		if (layer)
		{
			 if (layer->isInvisibleAlphaMask())
			 {
				 return true;
			 }
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// finds a specific layer based on a passed in name
//-----------------------------------------------------------------------------
LLTexLayerInterface*  LLTexLayerSet::findLayerByName(const std::string& name)
{
	for (layer_list_t::iterator iter = mLayerList.begin(),
								end = mLayerList.end();
		 iter != end; ++iter)
	{
		LLTexLayerInterface* layer = *iter;
		if (layer->getName() == name)
		{
			return layer;
		}
	}
	for (layer_list_t::iterator iter = mMaskLayerList.begin(),
								end = mMaskLayerList.end();
		 iter != end; ++iter)
	{
		LLTexLayerInterface* layer = *iter;
		if (layer->getName() == name)
		{
			return layer;
		}
	}
	return NULL;
}

void LLTexLayerSet::cloneTemplates(LLLocalTextureObject* lto,
								   LLAvatarAppearanceDefines::ETextureIndex tex_index,
								   LLWearable* wearable)
{
	// initialize all texlayers with this texture type for this LTO
	for (LLTexLayerSet::layer_list_t::iterator iter = mLayerList.begin(),
											   end = mLayerList.end();
		 iter != end; ++iter)
	{
		LLTexLayerTemplate* layer = (LLTexLayerTemplate*)*iter;
		if (layer->getInfo()->getLocalTexture() == (S32)tex_index)
		{
			lto->addTexLayer(layer, wearable);
		}
	}
	for (LLTexLayerSet::layer_list_t::iterator iter = mMaskLayerList.begin(),
											   end = mMaskLayerList.end();
		 iter != end; ++iter)
	{
		LLTexLayerTemplate* layer = (LLTexLayerTemplate*)*iter;
		if (layer->getInfo()->getLocalTexture() == (S32)tex_index)
		{
			lto->addTexLayer(layer, wearable);
		}
	}
}

//-----------------------------------------------------------------------------
// LLTexLayerStaticImageList
//-----------------------------------------------------------------------------

LLTexLayerStaticImageList::LLTexLayerStaticImageList()
:	mGLBytes(0),
	mTGABytes(0),
	mImageNames(16384)
{
}

LLTexLayerStaticImageList::~LLTexLayerStaticImageList()
{
	deleteCachedImages();
}

void LLTexLayerStaticImageList::dumpByteCount() const
{
	llinfos << "Avatar static textures " << "KB GL:" << mGLBytes / 1024
			<< "KB TGA:" << mTGABytes / 1024 << "KB" << llendl;
}

void LLTexLayerStaticImageList::deleteCachedImages()
{
	if (mGLBytes || mTGABytes)
	{
		llinfos << "Clearing static textures " << "KB GL:" << mGLBytes / 1024
				<< "KB TGA:" << mTGABytes / 1024 << "KB" << llendl;

		//mStaticImageLists uses LLPointers, clear() will cause deletion

		mStaticImageListTGA.clear();
		mStaticImageList.clear();

		mGLBytes = 0;
		mTGABytes = 0;
	}
}

// Note: in general, for a given image image we'll call either getImageTga() or
// getTexture(). We call getImageTga() if the image is used as an alpha
//gradient. Otherwise, we call getTexture()

// Returns an LLImageTGA that contains the encoded data from a tga file named
// file_name. Caches the result to speed identical subsequent requests.
LLImageTGA* LLTexLayerStaticImageList::getImageTGA(const std::string& file_name)
{
	LL_TRACY_TIMER(TRC_LOAD_STATIC_TGA);

	const char *namekey = mImageNames.addString(file_name);
	image_tga_map_t::const_iterator iter = mStaticImageListTGA.find(namekey);
	if (iter != mStaticImageListTGA.end())
	{
		return iter->second;
	}
	else
	{
		std::string path;
		path = gDirUtilp->getExpandedFilename(LL_PATH_CHARACTER, file_name);
		LLPointer<LLImageTGA> image_tga = new LLImageTGA(path);
		if (image_tga->getDataSize() > 0)
		{
			mStaticImageListTGA[namekey] = image_tga;
			mTGABytes += image_tga->getDataSize();
			return image_tga;
		}
		else
		{
			return NULL;
		}
	}
}

// Returns a GL Image (without a backing ImageRaw) that contains the decoded
// data from a tga file named file_name. Caches the result to speed identical
// subsequent requests.
LLGLTexture* LLTexLayerStaticImageList::getTexture(const std::string& file_name,
												   bool is_mask)
{
	LL_TRACY_TIMER(TRC_LOAD_STATIC_TEXTURE);

	LLPointer<LLGLTexture> tex = NULL;
	const char* namekey = mImageNames.addString(file_name);

	texture_map_t::const_iterator iter = mStaticImageList.find(namekey);
	if (iter != mStaticImageList.end())
	{
		tex = iter->second;
	}
	else if (gTextureManagerBridgep)
	{
		tex = gTextureManagerBridgep->getLocalTexture(false);
		LLPointer<LLImageRaw> image_raw = new LLImageRaw;
		if (loadImageRaw(file_name, image_raw))
		{
			if (is_mask && image_raw->getComponents() == 1)
			{
				// Convert grayscale alpha masks from single channel into RGBA.
				// Fill RGB with black to allow fixed function gl calls to
				// match shader implementation.
				LLPointer<LLImageRaw> alpha_image_raw = image_raw;
				image_raw = new LLImageRaw(image_raw->getWidth(),
										   image_raw->getHeight(), 4);

				image_raw->copyUnscaledAlphaMask(alpha_image_raw,
												 LLColor4U::black);
			}
			tex->createGLTexture(0, image_raw, 0, true, LLGLTexture::LOCAL);

			gGL.getTexUnit(0)->bind(tex);
			tex->setAddressMode(LLTexUnit::TAM_CLAMP);

			mStaticImageList[namekey] = tex;
			mGLBytes += (S32)tex->getWidth() * tex->getHeight() *
						tex->getComponents();
		}
	}

	return tex;
}

// Reads a .tga file, decodes it, and puts the decoded data in image_raw.
// Returns true if successful.
bool LLTexLayerStaticImageList::loadImageRaw(const std::string& file_name,
											 LLImageRaw* image_raw)
{
	LL_TRACY_TIMER(TRC_LOAD_IMAGE_RAW);

	bool success = false;
	std::string path;
	path = gDirUtilp->getExpandedFilename(LL_PATH_CHARACTER, file_name);
	LLPointer<LLImageTGA> image_tga = new LLImageTGA(path);
	if (image_tga->getDataSize() > 0)
	{
		// Copy data from tga to raw.
		success = image_tga->decode(image_raw);
	}

	return success;
}

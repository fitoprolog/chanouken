/**
 * @file lliconctrl.cpp
 * @brief LLIconCtrl base class
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

#include "linden_common.h"

#include "lliconctrl.h"

#include "llcontrol.h"
#include "lluictrlfactory.h"

static const std::string LL_ICON_CTRL_TAG = "icon";
static LLRegisterWidget<LLIconCtrl> r05(LL_ICON_CTRL_TAG);

LLIconCtrl::LLIconCtrl(const std::string& name,
					   const LLRect& rect,
					   const LLUUID& image_id)
:	LLUICtrl(name, rect, false, // mouse opaque
			 NULL, NULL, FOLLOWS_LEFT | FOLLOWS_TOP),
	mColor(LLColor4::white)
{
	setImage(image_id);
	setTabStop(false);
}

LLIconCtrl::LLIconCtrl(const std::string& name,
					   const LLRect& rect,
					   const std::string& image_name)
:	LLUICtrl(name, rect, false, // mouse opaque
			 NULL, NULL, FOLLOWS_LEFT | FOLLOWS_TOP),
	mColor(LLColor4::white),
	mImageName(image_name)
{
	setImage(image_name);
	setTabStop(false);
}

LLIconCtrl::~LLIconCtrl()
{
	mImagep = NULL;
}

void LLIconCtrl::setImage(const std::string& image_name)
{
	// RN: support UUIDs masquerading as strings
	if (LLUUID::validate(image_name))
	{
		mImageID = LLUUID(image_name);
		setImage(mImageID);
	}
	else
	{
		mImageName = image_name;
		mImagep = LLUI::getUIImage(image_name);
		mImageID.setNull();
	}
}

void LLIconCtrl::setImage(const LLUUID& image_id)
{
	mImageName.clear();
	mImagep = LLUI::getUIImageByID(image_id);
	mImageID = image_id;
}

void LLIconCtrl::draw()
{
	if (mImagep.notNull())
	{
		mImagep->draw(getLocalRect(), mColor);
	}

	LLUICtrl::draw();
}

// virtual
void LLIconCtrl::setAlpha(F32 alpha)
{
	mColor.setAlpha(alpha);
}

// virtual
void LLIconCtrl::setValue(const LLSD& value)
{
	if (value.isUUID())
	{
		setImage(value.asUUID());
	}
	else
	{
		setImage(value.asString());
	}
}

// virtual
LLSD LLIconCtrl::getValue() const
{
	LLSD ret = getImage();
	return ret;
}

// virtual
LLXMLNodePtr LLIconCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_ICON_CTRL_TAG);

	if (mImageName != "")
	{
		node->createChild("image_name", true)->setStringValue(mImageName);
	}

	node->createChild("color", true)->setFloatValue(4, mColor.mV);

	return node;
}

LLView* LLIconCtrl::fromXML(LLXMLNodePtr node, LLView* parent,
							LLUICtrlFactory* factory)
{
	std::string name = LL_ICON_CTRL_TAG;
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	std::string image_name;
	if (node->hasAttribute("image_name"))
	{
		node->getAttributeString("image_name", image_name);
	}

	LLColor4 color(LLColor4::white);
	LLUICtrlFactory::getAttributeColor(node, "color", color);

	LLIconCtrl* icon = new LLIconCtrl(name, rect, image_name);

	icon->setColor(color);

	icon->initFromXML(node, parent);

	return icon;
}

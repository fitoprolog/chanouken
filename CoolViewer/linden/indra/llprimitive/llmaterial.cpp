/**
 * @file llmaterial.cpp
 * @brief Material definition
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#include "llmaterial.h"

/**
 * Materials cap parameters
 */
#define MATERIALS_CAP_NORMAL_MAP_FIELD            "NormMap"
#define MATERIALS_CAP_NORMAL_MAP_OFFSET_X_FIELD   "NormOffsetX"
#define MATERIALS_CAP_NORMAL_MAP_OFFSET_Y_FIELD   "NormOffsetY"
#define MATERIALS_CAP_NORMAL_MAP_REPEAT_X_FIELD   "NormRepeatX"
#define MATERIALS_CAP_NORMAL_MAP_REPEAT_Y_FIELD   "NormRepeatY"
#define MATERIALS_CAP_NORMAL_MAP_ROTATION_FIELD   "NormRotation"

#define MATERIALS_CAP_SPECULAR_MAP_FIELD          "SpecMap"
#define MATERIALS_CAP_SPECULAR_MAP_OFFSET_X_FIELD "SpecOffsetX"
#define MATERIALS_CAP_SPECULAR_MAP_OFFSET_Y_FIELD "SpecOffsetY"
#define MATERIALS_CAP_SPECULAR_MAP_REPEAT_X_FIELD "SpecRepeatX"
#define MATERIALS_CAP_SPECULAR_MAP_REPEAT_Y_FIELD "SpecRepeatY"
#define MATERIALS_CAP_SPECULAR_MAP_ROTATION_FIELD "SpecRotation"

#define MATERIALS_CAP_SPECULAR_COLOR_FIELD        "SpecColor"
#define MATERIALS_CAP_SPECULAR_EXP_FIELD          "SpecExp"
#define MATERIALS_CAP_ENV_INTENSITY_FIELD         "EnvIntensity"
#define MATERIALS_CAP_ALPHA_MASK_CUTOFF_FIELD     "AlphaMaskCutoff"
#define MATERIALS_CAP_DIFFUSE_ALPHA_MODE_FIELD    "DiffuseAlphaMode"

const LLColor4U LLMaterial::DEFAULT_SPECULAR_LIGHT_COLOR(255, 255, 255, 255);

/**
 * Materials constants
 */

constexpr F32 MAT_MULTIPLIER = 10000.f;

/**
 * Helper functions
 */

template<typename T> T getMaterialField(const LLSD& data,
										const std::string& field,
										const LLSD::Type field_type)
{
	if (data.has(field) && field_type == data[field].type())
	{
		return (T)data[field];
	}
	llwarns << "Missing or mistyped field '" << field
			<< "' in material definition" << llendl;
	return (T)LLSD();
}

// GCC didn't like the generic form above for some reason
template<> LLUUID getMaterialField(const LLSD& data, const std::string& field,
								   const LLSD::Type field_type)
{
	if (data.has(field) && field_type == data[field].type())
	{
		return data[field].asUUID();
	}
	llwarns << "Missing or mistyped field '" << field
			<< "' in material definition" << llendl;
	return LLUUID::null;
}

/**
 * LLMaterial class
 */

const LLMaterial LLMaterial::null;

LLMaterial::LLMaterial()
:	mNormalOffsetX(0.0f),
	mNormalOffsetY(0.0f),
	mNormalRepeatX(1.0f),
	mNormalRepeatY(1.0f),
	mNormalRotation(0.0f),
	mSpecularOffsetX(0.0f),
	mSpecularOffsetY(0.0f),
	mSpecularRepeatX(1.0f),
	mSpecularRepeatY(1.0f),
	mSpecularRotation(0.0f),
	mSpecularLightColor(LLMaterial::DEFAULT_SPECULAR_LIGHT_COLOR),
	mSpecularLightExponent(LLMaterial::DEFAULT_SPECULAR_LIGHT_EXPONENT),
	mEnvironmentIntensity(LLMaterial::DEFAULT_ENV_INTENSITY),
	mDiffuseAlphaMode(LLMaterial::DIFFUSE_ALPHA_MODE_BLEND),
	mAlphaMaskCutoff(0)
{
}

LLMaterial::LLMaterial(const LLSD& material_data)
{
	fromLLSD(material_data);
}

LLSD LLMaterial::asLLSD() const
{
	LLSD material_data;

	material_data[MATERIALS_CAP_NORMAL_MAP_FIELD] = mNormalID;
	material_data[MATERIALS_CAP_NORMAL_MAP_OFFSET_X_FIELD] =
		ll_round(mNormalOffsetX * MAT_MULTIPLIER);
	material_data[MATERIALS_CAP_NORMAL_MAP_OFFSET_Y_FIELD] =
		ll_round(mNormalOffsetY * MAT_MULTIPLIER);
	material_data[MATERIALS_CAP_NORMAL_MAP_REPEAT_X_FIELD] =
		ll_round(mNormalRepeatX * MAT_MULTIPLIER);
	material_data[MATERIALS_CAP_NORMAL_MAP_REPEAT_Y_FIELD] =
		ll_round(mNormalRepeatY * MAT_MULTIPLIER);
	material_data[MATERIALS_CAP_NORMAL_MAP_ROTATION_FIELD] =
		ll_round(mNormalRotation * MAT_MULTIPLIER);

	material_data[MATERIALS_CAP_SPECULAR_MAP_FIELD] = mSpecularID;
	material_data[MATERIALS_CAP_SPECULAR_MAP_OFFSET_X_FIELD] =
		ll_round(mSpecularOffsetX * MAT_MULTIPLIER);
	material_data[MATERIALS_CAP_SPECULAR_MAP_OFFSET_Y_FIELD] =
		ll_round(mSpecularOffsetY * MAT_MULTIPLIER);
	material_data[MATERIALS_CAP_SPECULAR_MAP_REPEAT_X_FIELD] =
		ll_round(mSpecularRepeatX * MAT_MULTIPLIER);
	material_data[MATERIALS_CAP_SPECULAR_MAP_REPEAT_Y_FIELD] =
		ll_round(mSpecularRepeatY * MAT_MULTIPLIER);
	material_data[MATERIALS_CAP_SPECULAR_MAP_ROTATION_FIELD] =
		ll_round(mSpecularRotation * MAT_MULTIPLIER);

	material_data[MATERIALS_CAP_SPECULAR_COLOR_FIELD] =
		mSpecularLightColor.getValue();
	material_data[MATERIALS_CAP_SPECULAR_EXP_FIELD] = mSpecularLightExponent;
	material_data[MATERIALS_CAP_ENV_INTENSITY_FIELD] = mEnvironmentIntensity;
	material_data[MATERIALS_CAP_DIFFUSE_ALPHA_MODE_FIELD] = mDiffuseAlphaMode;
	material_data[MATERIALS_CAP_ALPHA_MASK_CUTOFF_FIELD] = mAlphaMaskCutoff;

	return material_data;
}

void LLMaterial::fromLLSD(const LLSD& material_data)
{
	mNormalID = getMaterialField<LLSD::UUID>(material_data,
											 MATERIALS_CAP_NORMAL_MAP_FIELD,
											 LLSD::TypeUUID);
	mNormalOffsetX =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MATERIALS_CAP_NORMAL_MAP_OFFSET_X_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mNormalOffsetY =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MATERIALS_CAP_NORMAL_MAP_OFFSET_Y_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mNormalRepeatX =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MATERIALS_CAP_NORMAL_MAP_REPEAT_X_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mNormalRepeatY =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MATERIALS_CAP_NORMAL_MAP_REPEAT_Y_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mNormalRotation =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MATERIALS_CAP_NORMAL_MAP_ROTATION_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;

	mSpecularID =
		getMaterialField<LLSD::UUID>(material_data,
									 MATERIALS_CAP_SPECULAR_MAP_FIELD,
									 LLSD::TypeUUID);
	mSpecularOffsetX =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MATERIALS_CAP_SPECULAR_MAP_OFFSET_X_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mSpecularOffsetY =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MATERIALS_CAP_SPECULAR_MAP_OFFSET_Y_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mSpecularRepeatX =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MATERIALS_CAP_SPECULAR_MAP_REPEAT_X_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mSpecularRepeatY =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MATERIALS_CAP_SPECULAR_MAP_REPEAT_Y_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mSpecularRotation =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MATERIALS_CAP_SPECULAR_MAP_ROTATION_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;

	mSpecularLightColor.setValue(getMaterialField<LLSD>(material_data,
														MATERIALS_CAP_SPECULAR_COLOR_FIELD,
														LLSD::TypeArray));
	mSpecularLightExponent =
		(U8)getMaterialField<LLSD::Integer>(material_data,
											MATERIALS_CAP_SPECULAR_EXP_FIELD,
											LLSD::TypeInteger);
	mEnvironmentIntensity =
		(U8)getMaterialField<LLSD::Integer>(material_data,
											MATERIALS_CAP_ENV_INTENSITY_FIELD,
											LLSD::TypeInteger);
	mDiffuseAlphaMode =
		(U8)getMaterialField<LLSD::Integer>(material_data,
											MATERIALS_CAP_DIFFUSE_ALPHA_MODE_FIELD,
											LLSD::TypeInteger);
	mAlphaMaskCutoff =
		(U8)getMaterialField<LLSD::Integer>(material_data,
											MATERIALS_CAP_ALPHA_MASK_CUTOFF_FIELD,
											LLSD::TypeInteger);
}

bool LLMaterial::isNull() const
{
	return *this == null;
}

bool LLMaterial::operator==(const LLMaterial& rhs) const
{
	return mNormalID == rhs.mNormalID &&
		   mNormalOffsetX == rhs.mNormalOffsetX &&
		   mNormalOffsetY == rhs.mNormalOffsetY &&
		   mNormalRepeatX == rhs.mNormalRepeatX &&
		   mNormalRepeatY == rhs.mNormalRepeatY &&
		   mNormalRotation == rhs.mNormalRotation &&
		   mSpecularID == rhs.mSpecularID &&
		   mSpecularOffsetX == rhs.mSpecularOffsetX &&
		   mSpecularOffsetY == rhs.mSpecularOffsetY &&
		   mSpecularRepeatX == rhs.mSpecularRepeatX &&
		   mSpecularRepeatY == rhs.mSpecularRepeatY &&
		   mSpecularRotation == rhs.mSpecularRotation &&
		   mSpecularLightColor == rhs.mSpecularLightColor &&
		   mSpecularLightExponent == rhs.mSpecularLightExponent &&
		   mEnvironmentIntensity == rhs.mEnvironmentIntensity &&
		   mDiffuseAlphaMode == rhs.mDiffuseAlphaMode &&
		   mAlphaMaskCutoff == rhs.mAlphaMaskCutoff;
}

bool LLMaterial::operator!=(const LLMaterial& rhs) const
{
	return !(*this == rhs);
}

// NEVER incorporate this value into the message system: this function will
// vary depending on viewer implementation
U32 LLMaterial::getShaderMask(U32 alpha_mode)
{
	U32 ret = 0;

	// Two least significant bits are "diffuse alpha mode"
	if (alpha_mode != DIFFUSE_ALPHA_MODE_DEFAULT)
	{
		ret = alpha_mode;
	}
	else
	{
		ret = getDiffuseAlphaMode();
	}

	llassert(ret < SHADER_COUNT);

	// Next bit is whether or not specular map is present
	constexpr U32 SPEC_BIT = 0x4;

	if (getSpecularID().notNull())
	{
		ret |= SPEC_BIT;
	}

	llassert(ret < SHADER_COUNT);

	// Next bit is whether or not normal map is present
	constexpr U32 NORM_BIT = 0x8;
	if (getNormalID().notNull())
	{
		ret |= NORM_BIT;
	}

	llassert(ret < SHADER_COUNT);

	return ret;
}

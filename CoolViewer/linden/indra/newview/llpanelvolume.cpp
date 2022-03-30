/**
 * @file llpanelvolume.cpp
 * @brief Object editing (position, scale, etc.) in the tools floater
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

#include "llpanelvolume.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lleconomy.h"
#include "llnotifications.h"
#include "llpermissionsflags.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llvolume.h"

#include "llagent.h"
#include "llcolorswatch.h"
#include "lldrawpool.h"
#include "llfirstuse.h"
#include "llflexibleobject.h"
#include "llmanipscale.h"
#include "llmeshrepository.h"
#include "llpipeline.h"
#include "llpreviewscript.h"
#include "llselectmgr.h"
#include "lltexturectrl.h"
#include "lltooldraganddrop.h"
#include "lltool.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llvovolume.h"
#include "llworld.h"

constexpr F32 DEFAULT_GRAVITY_MULTIPLIER = 1.f;
constexpr F32 DEFAULT_DENSITY = 1000.f;

// "Features" Tab

LLPanelVolume::LLPanelVolume(const std::string& name)
:	LLPanel(name),
	mComboMaterialItemCount(0)
{
	setMouseOpaque(false);
}

LLPanelVolume::~LLPanelVolume()
{
	// Children all cleaned up by default view destructor.
}

bool LLPanelVolume::postBuild()
{
	mLabelSelectSingle = getChild<LLTextBox>("select_single");
	mLabelEditObject = getChild<LLTextBox>("edit_object");

	// Flexible objects parameters

	mCheckFlexiblePath = getChild<LLCheckBoxCtrl>("Flexible1D Checkbox Ctrl");
	mCheckFlexiblePath->setCommitCallback(onCommitIsFlexible);
	mCheckFlexiblePath->setCallbackUserData(this);

	mSpinFlexSections = getChild<LLSpinCtrl>("FlexNumSections");
	mSpinFlexSections->setCommitCallback(onCommitFlexible);
	mSpinFlexSections->setCallbackUserData(this);
	mSpinFlexSections->setValidateBeforeCommit(precommitValidate);

	mSpinFlexGravity = getChild<LLSpinCtrl>("FlexGravity");
	mSpinFlexGravity->setCommitCallback(onCommitFlexible);
	mSpinFlexGravity->setCallbackUserData(this);
	mSpinFlexGravity->setValidateBeforeCommit(precommitValidate);

	mSpinFlexFriction = getChild<LLSpinCtrl>("FlexFriction");
	mSpinFlexFriction->setCommitCallback(onCommitFlexible);
	mSpinFlexFriction->setCallbackUserData(this);
	mSpinFlexFriction->setValidateBeforeCommit(precommitValidate);

	mSpinFlexWind = getChild<LLSpinCtrl>("FlexWind");
	mSpinFlexWind->setCommitCallback(onCommitFlexible);
	mSpinFlexWind->setCallbackUserData(this);
	mSpinFlexWind->setValidateBeforeCommit(precommitValidate);

	mSpinFlexTension = getChild<LLSpinCtrl>("FlexTension");
	mSpinFlexTension->setCommitCallback(onCommitFlexible);
	mSpinFlexTension->setCallbackUserData(this);
	mSpinFlexTension->setValidateBeforeCommit(precommitValidate);

	mSpinFlexForceX = getChild<LLSpinCtrl>("FlexForceX");
	mSpinFlexForceX->setCommitCallback(onCommitFlexible);
	mSpinFlexForceX->setCallbackUserData(this);
	mSpinFlexForceX->setValidateBeforeCommit(precommitValidate);

	mSpinFlexForceY = getChild<LLSpinCtrl>("FlexForceY");
	mSpinFlexForceY->setCommitCallback(onCommitFlexible);
	mSpinFlexForceY->setCallbackUserData(this);
	mSpinFlexForceY->setValidateBeforeCommit(precommitValidate);

	mSpinFlexForceZ = getChild<LLSpinCtrl>("FlexForceZ");
	mSpinFlexForceZ->setCommitCallback(onCommitFlexible);
	mSpinFlexForceZ->setCallbackUserData(this);
	mSpinFlexForceZ->setValidateBeforeCommit(precommitValidate);

	// Light parameters

	mCheckEmitLight = getChild<LLCheckBoxCtrl>("Light Checkbox Ctrl");
	mCheckEmitLight->setCommitCallback(onCommitIsLight);
	mCheckEmitLight->setCallbackUserData(this);

	mSwatchLightColor = getChild<LLColorSwatchCtrl>("colorswatch");
	mSwatchLightColor->setOnCancelCallback(onLightCancelColor);
	mSwatchLightColor->setOnSelectCallback(onLightSelectColor);
	mSwatchLightColor->setCommitCallback(onCommitLight);
	mSwatchLightColor->setCallbackUserData(this);

	mTextureLight = getChild<LLTextureCtrl>("light texture control");
	mTextureLight->setCommitCallback(onCommitLight);
	mTextureLight->setCallbackUserData(this);
	mTextureLight->setOnCancelCallback(onLightCancelTexture);
	mTextureLight->setOnSelectCallback(onLightSelectTexture);
	mTextureLight->setDragCallback(onDragTexture);
	// Do not allow (no copy) or (no transfer) textures to be selected during
	// immediate mode
	mTextureLight->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
	// Allow any texture to be used during non-immediate mode.
	mTextureLight->setNonImmediateFilterPermMask(PERM_NONE);
	LLAggregatePermissions texture_perms;
	if (gSelectMgr.selectGetAggregateTexturePermissions(texture_perms))
	{
		bool can_copy = texture_perms.getValue(PERM_COPY) == LLAggregatePermissions::AP_EMPTY ||
			            texture_perms.getValue(PERM_COPY) == LLAggregatePermissions::AP_ALL;
		bool can_transfer = texture_perms.getValue(PERM_TRANSFER) == LLAggregatePermissions::AP_EMPTY ||
							texture_perms.getValue(PERM_TRANSFER) == LLAggregatePermissions::AP_ALL;
		mTextureLight->setCanApplyImmediately(can_copy && can_transfer);
	}
	else
	{
		mTextureLight->setCanApplyImmediately(false);
	}

	mSpinLightIntensity = getChild<LLSpinCtrl>("Light Intensity");
	mSpinLightIntensity->setCommitCallback(onCommitLight);
	mSpinLightIntensity->setCallbackUserData(this);
	mSpinLightIntensity->setValidateBeforeCommit(precommitValidate);

	mSpinLightRadius = getChild<LLSpinCtrl>("Light Radius");
	mSpinLightRadius->setCommitCallback(onCommitLight);
	mSpinLightRadius->setCallbackUserData(this);
	mSpinLightRadius->setValidateBeforeCommit(precommitValidate);

	mSpinLightFalloff = getChild<LLSpinCtrl>("Light Falloff");
	mSpinLightFalloff->setCommitCallback(onCommitLight);
	mSpinLightFalloff->setCallbackUserData(this);
	mSpinLightFalloff->setValidateBeforeCommit(precommitValidate);

	mSpinLightFOV = getChild<LLSpinCtrl>("Light FOV");
	mSpinLightFOV->setCommitCallback(onCommitLight);
	mSpinLightFOV->setCallbackUserData(this);
	mSpinLightFOV->setValidateBeforeCommit(precommitValidate);

	mSpinLightFocus = getChild<LLSpinCtrl>("Light Focus");
	mSpinLightFocus->setCommitCallback(onCommitLight);
	mSpinLightFocus->setCallbackUserData(this);
	mSpinLightFocus->setValidateBeforeCommit(precommitValidate);

	mSpinLightAmbiance = getChild<LLSpinCtrl>("Light Ambiance");
	mSpinLightAmbiance->setCommitCallback(onCommitLight);
	mSpinLightAmbiance->setCallbackUserData(this);
	mSpinLightAmbiance->setValidateBeforeCommit(precommitValidate);

	// Physics parameters

	mLabelPhysicsShape = getChild<LLTextBox>("label physicsshapetype");
	mLabelPhysicsParams = getChild<LLTextBox>("label physicsparams");

	mComboPhysicsShape = getChild<LLComboBox>("Physics Shape Type Combo Ctrl");
	mComboPhysicsShape->setCommitCallback(sendPhysicsShapeType);
	mComboPhysicsShape->setCallbackUserData(this);

	mSpinPhysicsGravity = getChild<LLSpinCtrl>("Physics Gravity");
	mSpinPhysicsGravity->setCommitCallback(sendPhysicsGravity);
	mSpinPhysicsGravity->setCallbackUserData(this);

	mSpinPhysicsFriction = getChild<LLSpinCtrl>("Physics Friction");
	mSpinPhysicsFriction->setCommitCallback(sendPhysicsFriction);
	mSpinPhysicsFriction->setCallbackUserData(this);

	mSpinPhysicsDensity = getChild<LLSpinCtrl>("Physics Density");
	mSpinPhysicsDensity->setCommitCallback(sendPhysicsDensity);
	mSpinPhysicsDensity->setCallbackUserData(this);

	mSpinPhysicsRestitution = getChild<LLSpinCtrl>("Physics Restitution");
	mSpinPhysicsRestitution->setCommitCallback(sendPhysicsRestitution);
	mSpinPhysicsRestitution->setCallbackUserData(this);

	mPhysicsNone = getString("None");
	mPhysicsPrim = getString("Prim");
	mPhysicsHull = getString("Convex Hull");

	// Material parameters

	mFullBright = LLTrans::getString("Fullbright");

	std::map<std::string, std::string> material_name_map;
	material_name_map["Stone"]= LLTrans::getString("Stone");
	material_name_map["Metal"]= LLTrans::getString("Metal");
	material_name_map["Glass"]= LLTrans::getString("Glass");
	material_name_map["Wood"]= LLTrans::getString("Wood");
	material_name_map["Flesh"]= LLTrans::getString("Flesh");
	material_name_map["Plastic"]= LLTrans::getString("Plastic");
	material_name_map["Rubber"]= LLTrans::getString("Rubber");
	material_name_map["Light"]= LLTrans::getString("Light");
	gMaterialTable.initTableTransNames(material_name_map);

	mLabelMaterial = getChild<LLTextBox>("label material");
	mComboMaterial = getChild<LLComboBox>("material");
	childSetCommitCallback("material", onCommitMaterial, this);
	mComboMaterial->removeall();

	for (LLMaterialTable::info_list_t::const_iterator
			iter = gMaterialTable.mMaterialInfoList.begin(),
			end = gMaterialTable.mMaterialInfoList.end();
		 iter != end; ++iter)
	{
		const LLMaterialInfo& minfo = *iter;
		if (minfo.mMCode != LL_MCODE_LIGHT)
		{
			mComboMaterial->add(minfo.mName);
		}
	}
	mComboMaterialItemCount = mComboMaterial->getItemCount();

	// Animated mesh/puppet parameter
	mCheckAnimatedMesh = getChild<LLCheckBoxCtrl>("AniMesh Checkbox Ctrl");
	mCheckAnimatedMesh->setCommitCallback(onCommitAnimatedMesh);
	mCheckAnimatedMesh->setCallbackUserData(this);

	// Start with everyone disabled
	clearCtrls();

	return true;
}

void LLPanelVolume::getState()
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	LLViewerObject* objectp = selection->getFirstRootObject();
	LLViewerObject* root_objectp = objectp;
	if (!objectp)
	{
		objectp = selection->getFirstObject();
		// *FIX: shouldn't we just keep the child ?
		if (objectp)
		{
			LLViewerObject* parentp = objectp->getRootEdit();
			root_objectp = parentp ? parentp : objectp;
		}
	}

	LLVOVolume* volobjp = NULL;
	if (objectp && objectp->getPCode() == LL_PCODE_VOLUME)
	{
		volobjp = (LLVOVolume*)objectp;
	}

	LLVOVolume* root_volobjp = NULL;
	if (root_objectp && root_objectp->getPCode() == LL_PCODE_VOLUME)
	{
		root_volobjp = (LLVOVolume*)root_objectp;
	}

	if (!objectp)
	{
		// Forfeit focus
		if (gFocusMgr.childHasKeyboardFocus(this))
		{
			gFocusMgr.setKeyboardFocus(NULL);
		}

		// Disable all text input fields
		clearCtrls();

		return;
	}

	LLUUID owner_id;
	std::string owner_name;
	gSelectMgr.selectGetOwner(owner_id, owner_name);

	// BUG ?  Check for all objects being editable ?
	bool editable = root_objectp->permModify() &&
					!root_objectp->isPermanentEnforced();
	bool visbile_params = (editable || gAgent.isGodlikeWithoutAdminMenuFakery());
	bool single_volume = gSelectMgr.selectionAllPCode(LL_PCODE_VOLUME) &&
						 selection->getObjectCount() == 1;
	bool single_root_volume = gSelectMgr.selectionAllPCode(LL_PCODE_VOLUME) &&
							  gSelectMgr.getSelection()->getRootObjectCount() == 1;

	// Select Single Message
	if (single_volume)
	{
		mLabelSelectSingle->setVisible(false);
		mLabelEditObject->setVisible(true);
		mLabelEditObject->setEnabled(true);
	}
	else
	{
		mLabelSelectSingle->setVisible(true);
		mLabelSelectSingle->setEnabled(true);
		mLabelEditObject->setVisible(false);
	}

	// Light properties
	bool is_light = volobjp && volobjp->getIsLight();
	mCheckEmitLight->setValue(is_light);
	mCheckEmitLight->setEnabled(editable && single_volume && volobjp);

	if (is_light && single_volume && visbile_params)
	{
		mLightSavedColor = volobjp->getLightSRGBBaseColor();

		mSwatchLightColor->setEnabled(editable);
		mSwatchLightColor->setValid(true);
		mSwatchLightColor->set(mLightSavedColor);

		mTextureLight->setEnabled(editable);
		mTextureLight->setValid(true);
		mTextureLight->setImageAssetID(volobjp->getLightTextureID());

		mSpinLightIntensity->setEnabled(editable);
		mSpinLightRadius->setEnabled(true);
		mSpinLightFalloff->setEnabled(true);

		mSpinLightFOV->setEnabled(editable);
		mSpinLightFocus->setEnabled(true);
		mSpinLightAmbiance->setEnabled(true);

		mSpinLightIntensity->setValue(volobjp->getLightIntensity());
		mSpinLightRadius->setValue(volobjp->getLightRadius());
		mSpinLightFalloff->setValue(volobjp->getLightFalloff());

		LLVector3 params = volobjp->getSpotLightParams();
		mSpinLightFOV->setValue(params.mV[0]);
		mSpinLightFocus->setValue(params.mV[1]);
		mSpinLightAmbiance->setValue(params.mV[2]);
	}
	else
	{
		mSpinLightIntensity->clear();
		mSpinLightRadius->clear();
		mSpinLightFalloff->clear();

		mSwatchLightColor->setEnabled(false);
		mSwatchLightColor->setValid(false);

		mTextureLight->setEnabled(false);
		mTextureLight->setValid(false);

		mSpinLightIntensity->setEnabled(false);
		mSpinLightRadius->setEnabled(false);
		mSpinLightFalloff->setEnabled(false);

		mSpinLightFOV->setEnabled(false);
		mSpinLightFocus->setEnabled(false);
		mSpinLightAmbiance->setEnabled(false);
	}

	// Animated mesh property
	bool is_animated_mesh = single_root_volume && root_volobjp &&
							root_volobjp->isAnimatedObject();
	mCheckAnimatedMesh->setValue(is_animated_mesh);

	bool enabled_animated_mesh = false;
	if (editable && single_root_volume && root_volobjp &&
		root_volobjp == volobjp)
	{
		enabled_animated_mesh = root_volobjp->canBeAnimatedObject();
		if (enabled_animated_mesh && !is_animated_mesh &&
			root_volobjp->isAttachment())
		{
			 enabled_animated_mesh =
				isAgentAvatarValid() &&
				gAgentAvatarp->canAttachMoreAnimatedObjects();
		}
	}
	mCheckAnimatedMesh->setEnabled(enabled_animated_mesh);

	// Refresh any bake on mesh texture
	if (root_volobjp)
	{
		root_volobjp->refreshBakeTexture();
		LLViewerObject::const_child_list_t& child_list =
			root_volobjp->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				it = child_list.begin(), end = child_list.end();
			 it != end; ++it)
		{
			LLViewerObject* childp = *it;
			if (childp)
			{
				childp->refreshBakeTexture();
			}
		}
		if (isAgentAvatarValid())
		{
			gAgentAvatarp->updateMeshVisibility();
		}
	}

	// Flexible properties
	bool is_flexible = volobjp && volobjp->isFlexible();
	mCheckFlexiblePath->setValue(is_flexible);
	if (is_flexible || (volobjp && volobjp->canBeFlexible()))
	{
		mCheckFlexiblePath->setEnabled(editable && single_volume && volobjp &&
									   !volobjp->isMesh() &&
									   !objectp->isPermanentEnforced());
	}
	else
	{
		mCheckFlexiblePath->setEnabled(false);
	}
	if (is_flexible && single_volume && visbile_params)
	{
		mSpinFlexSections->setVisible(true);
		mSpinFlexGravity->setVisible(true);
		mSpinFlexFriction->setVisible(true);
		mSpinFlexWind->setVisible(true);
		mSpinFlexTension->setVisible(true);
		mSpinFlexForceX->setVisible(true);
		mSpinFlexForceY->setVisible(true);
		mSpinFlexForceZ->setVisible(true);

		mSpinFlexSections->setEnabled(editable);
		mSpinFlexGravity->setEnabled(editable);
		mSpinFlexFriction->setEnabled(editable);
		mSpinFlexWind->setEnabled(editable);
		mSpinFlexTension->setEnabled(editable);
		mSpinFlexForceX->setEnabled(editable);
		mSpinFlexForceY->setEnabled(editable);
		mSpinFlexForceZ->setEnabled(editable);

		const LLFlexibleObjectData* params = objectp->getFlexibleObjectData();
		mSpinFlexSections->setValue((F32)params->getSimulateLOD());
		mSpinFlexGravity->setValue(params->getGravity());
		mSpinFlexFriction->setValue(params->getAirFriction());
		mSpinFlexWind->setValue(params->getWindSensitivity());
		mSpinFlexTension->setValue(params->getTension());
		mSpinFlexForceX->setValue(params->getUserForce().mV[VX]);
		mSpinFlexForceY->setValue(params->getUserForce().mV[VY]);
		mSpinFlexForceZ->setValue(params->getUserForce().mV[VZ]);
	}
	else
	{
		mSpinFlexSections->clear();
		mSpinFlexGravity->clear();
		mSpinFlexFriction->clear();
		mSpinFlexWind->clear();
		mSpinFlexTension->clear();
		mSpinFlexForceX->clear();
		mSpinFlexForceY->clear();
		mSpinFlexForceZ->clear();

		mSpinFlexSections->setEnabled(false);
		mSpinFlexGravity->setEnabled(false);
		mSpinFlexFriction->setEnabled(false);
		mSpinFlexWind->setEnabled(false);
		mSpinFlexTension->setEnabled(false);
		mSpinFlexForceX->setEnabled(false);
		mSpinFlexForceY->setEnabled(false);
		mSpinFlexForceZ->setEnabled(false);
	}

	// Material properties

	// Update material part
	// slightly inefficient - materials are unique per object, not per TE
	U8 mcode = 0;
	struct f : public LLSelectedTEGetFunctor<U8>
	{
		U8 get(LLViewerObject* object, S32 te)
		{
			return object->getMaterial();
		}
	} func;
	bool material_same = selection->getSelectedTEValue(&func, mcode);
	if (single_volume && material_same && visbile_params)
	{
		mComboMaterial->setEnabled(editable);
		mLabelMaterial->setEnabled(editable);
		if (mcode == LL_MCODE_LIGHT)
		{
			if (mComboMaterial->getItemCount() == mComboMaterialItemCount)
			{
				mComboMaterial->add(mFullBright);
			}
			mComboMaterial->setSimple(mFullBright);
		}
		else
		{
			if (mComboMaterial->getItemCount() != mComboMaterialItemCount)
			{
				mComboMaterial->remove(mFullBright);
			}
			// *TODO:Translate
			mComboMaterial->setSimple(gMaterialTable.getName(mcode));
		}
	}
	else
	{
		mComboMaterial->setEnabled(false);
		mLabelMaterial->setEnabled(false);
	}

	// Physics properties

	bool is_physical = root_objectp && root_objectp->flagUsePhysics();
	if (is_physical && visbile_params)
	{
		mLabelPhysicsParams->setEnabled(editable);
		mSpinPhysicsGravity->setValue(objectp->getPhysicsGravity());
		mSpinPhysicsGravity->setEnabled(editable);
		mSpinPhysicsFriction->setValue(objectp->getPhysicsFriction());
		mSpinPhysicsFriction->setEnabled(editable);
		mSpinPhysicsDensity->setValue(objectp->getPhysicsDensity());
		mSpinPhysicsDensity->setEnabled(editable);
		mSpinPhysicsRestitution->setValue(objectp->getPhysicsRestitution());
		mSpinPhysicsRestitution->setEnabled(editable);
	}
	else
	{
		mLabelPhysicsParams->setEnabled(false);
		mSpinPhysicsGravity->clear();
		mSpinPhysicsGravity->setEnabled(false);
		mSpinPhysicsFriction->clear();
		mSpinPhysicsFriction->setEnabled(false);
		mSpinPhysicsDensity->clear();
		mSpinPhysicsDensity->setEnabled(false);
		mSpinPhysicsRestitution->clear();
		mSpinPhysicsRestitution->setEnabled(false);
	}

	// Update the physics shape combo to include allowed physics shapes
	mComboPhysicsShape->removeall();
	mComboPhysicsShape->add(mPhysicsNone, LLSD(1));

	bool is_mesh = false;
	if (objectp)
	{
		const LLSculptParams* sculpt_params = objectp->getSculptParams();
		is_mesh = sculpt_params &&
				  (sculpt_params->getSculptType() &
				   LL_SCULPT_TYPE_MASK) == LL_SCULPT_TYPE_MESH;
	}
	if (is_mesh)
	{
		const LLVolumeParams& volume_params =
			objectp->getVolume()->getParams();
		LLUUID mesh_id = volume_params.getSculptID();
		if (gMeshRepo.hasPhysicsShape(mesh_id))
		{
			// If a mesh contains an uploaded or decomposed physics mesh,
			// allow 'Prim'
			mComboPhysicsShape->add(mPhysicsPrim, LLSD(0));
		}
	}
	else
	{
		// Simple prims always allow physics shape prim
		mComboPhysicsShape->add(mPhysicsPrim, LLSD(0));
	}

	mComboPhysicsShape->add(mPhysicsHull, LLSD(2));
	mComboPhysicsShape->setValue(LLSD(objectp->getPhysicsShapeType()));
	bool enabled = editable && !objectp->isPermanentEnforced() &&
				   (!root_objectp || !root_objectp->isPermanentEnforced());
	mComboPhysicsShape->setEnabled(enabled);
	mLabelPhysicsShape->setEnabled(enabled);

	mObject = objectp;
	mRootObject = root_objectp;
}

//static
bool LLPanelVolume::precommitValidate(LLUICtrl* ctrl, void* userdata)
{
	// *TODO: Richard will fill this in later.
	// false means that validation failed and new value should not be commited.
	return true;
}

void LLPanelVolume::refresh()
{
	getState();
	if (mObject.notNull() && mObject->isDead())
	{
		mObject = NULL;
	}

	if (mRootObject.notNull() && mRootObject->isDead())
	{
		mRootObject = NULL;
	}

	bool enable_physics = false;
	LLSD sim_features;
	LLViewerRegion* region = gAgent.getRegion();
	if (region)
	{
		enable_physics = region->physicsShapeTypes();
	}
	mLabelPhysicsShape->setVisible(enable_physics);
	mComboPhysicsShape->setVisible(enable_physics);
	mLabelPhysicsParams->setVisible(enable_physics);
	mSpinPhysicsGravity->setVisible(enable_physics);
	mSpinPhysicsFriction->setVisible(enable_physics);
	mSpinPhysicsDensity->setVisible(enable_physics);
	mSpinPhysicsRestitution->setVisible(enable_physics);
    // *TODO: add/remove individual physics shape types as per the
	// PhysicsShapeTypes simulator features
}

// virtual
void LLPanelVolume::clearCtrls()
{
	LLPanel::clearCtrls();

	mLabelSelectSingle->setEnabled(false);
	mLabelSelectSingle->setVisible(true);
	mLabelEditObject->setEnabled(false);
	mLabelEditObject->setVisible(false);

	mCheckEmitLight->setEnabled(false);
	mSwatchLightColor->setEnabled(false);
	mSwatchLightColor->setValid(false);

	mTextureLight->setEnabled(false);
	mTextureLight->setValid(false);

	mSpinLightIntensity->setEnabled(false);
	mSpinLightRadius->setEnabled(false);
	mSpinLightFalloff->setEnabled(false);
	mSpinLightFOV->setEnabled(false);
	mSpinLightFocus->setEnabled(false);
	mSpinLightAmbiance->setEnabled(false);

	mCheckFlexiblePath->setEnabled(false);
	mSpinFlexSections->setEnabled(false);
	mSpinFlexGravity->setEnabled(false);
	mSpinFlexFriction->setEnabled(false);
	mSpinFlexWind->setEnabled(false);
	mSpinFlexTension->setEnabled(false);
	mSpinFlexForceX->setEnabled(false);
	mSpinFlexForceY->setEnabled(false);
	mSpinFlexForceZ->setEnabled(false);

	mSpinPhysicsGravity->setEnabled(false);
	mSpinPhysicsFriction->setEnabled(false);
	mSpinPhysicsDensity->setEnabled(false);
	mSpinPhysicsRestitution->setEnabled(false);

	mComboMaterial->setEnabled(false);
	mLabelMaterial->setEnabled(false);

	mCheckAnimatedMesh->setEnabled(false);
}

//
// Static functions
//

void LLPanelVolume::sendIsLight()
{
	LLViewerObject* objectp = mObject;
	if (!objectp || (objectp->getPCode() != LL_PCODE_VOLUME))
	{
		return;
	}
	LLVOVolume* volobjp = (LLVOVolume*)objectp;

	bool value = mCheckEmitLight->getValue();
	volobjp->setIsLight(value);
	llinfos << "update light sent" << llendl;
}

void LLPanelVolume::sendIsFlexible()
{
	LLViewerObject* objectp = mObject;
	if (!objectp || (objectp->getPCode() != LL_PCODE_VOLUME))
	{
		return;
	}
	LLVOVolume* volobjp = (LLVOVolume*)objectp;

	bool is_flexible = mCheckFlexiblePath->getValue();
	if (is_flexible)
	{
		LLFirstUse::useFlexible();

		if (objectp->getClickAction() == CLICK_ACTION_SIT)
		{
			gSelectMgr.selectionSetClickAction(CLICK_ACTION_NONE);
		}
	}

	if (volobjp->setIsFlexible(is_flexible))
	{
		mObject->sendShapeUpdate();
		gSelectMgr.selectionUpdatePhantom(volobjp->flagPhantom());
	}

	llinfos << "update flexible sent" << llendl;
}

void LLPanelVolume::sendPhysicsShapeType(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*) userdata;
	if (!self || !ctrl) return;
	U8 type = ctrl->getValue().asInteger();
	gSelectMgr.selectionSetPhysicsType(type);

	self->refreshCost();
}

void LLPanelVolume::sendPhysicsGravity(LLUICtrl* ctrl, void* userdata)
{
	F32 val = ctrl->getValue().asReal();
	gSelectMgr.selectionSetGravity(val);
}

void LLPanelVolume::sendPhysicsFriction(LLUICtrl* ctrl, void* userdata)
{
	F32 val = ctrl->getValue().asReal();
	gSelectMgr.selectionSetFriction(val);
}

void LLPanelVolume::sendPhysicsRestitution(LLUICtrl* ctrl, void* userdata)
{
	F32 val = ctrl->getValue().asReal();
	gSelectMgr.selectionSetRestitution(val);
}

void LLPanelVolume::sendPhysicsDensity(LLUICtrl* ctrl, void* userdata)
{
	F32 val = ctrl->getValue().asReal();
	gSelectMgr.selectionSetDensity(val);
}

void LLPanelVolume::refreshCost()
{
	LLViewerObject* obj = gSelectMgr.getSelection()->getFirstObject();
	if (obj)
	{
		obj->getObjectCost();
	}
}

void LLPanelVolume::onLightCancelColor(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (self)
	{
		self->mSwatchLightColor->setColor(self->mLightSavedColor);
		onLightSelectColor(NULL, userdata);
	}
}

void LLPanelVolume::onLightCancelTexture(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (self)
	{
		self->mTextureLight->setImageAssetID(self->mLightSavedTexture);
	}
}

void LLPanelVolume::onLightSelectColor(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (!self || !self->mObject ||
		self->mObject->getPCode() != LL_PCODE_VOLUME)
	{
		return;
	}
	LLVOVolume* volobjp = (LLVOVolume*)self->mObject.get();
	if (!volobjp) return;

	LLColor4 clr = self->mSwatchLightColor->get();
	LLColor3 clr3(clr);
	volobjp->setLightSRGBColor(clr3);
	self->mLightSavedColor = clr;
}

void LLPanelVolume::onLightSelectTexture(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (!self || !self->mObject ||
		self->mObject->getPCode() != LL_PCODE_VOLUME)
	{
		return;
	}
	LLVOVolume* volobjp = (LLVOVolume*)self->mObject.get();
	if (!volobjp) return;

	LLUUID id = self->mTextureLight->getImageAssetID();
	volobjp->setLightTextureID(id);
	self->mLightSavedTexture = id;
}

//static
bool LLPanelVolume::onDragTexture(LLUICtrl* ctrl, LLInventoryItem* item,
								  void* userdata)
{
	bool accept = true;
	for (LLObjectSelection::root_iterator
			iter = gSelectMgr.getSelection()->root_begin(),
			end = gSelectMgr.getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* obj = node->getObject();
		if (!LLToolDragAndDrop::isInventoryDropAcceptable(obj, item))
		{
			accept = false;
			break;
		}
	}
	return accept;
}

//static
void LLPanelVolume::onCommitMaterial(LLUICtrl* ctrl, void* userdata)
{
	if (!userdata || !ctrl)
	{
		return;
	}

	LLPanelVolume* self = (LLPanelVolume*)userdata;

	LLComboBox* combop = (LLComboBox*)ctrl;
	const std::string& material_name = combop->getSimple();
	if (material_name == self->mFullBright)
	{
		return;
	}

	// Apply the currently selected material to the object
	U8 mcode = gMaterialTable.getMCode(material_name);
	LLViewerObject* objectp = self->mObject;
	if (objectp)
	{
		objectp->setPhysicsGravity(DEFAULT_GRAVITY_MULTIPLIER);
		objectp->setPhysicsFriction(gMaterialTable.getFriction(mcode));
		// Currently density is always set to 1000 server side regardless of
		// chosen material, actual material density should be used here, if
		// this behavior changes.
		objectp->setPhysicsDensity(DEFAULT_DENSITY);
		objectp->setPhysicsRestitution(gMaterialTable.getRestitution(mcode));
	}
	gSelectMgr.selectionSetMaterial(mcode);
}

//static
void LLPanelVolume::onCommitLight(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (!self || !self->mObject ||
		self->mObject->getPCode() != LL_PCODE_VOLUME)
	{
		return;
	}
	LLVOVolume* volobjp = (LLVOVolume*)self->mObject.get();
	if (!volobjp) return;

	volobjp->setLightIntensity((F32)self->mSpinLightIntensity->getValue().asReal());
	volobjp->setLightRadius((F32)self->mSpinLightRadius->getValue().asReal());
	volobjp->setLightFalloff((F32)self->mSpinLightFalloff->getValue().asReal());

	LLColor4 clr = self->mSwatchLightColor->get();
	volobjp->setLightSRGBColor(LLColor3(clr));

	LLUUID id = self->mTextureLight->getImageAssetID();
	if (id.notNull())
	{
		if (!volobjp->isLightSpotlight())
		{
			// This commit is making this a spot light; set UI to
			// default params
			volobjp->setLightTextureID(id);
			LLVector3 spot_params = volobjp->getSpotLightParams();
			self->mSpinLightFOV->setValue(spot_params.mV[0]);
			self->mSpinLightFocus->setValue(spot_params.mV[1]);
			self->mSpinLightAmbiance->setValue(spot_params.mV[2]);
		}
		else
		{
			// Modifying existing params
			LLVector3 spot_params;
			spot_params.mV[0] = (F32)self->mSpinLightFOV->getValue().asReal();
			spot_params.mV[1] =
				(F32)self->mSpinLightFocus->getValue().asReal();
			spot_params.mV[2] =
				(F32)self->mSpinLightAmbiance->getValue().asReal();
			volobjp->setSpotLightParams(spot_params);
		}
	}
	else if (volobjp->isLightSpotlight())
	{
		// No longer a spot light
		volobjp->setLightTextureID(id);
#if 0
		self->mSpinLightFOV->setEnabled(false);
		self->mSpinLightFocus->setEnabled(false);
		self->mSpinLightAmbiance->setEnabled(false);
#endif
	}
}

//static
void LLPanelVolume::onCommitIsLight(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (self)
	{
		self->sendIsLight();
	}
}

//----------------------------------------------------------------------------

//static
void LLPanelVolume::onCommitFlexible(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (!self) return;
	LLViewerObject* objectp = self->mObject;
	if (!objectp || objectp->getPCode() != LL_PCODE_VOLUME)
	{
		return;
	}

	const LLFlexibleObjectData* params = objectp->getFlexibleObjectData();
	if (params)
	{
		LLFlexibleObjectData new_params(*params);
		new_params.setSimulateLOD(self->mSpinFlexSections->getValue().asInteger());
		new_params.setGravity((F32)self->mSpinFlexGravity->getValue().asReal());
		new_params.setAirFriction((F32)self->mSpinFlexFriction->getValue().asReal());
		new_params.setWindSensitivity((F32)self->mSpinFlexWind->getValue().asReal());
		new_params.setTension((F32)self->mSpinFlexTension->getValue().asReal());
		F32 fx = (F32)self->mSpinFlexForceX->getValue().asReal();
		F32 fy = (F32)self->mSpinFlexForceY->getValue().asReal();
		F32 fz = (F32)self->mSpinFlexForceZ->getValue().asReal();
		LLVector3 force(fx, fy, fz);

		new_params.setUserForce(force);
		objectp->setParameterEntry(LLNetworkData::PARAMS_FLEXIBLE, new_params, true);
	}

	// Values may fail validation
	self->refresh();
}

bool handleResponseChangeToFlexible(const LLSD& notification,
									const LLSD& response,
									LLPanelVolume* self)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (self && option == 0)
	{
		self->sendIsFlexible();
	}
	return false;
}

//static
void LLPanelVolume::onCommitIsFlexible(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (self && self->mObject)
	{
		if (self->mObject->flagObjectPermanent())
		{
			gNotifications.add("ChangeToFlexiblePath", LLSD(), LLSD(),
							   boost::bind(handleResponseChangeToFlexible,
										   _1, _2, self));
		}
		else
		{
			self->sendIsFlexible();
		}
	}
}

//static
void LLPanelVolume::onCommitAnimatedMesh(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	LLViewerObject* objectp = self->mObject;
	if (!objectp || objectp->getPCode() != LL_PCODE_VOLUME)
	{
		return;
	}

	LLVOVolume* volobjp = (LLVOVolume*)objectp;
	U32 flags = volobjp->getExtendedMeshFlags();
	U32 new_flags = flags;
	if (check->get())
	{
		new_flags |= LLExtendedMeshParams::ANIMATED_MESH_ENABLED_FLAG;
	}
	else
	{
		new_flags &= ~LLExtendedMeshParams::ANIMATED_MESH_ENABLED_FLAG;
	}
	if (new_flags != flags)
	{
		volobjp->setExtendedMeshFlags(new_flags);
	}

	// Refresh any bake on mesh texture
	volobjp->refreshBakeTexture();
	LLViewerObject::const_child_list_t& child_list = volobjp->getChildren();
	for (LLViewerObject::child_list_t::const_iterator
			it = child_list.begin(), end = child_list.end();
		 it != end; ++it)
	{
		LLViewerObject* childp = *it;
		if (childp)
		{
			childp->refreshBakeTexture();
		}
	}
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->updateMeshVisibility();
	}
}

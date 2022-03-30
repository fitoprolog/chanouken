/**
 * @file llviewerobjectbackup.cpp
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
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

#include "llviewerobjectbackup.h"

#include "imageids.h"
#include "llalertdialog.h"
#include "llcallbacklist.h"
#include "lldir.h"
#include "lleconomy.h"
#include "llfilesystem.h"
#include "llimagej2c.h"
#include "lllfsthread.h"
#include "llnotifications.h"
#include "llsdserialize.h"
#include "llsdutil_math.h"
#include "lltransactiontypes.h"
#include "lluictrlfactory.h"
#include "lluploaddialog.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llfloaterperms.h"
#include "llgridmanager.h"				// For gIsInSecondLife
#include "llinventorymodel.h"			// For gInventory
#include "llmaterialmgr.h"
#include "lltexturecache.h"
#include "lltoolplacer.h"
#include "llviewerassetupload.h"		// For upload_new_resource()
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llviewerobjectlist.h"
#include "llviewermenu.h"
#include "llviewerregion.h"

// Note: these default textures are initialized with hard coded values to
// prevent cheating. When not in SL, the user-configurable values are used
// instead (see setDefaultTextures() below).
LLUUID LL_TEXTURE_PLYWOOD        = LLUUID("89556747-24cb-43ed-920b-47caed15465f");
LLUUID LL_TEXTURE_BLANK          = LLUUID("5748decc-f629-461c-9a36-a35a221fe21f");
LLUUID LL_TEXTURE_INVISIBLE      = LLUUID("38b86f85-2575-52a9-a531-23108d8da837");
LLUUID LL_TEXTURE_TRANSPARENT    = LLUUID("8dcd4a48-2d37-4909-9f78-f7a9eb4ef903");
LLUUID LL_TEXTURE_MEDIA          = LLUUID("8b5fec65-8d8d-9dc5-cda8-8fdf2716e361");

class BackupCacheReadResponder final : public LLTextureCache::ReadResponder
{
protected:
	LOG_CLASS(BackupCacheReadResponder);

public:
	BackupCacheReadResponder(const LLUUID& id, LLImageFormatted* image)
	:	mFormattedImage(image), mID(id)
	{
		setImage(image);
	}

	void setData(U8* data, S32 datasize, S32 imagesize, S32 imageformat,
				 bool imagelocal) override
	{
		LLObjectBackup* self = LLObjectBackup::findInstance();
		if (!self) return;

		if (imageformat == IMG_CODEC_TGA &&
			mFormattedImage->getCodec() == IMG_CODEC_J2C)
		{
			llwarns << "FAILED: texture " << mID
					<< " is formatted as TGA. Not saving." << llendl;
			self->mNonExportedTextures |= LLObjectBackup::TEXTURE_BAD_ENCODING;
			mFormattedImage = NULL;
			mImageSize = 0;
			return;
		}

		if (mFormattedImage.notNull())
		{
			if (mFormattedImage->getCodec() == imageformat)
			{
				mFormattedImage->appendData(data, datasize);
			}
			else
			{
				llwarns << "FAILED: texture " << mID
						<< " is formatted as " << mFormattedImage->getCodec()
						<< " while expecting " << imageformat
						<< ". Not saving." << llendl;
				mFormattedImage = NULL;
				mImageSize = 0;
				return;
			}
		}
		else
		{
			mFormattedImage = LLImageFormatted::createFromType(imageformat);
			mFormattedImage->setData(data, datasize);
		}
		mImageSize = imagesize;
		mImageLocal = imagelocal;
	}

	void completed(bool success) override
	{
		LLObjectBackup* self = LLObjectBackup::findInstance();
		if (!self)
		{
			llwarns << "Export aborted, LLObjectBackup instance gone !"
					<< llendl;
			return;
		}

		if (success && mFormattedImage.notNull() && mImageSize > 0)
		{
			llinfos << "SUCCESS getting texture " << mID << llendl;
			std::string name;
			mID.toString(name);
			name = self->getFolder() + name;
			llinfos << "Saving to " << name << llendl;
			if (!mFormattedImage->save(name))
			{
				llwarns << "FAILED to save texture " << mID << llendl;
				self->mNonExportedTextures |= LLObjectBackup::TEXTURE_SAVED_FAILED;
			}
		}
		else
		{
			if (!success)
			{
				llwarns << "FAILED to get texture " << mID << llendl;
				self->mNonExportedTextures |= LLObjectBackup::TEXTURE_MISSING;
			}
			if (mFormattedImage.isNull())
			{
				llwarns << "FAILED: NULL texture " << mID << llendl;
				self->mNonExportedTextures |= LLObjectBackup::TEXTURE_IS_NULL;
			}
		}

		self->mCheckNextTexture = true;
	}

private:
	LLPointer<LLImageFormatted> mFormattedImage;
	LLUUID mID;
};

LLObjectBackup::LLObjectBackup(const LLSD&)
:	mRunning(false),
	mRetexture(false),
	mTexturesList(),
	mCurrentAsset()
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_object_backup.xml",
												 NULL, false);	// Do not open
}

//virtual
LLObjectBackup::~LLObjectBackup()
{
	// Just in case we got closed unexpectedly...
	gIdleCallbacks.deleteFunction(exportWorker);

	// Release the selection handle
	mSelectedObjects = NULL;
}

//static
bool LLObjectBackup::confirmCloseCallback(const LLSD& notification,
										  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLObjectBackup* self = findInstance();
		if (self)
		{
			self->destroy();
		}
	}
	return false;
}

//virtual
void LLObjectBackup::onClose(bool app_quitting)
{
	// Do not destroy the floater on user close action to avoid getting things
	// messed up during import/export.
	if (app_quitting)
	{
		destroy();
	}
	else
	{
		gNotifications.add("ConfirmAbortBackup", LLSD(), LLSD(),
						   confirmCloseCallback);
	}
}

void LLObjectBackup::showFloater(bool exporting)
{
	// Set the title
	setTitle(getString(exporting ? "export" : "import"));

	mCurObject = 1;
	mCurPrim = mObjects = mPrims = mRezCount = 0;

	// Make the floater pop up
	setVisibleAndFrontmost();
}

void LLObjectBackup::updateExportNumbers()
{
	std::stringstream sstr;
	LLUICtrl* ctrl = getChild<LLUICtrl>("name_label");

	sstr << "Export Progress \n";

	sstr << "Remaining Textures " << mTexturesList.size() << "\n";
	ctrl->setValue(LLSD("Text") = sstr.str());
}

void LLObjectBackup::updateImportNumbers()
{
	std::stringstream sstr;
	LLUICtrl* ctrl = getChild<LLUICtrl>("name_label");

	if (mRetexture)
	{
		sstr << " Textures uploads remaining : " << mTexturesList.size()
			 << "\n";
		ctrl->setValue(LLSD("Text") = sstr.str());
	}
	else
	{
		sstr << " Textures uploads N/A \n";
		ctrl->setValue(LLSD("Text") = sstr.str());
	}

	sstr << " Objects " << mCurObject << "/" << mObjects << "\n";
	ctrl->setValue(LLSD("Text") = sstr.str());

	sstr << " Rez " << mRezCount << "/" << mPrims;
	ctrl->setValue(LLSD("Text") = sstr.str());

	sstr << " Build " << mCurPrim << "/" << mPrims;
	ctrl->setValue(LLSD("Text") = sstr.str());
}

//static
void LLObjectBackup::exportCallback(HBFileSelector::ESaveFilter type,
									std::string& filename, void*)
{
	if (!filename.empty())
	{
		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
		if (!object || object->isDead())
		{
			gNotifications.add("ExportAborted");
			return;
		}
		LLObjectBackup* self = getInstance();
		if (self)
		{
			self->doExportObject(filename);
		}
	}
}

//static
void LLObjectBackup::exportObject()
{
	if (findInstance())
	{
		llwarns << "Backup operation already in progress !" << llendl;
		showInstance();
	}
	else
	{
		LLObjectSelectionHandle selection = gSelectMgr.getSelection();
		if (selection && selection->getFirstRootNode())
		{
			std::string suggestion;
			suggestion = selection->getFirstRootNode()->mName;
			suggestion = LLDir::getScrubbedFileName(suggestion) + ".xml";
			// Open the file save dialog
			HBFileSelector::saveFile(HBFileSelector::FFSAVE_XML, suggestion,
									 exportCallback);
		}
	}
}

void LLObjectBackup::doExportObject(std::string filename)
{
	mFileName = filename;
	mFolder = gDirUtilp->getDirName(mFileName) + LL_DIR_DELIM_STR;
	mTexturesList.clear();
	mBadPermsTexturesList.clear();
	mLLSD.clear();
	mThisGroup.clear();
	setDefaultTextures();
	mNonExportedTextures = TEXTURE_OK;
	mExportState = EXPORT_INIT;
	mSelectedObjects = gSelectMgr.getSelection();
	mGotExtraPhysics = gAgent.hasRegionCapability("GetObjectPhysicsData");
	gIdleCallbacks.addFunction(exportWorker);
}

//---------------------------------------------------------------------------//
// Permissions checking functions
//---------------------------------------------------------------------------//

//static
void LLObjectBackup::setDefaultTextures()
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!gIsInSecondLife && regionp && !regionp->isOSExportPermSupported())
	{
		// When not in SL and not in an OpenSIM grid with export permission
		// support (i.e. when no texture permission check is needed), we can
		// get these defaults from the user settings...
		LL_TEXTURE_PLYWOOD = LLUUID(gSavedSettings.getString("DefaultObjectTexture"));
		LL_TEXTURE_BLANK = LLUUID(gSavedSettings.getString("UIImgWhiteUUID"));
		LL_TEXTURE_INVISIBLE = LLUUID(gSavedSettings.getString("UIImgInvisibleUUID"));
	}
}

//static
bool LLObjectBackup::validatePerms(const LLPermissions* item_permissions,
								   bool strict)
{
	// Let's see if we have got export permission bit support
	LLViewerRegion* regionp = gAgent.getRegion();
	bool has_export_perm = regionp && regionp->isOSExportPermSupported();

	// By default, allow to export if the asset is full perm and owned by you.
	ExportPolicy policy = ep_full_perm;
	if (gIsInSecondLife || (strict && !has_export_perm))
	{
		// In Second Life you must be the creator to be permitted to export the
		// asset. This is also the case when asking for strict checking (i.e.
		// for mesh exporting or wearable textures viewing) in OpenSIM grids
		// without support for the export permission bit.
		policy = ep_creator_only;
	}
	else if (has_export_perm)
	{
		// In OpenSIM grids with support for export permission, rely on that
		// bit to allow/disallow the export.
		policy = ep_export_bit;
	}

	return item_permissions->allowExportBy(gAgentID, policy);
}

// So far, only Second Life forces TPVs to verify the creator for textures...
// which sucks, because there is no other way to check for the texture
// permissions or creator than to try and find the asset(s) corresponding to
// the texture in the inventory and check the permissions/creator on the said
// asset(s), meaning that if you created the texture and subsequently deleted
// it from your inventory, you will not be able to export it any more !!!
// The "must be creator" stuff also goes against the usage in Linden Lab's own
// official viewers, since those allow you to save full perm textures (such as
// the textures in the Library), whoever is the actual creator... Go figure !

//static
bool LLObjectBackup::validateTexturePerms(const LLUUID& asset_id, bool strict)
{
	if (!strict && !gIsInSecondLife)
	{
		LLViewerRegion* regionp = gAgent.getRegion();
		if (regionp && !regionp->isOSExportPermSupported())
		{
			// If we are not in Second Life, and this is not a strict checking
			// (used for wearables textures) and we do not have support for the
			// export permission, do not bother and unconditionally accept the
			// texture export (legacy behaviour).
			return true;
		}
	}

	if (asset_id == LL_TEXTURE_PLYWOOD ||
		asset_id == LL_TEXTURE_BLANK ||
		asset_id == LL_TEXTURE_INVISIBLE ||
		asset_id == LL_TEXTURE_TRANSPARENT ||
		asset_id == LL_TEXTURE_MEDIA ||
		asset_id == IMG_DEFAULT)
	{
		// Allow to export a few default SL textures.
		return true;
	}

	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLAssetIDMatches asset_id_matches(asset_id);
	gInventory.collectDescendentsIf(LLUUID::null, cats, items,
									LLInventoryModel::INCLUDE_TRASH,
									asset_id_matches);
	for (S32 i = 0, count = items.size(); i < count; ++i)
	{
		const LLPermissions item_permissions = items[i]->getPermissions();
		if (validatePerms(&item_permissions, strict))
		{
			return true;
		}
	}

	return false;
}

LLUUID LLObjectBackup::validateTextureID(const LLUUID& asset_id)
{
	if (mBadPermsTexturesList.count(asset_id))
	{
		// We already checked it and know it's bad...
		return LL_TEXTURE_PLYWOOD;
	}
	else if (asset_id.isNull() || validateTexturePerms(asset_id))
	{
		return asset_id;
	}
	else
	{
		mBadPermsTexturesList.emplace(asset_id);	// Cache bad texture Id
		mNonExportedTextures |= TEXTURE_BAD_PERM;
		llwarns << "Bad permissions for texture Id: " << asset_id
				<< " - Texture will not be exported." << llendl;
		return LL_TEXTURE_PLYWOOD;
	}
}

//static
bool LLObjectBackup::validateNode(LLSelectNode* node)
{
	LLPermissions* perms = node->mPermissions;
	if (!perms || !validatePerms(perms))
	{
		return false;
	}

	// Additionally check if this is a sculpt or a mesh object and if yes, if
	// we have export permission on the sclupt texture or the mesh object.
	LLViewerObject* obj = node->getObject();
	if (!obj)	// Paranoia
	{
		return false;
	}
	else if (obj->isSculpted())
	{
		if (obj->isMesh())
		{
			return validatePerms(perms, true);
		}
		else
		{
			const LLSculptParams* params = obj->getSculptParams();
			if (params)
			{
				const LLUUID& sculpt_id = params->getSculptTexture();
				return validateTexturePerms(sculpt_id);
			}
		}
	}

	return true;
}

//---------------------------------------------------------------------------//

//static
void LLObjectBackup::exportWorker(void*)
{
	LLObjectBackup* self = findInstance();
	if (!self)
	{
		gIdleCallbacks.deleteFunction(exportWorker);
		llwarns << "Export process aborted. LLObjectBackup instance gone !"
				<< llendl;
		gNotifications.add("ExportAborted");
		return;
	}

	self->updateExportNumbers();

	switch (self->mExportState)
	{
		case EXPORT_INIT:
		{
			self->showFloater(true);
			// Fall through to EXPORT_CHECK_PERMS
		}
		case EXPORT_CHECK_PERMS:
		{
			struct ff final : public LLSelectedNodeFunctor
			{
				bool apply(LLSelectNode* node) override
				{
					return LLObjectBackup::validateNode(node);
				}
			} func;

			LLViewerObject* object;
			object = self->mSelectedObjects->getPrimaryObject();
			if (object)
			{
				if (self->mSelectedObjects->applyToNodes(&func, false))
				{
					self->mExportState = EXPORT_FETCH_PHYSICS;
				}
				else
				{
					struct vv final : public LLSelectedNodeFunctor
					{
						bool apply(LLSelectNode* node) override
						{
							return node->mValid;
						}
					} func2;

					if (self->mSelectedObjects->applyToNodes(&func2, false))
					{
						llwarns << "Incorrect permission to export" << llendl;
						self->mExportState = EXPORT_FAILED;
					}
					else
					{
						LL_DEBUGS("ObjectBackup") << "Nodes permissions not yet received, delaying..."
												  << LL_ENDL;
						self->mExportState = EXPORT_CHECK_PERMS;
					}
				}
			}
			else
			{
				self->mExportState = EXPORT_ABORTED;
			}
			break;
		}

		case EXPORT_FETCH_PHYSICS:
		{
			// Do not bother to try and fetch the extra physics flags if we
			// do not have sim support for them...
			if (!self->mGotExtraPhysics)
			{
				self->mExportState = EXPORT_STRUCTURE;
				break;
			}

			struct ff final : public LLSelectedNodeFunctor
			{
				bool apply(LLSelectNode* node) override
				{
					LLViewerObject* object = node->getObject();
					return gObjectList.gotObjectPhysicsFlags(object);
				}
			} func;

			LLViewerObject* object = self->mSelectedObjects->getPrimaryObject();
			if (object)
			{
				if (self->mSelectedObjects->applyToNodes(&func, false))
				{
					self->mExportState = EXPORT_STRUCTURE;
				}
				else
				{
					LL_DEBUGS("ObjectBackup") << "Nodes physics not yet received, delaying..."
											  << LL_ENDL;
				}
			}
			else
			{
				self->mExportState = EXPORT_ABORTED;
			}
			break;
		}

		case EXPORT_STRUCTURE:
		{
			struct ff final : public LLSelectedObjectFunctor
			{
				bool apply(LLViewerObject* object) override
				{
					bool is_attachment = object->isAttachment();
					object->boostTexturePriority(true);
					LLViewerObject::child_list_t children =
						object->getChildren();
					children.push_front(object); // Push root onto list
					LLObjectBackup* self = findInstance();
					LLSD prim_llsd = self->primsToLLSD(children,
													   is_attachment);
					LLSD stuff;
					if (is_attachment)
					{
						stuff["root_position"] =
							object->getPositionEdit().getValue();
						stuff["root_rotation"] =
							ll_sd_from_quaternion(object->getRotationEdit());
					}
					else
					{
						stuff["root_position"] =
							object->getPosition().getValue();
						stuff["root_rotation"] =
							ll_sd_from_quaternion(object->getRotation());
					}
					stuff["group_body"] = prim_llsd;
					self->mLLSD["data"].append(stuff);
					return true;
				}
			} func;

			LLViewerObject* object;
			object = self->mSelectedObjects->getPrimaryObject();
			if (object)
			{
				self->mExportState = EXPORT_LLSD;
				self->mSelectedObjects->applyToRootObjects(&func, false);
			}
			else
			{
				self->mExportState = EXPORT_ABORTED;
			}
			break;
		}

		case EXPORT_TEXTURES:
		{
			if (!self->mCheckNextTexture)
			{
				// The texture is being fetched. Wait till next idle callback.
				return;
			}

			if (self->mTexturesList.empty())
			{
				self->mExportState = EXPORT_DONE;
				return;
			}

			// Ok, we got work to do...
			self->mCheckNextTexture = false;
			self->exportNextTexture();
			break;
		}

		case EXPORT_LLSD:
		{
			// Create a file stream and write to it
			llofstream export_file(self->mFileName.c_str());
			if (export_file.is_open())
			{
				LLSDSerialize::toPrettyXML(self->mLLSD, export_file);
				export_file.close();
				self->mCheckNextTexture = true;
				self->mExportState = EXPORT_TEXTURES;
			}
			else
			{
				llwarns << "Could not open file '" << self->mFileName
						<< "' for writing." << llendl;
				self->mExportState = EXPORT_FAILED;
			}
			break;
		}

		case EXPORT_DONE:
		{
			gIdleCallbacks.deleteFunction(exportWorker);
			if (self->mNonExportedTextures == LLObjectBackup::TEXTURE_OK)
			{
				llinfos << "Export successful and complete." << llendl;
				gNotifications.add("ExportSuccessful");
			}
			else
			{
				llinfos << "Export successful but incomplete: some texture(s) not saved."
						<< llendl;
				// *TODO: translate
				std::string reason;
				U32 error_bits_map = self->mNonExportedTextures;
				if (error_bits_map & LLObjectBackup::TEXTURE_BAD_PERM)
				{
					reason += "\nBad permissions/creator.";
				}
				if (error_bits_map & LLObjectBackup::TEXTURE_MISSING)
				{
					reason += "\nMissing texture (retrying after full rezzing might work).";
				}
				if (error_bits_map & LLObjectBackup::TEXTURE_BAD_ENCODING)
				{
					reason += "\nBad texture encoding.";
				}
				if (error_bits_map & LLObjectBackup::TEXTURE_IS_NULL)
				{
					reason += "\nNull texture.";
				}
				if (error_bits_map & LLObjectBackup::TEXTURE_SAVED_FAILED)
				{
					reason += "\nCould not write to disk.";
				}
				LLSD args;
				args["REASON"] = reason;
				gNotifications.add("ExportPartial", args);
			}
			self->destroy();
			break;
		}

		case EXPORT_FAILED:
		{
			gIdleCallbacks.deleteFunction(exportWorker);
			llwarns << "Export process failed." << llendl;
			gNotifications.add("ExportFailed");
			self->destroy();
			break;
		}

		case EXPORT_ABORTED:
		{
			gIdleCallbacks.deleteFunction(exportWorker);
			llwarns << "Export process aborted." << llendl;
			gNotifications.add("ExportAborted");
			self->destroy();
			break;
		}
	}
}

LLSD LLObjectBackup::primsToLLSD(LLViewerObject::child_list_t child_list,
								 bool is_attachment)
{
	LLSD llsd;
	char localid[16];
	LLUUID t_id;

	for (LLViewerObject::child_list_t::iterator i = child_list.begin();
		 i != child_list.end(); ++i)
	{
		LLViewerObject* object = i->get();
		if (!object || object->isDead())
		{
			continue;
		}

		llinfos << "Exporting prim " << object->getID() << llendl;

		// Create an LLSD object that represents this prim. It will be injected
		// in to the overall LLSD tree structure
		LLSD prim_llsd;

		if (!object->isRoot())
		{
			// Parent id
			snprintf(localid, sizeof(localid), "%u",
					 object->getSubParent()->getLocalID());
			prim_llsd["parent"] = localid;
		}

		// Name and description
		LLSelectNode* node = mSelectedObjects->findNode(object);
		if (node)
		{
			prim_llsd["name"] = node->mName;
			prim_llsd["description"] = node->mDescription;
		}

		// Transforms
		if (is_attachment)
		{
			prim_llsd["position"] = object->getPositionEdit().getValue();
			prim_llsd["rotation"] =
				ll_sd_from_quaternion(object->getRotationEdit());
		}
		else
		{
			prim_llsd["position"] = object->getPosition().getValue();
			prim_llsd["rotation"] =
				ll_sd_from_quaternion(object->getRotation());
		}
		prim_llsd["scale"] = object->getScale().getValue();

		// Flags
		prim_llsd["phantom"] = object->flagPhantom();		// legacy
		prim_llsd["physical"] = object->flagUsePhysics();	// legacy
		prim_llsd["flags"] = (S32)object->getFlags();		// new way

		// Extra physics flags
		if (mGotExtraPhysics)
		{
			LLSD& physics = prim_llsd["ExtraPhysics"];
			physics["PhysicsShapeType"] = object->getPhysicsShapeType();
			physics["Gravity"] = object->getPhysicsGravity();
			physics["Friction"] = object->getPhysicsFriction();
			physics["Density"] = object->getPhysicsDensity();
			physics["Restitution"] = object->getPhysicsRestitution();
		}

		// Click action
		prim_llsd["clickaction"] = object->getClickAction();

		// Prim material
		prim_llsd["material"] = object->getMaterial();

		// Volume params
		LLVolumeParams params = object->getVolume()->getParams();
		prim_llsd["volume"] = params.asLLSD();

		// Extra params

		// Flexible
		if (object->isFlexible())
		{
			const LLFlexibleObjectData* data = object->getFlexibleObjectData();
			if (data)
			{
				prim_llsd["flexible"] = data->asLLSD();
			}
		}

		// Light
		const LLLightParams* light_params = object->getLightParams();
		if (light_params)
		{
			prim_llsd["light"] = light_params->asLLSD();
		}

		// Light image
		const LLLightImageParams* light_img = object->getLightImageParams();
		if (light_img)
		{
			t_id = validateTextureID(light_img->getLightTexture());
			if (mTexturesList.count(t_id) == 0)
			{
				llinfos << "Found a light texture, adding to list " << t_id
						<< llendl;
				mTexturesList.emplace(t_id);
			}
			prim_llsd["light_texture"] = light_img->asLLSD();
		}

		// Sculpt
		const LLSculptParams* sculpt = object->getSculptParams();
		if (sculpt)
		{
			prim_llsd["sculpt"] = sculpt->asLLSD();
			if ((sculpt->getSculptType() & LL_SCULPT_TYPE_MASK) !=
					LL_SCULPT_TYPE_MESH)
			{
				LLUUID sculpt_texture = sculpt->getSculptTexture();
				if (sculpt_texture == validateTextureID(sculpt_texture))
				{
					if (mTexturesList.count(sculpt_texture) == 0)
					{
						llinfos << "Found a sculpt texture, adding to list "
								<< sculpt_texture << llendl;
						mTexturesList.emplace(sculpt_texture);
					}
				}
				else
				{
					llwarns << "Incorrect permission to export a sculpt texture."
							<< llendl;
					mExportState = EXPORT_FAILED;
				}
			}
		}

		// Textures and materials
		LLSD te_llsd;
		LLSD this_te_llsd;
		LLSD te_mat_llsd;
		LLSD this_te_mat_llsd;
		bool has_materials = false;
		for (U8 i = 0, count = object->getNumTEs(); i < count; ++i)
		{
			LLTextureEntry* te = object->getTE(i);
			if (!te) continue;	// Paranoia

			// Normal texture/diffuse map
			t_id = validateTextureID(te->getID());
			this_te_llsd = te->asLLSD();
			this_te_llsd["imageid"] = t_id;
			te_llsd.append(this_te_llsd);
			// Do not export non-existent default textures
			if (t_id != LL_TEXTURE_BLANK && t_id != LL_TEXTURE_INVISIBLE)
 			{
				if (mTexturesList.count(t_id) == 0)
				{
					mTexturesList.emplace(t_id);
				}
			}

			// Materials
			LLMaterial* mat = te->getMaterialParams().get();
			if (mat)
			{
				has_materials = true;
				this_te_mat_llsd = mat->asLLSD();

				t_id = validateTextureID(mat->getNormalID());
				this_te_mat_llsd["NormMap"] = t_id;
				if (mTexturesList.count(t_id) == 0)
				{
					mTexturesList.emplace(t_id);
				}

				t_id = validateTextureID(mat->getSpecularID());
				this_te_mat_llsd["SpecMap"] = t_id;
				if (mTexturesList.count(t_id) == 0)
				{
					mTexturesList.emplace(t_id);
				}

				te_mat_llsd.append(this_te_mat_llsd);
			}
		}
		prim_llsd["textures"] = te_llsd;
		if (has_materials)
		{
			prim_llsd["materials"] = te_mat_llsd;
		}

		// The keys in the primitive maps do not have to be localids, they can
		// be any string. We simply use localids because they are a unique
		// identifier
		snprintf(localid, sizeof(localid), "%u", object->getLocalID());
		llsd[(const char*)localid] = prim_llsd;
	}

	updateExportNumbers();

	return llsd;
}

void LLObjectBackup::exportNextTexture()
{
	LLUUID id;
	uuid_list_t::iterator iter = mTexturesList.begin();
	while (true)
	{
		if (mTexturesList.empty())
		{
			mCheckNextTexture = true;
			llinfos << "Finished exporting textures." << llendl;
			return;
		}
		if (iter == mTexturesList.end())
		{
			// Not yet ready, wait and re-check at next idle callback...
			mCheckNextTexture = true;
			return;
		}

		id = *iter++;
		if (id.isNull())
		{
			// NULL texture id: just remove and ignore.
			mTexturesList.erase(id);
			LL_DEBUGS("ObjectBackup") << "Null texture UUID found, ignoring."
									  << LL_ENDL;
			continue;
		}

		LLViewerTexture* imagep = LLViewerTextureManager::findTexture(id);
		if (imagep)
		{
			if (imagep->getDiscardLevel() > 0)
			{
				// Boost texture loading
				imagep->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
				LL_DEBUGS("ObjectBackup") << "Boosting texture: " << id
										  << LL_ENDL;
				LLViewerFetchedTexture* tex;
				tex = LLViewerTextureManager::staticCastToFetchedTexture(imagep);
				if (tex && tex->getDesiredDiscardLevel() > 0)
				{
					// Set min discard level to 0
					tex->setMinDiscardLevel(0);
					LL_DEBUGS("ObjectBackup") << "Min discard level set to 0 for texture: "
											  << id << LL_ENDL;
				}
			}
			else
			{
				// Texture is ready !
				break;
			}
		}
		else
		{
			llwarns << "We *DO NOT* have the texture " << id << llendl;
			mNonExportedTextures |= TEXTURE_MISSING;
			mTexturesList.erase(id);
		}
	}

	mTexturesList.erase(id);

	llinfos << "Requesting texture " << id << " from cache." << llendl;
	LLImageJ2C* mFormattedImage = new LLImageJ2C;
	BackupCacheReadResponder* responder;
	responder = new BackupCacheReadResponder(id, mFormattedImage);
  	gTextureCachep->readFromCache(id, LLWorkerThread::PRIORITY_HIGH, 0, 999999,
								  responder);
}

//static
void LLObjectBackup::importCallback(HBFileSelector::ELoadFilter type,
									std::string& filename, void* data)
{
	if (!filename.empty())
	{
		LLObjectBackup* self = getInstance();
		if (self)
		{
			self->mRetexture = (bool)(intptr_t)data;
			self->doImportObject(filename);
		}
	}
}

//static
void LLObjectBackup::importObject(bool upload)
{
	if (findInstance())
	{
		llwarns << "Backup operation already in progress !" << llendl;
		showInstance();
	}
	else
	{
		HBFileSelector::loadFile(HBFileSelector::FFLOAD_XML, importCallback,
								 (void*)upload);
	}
}

bool is_default_texture(const LLUUID& id)
{
	return id.isNull() || id == LL_TEXTURE_PLYWOOD || id == LL_TEXTURE_BLANK ||
		   id == LL_TEXTURE_INVISIBLE;
}

void LLObjectBackup::doImportObject(std::string filename)
{
	mTexturesList.clear();
	mAssetMap.clear();
	mCurrentAsset.setNull();

	mGotExtraPhysics = gAgent.hasRegionCapability("GetObjectPhysicsData");

	setDefaultTextures();

	mFolder = gDirUtilp->getDirName(filename) + LL_DIR_DELIM_STR;
	llifstream import_file(filename.c_str());
	bool success = import_file.is_open();
	if (success)
	{
		LLSDSerialize::fromXML(mLLSD, import_file);
		import_file.close();
		success = mLLSD.has("data");
	}
	else
	{
		llwarns << "Could not open file '" << filename << "' for reading."
				<< llendl;
	}
	if (!success)
	{
		gNotifications.add("ImportFailed");
		destroy();
		return;
	}

	showFloater(false);

	mAgentPos = gAgent.getPositionAgent();
	mAgentRot = LLQuaternion(gAgent.getAtAxis(), gAgent.getLeftAxis(),
							 gAgent.getUpAxis());

	// Get the texture map

	mCurObject = 1;
	mCurPrim = 1;
	mObjects = mLLSD["data"].size();
	mPrims = 0;
	mRezCount = 0;
	updateImportNumbers();

	for (LLSD::array_const_iterator prim_arr_it = mLLSD["data"].beginArray(),
									prim_arr_end = mLLSD["data"].endArray();
		 prim_arr_it != prim_arr_end; ++prim_arr_it)
	{
		LLSD llsd2 = (*prim_arr_it)["group_body"];

		for (LLSD::map_const_iterator prim_it = llsd2.beginMap(),
									  prim_end = llsd2.endMap();
			 prim_it != prim_end; ++prim_it)
		{
			LLSD prim_llsd = llsd2[prim_it->first];
			if (prim_llsd.has("sculpt"))
			{
				LLSculptParams sculpt;
				sculpt.fromLLSD(prim_llsd["sculpt"]);
				if ((sculpt.getSculptType() & LL_SCULPT_TYPE_MASK) !=
						LL_SCULPT_TYPE_MESH)
				{
					LLUUID orig = sculpt.getSculptTexture();
					if (mTexturesList.count(orig) == 0)
					{
						llinfos << "Found a new SCULPT texture to upload "
								<< orig << llendl;
						mTexturesList.emplace(orig);
					}
				}
			}

			if (prim_llsd.has("light_texture"))
			{
				LLLightImageParams lightimg;
				lightimg.fromLLSD(prim_llsd["light_texture"]);
				const LLUUID& t_id = lightimg.getLightTexture();
				if (!is_default_texture(t_id) &&
					mTexturesList.count(t_id) == 0)
				{
					llinfos << "Found a new light texture to upload: " << t_id
							<< llendl;
					mTexturesList.emplace(t_id);
				}
			}

			// Check both for "textures" and "texture" since the second (buggy)
			// case has already been seen in some exported prims XML files...
			LLSD te_llsd = prim_llsd.has("textures") ? prim_llsd["textures"]
													 : prim_llsd["texture"];
			for (LLSD::array_iterator it = te_llsd.beginArray();
				 it != te_llsd.endArray(); ++it)
			{
				LLSD the_te = *it;
				LLTextureEntry te;
				te.fromLLSD(the_te);

				LLUUID t_id = te.getID();
				if (!is_default_texture(t_id) &&
					mTexturesList.count(t_id) == 0)
				{
					llinfos << "Found a new texture to upload: " << t_id
							<< llendl;
					mTexturesList.emplace(t_id);
				}
			}

			if (prim_llsd.has("materials"))
			{
				LLSD mat_llsd = prim_llsd["materials"];
				for (LLSD::array_iterator it = mat_llsd.beginArray();
					 it != mat_llsd.endArray(); ++it)
				{
					LLSD the_mat = *it;
					LLMaterial mat;
					mat.fromLLSD(the_mat);

					LLUUID t_id = mat.getNormalID();
					if (!is_default_texture(t_id) &&
						mTexturesList.count(t_id) == 0)
 					{
						llinfos << "Found a new normal map to upload: "
								<< t_id << llendl;
						mTexturesList.emplace(t_id);
					}

					t_id = mat.getSpecularID();
					if (!is_default_texture(t_id) &&
						mTexturesList.count(t_id) == 0)
 					{
						llinfos << "Found a new specular map to upload: "
								<< t_id << llendl;
						mTexturesList.emplace(t_id);
					}
				}
			}
		}
	}

	if (mRetexture)
	{
		uploadNextAsset();
	}
	else
	{
		importFirstObject();
	}
}

LLVector3 LLObjectBackup::offsetAgent(LLVector3 offset)
{
	return offset * mAgentRot + mAgentPos;
}

void LLObjectBackup::rezAgentOffset(LLVector3 offset)
{
	// This will break for a sitting agent
	LLToolPlacer mPlacer;
	mPlacer.setObjectType(LL_PCODE_CUBE);
	mPlacer.placeObject((S32)offset.mV[0], (S32)offset.mV[1], MASK_NONE);
}

void LLObjectBackup::importFirstObject()
{
	mRunning = true;
	showFloater(false);
	mGroupPrimImportIter = mLLSD["data"].beginArray();
	mRootRootPos = LLVector3((*mGroupPrimImportIter)["root_position"]);
	mObjects = mLLSD["data"].size();
	mCurObject = 1;
	importNextObject();
}

void LLObjectBackup::importNextObject()
{
	mToSelect.clear();
	mRezCount = 0;

	mThisGroup = (*mGroupPrimImportIter)["group_body"];
	mPrimImportIter = mThisGroup.beginMap();

	mCurPrim = 0;
	mPrims = mThisGroup.size();
	updateImportNumbers();

	LLVector3 lgpos = LLVector3((*mGroupPrimImportIter)["root_position"]);
	mGroupOffset = lgpos - mRootRootPos;
	mRootPos = offsetAgent(LLVector3(2.f, 0.f, 0.f));
	mRootRot = ll_quaternion_from_sd((*mGroupPrimImportIter)["root_rotation"]);

	rezAgentOffset(LLVector3(0.f, 2.f, 0.f));
	// Now we must wait for the callback when ViewerObjectList gets the new
	// objects and we have the correct number selected
}

// This function takes a pointer to a viewerobject and applies the prim
// definition that prim_llsd has
void LLObjectBackup::xmlToPrim(LLSD prim_llsd, LLViewerObject* object)
{
	const LLUUID& id = object->getID();
	mExpectingUpdate = id;
	gSelectMgr.selectObjectAndFamily(object);

	if (prim_llsd.has("name"))
	{
		gSelectMgr.selectionSetObjectName(prim_llsd["name"]);
	}

	if (prim_llsd.has("description"))
	{
		gSelectMgr.selectionSetObjectDescription(prim_llsd["description"]);
	}

	if (prim_llsd.has("material"))
	{
		gSelectMgr.selectionSetMaterial(prim_llsd["material"].asInteger());
	}

	if (prim_llsd.has("clickaction"))
	{
		gSelectMgr.selectionSetClickAction(prim_llsd["clickaction"].asInteger());
	}

	if (prim_llsd.has("parent"))
	{
		//we are not the root node.
		LLVector3 pos = LLVector3(prim_llsd["position"]);
		LLQuaternion rot = ll_quaternion_from_sd(prim_llsd["rotation"]);
		object->setPositionRegion(pos * mRootRot + mRootPos + mGroupOffset);
		object->setRotation(rot * mRootRot);
	}
	else
	{
		object->setPositionRegion(mRootPos + mGroupOffset);
		LLQuaternion rot=ll_quaternion_from_sd(prim_llsd["rotation"]);
		object->setRotation(rot);
	}

	object->setScale(LLVector3(prim_llsd["scale"]));

	if (prim_llsd.has("flags"))
	{
		U32 flags = (U32)prim_llsd["flags"].asInteger();
		object->setFlags(flags, true);
	}
	else	// Kept for backward compatibility
	{
		if (prim_llsd.has("phantom") && prim_llsd["phantom"].asInteger() == 1)
		{
			object->setFlags(FLAGS_PHANTOM, true);
		}

		if (prim_llsd.has("physical") &&
			prim_llsd["physical"].asInteger() == 1)
		{
			object->setFlags(FLAGS_USE_PHYSICS, true);
		}
	}

	if (mGotExtraPhysics && prim_llsd.has("ExtraPhysics"))
	{
		const LLSD& physics = prim_llsd["ExtraPhysics"];
		object->setPhysicsShapeType(physics["PhysicsShapeType"].asInteger());
		F32 gravity = physics.has("Gravity") ? physics["Gravity"].asReal()
											 : physics["GravityMultiplier"].asReal();
		object->setPhysicsGravity(gravity);
		object->setPhysicsFriction(physics["Friction"].asReal());
		object->setPhysicsDensity(physics["Density"].asReal());
		object->setPhysicsRestitution(physics["Restitution"].asReal());
		object->updateFlags(true);
	}

	// Volume params
	LLVolumeParams volume_params = object->getVolume()->getParams();
	volume_params.fromLLSD(prim_llsd["volume"]);
	object->updateVolume(volume_params);

	if (prim_llsd.has("sculpt"))
	{
		LLSculptParams sculpt;
		sculpt.fromLLSD(prim_llsd["sculpt"]);

		// *TODO: check if map is valid and only set texture if map is valid
		// and changes
		const LLUUID& t_id = sculpt.getSculptTexture();
		rebase_map_t::const_iterator it = mAssetMap.find(t_id);
		if (it != mAssetMap.end())
		{
			sculpt.setSculptTexture(it->second, LL_SCULPT_TYPE_MESH);
		}

		object->setParameterEntry(LLNetworkData::PARAMS_SCULPT, sculpt, true);
	}

	if (prim_llsd.has("light"))
	{
		LLLightParams light;
		light.fromLLSD(prim_llsd["light"]);
		object->setParameterEntry(LLNetworkData::PARAMS_LIGHT, light, true);
	}
	if (prim_llsd.has("light_texture"))
	{
		// Light image
		LLLightImageParams lightimg;
		lightimg.fromLLSD(prim_llsd["light_texture"]);
		const LLUUID& t_id = lightimg.getLightTexture();
		rebase_map_t::const_iterator it = mAssetMap.find(t_id);
		if (it != mAssetMap.end())
		{
			lightimg.setLightTexture(it->second);
		}
		object->setParameterEntry(LLNetworkData::PARAMS_LIGHT_IMAGE, lightimg,
								  true);
	}

	if (prim_llsd.has("flexible"))
	{
		LLFlexibleObjectData flex;
		flex.fromLLSD(prim_llsd["flexible"]);
		object->setParameterEntry(LLNetworkData::PARAMS_FLEXIBLE, flex, true);
	}

	// Textures
	// Check both for "textures" and "texture" since the second (buggy) case
	// has already been seen in some exported prims XML files...
	llinfos << "Processing textures for prim " << id << llendl;
	LLSD te_llsd = prim_llsd.has("textures") ? prim_llsd["textures"]
											 : prim_llsd["texture"];
	U8 i = 0;
	for (LLSD::array_iterator it = te_llsd.beginArray();
		 it != te_llsd.endArray(); ++it)
	{
	    LLSD the_te = *it;
	    LLTextureEntry te;
	    te.fromLLSD(the_te);
		const LLUUID& t_id = te.getID();
		rebase_map_t::const_iterator tit = mAssetMap.find(t_id);
		if (tit != mAssetMap.end())
		{
			te.setID(tit->second);
		}

	    object->setTE(i++, te);
	}
	llinfos << "Textures done !" << llendl;

	// Materials
	if (prim_llsd.has("materials"))
	{
		llinfos << "Processing materials for prim " << id << llendl;
		te_llsd = prim_llsd["materials"];
		i = 0;
		for (LLSD::array_iterator it = te_llsd.beginArray();
			 it != te_llsd.endArray(); ++it)
		{
		    LLSD the_mat = *it;
			LLMaterialPtr mat = new LLMaterial(the_mat);

			const LLUUID& n_id = mat->getNormalID();
			if (n_id.notNull())
			{
				rebase_map_t::const_iterator tit = mAssetMap.find(n_id);
				if (tit != mAssetMap.end())
				{
					mat->setNormalID(tit->second);
				}
			}

			const LLUUID& s_id = mat->getSpecularID();
			if (s_id.notNull())
			{
				rebase_map_t::const_iterator tit = mAssetMap.find(n_id);
				if (tit != mAssetMap.end())
				{
					mat->setSpecularID(tit->second);
				}
			}

			LLMaterialMgr::getInstance()->put(id, i++, *mat);
		}
		llinfos << "Materials done !" << llendl;
	}

	object->sendTEUpdate();
	object->sendShapeUpdate();

	// There is a server bug preventing to update the scale, position and
	// rotation at once...
	static LLCachedControl<bool> multiple_update_bug(gSavedSettings,
													 "MultipleUpdateBug");
	if (multiple_update_bug)
	{
		gSelectMgr.sendMultipleUpdate(UPD_SCALE);
		gSelectMgr.sendMultipleUpdate(UPD_POSITION | UPD_ROTATION);
	}
	else
	{
		gSelectMgr.sendMultipleUpdate(UPD_SCALE | UPD_POSITION | UPD_ROTATION);
	}

	gSelectMgr.deselectAll();
}

// This is fired when the update packet is processed so we know the prim
// settings have stuck
//static
void LLObjectBackup::primUpdate(LLViewerObject* object)
{
	LLObjectBackup* self = findInstance();
	if (!object || object->isDead() || !self || !self->mRunning ||
		object->mID != self->mExpectingUpdate)
	{
		return;
	}

	++self->mCurPrim;
	self->updateImportNumbers();
	++self->mPrimImportIter;

	self->mExpectingUpdate.setNull();

	if (self->mPrimImportIter == self->mThisGroup.endMap())
	{
		llinfos << "Trying to link..." << llendl;

		if (self->mToSelect.size() > 1)
		{
			std::reverse(self->mToSelect.begin(), self->mToSelect.end());
			// Now link
			gSelectMgr.deselectAll();
			gSelectMgr.selectObjectAndFamily(self->mToSelect, true);
			gSelectMgr.sendLink();
			LLViewerObject* root = self->mToSelect.back();
			root->setRotation(self->mRootRot);
		}

		++self->mCurObject;
		++self->mGroupPrimImportIter;
		if (self->mGroupPrimImportIter != self->mLLSD["data"].endArray())
		{
			self->importNextObject();
			return;
		}

		self->mRunning = false;
		self->destroy();
		return;
	}

	LLSD prim_llsd = self->mThisGroup[self->mPrimImportIter->first];

	if (self->mToSelect.empty())
	{
		llwarns << "error: ran out of objects to mod." << llendl;
		self->mRunning = false;
		self->destroy();
		return;
	}

	if (self->mPrimImportIter != self->mThisGroup.endMap())
	{
		//rezAgentOffset(LLVector3(1.f, 0.f, 0.f));
		LLSD prim_llsd =self-> mThisGroup[self->mPrimImportIter->first];
		++self->mProcessIter;
		self->xmlToPrim(prim_llsd, *(self->mProcessIter));
	}
}

// Callback when we rez a new object when the importer is running.
//static
void LLObjectBackup::newPrim(LLViewerObject* object)
{
	LLObjectBackup* self = findInstance();
	if (!object || object->isDead() || !self || !self->mRunning) return;

	++self->mRezCount;
	self->mToSelect.push_back(object);
	self->updateImportNumbers();
	++self->mPrimImportIter;

	object->setPositionLocal(self->offsetAgent(LLVector3(0.f, 1.f, 0.f)));
	gSelectMgr.sendMultipleUpdate(UPD_POSITION);

	if (self->mPrimImportIter != self->mThisGroup.endMap())
	{
		self->rezAgentOffset(LLVector3(1.f, 0.f ,0.f));
	}
	else
	{
		llinfos << "All prims rezzed, moving to build stage" << llendl;
		// Deselecting is required to ensure that the first child prim in
		// the link set (which is also the last rezzed prim and thus
		// currently selected) will be properly renamed and desced.
		gSelectMgr.deselectAll();
		self->mPrimImportIter = self->mThisGroup.beginMap();
		LLSD prim_llsd = self->mThisGroup[self->mPrimImportIter->first];
		self->mProcessIter = self->mToSelect.begin();
		self->xmlToPrim(prim_llsd, *(self->mProcessIter));
	}
}

void LLObjectBackup::updateMap(const LLUUID& uploaded_asset)
{
	if (mCurrentAsset.notNull())
	{
		llinfos << "Mapping " << mCurrentAsset << " to " << uploaded_asset
				<< llendl;
		mAssetMap.emplace(mCurrentAsset, uploaded_asset);
	}
}

void LLObjectBackup::uploadNextAsset()
{
	if (gAgent.getRegionCapability("NewFileAgentInventory").empty() &&
		!mTexturesList.empty())
	{
		llwarns << "NewAgentInventory capability not found. Cannot upload !"
				<< llendl;
		mTexturesList.clear();
	}

	if (mTexturesList.empty())
	{
		llinfos << "Texture list is empty, moving to rez stage." << llendl;
		mCurrentAsset.setNull();
		importFirstObject();
		return;
	}

	updateImportNumbers();

	uuid_list_t::iterator iter = mTexturesList.begin();
	LLUUID id = *iter;
	mTexturesList.erase(iter);

	llinfos << "Got texture ID " << id << ": trying to upload..." << llendl;

	mCurrentAsset = id;
	std::string struid;
	id.toString(struid);
	std::string filename = mFolder + struid;
	LLAssetID uuid;
	LLTransactionID tid;

	// Generate a new transaction ID for this asset
	tid.generate();
	uuid = tid.makeAssetID(gAgent.getSecureSessionID());

	S64 file_size;
	LLFile infile(filename, "rb", &file_size);
	if (!infile)
	{
		llwarns << "Unable to access input file " << filename << llendl;
		uploadNextAsset();
		return;
	}

	constexpr S32 buf_size = 65536;
	U8 copy_buf[buf_size];
	LLFileSystem file(uuid, LLFileSystem::APPEND);
	while ((file_size = infile.read(copy_buf, buf_size)))
	{
		file.write(copy_buf, file_size);
	}

	S32 upload_cost = LLEconomy::getInstance()->getTextureUploadCost();

	LLResourceUploadInfo::ptr_t
		info(new LLResourceUploadInfo(tid, LLAssetType::AT_TEXTURE,
									  struid, struid, 0,
									  LLFolderType::FT_TEXTURE,
									  LLInventoryType::IT_TEXTURE,
									  LLFloaterPerms::getNextOwnerPerms(),
									  LLFloaterPerms::getGroupPerms(),
									  LLFloaterPerms::getEveryonePerms(),
									  upload_cost));
	info->setCapCallback(uploadNextAssetCallback, NULL);
	info->setShowInventoryPanel(false);
	upload_new_resource(info);
}

// Recursively calls uploadNextAsset()... *TODO: turn the whole import process
// into an idle callback worker, like for the export one...
//static
void LLObjectBackup::uploadNextAssetCallback(const LLSD& result, void*)
{
	LLObjectBackup* self = LLObjectBackup::findInstance();
	if (self)
	{
		self->updateMap(result["new_asset"].asUUID());
		self->uploadNextAsset();
	}
	else
	{
		llwarns << "Import aborted, LLObjectBackup instance gone !" << llendl;
	}
}

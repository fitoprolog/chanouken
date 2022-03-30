/**
 * @file lltooldraganddrop.h
 * @brief LLToolDragAndDrop class header file
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

#ifndef LL_TOOLDRAGANDDROP_H
#define LL_TOOLDRAGANDDROP_H

#include "llassetstorage.h"
#include "lldictionary.h"
#include "llpermissions.h"
#include "lltool.h"
#include "lluuid.h"
#include "llview.h"

#include "llviewerinventory.h"
#include "llwindow.h"

class LLToolDragAndDrop;
class LLViewerRegion;
class LLVOAvatar;
class LLPickInfo;

class LLToolDragAndDrop : public LLTool, public LLSingleton<LLToolDragAndDrop>
{
	friend class LLSingleton<LLToolDragAndDrop>;

protected:
	LOG_CLASS(LLToolDragAndDrop);

public:
	LLToolDragAndDrop();

	// overridden from LLTool
	virtual bool	handleMouseUp(S32 x, S32 y, MASK mask);
	virtual bool	handleHover(S32 x, S32 y, MASK mask);
	virtual bool	handleKey(KEY key, MASK mask);
	virtual bool	handleToolTip(S32 x, S32 y, std::string& msg,
								  LLRect* sticky_rect_screen);
	virtual void	onMouseCaptureLost();
	virtual void	handleDeselect();

	void			setDragStart(S32 x, S32 y);			// In screen space
	bool			isOverThreshold(S32 x, S32 y);		// In screen space

	enum ESource
	{
		SOURCE_AGENT,
		SOURCE_WORLD,
		SOURCE_NOTECARD,
		SOURCE_LIBRARY
	};

	void beginDrag(EDragAndDropType type,
				   const LLUUID& cargo_id,
				   ESource source,
				   const LLUUID& source_id = LLUUID::null,
				   const LLUUID& object_id = LLUUID::null);
	void beginMultiDrag(const std::vector<EDragAndDropType> types,
						const std::vector<LLUUID>& cargo_ids,
						ESource source,
						const LLUUID& source_id = LLUUID::null);
	void endDrag();

	LL_INLINE ESource getSource() const			{ return mSource; }
	LL_INLINE const LLUUID& getSourceID() const	{ return mSourceID; }
	LL_INLINE const LLUUID& getObjectID() const	{ return mObjectID; }
	LL_INLINE EAcceptance getLastAccept()		{ return mLastAccept; }

	LL_INLINE U32 getCargoCount() const			{ return mCargoIDs.size(); }
	LL_INLINE S32 getCargoIndex() const			{ return mCurItemIndex; }

protected:
	enum EDropTarget
	{
		DT_NONE = 0,
		DT_SELF = 1,
		DT_AVATAR = 2,
		DT_OBJECT = 3,
		DT_LAND = 4,
		DT_COUNT = 5
	};

	// dragOrDrop3dImpl points to a member of LLToolDragAndDrop that
	// takes parameters (LLViewerObject* obj, S32 face, MASK, bool
	// drop) and returns a bool if drop is ok
	typedef EAcceptance (LLToolDragAndDrop::*dragOrDrop3dImpl)(LLViewerObject*,
															   S32, MASK,
															   bool);

	void dragOrDrop(S32 x, S32 y, MASK mask, bool drop,
					EAcceptance* acceptance);
	void dragOrDrop3D(S32 x, S32 y, MASK mask, bool drop,
					  EAcceptance* acceptance);
	static void pickCallback(const LLPickInfo& pick_info);

protected:
	// 3d drop functions. these call down into the static functions named
	// drop<ThingToDrop> if drop is true and permissions allow that behavior.
	EAcceptance dad3dNULL(LLViewerObject*, S32, MASK, bool);
	EAcceptance dad3dRezObjectOnLand(LLViewerObject* obj, S32 face,
									 MASK mask, bool drop);
	EAcceptance dad3dRezObjectOnObject(LLViewerObject* obj, S32 face,
									   MASK mask, bool drop);
	EAcceptance dad3dRezCategoryOnObject(LLViewerObject* obj, S32 face,
										 MASK mask, bool drop);
	EAcceptance dad3dRezScript(LLViewerObject* obj, S32 face,
							   MASK mask, bool drop);
	EAcceptance dad3dTextureObject(LLViewerObject* obj, S32 face,
								   MASK mask, bool drop);
#if LL_MESH_ASSET_SUPPORT
	EAcceptance dad3dMeshObject(LLViewerObject* obj, S32 face,
								MASK mask, bool drop);
#endif
	EAcceptance dad3dWearItem(LLViewerObject* obj, S32 face,
							  MASK mask, bool drop);
	EAcceptance dad3dWearCategory(LLViewerObject* obj, S32 face,
								  MASK mask, bool drop);
	EAcceptance dad3dUpdateInventory(LLViewerObject* obj, S32 face,
									 MASK mask, bool drop);
	EAcceptance dad3dUpdateInventoryCategory(LLViewerObject* obj, S32 face,
											 MASK mask, bool drop);
	EAcceptance dad3dGiveInventoryObject(LLViewerObject* obj, S32 face,
								   MASK mask, bool drop);
	EAcceptance dad3dGiveInventory(LLViewerObject* obj, S32 face,
								   MASK mask, bool drop);
	EAcceptance dad3dGiveInventoryCategory(LLViewerObject* obj, S32 face,
										   MASK mask, bool drop);
	EAcceptance dad3dRezFromObjectOnLand(LLViewerObject* obj, S32 face,
										 MASK mask, bool drop);
	EAcceptance dad3dRezFromObjectOnObject(LLViewerObject* obj, S32 face,
										   MASK mask, bool drop);
	EAcceptance dad3dRezAttachmentFromInv(LLViewerObject* obj, S32 face,
										  MASK mask, bool drop);
	EAcceptance dad3dCategoryOnLand(LLViewerObject* obj, S32 face,
									MASK mask, bool drop);
	EAcceptance dad3dAssetOnLand(LLViewerObject* obj, S32 face,
								 MASK mask, bool drop);
	EAcceptance dad3dActivateGesture(LLViewerObject* obj, S32 face,
									 MASK mask, bool drop);

	// Helper called by methods above to handle "application" of an item to an
	// object (texture applied to face, mesh applied to shape, etc.)
	EAcceptance dad3dApplyToObject(LLViewerObject* obj, S32 face, MASK mask,
								   bool drop, EDragAndDropType cargo_type);

	// Sets the LLToolDragAndDrop's cursor based on the given acceptance
	ECursorType acceptanceToCursor(EAcceptance acceptance);

	// This method converts mCargoID to an inventory item or folder. If no item
	// or category is found, both pointers will be returned NULL.
	LLInventoryObject* locateInventory(LLViewerInventoryItem*& item,
									   LLViewerInventoryCategory*& cat);

	void dropObject(LLViewerObject* raycast_target, bool bypass_sim_raycast,
					bool from_task_inventory, bool remove_from_inventory);

	// Accessor that looks at permissions, copyability, and names of inventory
	// items to determine if a drop would be ok.
	static EAcceptance willObjectAcceptInventory(LLViewerObject* obj,
												 LLInventoryItem* item,
												 EDragAndDropType type = DAD_NONE);

	// Deals with permissions of object, etc. returns true if drop can
	// proceed, otherwise false.
	static bool handleDropTextureProtections(LLViewerObject* hit_obj,
											 LLInventoryItem* item,
											 LLToolDragAndDrop::ESource source,
											 const LLUUID& src_id);

	// Gives inventory item functionality
	static bool handleCopyProtectedItem(const LLSD& notification,
										const LLSD& response);
	static void commitGiveInventoryItem(const LLUUID& to_agent,
										LLInventoryItem* item,
										const LLUUID& im_session_id = LLUUID::null);

	// Gives inventory category functionality
	static bool handleCopyProtectedCategory(const LLSD& notification,
											const LLSD& response);
	static void commitGiveInventoryCategory(const LLUUID& to_agent,
											LLInventoryCategory* cat,
											const LLUUID& im_session_id = LLUUID::null);

public:
	// Helper method
	LL_INLINE static bool isInventoryDropAcceptable(LLViewerObject* obj,
													LLInventoryItem* item)
	{
		return ACCEPT_YES_COPY_SINGLE <= willObjectAcceptInventory(obj, item);
	}

	// This simple helper function assumes you are attempting to
	// transfer item. returns true if you can give, otherwise false.
	static bool isInventoryGiveAcceptable(LLInventoryItem* item);
	static bool isInventoryGroupGiveAcceptable(LLInventoryItem* item);

	bool dadUpdateInventory(LLViewerObject* obj, bool drop);
	bool dadUpdateInventoryCategory(LLViewerObject* obj, bool drop);

	// methods that act on the simulator state.
	static void dropScript(LLViewerObject* hit_obj, LLInventoryItem* item,
						   bool active, ESource source, const LLUUID& src_id);
	static void dropTextureOneFace(LLViewerObject* hit_obj, S32 hit_face,
								   LLInventoryItem* item, ESource source,
								   const LLUUID& src_id);
	static void dropTextureAllFaces(LLViewerObject* hit_obj,
									LLInventoryItem* item, ESource source,
									const LLUUID& src_id);
#if LL_MESH_ASSET_SUPPORT
	static void dropMesh(LLViewerObject* hit_obj, LLInventoryItem* item,
						 ESource source, const LLUUID& src_id);
#endif
	static void dropInventory(LLViewerObject* hit_obj, LLInventoryItem* item,
							  ESource source, const LLUUID& src_id);

	static void giveInventory(const LLUUID& to_agent, LLInventoryItem* item,
							  const LLUUID& session_id = LLUUID::null);
	static void giveInventoryCategory(const LLUUID& to_agent,
									  LLInventoryCategory* item,
									  const LLUUID& session_id = LLUUID::null);

	static bool handleGiveDragAndDrop(const LLUUID& agent, const LLUUID& session,
									  bool drop, EDragAndDropType cargo_type,
									  void* cargo_data, EAcceptance* accept);

	// Classes used for determining 3d drag and drop types.
private:
	struct DragAndDropEntry : public LLDictionaryEntry
	{
		DragAndDropEntry(dragOrDrop3dImpl f_none,
						 dragOrDrop3dImpl f_self,
						 dragOrDrop3dImpl f_avatar,
						 dragOrDrop3dImpl f_object,
						 dragOrDrop3dImpl f_land);
		dragOrDrop3dImpl mFunctions[DT_COUNT];
	};
	class LLDragAndDropDictionary : public LLSingleton<LLDragAndDropDictionary>,
									public LLDictionary<EDragAndDropType, DragAndDropEntry>
	{
	public:
		LLDragAndDropDictionary();
		dragOrDrop3dImpl get(EDragAndDropType dad_type,
							 EDropTarget drop_target);
	};

protected:
	S32					mDragStartX;
	S32					mDragStartY;

	S32					mCurItemIndex;

	ESource				mSource;
	ECursorType			mCursor;
	EAcceptance			mLastAccept;

	LLVector3d			mLastCameraPos;
	LLVector3d			mLastHitPos;

	LLUUID				mSourceID;
	LLUUID				mObjectID;
	std::vector<EDragAndDropType> mCargoTypes;
	std::vector<LLUUID>	mCargoIDs;

	std::string			mToolTipMsg;
	bool				mDrop;
};

// Utility function
void pack_permissions_slam(LLMessageSystem* msg, U32 flags,
						   const LLPermissions& perms);

#endif  // LL_TOOLDRAGANDDROP_H

/**
 * @file lllocalbitmaps.h
 * @author Vaalith Jinn, code cleanup by Henri Beauchamp
 * @brief Local Bitmaps header
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 *
 * Copyright (c) 2011, Linden Research, Inc.
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

#ifndef LL_LOCALBITMAPS_H
#define LL_LOCALBITMAPS_H

#include "llavatarappearancedefines.h"
#include "lleventtimer.h"
#include "hbfileselector.h"
#include "llimage.h"
#include "llpointer.h"
#include "llwearabletype.h"

class LLViewerObject;

class LLLocalBitmapTimer : public LLEventTimer
{
public:
	LLLocalBitmapTimer();
	~LLLocalBitmapTimer() override					{}

	bool tick() override;

	void startTimer();
	void stopTimer();
	bool isRunning();
};

class LLLocalBitmap
{
protected:
	LOG_CLASS(LLLocalBitmap);

public:
	LLLocalBitmap(std::string filename);
	~LLLocalBitmap();

	LL_INLINE std::string& getFilename()				{ return mFilename; }
	LL_INLINE std::string& getShortName()				{ return mShortName; }
	LL_INLINE LLUUID& getTrackingID()					{ return mTrackingID; }
	LL_INLINE LLUUID& getWorldID()						{ return mWorldID; }
	LL_INLINE bool getValid()							{ return mValid; }

	enum EUpdateType
	{
		UT_FIRSTUSE,
		UT_REGUPDATE
	};

	bool updateSelf(EUpdateType = UT_REGUPDATE);

	static void cleanupClass();

	static void addUnits();
	static void delUnit(LLUUID& tracking_id);

	typedef std::list<LLLocalBitmap*> local_list;
	typedef local_list::iterator local_list_iter;
	typedef local_list::const_iterator local_list_const_iter;

	LL_INLINE static const local_list& getBitmapList()	{ return sBitmapList; }
	LL_INLINE static S32 getBitmapListVersion()			{ return sBitmapsListVersion; }

	static LLUUID getWorldID(const LLUUID& tracking_id);
	static bool isLocal(const LLUUID& world_id);
	static std::string getFilename(const LLUUID& tracking_id);

	static void doUpdates();
	static void setNeedsRebake();
	static void doRebake();

private:
	bool decodeBitmap(LLPointer<LLImageRaw> raw);
	void replaceIDs(const LLUUID& old_id, LLUUID new_id);
	std::vector<LLViewerObject*> prepUpdateObjects(const LLUUID& old_id,
												   U32 channel);
	void updateUserPrims(const LLUUID& old_id, const LLUUID& new_id,
						 U32 channel);
	void updateUserVolumes(const LLUUID& old_id, const LLUUID& new_id,
						   U32 channel);
	void updateUserLayers(const LLUUID& old_id, const LLUUID& new_id,
						  LLWearableType::EType type);
	LLAvatarAppearanceDefines::ETextureIndex getTexIndex(LLWearableType::EType type,
														 LLAvatarAppearanceDefines::EBakedTextureIndex baked_texind);

	enum ELinkStatus
	{
		LS_ON,
		LS_BROKEN
	};

	enum EExtension
	{
		ET_IMG_BMP,
		ET_IMG_TGA,
		ET_IMG_JPG,
		ET_IMG_PNG
	};

	static void addUnitsCallback(HBFileSelector::ELoadFilter type,
								 std::deque<std::string>& files, void*);

private:
	S32							mUpdateRetries;
	EExtension					mExtension;
	ELinkStatus					mLinkStatus;
	LLUUID						mTrackingID;
	LLUUID						mWorldID;
	LLSD						mLastModified;
	std::string					mFilename;
	std::string					mShortName;
	bool						mValid;

	static S32					sBitmapsListVersion;
	static local_list			sBitmapList;
	static LLLocalBitmapTimer	sTimer;
	static bool					sNeedsRebake;
};

#endif

/**
 * @file llvovolume.cpp
 * @brief LLVOVolume class implementation
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

// A "volume" is a box, cylinder, sphere, or other primitive shape.

#include "llviewerprecompiledheaders.h"

#include <sstream>

#include "llvovolume.h"

#include "imageids.h"
#include "llanimationstates.h"
#include "llavatarappearancedefines.h"
#include "lldir.h"
#include "llfastmap.h"
#include "llfasttimer.h"
#include "llmaterialtable.h"
#include "llmatrix4a.h"
#include "llmediaentry.h"
#include "llpluginclassmedia.h"		// For code in the mediaEvent handler
#include "llprimitive.h"
#include "llsdutil.h"
#include "llvolume.h"
#include "llvolumemessage.h"
#include "llvolumemgr.h"
#include "llvolumeoctree.h"
#include "llmessage.h"
#include "object_flags.h"

#include "llagent.h"
#include "llappearancemgr.h"
#include "lldrawable.h"
#include "lldrawpoolavatar.h"
#include "lldrawpoolbump.h"
#include "llface.h"
#include "llflexibleobject.h"
#include "llfloatertools.h"
#include "llhudmanager.h"
#include "llmaterialmgr.h"
#include "llmediadataclient.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llskinningutil.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "lltexturefetch.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermediafocus.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertextureanim.h"
#include "llviewertexturelist.h"
#include "llvoavatarpuppet.h"
#include "llvoavatarself.h"
#include "llvocache.h"
#include "llworld.h"

using namespace LLAvatarAppearanceDefines;

constexpr F32 FORCE_SIMPLE_RENDER_AREA = 512.f;
constexpr F32 FORCE_CULL_AREA = 8.f;
constexpr F32 ONE255TH = 1.f / 255.f;

bool LLVOVolume::sAnimateTextures = true;
U32 LLVOVolume::sRenderMaxVBOSize = 4096;
F32 LLVOVolume::sLODFactor = 1.f;
F32 LLVOVolume::sDistanceFactor = 1.f;
S32 LLVOVolume::sNumLODChanges = 0;
#if 0	// Not yet implemented
S32 LLVOVolume::mRenderComplexity_last = 0;
S32 LLVOVolume::mRenderComplexity_current = 0;
#endif
LLPointer<LLObjectMediaDataClient> LLVOVolume::sObjectMediaClient = NULL;
LLPointer<LLObjectMediaNavigateClient> LLVOVolume::sObjectMediaNavigateClient = NULL;

constexpr U32 MAX_FACE_COUNT = 4096U;
S32 LLVolumeGeometryManager::sInstanceCount = 0;
LLFace** LLVolumeGeometryManager::sFullbrightFaces = NULL;
LLFace** LLVolumeGeometryManager::sBumpFaces = NULL;
LLFace** LLVolumeGeometryManager::sSimpleFaces = NULL;
LLFace** LLVolumeGeometryManager::sNormFaces = NULL;
LLFace** LLVolumeGeometryManager::sSpecFaces = NULL;
LLFace** LLVolumeGeometryManager::sNormSpecFaces = NULL;
LLFace** LLVolumeGeometryManager::sAlphaFaces = NULL;

// Implementation class of LLMediaDataClientObject. See llmediadataclient.h
class LLMediaDataClientObjectImpl final : public LLMediaDataClientObject
{
public:
	LLMediaDataClientObjectImpl(LLVOVolume* obj, bool isNew)
	:	mObject(obj),
		mNew(isNew)
	{
		mObject->addMDCImpl();
	}

	~LLMediaDataClientObjectImpl() override
	{
		mObject->removeMDCImpl();
	}

	LL_INLINE U8 getMediaDataCount() const override
	{
		return mObject->getNumTEs();
	}

	LLSD getMediaDataLLSD(U8 index) const override
	{
		LLSD result;
		LLTextureEntry* te = mObject->getTE(index);
		if (te)
		{
			llassert((te->getMediaData() != NULL) == te->hasMedia());
			if (te->getMediaData())
			{
				result = te->getMediaData()->asLLSD();
				// *HACK: workaround bug in asLLSD() where whitelist is not set
				// properly. See DEV-41949
				if (!result.has(LLMediaEntry::WHITELIST_KEY))
				{
					result[LLMediaEntry::WHITELIST_KEY] = LLSD::emptyArray();
				}
			}
		}
		return result;
	}

	bool isCurrentMediaUrl(U8 index, const std::string& url) const override
	{
		LLTextureEntry* te = mObject->getTE(index);
		if (te && te->getMediaData())
		{
			return te->getMediaData()->getCurrentURL() == url;
		}
		return url.empty();
	}

	LL_INLINE LLUUID getID() const override		{ return mObject->getID(); }

	LL_INLINE void mediaNavigateBounceBack(U8 index) override
	{
		mObject->mediaNavigateBounceBack(index);
	}

	LL_INLINE bool hasMedia() const override
	{
		return mObject->hasMedia();
	}

	LL_INLINE void updateObjectMediaData(LLSD const& data,
										 const std::string& ver) override
	{
		mObject->updateObjectMediaData(data, ver);
	}

	F64 getMediaInterest() const override
	{
		F64 interest = mObject->getTotalMediaInterest();
		if (interest < (F64)0.0)
		{
			// media interest not valid yet, try pixel area
			interest = mObject->getPixelArea();
			// *HACK: force recalculation of pixel area if interest is the
			// "magic default" of 1024.
			if (interest == 1024.f)
			{
				const_cast<LLVOVolume*>((LLVOVolume*)mObject)->setPixelAreaAndAngle();
				interest = mObject->getPixelArea();
			}
		}
		return interest;
	}

	LL_INLINE bool isInterestingEnough() const override
	{
		return LLViewerMedia::isInterestingEnough(mObject, getMediaInterest());
	}

	LL_INLINE const std::string& getCapabilityUrl(const char* name) const override
	{
		return mObject->getRegion()->getCapability(name);
	}

	LL_INLINE bool isDead() const override		{ return mObject->isDead(); }

	LL_INLINE U32 getMediaVersion() const override
	{
		return LLTextureEntry::getVersionFromMediaVersionString(mObject->getMediaURL());
	}

	LL_INLINE bool isNew() const override		{ return mNew; }

private:
	LLPointer<LLVOVolume> mObject;
	bool mNew;
};

///////////////////////////////////////////////////////////////////////////////
// LLVOVolume class
///////////////////////////////////////////////////////////////////////////////

LLVOVolume::LLVOVolume(const LLUUID& id, LLPCode pcode,
					   LLViewerRegion* regionp)
:	LLViewerObject(id, pcode, regionp),
	mVolumeImpl(NULL),
	mTextureAnimp(NULL),
	mTexAnimMode(0),
	mVObjRadius(LLVector3(1.f, 1.f, 0.5f).length()),
	mLastDistance(0.f),
	mLOD(0),
	mLockMaxLOD(false),
	mLODChanged(false),
	mVolumeChanged(false),
	mSculptChanged(false),
	mColorChanged(false),
	mFaceMappingChanged(false),
	mSpotLightPriority(0.f),
	mLastFetchedMediaVersion(-1),
	mMDCImplCount(0),
	mLastRiggingInfoLOD(-1)
{
	mRelativeXform.setIdentity();
	mRelativeXformInvTrans.setIdentity();

	mNumFaces = 0;

	mMediaImplList.resize(getNumTEs());

	memset(&mIndexInTex, 0,
		   sizeof(S32) * LLRender::NUM_VOLUME_TEXTURE_CHANNELS);
}

LLVOVolume::~LLVOVolume()
{
	if (mTextureAnimp)
	{
		delete mTextureAnimp;
		mTextureAnimp = NULL;
	}

	if (mVolumeImpl)
	{
		delete mVolumeImpl;
		mVolumeImpl = NULL;
	}

	if (!mMediaImplList.empty())
	{
		for (U32 i = 0, count = mMediaImplList.size(); i < count; ++i)
		{
			if (mMediaImplList[i].notNull())
			{
				mMediaImplList[i]->removeObject(this);
			}
		}
	}

	mCostData = NULL;
}

void LLVOVolume::markDead()
{
	if (mDead)
	{
		return;
	}

	if (mMDCImplCount > 0 &&
		(sObjectMediaClient || sObjectMediaNavigateClient))
	{
		LLMediaDataClientObject::ptr_t obj =
			new LLMediaDataClientObjectImpl(const_cast<LLVOVolume*>(this),
											false);
		if (sObjectMediaClient)
		{
			sObjectMediaClient->removeFromQueue(obj);
		}
		if (sObjectMediaNavigateClient)
		{
			sObjectMediaNavigateClient->removeFromQueue(obj);
		}
	}

	// Detach all media impls from this object
	for (U32 i = 0, count = mMediaImplList.size(); i < count; ++i)
	{
		removeMediaImpl(i);
	}

	if (mSculptTexture.notNull())
	{
		mSculptTexture->removeVolume(LLRender::SCULPT_TEX, this);
	}

	if (mLightTexture.notNull())
	{
		mLightTexture->removeVolume(LLRender::LIGHT_TEX, this);
	}

	LLViewerObject::markDead();
}

//static
void LLVOVolume::initClass()
{
	updateSettings();
	initSharedMedia();
}

//static
void LLVOVolume::updateSettings()
{
	sRenderMaxVBOSize = llmin(gSavedSettings.getU32("RenderMaxVBOSize"), 32U);
	sLODFactor = llclamp(gSavedSettings.getF32("RenderVolumeLODFactor"), 0.1f,
						 9.f);
	sDistanceFactor = 1.f - 0.1f * sLODFactor;
}

//static
void LLVOVolume::initSharedMedia()
{
	// gSavedSettings better be around
	if (gSavedSettings.getBool("EnableStreamingMedia") &&
		gSavedSettings.getBool("PrimMediaMasterEnabled"))
	{
		F32 queue_delay = gSavedSettings.getF32("PrimMediaRequestQueueDelay");
		F32 retry_delay = gSavedSettings.getF32("PrimMediaRetryTimerDelay");
		U32 max_retries = gSavedSettings.getU32("PrimMediaMaxRetries");
		U32 sorted_size = gSavedSettings.getU32("PrimMediaMaxSortedQueueSize");
		U32 rr_size = gSavedSettings.getU32("PrimMediaMaxRoundRobinQueueSize");
		sObjectMediaClient = new LLObjectMediaDataClient(queue_delay,
														 retry_delay,
														 max_retries,
														 sorted_size, rr_size);
		sObjectMediaNavigateClient =
			new LLObjectMediaNavigateClient(queue_delay, retry_delay,
											max_retries, sorted_size, rr_size);
	}
	else
	{
		// Make sure all shared media are unloaded
		LLViewerMedia::setAllMediaEnabled(false, false);
		// Make sure the media clients will not be called uselessly
		sObjectMediaClient = NULL;
		sObjectMediaNavigateClient = NULL;
	}
}

//static
void LLVOVolume::cleanupClass()
{
	sObjectMediaClient = NULL;
	sObjectMediaNavigateClient = NULL;
	llinfos << "Number of LOD cache hits: " << LLVolume::sLODCacheHit
			<< " - Cache misses: " << LLVolume::sLODCacheMiss << llendl;
}

U32 LLVOVolume::processUpdateMessage(LLMessageSystem* mesgsys,
									 void** user_data, U32 block_num,
									 EObjectUpdateType update_type,
									 LLDataPacker* dp)
{
	static LLCachedControl<bool> kill_bogus_objects(gSavedSettings,
													"KillBogusObjects");

	// Do base class updates...
	U32 retval = LLViewerObject::processUpdateMessage(mesgsys, user_data,
													  block_num, update_type,
													  dp);

	LLUUID sculpt_id;
	U8 sculpt_type = 0;
	if (isSculpted())
	{
		const LLSculptParams* sculpt_params = getSculptParams();
		if (sculpt_params)
		{
			sculpt_id = sculpt_params->getSculptTexture();
			sculpt_type = sculpt_params->getSculptType();
		}
	}

	if (!dp)
	{
		if (update_type == OUT_FULL)
		{
			////////////////////////////////
			// Unpack texture animation data

			if (mesgsys->getSizeFast(_PREHASH_ObjectData, block_num,
									 _PREHASH_TextureAnim))
			{
				if (!mTextureAnimp)
				{
					mTextureAnimp = new LLViewerTextureAnim(this);
				}
				else if (!(mTextureAnimp->mMode & LLTextureAnim::SMOOTH))
				{
					mTextureAnimp->reset();
				}
				mTexAnimMode = 0;

				mTextureAnimp->unpackTAMessage(mesgsys, block_num);
			}
			else if (mTextureAnimp)
			{
				delete mTextureAnimp;
				mTextureAnimp = NULL;
				for (S32 i = 0, count = getNumTEs(); i < count; ++i)
				{
					LLFace* facep = mDrawable->getFace(i);
					if (facep && facep->mTextureMatrix)
					{
						delete facep->mTextureMatrix;
						facep->mTextureMatrix = NULL;
					}
				}
				gPipeline.markTextured(mDrawable);
				mFaceMappingChanged = true;
				mTexAnimMode = 0;
			}

			// Unpack volume data
			LLVolumeParams volume_params;
			bool success =
				LLVolumeMessage::unpackVolumeParams(&volume_params, mesgsys,
													_PREHASH_ObjectData,
													block_num);
			if (!success)
			{
				llwarns_once << "Bogus volume parameters in object " << getID()
							 << " at " << getPositionRegion() << " owned by "
							 << mOwnerID << llendl;
				if (mRegionp)
				{
					// Do not cache this bogus object
					mRegionp->addCacheMissFull(getLocalID());
				}
				if (kill_bogus_objects)
				{
					LLViewerObjectList::sBlackListedObjects.emplace(getID());
					gObjectList.killObject(this);
					return INVALID_UPDATE;
				}
			}

			volume_params.setSculptID(sculpt_id, sculpt_type);

			if (setVolume(volume_params, 0))
			{
				markForUpdate(true);
			}
		}

		// Sigh, this needs to be done AFTER the volume is set as well,
		// otherwise bad stuff happens...
		////////////////////////////
		// Unpack texture entry data
		S32 result = unpackTEMessage(mesgsys, _PREHASH_ObjectData,
									 (S32)block_num);
		if (result == TEM_INVALID)
		{
			llwarns_once << "Bogus TE data in object " << getID() << " at "
						 << getPositionRegion() << " owned by " << mOwnerID
						 << llendl;
			if (mRegionp)
			{
				// Do not cache this bogus object
				mRegionp->addCacheMissFull(getLocalID());
			}
			if (kill_bogus_objects)
			{
				LLViewerObjectList::sBlackListedObjects.emplace(getID());
				gObjectList.killObject(this);
				return INVALID_UPDATE;
			}
		}
		if (result & TEM_CHANGE_MEDIA)
		{
			retval |= MEDIA_FLAGS_CHANGED;
		}
	}
	else if (update_type != OUT_TERSE_IMPROVED)
	{
		LLVolumeParams volume_params;
		bool success = LLVolumeMessage::unpackVolumeParams(&volume_params,
														   *dp);
		if (!success)
		{
			llwarns_once << "Bogus volume parameters in object " << getID()
						 << " at " << getPositionRegion() << " owned by "
						 << mOwnerID << llendl;
			if (mRegionp)
			{
				// Do not cache this bogus object
				mRegionp->addCacheMissFull(getLocalID());
			}
			if (kill_bogus_objects)
			{
				LLViewerObjectList::sBlackListedObjects.emplace(getID());
				gObjectList.killObject(this);
				return INVALID_UPDATE;
			}
		}

		volume_params.setSculptID(sculpt_id, sculpt_type);

		if (setVolume(volume_params, 0))
		{
			markForUpdate(true);
		}

		S32 result = unpackTEMessage(*dp);
		if (result == TEM_INVALID)
		{
			llwarns_once << "Bogus TE data in object " << getID() << " at "
						 << getPositionRegion() << " owned by " << mOwnerID
						 << llendl;
			if (mRegionp)
			{
				// Do not cache this bogus object
				mRegionp->addCacheMissFull(getLocalID());
			}
			if (kill_bogus_objects)
			{
				LLViewerObjectList::sBlackListedObjects.emplace(getID());
				gObjectList.killObject(this);
				return INVALID_UPDATE;
			}
		}
		else if (result & TEM_CHANGE_MEDIA)
		{
			retval |= MEDIA_FLAGS_CHANGED;
		}

		U32 value = dp->getPassFlags();
		if (value & 0x40)
		{
			if (!mTextureAnimp)
			{
				mTextureAnimp = new LLViewerTextureAnim(this);
			}
			else if (!(mTextureAnimp->mMode & LLTextureAnim::SMOOTH))
			{
				mTextureAnimp->reset();
			}
			mTexAnimMode = 0;
			mTextureAnimp->unpackTAMessage(*dp);
		}
		else if (mTextureAnimp)
		{
			delete mTextureAnimp;
			mTextureAnimp = NULL;
			for (S32 i = 0, count = getNumTEs(); i < count; ++i)
			{
				LLFace* facep = mDrawable->getFace(i);
				if (facep && facep->mTextureMatrix)
				{
					delete facep->mTextureMatrix;
					facep->mTextureMatrix = NULL;
				}
			}
			gPipeline.markTextured(mDrawable);
			mFaceMappingChanged = true;
			mTexAnimMode = 0;
		}
		if (value & 0x400)
		{
			// Particle system (new)
			unpackParticleSource(*dp, mOwnerID, false);
		}
	}
	else
	{
		S32 texture_length = mesgsys->getSizeFast(_PREHASH_ObjectData,
												  block_num,
												  _PREHASH_TextureEntry);
		if (texture_length)
		{
			U8 tdpbuffer[1024];
			LLDataPackerBinaryBuffer tdp(tdpbuffer, 1024);
			mesgsys->getBinaryDataFast(_PREHASH_ObjectData,
									   _PREHASH_TextureEntry,
									   tdpbuffer, 0, block_num, 1024);
			S32 result = unpackTEMessage(tdp);
			if (result & TEM_CHANGE_MEDIA)
			{
				retval |= MEDIA_FLAGS_CHANGED;
			}
		}
	}

	if (retval & (MEDIA_URL_REMOVED | MEDIA_URL_ADDED | MEDIA_URL_UPDATED |
				  MEDIA_FLAGS_CHANGED))
	{
		// If only the media URL changed, and it is not a media version URL,
		// ignore it
		if (!((retval & (MEDIA_URL_ADDED | MEDIA_URL_UPDATED)) &&
			mMedia && !mMedia->mMediaURL.empty() &&
			!LLTextureEntry::isMediaVersionString(mMedia->mMediaURL)))
		{
			// If the media changed at all, request new media data
			LL_DEBUGS("MediaOnAPrim") << "Media update: " << getID()
									  << ": retval=" << retval << " Media URL: "
									  << (mMedia ?  mMedia->mMediaURL : std::string(""))
									  << LL_ENDL;
			requestMediaDataUpdate(retval & MEDIA_FLAGS_CHANGED);
		}
		else
		{
			llinfos << "Ignoring media update for: " << getID()
					<< " Media URL: "
					<< (mMedia ?  mMedia->mMediaURL : std::string(""))
					<< llendl;
		}
	}
	// ... and clean up any media impls
	cleanUpMediaImpls();

	return retval;
}

void LLVOVolume::animateTextures()
{
	if (mDead || !mTextureAnimp) return;

	F32 off_s = 0.f, off_t = 0.f, scale_s = 1.f, scale_t = 1.f, rot = 0.f;
	S32 result = mTextureAnimp->animateTextures(off_s, off_t, scale_s, scale_t,
												rot);
	if (result)
	{
		if (!mTexAnimMode)
		{
			mFaceMappingChanged = true;
			gPipeline.markTextured(mDrawable);
		}
		mTexAnimMode = result | mTextureAnimp->mMode;

		S32 start = 0, end = mDrawable->getNumFaces() - 1;
		if (mTextureAnimp->mFace >= 0 && mTextureAnimp->mFace <= end)
		{
			start = end = mTextureAnimp->mFace;
		}

		LLVector3 trans, scale;
		LLMatrix4a scale_mat, tex_mat;
		static const LLVector3 translation(-0.5f, -0.5f, 0.f);
		for (S32 i = start; i <= end; ++i)
		{
			LLFace* facep = mDrawable->getFace(i);
			if (!facep ||
				(facep->getVirtualSize() < MIN_TEX_ANIM_SIZE &&
				 facep->mTextureMatrix))
			{
				continue;
			}

			const LLTextureEntry* te = facep->getTextureEntry();
			if (!te)
			{
				continue;
			}

			if (!(result & LLViewerTextureAnim::ROTATE))
			{
				te->getRotation(&rot);
			}

			if (!(result & LLViewerTextureAnim::TRANSLATE))
			{
				te->getOffset(&off_s, &off_t);
			}
			trans.set(LLVector3(off_s + 0.5f, off_t + 0.5f, 0.f));

			if (!(result & LLViewerTextureAnim::SCALE))
			{
				te->getScale(&scale_s, &scale_t);
			}
			scale.set(scale_s, scale_t, 1.f);

			if (!facep->mTextureMatrix)
			{
				facep->mTextureMatrix = new LLMatrix4();
			}

			tex_mat.setIdentity();
			tex_mat.translateAffine(translation);

			static const LLVector4a z_neg_axis(0.f, 0.f, -1.f);
			tex_mat.setMul(gl_gen_rot(rot * RAD_TO_DEG, z_neg_axis), tex_mat);

			scale_mat.setIdentity();
			scale_mat.applyScaleAffine(scale);
			tex_mat.setMul(scale_mat, tex_mat);	// Left mul

			tex_mat.translateAffine(trans);

			facep->mTextureMatrix->set(tex_mat.getF32ptr());
		}
	}
	else if (mTexAnimMode && mTextureAnimp->mRate == 0)
	{
		U8 start, count;
		if (mTextureAnimp->mFace == -1)
		{
			start = 0;
			count = getNumTEs();
		}
		else
		{
			start = (U8)mTextureAnimp->mFace;
			count = 1;
		}

		for (S32 i = start; i < start + count; ++i)
		{
			if (mTexAnimMode & LLViewerTextureAnim::TRANSLATE)
			{
				setTEOffset(i, mTextureAnimp->mOffS, mTextureAnimp->mOffT);
			}
			if (mTexAnimMode & LLViewerTextureAnim::SCALE)
			{
				setTEScale(i, mTextureAnimp->mScaleS, mTextureAnimp->mScaleT);
			}
			if (mTexAnimMode & LLViewerTextureAnim::ROTATE)
			{
				setTERotation(i, mTextureAnimp->mRot);
			}
		}

		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
		mTexAnimMode = 0;
	}
}

void LLVOVolume::updateTextures()
{
	constexpr F32 TEXTURE_AREA_REFRESH_TIME = 5.f; // seconds
	if (mTextureUpdateTimer.getElapsedTimeF32() > TEXTURE_AREA_REFRESH_TIME)
	{
		updateTextureVirtualSize();

		if (mDrawable.isNull() || isVisible() || mDrawable->isActive())
		{
			return;
		}

		// Delete vertex buffer to free up some VRAM
		LLSpatialGroup* group = mDrawable->getSpatialGroup();
		if (group &&
			(group->mVertexBuffer.notNull() || !group->mBufferMap.empty() ||
			 !group->mDrawMap.empty()))
		{
			group->destroyGL(true);

			// Flag the group as having changed geometry so it gets a rebuild
			// next time it becomes visible
			group->setState(LLSpatialGroup::GEOM_DIRTY |
							LLSpatialGroup::MESH_DIRTY |
							LLSpatialGroup::NEW_DRAWINFO);
		}
	}
}

bool LLVOVolume::isVisible() const
{
	if (mDrawable.notNull() && mDrawable->isVisible())
	{
		return true;
	}
#if 0
	if (isHUDAttachment())
	{
		return true;
	}
#endif
	if (isAttachment())
	{
		LLViewerObject* objp = (LLViewerObject*)getParent();
		while (objp && !objp->isAvatar())
		{
			objp = (LLViewerObject*)objp->getParent();
		}

		return objp && objp->mDrawable.notNull() &&
			   objp->mDrawable->isVisible();
	}

	return false;
}

// Updates the pixel area of all faces
void LLVOVolume::updateTextureVirtualSize(bool forced)
{
	if (!forced)
	{
		if (!isVisible())
		{
			// Do not load textures for non-visible faces
			for (S32 i = 0, count = mDrawable->getNumFaces(); i < count; ++i)
			{
				LLFace* face = mDrawable->getFace(i);
				if (face)
				{
					face->setPixelArea(0.f);
					face->setVirtualSize(0.f);
				}
			}
			return;
		}

		if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SIMPLE))
		{
			return;
		}
	}

	if (LLViewerTexture::sDontLoadVolumeTextures ||
		gTextureFetchp->mDebugPause)
	{
		return;
	}

	mTextureUpdateTimer.reset();

	F32 old_area = mPixelArea;
	mPixelArea = 0.f;

	const S32 num_faces = mDrawable->getNumFaces();
	F32 min_vsize = 999999999.f, max_vsize = 0.f;
	bool is_ours = permYouOwner();
	LLVOAvatar* avatar = getAvatar();
	for (S32 i = 0; i < num_faces; ++i)
	{
		LLFace* face = mDrawable->getFace(i);
		if (!face) continue;

		const LLTextureEntry* te = face->getTextureEntry();
		LLViewerTexture* imagep = face->getTexture();
		if (!imagep || !te || face->mExtents[0].equals3(face->mExtents[1]))
		{
			continue;
		}

		F32 vsize;
		F32 old_size = face->getVirtualSize();

		if (isHUDAttachment())
		{
			imagep->setBoostLevel(LLGLTexture::BOOST_HUD);
			// Treat as full screen
			vsize = (F32)gViewerCamera.getScreenPixelArea();
		}
		else
		{
			vsize = face->getTextureVirtualSize();
			if (isAttachment())
			{
				// Rez attachments faster and at full details !
				if (is_ours)
				{
					// Our attachments must really rez fast and fully:
					// we should not have to zoom on them to get the textures
					// fully loaded !
					imagep->setBoostLevel(LLGLTexture::BOOST_HUD);
					// ... and do not discard our attachments textures
					imagep->dontDiscard();
				}
				else if (avatar && !avatar->isImpostor())
				{
					// Others' can get their texture discarded to avoid filling
					// up the video buffers in crowded areas...
					static LLCachedControl<bool> boost_tex(gSavedSettings,
														   "TextureBoostAttachments");
					static LLCachedControl<S32> min_vsize_sqr(gSavedSettings,
															  "TextureMinAttachmentsVSize");
					static LLCachedControl<F32> boost_thresold(gSavedSettings,
															   "TextureBoostBiasThreshold");
					F32 high_bias = llclamp((F32)boost_thresold, 0.f, 1.5f);
					if (boost_tex &&
						LLViewerTexture::sDesiredDiscardBias <= high_bias)
					{
						// As long as the current bias is not too high (i.e. we
						// are not using too much memory), and provided that
						// the TextureBoostAttachments setting is true, let's
						// boost significantly the attachments.
						// First, raise the priority to the one of selected
						// objects, causing the attachments to rez much faster
						// and preventing them to get affected by the bias
						// level (see LLViewerTexture::processTextureStats() for
						// the algorithm).
						imagep->setBoostLevel(LLGLTexture::BOOST_SELECTED);
						// And now, the importance to the camera...
						// The problem with attachments is that they most often
						// use very small prims with high resolution textures,
						// and Snowglobe's algorithm considers such textures as
						// unimportant to the camera... Let's counter this
						// effect, using a minimum, user configurable virtual
						// size.
						F32 min_vsize = (F32)(min_vsize_sqr * min_vsize_sqr);
						if (vsize < min_vsize)
						{
							vsize = min_vsize;
						}
					}
					else
					{
						// Bias is at its maximum: only boost a little, not
						// preventing bias to affect this texture either.
						imagep->setBoostLevel(LLGLTexture::BOOST_AVATAR);
					}
					// Boost the decode priority too (does not affect memory
					// usage)
					LLViewerFetchedTexture* tex =
						LLViewerTextureManager::staticCastToFetchedTexture(imagep);
					if (tex)
					{
						tex->setAdditionalDecodePriority(1.5f);
					}
				}
			}
		}

		mPixelArea = llmax(mPixelArea, face->getPixelArea());

		if (face->mTextureMatrix != NULL)
		{
#if 1
			// Animating textures also rez badly in Snowglobe because the
			// actual displayed area is only a fraction (corresponding to one
			// frame) of the animating texture. Let's fix that here:
			if (mTextureAnimp && mTextureAnimp->mScaleS > 0.f &&
				mTextureAnimp->mScaleT > 0.f)
			{
				// Adjust to take into account the actual frame size which is
				// only a portion of the animating texture
				vsize = vsize / mTextureAnimp->mScaleS / mTextureAnimp->mScaleT;
			}
#endif
			if ((vsize < MIN_TEX_ANIM_SIZE && old_size >= MIN_TEX_ANIM_SIZE) ||
				(vsize >= MIN_TEX_ANIM_SIZE && old_size < MIN_TEX_ANIM_SIZE))
			{
				gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_TCOORD);
			}
		}

		face->setVirtualSize(vsize);

		if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_AREA))
		{
			if (vsize < min_vsize) min_vsize = vsize;
			if (vsize > max_vsize) max_vsize = vsize;
		}
		else if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY))
		{
			LLViewerFetchedTexture* img =
				LLViewerTextureManager::staticCastToFetchedTexture(imagep);
			if (img)
			{
				F32 pri = img->getDecodePriority();
				pri = llmax(pri, 0.f);
				if (pri < min_vsize)
				{
					min_vsize = pri;
				}
				if (pri > max_vsize)
				{
					max_vsize = pri;
				}
			}
		}
		else if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_FACE_AREA))
		{
			F32 pri = mPixelArea;
			if (pri < min_vsize)
			{
				min_vsize = pri;
			}
			if (pri > max_vsize)
			{
				max_vsize = pri;
			}
		}
	}

	if (isSculpted())
	{
		// Note: sets mSculptTexture to NULL if this is a mesh object:
		updateSculptTexture();

		if (mSculptTexture.notNull())
		{
			mSculptTexture->setBoostLevel(llmax((S32)mSculptTexture->getBoostLevel(),
												(S32)LLGLTexture::BOOST_SCULPTED));
			mSculptTexture->setForSculpt();

			if (!mSculptTexture->isCachedRawImageReady())
			{
				S32 lod = llmin(mLOD, 3);
				F32 lodf = (F32)(lod + 1) * 0.25f;
				F32 tex_size = lodf * MAX_SCULPT_REZ;
				mSculptTexture->addTextureStats(2.f * tex_size * tex_size, false);

				// If the sculpty very close to the view point, load first
				LLVector3 look_at = getPositionAgent() -
									gViewerCamera.getOrigin();
				F32 dist = look_at.normalize();
				F32 cos_to_view_dir = look_at * gViewerCamera.getXAxis();
				F32 prio = 0.8f *
						   LLFace::calcImportanceToCamera(cos_to_view_dir,
														  dist);
				mSculptTexture->setAdditionalDecodePriority(prio);
			}

			// Try to match the texture:
			S32 texture_discard = mSculptTexture->getCachedRawImageLevel();
			S32 current_discard = getVolume() ? getVolume()->getSculptLevel()
											  : -2;

			if (texture_discard >= 0 && // Texture has some data available
				 // Texture has more data than last rebuild
				(texture_discard < current_discard ||
				 // No previous rebuild
				 current_discard < 0))
			{
				gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);
				mSculptChanged = true;
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SCULPTED))
			{
				setDebugText(llformat("T%d C%d V%d\n%dx%d",
									  texture_discard, current_discard,
									  getVolume() ?
										getVolume()->getSculptLevel() : -2,
									  mSculptTexture->getHeight(),
									  mSculptTexture->getWidth()));
			}
		}
	}

	if (getLightTextureID().notNull())
	{
		const LLLightImageParams* params = getLightImageParams();
		if (params)
		{
			const LLUUID& id = params->getLightTexture();
			mLightTexture =
				LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT,
														  true,
														  LLGLTexture::BOOST_ALM);
			if (mLightTexture.notNull())
			{
				F32 rad = getLightRadius();
				F32 vsize = gPipeline.calcPixelArea(getPositionAgent(),
													LLVector3(rad, rad, rad),
													gViewerCamera);
				mLightTexture->addTextureStats(vsize);
			}
		}
	}

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_AREA))
	{
		setDebugText(llformat("%.0f:%.0f", sqrtf(min_vsize),
							  sqrtf(max_vsize)));
	}
 	else if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY))
 	{
 		setDebugText(llformat("%.0f:%.0f", sqrtf(min_vsize),
							  sqrtf(max_vsize)));
 	}
	else if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_FACE_AREA))
	{
		setDebugText(llformat("%.0f:%.0f", sqrtf(min_vsize),
							  sqrtf(max_vsize)));
	}
	else if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_SIZE))
	{
		std::map<U64, std::string> tex_list;
		std::map<U64, std::string>::iterator it;
		std::string faces;
		for (S32 i = 0, count = mDrawable->getNumFaces(); i < count; ++i)
		{
			LLFace* facep = mDrawable->getFace(i);
			if (!facep) continue;

			LLViewerTexture* vtex = facep->getTexture();
			if (!vtex) continue;

			LLViewerFetchedTexture* tex = vtex->asFetched();
			if (!tex) continue;

			faces = llformat("%d", i);
			U64 size = ((U64)tex->getWidth() << 32) + (U64)tex->getHeight();
			it = tex_list.find(size);
			if (it == tex_list.end())
			{
				tex_list.emplace(size, faces);
			}
			else
			{
				tex_list.emplace(size, it->second + " " + faces);
			}
		}

		std::map<U64, std::string>::iterator end = tex_list.end();
		std::string output;
		for (it = tex_list.begin(); it != end; ++it)
		{
			U64 size = it->first;
			S32 width = (S32)(size >> 32);
			S32 height = (S32)(size & 0x00000000ffffffff);
			faces = llformat("%dx%d (%s)", width, height, it->second.c_str());
			if (!output.empty())
			{
				output += "\n";
			}
			output += faces;
		}
		setDebugText(output);
	}

	if (mPixelArea == 0)
	{
		// Flexi phasing issues make this happen
		mPixelArea = old_area;
	}
}

void LLVOVolume::setTexture(S32 face)
{
	llassert(face < getNumTEs());
	gGL.getTexUnit(0)->bind(getTEImage(face));
}

void LLVOVolume::setScale(const LLVector3& scale, bool damped)
{
	if (scale != getScale())
	{
		// Store local radius
		LLViewerObject::setScale(scale);

		if (mVolumeImpl)
		{
			mVolumeImpl->onSetScale(scale, damped);
		}

		updateRadius();

		// Since drawable transforms do not include scale, changing volume
		// scale requires an immediate rebuild of volume verts.
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_POSITION, true);
	}
}

LLFace* LLVOVolume::addFace(S32 f)
{
	const LLTextureEntry* te = getTE(f);
	LLViewerTexture* imagep = getTEImage(f);
	if (te && te->getMaterialParams().notNull())
	{
		return mDrawable->addFace(te, imagep, getTENormalMap(f),
								  getTESpecularMap(f));
	}
	return mDrawable->addFace(te, imagep);
}

LLDrawable* LLVOVolume::createDrawable()
{
	gPipeline.allocDrawable(this);

	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_VOLUME);

	S32 max_tes_to_set = getNumTEs();
	for (S32 i = 0; i < max_tes_to_set; ++i)
	{
		addFace(i);
	}
	mNumFaces = max_tes_to_set;

	if (isAttachment())
	{
		mDrawable->makeActive();
	}

	if (getIsLight())
	{
		// Add it to the pipeline mLightSet
		gPipeline.setLight(mDrawable, true);
	}

	updateRadius();
	// Force_update = true to avoid non-alpha mDistance update being optimized
	// away
	mDrawable->updateDistance(gViewerCamera, true);

	return mDrawable;
}

bool LLVOVolume::setVolume(const LLVolumeParams& params_in, S32 detail,
						   bool unique_volume)
{
	mCostData = NULL;	//  Reset cost data cache since parameters changed

	LLVolumeParams volume_params = params_in;

	S32 last_lod;
	if (mVolumep.notNull())
	{
		last_lod =
			LLVolumeLODGroup::getVolumeDetailFromScale(mVolumep->getDetail());
	}
	else
	{
		last_lod = -1;
	}

	bool is404 = false;
	S32 lod = mLOD;
	if (isSculpted())
	{
		// If it is a mesh
		if ((volume_params.getSculptType() & LL_SCULPT_TYPE_MASK) ==
				LL_SCULPT_TYPE_MESH)
		{
			lod = gMeshRepo.getActualMeshLOD(volume_params, lod);
			if (lod == -1)
			{
				is404 = true;
				lod = 0;
			}
			else
			{
				mLOD = lod;	// Adopt the actual mesh LOD
			}
		}
	}

	// Check if we need to change implementations
	bool is_flexible = volume_params.getPathParams().getCurveType() ==
						LL_PCODE_PATH_FLEXIBLE;
	if (is_flexible)
	{
		setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, true, false);
		if (!mVolumeImpl)
		{
			mVolumeImpl = new LLVolumeImplFlexible(this,
												   getFlexibleObjectData());
		}
	}
	else
	{
		// Mark the parameter not in use
		setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, false, false);
		if (mVolumeImpl)
		{
			delete mVolumeImpl;
			mVolumeImpl = NULL;
			if (mDrawable.notNull())
			{
				// Undo the damage we did to this matrix
				mDrawable->updateXform(false);
			}
		}
	}

	if (is404)
	{
		setIcon(LLViewerTextureManager::getFetchedTextureFromFile("inv_item_mesh.tga"));
		// Render prim proxy when mesh loading attempts give up
		volume_params.setSculptID(LLUUID::null, LL_SCULPT_TYPE_NONE);
	}

	bool res = LLPrimitive::setVolume(volume_params, lod,
									  mVolumeImpl &&
									  mVolumeImpl->isVolumeUnique());
	if (!res && !mSculptChanged)
	{
		return false;
	}

	mFaceMappingChanged = true;

	if (mVolumeImpl)
	{
		mVolumeImpl->onSetVolume(volume_params, mLOD);
	}

	updateSculptTexture();

	if (!isSculpted())
	{
		return true;
	}

	LLVolume* volume = getVolume();
	if (!volume)
	{
		return false;
	}

	// If it is a mesh
	if ((volume_params.getSculptType() &
		 LL_SCULPT_TYPE_MASK) == LL_SCULPT_TYPE_MESH)
	{
		if (volume && !volume->isMeshAssetLoaded())
		{
			// Load request not yet issued, request pipeline load this mesh
			S32 available_lod = gMeshRepo.loadMesh(this, volume_params, lod,
												   last_lod);
			if (available_lod != lod)
			{
				LLPrimitive::setVolume(volume_params, available_lod);
			}
		}
	}
	// Otherwise it should be sculptie
	else
	{
		sculpt();
	}

	return true;
}

void LLVOVolume::updateSculptTexture()
{
	LLPointer<LLViewerFetchedTexture> old_sculpt = mSculptTexture;

	if (isSculpted() && !isMesh())
	{
		const LLSculptParams* sculpt_params = getSculptParams();
		if (sculpt_params)
		{
			const LLUUID& id = sculpt_params->getSculptTexture();
			if (id.notNull())
			{
				mSculptTexture =
					LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT,
															  true,
															  LLGLTexture::BOOST_NONE,
															  LLViewerTexture::LOD_TEXTURE);
			}
		}
	}
	else
	{
		mSculptTexture = NULL;
	}

	if (mSculptTexture != old_sculpt)
	{
		if (old_sculpt.notNull())
		{
			old_sculpt->removeVolume(LLRender::SCULPT_TEX, this);
		}
		if (mSculptTexture.notNull())
		{
			mSculptTexture->addVolume(LLRender::SCULPT_TEX, this);
		}
	}
}

void LLVOVolume::updateVisualComplexity()
{
	LLVOAvatar* avatar = getAvatarAncestor();
	if (avatar)
	{
		avatar->updateVisualComplexity();
	}
	LLVOAvatar* rigged_avatar = getAvatar();
	if (rigged_avatar && rigged_avatar != avatar)
	{
		rigged_avatar->updateVisualComplexity();
	}
}

void LLVOVolume::notifyMeshLoaded()
{
	mCostData = NULL;	//  Reset cost data cache since mesh changed
	mSculptChanged = true;
	gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_GEOMETRY, true);
	LLVOAvatar* avatarp = getAvatar();
	if (avatarp && !isAnimatedObject())
	{
		avatarp->addAttachmentOverridesForObject(this);
	}
	LLVOAvatarPuppet* puppetp = getPuppetAvatar();
	if (puppetp)
	{
		puppetp->addAttachmentOverridesForObject(this);
	}
	updateVisualComplexity();
}

// This replaces generate() for sculpted surfaces
void LLVOVolume::sculpt()
{
	if (mSculptTexture.isNull())
	{
		return;
	}

	U16 sculpt_height = 0;
	U16 sculpt_width = 0;
	S8 sculpt_components = 0;
	const U8* sculpt_data = NULL;

	S32 discard_level = mSculptTexture->getCachedRawImageLevel();
	LLImageRaw* raw_image = mSculptTexture->getCachedRawImage();

	S32 max_discard = mSculptTexture->getMaxDiscardLevel();
	if (discard_level > max_discard)
	{
		discard_level = max_discard;	// Clamp to the best we can do
	}
	if (discard_level > MAX_DISCARD_LEVEL)
	{
		return; // We think data is not ready yet.
	}

	S32 current_discard = getVolume()->getSculptLevel();
	if (current_discard < -2)
	{
		llwarns << "Current discard of sculpty at " << current_discard
				<< " is less than -2 !" << llendl;
		// Corrupted volume... do not update the sculpty
		return;
	}
	else if (current_discard > MAX_DISCARD_LEVEL)
	{
#if 0	// Disabled to avoid log spam. *FIXME: find the cause for that spam
		llwarns << "Current discard of sculpty at " << current_discard
				<< " is more than the allowed max of " << MAX_DISCARD_LEVEL
				<< llendl;
#endif
		// Corrupted volume... do not update the sculpty
		return;
	}

	if (current_discard == discard_level)
	{
		// No work to do here
		return;
	}

	if (!raw_image)
	{
		llassert(discard_level < 0);
		sculpt_width = 0;
		sculpt_height = 0;
		sculpt_data = NULL;
	}
	else
	{
		sculpt_height = raw_image->getHeight();
		sculpt_width = raw_image->getWidth();
		sculpt_components = raw_image->getComponents();
		sculpt_data = raw_image->getData();
	}
	getVolume()->sculpt(sculpt_width, sculpt_height, sculpt_components,
						sculpt_data, discard_level,
						mSculptTexture->isMissingAsset());

	// Notify rebuild any other VOVolumes that reference this sculpty volume
	for (S32 i = 0,
			 count = mSculptTexture->getNumVolumes(LLRender::SCULPT_TEX);
		 i < count; ++i)
	{
		LLVOVolume* volume =
			(*(mSculptTexture->getVolumeList(LLRender::SCULPT_TEX)))[i];
		if (volume != this && volume->getVolume() == getVolume())
		{
			gPipeline.markRebuild(volume->mDrawable,
								  LLDrawable::REBUILD_GEOMETRY, false);
		}
	}
}

S32 LLVOVolume::computeLODDetail(F32 distance, F32 radius, F32 lod_factor)
{
	if (LLPipeline::sDynamicLOD)
	{
		// We have got LOD in the profile, and in the twist. Use radius.
		F32 tan_angle = ll_round(lod_factor * radius / distance, 0.01f);
		return LLVolumeLODGroup::getDetailFromTan(tan_angle);
	}
	return llclamp((S32)(sqrtf(radius) * lod_factor * 4.f), 0, 3);
}

bool LLVOVolume::calcLOD()
{
	if (mDrawable.isNull())
	{
		return false;
	}

	// Locked to max LOD objects, selected objects and HUD attachments always
	// rendered at max LOD
	if (mLockMaxLOD || isSelected() || isHUDAttachment())
	{
		if (mLOD == 3)
		{
			return false;
		}
		mLOD = 3;
		return true;
	}

	F32 radius;
	F32 distance;
	LLVolume* volume = getVolume();
	if (mDrawable->isState(LLDrawable::RIGGED))
	{
		LLVOAvatar* avatar = getAvatar();
		if (!avatar)
		{
			llwarns << "NULL avatar pointer for rigged drawable" << llendl;
			return false;
		}
		if (avatar->mDrawable.isNull())
		{
			llwarns << "No drawable for avatar associated to rigged drawable"
					<< llendl;
			return false;
		}
		distance = avatar->mDrawable->mDistanceWRTCamera;
		if (avatar->isPuppetAvatar())
		{
			// Handle volumes in an animated object as a special case
			const LLVector3* box = avatar->getLastAnimExtents();
			radius = (box[1] - box[0]).length() * 0.5f;
		}
		else
		{
#if 1		// SL-937: add dynamic box handling for rigged mesh on regular
			// avatars
			const LLVector3* box = avatar->getLastAnimExtents();
			radius = (box[1] - box[0]).length();
#else
			radius = avatar->getBinRadius();
#endif
		}
	}
	else
	{
		if (volume)
		{
			radius = volume->mLODScaleBias.scaledVec(getScale()).length();
		}
		else
		{
			llwarns_once << "NULL volume associated with drawable " << std::hex
						 << mDrawable.get() << std::dec << llendl;
			radius = getScale().length();
		}
		distance = mDrawable->mDistanceWRTCamera;
	}

	if (distance <= 0.f || radius <= 0.f)
	{
		return false;
	}

	radius = ll_round(radius, 0.01f);
	distance *= sDistanceFactor;

	static LLCachedControl<F32> mesh_boost(gSavedSettings,
										   "MeshLODBoostFactor");
	F32 boost_factor = 1.f;
	if (mesh_boost > 1.f && isMesh())
	{
		boost_factor = llclamp((F32)mesh_boost, 1.f, 4.f);
	}

	// Boost LOD when you are REALLY close
	F32 ramp_dist = sLODFactor * 2.f * boost_factor;
	if (distance < ramp_dist)
	{
		distance /= ramp_dist;
		distance *= distance;
		distance *= ramp_dist;
	}
	distance = ll_round(distance * (F_PI / 3.f), 0.01f);

	static LLCachedControl<F32> hysteresis(gSavedSettings,
										   "DistanceHysteresisLOD");
	// Avoid blinking objects due to LOD changing every few frames because of
	// LOD-dependant (since bounding-box dependant) distance changes. HB
	if (mLastDistance > 0.f && fabs(mLastDistance - distance) < hysteresis)
	{
		distance = mLastDistance;
	}
	else
	{
		mLastDistance = distance;
	}

	F32 lod_factor = sLODFactor;
	static LLCachedControl<bool> ignore_fov_zoom(gSavedSettings,
												 "IgnoreFOVZoomForLODs");
	if (!ignore_fov_zoom)
	{
		lod_factor *= DEFAULT_FIELD_OF_VIEW / gViewerCamera.getDefaultFOV();
	}
	lod_factor *= boost_factor;

	S32 cur_detail = computeLODDetail(distance, radius, lod_factor);

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_LOD_INFO))
	{
		setDebugText(llformat("%d (d=%.2f/r=%.2f)", cur_detail, distance,
							  radius));
	}

	if (cur_detail == mLOD)
	{
		return false;
	}

	mAppAngle = ll_round(atan2f(mDrawable->getRadius(),
								mDrawable->mDistanceWRTCamera) * RAD_TO_DEG,
						 0.01f);
	mLOD = cur_detail;
	return true;
}

bool LLVOVolume::updateLOD()
{
	if (mDrawable.isNull())
	{
		return false;
	}

	if (calcLOD())
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);
		mLODChanged = true;
		return true;
	}

	F32 new_radius = getBinRadius();
	F32 old_radius = mDrawable->getBinRadius();
	if (new_radius < old_radius * 0.9f || new_radius > old_radius * 1.1f)
	{
		gPipeline.markPartitionMove(mDrawable);
	}

	return LLViewerObject::updateLOD();
}

void LLVOVolume::tempSetLOD(S32 lod)
{
	mLOD = lod;
	gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME, false);
	mLODChanged = true;
}

bool LLVOVolume::setDrawableParent(LLDrawable* parentp)
{
	if (!LLViewerObject::setDrawableParent(parentp))
	{
		// No change in drawable parent
		return false;
	}

	if (!mDrawable->isRoot())
	{
		// Rebuild vertices in parent relative space
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME, true);

		if (mDrawable->isActive() && !parentp->isActive())
		{
			parentp->makeActive();
		}
		else if (mDrawable->isStatic() && parentp->isActive())
		{
			mDrawable->makeActive();
		}
	}

	return true;
}

void LLVOVolume::updateFaceFlags()
{
	if (!mDrawable)
	{
		llwarns << "NULL drawable !" << llendl;
		return;
	}
	for (S32 i = 0, count = llmin(getVolume()->getNumFaces(),
								  mDrawable->getNumFaces());
		 i < count; ++i)
	{
		LLFace* face = mDrawable->getFace(i);
		if (face)
		{
			LLTextureEntry* te = getTE(i);
			if (!te) continue;

			bool fullbright = te->getFullbright();
			face->clearState(LLFace::FULLBRIGHT | LLFace::HUD_RENDER |
							 LLFace::LIGHT);

			if (fullbright || mMaterial == LL_MCODE_LIGHT)
			{
				face->setState(LLFace::FULLBRIGHT);
			}
			if (mDrawable->isLight())
			{
				face->setState(LLFace::LIGHT);
			}
			if (isHUDAttachment())
			{
				face->setState(LLFace::HUD_RENDER);
			}
		}
	}
}

bool LLVOVolume::setParent(LLViewerObject* parent)
{
	bool ret = false;

	LLViewerObject* old_parent = (LLViewerObject*)getParent();
	if (parent != old_parent)
	{
		ret = LLViewerObject::setParent(parent);
		if (ret && mDrawable)
		{
			gPipeline.markMoved(mDrawable);
			gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME, true);
		}
		onReparent(old_parent, parent);
	}

	return ret;
}

// NOTE: regenFaces() MUST be followed by genTriangles() !
void LLVOVolume::regenFaces()
{
	// Remove existing faces
	bool count_changed = mNumFaces != getNumTEs();
	if (count_changed)
	{
		deleteFaces();
		// Add new faces
		mNumFaces = getNumTEs();
	}

	S32 media_count = mMediaImplList.size();
	for (S32 i = 0; i < mNumFaces; ++i)
	{
		LLFace* facep = count_changed ? addFace(i) : mDrawable->getFace(i);
		if (!facep) continue;

		facep->setTEOffset(i);
		facep->setTexture(getTEImage(i));
		if (facep->getTextureEntry()->getMaterialParams().notNull())
		{
			facep->setNormalMap(getTENormalMap(i));
			facep->setSpecularMap(getTESpecularMap(i));
		}
		facep->setViewerObject(this);

		if (i >= media_count || mMediaImplList[i].isNull())
		{
			continue;
		}

		// If the face had media on it, this will have broken the link between
		// the LLViewerMediaTexture and the face. Re-establish the link.
		const LLUUID& id = mMediaImplList[i]->getMediaTextureID();
		LLViewerMediaTexture* media_tex =
			LLViewerTextureManager::findMediaTexture(id);
		if (media_tex)
		{
			media_tex->addMediaToFace(facep);
		}
	}

	if (!count_changed)
	{
		updateFaceFlags();
	}
}

bool LLVOVolume::genBBoxes(bool force_global)
{
	bool res = true;

	bool rebuild = mDrawable->isState(LLDrawable::REBUILD_VOLUME |
									  LLDrawable::REBUILD_POSITION |
									  LLDrawable::REBUILD_RIGGED);

	LLVolume* volume = mRiggedVolume.get();
	if (volume)
	{
		updateRiggedVolume();
	}
	else
	{
		volume = getVolume();
	}

	LLVector4a min, max;
	min.clear();
	max.clear();

	force_global |= mVolumeImpl && mVolumeImpl->isVolumeGlobal();
	bool any_valid_boxes = false;
	for (S32 i = 0, count = llmin(volume->getNumVolumeFaces(),
								  mDrawable->getNumFaces(),
								  (S32)getNumTEs()); i < count; ++i)
	{
		LLFace* face = mDrawable->getFace(i);
		if (!face)
		{
			continue;
		}
		bool face_res = face->genVolumeBBoxes(*volume, i, mRelativeXform,
											  force_global);
		res &= face_res;
		if (!face_res)
		{
			// MAINT-8264: ignore bboxes of ill-formed faces.
			continue;
		}
		if (rebuild)
		{
			if (!any_valid_boxes)
			{
				min = face->mExtents[0];
				max = face->mExtents[1];
				any_valid_boxes = true;
			}
			else
			{
				min.setMin(min, face->mExtents[0]);
				max.setMax(max, face->mExtents[1]);
			}
		}
	}

	if (any_valid_boxes)
	{
		if (rebuild)
		{
			mDrawable->setSpatialExtents(min, max);
			min.add(max);
			min.mul(0.5f);
			mDrawable->setPositionGroup(min);
		}

		updateRadius();
		mDrawable->movePartition();
	}

	return res;
}

void LLVOVolume::preRebuild()
{
	if (mVolumeImpl)
	{
		mVolumeImpl->preRebuild();
	}
}

void LLVOVolume::updateRelativeXform(bool force_identity)
{
	if (mVolumeImpl)
	{
		mVolumeImpl->updateRelativeXform(force_identity);
		return;
	}

	static LLVector3 vec3_x(1.f, 0.f, 0.f);
	static LLVector3 vec3_y(0.f, 1.f, 0.f);
	static LLVector3 vec3_z(0.f, 0.f, 1.f);

	LLDrawable* drawable = mDrawable;

	if (drawable->isState(LLDrawable::RIGGED) && mRiggedVolume.notNull())
	{
		// Rigged volume (which is in agent space) is used for generating
		// bounding boxes etc. Inverse of render matrix should go to
		// partition space
		mRelativeXform = getRenderMatrix();

		F32* dst = mRelativeXformInvTrans.getF32ptr();
		F32* src = mRelativeXform.getF32ptr();
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[4];
		dst[4] = src[5];
		dst[5] = src[6];
		dst[6] = src[8];
		dst[7] = src[9];
		dst[8] = src[10];

		mRelativeXform.invert();
		mRelativeXformInvTrans.transpose();
	}
	else if (drawable->isActive() || force_identity)
	{
		// Setup relative transforms
		LLQuaternion delta_rot;
		LLVector3 delta_pos;
		// Matrix from local space to parent relative/global space
		if (!force_identity && !drawable->isSpatialRoot())
		{
			delta_rot = mDrawable->getRotation();
			delta_pos = mDrawable->getPosition();
		}
		LLVector3 delta_scale = mDrawable->getScale();

		// Vertex transform (4x4)
		LLVector3 x_axis = LLVector3(delta_scale.mV[VX], 0.f, 0.f) * delta_rot;
		LLVector3 y_axis = LLVector3(0.f, delta_scale.mV[VY], 0.f) * delta_rot;
		LLVector3 z_axis = LLVector3(0.f, 0.f, delta_scale.mV[VZ]) * delta_rot;

		mRelativeXform.initRows(LLVector4(x_axis, 0.f),
								LLVector4(y_axis, 0.f),
								LLVector4(z_axis, 0.f),
								LLVector4(delta_pos, 1.f));

		// Compute inverse transpose for normals
		// grumble - invert is NOT a matrix invert, so we do it by hand:

		LLMatrix3 rot_inverse = LLMatrix3(~delta_rot);

		LLMatrix3 scale_inverse;
		scale_inverse.setRows(vec3_x / delta_scale.mV[VX],
							  vec3_y / delta_scale.mV[VY],
							  vec3_z / delta_scale.mV[VZ]);

		mRelativeXformInvTrans = rot_inverse * scale_inverse;

		mRelativeXformInvTrans.transpose();
	}
	else
	{
		LLVector3 pos = getPosition();
		LLVector3 scale = getScale();
		LLQuaternion rot = getRotation();

		if (mParent)
		{
			pos *= mParent->getRotation();
			pos += mParent->getPosition();
			rot *= mParent->getRotation();
		}
#if 0
		pos += getRegion()->getOriginAgent();
#endif

		LLVector3 x_axis = LLVector3(scale.mV[VX], 0.f, 0.f) * rot;
		LLVector3 y_axis = LLVector3(0.f, scale.mV[VY], 0.f) * rot;
		LLVector3 z_axis = LLVector3(0.f, 0.f, scale.mV[VZ]) * rot;

		mRelativeXform.initRows(LLVector4(x_axis, 0.f),
								LLVector4(y_axis, 0.f),
								LLVector4(z_axis, 0.f),
								LLVector4(pos, 1.f));

		// Compute inverse transpose for normals
		LLMatrix3 rot_inverse = LLMatrix3(~rot);

		LLMatrix3 scale_inverse;
		scale_inverse.setRows(vec3_x / scale.mV[VX],
							  vec3_y / scale.mV[VY],
							  vec3_z / scale.mV[VZ]);

		mRelativeXformInvTrans = rot_inverse * scale_inverse;

		mRelativeXformInvTrans.transpose();
	}
}

bool LLVOVolume::lodOrSculptChanged(LLDrawable* drawable)
{
	LLVolume* old_volumep = getVolume();
	if (!old_volumep)
	{
		return false;
	}
	F32 old_lod = old_volumep->getDetail();
	S32 old_num_faces = old_volumep->getNumFaces();;
	old_volumep = NULL;

	LLVolume* new_volumep = getVolume();
	{
		LL_FAST_TIMER(FTM_GEN_VOLUME);
		const LLVolumeParams& volume_params = new_volumep->getParams();
		setVolume(volume_params, 0);
	}
	F32 new_lod = new_volumep->getDetail();
	S32 new_num_faces = new_volumep->getNumFaces();
	new_volumep = NULL;

	bool regen_faces = false;
	if (new_lod != old_lod || mSculptChanged)
	{
		if (mDrawable->isState(LLDrawable::RIGGED))
		{
			updateVisualComplexity();
		}

		sNumLODChanges += new_num_faces;

		if ((S32)getNumTEs() != getVolume()->getNumFaces())
		{
			// Mesh loading may change number of faces.
			setNumTEs(getVolume()->getNumFaces());
		}

		// For face->genVolumeTriangles()
		drawable->setState(LLDrawable::REBUILD_VOLUME);

		{
			LL_FAST_TIMER(FTM_GEN_TRIANGLES);
			regen_faces = new_num_faces != old_num_faces ||
						  mNumFaces != (S32)getNumTEs();
			if (regen_faces)
			{
				regenFaces();
			}

			if (mSculptChanged)
			{
				// Changes in sculpt maps can thrash an object bounding box
				// without triggering a spatial group bounding box update:
				// force spatial group to update bounding boxes.
				LLSpatialGroup* group = mDrawable->getSpatialGroup();
				if (group)
				{
					group->unbound();
				}
			}
		}
	}

	return regen_faces;
}

bool LLVOVolume::updateGeometry(LLDrawable* drawable)
{
	LL_FAST_TIMER(FTM_UPDATE_PRIMITIVES);

	if (isDead() || !drawable || drawable->isDead() || mDrawable.isNull() ||
		mDrawable->isDead())
	{
		return true;
	}

	if (mDrawable->isState(LLDrawable::REBUILD_RIGGED))
	{
		updateRiggedVolume();
		genBBoxes(false);
		mDrawable->clearState(LLDrawable::REBUILD_RIGGED);
	}

	if (mVolumeImpl)
	{
		bool res;
		{
			LL_FAST_TIMER(FTM_GEN_FLEX);
			res = mVolumeImpl->doUpdateGeometry(drawable);
		}
		updateFaceFlags();
		return res;
	}

	LLSpatialGroup* group = drawable->getSpatialGroup();
	if (group)
	{
		group->dirtyMesh();
	}

	updateRelativeXform();

	// Not sure why this is happening, but it is...
	if (mDrawable.isNull() || mDrawable->isDead())
	{
		llwarns << "NULL or dead drawable detected. Aborted." << llendl;
		return true; // No update to complete
	}

	if (mVolumeChanged || mFaceMappingChanged)
	{
		dirtySpatialGroup(drawable->isState(LLDrawable::IN_REBUILD_Q1));

		bool was_regen_faces = false;
		if (mVolumeChanged)
		{
			was_regen_faces = lodOrSculptChanged(drawable);
			drawable->setState(LLDrawable::REBUILD_VOLUME);
		}
		else if (mSculptChanged || mLODChanged || mColorChanged)
		{
			was_regen_faces = lodOrSculptChanged(drawable);
		}

		if (!was_regen_faces)
		{
			LL_FAST_TIMER(FTM_GEN_TRIANGLES);
			regenFaces();
		}

		genBBoxes(false);
	}
	else if (mLODChanged || mSculptChanged || mColorChanged)
	{
		dirtySpatialGroup(drawable->isState(LLDrawable::IN_REBUILD_Q1));
		lodOrSculptChanged(drawable);
		if (drawable->isState(LLDrawable::REBUILD_RIGGED | LLDrawable::RIGGED))
		{
			updateRiggedVolume();
		}
		genBBoxes(false);
	}
	else
	{
		// It has its own drawable (it has moved) or it has changed UVs or it
		// has changed xforms from global<->local
		genBBoxes(false);
	}

	// Update face flags
	updateFaceFlags();

	mVolumeChanged = mLODChanged = mSculptChanged = mColorChanged =
					 mFaceMappingChanged = false;

	return LLViewerObject::updateGeometry(drawable);
}

//virtual
void LLVOVolume::updateFaceSize(S32 idx)
{
	if (mDrawable->getNumFaces() <= idx)
	{
		return;
	}

	LLFace* facep = mDrawable->getFace(idx);
	if (!facep)
	{
		return;
	}

	if (idx >= getVolume()->getNumVolumeFaces())
	{
		facep->setSize(0, 0, true);
	}
	else
	{
		const LLVolumeFace& vol_face = getVolume()->getVolumeFace(idx);
		facep->setSize(vol_face.mNumVertices, vol_face.mNumIndices,
					   // volume faces must be padded for 16-byte alignment
					   true);
	}
}

//virtual
void LLVOVolume::setNumTEs(U8 num_tes)
{
	U8 old_num_tes = getNumTEs();

	if (old_num_tes && old_num_tes < num_tes) // new faces added
	{
		LLViewerObject::setNumTEs(num_tes);

		// Duplicate the last media textures if exists.
		if (mMediaImplList.size() >= old_num_tes &&
			mMediaImplList[old_num_tes - 1].notNull())
		{
			mMediaImplList.resize(num_tes);
			const LLTextureEntry* te = getTE(old_num_tes - 1);
			for (U8 i = old_num_tes; i < num_tes; ++i)
			{
				setTE(i, *te);
				mMediaImplList[i] = mMediaImplList[old_num_tes - 1];
			}
			mMediaImplList[old_num_tes - 1]->setUpdated(true);
		}
		return;
	}

	if (old_num_tes > num_tes && mMediaImplList.size() > num_tes)
	{
		// Old faces removed
		for (U8 i = num_tes, end = mMediaImplList.size(); i < end; ++i)
		{
			removeMediaImpl(i);
		}
		mMediaImplList.resize(num_tes);
	}

	LLViewerObject::setNumTEs(num_tes);
}

//virtual
void LLVOVolume::setTEImage(U8 te, LLViewerTexture* imagep)
{
	bool changed = mTEImages[te] != imagep;
	LLViewerObject::setTEImage(te, imagep);
	if (changed)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
}

//virtual
S32 LLVOVolume::setTETexture(U8 te, const LLUUID& uuid)
{
	S32 res = LLViewerObject::setTETexture(te, uuid);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEColor(U8 te, const LLColor3& color)
{
	return setTEColor(te, LLColor4(color));
}

//virtual
S32 LLVOVolume::setTEColor(U8 te, const LLColor4& color)
{
	const LLTextureEntry* tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID
				<< llendl;
		return 0;
	}

	if (color == tep->getColor())
	{
		return 0;
	}

	if (color.mV[3] != tep->getColor().mV[3])
	{
		gPipeline.markTextured(mDrawable);
		// Treat this alpha change as an LoD update since render batches may
		// need to get rebuilt
		mLODChanged = true;
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME, false);
	}

	S32 retval = LLPrimitive::setTEColor(te, color);
	if (retval && mDrawable.notNull())
	{
		// These should only happen on updates which are not the initial
		// update.
		mColorChanged = true;
		mDrawable->setState(LLDrawable::REBUILD_COLOR);
		dirtyMesh();
	}
	return  retval;
}

//virtual
S32 LLVOVolume::setTEBumpmap(U8 te, U8 bumpmap)
{
	S32 res = LLViewerObject::setTEBumpmap(te, bumpmap);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return  res;
}

//virtual
S32 LLVOVolume::setTETexGen(U8 te, U8 texgen)
{
	S32 res = LLViewerObject::setTETexGen(te, texgen);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return  res;
}

//virtual
S32 LLVOVolume::setTEMediaTexGen(U8 te, U8 media)
{
	S32 res = LLViewerObject::setTEMediaTexGen(te, media);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return  res;
}

//virtual
S32 LLVOVolume::setTEShiny(U8 te, U8 shiny)
{
	S32 res = LLViewerObject::setTEShiny(te, shiny);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return  res;
}

//virtual
S32 LLVOVolume::setTEFullbright(U8 te, U8 fullbright)
{
	S32 res = LLViewerObject::setTEFullbright(te, fullbright);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return  res;
}

//virtual
S32 LLVOVolume::setTEBumpShinyFullbright(U8 te, U8 bump)
{
	S32 res = LLViewerObject::setTEBumpShinyFullbright(te, bump);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEMediaFlags(U8 te, U8 media_flags)
{
	S32 res = LLViewerObject::setTEMediaFlags(te, media_flags);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return  res;
}

//virtual
S32 LLVOVolume::setTEGlow(U8 te, F32 glow)
{
	S32 res = LLViewerObject::setTEGlow(te, glow);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return  res;
}

//static
void LLVOVolume::setTEMaterialParamsCallbackTE(const LLUUID& objid,
											   const LLMaterialID& matidp,
											   const LLMaterialPtr paramsp,
											   U32 te)
{
	LLVOVolume* pvol = (LLVOVolume*)gObjectList.findObject(objid);
	if (!pvol) return;	// stale callback for removed object

	if (te >= pvol->getNumTEs())
	{
		llwarns << "Got a callback for materialid " << matidp.asString()
				<< " with an out of range face number: " << te << ". Ignoring."
				<< llendl;
		return;
	}

	LLTextureEntry* tep = pvol->getTE(te);
	if (tep && tep->getMaterialID() == matidp)
	{
		LL_DEBUGS("Materials") << "Applying materialid " << matidp.asString()
							   << " to face " << te << LL_ENDL;
		pvol->setTEMaterialParams(te, paramsp);
	}
}

//virtual
S32 LLVOVolume::setTEMaterialID(U8 te, const LLMaterialID& matidp)
{
	S32 res = LLViewerObject::setTEMaterialID(te, matidp);

	LL_DEBUGS("Materials") << "te = "<< (S32)te << " - materialid = "
						   << matidp.asString() << " - result: " << res;
	bool sel = gSelectMgr.getSelection()->contains(this, te);
	LL_CONT << (sel ? "," : ", not") << " selected" << LL_ENDL;

	if (res == TEM_CHANGE_NONE)
	{
		return res;
	}

	LLViewerRegion* regionp = getRegion();
	if (!regionp)
	{
		return res;
	}

	LLMaterialMgr* matmgrp = LLMaterialMgr::getInstance();
	matmgrp->getTE(regionp->getRegionID(), matidp, te,
				   boost::bind(&LLVOVolume::setTEMaterialParamsCallbackTE,
							   getID(), _1, _2, _3));
	setChanged(ALL_CHANGED);
	if (mDrawable.notNull())
	{
		gPipeline.markTextured(mDrawable);
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_ALL);
	}
	mFaceMappingChanged = true;

	return res;
}

// Here, we have confirmation about texture creation, check our wait-list and
// make changes, or return false
bool LLVOVolume::notifyAboutCreatingTexture(LLViewerTexture* texture)
{
	std::pair<mmap_uuid_map_t::iterator, mmap_uuid_map_t::iterator> range =
		mWaitingTextureInfo.equal_range(texture->getID());

	typedef fast_hmap<U8, LLMaterialPtr> map_te_material_t;
	map_te_material_t new_material;

	for (mmap_uuid_map_t::iterator it = range.first;
		 it != range.second; ++it)
	{
		LLMaterialPtr cur_material = getTEMaterialParams(it->second.te);
		if (cur_material.isNull()) continue;

		// Here we have interest in DIFFUSE_MAP only !
		if (it->second.map == LLRender::DIFFUSE_MAP &&
			texture->getPrimaryFormat() != GL_RGBA)
		{
			// Let's check the diffuse mode
			U8 mode = cur_material->getDiffuseAlphaMode();
			if (mode == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND ||
				mode == LLMaterial::DIFFUSE_ALPHA_MODE_EMISSIVE ||
				mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
			{
				// We have non 32 bits texture with DIFFUSE_ALPHA_MODE_* so set
				// mode to DIFFUSE_ALPHA_MODE_NONE instead
				LLMaterialPtr mat = NULL;
				map_te_material_t::iterator it2 =
					new_material.find(it->second.te);
				if (it2 == new_material.end())
				{
					mat = new LLMaterial(cur_material->asLLSD());
					new_material.emplace(it->second.te, mat);
				}
				else
				{
					mat = it2->second;
				}
				mat->setDiffuseAlphaMode(LLMaterial::DIFFUSE_ALPHA_MODE_NONE);
			}
		}
	}

	if (new_material.empty() || !getRegion())
	{
		// Clear the wait-list
		mWaitingTextureInfo.erase(range.first, range.second);
		return false;
	}

	// Setup new materials
	const LLUUID& regiond_id = getRegion()->getRegionID();
	LLMaterialMgr* matmgr = LLMaterialMgr::getInstance();
	for (map_te_material_t::const_iterator it = new_material.begin(),
										   end = new_material.end();
		 it != end; ++it)
	{
		matmgr->setLocalMaterial(regiond_id, it->second);
		LLViewerObject::setTEMaterialParams(it->first, it->second);
	}

	// Clear the wait-list
	mWaitingTextureInfo.erase(range.first, range.second);

	return true;
}

// Here, if we wait information about the texture and it is missing, then
// depending on the texture map (diffuse, normal, or specular), we make changes
// in material and confirm it. If not return false.
bool LLVOVolume::notifyAboutMissingAsset(LLViewerTexture* texture)
{
	std::pair<mmap_uuid_map_t::iterator, mmap_uuid_map_t::iterator> range =
		mWaitingTextureInfo.equal_range(texture->getID());
	if (range.first == range.second)
	{
		return false;
	}

	typedef fast_hmap<U8, LLMaterialPtr> map_te_material_t;
	map_te_material_t new_material;

	for (mmap_uuid_map_t::iterator it = range.first;
		 it != range.second; ++it)
	{
		LLMaterialPtr cur_material = getTEMaterialParams(it->second.te);
		if (cur_material.isNull()) continue;

		switch (it->second.map)
		{
			case LLRender::DIFFUSE_MAP:
			{
				if (cur_material->getDiffuseAlphaMode() !=
						LLMaterial::DIFFUSE_ALPHA_MODE_NONE)
				{
					// Switch to DIFFUSE_ALPHA_MODE_NONE
					LLMaterialPtr mat;
					map_te_material_t::iterator it2 =
						new_material.find(it->second.te);
					if (it2 == new_material.end())
					{
						mat = new LLMaterial(cur_material->asLLSD());
						new_material.emplace(it->second.te, mat);
					}
					else
					{
						mat = it2->second;
					}
					mat->setDiffuseAlphaMode(LLMaterial::DIFFUSE_ALPHA_MODE_NONE);
				}
				break;
			}

			case LLRender::NORMAL_MAP:
			{
				// Reset material texture id
				LLMaterialPtr mat;
				map_te_material_t::iterator it2 =
					 new_material.find(it->second.te);
				if (it2 == new_material.end())
				{
					mat = new LLMaterial(cur_material->asLLSD());
					new_material.emplace(it->second.te, mat);
				}
				else
				{
					mat = it2->second;
				}
				mat->setNormalID(LLUUID::null);
				break;
			}

			case LLRender::SPECULAR_MAP:
			{
				// Reset material texture id
				LLMaterialPtr mat;
				map_te_material_t::iterator it2 =
					new_material.find(it->second.te);
				if (it2 == new_material.end())
				{
					mat = new LLMaterial(cur_material->asLLSD());
					new_material.emplace(it->second.te, mat);
				}
				else
				{
					mat = it2->second;
				}
				mat->setSpecularID(LLUUID::null);
			}

			default:
				break;
		}
	}

	if (new_material.empty() || !getRegion())
	{
		// Clear the wait-list
		mWaitingTextureInfo.erase(range.first, range.second);
		return false;
	}

	// Setup new materials
	const LLUUID& regiond_id = getRegion()->getRegionID();
	LLMaterialMgr* matmgr = LLMaterialMgr::getInstance();
	for (map_te_material_t::const_iterator it = new_material.begin(),
										   end = new_material.end();
		 it != end; ++it)
	{
		matmgr->setLocalMaterial(regiond_id, it->second);
		LLViewerObject::setTEMaterialParams(it->first, it->second);
	}

	// Clear the wait-list
	mWaitingTextureInfo.erase(range.first, range.second);

	return true;
}

//virtual
S32 LLVOVolume::setTEMaterialParams(U8 te, const LLMaterialPtr paramsp)
{
	LLMaterialPtr matp = const_cast<LLMaterialPtr&>(paramsp);
	if (paramsp)
	{
		LLMaterialPtr new_material = NULL;
		LLViewerTexture* img_diffuse = getTEImage(te);
		if (img_diffuse)
		{
			if (img_diffuse->getPrimaryFormat() == 0 &&
				!img_diffuse->isMissingAsset())
			{
				// Texture information is missing, wait for it
				mWaitingTextureInfo.emplace(img_diffuse->getID(),
											material_info(LLRender::DIFFUSE_MAP,
														  te));
			}
			else
			{
				bool set_diffuse_none = img_diffuse->isMissingAsset();
				if (!set_diffuse_none)
				{
					U8 mode = paramsp->getDiffuseAlphaMode();
					if (mode == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND ||
						mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK ||
						mode == LLMaterial::DIFFUSE_ALPHA_MODE_EMISSIVE)
					{
						LLTextureEntry* tep = getTE(te);
						bool baked = tep &&
									 LLAvatarAppearanceDictionary::isBakedImageId(tep->getID());
						if (!baked &&
							img_diffuse->getPrimaryFormat() != GL_RGBA)
						{
							set_diffuse_none = true;
						}
					}
				}
				if (set_diffuse_none)
				{
					// Substitute this material with DIFFUSE_ALPHA_MODE_NONE
					new_material = new LLMaterial(paramsp->asLLSD());
					new_material->setDiffuseAlphaMode(LLMaterial::DIFFUSE_ALPHA_MODE_NONE);
				}
			}
		}
		else
		{
			llwarns_once << "Missing diffuse channel for material !" << llendl;
			llassert(false);
		}

		const LLUUID& normal_id = paramsp->getNormalID();
		if (normal_id.notNull())
		{
			LLViewerTexture* img_normal = getTENormalMap(te);
			if (img_normal && img_normal->isMissingAsset() &&
				img_normal->getID() == normal_id)
			{
				if (!new_material)
				{
					new_material = new LLMaterial(paramsp->asLLSD());
				}
				new_material->setNormalID(LLUUID::null);
			}
			else if (!img_normal || img_normal->getPrimaryFormat() == 0)
			{
				// Texture information is missing, wait for it
				mWaitingTextureInfo.emplace(normal_id,
											material_info(LLRender::NORMAL_MAP,
														  te));
			}
		}

		const LLUUID& specular_id = paramsp->getSpecularID();
		if (specular_id.notNull())
		{
			LLViewerTexture* img_specular = getTESpecularMap(te);
			if (img_specular && img_specular->isMissingAsset() &&
				img_specular->getID() == specular_id)
			{
				if (!new_material)
				{
					new_material = new LLMaterial(paramsp->asLLSD());
				}
				new_material->setSpecularID(LLUUID::null);
			}
			else if (!img_specular || img_specular->getPrimaryFormat() == 0)
			{
				// Texture information is missing, wait for it
				mWaitingTextureInfo.emplace(specular_id,
											material_info(LLRender::SPECULAR_MAP,
														  te));
			}
		}

		if (new_material && getRegion())
		{
			matp = new_material;
			const LLUUID& regiond_id = getRegion()->getRegionID();
			LLMaterialMgr::getInstance()->setLocalMaterial(regiond_id, matp);
		}
	}

	S32 res = LLViewerObject::setTEMaterialParams(te, matp);

	LL_DEBUGS("Materials") << "te = "<< (S32)te << ", material = "
						   << (paramsp ? paramsp->asLLSD() : LLSD("null"))
						   << ", res = " << res;
	bool sel = gSelectMgr.getSelection()->contains(this, te);
	LL_CONT << (sel ? "," : ", not") << " selected" << LL_ENDL;

	setChanged(ALL_CHANGED);
	if (mDrawable.notNull())
	{
		gPipeline.markTextured(mDrawable);
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_ALL);
	}
	mFaceMappingChanged = true;

	return TEM_CHANGE_TEXTURE;
}

//virtual
S32 LLVOVolume::setTEScale(U8 te, F32 s, F32 t)
{
	S32 res = LLViewerObject::setTEScale(te, s, t);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEScaleS(U8 te, F32 s)
{
	S32 res = LLViewerObject::setTEScaleS(te, s);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

//virtual
S32 LLVOVolume::setTEScaleT(U8 te, F32 t)
{
	S32 res = LLViewerObject::setTEScaleT(te, t);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = true;
	}
	return res;
}

bool LLVOVolume::hasMedia() const
{
	for (U8 i = 0, count = getNumTEs(); i < count; ++i)
	{
		const LLTextureEntry* te = getTE(i);
		if (te && te->hasMedia())
		{
			return true;
		}
	}
	return false;
}

LLVector3 LLVOVolume::getApproximateFaceNormal(U8 face_id)
{
	LLVolume* volume = getVolume();
	if (volume && face_id < volume->getNumVolumeFaces())
	{
		LLVector4a result;
		// Necessary since LLVector4a members are not initialized on creation
		result.clear();

		const LLVolumeFace& face = volume->getVolumeFace(face_id);
		for (S32 i = 0, count = face.mNumVertices; i < count; ++i)
		{
			result.add(face.mNormals[i]);
		}

		LLVector3 ret(result.getF32ptr());
		ret = volumeDirectionToAgent(ret);
		ret.normalize();
		return ret;
	}

	return LLVector3();
}

void LLVOVolume::requestMediaDataUpdate(bool isNew)
{
	if (sObjectMediaClient)
	{
		sObjectMediaClient->fetchMedia(new LLMediaDataClientObjectImpl(this,
																	   isNew));
	}
}

bool LLVOVolume::isMediaDataBeingFetched() const
{
	if (!sObjectMediaClient)
	{
		return false;
	}
	// It is OK to const_cast this away since this is just a wrapper class that
	// is only going to do a lookup.
	LLVOVolume* self = const_cast<LLVOVolume*>(this);
	return sObjectMediaClient->isInQueue(new LLMediaDataClientObjectImpl(self,
																		 false));
}

void LLVOVolume::cleanUpMediaImpls()
{
	// Iterate through our TEs and remove any Impls that are no longer used
	for (U8 i = 0, count = getNumTEs(); i < count; ++i)
	{
		const LLTextureEntry* te = getTE(i);
		if (te && !te->hasMedia())
		{
			// Delete the media implement !
			removeMediaImpl(i);
		}
	}
}

// media_data_array is an array of media entry maps, media_version is the
// version string in the response.
void LLVOVolume::updateObjectMediaData(const LLSD& media_data_array,
									   const std::string& media_version)
{
	U32 fetched_version =
		LLTextureEntry::getVersionFromMediaVersionString(media_version);

	// Only update it if it is newer !
	if ((S32)fetched_version <= mLastFetchedMediaVersion)
	{
		return;
	}

	mLastFetchedMediaVersion = fetched_version;

	LLSD::array_const_iterator iter = media_data_array.beginArray();
	LLSD::array_const_iterator end = media_data_array.endArray();
	U8 texture_index = 0;
	for ( ; iter != end; ++iter, ++texture_index)
	{
		syncMediaData(texture_index, *iter, false, false);
	}
}

void LLVOVolume::syncMediaData(S32 texture_index, const LLSD& media_data,
							   bool merge, bool ignore_agent)
{
	if (mDead)
	{
		// If the object has been marked dead, do not process media updates.
		return;
	}

	LLTextureEntry* te = getTE(texture_index);
	if (!te)
	{
		return;
	}

	LL_DEBUGS("MediaOnAPrim") << "BEFORE: texture_index = " << texture_index
							  << " hasMedia = " << te->hasMedia() << " : "
							  << (!te->getMediaData() ? "NULL MEDIA DATA" :
								  ll_pretty_print_sd(te->getMediaData()->asLLSD()))
							  << LL_ENDL;

	std::string previous_url;
	LLMediaEntry* mep = te->getMediaData();
	if (mep)
	{
		// Save the "current url" from before the update so we can tell if
		// it changes.
		previous_url = mep->getCurrentURL();
	}

	if (merge)
	{
		te->mergeIntoMediaData(media_data);
	}
	else
	{
		// XXX Question: what if the media data is undefined LLSD, but the
		// update we got above said that we have media flags??	Here we clobber
		// that, assuming the data from the service is more up-to-date.
		te->updateMediaData(media_data);
	}

	mep = te->getMediaData();
	if (mep)
	{
		bool update_from_self = !ignore_agent &&
			LLTextureEntry::getAgentIDFromMediaVersionString(getMediaURL()) == gAgentID;
		viewer_media_t media_impl =
		LLViewerMedia::updateMediaImpl(mep, previous_url, update_from_self);
		addMediaImpl(media_impl, texture_index);
	}
	else
	{
		removeMediaImpl(texture_index);
	}

	LL_DEBUGS("MediaOnAPrim") << "AFTER: texture_index = " << texture_index
							  << " hasMedia = " << te->hasMedia() << " : "
							  << (!te->getMediaData() ? "NULL MEDIA DATA"
													  : ll_pretty_print_sd(te->getMediaData()->asLLSD()))
							  << LL_ENDL;
}

void LLVOVolume::mediaNavigateBounceBack(U8 texture_index)
{
	// Find the media entry for this navigate
	const LLMediaEntry* mep = NULL;
	viewer_media_t impl = getMediaImpl(texture_index);
	LLTextureEntry* te = getTE(texture_index);
	if (te)
	{
		mep = te->getMediaData();
	}

	if (mep && impl)
	{
		std::string url = mep->getCurrentURL();
		// Look for a ":", if not there, assume "http://"
		if (!url.empty() && url.find(':') == std::string::npos)
		{
			url = "http://" + url;
		}
		// If the url we are trying to "bounce back" to either empty or not
		// allowed by the whitelist, try the home url. If *that* does not work,
		// set the media as failed and unload it
		if (url.empty() || !mep->checkCandidateUrl(url))
		{
			url = mep->getHomeURL();
			// Look for a ":", if not there, assume "http://"
			if (!url.empty() && url.find(':') == std::string::npos)
			{
				url = "http://" + url;
			}
		}
		if (url.empty() || !mep->checkCandidateUrl(url))
		{
			// The url to navigate back to is not good, and we have nowhere
			// else to go.
			llwarns << "FAILED to bounce back URL \"" << url
					<< "\" -- unloading impl" << llendl;
			impl->setMediaFailed(true);
		}
		else if (impl->getCurrentMediaURL() != url)
		{
			// Okay, navigate now
			llinfos << "bouncing back to URL: " << url << llendl;
			impl->navigateTo(url, "", false, true);
		}
	}
}

bool LLVOVolume::hasMediaPermission(const LLMediaEntry* media_entry,
									MediaPermType perm_type)
{
	// NOTE: This logic ALMOST duplicates the logic in the server (in
	// particular, in llmediaservice.cpp).
	if (!media_entry) return false; // XXX should we assert here?

	// The agent has permissions if:
	// - world permissions are on, or
	// - group permissions are on, and agent_id is in the group, or
	// - agent permissions are on, and agent_id is the owner

	// *NOTE: We *used* to check for modify permissions here (i.e. permissions
	// were granted if permModify() was true).  However, this doesn't make
	// sense in the viewer: we do not want to show controls or allow
	// interaction if the author has deemed it so. See DEV-42115.

	U8 media_perms = perm_type ==
		MEDIA_PERM_INTERACT ? media_entry->getPermsInteract()
							: media_entry->getPermsControl();

	// World permissions
	if ((media_perms & LLMediaEntry::PERM_ANYONE) != 0)
	{
		return true;
	}
	// Group permissions
	else if ((media_perms & LLMediaEntry::PERM_GROUP) != 0)
	{
		LLPermissions* obj_perm;
		obj_perm = gSelectMgr.findObjectPermissions(this);
		if (obj_perm && gAgent.isInGroup(obj_perm->getGroup()))
		{
			return true;
		}
	}
	// Owner permissions
	else if ((media_perms & LLMediaEntry::PERM_OWNER) != 0 && permYouOwner())
	{
		return true;
	}

	return false;
}

void LLVOVolume::mediaNavigated(LLViewerMediaImpl* impl,
								LLPluginClassMedia* plugin,
								std::string new_location)
{
	bool block_navigation = false;
	// FIXME: if/when we allow the same media impl to be used by multiple
	// faces, the logic here will need to be fixed to deal with multiple face
	// indices.
	S32 face_index = getFaceIndexWithMediaImpl(impl, -1);

	// Find the media entry for this navigate
	LLMediaEntry* mep = NULL;
	LLTextureEntry* te = getTE(face_index);
	if (te)
	{
		mep = te->getMediaData();
	}

	if (mep)
	{
		if (!mep->checkCandidateUrl(new_location))
		{
			block_navigation = true;
		}
		if (!block_navigation && !hasMediaPermission(mep, MEDIA_PERM_INTERACT))
		{
			block_navigation = true;
		}
	}
	else
	{
		llwarns_once << "Could not find media entry" << llendl;
	}

	if (block_navigation)
	{
		llinfos << "blocking navigate to URI " << new_location << llendl;

		// "bounce back" to the current URL from the media entry
		mediaNavigateBounceBack(face_index);
	}
	else if (sObjectMediaNavigateClient)
	{

		LL_DEBUGS("MediaOnAPrim") << "broadcasting navigate with URI "
								  << new_location << LL_ENDL;

		sObjectMediaNavigateClient->navigate(new LLMediaDataClientObjectImpl(this,
																			 false),
											 face_index, new_location);
	}
}

void LLVOVolume::mediaEvent(LLViewerMediaImpl* impl,
							LLPluginClassMedia* plugin,
							LLViewerMediaObserver::EMediaEvent event)
{
	switch (event)
	{
		case LLViewerMediaObserver::MEDIA_EVENT_LOCATION_CHANGED:
		{
			switch (impl->getNavState())
			{
				case LLViewerMediaImpl::MEDIANAVSTATE_FIRST_LOCATION_CHANGED:
					// This is the first location changed event after the start
					// of a non-server-directed nav. It may need to be
					// broadcast or bounced back.
					mediaNavigated(impl, plugin, plugin->getLocation());
					break;

				case LLViewerMediaImpl::MEDIANAVSTATE_FIRST_LOCATION_CHANGED_SPURIOUS:
					// This navigate didn't change the current URL.
					LL_DEBUGS("MediaOnAPrim") << "NOT broadcasting navigate (spurious)"
											  << LL_ENDL;
					break;

				case LLViewerMediaImpl::MEDIANAVSTATE_SERVER_FIRST_LOCATION_CHANGED:
					// This is the first location changed event after the start
					// of a server-directed nav. do not broadcast it.
					llinfos << "NOT broadcasting navigate (server-directed)"
							<< llendl;
					break;

				default:
					// This is a subsequent location-changed due to a redirect.
					// do not broadcast.
					llinfos << "NOT broadcasting navigate (redirect)"
							<< llendl;
			}
			break;
		}

		case LLViewerMediaObserver::MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			switch (impl->getNavState())
			{
				case LLViewerMediaImpl::MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED:
					// This is the first location changed event after the start
					// of a non-server-directed nav. It may need to be
					// broadcast or bounced back.
					mediaNavigated(impl, plugin, plugin->getNavigateURI());
					break;

				case LLViewerMediaImpl::MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED_SPURIOUS:
					// This navigate didn't change the current URL.
					LL_DEBUGS("MediaOnAPrim") << "NOT broadcasting navigate (spurious)"
											  << LL_ENDL;
					break;

				case LLViewerMediaImpl::MEDIANAVSTATE_SERVER_COMPLETE_BEFORE_LOCATION_CHANGED:
					// This is the the navigate complete event from a
					// server-directed nav. do not broadcast it.
					llinfos << "NOT broadcasting navigate (server-directed)"
							<< llendl;
					break;

				default:
					// For all other states, the navigate should have been
					// handled by LOCATION_CHANGED events already.
					break;
			}
			break;
		}

		default:
			break;
	}
}

void LLVOVolume::sendMediaDataUpdate()
{
	if (sObjectMediaClient)
	{
		sObjectMediaClient->updateMedia(new LLMediaDataClientObjectImpl(this,
																		false));
	}
}

void LLVOVolume::removeMediaImpl(S32 tex_index)
{
	S32 media_count = mMediaImplList.size();
	if (tex_index >= media_count || mMediaImplList[tex_index].isNull())
	{
		return;
	}

	// Make the face referencing to mMediaImplList[tex_index] to point back
	// to the old texture.
	if (mDrawable && tex_index < mDrawable->getNumFaces())
	{
		LLFace* facep = mDrawable->getFace(tex_index);
		if (facep)
		{
			const LLUUID& id = mMediaImplList[tex_index]->getMediaTextureID();
			LLViewerMediaTexture* media_tex =
				LLViewerTextureManager::findMediaTexture(id);
			if (media_tex)
			{
				media_tex->removeMediaFromFace(facep);
			}
		}
	}

	// Check if some other face(s) of this object reference(s)to this media
	// impl.
	S32 i;
	for (i = 0; i < media_count; ++i)
	{
		if (i != tex_index && mMediaImplList[i] == mMediaImplList[tex_index])
		{
			break;
		}
	}

	if (i == media_count)	// This object does not need this media impl.
	{
		mMediaImplList[tex_index]->removeObject(this);
	}

	mMediaImplList[tex_index] = NULL;
}

void LLVOVolume::addMediaImpl(LLViewerMediaImpl* media_impl, S32 texture_index)
{
	if ((S32)mMediaImplList.size() < texture_index + 1)
	{
		mMediaImplList.resize(texture_index + 1);
	}

	if (mMediaImplList[texture_index].notNull())
	{
		if (mMediaImplList[texture_index] == media_impl)
		{
			return;
		}

		removeMediaImpl(texture_index);
	}

	mMediaImplList[texture_index] = media_impl;
	media_impl->addObject(this);

	// Add the face to show the media if it is in playing
	if (mDrawable)
	{
		LLFace* facep = NULL;
		if (texture_index < mDrawable->getNumFaces())
		{
			facep = mDrawable->getFace(texture_index);
		}
		if (facep)
		{
			LLViewerMediaTexture* media_tex =
				LLViewerTextureManager::findMediaTexture(mMediaImplList[texture_index]->getMediaTextureID());
			if (media_tex)
			{
				media_tex->addMediaToFace(facep);
			}
		}
		else // The face is not available now, start media on this face later.
		{
			media_impl->setUpdated(true);
		}
	}
}

viewer_media_t LLVOVolume::getMediaImpl(U8 face_id) const
{
	if (face_id < mMediaImplList.size())
	{
		return mMediaImplList[face_id];
	}
	return NULL;
}

F64 LLVOVolume::getTotalMediaInterest() const
{
	// If this object is currently focused, this object has "high" interest
	if (LLViewerMediaFocus::getInstance()->getFocusedObjectID() == getID())
	{
		return F64_MAX;
	}

	F64 interest = -1.0;  // means not interested;

	// If this object is selected, this object has "high" interest, but since
	// there can be more than one, we still add in calculated impl interest
	// XXX Sadly, 'contains()' doesn't take a const :(
	if (gSelectMgr.getSelection()->contains(const_cast<LLVOVolume*>(this)))
	{
		interest = F64_MAX * 0.5;
	}

	for (S32 i = 0, end = getNumTEs(); i < end; ++i)
	{
		const viewer_media_t& impl = getMediaImpl(i);
		if (impl.notNull())
		{
			if (interest == -1.0)
			{
				interest = 0.0;
			}
			interest += impl->getInterest();
		}
	}
	return interest;
}

S32 LLVOVolume::getFaceIndexWithMediaImpl(const LLViewerMediaImpl* media_impl,
										  S32 start_face_id)
{
	for (S32 face_id = start_face_id + 1, end = mMediaImplList.size();
		 face_id < end; ++face_id)
	{
		if (mMediaImplList[face_id] == media_impl)
		{
			return face_id;
		}
	}
	return -1;
}

void LLVOVolume::setLightTextureID(const LLUUID& id)
{
	// Same as mLightTexture, but initializes if necessary:
	LLViewerTexture* old_texturep = getLightTexture();

	if (id.notNull())
	{
		if (!hasLightTexture())
		{
			setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT_IMAGE, true,
								   true);
		}
		else if (old_texturep)
		{
			old_texturep->removeVolume(LLRender::LIGHT_TEX, this);
		}
		LLLightImageParams* param_block = getLightImageParams();
		if (param_block && param_block->getLightTexture() != id)
		{
			param_block->setLightTexture(id);
			parameterChanged(LLNetworkData::PARAMS_LIGHT_IMAGE, true);
		}
		// New light texture
		LLViewerTexture* new_texturep = getLightTexture();
		if (new_texturep)
		{
			new_texturep->addVolume(LLRender::LIGHT_TEX, this);
		}
	}
	else if (hasLightTexture())
	{
		if (old_texturep)
		{
			old_texturep->removeVolume(LLRender::LIGHT_TEX, this);
		}
		setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT_IMAGE, false, true);
		parameterChanged(LLNetworkData::PARAMS_LIGHT_IMAGE, true);
		mLightTexture = NULL;
	}
}

void LLVOVolume::setSpotLightParams(const LLVector3& params)
{
	LLLightImageParams* param_block = getLightImageParams();
	if (param_block && param_block->getParams() != params)
	{
		param_block->setParams(params);
		parameterChanged(LLNetworkData::PARAMS_LIGHT_IMAGE, true);
	}
}

void LLVOVolume::setIsLight(bool is_light)
{
	if (is_light != getIsLight())
	{
		if (is_light)
		{
			setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT, true, true);
			// Add it to the pipeline mLightSet
			gPipeline.setLight(mDrawable, true);
		}
		else
		{
			setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT, false, true);
			// Not a light.  Remove it from the pipeline's light set.
			gPipeline.setLight(mDrawable, false);
		}
	}
}

void LLVOVolume::setLightLinearColor(const LLColor3& color)
{
	LLLightParams* param_block = getLightParams();
	if (!param_block || param_block->getLinearColor() == color)
	{
		return;
	}
	param_block->setLinearColor(LLColor4(color,
										 param_block->getLinearColor().mV[3]));
	parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
	gPipeline.markTextured(mDrawable);
	mFaceMappingChanged = true;
}

void LLVOVolume::setLightIntensity(F32 intensity)
{
	LLLightParams* param_block = getLightParams();
	if (!param_block || param_block->getLinearColor().mV[3] == intensity)
	{
		return;
	}
	param_block->setLinearColor(LLColor4(LLColor3(param_block->getLinearColor()),
										 intensity));
	parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
}

void LLVOVolume::setLightRadius(F32 radius)
{
	LLLightParams* param_block = getLightParams();
	if (param_block && param_block->getRadius() != radius)
	{
		param_block->setRadius(radius);
		parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
	}
}

void LLVOVolume::setLightFalloff(F32 falloff)
{
	LLLightParams* param_block = getLightParams();
	if (param_block && param_block->getFalloff() != falloff)
	{
		param_block->setFalloff(falloff);
		parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
	}
}

void LLVOVolume::setLightCutoff(F32 cutoff)
{
	LLLightParams* param_block = getLightParams();
	if (param_block && param_block->getCutoff() != cutoff)
	{
		param_block->setCutoff(cutoff);
		parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
	}
}

bool LLVOVolume::getIsLight() const
{
	return getParameterEntryInUse(LLNetworkData::PARAMS_LIGHT);
}

LLColor3 LLVOVolume::getLightLinearBaseColor() const
{
	const LLLightParams* param_block = getLightParams();
	return param_block ? LLColor3(param_block->getLinearColor())
					   : LLColor3::white;
}

LLColor3 LLVOVolume::getLightLinearColor() const
{
	const LLLightParams* param_block = getLightParams();
	return param_block ? LLColor3(param_block->getLinearColor()) *
								  param_block->getLinearColor().mV[3]
					   : LLColor3::white;
}

LLColor3 LLVOVolume::getLightSRGBColor() const
{
	LLColor3 ret = getLightLinearColor();
	ret = srgbColor3(ret);
	return ret;
}

const LLUUID& LLVOVolume::getLightTextureID() const
{
	const LLLightImageParams* param_block = getLightImageParams();
	return param_block ? param_block->getLightTexture() : LLUUID::null;
}

LLVector3 LLVOVolume::getSpotLightParams() const
{
	const LLLightImageParams* param_block = getLightImageParams();
	return param_block ? param_block->getParams() : LLVector3::zero;
}

void LLVOVolume::updateSpotLightPriority()
{
	LLVector3 pos = mDrawable->getPositionAgent();
	LLVector3 at(0.f, 0.f, -1.f);
	at *= getRenderRotation();

	F32 r = getLightRadius() * 0.5f;

	pos += at * r;

	at = gViewerCamera.getAtAxis();

	pos -= at * r;

	mSpotLightPriority = gPipeline.calcPixelArea(pos, LLVector3(r, r, r),
												 gViewerCamera);

	if (mLightTexture.notNull())
	{
		mLightTexture->addTextureStats(mSpotLightPriority);
		mLightTexture->setBoostLevel(LLGLTexture::BOOST_CLOUDS);
	}
}

bool LLVOVolume::isLightSpotlight() const
{
	const LLLightImageParams* params = getLightImageParams();
	return params && params->isLightSpotlight();
}

LLViewerTexture* LLVOVolume::getLightTexture()
{
	const LLUUID& id = getLightTextureID();
	if (id.isNull())
	{
		mLightTexture = NULL;
	}
	else if (mLightTexture.isNull() || id != mLightTexture->getID())
	{
		mLightTexture =
			LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT, true,
													  LLGLTexture::BOOST_ALM);
	}

	return mLightTexture;
}

F32 LLVOVolume::getLightIntensity() const
{
	const LLLightParams* param_block = getLightParams();
	return param_block ? param_block->getLinearColor().mV[3] : 1.f;
}

F32 LLVOVolume::getLightRadius() const
{
	const LLLightParams* param_block = getLightParams();
	return param_block ? param_block->getRadius() : 0.f;
}

F32 LLVOVolume::getLightFalloff(F32 fudge_factor) const
{
	const LLLightParams* param_block = getLightParams();
	return param_block ? param_block->getFalloff() * fudge_factor : 0.f;
}

F32 LLVOVolume::getLightCutoff() const
{
	const LLLightParams* param_block = getLightParams();
	return param_block ? param_block->getCutoff() : 0.f;
}

U32 LLVOVolume::getVolumeInterfaceID() const
{
	return mVolumeImpl ? mVolumeImpl->getID() : 0;
}

bool LLVOVolume::isFlexible() const
{
	if (!getParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE))
	{
		return false;
	}

	LLVolume* volume = getVolume();
	if (!volume)
	{
		return true;
	}

#if 0	// This is LL's code which does not modify the object volume params but
		// instead fruitelssly creates a new LLVolumeParams, modifying the new
		// instance and getting it deleted immediately... This bogus code is
		// therefore a no-op and this never proved to be an issue, so it simply
		// shows that this whole piece of code is useless !

	if (volume->getParams().getPathParams().getCurveType() !=
			LL_PCODE_PATH_FLEXIBLE)
	{
		LLVolumeParams volume_params = volume->getParams();
		U8 profile_and_hole = volume_params.getProfileParams().getCurveType();
		volume_params.setType(profile_and_hole, LL_PCODE_PATH_FLEXIBLE);
	}
#else	// Just check params data and warn in case of mismatch
	if (volume->getParams().getPathParams().getCurveType() !=
			LL_PCODE_PATH_FLEXIBLE)
	{
		llwarns_once << "Volume for object " << getID()
					 << " got flexible network data with non-flexible pcode in volume params"
					 << llendl;
	}
#endif

	return true;
}

bool LLVOVolume::isSculpted() const
{
	return getParameterEntryInUse(LLNetworkData::PARAMS_SCULPT);
}

bool LLVOVolume::isMesh() const
{
	if (isSculpted())
	{
		const LLSculptParams* params = getSculptParams();
		if (params)
		{
			U8 sculpt_type = params->getSculptType();
			if ((sculpt_type & LL_SCULPT_TYPE_MASK) == LL_SCULPT_TYPE_MESH)
			{
				return true;
			}
		}
	}

	return false;
}

bool LLVOVolume::hasLightTexture() const
{
	return getParameterEntryInUse(LLNetworkData::PARAMS_LIGHT_IMAGE);
}

bool LLVOVolume::isVolumeGlobal() const
{
	return mVolumeImpl ? mVolumeImpl->isVolumeGlobal()
					   : mRiggedVolume.notNull();
}

bool LLVOVolume::canBeFlexible() const
{
	U8 path = getVolume()->getParams().getPathParams().getCurveType();
	return path == LL_PCODE_PATH_FLEXIBLE || path == LL_PCODE_PATH_LINE;
}

bool LLVOVolume::setIsFlexible(bool is_flexible)
{
	bool res = false;
	bool was_flexible = isFlexible();
	LLVolumeParams volume_params;
	if (is_flexible)
	{
		if (!was_flexible)
		{
			volume_params = getVolume()->getParams();
			U8 profile_and_hole = volume_params.getProfileParams().getCurveType();
			volume_params.setType(profile_and_hole, LL_PCODE_PATH_FLEXIBLE);
			res = true;
			setFlags(FLAGS_USE_PHYSICS, false);
			setFlags(FLAGS_PHANTOM, true);
			setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, true, true);
			if (mDrawable)
			{
				mDrawable->makeActive();
			}
		}
	}
	else if (was_flexible)
	{
		volume_params = getVolume()->getParams();
		U8 profile_and_hole = volume_params.getProfileParams().getCurveType();
		volume_params.setType(profile_and_hole, LL_PCODE_PATH_LINE);
		res = true;
		setFlags(FLAGS_PHANTOM, false);
		setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, false, true);
	}
	if (res)
	{
		res = setVolume(volume_params, 1);
		if (res)
		{
			markForUpdate(true);
		}
	}
	return res;
}

const LLMeshSkinInfo* LLVOVolume::getSkinInfo() const
{
	LLVolume* vol = getVolume();
	return vol ? gMeshRepo.getSkinInfo(vol->getParams().getSculptID(), this)
			   : NULL;
}

bool LLVOVolume::isRiggedMesh() const
{
	return isMesh() && getSkinInfo();
}

U32 LLVOVolume::getExtendedMeshFlags() const
{
	const LLExtendedMeshParams* param_block = getExtendedMeshParams();
	return param_block ? param_block->getFlags() : 0;
}

void LLVOVolume::onSetExtendedMeshFlags(U32 flags)
{
	if (mDrawable.notNull())
	{
		// Need to trigger rebuildGeom(), which is where puppet avatars get
		// created/removed.
		getRootEdit()->recursiveMarkForUpdate(true);
	}
	
	if (isAttachment())
	{
		LLVOAvatar* avatarp = getAvatarAncestor();
		if (avatarp)
		{
			updateVisualComplexity();
			avatarp->updateAttachmentOverrides();
		}
	}
}

void LLVOVolume::setExtendedMeshFlags(U32 flags)
{
	U32 curr_flags = getExtendedMeshFlags();
	if (curr_flags != flags)
	{
		setParameterEntryInUse(LLNetworkData::PARAMS_EXTENDED_MESH, true,
							   true);
		LLExtendedMeshParams* param_block = getExtendedMeshParams();
		if (param_block)
		{
			param_block->setFlags(flags);
		}
		parameterChanged(LLNetworkData::PARAMS_EXTENDED_MESH, true);
		onSetExtendedMeshFlags(flags);
	}
}

bool LLVOVolume::canBeAnimatedObject() const
{
	F32 est_tris = recursiveGetEstTrianglesMax();
	F32 max_tris = getAnimatedObjectMaxTris();
	if (est_tris < 0 || est_tris > getAnimatedObjectMaxTris())
	{
		LL_DEBUGS("Axon") << "Estimated tris amount " << est_tris
						 << " out of limit 0-" << max_tris << LL_ENDL;
		return false;
	}

	return true;
}

bool LLVOVolume::isAnimatedObject() const
{
	LLVOVolume* root_vol = (LLVOVolume*)getRootEdit();
	return root_vol &&
		   (root_vol->getExtendedMeshFlags() &
			LLExtendedMeshParams::ANIMATED_MESH_ENABLED_FLAG) != 0;
}

// Called any time parenting changes for a volume. Update flags and puppet
// avatar accordingly. This is called after parent has been changed to
// new_parent.
//virtual
void LLVOVolume::onReparent(LLViewerObject* old_parent,
							LLViewerObject* new_parent)
{
	LLVOVolume* old_volp = old_parent ? old_parent->asVolume() : NULL;

	// AXON: depending on whether animated objects can be attached, we may want
	// to include or remove the isAvatar() check.
	if (new_parent && !new_parent->isAvatar())
	{
		if (mPuppetAvatar.notNull())
		{
			mPuppetAvatar->markForDeath();
			mPuppetAvatar = NULL;
		}
	}
	if (old_volp && old_volp->isAnimatedObject())
	{
		LLVOAvatarPuppet* puppetp = old_volp->getPuppetAvatar();
		if (puppetp)
		{
			// We have been removed from an animated object, need to do cleanup
			puppetp->updateAttachmentOverrides();
			puppetp->updateAnimations();
		}
	}
}

// This needs to be called after onReparent(), because mChildList is not
// updated until the end of addChild()
//virtual
void LLVOVolume::afterReparent()
{
	if (isAnimatedObject())
	{
		LLVOAvatarPuppet* puppetp = getPuppetAvatar();
		if (puppetp)
		{
#if 0
			puppetp->rebuildAttachmentOverrides();
#endif
			puppetp->updateAnimations();
		}
	}
}

//virtual
void LLVOVolume::updateRiggingInfo()
{
	if (!isRiggedMesh() || (mLOD != 3 && mLOD <= mLastRiggingInfoLOD))
	{
		return;
	}

	const LLMeshSkinInfo* skin = getSkinInfo();
	if (!skin) return;

	LLVOAvatar* avatar = getAvatar();
	if (!avatar) return;

	LLVolume* volume = getVolume();
	if (!volume) return;

	// Rigging info may need update
	mJointRiggingInfoTab.clear();
	for (S32 i = 0, count = volume->getNumVolumeFaces(); i < count; ++i)
	{
		LLVolumeFace& vol_face = volume->getVolumeFace(i);
		LLSkinningUtil::updateRiggingInfo(skin, avatar, vol_face);
		if (vol_face.mJointRiggingInfoTab.size())
		{
			mJointRiggingInfoTab.merge(vol_face.mJointRiggingInfoTab);
		}
	}

	mLastRiggingInfoLOD = mLOD;
}

void LLVOVolume::generateSilhouette(LLSelectNode* nodep,
									const LLVector3& view_point)
{
	LLVolume* volume = getVolume();
	if (volume && getRegion())
	{
		LLVector3 view_vector;
		view_vector = view_point;

		// Transform view vector into volume space
		view_vector -= getRenderPosition();
#if 0
		mDrawable->mDistanceWRTCamera = view_vector.length();
#endif
		LLQuaternion worldRot = getRenderRotation();
		view_vector = view_vector * ~worldRot;
		if (!isVolumeGlobal())
		{
			LLVector3 obj_scale = getScale();
			LLVector3 inv_obj_scale(1.f / obj_scale.mV[VX],
									1.f / obj_scale.mV[VY],
									1.f / obj_scale.mV[VZ]);
			view_vector.scaleVec(inv_obj_scale);
		}

		updateRelativeXform();
		LLMatrix4 trans_mat = mRelativeXform;
		if (mDrawable->isStatic())
		{
			trans_mat.translate(getRegion()->getOriginAgent());
		}

		volume->generateSilhouetteVertices(nodep->mSilhouetteVertices,
										   nodep->mSilhouetteNormals,
										   view_vector, trans_mat,
										   mRelativeXformInvTrans,
										   nodep->getTESelectMask());

		nodep->mSilhouetteGenerated = true;
	}
}

void LLVOVolume::deleteFaces()
{
	S32 face_count = mNumFaces;
	if (mDrawable.notNull())
	{
		mDrawable->deleteFaces(0, face_count);
	}

	mNumFaces = 0;
}

void LLVOVolume::updateRadius()
{
	if (mDrawable.notNull())
	{
		mVObjRadius = getScale().length();
		mDrawable->setRadius(mVObjRadius);
	}
}

bool LLVOVolume::isAttachment() const
{
	return mAttachmentState != 0;
}

bool LLVOVolume::isHUDAttachment() const
{
	// *NOTE: we assume hud attachment points are in defined range since this
	// range is constant for backwards compatibility reasons this is probably a
	// reasonable assumption to make.
	S32 attachment_id = ATTACHMENT_ID_FROM_STATE(mAttachmentState);
	return attachment_id >= 31 && attachment_id <= 38;
}

const LLMatrix4& LLVOVolume::getRenderMatrix() const
{
	if (mDrawable->isActive() && !mDrawable->isRoot())
	{
		return mDrawable->getParent()->getWorldMatrix();
	}
	return mDrawable->getWorldMatrix();
}

// Returns a base cost and adds textures to passed in set. Total cost is
// returned value + 5 * size of the resulting set. Cannot include cost of
// textures, as they may be re-used in linked children, and cost should only be
// increased for unique textures  -Nyx
/**************************************************************************
 * The calculation in this method should not be modified by third party
 * viewers, since it is used to limit rendering and should be uniform for
 * everyone. If you have suggested improvements, submit them to the official
 * viewer for consideration.
 *************************************************************************/
U32 LLVOVolume::getRenderCost(texture_cost_t& textures) const
{
	if (!mDrawable) return 0;

	// Per-prim costs, determined experimentally
	constexpr U32 ARC_PARTICLE_COST = 1;
	// Default value
	constexpr U32 ARC_PARTICLE_MAX = 2048;
	// Multiplier for texture resolution - performance tested
	constexpr F32 ARC_TEXTURE_COST_BY_128 = 16.f / 128.f;
	// Cost for light-producing prims
	constexpr U32 ARC_LIGHT_COST = 500;
	// Cost per media-enabled face
	constexpr U32 ARC_MEDIA_FACE_COST = 1500;

	// Per-prim multipliers (tested based on performances)
	constexpr F32 ARC_GLOW_MULT = 1.5f;
	constexpr F32 ARC_BUMP_MULT = 1.25f;
	constexpr F32 ARC_FLEXI_MULT = 5.f;
	constexpr F32 ARC_SHINY_MULT = 1.6f;
	constexpr F32 ARC_INVISI_COST = 1.2f;
	constexpr F32 ARC_WEIGHTED_MESH = 1.2f;
	// Tested to have negligible impact
	constexpr F32 ARC_PLANAR_COST = 1.f;
	constexpr F32 ARC_ANIM_TEX_COST = 4.f;
	// 4x max based on performance
	constexpr F32 ARC_ALPHA_COST = 4.f;

	// Note: this object might not have a volume (e.g. if it is an avatar).
	U32 num_triangles = 0;
	LLVolume* volp = getVolume();
	if (volp)
	{
		LLMeshCostData* costs = getCostData();
		if (costs)
		{
			if (isAnimatedObject() && isRiggedMesh())
			{
				// AXON: scaling here is to make animated object versus non
				// animated object ARC proportional to the corresponding
				// calculations for streaming cost.
				 num_triangles = (U32)(ANIMATED_OBJECT_COST_PER_KTRI * 0.001f *
									   costs->getEstTrisForStreamingCost() /
									   0.06f);
			}
			else
			{
				 F32 radius = getScale().length() * 0.5f;
				 num_triangles = costs->getRadiusWeightedTris(radius);
			}
		}
	}

	if (num_triangles <= 0)
	{
		num_triangles = 4;
	}

	if (volp && isSculpted() && !isMesh())
	{
		const LLSculptParams* sculpt_params = getSculptParams();
		if (sculpt_params)
		{
			const LLUUID& sculpt_id = sculpt_params->getSculptTexture();
			if (!textures.count(sculpt_id))
			{
				LLViewerFetchedTexture* tex =
					LLViewerTextureManager::getFetchedTexture(sculpt_id);
				if (tex)
				{
					S32 cost = 256 +
							  (S32)(ARC_TEXTURE_COST_BY_128 *
									(F32)(tex->getFullHeight() +
										  tex->getFullWidth()));
					textures[sculpt_id] = cost;
				}
			}
		}
	}

	// These are multipliers flags: do not add per-face.
	bool invisi = false;
	bool shiny = false;
	bool glow = false;
	bool alpha = false;
	bool animtex = false;
	bool bump = false;
	bool planar = false;
	// Per media-face shame
	U32 media_faces = 0;
	for (S32 i = 0, count = mDrawable->getNumFaces(); i < count; ++i)
	{
		const LLFace* face = mDrawable->getFace(i);
		if (!face) continue;

		const LLViewerTexture* tex = face->getTexture();
		if (tex)
		{
			const LLUUID& tex_id = tex->getID();
			if (!textures.count(tex_id))
			{
				S32 cost = 0;
				S8 type = tex->getType();
				if (type == LLViewerTexture::FETCHED_TEXTURE ||
					type == LLViewerTexture::LOD_TEXTURE)
				{
					const LLViewerFetchedTexture* ftex =
						(LLViewerFetchedTexture*)tex;
					if (ftex->getFTType() == FTT_LOCAL_FILE &&
						(tex_id == IMG_ALPHA_GRAD_2D ||
						 tex_id == IMG_ALPHA_GRAD))
					{
						// These two textures appear to switch between each
						// other, but are of different sizes (4x256 and
						// 256x256). Hard-code cost from larger one to not
						// cause random complexity changes.
						cost = 320;
					}
				}
				if (cost == 0)
				{
					cost = 256 +
						  (S32)(ARC_TEXTURE_COST_BY_128 *
								(F32)(tex->getFullHeight() +
									  tex->getFullWidth()));
				}
				textures[tex_id] = cost;
			}
		}

		if (face->getPoolType() == LLDrawPool::POOL_ALPHA)
		{
			alpha = true;
		}
		else if (tex && tex->getPrimaryFormat() == GL_ALPHA)
		{
			invisi = true;
		}

		if (face->hasMedia())
		{
			++media_faces;
		}

		animtex |= face->mTextureMatrix != NULL;

		const LLTextureEntry* te = face->getTextureEntry();
		if (te)
		{
			bump |= te->getBumpmap() != 0;
			shiny |= te->getShiny() != 0;
			glow |= te->getGlow() > 0.f;
			planar |= te->getTexGen() != 0;
		}
	}

	// Shame currently has the "base" cost of 1 point per 15 triangles, min 2.
	F32 shame = num_triangles * 5.f;
	shame = shame < 2.f ? 2.f : shame;

	// Multiply by per-face modifiers
	if (planar)
	{
		shame *= ARC_PLANAR_COST;
	}

	if (animtex)
	{
		shame *= ARC_ANIM_TEX_COST;
	}

	if (alpha)
	{
		shame *= ARC_ALPHA_COST;
	}

	if (invisi)
	{
		shame *= ARC_INVISI_COST;
	}

	if (glow)
	{
		shame *= ARC_GLOW_MULT;
	}

	if (bump)
	{
		shame *= ARC_BUMP_MULT;
	}

	if (shiny)
	{
		shame *= ARC_SHINY_MULT;
	}

	if (isRiggedMesh())
	{
		shame *= ARC_WEIGHTED_MESH;
	}

	if (isFlexible())
	{
		shame *= ARC_FLEXI_MULT;
	}

	// Add additional costs
	if (isParticleSource())
	{
		const LLPartSysData* part_sys_data = &(mPartSourcep->mPartSysData);
		const LLPartData* part_data = &(part_sys_data->mPartData);
		U32 num_particles = (U32)(part_sys_data->mBurstPartCount *
								  llceil(part_data->mMaxAge /
										 part_sys_data->mBurstRate));
		num_particles = num_particles > ARC_PARTICLE_MAX ? ARC_PARTICLE_MAX
														 : num_particles;
		F32 part_size = (llmax(part_data->mStartScale[0],
							   part_data->mEndScale[0]) +
						 llmax(part_data->mStartScale[1],
							   part_data->mEndScale[1])) * 0.5f;
		shame += num_particles * part_size * ARC_PARTICLE_COST;
	}

	if (getIsLight())
	{
		shame += ARC_LIGHT_COST;
	}

	if (media_faces)
	{
		shame += media_faces * ARC_MEDIA_FACE_COST;
	}

	// Streaming cost for animated objects includes a fixed cost per linkset.
	// Add a corresponding charge here translated into triangles, but not
	// weighted by any graphics properties.
	if (isAnimatedObject() && isRootEdit())
	{
		shame += ANIMATED_OBJECT_BASE_COST * 5.f / 0.06f;
	}

	return (U32)shame;
}

//virtual
F32 LLVOVolume::getEstTrianglesMax() const
{
	LLVolume* volp = getVolume();
	if (volp && isMesh())
	{
		return gMeshRepo.getEstTrianglesMax(volp->getParams().getSculptID());
	}
	return 0.f;
}

//virtual
F32 LLVOVolume::getEstTrianglesStreamingCost() const
{
	LLVolume* volp = getVolume();
	if (volp && isMesh())
	{
		return gMeshRepo.getEstTrianglesStreamingCost(volp->getParams().getSculptID());
	}
	return 0.f;
}

F32 LLVOVolume::getStreamingCost(S32* bytes, S32* visible_bytes,
								 F32* unscaled_value) const
{
	LLMeshCostData* cost_data = getCostData();
	if (!cost_data)
	{
		return 0.f;
	}

	F32 cost = 0.f;
	bool animated = isAnimatedObject();
	if (animated && isRootEdit())
	{
		// Root object of an animated object has this to account for skeleton
		// overhead.
		cost = ANIMATED_OBJECT_BASE_COST;
	}

	F32 radius = getScale().length() * 0.5f;

	if (isMesh() && animated && isRiggedMesh())
	{
		cost += cost_data->getTriangleBasedStreamingCost();
	}
	else
	{
		cost += cost_data->getRadiusBasedStreamingCost(radius);
	}

	if (bytes)
	{
		*bytes = cost_data->getSizeTotal();
	}

	if (visible_bytes)
	{
		*visible_bytes = cost_data->getSizeByLOD(mLOD);
	}

	if (unscaled_value)
	{
		*unscaled_value = cost_data->getRadiusWeightedTris(radius);
	}

	return cost;
}

LLMeshCostData* LLVOVolume::getCostData() const
{
	if (mCostData.notNull())
	{
		return mCostData.get();
	}

	LLVolume* volume = getVolume();
	if (volume)	// Paranoia
	{
		if (isMesh())
		{
			mCostData =
				gMeshRepo.getCostData(volume->getParams().getSculptID());
		}
		else
		{
			mCostData = new LLMeshCostData();
			S32 counts[4];
			volume->getLoDTriangleCounts(counts);
			if (!mCostData->init(counts[0] * 10, counts[1] * 10,
								 counts[2] * 10, counts[3] * 10))
			{
				mCostData = NULL;
			}
		}
	}

	return mCostData.get();
}

#if 0	// Not yet implemented
//static
void LLVOVolume::updateRenderComplexity()
{
	mRenderComplexity_last = mRenderComplexity_current;
	mRenderComplexity_current = 0;
}
#endif

U32 LLVOVolume::getTriangleCount(S32* vcount) const
{
	U32 count = 0;
	LLVolume* volume = getVolume();
	if (volume)
	{
		count = volume->getNumTriangles(vcount);
	}

	return count;
}

U32 LLVOVolume::getHighLODTriangleCount()
{
	U32 ret = 0;

	LLVolume* volume = getVolume();

	if (!isSculpted())
	{
		LLVolume* ref = gVolumeMgrp->refVolume(volume->getParams(), 3);
		if (ref)
		{
			ret = ref->getNumTriangles();
			gVolumeMgrp->unrefVolume(ref);
		}
	}
	else if (isMesh())
	{
		LLVolume* ref = gVolumeMgrp->refVolume(volume->getParams(), 3);
		if (ref)
		{
			if (!ref->isMeshAssetLoaded() || ref->getNumVolumeFaces() == 0)
			{
				gMeshRepo.loadMesh(this, volume->getParams(),
								   LLModel::LOD_HIGH);
			}
			ret = ref->getNumTriangles();
			gVolumeMgrp->unrefVolume(ref);
		}
	}
	else
	{
		// Default sculpts have a constant number of triangles: 31 rows of 31
		// columns of quads for a 32x32 vertex patch.
		ret = 31 * 2 * 31;
	}

	return ret;
}

//virtual
void LLVOVolume::parameterChanged(U16 param_type, bool local_origin)
{
	LLViewerObject::parameterChanged(param_type, local_origin);
}

//virtual
void LLVOVolume::parameterChanged(U16 param_type, LLNetworkData* data,
								  bool in_use, bool local_origin)
{
	LLViewerObject::parameterChanged(param_type, data, in_use, local_origin);
	if (mVolumeImpl)
	{
		mVolumeImpl->onParameterChanged(param_type, data, in_use,
										local_origin);
	}
	if (!local_origin && param_type == LLNetworkData::PARAMS_EXTENDED_MESH)
	{
		U32 extended_mesh_flags = getExtendedMeshFlags();
		bool enabled = (extended_mesh_flags &
						LLExtendedMeshParams::ANIMATED_MESH_ENABLED_FLAG) != 0;
		// AXON: this is kind of a guess. Better if we could compare the before
		// and after flags directly. What about cases where there is no puppet
		// avatar for optimization reasons ?
		bool was_enabled = getPuppetAvatar() != NULL;
		if (enabled != was_enabled)
		{
			onSetExtendedMeshFlags(extended_mesh_flags);
		}
	}
	if (mDrawable.notNull())
	{
		bool is_light = getIsLight();
		if (is_light != mDrawable->isState(LLDrawable::LIGHT))
		{
			gPipeline.setLight(mDrawable, is_light);
		}
	}
}

void LLVOVolume::setSelected(bool sel)
{
	LLViewerObject::setSelected(sel);
	if (isAnimatedObject())
	{
		getRootEdit()->recursiveMarkForUpdate(true);
	}
	else
	{
		markForUpdate(true);
	}
}

void LLVOVolume::updateSpatialExtents(LLVector4a&, LLVector4a&)
{
}

F32 LLVOVolume::getBinRadius()
{
	static LLCachedControl<S32> object_size_factor(gSavedSettings,
												   "OctreeStaticObjectSizeFactor");
	static LLCachedControl<S32> octree_att_size_factor(gSavedSettings,
													   "OctreeAttachmentSizeFactor");
	static LLCachedControl<LLVector3> octree_dist_factor(gSavedSettings,
														 "OctreeDistanceFactor");
	static LLCachedControl<LLVector3> alpha_dist_factor(gSavedSettings,
														"OctreeAlphaDistanceFactor");

	const LLVector4a* ext = mDrawable->getSpatialExtents();

	bool shrink_wrap = mDrawable->isAnimating();
	bool alpha_wrap = false;
	if (!isHUDAttachment())
	{
		for (S32 i = 0, count = mDrawable->getNumFaces(); i < count; ++i)
		{
			LLFace* face = mDrawable->getFace(i);
			if (face && face->getPoolType() == LLDrawPool::POOL_ALPHA &&
				!face->canRenderAsMask())
			{
				alpha_wrap = true;
				break;
			}
		}
	}
	else
	{
		shrink_wrap = false;
	}

	F32 radius;
	if (alpha_wrap)
	{
		LLVector3 alpha_distance_factor = alpha_dist_factor;
		LLVector3 bounds = getScale();
		radius = llmin(bounds.mV[1], bounds.mV[2]);
		radius = llmin(radius, bounds.mV[0]);
		radius *= 0.5f;
		radius *= 1.f + mDrawable->mDistanceWRTCamera * alpha_distance_factor[1];
		radius += mDrawable->mDistanceWRTCamera * alpha_distance_factor[0];
	}
	else if (shrink_wrap)
	{
		LLVector4a rad;
		rad.setSub(ext[1], ext[0]);
		radius = rad.getLength3().getF32() * 0.5f;
	}
	else if (mDrawable->isStatic())
	{
		LLVector3 distance_factor = octree_dist_factor;
		F32 szf = llmax((S32)object_size_factor, 1);
		radius = llmax(mDrawable->getRadius(), szf);
		radius = powf(radius, 1.f + szf / radius);
		radius *= 1.f + mDrawable->mDistanceWRTCamera * distance_factor[1];
		radius += mDrawable->mDistanceWRTCamera * distance_factor[0];
	}
	else if (mDrawable->getVObj()->isAttachment())
	{
		S32 attachment_size_factor = llmax((S32)octree_att_size_factor, 1);
		radius = llmax((S32) mDrawable->getRadius(), 1) * attachment_size_factor;
	}
	else
	{
		LLVector3 distance_factor = octree_dist_factor;
		radius = mDrawable->getRadius();
		radius *= 1.f + mDrawable->mDistanceWRTCamera * distance_factor[1];
		radius += mDrawable->mDistanceWRTCamera * distance_factor[0];
	}

	return llclamp(radius, 0.5f, 256.f);
}

const LLVector3 LLVOVolume::getPivotPositionAgent() const
{
	return mVolumeImpl ? mVolumeImpl->getPivotPosition()
					   : LLViewerObject::getPivotPositionAgent();
}

void LLVOVolume::onShift(const LLVector4a &shift_vector)
{
	if (mVolumeImpl)
	{
		mVolumeImpl->onShift(shift_vector);
	}
	updateRelativeXform();
}

const LLMatrix4& LLVOVolume::getWorldMatrix(LLXformMatrix* xform) const
{
	return mVolumeImpl ? mVolumeImpl->getWorldMatrix(xform)
					   : xform->getWorldMatrix();
}

LLVector3 LLVOVolume::agentPositionToVolume(const LLVector3& pos) const
{
	LLVector3 ret = pos - getRenderPosition();
	ret = ret * ~getRenderRotation();
	if (!isVolumeGlobal())
	{
		LLVector3 obj_scale = getScale();
		LLVector3 inv_obj_scale(1.f / obj_scale.mV[VX], 1.f / obj_scale.mV[VY],
							  1.f / obj_scale.mV[VZ]);
		ret.scaleVec(inv_obj_scale);
	}
	return ret;
}

LLVector3 LLVOVolume::agentDirectionToVolume(const LLVector3& dir) const
{
	LLVector3 ret = dir * ~getRenderRotation();

	LLVector3 obj_scale = isVolumeGlobal() ? LLVector3(1,1,1) : getScale();
	ret.scaleVec(obj_scale);

	return ret;
}

LLVector3 LLVOVolume::volumePositionToAgent(const LLVector3& dir) const
{
	LLVector3 ret = dir;
	if (!isVolumeGlobal())
	{
		LLVector3 obj_scale = getScale();
		ret.scaleVec(obj_scale);
	}

	ret *= getRenderRotation();
	ret += getRenderPosition();

	return ret;
}

LLVector3 LLVOVolume::volumeDirectionToAgent(const LLVector3& dir) const
{
	LLVector3 ret = dir;
	LLVector3 obj_scale = isVolumeGlobal() ? LLVector3(1.f, 1.f, 1.f)
										   : getScale();
	LLVector3 inv_obj_scale(1.f / obj_scale.mV[VX], 1.f / obj_scale.mV[VY],
							1.f / obj_scale.mV[VZ]);
	ret.scaleVec(inv_obj_scale);
	ret = ret * getRenderRotation();

	return ret;
}


bool LLVOVolume::lineSegmentIntersect(const LLVector4a& start,
									  const LLVector4a& end,
									  S32 face,
									  bool pick_transparent,
									  bool pick_rigged,
									  S32* face_hitp,
									  LLVector4a* intersection,
									  LLVector2* tex_coord,
									  LLVector4a* normal,
									  LLVector4a* tangent)

{
	if (!mCanSelect || mDrawable.isNull() || mDrawable->isDead() ||
#if 0
		(gSelectMgr.mHideSelectedObjects && isSelected()) ||
#endif
		!gPipeline.hasRenderType(mDrawable->getRenderType()))
	{
		return false;
	}

	LLVolume* volume = getVolume();
	if (!volume)
	{
		return false;
	}

	bool transform = true;
	if (mDrawable->isState(LLDrawable::RIGGED))
	{
		if (pick_rigged ||
			(getAvatar() && getAvatar()->isSelf() &&
			 LLFloaterTools::isVisible()))
		{
			updateRiggedVolume(true);
			volume = mRiggedVolume;
			transform = false;
		}
		else
		{
			// Cannot pick rigged attachments on other avatars or when not in
			// build mode
			return false;
		}
	}

	LLVector4a local_start = start;
	LLVector4a local_end = end;
	if (transform)
	{
		LLVector3 v_start(start.getF32ptr());
		LLVector3 v_end(end.getF32ptr());

		v_start = agentPositionToVolume(v_start);
		v_end = agentPositionToVolume(v_end);

		local_start.load3(v_start.mV);
		local_end.load3(v_end.mV);
	}

	LLVector4a p, n, tn;
	LLVector2 tc;
	if (intersection)
	{
		p = *intersection;
	}
	if (tex_coord)
	{
		tc = *tex_coord;
	}
	if (normal)
	{
		n = *normal;
	}
	if (tangent)
	{
		tn = *tangent;
	}

	S32 start_face, end_face;
	if (face == -1)
	{
		start_face = 0;
		end_face = volume->getNumVolumeFaces();
	}
	else
	{
		start_face = face;
		end_face = face + 1;
	}

	pick_transparent |= isHiglightedOrBeacon();

	bool ret = false;
	S32 face_hit = -1;

	bool special_cursor = specialHoverCursor();
	S32 num_faces = mDrawable->getNumFaces();
	for (S32 i = start_face; i < end_face; ++i)
	{
		if (!special_cursor && !pick_transparent && getTE(i) &&
			getTE(i)->getColor().mV[3] == 0.f)
		{
			// Do not attempt to pick completely transparent faces unless
			// pick_transparent is true
			continue;
		}

		face_hit = volume->lineSegmentIntersect(local_start, local_end, i,
												&p, &tc, &n, &tn);
		if (face_hit < 0 || face_hit >= num_faces)
		{
			continue;
		}

		LLFace* face = mDrawable->getFace(face_hit);
		if (!face)
		{
			continue;
		}

		bool ignore_alpha = false;
		const LLTextureEntry* te = face->getTextureEntry();
		if (te)
		{
			LLMaterial* mat = te->getMaterialParams();
			if (mat)
			{
				U8 mode = mat->getDiffuseAlphaMode();
				if (mode == LLMaterial::DIFFUSE_ALPHA_MODE_EMISSIVE ||
					mode == LLMaterial::DIFFUSE_ALPHA_MODE_NONE ||
					(mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK &&
					 mat->getAlphaMaskCutoff() == 0))
				{
					ignore_alpha = true;
				}
			}
		}

		if (ignore_alpha || pick_transparent || !face->getTexture() ||
			!face->getTexture()->hasGLTexture() ||
			face->getTexture()->getMask(face->surfaceToTexture(tc, p, n)))
		{
			local_end = p;
			if (face_hitp)
			{
				*face_hitp = face_hit;
			}

			if (intersection)
			{
				if (transform)
				{
					LLVector3 v_p(p.getF32ptr());
					// Must map back to agent space:
					intersection->load3(volumePositionToAgent(v_p).mV);
				}
				else
				{
					*intersection = p;
				}
			}

			if (normal)
			{
				if (transform)
				{
					LLVector3 v_n(n.getF32ptr());
					normal->load3(volumeDirectionToAgent(v_n).mV);
				}
				else
				{
					*normal = n;
				}
				(*normal).normalize3fast();
			}

			if (tangent)
			{
				if (transform)
				{
					LLVector3 v_tn(tn.getF32ptr());

					LLVector4a trans_tangent;
					trans_tangent.load3(volumeDirectionToAgent(v_tn).mV);

					LLVector4Logical mask;
					mask.clear();
					mask.setElement<3>();

					tangent->setSelectWithMask(mask, tn, trans_tangent);
				}
				else
				{
					*tangent = tn;
				}
					(*tangent).normalize3fast();
			}

			if (tex_coord)
			{
				*tex_coord = tc;
			}

			ret = true;
		}
	}

	return ret;
}

bool LLVOVolume::treatAsRigged()
{
	return isSelected() && mDrawable.notNull() &&
		   mDrawable->isState(LLDrawable::RIGGED) &&
		   (isAttachment() || isAnimatedObject());
}

void LLVOVolume::clearRiggedVolume()
{
	if (mRiggedVolume.notNull())
	{
		mRiggedVolume = NULL;
		updateRelativeXform();
	}
}

// Updates mRiggedVolume to match current animation frame of avatar. Also
// updates position/size in octree.
void LLVOVolume::updateRiggedVolume(bool force_update)
{
	if (isDead())
	{
		return;
	}

	if (!force_update && !treatAsRigged())
	{
		clearRiggedVolume();
		return;
	}

	LLVolume* volume = getVolume();
	if (!volume) return;

	const LLMeshSkinInfo* skin = getSkinInfo();
	if (!skin)
	{
		clearRiggedVolume();
		return;
	}

	LLVOAvatar* avatar = getAvatar();
	if (!avatar || avatar->isDead())
	{
		clearRiggedVolume();
		return;
	}

	if (!mRiggedVolume)
	{
		LLVolumeParams p;
		mRiggedVolume = new LLRiggedVolume(p);
		updateRelativeXform();
	}

	mRiggedVolume->update(skin, avatar, volume);
}

void LLRiggedVolume::update(const LLMeshSkinInfo* skin, LLVOAvatar* avatar,
							const LLVolume* volume)
{
	LL_FAST_TIMER(FTM_UPDATE_RIGGED_VOLUME);

	bool copy = volume->getNumVolumeFaces() != getNumVolumeFaces();
	if (!copy)
	{
		for (S32 i = 0, count = volume->getNumVolumeFaces(); i < count; ++i)
		{
			const LLVolumeFace& src_face = volume->getVolumeFace(i);
			const LLVolumeFace& dst_face = getVolumeFace(i);
			if (src_face.mNumIndices != dst_face.mNumIndices ||
				src_face.mNumVertices != dst_face.mNumVertices)
			{
				copy = true;
				break;
			}
		}
	}
	if (copy)
	{
		copyVolumeFaces(volume);
	}
	else if (!avatar || avatar->isDead() ||
			 avatar->getMotionController().isReallyPaused())
	{
		return;
	}

	// Build matrix palette
	U32 count = 0;
	const LLMatrix4a* mat = avatar->getRiggedMatrix4a(skin, count);

	LLVector4a t, dst, size;
	LLMatrix4a final_mat, bind_shape_matrix;
	bind_shape_matrix.loadu(skin->mBindShapeMatrix);
	for (S32 i = 0, count = volume->getNumVolumeFaces(); i < count; ++i)
	{
		const LLVolumeFace& vol_face = volume->getVolumeFace(i);

		LLVolumeFace& dst_face = mVolumeFaces[i];

		LLVector4a* weight = vol_face.mWeights;
		if (!weight) continue;

		LLSkinningUtil::checkSkinWeights(weight, dst_face.mNumVertices, skin);

		LLVector4a* pos = dst_face.mPositions;
		if (pos && dst_face.mExtents)
		{
			for (S32 j = 0; j < dst_face.mNumVertices; ++j)
			{
				LLSkinningUtil::getPerVertexSkinMatrix(weight[j], mat,
													   final_mat);
				LLVector4a& v = vol_face.mPositions[j];
				bind_shape_matrix.affineTransform(v, t);
				final_mat.affineTransform(t, dst);
				pos[j] = dst;
			}

			// Update bounding box
			LLVector4a& min = dst_face.mExtents[0];
			LLVector4a& max = dst_face.mExtents[1];

			min = pos[0];
			max = pos[1];

			for (S32 j = 1; j < dst_face.mNumVertices; ++j)
			{
				min.setMin(min, pos[j]);
				max.setMax(max, pos[j]);
			}

			dst_face.mCenter->setAdd(dst_face.mExtents[0],
									 dst_face.mExtents[1]);
			dst_face.mCenter->mul(0.5f);

			{
				LL_FAST_TIMER(FTM_RIGGED_OCTREE);
				delete dst_face.mOctree;
				dst_face.mOctree = NULL;

				size.setSub(dst_face.mExtents[1], dst_face.mExtents[0]);
				size.splat(size.getLength3().getF32() * 0.5f);

				dst_face.createOctree(1.f);
			}
		}
	}
}

U32 LLVOVolume::getPartitionType() const
{
	if (isHUDAttachment())
	{
		return LLViewerRegion::PARTITION_HUD;
	}
	if (isAnimatedObject() && getPuppetAvatar())
	{
		return LLViewerRegion::PARTITION_PUPPET;
	}
	if (isAttachment())
	{
		return LLViewerRegion::PARTITION_AVATAR;
	}
	return LLViewerRegion::PARTITION_VOLUME;
}

///////////////////////////////////////////////////////////////////////////////
// LLVolumePartition class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLVolumePartition::LLVolumePartition(LLViewerRegion* regionp)
:	LLSpatialPartition(LLVOVolume::VERTEX_DATA_MASK, true, GL_DYNAMIC_DRAW_ARB,
					   regionp),
	LLVolumeGeometryManager()
{
	mLODPeriod = 32;
	mDepthMask = false;
	mDrawableType = LLPipeline::RENDER_TYPE_VOLUME;
	mPartitionType = LLViewerRegion::PARTITION_VOLUME;
	mSlopRatio = 0.25f;
	mBufferUsage = GL_DYNAMIC_DRAW_ARB;
}

///////////////////////////////////////////////////////////////////////////////
// LLVolumeBridge class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLVolumeBridge::LLVolumeBridge(LLDrawable* drawablep, LLViewerRegion* regionp)
:	LLSpatialBridge(drawablep, true, LLVOVolume::VERTEX_DATA_MASK, regionp),
	LLVolumeGeometryManager()
{
	mDepthMask = false;
	mLODPeriod = 32;
	mDrawableType = LLPipeline::RENDER_TYPE_VOLUME;
	mPartitionType = LLViewerRegion::PARTITION_BRIDGE;

	mBufferUsage = GL_DYNAMIC_DRAW_ARB;

	mSlopRatio = 0.25f;
}

///////////////////////////////////////////////////////////////////////////////
// LLAvatarBridge class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLAvatarBridge::LLAvatarBridge(LLDrawable* drawablep,
							   LLViewerRegion* regionp)
:	LLVolumeBridge(drawablep, regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_AVATAR;
	mPartitionType = LLViewerRegion::PARTITION_AVATAR;
}

///////////////////////////////////////////////////////////////////////////////
// LLPuppetBridge class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLPuppetBridge::LLPuppetBridge(LLDrawable* drawablep,
							   LLViewerRegion* regionp)
:	LLVolumeBridge(drawablep, regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_PUPPET;
	mPartitionType = LLViewerRegion::PARTITION_PUPPET;
}

///////////////////////////////////////////////////////////////////////////////
// LLVolumeGeometryManager class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

// Helper function
static bool can_batch_texture(LLFace* facep)
{
	const LLTextureEntry* te = facep->getTextureEntry();
	if (!te || te->getBumpmap() || te->getMaterialParams().notNull())
	{
		// Bump maps and materials do not work with texture batching yet
		return false;
	}

	LLViewerTexture* tex = facep->getTexture();
	if (tex && tex->getPrimaryFormat() == GL_ALPHA)
	{
		// Cannot batch invisiprims
		return false;
	}

	if (facep->isState(LLFace::TEXTURE_ANIM) &&
		facep->getVirtualSize() >= MIN_TEX_ANIM_SIZE)
	{
		// Texture animation breaks batches
		return false;
	}

	return true;
}

LLVolumeGeometryManager::LLVolumeGeometryManager()
:	LLGeometryManager()
{
	if (sInstanceCount == 0)
	{
		allocateFaces(MAX_FACE_COUNT);
	}

	++sInstanceCount;
}

LLVolumeGeometryManager::~LLVolumeGeometryManager()
{
	llassert(sInstanceCount > 0);
	--sInstanceCount;

	if (sInstanceCount <= 0)
	{
		freeFaces();
		sInstanceCount = 0;
	}
}

void LLVolumeGeometryManager::allocateFaces(U32 max_face_count)
{
	size_t bytes = max_face_count * sizeof(LLFace*);
	sFullbrightFaces = (LLFace**)ll_aligned_malloc(bytes, 64);
	sBumpFaces = (LLFace**)ll_aligned_malloc(bytes, 64);
	sSimpleFaces = (LLFace**)ll_aligned_malloc(bytes, 64);
	sNormFaces = (LLFace**)ll_aligned_malloc(bytes, 64);
	sSpecFaces = (LLFace**)ll_aligned_malloc(bytes, 64);
	sNormSpecFaces = (LLFace**)ll_aligned_malloc(bytes, 64);
	sAlphaFaces = (LLFace**)ll_aligned_malloc(bytes, 64);
}

void LLVolumeGeometryManager::freeFaces()
{
	ll_aligned_free(sFullbrightFaces);
	ll_aligned_free(sBumpFaces);
	ll_aligned_free(sSimpleFaces);
	ll_aligned_free(sNormFaces);
	ll_aligned_free(sSpecFaces);
	ll_aligned_free(sNormSpecFaces);
	ll_aligned_free(sAlphaFaces);

	sFullbrightFaces = NULL;
	sBumpFaces = NULL;
	sSimpleFaces = NULL;
	sNormFaces = NULL;
	sSpecFaces = NULL;
	sNormSpecFaces = NULL;
	sAlphaFaces = NULL;
}

void LLVolumeGeometryManager::registerFace(LLSpatialGroup* group,
										   LLFace* facep, U32 type)
{
	LL_FAST_TIMER(FTM_REGISTER_FACE);

	if (!facep || !group) return;	// Paranoia

	if (facep->getViewerObject()->isSelected() &&
//MK
		(!gRLenabled || !gRLInterface.mContainsEdit) &&
//mk
		gSelectMgr.mHideSelectedObjects)
	{
		return;
	}

	const LLTextureEntry* tep = facep->getTextureEntry();
	if (!tep)
	{
		llwarns_once << "NULL texture entry pointer. Aborting." << llendl;
		return;
	}

#if 0	// Pretty common and harmless
	if (type == LLRenderPass::PASS_ALPHA &&
		tep->getMaterialParams().notNull() &&
		!facep->getVertexBuffer()->hasDataType(LLVertexBuffer::TYPE_TANGENT))
	{
		llwarns_once << "No tangent for this alpha blended face !" << llendl;
	}
#endif

	// Add face to drawmap
	LLSpatialGroup::drawmap_elem_t& draw_vec = group->mDrawMap[type];

	bool fullbright = type == LLRenderPass::PASS_FULLBRIGHT ||
					  type == LLRenderPass::PASS_INVISIBLE ||
					  type == LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK ||
					  (type == LLRenderPass::PASS_ALPHA &&
					   facep->isState(LLFace::FULLBRIGHT)) ||
					  tep->getFullbright();

	if (!fullbright && type != LLRenderPass::PASS_GLOW &&
		!facep->getVertexBuffer()->hasDataType(LLVertexBuffer::TYPE_NORMAL))
	{
		llwarns_once << "Non fullbright face has no normals !" << llendl;
		return;
	}

	const LLMatrix4* tex_mat = NULL;
	if (facep->isState(LLFace::TEXTURE_ANIM) &&
		facep->getVirtualSize() >= MIN_TEX_ANIM_SIZE)
	{
		tex_mat = facep->mTextureMatrix;
	}

	LLDrawable* drawable = facep->getDrawable();
	if (!drawable) return;	// Paranoia

	const LLMatrix4* model_mat = NULL;
	if (drawable->isState(LLDrawable::ANIMATED_CHILD))
	{
		model_mat = &drawable->getWorldMatrix();
	}
	else if (drawable->isActive())
	{
		model_mat = &drawable->getRenderMatrix();
	}
	else if (drawable->getRegion())
	{
		model_mat = &(drawable->getRegion()->mRenderMatrix);
	}
#if 1
	if (model_mat && model_mat->isIdentity())
	{
		model_mat = NULL;
	}
#endif

	U8 bump = type == LLRenderPass::PASS_BUMP ||
			  type == LLRenderPass::PASS_POST_BUMP ? tep->getBumpmap() : 0;

	U8 shiny = tep->getShiny();

	LLViewerTexture* tex = facep->getTexture();
//MK
	// If @camtexture is set, do not show any texture in world (but show
	// attachments normally)
	if (gRLenabled && gRLInterface.mContainsCamTextures &&
		gRLInterface.mCamTexturesCustom &&
		!facep->getViewerObject()->isAttachment())
	{
		tex = gRLInterface.mCamTexturesCustom;
	}
//mk

	U8 index = facep->getTextureIndex();

	LLMaterialID mat_id = tep->getMaterialID();
	LLMaterial* mat = tep->getMaterialParams().get();

	U32 shader_mask = 0xFFFFFFFF; // no shader
	if (mat)
	{
		if (type == LLRenderPass::PASS_ALPHA)
		{
			shader_mask =
				mat->getShaderMask(LLMaterial::DIFFUSE_ALPHA_MODE_BLEND);
		}
		else
		{
			shader_mask = mat->getShaderMask();
		}
	}

	S32 idx = draw_vec.size() - 1;
	LLDrawInfo* dinfo_idx = idx >= 0 ? draw_vec[idx].get() : NULL;
	bool batchable = false;
	if (dinfo_idx && index < FACE_DO_NOT_BATCH_TEXTURES &&
		can_batch_texture(facep))
	{
		if (index < dinfo_idx->mTextureList.size())
		{
			if (dinfo_idx->mTextureList[index].isNull())
			{
				batchable = true;
				dinfo_idx->mTextureList[index] = tex;
			}
			else if (dinfo_idx->mTextureList[index] == tex)
			{
				// This face's texture index can be used with this batch
				batchable = true;
			}
		}
		else
		{
			// Texture list can be expanded to fit this texture index
			batchable = true;
		}
	}

	if (dinfo_idx &&
		dinfo_idx->mVertexBuffer == facep->getVertexBuffer() &&
		dinfo_idx->mEnd == facep->getGeomIndex() - 1 &&
		(batchable || LLPipeline::sTextureBindTest ||
		 dinfo_idx->mTexture == tex) &&
#if LL_DARWIN
		(S32)(dinfo_idx->mEnd - dinfo_idx->mStart +
			facep->getGeomCount()) <= gGLManager.mGLMaxVertexRange &&
		(S32)(dinfo_idx->mCount +
			facep->getIndicesCount()) <= gGLManager.mGLMaxIndexRange &&
#endif
		dinfo_idx->mMaterial == mat &&
		dinfo_idx->mMaterialID == mat_id &&
		dinfo_idx->mFullbright == fullbright &&
		dinfo_idx->mBump == bump &&
		// Need to break batches when a material is shared, but legacy shiny is
		// different
		(!mat || dinfo_idx->mShiny == shiny) &&
		dinfo_idx->mTextureMatrix == tex_mat &&
		dinfo_idx->mModelMatrix == model_mat &&
		dinfo_idx->mShaderMask == shader_mask)
	{
		dinfo_idx->mCount += facep->getIndicesCount();
		dinfo_idx->mEnd += facep->getGeomCount();
		dinfo_idx->mVSize = llmax(dinfo_idx->mVSize, facep->getVirtualSize());

		if (index < FACE_DO_NOT_BATCH_TEXTURES &&
			index >= dinfo_idx->mTextureList.size())
		{
			dinfo_idx->mTextureList.resize(index + 1);
			dinfo_idx->mTextureList[index] = tex;
		}
		dinfo_idx->validate();
		update_min_max(dinfo_idx->mExtents[0], dinfo_idx->mExtents[1],
					   facep->mExtents[0]);
		update_min_max(dinfo_idx->mExtents[0], dinfo_idx->mExtents[1],
					   facep->mExtents[1]);
	}
	else
	{
		U32 start = facep->getGeomIndex();
		U32 end = start + facep->getGeomCount() - 1;
		U32 offset = facep->getIndicesStart();
		U32 count = facep->getIndicesCount();
		LLPointer<LLDrawInfo> draw_info =
			new LLDrawInfo(start, end, count, offset, tex,
						   facep->getVertexBuffer(), fullbright, bump);
		draw_vec.push_back(draw_info);
		draw_info->mGroup = group;
		draw_info->mVSize = facep->getVirtualSize();
		draw_info->mTextureMatrix = tex_mat;
		draw_info->mModelMatrix = model_mat;
		draw_info->mBump = bump;
		draw_info->mShiny = shiny;

		static const F32 alpha[4] = { 0.f, 0.25f, 0.5f, 0.75f };
		F32 spec = alpha[shiny & TEM_SHINY_MASK];
		draw_info->mSpecColor.set(spec, spec, spec, spec);
		draw_info->mEnvIntensity = spec;
		draw_info->mSpecularMap = NULL;
		draw_info->mMaterial = mat;
		draw_info->mShaderMask = shader_mask;

		if (mat)
		{
			S32 te_offset = facep->getTEOffset();
			// We have a material. Update our draw info accordingly.
			draw_info->mMaterialID = mat_id;
			if (mat->getSpecularID().notNull())
			{
				const LLColor4U& spec_col = mat->getSpecularLightColor();
				F32 alpha = (F32)mat->getSpecularLightExponent() * ONE255TH;
				draw_info->mSpecColor.set(spec_col.mV[0] * ONE255TH,
										  spec_col.mV[1] * ONE255TH,
										  spec_col.mV[2] * ONE255TH, alpha);

				draw_info->mEnvIntensity = mat->getEnvironmentIntensity() *
										   ONE255TH;
				draw_info->mSpecularMap =
					facep->getViewerObject()->getTESpecularMap(te_offset);
			}
			draw_info->mAlphaMaskCutoff = mat->getAlphaMaskCutoff() * ONE255TH;
			draw_info->mDiffuseAlphaMode = mat->getDiffuseAlphaMode();
			draw_info->mNormalMap =
				facep->getViewerObject()->getTENormalMap(te_offset);
		}
		else if (type == LLRenderPass::PASS_GRASS)
		{
			draw_info->mAlphaMaskCutoff = 0.5f;
		}
		else
		{
			draw_info->mAlphaMaskCutoff = 0.33f;
		}

		if (type == LLRenderPass::PASS_ALPHA)
		{
			facep->setDrawInfo(draw_info);	// For alpha sorting
		}
		draw_info->mExtents[0] = facep->mExtents[0];
		draw_info->mExtents[1] = facep->mExtents[1];

		if (index < FACE_DO_NOT_BATCH_TEXTURES)
		{
			// Initialize texture list for texture batching
			draw_info->mTextureList.resize(index + 1);
			draw_info->mTextureList[index] = tex;
		}
		draw_info->validate();
	}
}

static LLDrawPoolAvatar* get_avatar_drawpool(LLViewerObject* vobj)
{
	LLVOAvatar* avatar = vobj->getAvatar();
	if (avatar)
	{
		LLDrawable* drawable = avatar->mDrawable;
		if (drawable && drawable->getNumFaces() > 0)
		{
			LLFace* face = drawable->getFace(0);
			if (face)
			{
				LLDrawPool* drawpool = face->getPool();
				if (drawpool)
				{
					U32 type = drawpool->getType();
					if (type == LLDrawPool::POOL_AVATAR ||
						type == LLDrawPool::POOL_PUPPET)
					{
						return (LLDrawPoolAvatar*)drawpool;
					}
				}
			}
		}
	}

	return NULL;
}

void LLVolumeGeometryManager::rebuildGeom(LLSpatialGroup* group)
{
	LL_FAST_TIMER(FTM_REBUILD_VBO);

	if (group->changeLOD())
	{
		group->mLastUpdateDistance = group->mDistance;
	}

	group->mLastUpdateViewAngle = group->mViewAngle;

	if (!group->hasState(LLSpatialGroup::GEOM_DIRTY |
						 LLSpatialGroup::ALPHA_DIRTY))
	{
		if (!LLPipeline::sDelayVBUpdate &&
			group->hasState(LLSpatialGroup::MESH_DIRTY))
		{
			rebuildMesh(group);
		}
		return;
	}

	group->mBuilt = 1.f;

	LLVOAvatar* voavp = NULL;
	LLVOAvatar* voattachavp = NULL;
	LLSpatialPartition* partition = group->getSpatialPartition();
	if (!partition) return;	// Paranoia
	LLSpatialBridge* bridge = partition->asBridge();
	LLVOVolume* vovol = NULL;
	if (bridge)
	{
		LLViewerObject* vobj =
			bridge->mDrawable ? bridge->mDrawable->getVObj().get() : NULL;
		if (vobj)
		{
			if (bridge->mAvatar.isNull())
			{
				bridge->mAvatar = vobj->getAvatar();
			}
			voattachavp = vobj->getAvatarAncestor();
			vovol = vobj->asVolume();
		}
		voavp = bridge->mAvatar;
	}
	if (voattachavp)
	{
		voattachavp->subtractAttachmentBytes(group->mGeometryBytes);
		voattachavp->subtractAttachmentArea(group->mSurfaceArea);
	}
	if (voavp && voavp != voattachavp)
	{
		voavp->subtractAttachmentBytes(group->mGeometryBytes);
		voavp->subtractAttachmentArea(group->mSurfaceArea);
	}
	if (vovol)
	{
		vovol->updateVisualComplexity();
	}

	group->mGeometryBytes = 0;
	group->mSurfaceArea = 0.f;

	// Cache object box size since it might be used for determining visibility
	const LLVector4a* bounds = group->getObjectBounds();
	group->mObjectBoxSize = llmax(bounds[1].getLength3().getF32(), 10.f);

	group->clearDrawMap();

	U32 fullbright_count = 0;
	U32 bump_count = 0;
	U32 simple_count = 0;
	U32 alpha_count = 0;
	U32 norm_count = 0;
	U32 spec_count = 0;
	U32 normspec_count = 0;

	U32 usage = partition->mBufferUsage;

	U32 vertex_size =
		(U32)LLVertexBuffer::calcVertexSize(partition->mVertexDataMask);
	U32 max_vertices = LLVOVolume::sRenderMaxVBOSize * 1024 / vertex_size;
	static LLCachedControl<U32> render_max_node_size(gSavedSettings,
													 "RenderMaxNodeSize");
	U32 max_total = render_max_node_size * 1024U / vertex_size;
	max_vertices = llmin(max_vertices, 65535U);

	U32 cur_total = 0;

	bool emissive = false;

	bool can_use_vertex_shaders = gPipeline.canUseVertexShaders();
	bool not_debugging_alpha = !LLDrawPoolAlpha::sShowDebugAlpha;

	static LLCachedControl<F32> mesh_boost(gSavedSettings,
										   "MeshLODBoostFactor");
	bool all_rigged_faces = mesh_boost > 1.f;

	{
		LL_FAST_TIMER(FTM_REBUILD_VOLUME_FACE_LIST);

		// Get all the faces into a list
		for (LLSpatialGroup::element_iter it = group->getDataBegin();
			 it != group->getDataEnd(); ++it)
		{
			LLDrawable* drawablep = (LLDrawable*)(*it)->getDrawable();
			if (!drawablep || drawablep->isDead() ||
				drawablep->isState(LLDrawable::FORCE_INVISIBLE))
			{
				continue;
			}

			if (drawablep->isAnimating())
			{
				// Fall back to stream draw for animating verts
				usage = GL_STREAM_DRAW_ARB;
			}

			LLVOVolume* vobj = drawablep->getVOVolume();
			if (!vobj)
			{
				continue;
			}

			LLVolume* volume = vobj->getVolume();
			if (!volume)
			{
				continue;
			}

			if (vobj->isMesh())
			{
				if (!gMeshRepo.meshRezEnabled() ||
					!volume->isMeshAssetLoaded())
				{
					continue;
				}
			}

			vobj->updatePuppetAvatar();

			const LLVector3& scale = vobj->getScale();
			group->mSurfaceArea += volume->getSurfaceArea() *
								   llmax(llmax(scale.mV[0], scale.mV[1]),
										 scale.mV[2]);

			{
				LL_FAST_TIMER(FTM_VOLUME_TEXTURES);
				vobj->updateTextureVirtualSize(true);
			}
			vobj->preRebuild();

			drawablep->clearState(LLDrawable::HAS_ALPHA);

			voavp = vobj->getPuppetAvatar();
			bool animated = vobj->isAnimatedObject();
			bool rigged = vobj->isRiggedMesh();
			LLVOAvatar* vobj_av = vobj->getAvatar();
			if (rigged && vobj_av && (!animated || (animated && voavp)))
			{
				vobj_av->addAttachmentOverridesForObject(vobj, NULL, false);
			}

			// New rigged flag for the for loop below
					  // Standard rigged attachments...
			rigged &= (!animated && vobj->isAttachment()) ||
					  // ...or animated mesh object.
					 (animated && voavp &&
					  ((LLVOAvatarPuppet*)voavp)->mPlaying);

			bool bake_sunlight =
				LLPipeline::sBakeSunlight && drawablep->isStatic();

			bool is_rigged = false;

			// For each face
			for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
			{
				LLFace* facep = drawablep->getFace(i);
				if (!facep)
				{
					continue;
				}

				// ALWAYS null out vertex buffer on rebuild: if the face lands
				// in a render batch, it will recover its vertex buffer
				// reference from the spatial group
				facep->setVertexBuffer(NULL);

				// Sum up face verts and indices
				drawablep->updateFaceSize(i);

				if (rigged)
				{
					if (!facep->isState(LLFace::RIGGED))
					{
						// Completely reset vertex buffer
						facep->clearVertexBuffer();
					}

					facep->setState(LLFace::RIGGED);
					is_rigged = true;

					// Get drawpool of avatar with rigged face
					LLDrawPoolAvatar* pool = get_avatar_drawpool(vobj);

					const LLTextureEntry* te = facep->getTextureEntry();
					if (pool && te)
					{
						// Remove face from old pool if it exists
						LLDrawPool* old_pool = facep->getPool();
						U32 old_type = old_pool ? old_pool->getType() : 0;
						if (old_type == LLDrawPool::POOL_AVATAR ||
							old_type == LLDrawPool::POOL_PUPPET)
						{
							((LLDrawPoolAvatar*)old_pool)->removeRiggedFace(facep);
						}

						F32 alpha_channel = te->getColor().mV[3];
						if (not_debugging_alpha && alpha_channel < 0.001f)
						{
							// Do not render invisible rigged faces. HB
							continue;
						}

						// Add face to new pool

						if (te->getGlow())
						{
							if (!pool->addRiggedFace(facep,
													 LLDrawPoolAvatar::RIGGED_GLOW))
							{
								continue;
							}
						}

						LLViewerTexture* tex = facep->getTexture();
						U32 type = gPipeline.getPoolTypeFromTE(te, tex);

						U8 alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
						bool fullbright = te->getFullbright();
						LLMaterial* mat = te->getMaterialParams().get();
						if (mat)
						{
							alpha_mode = mat->getDiffuseAlphaMode();
						}
						if (mat && LLPipeline::sRenderDeferred)
						{
							bool is_alpha = type == LLDrawPool::POOL_ALPHA &&
											(alpha_mode == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND ||
											 alpha_channel < 0.999f);

							if (is_alpha)
							{
								// This face needs alpha blending, override
								// alpha mode
								alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
							}

							// Only add the face if it will actually be visible
							if (fullbright &&
								alpha_mode == LLMaterial::DIFFUSE_ALPHA_MODE_NONE)
							{
								pool->addRiggedFace(facep,
													LLDrawPoolAvatar::RIGGED_FULLBRIGHT);
							}
							else if (!is_alpha || alpha_channel > 0.f)
							{
								U32 mask = mat->getShaderMask(alpha_mode);
								if (!pool->addRiggedFace(facep, mask))
								{
									continue;
								}
							}
							if (animated && rigged && vobj_av)
							{
								pool->updateRiggedVertexBuffers(vobj_av);
							}
						}
						else if (mat)
						{
							bool is_alpha = type == LLDrawPool::POOL_ALPHA;
							bool can_be_shiny =
								alpha_mode == LLMaterial::DIFFUSE_ALPHA_MODE_NONE ||
								alpha_mode == LLMaterial::DIFFUSE_ALPHA_MODE_EMISSIVE;
							U32 mask = LLDrawPoolAvatar::RIGGED_UNKNOWN;
							if (alpha_channel >= 0.999f &&
								alpha_mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
							{
								mask = fullbright ? LLDrawPoolAvatar::RIGGED_FULLBRIGHT
												  : LLDrawPoolAvatar::RIGGED_SIMPLE;
							}
							else if (is_alpha || alpha_channel < 0.999f)
							{
								if (alpha_channel > 0.f)
								{
									mask = fullbright ? LLDrawPoolAvatar::RIGGED_FULLBRIGHT_ALPHA
													  : LLDrawPoolAvatar::RIGGED_ALPHA;
								}
							}
							else if (can_use_vertex_shaders && can_be_shiny &&
									 LLPipeline::sRenderBump && te->getShiny())
							{
								mask = fullbright ? LLDrawPoolAvatar::RIGGED_FULLBRIGHT_SHINY
												  : LLDrawPoolAvatar::RIGGED_SHINY;
							}
							else
							{
								mask = fullbright ? LLDrawPoolAvatar::RIGGED_FULLBRIGHT
												  : LLDrawPoolAvatar::RIGGED_SIMPLE;
							}
							if (mask != LLDrawPoolAvatar::RIGGED_UNKNOWN)
							{
								if (!pool->addRiggedFace(facep, mask))
								{
									continue;
								}
							}
						}
						else
						{
							U32 mask = LLDrawPoolAvatar::RIGGED_UNKNOWN;
							bool fullbright = te->getFullbright();
							if (type == LLDrawPool::POOL_ALPHA)
							{
								if (alpha_channel > 0.f)
								{
									mask = fullbright ? LLDrawPoolAvatar::RIGGED_FULLBRIGHT_ALPHA
													  : LLDrawPoolAvatar::RIGGED_ALPHA;
								}
							}
							else if (te->getShiny())
							{
								mask = fullbright ? LLDrawPoolAvatar::RIGGED_FULLBRIGHT_SHINY
												  : (LLPipeline::sRenderDeferred ? LLDrawPoolAvatar::RIGGED_SIMPLE
																				 : LLDrawPoolAvatar::RIGGED_SHINY);
							}
							else if (fullbright)
							{
								mask = LLDrawPoolAvatar::RIGGED_FULLBRIGHT;
							}
							else
							{
								mask = LLDrawPoolAvatar::RIGGED_SIMPLE;
							}
							if (mask != LLDrawPoolAvatar::RIGGED_UNKNOWN)
							{
								if (!pool->addRiggedFace(facep, mask))
								{
									continue;
								}
							}

							if (!fullbright && type != LLDrawPool::POOL_ALPHA &&
								LLPipeline::sRenderDeferred)
							{
								mask = te->getBumpmap() ? LLDrawPoolAvatar::RIGGED_DEFERRED_BUMP
														: LLDrawPoolAvatar::RIGGED_DEFERRED_SIMPLE;
								// No test for failure done here, since we
								// 'continue;' just after anyway.
								pool->addRiggedFace(facep, mask);
							}
						}
					}

					continue;
				}
				else if (facep->isState(LLFace::RIGGED))
				{
					// Face is not rigged but used to be, remove from rigged
					// face pool
					LLDrawPool* pool = facep->getPool();
					U32 type = pool ? pool->getType() : 0;
					if (type == LLDrawPool::POOL_AVATAR ||
						type == LLDrawPool::POOL_PUPPET)
					{
						((LLDrawPoolAvatar*)pool)->removeRiggedFace(facep);
					}
					facep->clearState(LLFace::RIGGED);
				}

				if (facep->getIndicesCount() <= 0 ||
					facep->getGeomCount() <= 0)
				{
					facep->clearVertexBuffer();
					continue;
				}

				if (cur_total > max_total)
				{
					facep->clearVertexBuffer();
					continue;
				}

				if (facep->hasGeometry() &&
					// *FIXME: getPixelArea() is sometimes incorrect for rigged
					// meshes (thus the test for 'rigged', which is a hack).
					((rigged && all_rigged_faces) ||
					 facep->getPixelArea() > FORCE_CULL_AREA))
				{
					cur_total += facep->getGeomCount();

					LLViewerTexture* tex = facep->getTexture();
					const LLTextureEntry* te = facep->getTextureEntry();
					if (!te) continue;

					if (te->getGlow() >= ONE255TH)
					{
						emissive = true;
					}

					if (facep->isState(LLFace::TEXTURE_ANIM) &&
						!vobj->mTexAnimMode)
					{
						facep->clearState(LLFace::TEXTURE_ANIM);
					}

					bool force_simple =
						facep->getPixelArea() < FORCE_SIMPLE_RENDER_AREA;
					U32 type = gPipeline.getPoolTypeFromTE(te, tex);
					if (type != LLDrawPool::POOL_ALPHA && force_simple)
					{
						type = LLDrawPool::POOL_SIMPLE;
					}
					facep->setPoolType(type);

					if (vobj->isHUDAttachment())
					{
						facep->setState(LLFace::FULLBRIGHT);
					}

					if (vobj->mTextureAnimp && vobj->mTexAnimMode)
					{
						if (vobj->mTextureAnimp->mFace <= -1)
						{
							for (S32 face = 0, count2 = vobj->getNumTEs();
								 face < count2; ++face)
							{
								LLFace* facep2 = drawablep->getFace(face);
								if (facep2)
								{
									facep2->setState(LLFace::TEXTURE_ANIM);
								}
							}
						}
						else if (vobj->mTextureAnimp->mFace < vobj->getNumTEs())
						{
							LLFace* facep2 =
								drawablep->getFace(vobj->mTextureAnimp->mFace);
							if (facep2)
							{
								facep2->setState(LLFace::TEXTURE_ANIM);
							}
						}
					}

					if (type == LLDrawPool::POOL_ALPHA)
					{
						bool add_face = alpha_count < MAX_FACE_COUNT;
						if (facep->canRenderAsMask())
						{
							// Can be treated as alpha mask
							if (simple_count < MAX_FACE_COUNT)
							{
								sSimpleFaces[simple_count++] = facep;
							}
						}
						else if (te->getColor().mV[3] > 0.f ||
								 te->getGlow() > 0.f)
						{
							// Only treat as alpha in the pipeline if not fully
							// transparent
							drawablep->setState(LLDrawable::HAS_ALPHA);
						}
						else if (not_debugging_alpha)
						{
							// Do not add invisible faces when not in show
							// alpha mode.
							add_face = false;
						}
						if (add_face)
						{
							sAlphaFaces[alpha_count++] = facep;
						}
					}
					else
					{
						if (drawablep->isState(LLDrawable::REBUILD_VOLUME))
						{
							facep->mLastUpdateTime = gFrameTimeSeconds;
						}

						if (LLPipeline::sRenderBump &&
							gPipeline.canUseWindLightShadersOnObjects())
						{
							if (LLPipeline::sRenderDeferred &&
								te->getMaterialID().notNull() &&
								te->getMaterialParams().notNull())
							{
								LLMaterial* mat = te->getMaterialParams().get();
								if (mat && mat->getNormalID().notNull())
								{
									if (mat->getSpecularID().notNull())
									{
										// Has normal and specular maps (needs
										// texcoord1, texcoord2, and tangent)
										if (normspec_count < MAX_FACE_COUNT)
										{
											sNormSpecFaces[normspec_count++] = facep;
										}
									}
									// Has normal map: needs texcoord1 and
									// tangent
									else if (norm_count < MAX_FACE_COUNT)
									{
										sNormFaces[norm_count++] = facep;
									}
								}
								else if (mat && mat->getSpecularID().notNull())
								{
									// Has specular map but no normal map,
									// needs texcoord2
									if (spec_count < MAX_FACE_COUNT)
									{
										sSpecFaces[spec_count++] = facep;
									}
								}
								// Has neither specular map nor normal map,
								// only needs texcoord0
								else if (simple_count < MAX_FACE_COUNT)
								{
									sSimpleFaces[simple_count++] = facep;
								}
							}
							else if (te->getBumpmap())
							{
								// Needs normal + tangent
								if (bump_count < MAX_FACE_COUNT)
								{
									sBumpFaces[bump_count++] = facep;
								}
							}
							else if (te->getShiny() || !te->getFullbright())
							{
								// Needs normal
								if (simple_count < MAX_FACE_COUNT)
								{
									sSimpleFaces[simple_count++] = facep;
								}
							}
							else
							{
								// Does not need normal
								facep->setState(LLFace::FULLBRIGHT);
								if (fullbright_count < MAX_FACE_COUNT)
								{
									sFullbrightFaces[fullbright_count++] = facep;
								}
							}
						}
						else if (te->getBumpmap() && LLPipeline::sRenderBump)
						{
							// Needs normal + tangent
							if (bump_count < MAX_FACE_COUNT)
							{
								sBumpFaces[bump_count++] = facep;
							}
						}
						else if ((te->getShiny() && LLPipeline::sRenderBump) ||
								 !(te->getFullbright() || bake_sunlight))
						{
							// Needs normal
							if (simple_count < MAX_FACE_COUNT)
							{
								sSimpleFaces[simple_count++] = facep;
							}
						}
						else
						{
							// Does not need normal
							facep->setState(LLFace::FULLBRIGHT);
							if (fullbright_count < MAX_FACE_COUNT)
							{
								sFullbrightFaces[fullbright_count++] = facep;
							}
						}
					}
				}
				else
				{
					// Face has no renderable geometry
					facep->clearVertexBuffer();
				}
			}

			if (is_rigged)
			{
				if (!drawablep->isState(LLDrawable::RIGGED))
				{
					drawablep->setState(LLDrawable::RIGGED);

					// First time this is drawable is being marked as rigged,
					// do another LoD update to use avatar bounding box
					vobj->updateLOD();
				}
			}
			else
			{
				drawablep->clearState(LLDrawable::RIGGED);
				vobj->updateRiggedVolume();
			}
		}
	}

	group->mBufferUsage = usage;

	// Process non-alpha faces
	constexpr U32 BASE_MASK = LLVertexBuffer::MAP_TEXTURE_INDEX |
							  LLVertexBuffer::MAP_TEXCOORD0 |
							  LLVertexBuffer::MAP_VERTEX |
							  LLVertexBuffer::MAP_COLOR;

	U32 simple_mask = BASE_MASK | LLVertexBuffer::MAP_NORMAL;

	// hack to give alpha verts their own VBO
	U32 alpha_mask = simple_mask | 0x80000000;

	U32 bump_mask = simple_mask | LLVertexBuffer::MAP_TEXCOORD1;

	U32 fullbright_mask = BASE_MASK;

	U32 norm_mask = simple_mask | LLVertexBuffer::MAP_TEXCOORD1 |
					LLVertexBuffer::MAP_TANGENT;

	U32 normspec_mask = norm_mask | LLVertexBuffer::MAP_TEXCOORD2;

	U32 spec_mask = simple_mask | LLVertexBuffer::MAP_TEXCOORD2;

	if (emissive)
	{
		// Emissive faces are present, include emissive byte to preserve
		// batching
		simple_mask |= LLVertexBuffer::MAP_EMISSIVE;
		alpha_mask |= LLVertexBuffer::MAP_EMISSIVE;
		bump_mask |= LLVertexBuffer::MAP_EMISSIVE;
		fullbright_mask |= LLVertexBuffer::MAP_EMISSIVE;
		norm_mask |= LLVertexBuffer::MAP_EMISSIVE;
		normspec_mask |= LLVertexBuffer::MAP_EMISSIVE;
		spec_mask |= LLVertexBuffer::MAP_EMISSIVE;
	}

	bool batch_textures =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT) > 1;
	if (batch_textures)
	{
		bump_mask |= LLVertexBuffer::MAP_TANGENT;
		alpha_mask |= LLVertexBuffer::MAP_TANGENT |
					  LLVertexBuffer::MAP_TEXCOORD1 |
					  LLVertexBuffer::MAP_TEXCOORD2;
	}

	genDrawInfo(group, simple_mask, sSimpleFaces, simple_count, false,
				batch_textures);
	genDrawInfo(group, fullbright_mask, sFullbrightFaces, fullbright_count,
				false, batch_textures);
	genDrawInfo(group, alpha_mask, sAlphaFaces, alpha_count, true,
				batch_textures);
	genDrawInfo(group, bump_mask, sBumpFaces, bump_count);
	genDrawInfo(group, norm_mask, sNormFaces, norm_count);
	genDrawInfo(group, spec_mask, sSpecFaces, spec_count);
	genDrawInfo(group, normspec_mask, sNormSpecFaces, normspec_count);

	if (!LLPipeline::sDelayVBUpdate)
	{
		// Drawables have been rebuilt, clear rebuild status
		for (LLSpatialGroup::element_iter it = group->getDataBegin(),
										  end = group->getDataEnd();
			 it != end; ++it)
		{
			LLDrawable* drawablep = (LLDrawable*)(*it)->getDrawable();
			if (drawablep)
			{
				drawablep->clearState(LLDrawable::REBUILD_ALL);
			}
		}
	}

	group->mLastUpdateTime = gFrameTimeSeconds;
	group->mBuilt = 1.f;
	group->clearState(LLSpatialGroup::GEOM_DIRTY |
					  LLSpatialGroup::ALPHA_DIRTY);

	if (LLPipeline::sDelayVBUpdate)
	{
		group->setState(LLSpatialGroup::MESH_DIRTY |
						LLSpatialGroup::NEW_DRAWINFO);
	}

	if (voattachavp)
	{
		voattachavp->addAttachmentBytes(group->mGeometryBytes);
		voattachavp->addAttachmentArea(group->mSurfaceArea);
	}
	if (voavp && voavp != voattachavp)
	{
		voavp->addAttachmentBytes(group->mGeometryBytes);
		voavp->addAttachmentArea(group->mSurfaceArea);
	}
}

void LLVolumeGeometryManager::rebuildMesh(LLSpatialGroup* group)
{
	if (!group || !group->hasState(LLSpatialGroup::MESH_DIRTY) ||
		group->hasState(LLSpatialGroup::GEOM_DIRTY))
	{
		llassert(group);
		return;
	}

	LL_FAST_TIMER(FTM_VOLUME_GEOM);

	group->mBuilt = 1.f;

	S32 num_mapped_vertex_buffer = LLVertexBuffer::sMappedCount;

	constexpr U32 MAX_BUFFER_COUNT = 4096;
	LLVertexBuffer* locked_buffer[MAX_BUFFER_COUNT];

	U32 buffer_count = 0;

	for (LLSpatialGroup::element_iter it = group->getDataBegin(),
									  end = group->getDataEnd();
		 it != end; ++it)
	{
		LLDrawable* drawablep = (LLDrawable*)(*it)->getDrawable();
		if (!drawablep || drawablep->isDead() ||
			!drawablep->isState(LLDrawable::REBUILD_ALL) ||
			drawablep->isState(LLDrawable::RIGGED))
		{
			continue;
		}

		LLVOVolume* vobj = drawablep->getVOVolume();
		if (!vobj || vobj->getLOD() == -1)
		{
			continue;
		}

		LLVolume* volume = vobj->getVolume();
		if (!volume)
		{
			continue;
		}

		vobj->preRebuild();

		if (drawablep->isState(LLDrawable::ANIMATED_CHILD))
		{
			vobj->updateRelativeXform(true);
		}

		for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
		{
			LLFace* face = drawablep->getFace(i);
			if (!face) continue;

			LLVertexBuffer* buff = face->getVertexBuffer();
			if (!buff) continue;

			if (!face->getGeometryVolume(*volume, face->getTEOffset(),
										 vobj->getRelativeXform(),
										 vobj->getRelativeXformInvTrans(),
										 face->getGeomIndex()))
			{
				// Something gone wrong with the vertex buffer accounting,
				// rebuild this group
				group->dirtyGeom();
				gPipeline.markRebuild(group, true);
			}

			if (buff->isLocked() && buffer_count < MAX_BUFFER_COUNT)
			{
				locked_buffer[buffer_count++] = buff;
			}
		}

		if (drawablep->isState(LLDrawable::ANIMATED_CHILD))
		{
			vobj->updateRelativeXform();
		}

		drawablep->clearState(LLDrawable::REBUILD_ALL);
	}

	for (LLVertexBuffer** iter = locked_buffer,
					   ** end = locked_buffer + buffer_count;
		 iter != end; ++iter)
	{
		(*iter)->flush();
	}

	// Do not forget alpha
	if (group)
	{
		LLVertexBuffer* buffp = group->mVertexBuffer.get();
		if (buffp && buffp->isLocked())
		{
			buffp->flush();
		}
	}

	// If not all buffers are unmapped
	if (num_mapped_vertex_buffer != LLVertexBuffer::sMappedCount)
	{
		llwarns_once << "Not all mapped vertex buffers are unmapped !"
					 << llendl;
		for (LLSpatialGroup::element_iter it = group->getDataBegin(),
										  end = group->getDataEnd();
			 it != end; ++it)
		{
			LLDrawable* drawablep = (LLDrawable*)(*it)->getDrawable();
			if (!drawablep)
			{
				continue;
			}
			for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
			{
				LLFace* face = drawablep->getFace(i);
				if (face)
				{
					LLVertexBuffer* buff = face->getVertexBuffer();
					if (buff && buff->isLocked())
					{
						buff->flush();
					}
				}
			}
		}
	}

	group->clearState(LLSpatialGroup::MESH_DIRTY |
					  LLSpatialGroup::NEW_DRAWINFO);
}

struct CompareBatchBreakerModified
{
	bool operator()(const LLFace* const& lhs, const LLFace* const& rhs)
	{
		const LLTextureEntry* lte = lhs->getTextureEntry();
		const LLTextureEntry* rte = rhs->getTextureEntry();

		if (lte->getBumpmap() != rte->getBumpmap())
		{
			return lte->getBumpmap() < rte->getBumpmap();
		}
		if (lte->getFullbright() != rte->getFullbright())
		{
			return lte->getFullbright() < rte->getFullbright();
		}
		if (LLPipeline::sRenderDeferred &&
			lte->getMaterialParams() != rte->getMaterialParams())
		{
			return lte->getMaterialParams() < rte->getMaterialParams();
		}
		if (LLPipeline::sRenderDeferred &&
			lte->getMaterialParams() == rte->getMaterialParams() &&
			lte->getShiny() != rte->getShiny())
		{
			return lte->getShiny() < rte->getShiny();
		}
		return lhs->getTexture() < rhs->getTexture();
	}
};

void LLVolumeGeometryManager::genDrawInfo(LLSpatialGroup* group, U32 mask,
										  LLFace** faces, U32 face_count,
										  bool distance_sort,
										  bool batch_textures,
										  bool no_materials)
{
	LL_FAST_TIMER(FTM_REBUILD_VOLUME_GEN_DRAW_INFO);

	U32 buffer_usage = group->mBufferUsage;

#if LL_DARWIN
	// *HACK: from Leslie:
	// Disable VBO usage for alpha on macOS because it kills the framerate due
	// to implicit calls to glTexSubImage that are beyond our control (this
	// works because the only calls here that sort by distance are alpha).
	if (distance_sort)
	{
		buffer_usage = 0x0;
	}
#endif

	// Calculate maximum number of vertices to store in a single buffer
	U32 max_vertices =
		(LLVOVolume::sRenderMaxVBOSize * 1024) /
		LLVertexBuffer::calcVertexSize(group->getSpatialPartition()->mVertexDataMask);
	max_vertices = llmin(max_vertices, 65535U);

	{
		LL_FAST_TIMER(FTM_GEN_DRAW_INFO_SORT);
		if (!distance_sort)
		{
			// Sort faces by things that break batches
			std::sort(faces, faces + face_count,
					  CompareBatchBreakerModified());
		}
		else
		{
			// Sort faces by distance
			std::sort(faces, faces + face_count,
					  LLFace::CompareDistanceGreater());
		}
	}

	bool hud_group = group->isHUDGroup();
	LLFace** face_iter = faces;
	LLFace** end_faces = faces + face_count;

	LLSpatialGroup::buffer_map_t buffer_map;

	LLViewerTexture* last_tex = NULL;

	S32 buffer_index = distance_sort ? -1 : 0;

	S32 texture_index_channels = 1;
	if (gGLManager.mGLSLVersionMajor > 1 || gGLManager.mGLSLVersionMinor >= 30)
	{
		// - 1 to always reserve one for shiny for now just for simplicity
		texture_index_channels = LLGLSLShader::sIndexedTextureChannels - 1;
	}
	if (LLPipeline::sRenderDeferred && distance_sort)
	{
		texture_index_channels =
			gDeferredAlphaProgram.mFeatures.mIndexedTextureChannels;
	}
	static LLCachedControl<U32> max_texture_index(gSavedSettings,
												  "RenderMaxTextureIndex");
	texture_index_channels = llmin(texture_index_channels,
								   (S32)max_texture_index);
	// NEVER use more than 16 texture index channels (workaround for prevalent
	// driver bug)
	texture_index_channels = llmin(texture_index_channels, 16);

//MK
	bool restricted_vision = gRLenabled && gRLInterface.mVisionRestricted;
	F32 cam_dist_max_squared = EXTREMUM;
	LLVector3 joint_pos;
	if (restricted_vision)
	{
		LLJoint* ref_joint = gRLInterface.getCamDistDrawFromJoint();
		if (ref_joint)
		{
			// Calculate the position of the avatar here so we do not have to
			// do it for each face
			joint_pos = ref_joint->getWorldPosition();
			cam_dist_max_squared = gRLInterface.mCamDistDrawMax;
			cam_dist_max_squared *= cam_dist_max_squared;
		}
		else
		{
			restricted_vision = false;
		}
	}
//mk

	bool not_debugging_alpha = !LLDrawPoolAlpha::sShowDebugAlpha;
	bool flexi = false;
	while (face_iter != end_faces)
	{
		// Pull off next face
		LLFace* facep = *face_iter;
		if (!facep)
		{
			llwarns << "NULL face found !" << llendl;
			continue;
		}

//MK
		bool is_far_face = false;
		if (restricted_vision)
		{
			LLVector3 face_offset = facep->getPositionAgent() - joint_pos;
			is_far_face = face_offset.lengthSquared() > cam_dist_max_squared;
		}
//mk

		LLViewerTexture* tex = distance_sort ? NULL : facep->getTexture();
		if (last_tex == tex)
		{
			++buffer_index;
		}
		else
		{
			last_tex = tex;
			buffer_index = 0;
		}

		LLMaterialPtr mat = facep->getTextureEntry()->getMaterialParams();

		bool bake_sunlight = LLPipeline::sBakeSunlight &&
							 facep->getDrawable()->isStatic();

		U32 index_count = facep->getIndicesCount();
		U32 geom_count = facep->getGeomCount();

		flexi |= facep->getViewerObject()->getVolume()->isUnique();

		// Sum up vertices needed for this render batch
		LLFace** i = face_iter;
		++i;

		constexpr U32 MAX_TEXTURE_COUNT = 32;
		LLViewerTexture* texture_list[MAX_TEXTURE_COUNT];

		U32 texture_count = 0;

		{
			LL_FAST_TIMER(FTM_GEN_DRAW_INFO_FACE_SIZE);
			if (batch_textures)
			{
				U8 cur_tex = 0;
				facep->setTextureIndex(cur_tex);
				if (texture_count < MAX_TEXTURE_COUNT)
				{
					texture_list[texture_count++] = tex;
				}

				if (can_batch_texture(facep))
				{
					// Populate texture_list with any textures that can be
					// batched. Move i to the next unbatchable face.
					while (i != end_faces)
					{
						facep = *i;

						if (!can_batch_texture(facep))
						{
							// Face is bump mapped or has an animated texture
							// matrix; cannot batch more than 1 texture at a
							// time.
							facep->setTextureIndex(0);
							break;
						}

						if (facep->getTexture() != tex)
						{
							if (distance_sort)
							{
								// Textures might be out of order, see if
								// texture exists in current batch
								bool found = false;
								for (U32 tex_idx = 0; tex_idx < texture_count;
									 ++tex_idx)
								{
									if (facep->getTexture() == texture_list[tex_idx])
									{
										cur_tex = tex_idx;
										found = true;
										break;
									}
								}
								if (!found)
								{
									cur_tex = texture_count;
								}
							}
							else
							{
								++cur_tex;
							}

							if (cur_tex >= texture_index_channels)
							{
								// Cut batches when index channels are depleted
								break;
							}

							tex = facep->getTexture();
							if (texture_count < MAX_TEXTURE_COUNT)
							{
								texture_list[texture_count++] = tex;
							}
						}

						if (geom_count + facep->getGeomCount() > max_vertices)
						{
							// Cut batches on geom count too big
							break;
						}

						++i;
						index_count += facep->getIndicesCount();
						geom_count += facep->getGeomCount();

						facep->setTextureIndex(cur_tex);

						flexi |= facep->getViewerObject()->getVolume()->isUnique();
					}
				}
				else
				{
					facep->setTextureIndex(0);
				}

				tex = texture_list[0];
			}
			else
			{
				while (i != end_faces &&
					   (LLPipeline::sTextureBindTest ||
						(distance_sort ||
						 ((*i)->getTexture() == tex &&
						  (*i)->getTextureEntry()->getMaterialParams() == mat))))
				{
					facep = *i;

					// Face has no texture index
					facep->mDrawInfo = NULL;
					facep->setTextureIndex(FACE_DO_NOT_BATCH_TEXTURES);

					if (geom_count + facep->getGeomCount() > max_vertices)
					{
						// Cut batches on geom count too big
						break;
					}

					++i;
					index_count += facep->getIndicesCount();
					geom_count += facep->getGeomCount();

					flexi |= facep->getViewerObject()->getVolume()->isUnique();
				}
			}
		}

		if (flexi && buffer_usage && buffer_usage != GL_STREAM_DRAW_ARB)
		{
			buffer_usage = GL_STREAM_DRAW_ARB;
		}

		// Create vertex buffer
		LLPointer<LLVertexBuffer> buffer;
		{
			LL_FAST_TIMER(FTM_GEN_DRAW_INFO_ALLOCATE);
			buffer = createVertexBuffer(mask, buffer_usage);
			if (!buffer->allocateBuffer(geom_count, index_count, true))
			{
				llwarns << "Failure to resize a vertex buffer with "
						<< geom_count << " vertices and " << index_count
						<< " indices" << llendl;
				buffer = NULL;
			}
		}
		if (buffer.notNull())
		{
			group->mGeometryBytes += buffer->getSize() +
									 buffer->getIndicesSize();
			buffer_map[mask][*face_iter].push_back(buffer);
		}

		// Add face geometry

		U32 indices_index = 0;
		U16 index_offset = 0;

		bool can_use_vertex_shaders = gPipeline.canUseVertexShaders();

		while (face_iter < i)
		{
			// Update face indices for new buffer
			facep = *face_iter;

			if (buffer.isNull())
			{
				// Bulk allocation failed
				facep->setVertexBuffer(buffer);
				facep->setSize(0, 0); // Mark as no geometry
				++face_iter;
				continue;
			}

			facep->setIndicesIndex(indices_index);
			facep->setGeomIndex(index_offset);
			facep->setVertexBuffer(buffer);

			if (batch_textures &&
				facep->getTextureIndex() == FACE_DO_NOT_BATCH_TEXTURES)
			{
				llwarns_once << "Invalid texture index. Skipping." << llendl;
				++face_iter;
				continue;
			}

			// For debugging, set last time face was updated vs moved
			facep->updateRebuildFlags();

			if (!LLPipeline::sDelayVBUpdate)
			{
				// Copy face geometry into vertex buffer
				LLDrawable* drawablep = facep->getDrawable();
				LLVOVolume* vobj = drawablep->getVOVolume();
				LLVolume* volume = vobj->getVolume();

				if (drawablep->isState(LLDrawable::ANIMATED_CHILD))
				{
					vobj->updateRelativeXform(true);
				}

				U32 te_idx = facep->getTEOffset();

				llassert(!facep->isState(LLFace::RIGGED));

				if (!facep->getGeometryVolume(*volume, te_idx,
											  vobj->getRelativeXform(),
											  vobj->getRelativeXformInvTrans(),
											  index_offset, true))
				{
					llwarns << "Failed to get geometry for face !" << llendl;
				}

				if (drawablep->isState(LLDrawable::ANIMATED_CHILD))
				{
					vobj->updateRelativeXform(false);
				}
			}

			index_offset += facep->getGeomCount();
			indices_index += facep->getIndicesCount();

			// Append face to appropriate render batch

			bool force_simple =
				facep->getPixelArea() < FORCE_SIMPLE_RENDER_AREA;
			bool fullbright = facep->isState(LLFace::FULLBRIGHT);
			if ((mask & LLVertexBuffer::MAP_NORMAL) == 0)
			{
				// Paranoia check to make sure GL does not try to read
				// non-existant normals
				fullbright = true;
			}

			if (hud_group)
			{
				// All hud attachments are fullbright
				fullbright = true;
			}

			const LLTextureEntry* te = facep->getTextureEntry();
			tex = facep->getTexture();

			bool is_alpha = facep->getPoolType() == LLDrawPool::POOL_ALPHA;

			bool can_be_shiny = true;
			LLMaterial* mat = te->getMaterialParams().get();
			U8 diffuse_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
			if (mat)
			{
				diffuse_mode = mat->getDiffuseAlphaMode();
				can_be_shiny =
					diffuse_mode == LLMaterial::DIFFUSE_ALPHA_MODE_NONE ||
					diffuse_mode == LLMaterial::DIFFUSE_ALPHA_MODE_EMISSIVE;
			}

			bool use_legacy_bump = te->getBumpmap() && te->getBumpmap() < 18 &&
								   (!mat || mat->getNormalID().isNull());
			F32 alpha_channel = te->getColor().mV[3];
			// Do not render transparent faces, unless we highlight transparent
			if (not_debugging_alpha && alpha_channel < 0.001f)
			{
				++face_iter;
				continue;
			}
			bool opaque = alpha_channel >= 0.999f;
//MK
			if (restricted_vision)
			{
				// Due to a rendering bug, we must completely ignore the alpha
				// and fullbright of any object (except our own attachments and
				// 100% invisible objects) when the vision is restricted
				LLDrawable* drawablep = facep->getDrawable();
				LLVOVolume* vobj = drawablep->getVOVolume();
				if ((is_alpha || fullbright) && alpha_channel > 0.f)
				{
					if (vobj && vobj->getAvatar() != gAgentAvatarp)
					{
						// If this is an attachment with alpha or full bright
						// and its wearer is farther than the vision range, do
						// not render it at all
						if (is_far_face && vobj->isAttachment())
						{
							++face_iter;
							continue;
						}
						else if (vobj->flagPhantom())
						{
							// If the object is phantom, no need to even render
							// it at all. If it is solid, then a blind avatar
							// will have to "see" it since it may bump into it
							++face_iter;
							continue;
						}
						else if (is_far_face)
						{
							is_alpha = fullbright = can_be_shiny = false;
							opaque = no_materials = true;
						}
					}
				}
				else if (alpha_channel == 0.f &&
						 !(vobj && vobj->isAttachment()))
				{
					// Completely transparent and not an attachment => do not
					// bother rendering it at all (even when highlighting
					// transparent)
					++face_iter;
					continue;
				}
			}
//mk

			if (mat && !hud_group && LLPipeline::sRenderDeferred)
			{
				bool material_pass = false;

				// Do NOT use 'fullbright' for this logic or you risk sending
				// things without normals down the materials pipeline and will
				// render poorly if not crash NORSPEC-240,314
				if (te->getFullbright())
				{
					if (diffuse_mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
					{
						if (opaque)
						{
							registerFace(group, facep,
										 LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK);
						}
						else
						{
							registerFace(group, facep,
										 LLRenderPass::PASS_ALPHA);
						}
					}
					else if (is_alpha)
					{
						registerFace(group, facep, LLRenderPass::PASS_ALPHA);
					}
					else if (!restricted_vision &&	// mk
							 (te->getShiny() > 0 ||
							  mat->getSpecularID().notNull()))
					{
						material_pass = true;
					}
					else if (opaque)
					{
						registerFace(group, facep,
									 LLRenderPass::PASS_FULLBRIGHT);
					}
					else
					{
						registerFace(group, facep, LLRenderPass::PASS_ALPHA);
					}
				}
				else if (no_materials)
				{
					registerFace(group, facep, LLRenderPass::PASS_SIMPLE);
				}
				else if (!opaque)
				{
					registerFace(group, facep, LLRenderPass::PASS_ALPHA);
				}
				else if (use_legacy_bump)
				{
					// We have a material AND legacy bump settings, but no
					// normal map
					registerFace(group, facep, LLRenderPass::PASS_BUMP);
				}
				else
				{
					material_pass = true;
				}

				if (material_pass &&
					diffuse_mode != LLMaterial::DIFFUSE_ALPHA_MODE_DEFAULT)
				{
					static const U32 pass[] =
					{
						LLRenderPass::PASS_MATERIAL,
						LLRenderPass::PASS_ALPHA, //LLRenderPass::PASS_MATERIAL_ALPHA,
						LLRenderPass::PASS_MATERIAL_ALPHA_MASK,
						LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
						LLRenderPass::PASS_SPECMAP,
						LLRenderPass::PASS_ALPHA, //LLRenderPass::PASS_SPECMAP_BLEND,
						LLRenderPass::PASS_SPECMAP_MASK,
						LLRenderPass::PASS_SPECMAP_EMISSIVE,
						LLRenderPass::PASS_NORMMAP,
						LLRenderPass::PASS_ALPHA, //LLRenderPass::PASS_NORMMAP_BLEND,
						LLRenderPass::PASS_NORMMAP_MASK,
						LLRenderPass::PASS_NORMMAP_EMISSIVE,
						LLRenderPass::PASS_NORMSPEC,
						LLRenderPass::PASS_ALPHA, //LLRenderPass::PASS_NORMSPEC_BLEND,
						LLRenderPass::PASS_NORMSPEC_MASK,
						LLRenderPass::PASS_NORMSPEC_EMISSIVE,
					};

					U32 mask = mat->getShaderMask();

					llassert(mask < sizeof(pass) / sizeof(U32));

					mask = llmin(mask, (U32)(sizeof(pass) / sizeof(U32) - 1));

					registerFace(group, facep, pass[mask]);
				}
			}
			else if (mat)
			{
				U8 mode = diffuse_mode;
				is_alpha |= mode == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
				if (is_alpha)
				{
					mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
				}

				if (mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
				{
					registerFace(group, facep,
								 fullbright ? LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK
											: LLRenderPass::PASS_ALPHA_MASK);
				}
				else if (is_alpha)
				{
					registerFace(group, facep, LLRenderPass::PASS_ALPHA);
				}
				else if (can_be_shiny && can_use_vertex_shaders &&
						 LLPipeline::sRenderBump && te->getShiny())
				{
					registerFace(group, facep,
								 fullbright ? LLRenderPass::PASS_FULLBRIGHT_SHINY
											: LLRenderPass::PASS_SHINY);
				}
				else
				{
					registerFace(group, facep,
								 fullbright ? LLRenderPass::PASS_FULLBRIGHT
											: LLRenderPass::PASS_SIMPLE);
				}
			}
			else if (is_alpha)
			{
				// Can we safely treat this as an alpha mask ?
				if (facep->getFaceColor().mV[3] <= 0.f)
				{
					// 100% transparent, do not render unless we are
					// highlighting transparent
					registerFace(group, facep,
								 LLRenderPass::PASS_ALPHA_INVISIBLE);
				}
				else if (facep->canRenderAsMask())
				{
					if (te->getFullbright() || LLPipeline::sNoAlpha)
					{
						registerFace(group, facep,
									 LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK);
					}
					else
					{
						registerFace(group, facep,
									 LLRenderPass::PASS_ALPHA_MASK);
					}
				}
				else
				{
					registerFace(group, facep, LLRenderPass::PASS_ALPHA);
				}
			}
			else if (can_be_shiny && can_use_vertex_shaders &&
					 LLPipeline::sRenderBump && te->getShiny())
			{
				// Shiny
				if (tex->getPrimaryFormat() == GL_ALPHA)
				{
					// Invisiprim + shiny
					registerFace(group, facep, LLRenderPass::PASS_INVISI_SHINY);
					registerFace(group, facep, LLRenderPass::PASS_INVISIBLE);
				}
				else if (!hud_group && LLPipeline::sRenderDeferred)
				{
					// Deferred rendering
					if (te->getFullbright())
					{
						// Register in post deferred fullbright shiny pass
						registerFace(group, facep, LLRenderPass::PASS_FULLBRIGHT_SHINY);
						if (te->getBumpmap())
						{
							// Register in post deferred bump pass
							registerFace(group, facep, LLRenderPass::PASS_POST_BUMP);
						}
					}
					else if (use_legacy_bump)
					{
						// Register in deferred bump pass
						registerFace(group, facep, LLRenderPass::PASS_BUMP);
					}
					else
					{
						// Register in deferred simple pass (deferred simple
						// includes shiny)
						llassert(mask & LLVertexBuffer::MAP_NORMAL);
						registerFace(group, facep, LLRenderPass::PASS_SIMPLE);
					}
				}
				else if (fullbright)
				{
					// Not deferred, register in standard fullbright shiny pass
					registerFace(group, facep, LLRenderPass::PASS_FULLBRIGHT_SHINY);
				}
				else
				{
					// Not deferred or fullbright, register in standard shiny
					// pass
					registerFace(group, facep, LLRenderPass::PASS_SHINY);
				}
			}
			else	// Not alpha and not shiny
			{
				if (!is_alpha && tex->getPrimaryFormat() == GL_ALPHA)
				{
					// Invisiprim
					registerFace(group, facep, LLRenderPass::PASS_INVISIBLE);
				}
				else if (fullbright || bake_sunlight)
				{
					// Fullbright
					if (mat &&
						diffuse_mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
					{
						registerFace(group, facep,
									 LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK);
					}
					else
					{
						registerFace(group, facep,
									 LLRenderPass::PASS_FULLBRIGHT);
					}
					if (!hud_group && LLPipeline::sRenderDeferred &&
						LLPipeline::sRenderBump && use_legacy_bump)
					{
						// If this is the deferred render and a bump map is
						// present, register in post deferred bump
						registerFace(group, facep,
									 LLRenderPass::PASS_POST_BUMP);
					}
				}
				else if (LLPipeline::sRenderDeferred &&
						 LLPipeline::sRenderBump && use_legacy_bump)
				{
					// Non-shiny or fullbright deferred bump
					registerFace(group, facep, LLRenderPass::PASS_BUMP);
				}
				// All around simple from there
				else if (mat &&
						diffuse_mode == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
				{
					llassert(mask & LLVertexBuffer::MAP_NORMAL);
					// Material alpha mask can be respected in non-deferred
					registerFace(group, facep, LLRenderPass::PASS_ALPHA_MASK);
				}
				else
				{
					llassert(mask & LLVertexBuffer::MAP_NORMAL);
					registerFace(group, facep, LLRenderPass::PASS_SIMPLE);
				}

				if (!can_use_vertex_shaders && !is_alpha &&
					LLPipeline::sRenderBump && te->getShiny())
				{
					// Shiny has an extra pass when shaders are disabled
					registerFace(group, facep, LLRenderPass::PASS_SHINY);
				}
			}

			// Not sure why this is here, and looks like it might cause bump
			// mapped objects to get rendered redundantly -- davep 5/11/2010
			if (!is_alpha && (hud_group || !LLPipeline::sRenderDeferred))
			{
				llassert((mask & LLVertexBuffer::MAP_NORMAL) || fullbright);
				facep->setPoolType(fullbright ? LLDrawPool::POOL_FULLBRIGHT
											  : LLDrawPool::POOL_SIMPLE);

				if (!force_simple && use_legacy_bump &&
					LLPipeline::sRenderBump)
				{
					registerFace(group, facep, LLRenderPass::PASS_BUMP);
				}
			}

			if (!is_alpha && LLPipeline::RenderGlow && te->getGlow() > 0.f)
			{
//MK
				if (is_far_face)
				{
					registerFace(group, facep, LLRenderPass::PASS_SIMPLE);
				}
				else
//mk
				{
					registerFace(group, facep, LLRenderPass::PASS_GLOW);
				}
			}

			++face_iter;
		}

		if (buffer.notNull())
		{
			buffer->flush();
		}
	}

	// Replace old buffer map with the new one (swapping them is by far the
	// fastest way to do this). HB
	group->mBufferMap[mask].swap(buffer_map[mask]);
}

//virtual
void LLVolumeGeometryManager::addGeometryCount(LLSpatialGroup* group,
											   U32& vertex_count,
											   U32& index_count)
{
	// Initialize to default usage for this partition
	U32 usage = group->getSpatialPartition()->mBufferUsage;

	// For each drawable
	for (LLSpatialGroup::element_iter it = group->getDataBegin(),
									  end = group->getDataEnd();
		 it != end; ++it)
	{
		LLDrawable* drawablep = (LLDrawable*)(*it)->getDrawable();
		if (!drawablep || drawablep->isDead())
		{
			continue;
		}

		if (drawablep->isAnimating())
		{
			// Fall back to stream draw for animating verts
			usage = GL_STREAM_DRAW_ARB;
		}
	}

	group->mBufferUsage = usage;
}

//virtual
void LLGeometryManager::addGeometryCount(LLSpatialGroup* group,
										 U32& vertex_count, U32& index_count)
{
	// Initialize to default usage for this partition
	U32 usage = group->getSpatialPartition()->mBufferUsage;

	// Clear off any old faces
	mFaceList.clear();

	// For each drawable
	for (LLSpatialGroup::element_iter it = group->getDataBegin(),
									  end = group->getDataEnd();
		 it != end; ++it)
	{
		LLDrawable* drawablep = (LLDrawable*)(*it)->getDrawable();
		if (!drawablep || drawablep->isDead())
		{
			continue;
		}

		if (drawablep->isAnimating())
		{
			// Fall back to stream draw for animating verts
			usage = GL_STREAM_DRAW_ARB;
		}

		// For each face
		for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
		{
			// Sum up face verts and indices
			drawablep->updateFaceSize(i);
			LLFace* facep = drawablep->getFace(i);
			if (facep)
			{
				if (facep->hasGeometry() &&
					facep->getPixelArea() > FORCE_CULL_AREA &&
					facep->getGeomCount() + vertex_count <= 65536)
				{
					vertex_count += facep->getGeomCount();
					index_count += facep->getIndicesCount();

					// Remember face (for sorting)
					mFaceList.push_back(facep);
				}
				else
				{
					facep->clearVertexBuffer();
				}
			}
		}
	}

	group->mBufferUsage = usage;
}

LLHUDPartition::LLHUDPartition(LLViewerRegion* regionp)
:	LLBridgePartition(regionp)
{
	mPartitionType = LLViewerRegion::PARTITION_HUD;
	mDrawableType = LLPipeline::RENDER_TYPE_HUD;
	mSlopRatio = 0.f;
	mLODPeriod = 1;
}

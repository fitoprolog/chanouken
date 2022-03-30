/**
 * @file llviewertexture.h
 * @brief Object for managing images and their textures
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_LLVIEWERTEXTURE_H
#define LL_LLVIEWERTEXTURE_H

#include <list>

#include "llcorehttpcommon.h"
#include "llfastmap.h"
#include "llframetimer.h"
#include "llgltexture.h"
#include "llhost.h"
#include "llrender.h"
#include "lltimer.h"
#include "lluuid.h"

#include "llface.h"

#define BYTES2MEGABYTES(x) ((x) >> 20)

class LLImageGL;
class LLImageRaw;
class LLMessageSystem;
class LLViewerObject;
class LLViewerTexture;
class LLViewerFetchedTexture;
class LLViewerMediaTexture;
class LLVOVolume;
class LLViewerMediaImpl;

typedef	void (*loaded_callback_func)(bool success,
									 LLViewerFetchedTexture* src_vi,
									 LLImageRaw* src,
									 LLImageRaw* src_aux,
									 S32 discard_level,
									 bool final,
									 void* userdata);

class LLLoadedCallbackEntry
{
public:
	LLLoadedCallbackEntry(loaded_callback_func cb,
						  S32 discard_level,
						  // whether we need image raw for the callback
						  bool need_imageraw,
						  void* userdata, uuid_list_t* src_cb_list,
						  LLViewerFetchedTexture* target, bool pause);

	void removeTexture(LLViewerFetchedTexture* tex);

	loaded_callback_func	mCallback;
	S32						mLastUsedDiscard;
	S32						mDesiredDiscard;
	bool					mNeedsImageRaw;
	bool				    mPaused;
	void*					mUserData;
	uuid_list_t*			mSourceCallbackList;

public:
	static void cleanUpCallbackList(uuid_list_t* cb_list);
};

class LLTextureBar;

class LLViewerTexture : public LLGLTexture
{
	friend class LLBumpImageList;
	friend class LLUIImageList;
	friend class LLTextureBar;

protected:
	LOG_CLASS(LLViewerTexture);

	~LLViewerTexture() override;

	void cleanup();
	void init(bool firstinit);
	void reorganizeFaceList();
	void reorganizeVolumeList();

	void notifyAboutMissingAsset();
	void notifyAboutCreatingTexture();

public:
	enum
	{
		LOCAL_TEXTURE,
		MEDIA_TEXTURE,
		DYNAMIC_TEXTURE,
		FETCHED_TEXTURE,
		LOD_TEXTURE,
		INVALID_TEXTURE_TYPE
	};

	typedef std::vector<LLFace*> ll_face_list_t;
	typedef std::vector<LLVOVolume*> ll_volume_list_t;

	static void initClass();
	static void updateClass(F32 velocity, F32 angular_velocity);

	LLViewerTexture(bool usemipmaps = true);
	LLViewerTexture(const LLUUID& id, bool usemipmaps);
	LLViewerTexture(const LLImageRaw* raw, bool usemipmaps);
	LLViewerTexture(U32 width, U32 height, U8 components, bool usemipmaps);

	void setNeedsAlphaAndPickMask(bool b);

	S8 getType() const override;

	LL_INLINE virtual bool isViewerMediaTexture() const		{ return false; }

	LL_INLINE virtual bool isMissingAsset() const			{ return false; }

	void dump() override;	// Debug info to llinfos

	bool bindDefaultImage(S32 stage = 0) override;
	LL_INLINE void forceImmediateUpdate() override			{}

	LL_INLINE const LLUUID& getID() const override			{ return mID; }

	void setBoostLevel(S32 level);
	LL_INLINE S32 getBoostLevel()							{ return mBoostLevel; }

	void addTextureStats(F32 virtual_size,
						 bool needs_gltexture = true) const;
	void resetTextureStats();

	LL_INLINE void setMaxVirtualSizeResetInterval(S32 interval) const
	{
		mMaxVirtualSizeResetInterval = interval;
	}

	LL_INLINE void resetMaxVirtualSizeResetCounter() const
	{
		mMaxVirtualSizeResetCounter = mMaxVirtualSizeResetInterval;
	}

	LL_INLINE virtual F32 getMaxVirtualSize()				{ return mMaxVirtualSize; }

	void resetLastReferencedTime();
	F32 getElapsedLastReferenceTime();

	LL_INLINE void setKnownDrawSize(S32, S32) override		{}

	virtual void addFace(U32 channel, LLFace* facep);
	virtual void removeFace(U32 channel, LLFace* facep);
	S32 getTotalNumFaces() const;
	S32 getNumFaces(U32 ch) const;

	LL_INLINE const ll_face_list_t* getFaceList(U32 ch) const
	{
		return ch < LLRender::NUM_TEXTURE_CHANNELS ? &mFaceList[ch] : NULL;
	}

	virtual void addVolume(U32 ch, LLVOVolume* volumep);
	virtual void removeVolume(U32 ch, LLVOVolume* volumep);

	LL_INLINE S32 getNumVolumes(U32 ch) const				{ return mNumVolumes[ch]; }

	LL_INLINE const ll_volume_list_t* getVolumeList(U32 ch) const
	{
		return &mVolumeList[ch];
	}

	LL_INLINE virtual void setCachedRawImage(S32 discard_level,
											 LLImageRaw* imageraw)
	{
	}

	LL_INLINE bool isLargeImage()							
	{
		return mTexelsPerImage > sMinLargeImageSize;
	}

	LL_INLINE void setParcelMedia(LLViewerMediaTexture* m)	{ mParcelMedia = m; }
	LL_INLINE bool hasParcelMedia() const					{ return mParcelMedia != NULL; }
	LL_INLINE LLViewerMediaTexture* getParcelMedia() const	{ return mParcelMedia; }

private:
	LL_INLINE virtual void switchToCachedImage()			{}

	static bool isMemoryForTextureLow(F32& discard);

protected:
	// Do not use LLPointer here.
	LLViewerMediaTexture* mParcelMedia;

	LLUUID			mID;

	// The largest virtual size of the image, in pixels: how much data to we
	// need ?
	mutable F32		mMaxVirtualSize;
	mutable S32		mMaxVirtualSizeResetCounter;
	mutable S32		mMaxVirtualSizeResetInterval;
	// Priority adding to mDecodePriority.
	mutable F32		mAdditionalDecodePriority;

	F32				mLastReferencedTime;
	F32				mLastFaceListUpdate;
	F32				mLastVolumeListUpdate;

	U32				mNumFaces[LLRender::NUM_TEXTURE_CHANNELS];
	U32				mNumVolumes[LLRender::NUM_VOLUME_TEXTURE_CHANNELS];

	// Reverse pointer pointing to the faces using this image as texture
	ll_face_list_t	mFaceList[LLRender::NUM_TEXTURE_CHANNELS];

	ll_volume_list_t mVolumeList[LLRender::NUM_VOLUME_TEXTURE_CHANNELS];

public:
	static S32			sImageCount;
	static S32			sRawCount;
	static S32			sAuxCount;
	static F32			sDesiredDiscardBias;
	static F32			sDesiredDiscardScale;
	static S64			sBoundTextureMemoryInBytes;
	static S64			sTotalTextureMemoryInBytes;
	static S64			sMaxBoundTextureMemInMegaBytes;
	static S64			sMaxTotalTextureMemInMegaBytes;
	static F32			sCameraMovingBias;
	static S32			sMinLargeImageSize;
	static S32			sMaxSmallImageSize;
	static F32			sCurrentTime;
	static LLFrameTimer	sEvaluationTimer;
	static S8			sCameraMovingDiscardBias;
	static bool			sDontLoadVolumeTextures;

	// "Null" texture for non-textured objects.
	static LLPointer<LLViewerTexture> sNullImagep;
	// Texture for rezzing avatars particle cloud
	static LLPointer<LLViewerTexture> sCloudImagep;
};

constexpr S32 MAX_SCULPT_REZ = 128; // Maximum sculpt image size

enum FTType
{
	FTT_UNKNOWN = -1,
	FTT_DEFAULT = 0,	// Standard texture fetched by Id.
	FTT_SERVER_BAKE,	// Texture fetched from the baking service.
	FTT_HOST_BAKE,		// Baked tex uploaded by viewer and fetched from sim.
	FTT_MAP_TILE,		// Tiles fetched from map server.
	FTT_LOCAL_FILE		// Fetched directly from a local file.
};

const std::string& fttype_to_string(const FTType& fttype);

//
// Textures are managed in gTextureList. Raw image data is fetched from remote
// or local cache but the raw image this texture pointing to is fixed.
//
class LLViewerFetchedTexture : public LLViewerTexture
{
	friend class LLTextureBar;	// Debug info only
	friend class LLTextureView;	// Debug info only

protected:
	~LLViewerFetchedTexture() override;

public:
	LLViewerFetchedTexture(const LLUUID& id, FTType f_type,
						   const LLHost& host = LLHost(),
						   bool usemipmaps = true);
	LLViewerFetchedTexture(const LLImageRaw* raw, FTType f_type,
						   bool usemipmaps);
	LLViewerFetchedTexture(const std::string& url, FTType f_type,
						   const LLUUID& id, bool usemipmaps = true);

	LL_INLINE LLViewerFetchedTexture* asFetched() override
	{
		return this;
	}

	static F32 maxDecodePriority();

	struct Compare
	{
		// lhs < rhs
		LL_INLINE bool operator()(const LLPointer<LLViewerFetchedTexture>& lhs,
								  const LLPointer<LLViewerFetchedTexture>& rhs) const
		{
			const LLViewerFetchedTexture* lhsp = (const LLViewerFetchedTexture*)lhs;
			const LLViewerFetchedTexture* rhsp = (const LLViewerFetchedTexture*)rhs;
			// Greater priority is "less"
			const F32 lpriority = lhsp->getDecodePriority();
			const F32 rpriority = rhsp->getDecodePriority();
			if (lpriority > rpriority) // Higher priority
			{
				return true;
			}
			if (lpriority < rpriority)
			{
				return false;
			}
			return lhsp < rhsp;
		}
	};

	S8 getType() const override;
	LL_INLINE FTType getFTType() const						{ return mFTType; }
	void forceImmediateUpdate() override;
	void dump() override;

	// Set callbacks to get called when the image gets updated with higher
	// resolution versions.
	void setLoadedCallback(loaded_callback_func cb, S32 discard_level,
						   bool keep_imageraw, bool needs_aux, void* userdata,
						   uuid_list_t* cbs, bool pause = false);
	LL_INLINE bool hasCallbacks()							{ return !mLoadedCallbackList.empty(); }
	void pauseLoadedCallbacks(const uuid_list_t* cb_list);
	void unpauseLoadedCallbacks(const uuid_list_t* cb_list);
	bool doLoadedCallbacks();
	void deleteCallbackEntry(const uuid_list_t* cb_list);
	void clearCallbackEntryList();

	void addToCreateTexture();

	 // ONLY call from LLViewerTextureList
	bool createTexture(S32 usename = 0);
	void destroyTexture();

	virtual void processTextureStats();
	F32 calcDecodePriority();

	LL_INLINE bool needsAux() const							{ return mNeedsAux; }

	// Host we think might have this image, used for baked av textures.
	LL_INLINE void setTargetHost(LLHost host)				{ mTargetHost = host; }
	LL_INLINE LLHost getTargetHost() const					{ return mTargetHost; }

	// Set the decode priority for this image...
	// DON'T CALL THIS UNLESS YOU KNOW WHAT YOU'RE DOING, it can mess up
	// the priority list, and cause horrible things to happen.
	void setDecodePriority(F32 priority = -1.0f);
	LL_INLINE F32 getDecodePriority() const					{ return mDecodePriority; };

	void setAdditionalDecodePriority(F32 priority);

	void updateVirtualSize();

	LL_INLINE S32 getDesiredDiscardLevel()					{ return mDesiredDiscardLevel; }
	LL_INLINE void setMinDiscardLevel(S32 discard)			{ mMinDesiredDiscardLevel = llmin(mMinDesiredDiscardLevel,(S8)discard); }

	bool updateFetch();

	void clearFetchedResults();	// Clear all fetched results, for debug use.

	// Override the computation of discard levels if we know the exact output
	// size of the image.  Used for UI textures to not decode, even if we have
	// more data.
	void setKnownDrawSize(S32 width, S32 height) override;

	void setIsMissingAsset(bool is_missing = true);
	LL_INLINE bool isMissingAsset() const override			{ return mIsMissingAsset; }

	// returns dimensions of original image for local files (before power of two scaling)
	// and returns 0 for all asset system images
	LL_INLINE S32 getOriginalWidth()						{ return mOrigWidth; }
	LL_INLINE S32 getOriginalHeight()						{ return mOrigHeight; }

	LL_INLINE bool isInImageList() const					{ return mInImageList; }
	LL_INLINE void setInImageList(bool flag)				{ mInImageList = flag; }

	LL_INLINE U32 getFetchPriority() const					{ return mFetchPriority; }
	LL_INLINE F32 getDownloadProgress() const				{ return mDownloadProgress; }

	LLImageRaw* reloadRawImage(S8 discard_level);
	void destroyRawImage();
	LL_INLINE bool needsToSaveRawImage()					{ return mForceToSaveRawImage || mSaveRawImage; }

	LL_INLINE const std::string& getUrl() const				{ return mUrl; }
	LL_INLINE void setUrl(const std::string& url)			{ mUrl = url; }

	void setDeletionCandidate();
	void setInactive();

	LL_INLINE bool isDeleted()								{ return mTextureState == DELETED; }
	LL_INLINE bool isInactive()								{ return mTextureState == INACTIVE; }
	LL_INLINE bool isDeletionCandidate()					{ return mTextureState == DELETION_CANDIDATE; }
	LL_INLINE bool getUseDiscard() const					{ return mUseMipMaps && !mDontDiscard; }

	void setForSculpt();
	LL_INLINE bool forSculpt() const						{ return mForSculpt; }
	LL_INLINE bool isForSculptOnly() const					{ return mForSculpt && !mNeedsGLTexture; }

	//raw image management
	void checkCachedRawSculptImage();
	LL_INLINE LLImageRaw* getRawImage() const				{ return mRawImage; }
	LL_INLINE S32 getRawImageLevel() const					{ return mRawDiscardLevel; }
	LL_INLINE LLImageRaw* getCachedRawImage() const			{ return mCachedRawImage;}
	LL_INLINE S32 getCachedRawImageLevel() const			{ return mCachedRawDiscardLevel; }
	LL_INLINE bool isCachedRawImageReady() const			{ return mCachedRawImageReady; }
	LL_INLINE bool isRawImageValid() const					{ return mIsRawImageValid; }
	void forceToSaveRawImage(S32 desired_discard = 0, F32 kept_time = 0.f);

	void setCachedRawImage(S32 discard_level, LLImageRaw* imageraw) override;

	void destroySavedRawImage();
	LLImageRaw* getSavedRawImage();
	LL_INLINE bool hasSavedRawImage() const					{ return mSavedRawImage.notNull(); }

	F32 getElapsedLastReferencedSavedRawImageTime() const;
	bool isFullyLoaded() const;

	LL_INLINE bool hasFetcher() const						{ return mHasFetcher;}
	LL_INLINE void setCanUseHTTP(bool can_use_http)			{ mCanUseHTTP = can_use_http; }

	LL_INLINE void setSkipCache(bool skip)					{ mSkipCache = skip; }

	// Forces to re-fetch an image (for corrupted ones)
	void forceRefetch();
	// Mark request for fetch as deleted to avoid false missing asset issues
	void requestWasDeleted();

#if LL_FAST_TEX_CACHE
	void		loadFromFastCache();
	LL_INLINE void setInFastCacheList(bool in_list)			{ mInFastCacheList = in_list; }
	LL_INLINE bool isInFastCacheList()						{ return mInFastCacheList; }
#endif

protected:
	void switchToCachedImage() override;
	S32 getCurrentDiscardLevelForFetching();

private:
	void init(bool firstinit);
	void cleanup();

	void saveRawImage();
	void setCachedRawImage();
	void setCachedRawImagePtr(LLImageRaw* pRawImage);

public:
	// Texture to show NOTHING (whiteness)
	static LLPointer<LLViewerFetchedTexture> sWhiteImagep;
	// "Default" texture for error cases, the only case of fetched texture
	// which is generated in local.
	static LLPointer<LLViewerFetchedTexture> sDefaultImagep;
	// Old "Default" translucent texture
	static LLPointer<LLViewerFetchedTexture> sSmokeImagep;
	// Flat normal map denoting no bumpiness on a surface
	static LLPointer<LLViewerFetchedTexture> sFlatNormalImagep;
	// Default Sun image
	static LLPointer<LLViewerFetchedTexture> sDefaultSunImagep;
	// Default Moon image
	static LLPointer<LLViewerFetchedTexture> sDefaultMoonImagep;
	// Default Clouds image
	static LLPointer<LLViewerFetchedTexture> sDefaultCloudsImagep;
	// Cloud noise image
	static LLPointer<LLViewerFetchedTexture> sDefaultCloudNoiseImagep;
	// Bloom image
	static LLPointer<LLViewerFetchedTexture> sBloomImagep;
	// Opaque water image
	static LLPointer<LLViewerFetchedTexture> sOpaqueWaterImagep;
	// Transparent water image
	static LLPointer<LLViewerFetchedTexture> sWaterImagep;
	// Water normal map
	static LLPointer<LLViewerFetchedTexture> sWaterNormapMapImagep;

protected:
	S32						mOrigWidth;
	S32						mOrigHeight;

	// Override the computation of discard levels if we know the exact output
	// size of the image. Used for UI textures to not decode, even if we have
	// more data.
	S32						mKnownDrawWidth;
	S32						mKnownDrawHeight;

	S32						mRequestedDiscardLevel;
	F32						mRequestedDownloadPriority;
	S32						mFetchState;
	U32						mFetchPriority;
	F32						mDownloadProgress;
	F32						mFetchDeltaTime;
	F32						mRequestDeltaTime;

	// The priority for decoding this image.
	F32						mDecodePriority;

	S32						mMinDiscardLevel;

	// The discard level we'd LIKE to have - if we have it and there's space
	S8						mDesiredDiscardLevel;

	// The minimum discard level we would like to have
	S8						mMinDesiredDiscardLevel;

	// Result of the most recently completed http request for this texture.
	LLCore::HttpStatus		mLastHttpGetStatus;

	// If invalid, just request from agent's simulator
	LLHost					mTargetHost;

	FTType					mFTType;	// What category of image is this ?

	typedef std::list<LLLoadedCallbackEntry*> callback_list_t;
	callback_list_t			mLoadedCallbackList;
	F32						mLastCallBackActiveTime;

	LLPointer<LLImageRaw>	mRawImage;
	// Used ONLY for cloth meshes right now.  Make SURE you know what you're
	// doing if you use it for anything else! - djs
	LLPointer<LLImageRaw>	mAuxRawImage;
	LLPointer<LLImageRaw>	mSavedRawImage;
	// A small version of the copy of the raw image (<= 64 * 64)
	LLPointer<LLImageRaw>	mCachedRawImage;
	S32						mCachedRawDiscardLevel;

	S32						mRawDiscardLevel;
	S32						mSavedRawDiscardLevel;
	S32						mDesiredSavedRawDiscardLevel;
	F32						mLastReferencedSavedRawImageTime;
	F32						mKeptSavedRawImageTime;

	// Timing
	// Last time a packet was received.
	F32						mLastPacketTime;
	// Last time mDecodePriority was zeroed.
	F32						mStopFetchingTime;

	S8						mLoadedCallbackDesiredDiscardLevel;

	// true if we know that there is no image asset with this image id in the
	// database.
	mutable bool			mIsMissingAsset;
	// true if we deleted the fetch request in flight: used to avoid false
	// missing asset cases.
	mutable bool			mWasDeleted;

	// We need to decode the auxiliary channels
	bool					mNeedsAux;
	bool					mIsRawImageValid;
	bool					mHasFetcher;		// We've made a fecth request
	bool					mIsFetching;		// Fetch request is active

	// This texture can be fetched through http if true.
	bool					mCanUseHTTP;

	// Skip loading from cache when true
	bool					mSkipCache;

	bool					mKnownDrawSizeChanged;

	bool					mPauseLoadedCallBacks;

	// Keep a copy of mRawImage for some special purposes when
	// mForceToSaveRawImage is set.
	bool					mForceToSaveRawImage;
	bool					mSaveRawImage;

	// The rez of the mCachedRawImage reaches the upper limit.
	bool					mCachedRawImageReady;

	// true if image is in list (in which case do not reset priority !)
	bool					mInImageList;
	bool					mNeedsCreateTexture;

	// A flag if the texture is used as sculpt data.
	bool					mForSculpt;

	std::string				mLocalFileName;
	std::string				mUrl;

private:
	bool					mFullyLoaded;
#if LL_FAST_TEX_CACHE
	bool					mInFastCacheList;
#endif
};

//
// The image data is fetched from remote or from local cache. The resolution of
// the texture is adjustable: depends on the view-dependent parameters.
//
class LLViewerLODTexture final : public LLViewerFetchedTexture
{
protected:
	~LLViewerLODTexture() override = default;

public:
	LLViewerLODTexture(const LLUUID& id, FTType f_type,
					   const LLHost& host = LLHost(),
					   bool usemipmaps = true);
	LLViewerLODTexture(const std::string& url, FTType f_type,
					   const LLUUID& id, bool usemipmaps = true);

	S8 getType() const override;
	// Process image stats to determine priority/quality requirements.
	void processTextureStats() override;
	bool isUpdateFrozen();

private:
	void init(bool firstinit);
	bool scaleDown();

private:
	// Virtual size used to calculate desired discard
	F32 mDiscardVirtualSize;
	// Last calculated discard level
	F32 mCalculatedDiscardLevel;
};

//
// The image data is fetched from the media pipeline periodically; the
// resolution of the texture is also adjusted by the media pipeline.
//
class LLViewerMediaTexture final : public LLViewerTexture
{
protected:
	~LLViewerMediaTexture() override;

public:
	LLViewerMediaTexture(const LLUUID& id, bool usemipmaps = true,
						 LLImageGL* gl_image = NULL);

	S8 getType() const override;

	void reinit(bool usemipmaps = true);

	LL_INLINE bool getUseMipMaps()							{ return mUseMipMaps; }
	void setUseMipMaps(bool mipmap);

	void setPlaying(bool playing);
	LL_INLINE bool isPlaying() const						{ return mIsPlaying; }
	void setMediaImpl();

	void initVirtualSize();
	void invalidateMediaImpl();

	void addMediaToFace(LLFace* facep);
	void removeMediaFromFace(LLFace* facep);

	LL_INLINE bool isViewerMediaTexture() const override	{ return true; }

	void addFace(U32 ch, LLFace* facep) override;
	void removeFace(U32 ch, LLFace* facep) override;

	F32  getMaxVirtualSize() override;

	static void updateClass();
	static void cleanUpClass();

	static LLViewerMediaTexture* findMediaTexture(const LLUUID& media_id);
	static void removeMediaImplFromTexture(const LLUUID& media_id);

private:
	void switchTexture(U32 ch, LLFace* facep);
	bool findFaces();
	void stopPlaying();

private:
	// An instant list, recording all faces referencing or can reference to
	// this media texture. NOTE: it is NOT thread safe.
	std::list<LLFace*>	mMediaFaceList;

	// An instant list keeping all textures which are replaced by the current
	// media texture, is only used to avoid the removal of those textures from
	// memory.
	std::list<LLPointer<LLViewerTexture> > mTextureList;

	LLViewerMediaImpl*	mMediaImplp;
	U32					mUpdateVirtualSizeTime;
	bool				mIsPlaying;

	typedef fast_hmap<LLUUID, LLPointer<LLViewerMediaTexture> > media_map_t;
	static media_map_t	sMediaMap;
};

// Purely static class
class LLViewerTextureManager
{
	LLViewerTextureManager() = delete;
	~LLViewerTextureManager() = delete;

public:
	// Returns NULL if tex is not a LLViewerFetchedTexture nor derived from
	// LLViewerFetchedTexture.
	static LLViewerFetchedTexture* staticCastToFetchedTexture(LLGLTexture* tex,
															  bool report_error = false);

	// "find-texture" just check if the texture exists, if yes, return it,
	// otherwise return null.

	static LLViewerTexture* findTexture(const LLUUID& id);
	static LLViewerMediaTexture* findMediaTexture(const LLUUID& id);

	static LLViewerMediaTexture* createMediaTexture(const LLUUID& id,
													bool usemipmaps = true,
													LLImageGL* gl_image = NULL);

	// "get-texture" will create a new texture if the texture does not exist.

	static LLViewerMediaTexture* getMediaTexture(const LLUUID& id,
												 bool usemipmaps = true,
												 LLImageGL* gl_image = NULL);

	static LLPointer<LLViewerTexture> getLocalTexture(bool usemipmaps = true,
													  bool generate_gl_tex = true);
	static LLPointer<LLViewerTexture> getLocalTexture(const LLUUID& id,
													  bool usemipmaps,
													  bool generate_gl_tex = true);
	static LLPointer<LLViewerTexture> getLocalTexture(const LLImageRaw* raw,
													  bool usemipmaps);
	static LLPointer<LLViewerTexture> getLocalTexture(U32 width, U32 height,
													  U8 components,
													  bool usemipmaps,
													  bool generate_gl_tex = true);

	static LLViewerFetchedTexture* getFetchedTexture(const LLUUID& image_id,
													 FTType f_type = FTT_DEFAULT,
													 bool usemipmap = true,
													 // Get the requested level immediately upon creation.
													 LLGLTexture::EBoostLevel boost_priority = LLGLTexture::BOOST_NONE,
													 S8 texture_type = LLViewerTexture::FETCHED_TEXTURE,
													 S32 internal_format = 0,
													 U32 primary_format = 0,
													 LLHost request_from_host = LLHost());

	static LLViewerFetchedTexture* getFetchedTextureFromFile(const std::string& filename,
															 bool usemipmap = true,
															 LLGLTexture::EBoostLevel boost_priority = LLGLTexture::BOOST_UI,
															 S8 texture_type = LLViewerTexture::FETCHED_TEXTURE,
															 S32 internal_format = 0,
															 U32 primary_format = 0,
															 const LLUUID& force_id = LLUUID::null);

	static LLViewerFetchedTexture* getFetchedTextureFromUrl(const std::string& url,
															FTType f_type,
															bool usemipmap = true,
															LLGLTexture::EBoostLevel boost_priority = LLGLTexture::BOOST_NONE,
															S8 texture_type = LLViewerTexture::FETCHED_TEXTURE,
															S32 internal_format = 0,
															U32 primary_format = 0,
															const LLUUID& force_id = LLUUID::null);

	static LLViewerFetchedTexture* getFetchedTextureFromHost(const LLUUID& image_id,
															 FTType f_type,
															 LLHost host);

	static void init();
	static void cleanup();
};

#endif

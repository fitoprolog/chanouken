/**
 * @file llvertexbuffer.h
 * @brief LLVertexBuffer wrapper for OpengGL vertex buffer objects
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLVERTEXBUFFER_H
#define LL_LLVERTEXBUFFER_H

#if LL_JEMALLOC
# include "jemalloc/jemalloc.h"
# include "hbtracy.h"
#endif

#include <list>
#include <vector>

#include "llcolor4u.h"
#include "llgl.h"
#include "llrefcount.h"
#include "llrender.h"
#include "llstrider.h"

#define LL_MAX_VERTEX_ATTRIB_LOCATION 64

#define LL_USE_DYNAMIC_COPY_VBO_POOL 0

// Forward declarations
LL_INLINE void* allocate_vb_mem(size_t size);
LL_INLINE void free_vb_mem(void* addr) noexcept;

//============================================================================
// NOTES
// Threading:
//  All constructors should take an 'create' paramater which should only be
//  'true' when called from the main thread. Otherwise createGLBuffer() will
//  be called as soon as getVertexPointer(), etc is called (which MUST ONLY be
//  called from the main (i.e OpenGL) thread)

//============================================================================
// GL name pools for dynamic and streaming buffers

class LLVBOPool
{
public:
	LLVBOPool(U32 vbo_usage, U32 vbo_type);

	// Size MUST be a power of 2
	U8* allocate(U32& name, U32 size, U32 seed = 0);

	// Size MUST be the size provided to allocate that returned the given name
	void release(U32 name, U8* buffer, U32 size);

	// Batch allocates buffers to be provided to the application on demand
	void seedPool();

	// Destroys all records in mFreeList
	void cleanup();

	U32 genBuffer();
	void deleteBuffer(U32 name);

	// Batches VBO deletions together in a single call. Must be called at the
	// start of the render loop.
	static void deleteReleasedBuffers();

public:
	const U32					mUsage;
	const U32					mType;

	struct Record
	{
		U32	mGLName;
		U8*	mClientData;
	};

	typedef std::list<Record> record_list_t;
	std::vector<record_list_t>	mFreeList;
	std::vector<U32>			mMissCount;
	// Flag any changes to mFreeList or mMissCount
	bool						mMissCountDirty;

	static U64					sBytesPooled;
	static U64					sIndexBytesPooled;
	static std::vector<U32>		sPendingDeletions;
	// Used to avoid calling glGenBuffers() for every VBO creation
	static U32					sNamePool[1024];
	static U32					sNameIdx;
};

// Base class
class LLVertexBuffer final : public LLRefCount
{
	friend class LLRender;

protected:
	LOG_CLASS(LLVertexBuffer);

public:
#if LL_JEMALLOC
	LL_INLINE void* operator new(size_t size)
	{
		return allocate_vb_mem(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return allocate_vb_mem(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		free_vb_mem(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		free_vb_mem(ptr);
	}
#endif

	LLVertexBuffer(U32 typemask, S32 usage);

	LLVertexBuffer(const LLVertexBuffer& rhs)
	:	mUsage(rhs.mUsage)
	{
		*this = rhs;
	}

	const LLVertexBuffer& operator=(const LLVertexBuffer& rhs)
	{
		llerrs << "Illegal operation!" << llendl;
		return *this;
	}

	static void seedPools();

	static U32 getVAOName();
	static void releaseVAOName(U32 name);

	static void initClass(bool use_vbo, bool no_vbo_mapping);
	static void cleanupClass();
	static void setupClientArrays(U32 data_mask);
	static void drawArrays(U32 mode, const std::vector<LLVector3>& pos);
	static void drawArrays(U32 mode, const std::vector<LLVector3>& pos,
						   const std::vector<LLVector3>& norm);
	static void drawElements(U32 mode, S32 num_vertices, const LLVector4a* pos,
							 const LLVector2* tc, S32 num_indices,
							 const U16* indicesp);

	static void unbind(); // Unbind any bound vertex buffer

	// Gets the size of a vertex with the given typemask
	static S32 calcVertexSize(U32 typemask);

	// Gets the size of a buffer with the given typemask and vertex count,
	// fills offsets with the offset of each vertex component array into the
	// buffer indexed by the following enum.
	static S32 calcOffsets(U32 typemask, S32* offsets, S32 num_vertices);

	// WARNING: when updating these enums you MUST
	// 1 - update LLVertexBuffer::sTypeSize
	// 2 - add a strider accessor
	// 3 - modify LLVertexBuffer::setupVertexBuffer
	// 4 - modify LLVertexBuffer::setupClientArray
	// 5 - modify LLViewerShaderMgr::sReservedAttribs
	// 6 - update LLVertexBuffer::setupVertexArray
	enum {
		TYPE_VERTEX = 0,
		TYPE_NORMAL,
		TYPE_TEXCOORD0,
		TYPE_TEXCOORD1,
		TYPE_TEXCOORD2,
		TYPE_TEXCOORD3,
		TYPE_COLOR,
		TYPE_EMISSIVE,
		TYPE_TANGENT,
		TYPE_WEIGHT,
		TYPE_WEIGHT4,
		TYPE_CLOTHWEIGHT,
		TYPE_TEXTURE_INDEX,
		// TYPE_MAX is the size/boundary marker for attributes that go in the
		// vertex buffer
		TYPE_MAX,
		// TYPE_INDEX is beyond _MAX because it lives in a separate (index)
		// buffer
		TYPE_INDEX,
	};

	enum {
		MAP_VERTEX			= 1 << TYPE_VERTEX,
		MAP_NORMAL			= 1 << TYPE_NORMAL,
		MAP_TEXCOORD0		= 1 << TYPE_TEXCOORD0,
		MAP_TEXCOORD1		= 1 << TYPE_TEXCOORD1,
		MAP_TEXCOORD2		= 1 << TYPE_TEXCOORD2,
		MAP_TEXCOORD3		= 1 << TYPE_TEXCOORD3,
		MAP_COLOR			= 1 << TYPE_COLOR,
		MAP_EMISSIVE		= 1 << TYPE_EMISSIVE,
		// These use VertexAttribPointer and should possibly be made generic
		MAP_TANGENT			= 1 << TYPE_TANGENT,
		MAP_WEIGHT			= 1 << TYPE_WEIGHT,
		MAP_WEIGHT4			= 1 << TYPE_WEIGHT4,
		MAP_CLOTHWEIGHT		= 1 << TYPE_CLOTHWEIGHT,
		MAP_TEXTURE_INDEX	= 1 << TYPE_TEXTURE_INDEX,
	};

	// Maps for data access
	U8* mapVertexBuffer(S32 type, S32 index, S32 count, bool map_range);
	U8* mapIndexBuffer(S32 index, S32 count, bool map_range);

	// Sets for rendering; calls setupVertexBuffer() if data_mask is not 0
	void setBuffer(U32 data_mask);
	// Flush pending data to GL memory
	void flush();

	bool allocateBuffer(S32 nverts, S32 nindices, bool create);
	bool resizeBuffer(S32 newnverts, S32 newnindices);

	// Only call each getVertexPointer, etc, once before calling flush().
	// Call flush() after calls to getXXXStrider() and before any calls to
	// setBuffer(). Example:
	//   vb->getVertexBuffer(verts);
	//   vb->getNormalStrider(norms);
	//   setVertsNorms(verts, norms);
	//   vb->flush();
	bool getVertexStrider(LLStrider<LLVector3>& strider, S32 index = 0,
						  S32 count = -1, bool map_range = false);
	bool getVertexStrider(LLStrider<LLVector4a>& strider, S32 index = 0,
						  S32 count = -1, bool map_range = false);
	bool getIndexStrider(LLStrider<U16>& strider, S32 index = 0,
						 S32 count = -1, bool map_range = false);
	bool getTexCoord0Strider(LLStrider<LLVector2>& strider, S32 index = 0,
							 S32 count = -1, bool map_range = false);
	bool getTexCoord1Strider(LLStrider<LLVector2>& strider, S32 index = 0,
							 S32 count = -1, bool map_range = false);
	bool getTexCoord2Strider(LLStrider<LLVector2>& strider, S32 index = 0,
							 S32 count = -1, bool map_range = false);
	bool getNormalStrider(LLStrider<LLVector3>& strider, S32 index = 0,
						  S32 count = -1, bool map_range = false);
	bool getTangentStrider(LLStrider<LLVector3>& strider, S32 index = 0,
						   S32 count = -1, bool map_range = false);
	bool getTangentStrider(LLStrider<LLVector4a>& strider, S32 index = 0,
						   S32 count = -1, bool map_range = false);
	bool getColorStrider(LLStrider<LLColor4U>& strider, S32 index = 0,
						 S32 count = -1, bool map_range = false);
	bool getTextureIndexStrider(LLStrider<LLColor4U>& strider, S32 index = 0,
								S32 count = -1, bool map_range = false);
	bool getEmissiveStrider(LLStrider<LLColor4U>& strider, S32 index = 0,
							S32 count = -1, bool map_range = false);
	bool getWeightStrider(LLStrider<F32>& strider, S32 index = 0,
						  S32 count = -1, bool map_range = false);
	bool getWeight4Strider(LLStrider<LLVector4a>& strider, S32 index = 0,
						   S32 count = -1, bool map_range = false);
	bool getClothWeightStrider(LLStrider<LLVector4a>& strider, S32 index = 0,
							   S32 count = -1, bool map_range = false);

	LL_INLINE bool useVBOs() const					{ return mUsage != 0; }
	LL_INLINE bool isEmpty() const					{ return mEmpty; }
	LL_INLINE bool isLocked() const					{ return mVertexLocked || mIndexLocked; }
	LL_INLINE S32 getNumVerts() const				{ return mNumVerts; }
	LL_INLINE S32 getNumIndices() const				{ return mNumIndices; }

	LL_INLINE U8* getIndicesPointer() const
	{
		return useVBOs() ? (U8*)mAlignedIndexOffset : mMappedIndexData;
	}

	LL_INLINE U8* getVerticesPointer() const
	{
		return useVBOs() ? (U8*)mAlignedOffset : mMappedData;
	}

	LL_INLINE U32 getTypeMask() const				{ return mTypeMask; }
	LL_INLINE bool hasDataType(S32 type) const		{ return (1 << type) & getTypeMask(); }
	// This method allows to specify a mask that is negated and ANDed to
	// data_mask in setupVertexBuffer(). It allows to avoid using a derived
	// class and virtual method for the latter. HB
	LL_INLINE void setTypeMaskMask(U32 mask)		{ mTypeMaskMask = mask; }

	LL_INLINE S32 getSize() const					{ return mSize; }
	LL_INLINE S32 getIndicesSize() const			{ return mIndicesSize; }
	LL_INLINE U8* getMappedData() const				{ return mMappedData; }
	LL_INLINE U8* getMappedIndices() const			{ return mMappedIndexData; }
	LL_INLINE S32 getOffset(S32 type) const			{ return mOffsets[type]; }
	LL_INLINE S32 getUsage() const					{ return mUsage; }
	LL_INLINE bool isWriteable() const				{ return mMappable || mUsage == GL_STREAM_DRAW_ARB; }

	void draw(U32 mode, U32 count, U32 indices_offset) const;
	void drawArrays(U32 mode, U32 offset, U32 count) const;
	void drawRange(U32 mode, U32 start, U32 end, U32 count,
				   U32 indices_offset) const;

	// For debugging, checks range validity and validates data in given range
	bool validateRange(U32 start, U32 end, U32 count, U32 offset) const;

#if LL_JEMALLOC
	LL_INLINE static U32 getMallocxFlags()			{ return sMallocxFlags; }
#endif

	static std::string listMissingBits(U32 unsatisfied_mask);

	static S32 determineUsage(S32 usage);

protected:
	~LLVertexBuffer() override; // use unref()

	void setupVertexBuffer(U32 data_mask);
	void setupVertexArray();

	void genBuffer(U32 size);
	void genIndices(U32 size);
	bool bindGLBuffer(bool force_bind = false);
	bool bindGLIndices(bool force_bind = false);
	bool bindGLArray();
	void releaseBuffer();
	void releaseIndices();
	bool createGLBuffer(U32 size);
	bool createGLIndices(U32 size);
	void destroyGLBuffer();
	void destroyGLIndices();
	bool updateNumVerts(S32 nverts);
	bool updateNumIndices(S32 nindices);

	void placeFence() const;
	void waitFence() const;

public:
	struct MappedRegion
	{
		LL_INLINE MappedRegion(S32 type, S32 index, S32 count)
		:	mType(type),
			mIndex(index),
			mCount(count),
			mEnd(index + count)
		{
		}

		S32 mType;
		S32 mIndex;
		S32 mCount;
		S32 mEnd;
	};

	static const S32		sTypeSize[TYPE_MAX];
	static const U32		sGLMode[LLRender::NUM_MODES];
	static LLVBOPool		sStreamVBOPool;
	static LLVBOPool		sDynamicVBOPool;
	static LLVBOPool		sStreamIBOPool;
	static LLVBOPool		sDynamicIBOPool;
#if LL_USE_DYNAMIC_COPY_VBO_POOL
	static LLVBOPool		sDynamicCopyVBOPool;
#endif
	static std::list<U32>	sAvailableVAOName;
	static U32				sCurVAOName;
	static U32				sGLRenderBuffer;
	static U32				sGLRenderArray;
	static U32				sGLRenderIndices;
	static U32				sLastMask;
	static U64				sAllocatedBytes;
	static U64				sAllocatedIndexBytes;
	static U32				sVertexCount;
	static U32				sIndexCount;
	static U32				sBindCount;
	static U32				sSetCount;
	static S32				sCount;
	static S32				sGLCount;
	static S32				sMappedCount;
	static bool				sMapped;
	static bool				sDisableVBOMapping;	// Disable glMapBufferARB
	static bool				sEnableVBOs;
	static bool				sVBOActive;
	static bool				sIBOActive;
	static bool				sUseVAO;
	static bool				sUseStreamDraw;
	static bool				sPreferStreamDraw;

protected:
	typedef std::vector<MappedRegion> region_map_t;
	region_map_t			mMappedVertexRegions;
	region_map_t			mMappedIndexRegions;

	S32						mNumVerts;		// Number of vertices allocated
	S32						mNumIndices;	// Number of indices allocated

	ptrdiff_t				mAlignedOffset;
	ptrdiff_t				mAlignedIndexOffset;
	S32						mSize;
	S32						mIndicesSize;
	U32						mTypeMask;
	// This is negated and ANDed to data_mask in setupVertexBuffer(). It allows
	// to avoid using a derived class and virtual method for the latter. HB
	U32						mTypeMaskMask;

	const S32				mUsage;			// GL usage

	U32						mGLBuffer;		// GL VBO handle
	U32						mGLIndices;		// GL IBO handle
	U32						mGLArray;		// GL VAO handle

	S32						mOffsets[TYPE_MAX];

	// Pointer to currently mapped data (NULL if unmapped)
	U8*						mMappedData;
	// Pointer to currently mapped indices (NULL if unmapped)
	U8*						mMappedIndexData;

#if LL_USE_FENCE
	mutable LLGLFence*		mFence;
#endif

	bool					mMappedDataUsingVBOs;
	bool					mMappedIndexDataUsingVBOs;
	// If true, vertex buffer is being or has been written to in client memory
	bool					mVertexLocked;
	// If true, index buffer is being or has been written to in client memory
	bool					mIndexLocked;
	// If true, buffer can not be mapped again
	bool					mFinal;
	// If true, client buffer is empty (or NULL); old values have been
	// discarded.
	bool					mEmpty;

   	// If true, use memory mapping to upload data (otherwise doublebuffer and
	// use glBufferSubData)
	mutable bool			mMappable;

private:
	static LLVertexBuffer*	sUtilityBuffer;
#if LL_JEMALLOC
	static U32				sMallocxFlags;
#endif
};

// NOTE: since the memory functions below use void* pointers instead of char*
// (because void* is the type used by malloc and jemalloc), strict aliasing is
// not possible on structures allocated with them. Make sure you forbid your
// compiler to optimize with strict aliasing assumption (i.e. for gcc, DO use
// the -fno-strict-aliasing option) !

LL_INLINE void* allocate_vb_mem(size_t size)
{
	if (LL_UNLIKELY(size <= 0)) return NULL;

	void* addr;
#if LL_JEMALLOC
	addr = mallocx(size,LLVertexBuffer::getMallocxFlags());
	LL_TRACY_ALLOC(addr, size, trc_mem_vertex);
#else
	addr = ll_aligned_malloc_16(size);
#endif

	if (LL_UNLIKELY(addr == NULL))
	{
		LLMemory::allocationFailed(size);
	}

	return addr;
}

LL_INLINE void free_vb_mem(void* addr) noexcept
{
	if (LL_UNLIKELY(addr == NULL)) return;

#if LL_JEMALLOC
	LL_TRACY_FREE(addr, trc_mem_vertex);
	dallocx(addr, 0);
#else
	ll_aligned_free_16(addr);
#endif
}

// Also used by LLPipeline
U32 nhpo2(U32 v);

// Normally always true, excepted when using vertex buffers with libGLOD in
// LLModelPreview::genLODs()...
extern bool gNoFixedFunction;

#endif // LL_LLVERTEXBUFFER_H

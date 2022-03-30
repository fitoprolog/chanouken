/**
 * @file llvertexbuffer.cpp
 * @brief LLVertexBuffer implementation
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

#include "linden_common.h"

#include "boost/static_assert.hpp"

#include "llvertexbuffer.h"

#include "llglslshader.h"
#include "llshadermgr.h"
#include "llsys.h"
#include "hbtracy.h"

// Define to 1 to enable (but this is slow).
#define TRACE_WITH_TRACY 0

#if !TRACE_WITH_TRACY
# undef LL_TRACY_TIMER
# define LL_TRACY_TIMER(name)
#endif

// Normally always true, excepted when using vertex buffers with libGLOD in
// LLModelPreview::genLODs()...
bool gNoFixedFunction = true;

// Helper functions

// Next Highest Power Of Two: returns first number > v that is a power of 2, or
// v if v is already a power of 2
U32 nhpo2(U32 v)
{
	U32 r = 1;
	while (r < v) r *= 2;
	return r;
}

// Which power of 2 is i ? Assumes i is a power of 2 > 0.
U32 wpo2(U32 i)
{
	llassert(i > 0 && nhpo2(i) == i);
	U32 r = 0;
	while (i >>= 1) ++r;
	return r;
}

constexpr U32 LL_VBO_BLOCK_SIZE = 2048;
constexpr U32 LL_VBO_POOL_MAX_SEED_SIZE = 256 * 1024;

LL_INLINE U32 vbo_block_size(U32 size)
{
	// What block size will fit size ?
	U32 mod = size % LL_VBO_BLOCK_SIZE;
	return mod == 0 ? size : size + LL_VBO_BLOCK_SIZE - mod;
}

LL_INLINE U32 vbo_block_index(U32 size)
{
	U32 blocks = vbo_block_size(size) / LL_VBO_BLOCK_SIZE;
	if (blocks == 0)
	{
		llwarns_once << "VBO block size is too small !" << llendl;
		llassert(false);
		return 0;
	}
	return blocks - 1;
}

const U32 LL_VBO_POOL_SEED_COUNT =
	vbo_block_index(LL_VBO_POOL_MAX_SEED_SIZE) + 1;

///////////////////////////////////////////////////////////////////////////////
// LLVBOPool class
///////////////////////////////////////////////////////////////////////////////

//static
LLVBOPool LLVertexBuffer::sStreamVBOPool(GL_STREAM_DRAW_ARB,
										 GL_ARRAY_BUFFER_ARB);
LLVBOPool LLVertexBuffer::sDynamicVBOPool(GL_DYNAMIC_DRAW_ARB,
										  GL_ARRAY_BUFFER_ARB);
LLVBOPool LLVertexBuffer::sStreamIBOPool(GL_STREAM_DRAW_ARB,
										 GL_ELEMENT_ARRAY_BUFFER_ARB);
LLVBOPool LLVertexBuffer::sDynamicIBOPool(GL_DYNAMIC_DRAW_ARB,
										  GL_ELEMENT_ARRAY_BUFFER_ARB);
#if LL_USE_DYNAMIC_COPY_VBO_POOL
LLVBOPool LLVertexBuffer::sDynamicCopyVBOPool(GL_DYNAMIC_COPY_ARB,
											  GL_ARRAY_BUFFER_ARB);
#endif

U32 LLVBOPool::sNameIdx = 0;
U32 LLVBOPool::sNamePool[1024];
U64 LLVBOPool::sBytesPooled = 0;
U64 LLVBOPool::sIndexBytesPooled = 0;
std::vector<U32> LLVBOPool::sPendingDeletions;

LLVBOPool::LLVBOPool(U32 vbo_usage, U32 vbo_type)
:	mUsage(vbo_usage),
	mType(vbo_type),
	mMissCountDirty(true)
{
	mMissCount.resize(LL_VBO_POOL_SEED_COUNT);
	mFreeList.resize(LL_VBO_POOL_SEED_COUNT);
	std::fill(mMissCount.begin(), mMissCount.end(), 0);
	llinfos << "Created VBO pool for usage: " << vbo_usage << " and type: "
			<< vbo_type << llendl;
}

U32 LLVBOPool::genBuffer()
{
	if (!sNameIdx)
	{
		glGenBuffersARB(1024, sNamePool);
		sNameIdx = 1024;
	}
	return sNamePool[--sNameIdx];
}

void LLVBOPool::deleteBuffer(U32 name)
{
	if (gGLManager.mInited)
	{
		if (name == LLVertexBuffer::sGLRenderBuffer)
		{
			LLVertexBuffer::unbind();
		}
		sPendingDeletions.push_back(name);
	}
}

U8* LLVBOPool::allocate(U32& name, U32 size, U32 seed)
{
	llassert(vbo_block_size(size) == size);

	U8* ret = NULL;

	U32 i = vbo_block_index(size);

	if (mFreeList.size() <= i)
	{
		mFreeList.resize(i + 1);
	}

	if (seed || mFreeList[i].empty())
	{
		// Make a new buffer
		name = seed > 0 ? seed : genBuffer();

		glBindBufferARB(mType, name);

		if (!seed && i < LL_VBO_POOL_SEED_COUNT)
		{
			// Record this miss
			++mMissCount[i];
			mMissCountDirty = true;  // Signal to seedPool()
		}

		if (mType == GL_ARRAY_BUFFER_ARB)
		{
			LLVertexBuffer::sAllocatedBytes += size;
		}
		else
		{
			LLVertexBuffer::sAllocatedIndexBytes += size;
		}

		if (LLVertexBuffer::sDisableVBOMapping ||
			mUsage != GL_DYNAMIC_DRAW_ARB)
		{
			glBufferDataARB(mType, size, NULL, mUsage);
			if (mUsage != GL_DYNAMIC_COPY_ARB)
			{
				// Data will be provided by application
				ret = (U8*)ll_aligned_malloc(size, 64);
			}
		}
#if 0
		else if (mUsage == GL_DYNAMIC_DRAW_ARB)
		{
			glBufferDataARB(mType, size, NULL, GL_DYNAMIC_DRAW_ARB);
		}
#endif
		else
		{
			// Always use a true hint of static draw when allocating non-client
			// backed buffers
			glBufferDataARB(mType, size, NULL, GL_STATIC_DRAW_ARB);
		}

		if (seed)
		{
			// Put into pool for future use
			llassert(mFreeList.size() > i);

			Record rec;
			rec.mGLName = name;
			rec.mClientData = ret;

			if (mType == GL_ARRAY_BUFFER_ARB)
			{
				sBytesPooled += size;
			}
			else
			{
				sIndexBytesPooled += size;
			}
			mFreeList[i].emplace_back(rec);
			mMissCountDirty = true;  // Signal to seedPool()
		}
		else
		{
			glBindBufferARB(mType, 0);
		}
	}
	else
	{
		record_list_t& lst = mFreeList[i];
		const Record& record = lst.front();
		name = record.mGLName;
		ret = record.mClientData;
		lst.pop_front();
		mMissCountDirty = true;  // Signal to seedPool()

		if (mType == GL_ARRAY_BUFFER_ARB)
		{
			sBytesPooled -= size;
		}
		else
		{
			sIndexBytesPooled -= size;
		}
	}

	return ret;
}

void LLVBOPool::release(U32 name, U8* buffer, U32 size)
{
	llassert(vbo_block_size(size) == size);

	deleteBuffer(name);

	if (buffer)
	{
		ll_aligned_free((U8*)buffer);
	}

	if (mType == GL_ARRAY_BUFFER_ARB)
	{
		LLVertexBuffer::sAllocatedBytes -= size;
	}
	else
	{
		LLVertexBuffer::sAllocatedIndexBytes -= size;
	}
}

void LLVBOPool::seedPool()
{
	if (!mMissCountDirty)
	{
		return;
	}
	mMissCountDirty = false;

	U32 dummy_name = 0;

	static std::vector<U32> vb_sizes;
	U32 size = LL_VBO_BLOCK_SIZE;
	for (U32 i = 0; i < LL_VBO_POOL_SEED_COUNT; ++i)
	{
		U32 list_size = mFreeList[i].size();
		U32 miss_count = mMissCount[i];
		if (miss_count > list_size)
		{
			for (U32 j = 0, count = miss_count - list_size; j < count; ++j)
			{
				vb_sizes.push_back(size);
			}
		}
		size += LL_VBO_BLOCK_SIZE;
	}

	U32 count = vb_sizes.size();
	if (count)
	{
		U32* names = new U32[count];
		glGenBuffersARB(count, names);
		for (U32 i = 0; i < count; ++i)
		{
			allocate(dummy_name, vb_sizes[i], names[i]);
		}
		delete[] names;
		glBindBufferARB(mType, 0);
		vb_sizes.clear();
	}
}

void LLVBOPool::deleteReleasedBuffers()
{
	if (!sPendingDeletions.empty())
	{
		glDeleteBuffersARB(sPendingDeletions.size(), sPendingDeletions.data());
		sPendingDeletions.clear();
	}
}

void LLVBOPool::cleanup()
{
	U32 size = LL_VBO_BLOCK_SIZE;

	for (S32 i = 0, count = mFreeList.size(); i < count; ++i)
	{
		record_list_t& l = mFreeList[i];

		while (!l.empty())
		{
			Record& r = l.front();

			deleteBuffer(r.mGLName);

			if (r.mClientData)
			{
				ll_aligned_free((void*)r.mClientData);
			}

			l.pop_front();

			if (mType == GL_ARRAY_BUFFER_ARB)
			{
				sBytesPooled -= size;
				LLVertexBuffer::sAllocatedBytes -= size;
			}
			else
			{
				sIndexBytesPooled -= size;
				LLVertexBuffer::sAllocatedIndexBytes -= size;
			}
		}

		size += LL_VBO_BLOCK_SIZE;
	}

	// Reset miss counts
	std::fill(mMissCount.begin(), mMissCount.end(), 0);
	mMissCountDirty = false;
	deleteReleasedBuffers();
}

///////////////////////////////////////////////////////////////////////////////
// LLVertexBuffer class
///////////////////////////////////////////////////////////////////////////////

std::list<U32> LLVertexBuffer::sAvailableVAOName;
U32 LLVertexBuffer::sCurVAOName = 1;

LLVertexBuffer* LLVertexBuffer::sUtilityBuffer = NULL;

#if LL_JEMALLOC
// Initialize with a sane value, in case our allocator gets called before the
// jemalloc arena for it is set.
U32 LLVertexBuffer::sMallocxFlags = MALLOCX_ALIGN(16) | MALLOCX_TCACHE_NONE;
#endif

U32 LLVertexBuffer::sBindCount = 0;
U32 LLVertexBuffer::sSetCount = 0;
S32 LLVertexBuffer::sCount = 0;
S32 LLVertexBuffer::sGLCount = 0;
S32 LLVertexBuffer::sMappedCount = 0;
bool LLVertexBuffer::sDisableVBOMapping = false;
bool LLVertexBuffer::sEnableVBOs = true;
U32 LLVertexBuffer::sGLRenderBuffer = 0;
U32 LLVertexBuffer::sGLRenderArray = 0;
U32 LLVertexBuffer::sGLRenderIndices = 0;
U32 LLVertexBuffer::sLastMask = 0;
bool LLVertexBuffer::sVBOActive = false;
bool LLVertexBuffer::sIBOActive = false;
U64 LLVertexBuffer::sAllocatedBytes = 0;
U32 LLVertexBuffer::sVertexCount = 0;
U64 LLVertexBuffer::sAllocatedIndexBytes = 0;
U32 LLVertexBuffer::sIndexCount = 0;
bool LLVertexBuffer::sMapped = false;
bool LLVertexBuffer::sUseStreamDraw = true;
bool LLVertexBuffer::sUseVAO = false;
bool LLVertexBuffer::sPreferStreamDraw = false;

// NOTE: each component must be AT LEAST 4 bytes in size to avoid a performance
// penalty on AMD hardware
const S32 LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_MAX] =
{
	sizeof(LLVector4), // TYPE_VERTEX,
	sizeof(LLVector4), // TYPE_NORMAL,
	sizeof(LLVector2), // TYPE_TEXCOORD0,
	sizeof(LLVector2), // TYPE_TEXCOORD1,
	sizeof(LLVector2), // TYPE_TEXCOORD2,
	sizeof(LLVector2), // TYPE_TEXCOORD3,
	sizeof(LLColor4U), // TYPE_COLOR,
	sizeof(LLColor4U), // TYPE_EMISSIVE, only alpha is used currently
	sizeof(LLVector4), // TYPE_TANGENT,
	sizeof(F32),	   // TYPE_WEIGHT,
	sizeof(LLVector4), // TYPE_WEIGHT4,
	sizeof(LLVector4), // TYPE_CLOTHWEIGHT,
	sizeof(LLVector4), // TYPE_TEXTURE_INDEX (actually exists as position.w), no extra data, but stride is 16 bytes
};

static const std::string vb_type_name[] =
{
	"TYPE_VERTEX",
	"TYPE_NORMAL",
	"TYPE_TEXCOORD0",
	"TYPE_TEXCOORD1",
	"TYPE_TEXCOORD2",
	"TYPE_TEXCOORD3",
	"TYPE_COLOR",
	"TYPE_EMISSIVE",
	"TYPE_TANGENT",
	"TYPE_WEIGHT",
	"TYPE_WEIGHT4",
	"TYPE_CLOTHWEIGHT",
	"TYPE_TEXTURE_INDEX",
	"TYPE_MAX",
	"TYPE_INDEX",
};

const U32 LLVertexBuffer::sGLMode[LLRender::NUM_MODES] =
{
	GL_TRIANGLES,
	GL_TRIANGLE_STRIP,
	GL_TRIANGLE_FAN,
	GL_POINTS,
	GL_LINES,
	GL_LINE_STRIP,
	GL_LINE_LOOP,
};

//static
void LLVertexBuffer::initClass(bool use_vbo, bool no_vbo_mapping)
{
	sEnableVBOs = use_vbo && gGLManager.mHasVertexBufferObject;
	if (sEnableVBOs)
	{
		llinfos << "VBO is enabled." << llendl;
	}
	else
	{
		llinfos << "VBO is disabled." << llendl;
	}

	sDisableVBOMapping = !sEnableVBOs || no_vbo_mapping;

	if (sUtilityBuffer)
	{
		delete sUtilityBuffer;
		sUtilityBuffer = NULL;
	}
	if (!sDisableVBOMapping)
	{
		sUtilityBuffer = new LLVertexBuffer(MAP_VERTEX | MAP_NORMAL |
											MAP_TEXCOORD0, GL_STREAM_DRAW);
		if (!sUtilityBuffer->allocateBuffer(65536, 65536, true))
		{
			llwarns << "Failed to allocate the utility buffer" << llendl;
			delete sUtilityBuffer;
			sUtilityBuffer = NULL;
		}
	}

	static bool init_done = false;
	if (init_done)
	{
		return;
	}

#if GL_ARB_vertex_array_object
	llinfos << "GL_ARB_vertex_array_object in use." << llendl;
#endif
#ifdef GL_ARB_map_buffer_range
	llinfos << "GL_ARB_map_buffer_range in use." << llendl;
#endif

#if LL_JEMALLOC
	static unsigned int arena = 0;
	if (!arena)
	{
		size_t sz = sizeof(arena);
		if (mallctl("arenas.create", &arena, &sz, NULL, 0))
		{
			llwarns << "Failed to create a new jemalloc arena" << llendl;
		}
		else
		{
			llinfos << "Using jemalloc arena " << arena
					<< " for vertex buffers memory" << llendl;
			init_done = true;
		}
	}
	sMallocxFlags = MALLOCX_ARENA(arena) | MALLOCX_ALIGN(16) |
					MALLOCX_TCACHE_NONE;
#else
	init_done = true;
#endif
}

//static
void LLVertexBuffer::cleanupClass()
{
	unbind();

	sStreamIBOPool.cleanup();
	sDynamicIBOPool.cleanup();
	sStreamVBOPool.cleanup();
	sDynamicVBOPool.cleanup();
#if LL_USE_DYNAMIC_COPY_VBO_POOL
	sDynamicCopyVBOPool.cleanup();
#endif

	if (sUtilityBuffer)
	{
		delete sUtilityBuffer;
		sUtilityBuffer = NULL;
	}
}

LLVertexBuffer::LLVertexBuffer(U32 typemask, S32 usage)
:	LLRefCount(),
	mNumVerts(0),
	mNumIndices(0),
	mAlignedOffset(0),
	mAlignedIndexOffset(0),
	mSize(0),
	mIndicesSize(0),
	mTypeMask(typemask),
	mTypeMaskMask(0),
	mUsage(determineUsage(usage)),
	mGLBuffer(0),
	mGLIndices(0),
	mGLArray(0),
#if LL_USE_FENCE
	mFence(NULL),
#endif
	mMappedData(NULL),
	mMappedIndexData(NULL),
	mMappedDataUsingVBOs(false),
	mMappedIndexDataUsingVBOs(false),
	mVertexLocked(false),
	mIndexLocked(false),
	mFinal(false),
	mEmpty(true),
	mMappable(false)
{
	mMappable = mUsage == GL_DYNAMIC_DRAW_ARB && !sDisableVBOMapping;

	// Zero out offsets
	for (U32 i = 0; i < TYPE_MAX; ++i)
	{
		mOffsets[i] = 0;
	}

	++sCount;
}

// Protected, use unref()
//virtual
LLVertexBuffer::~LLVertexBuffer()
{
	flush();

	destroyGLBuffer();
	destroyGLIndices();

#if GL_ARB_vertex_array_object
	if (mGLArray)
	{
		releaseVAOName(mGLArray);
		mGLArray = 0;
	}
#endif

	--sCount;

#if LL_USE_FENCE
	if (mFence)
	{
		delete mFence;
		mFence = NULL;
	}
#endif

	sVertexCount -= mNumVerts;
	sIndexCount -= mNumIndices;

	if (mMappedData)
	{
		llerrs << "Failed to clear vertex buffer vertices" << llendl;
	}
	if (mMappedIndexData)
	{
		llerrs << "Failed to clear vertex buffer indices" << llendl;
	}
}

//static
U32 LLVertexBuffer::getVAOName()
{
	U32 ret = 0;

	if (!sAvailableVAOName.empty())
	{
		ret = sAvailableVAOName.front();
		sAvailableVAOName.pop_front();
	}
#ifdef GL_ARB_vertex_array_object
	else
	{
		glGenVertexArrays(1, &ret);
	}
#endif

	return ret;
}

//static
void LLVertexBuffer::releaseVAOName(U32 name)
{
	sAvailableVAOName.push_back(name);
}

//static
void LLVertexBuffer::seedPools()
{
	sStreamVBOPool.seedPool();
	sDynamicVBOPool.seedPool();
#if LL_USE_DYNAMIC_COPY_VBO_POOL
	sDynamicCopyVBOPool.seedPool();
#endif
	sStreamIBOPool.seedPool();
	sDynamicIBOPool.seedPool();
}

//static
void LLVertexBuffer::setupClientArrays(U32 data_mask)
{
	if (sLastMask != data_mask)
	{
#if LL_DARWIN
		if (gGLManager.mGLSLVersionMajor < 2 &&
			gGLManager.mGLSLVersionMinor < 30)
#else
		if (!gGLManager.mHasVertexAttribIPointer)
#endif
		{
			// Make sure texture index is disabled
			data_mask = data_mask & ~MAP_TEXTURE_INDEX;
		}

		if (gNoFixedFunction)
		{
			for (U32 i = 0; i < TYPE_MAX; ++i)
			{
				GLint loc = i;
				U32 mask = 1 << i;

				if (sLastMask & (1 << i))
				{
					// Was enabled
					if (!(data_mask & mask))
					{
						// Needs to be disabled
						glDisableVertexAttribArrayARB(loc);
					}
				}
				else if (data_mask & mask)
				{
					// Was disabled and needs to be enabled
					glEnableVertexAttribArrayARB(loc);
				}
			}
		}
		else
		{
			static const GLenum array[] =
			{
				GL_VERTEX_ARRAY,
				GL_NORMAL_ARRAY,
				GL_TEXTURE_COORD_ARRAY,
				GL_COLOR_ARRAY,
			};

			static const GLenum mask[] =
			{
				MAP_VERTEX,
				MAP_NORMAL,
				MAP_TEXCOORD0,
				MAP_COLOR
			};

			for (U32 i = 0; i < 4; ++i)
			{
				if (sLastMask & mask[i])
				{
					// Was enabled
					if (!(data_mask & mask[i]))
					{
						// Needs to be disabled
						glDisableClientState(array[i]);
					}
					// Needs to be enabled, make sure it was
					else if (gDebugGL && !glIsEnabled(array[i]))
					{
						llwarns << "Bad client state !  " << array[i]
								<< " disabled." << llendl;
					}
				}
				// Was disabled
				else if (data_mask & mask[i])
				{
					// Needs to be enabled
					glEnableClientState(array[i]);
				}
				else if (gDebugGL && glIsEnabled(array[i]))
				{
					// Needs to be disabled, make sure it was
					llwarns << "Bad client state !  " << array[i]
							<< " enabled." << llendl;
				}
			}

			static const U32 map_tc[] =
			{
				MAP_TEXCOORD1,
				MAP_TEXCOORD2,
				MAP_TEXCOORD3
			};

			for (U32 i = 0; i < 3; ++i)
			{
				if (sLastMask & map_tc[i])
				{
					if (!(data_mask & map_tc[i]))
					{
						// Disable
						glClientActiveTextureARB(GL_TEXTURE1_ARB + i);
						glDisableClientState(GL_TEXTURE_COORD_ARRAY);
						glClientActiveTextureARB(GL_TEXTURE0_ARB);
					}
				}
				else if (data_mask & map_tc[i])
				{
					glClientActiveTextureARB(GL_TEXTURE1_ARB + i);
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					glClientActiveTextureARB(GL_TEXTURE0_ARB);
				}
			}

			if (sLastMask & MAP_TANGENT)
			{
				if (!(data_mask & MAP_TANGENT))
				{
					glClientActiveTextureARB(GL_TEXTURE2_ARB);
					glDisableClientState(GL_TEXTURE_COORD_ARRAY);
					glClientActiveTextureARB(GL_TEXTURE0_ARB);
				}
			}
			else if (data_mask & MAP_TANGENT)
			{
				glClientActiveTextureARB(GL_TEXTURE2_ARB);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glClientActiveTextureARB(GL_TEXTURE0_ARB);
			}
		}

		sLastMask = data_mask;
	}
}

// LL's new (fixed) but slow code, and without normals support.
//static
void LLVertexBuffer::drawArrays(U32 mode, const std::vector<LLVector3>& pos)
{
	gGL.begin(mode);
	for (U32 i = 0, count = pos.size(); i < count; ++i)
	{
		gGL.vertex3fv(pos[i].mV);
	}
	gGL.end(true);
}

//static
void LLVertexBuffer::drawArrays(U32 mode, const std::vector<LLVector3>& pos,
								const std::vector<LLVector3>& norm)
{
	U32 count = pos.size();
	if (count == 0)
	{
		return;
	}
	if (sUtilityBuffer && count <= 65536)
	{
		gGL.syncMatrices();

		if (norm.size() < count)
		{
			llwarns_once << "Less normals (" << norm.size()
						 << ") than vertices (" << count
						 << "), aborting." << llendl;
			return;
		}
		// New, vertex-buffer based, optimized code
		LLStrider<LLVector3> vertex_strider;
		LLStrider<LLVector3> normal_strider;
		if (!sUtilityBuffer->getVertexStrider(vertex_strider) ||
			!sUtilityBuffer->getNormalStrider(normal_strider))
		{
			llwarns_once << "Failed to get striders, aborting." << llendl;
			return;
		}
		for (U32 i = 0; i < count; ++i)
		{
			*(vertex_strider++) = pos[i];
			*(normal_strider++) = norm[i];
		}
		sUtilityBuffer->setBuffer(MAP_VERTEX | MAP_NORMAL);
		sUtilityBuffer->drawArrays(mode, 0, pos.size());
	}
	else
	{
		// Fallback to LL's new (fixed) but slow code, and without normals
		// support
		drawArrays(mode, pos);
	}
}

//static
void LLVertexBuffer::drawElements(U32 mode, S32 num_vertices,
								  const LLVector4a* pos, const LLVector2* tc,
								  S32 num_indices, const U16* indicesp)
{
	if (!pos || !indicesp || num_vertices <= 0 || num_indices <= 0)
	{
		llwarns << (pos ? "" : "NULL pos pointer - ")
				<< (indicesp ? "" : "NULL indices pointer - ")
				<< num_vertices << " vertices - " << num_indices
				<< " indices. Aborting." << llendl;
		return;
	}

	gGL.syncMatrices();

	if (sUtilityBuffer && num_vertices <= 65536 && num_indices <= 65536)
	{
		// New, vertex-buffer based, optimized code
		LLStrider<U16> index_strider;
		sUtilityBuffer->getIndexStrider(index_strider);
		S32 index_size = ((num_indices * sizeof(U16)) + 0xF) & ~0xF;
		LLVector4a::memcpyNonAliased16((F32*)index_strider.get(),
									   (F32*)indicesp, index_size);

		LLStrider<LLVector4a> vertex_strider;
		sUtilityBuffer->getVertexStrider(vertex_strider);
		S32 vertex_size = ((num_vertices * 4 * sizeof(F32)) + 0xF) & ~0xF;
		LLVector4a::memcpyNonAliased16((F32*)vertex_strider.get(), (F32*)pos,
									   vertex_size);

		U32 mask = LLVertexBuffer::MAP_VERTEX;
		if (tc)
		{
			mask |= LLVertexBuffer::MAP_TEXCOORD0;
			LLStrider<LLVector2> tc_strider;
			sUtilityBuffer->getTexCoord0Strider(tc_strider);
			S32 tc_size = ((num_vertices * 2 * sizeof(F32)) + 0xF) & ~0xF;
			LLVector4a::memcpyNonAliased16((F32*)tc_strider.get(), (F32*)tc,
										   tc_size);
		}

		sUtilityBuffer->setBuffer(mask);
		sUtilityBuffer->draw(mode, num_indices, 0);
	}
	else	// LL's new (fixed) but slow code
	{
		unbind();

		gGL.begin(mode);

		if (tc)
		{
			for (S32 i = 0; i < num_indices; ++i)
			{
				U16 idx = indicesp[i];
				gGL.texCoord2fv(tc[idx].mV);
				gGL.vertex3fv(pos[idx].getF32ptr());
			}
		}
		else
		{
			for (S32 i = 0; i < num_indices; ++i)
			{
				U16 idx = indicesp[i];
				gGL.vertex3fv(pos[idx].getF32ptr());
			}
		}

		gGL.end(true);
	}
}

bool LLVertexBuffer::validateRange(U32 start, U32 end, U32 count,
								   U32 indices_offset) const
{
	if ((S32)start >= mNumVerts || (S32)end >= mNumVerts)
	{
		llwarns << "Bad vertex buffer draw range: [" << start << ", " << end
				<< "] vs " << mNumVerts << llendl;
		return false;
	}

	llassert(mNumIndices >= 0);

	if ((S32)indices_offset >= mNumIndices ||
	    (S32)(indices_offset + count) > mNumIndices)
	{
		llwarns << "Bad index buffer draw range: [" << indices_offset << ", "
				<< indices_offset + count << "] vs " << mNumIndices << llendl;
		return false;
	}

	if (gDebugGL && !useVBOs())
	{
		U16* idx = ((U16*)getIndicesPointer()) + indices_offset;
		for (U32 i = 0; i < count; ++i)
		{
			if (idx[i] < start || idx[i] > end)
			{
				llwarns << "Index out of range: " << idx[i] << " not in ["
						<< start << ", " << end << "]" << llendl;
				return false;
			}
		}

		LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
		if (shader && shader->mFeatures.mIndexedTextureChannels > 1)
		{
			LLStrider<LLVector4a> v;
			// *HACK: to get non-const reference
			LLVertexBuffer* vb = (LLVertexBuffer*)this;
			vb->getVertexStrider(v);

			for (U32 i = start; i < end; ++i)
			{
				S32 idx = (S32)(v[i][3] + 0.25f);
				if (idx < 0 || idx >= shader->mFeatures.mIndexedTextureChannels)
				{
					llwarns << "Bad texture index found in vertex data stream."
							<< llendl;
					return false;
				}
			}
		}
	}

	return true;
}

void LLVertexBuffer::drawRange(U32 mode, U32 start, U32 end, U32 count,
							   U32 indices_offset) const
{
	if (!validateRange(start, end, count, indices_offset))
	{
		llwarns << "Invalid range. Aborted." << llendl;
		return;
	}

	mMappable = false;
	gGL.syncMatrices();

	llassert(mNumVerts >= 0);
	llassert(!gDebugGL || !gNoFixedFunction ||
			 !LLGLSLShader::sCurBoundShaderPtr);

	if (mGLArray)
	{
		if (mGLArray != sGLRenderArray)
		{
			llerrs << "Wrong vertex array bound." << llendl;
		}
	}
	else
	{
		if (mGLIndices != sGLRenderIndices)
		{
			llerrs << "Wrong index buffer bound." << llendl;
		}
		if (mGLBuffer != sGLRenderBuffer)
		{
			llerrs << "Wrong vertex buffer bound." << llendl;
		}
	}

	if (gDebugGL && !mGLArray && useVBOs())
	{
		GLint elem = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &elem);
		if ((U32)elem != mGLIndices)
		{
			llwarns_once << "Wrong index buffer bound." << llendl;
		}
	}

	if (mode >= LLRender::NUM_MODES)
	{
		llerrs << "Invalid draw mode: " << mode << llendl;
	}

	U16* idx = ((U16*)getIndicesPointer()) + indices_offset;

	stop_glerror();
	LLGLSLShader::startProfile();
	glDrawRangeElements(sGLMode[mode], start, end, count, GL_UNSIGNED_SHORT,
						idx);
	LLGLSLShader::stopProfile(count, mode);
	stop_glerror();

	placeFence();
}

void LLVertexBuffer::draw(U32 mode, U32 count, U32 indices_offset) const
{
	llassert(!gDebugGL || !gNoFixedFunction ||
			 !LLGLSLShader::sCurBoundShaderPtr);

	mMappable = false;
	gGL.syncMatrices();

	llassert(mNumIndices >= 0);
	if ((S32)indices_offset >= mNumIndices ||
	    (S32)(indices_offset + count) > mNumIndices)
	{
		llwarns << "Bad index buffer draw range: [" << indices_offset << ", "
				<< indices_offset + count << "] vs " << mNumIndices
				<< ". Aborted." << llendl;
		return;
	}

	if (mGLArray)
	{
		if (mGLArray != sGLRenderArray)
		{
			llerrs << "Wrong vertex array bound." << llendl;
		}
	}
	else
	{
		if (mGLIndices != sGLRenderIndices)
		{
			llerrs << "Wrong index buffer bound." << llendl;
		}
		if (mGLBuffer != sGLRenderBuffer)
		{
			llerrs << "Wrong vertex buffer bound." << llendl;
		}
	}

	if (mode >= LLRender::NUM_MODES)
	{
		llerrs << "Invalid draw mode: " << mode << llendl;
	}

	stop_glerror();
	LLGLSLShader::startProfile();
	glDrawElements(sGLMode[mode], count, GL_UNSIGNED_SHORT,
				   ((U16*)getIndicesPointer()) + indices_offset);
	LLGLSLShader::stopProfile(count, mode);
	stop_glerror();
	placeFence();
}

void LLVertexBuffer::drawArrays(U32 mode, U32 first, U32 count) const
{
	llassert(!gDebugGL || !LLGLSLShader::sCurBoundShaderPtr);
	mMappable = false;
	gGL.syncMatrices();

	llassert(mNumVerts >= 0);
	if ((S32)first >= mNumVerts || (S32)(first + count) > mNumVerts)
	{
		llwarns << "Bad vertex buffer draw range: [" << first << ", "
				<< first + count << "] vs " << mNumVerts << ". Aborted."
				<< llendl;
		return;
	}

	if (mGLArray)
	{
		if (mGLArray != sGLRenderArray)
		{
			llerrs << "Wrong vertex array bound." << llendl;
		}
	}
	else if (mGLBuffer != sGLRenderBuffer || useVBOs() != sVBOActive)
	{
			llerrs << "Wrong vertex buffer bound." << llendl;
	}

	if (mode >= LLRender::NUM_MODES)
	{
		llerrs << "Invalid draw mode: " << mode << llendl;
	}

	stop_glerror();
	LLGLSLShader::startProfile();
	glDrawArrays(sGLMode[mode], first, count);
	LLGLSLShader::stopProfile(count, mode);
	stop_glerror();
	placeFence();
}

//static
void LLVertexBuffer::unbind()
{
	if (sGLRenderArray)
	{
#if GL_ARB_vertex_array_object
		glBindVertexArray(0);
#endif
		sGLRenderArray = 0;
		sGLRenderIndices = 0;
		sIBOActive = false;
	}

	if (sVBOActive)
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		sVBOActive = false;
	}
	if (sIBOActive)
	{
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		sIBOActive = false;
	}

	sGLRenderBuffer = 0;
	sGLRenderIndices = 0;

	setupClientArrays(0);
}

//static
S32 LLVertexBuffer::determineUsage(S32 usage)
{
	S32 ret_usage = usage;

	if (!sEnableVBOs)
	{
		ret_usage = 0;
	}

	if (ret_usage == GL_STREAM_DRAW_ARB && !sUseStreamDraw)
	{
		ret_usage = 0;
	}

	if (ret_usage == GL_DYNAMIC_DRAW_ARB && sPreferStreamDraw)
	{
		ret_usage = GL_STREAM_DRAW_ARB;
	}

	if (ret_usage == 0 && LLRender::sGLCoreProfile)
	{
		// MUST use VBOs for all rendering
		ret_usage = GL_STREAM_DRAW_ARB;
	}

	if (ret_usage && ret_usage != GL_STREAM_DRAW_ARB &&
		ret_usage != GL_DYNAMIC_COPY_ARB)
	{
		// Only stream_draw and dynamic_draw are supported when using VBOs,
		// dynamic draw is the default
		if (sDisableVBOMapping)
		{
			// Always use stream draw if VBO mapping is disabled
			ret_usage = GL_STREAM_DRAW_ARB;
		}
		else
		{
			ret_usage = GL_DYNAMIC_DRAW_ARB;
		}
	}

	return ret_usage;
}

//static
S32 LLVertexBuffer::calcOffsets(U32 typemask, S32* offsets, S32 num_vertices)
{
	S32 offset = 0;
	for (S32 i = 0; i < TYPE_TEXTURE_INDEX; ++i)
	{
		U32 mask = 1 << i;
		if (typemask & mask)
		{
			if (offsets && sTypeSize[i])
			{
				offsets[i] = offset;
				offset += sTypeSize[i] * num_vertices;
				offset = (offset + 0xF) & ~0xF;
			}
		}
	}

	offsets[TYPE_TEXTURE_INDEX] = offsets[TYPE_VERTEX] + 12;

	return offset + 16;
}

//static
S32 LLVertexBuffer::calcVertexSize(U32 typemask)
{
	S32 size = 0;
	for (S32 i = 0; i < TYPE_TEXTURE_INDEX; ++i)
	{
		U32 mask = 1 << i;
		if (typemask & mask)
		{
			size += sTypeSize[i];
		}
	}

	return size;
}

void LLVertexBuffer::placeFence() const
{
#if LL_USE_FENCE
	if (!mFence && useVBOs())
	{
		if (gGLManager.mHasSync)
		{
			mFence = new LLGLSyncFence();
		}
	}

	if (mFence)
	{
		mFence->placeFence();
	}
#endif
}

void LLVertexBuffer::waitFence() const
{
#if LL_USE_FENCE
	if (mFence)
	{
		mFence->wait();
	}
#endif
}

void LLVertexBuffer::genBuffer(U32 size)
{
	mSize = vbo_block_size(size);

	if (mUsage == GL_STREAM_DRAW_ARB)
	{
		mMappedData = sStreamVBOPool.allocate(mGLBuffer, mSize);
	}
#if LL_USE_DYNAMIC_COPY_VBO_POOL
	else if (mUsage == GL_DYNAMIC_DRAW_ARB)
	{
		mMappedData = sDynamicVBOPool.allocate(mGLBuffer, mSize);
	}
	else
	{
		mMappedData = sDynamicCopyVBOPool.allocate(mGLBuffer, mSize);
	}
#else
	else
	{
		mMappedData = sDynamicVBOPool.allocate(mGLBuffer, mSize);
	}
#endif

	++sGLCount;
}

void LLVertexBuffer::genIndices(U32 size)
{
	mIndicesSize = vbo_block_size(size);

	if (mUsage == GL_STREAM_DRAW_ARB)
	{
		mMappedIndexData = sStreamIBOPool.allocate(mGLIndices, mIndicesSize);
	}
	else
	{
		mMappedIndexData = sDynamicIBOPool.allocate(mGLIndices, mIndicesSize);
	}

	++sGLCount;
}

void LLVertexBuffer::releaseBuffer()
{
	if (mUsage == GL_STREAM_DRAW_ARB)
	{
		sStreamVBOPool.release(mGLBuffer, mMappedData, mSize);
	}
	else
	{
		sDynamicVBOPool.release(mGLBuffer, mMappedData, mSize);
	}

	mGLBuffer = 0;
	mMappedData = NULL;

	--sGLCount;
}

void LLVertexBuffer::releaseIndices()
{
	if (mUsage == GL_STREAM_DRAW_ARB)
	{
		sStreamIBOPool.release(mGLIndices, mMappedIndexData, mIndicesSize);
	}
	else
	{
		sDynamicIBOPool.release(mGLIndices, mMappedIndexData, mIndicesSize);
	}

	mGLIndices = 0;
	mMappedIndexData = NULL;

	--sGLCount;
}

bool LLVertexBuffer::createGLBuffer(U32 size)
{
	if (mGLBuffer)
	{
		destroyGLBuffer();
	}

	if (size == 0)
	{
		return true;
	}

	mEmpty = true;

	mMappedDataUsingVBOs = useVBOs();

	if (mMappedDataUsingVBOs)
	{
		genBuffer(size);
	}
	else
	{
		static int gl_buffer_idx = 0;
		mGLBuffer = ++gl_buffer_idx;
		mMappedData = (U8*)allocate_vb_mem(size);
		mSize = mMappedData ? size : 0;
	}

	return mMappedData != NULL;
}

bool LLVertexBuffer::createGLIndices(U32 size)
{
	if (mGLIndices)
	{
		destroyGLIndices();
	}

	if (size == 0)
	{
		return true;
	}

	mEmpty = true;

	// Pad by 16 bytes for aligned copies
	size += 16;

	mMappedIndexDataUsingVBOs = useVBOs();
	if (mMappedIndexDataUsingVBOs)
	{
		// Pad by another 16 bytes for VBO pointer adjustment
		size += 16;
		genIndices(size);
	}
	else
	{
		mMappedIndexData = (U8*)allocate_vb_mem(size);
		static int gl_buffer_idx = 0;
		mGLIndices = ++gl_buffer_idx;
		mIndicesSize = mMappedIndexData ? size : 0;
	}

	return mMappedIndexData != NULL;
}

void LLVertexBuffer::destroyGLBuffer()
{
	if (mGLBuffer || mMappedData)
	{
		if (mMappedDataUsingVBOs)
		{
			releaseBuffer();
		}
		else if (mMappedData)
		{
			free_vb_mem((void*)mMappedData);
			mMappedData = NULL;
		}
		mEmpty = true;
	}

	mGLBuffer = 0;
#if 0
	unbind();
#endif
}

void LLVertexBuffer::destroyGLIndices()
{
	if (mGLIndices || mMappedIndexData)
	{
		if (mMappedIndexDataUsingVBOs)
		{
			releaseIndices();
		}
		else if (mMappedIndexData)
		{
			free_vb_mem((void*)mMappedIndexData);
			mMappedIndexData = NULL;
		}
		mEmpty = true;
	}

	mGLIndices = 0;
#if 0
	unbind();
#endif
}

bool LLVertexBuffer::updateNumVerts(S32 nverts)
{
	bool success = true;

	llassert(nverts >= 0);

	if (nverts > 65536)
	{
		llwarns << "Vertex buffer overflow !" << llendl;
		nverts = 65536;
	}

	S32 needed_size = calcOffsets(mTypeMask, mOffsets, nverts);
	if (needed_size > mSize || needed_size <= mSize / 2)
	{
		success = createGLBuffer(needed_size);
	}

	sVertexCount -= mNumVerts;
	mNumVerts = nverts;
	sVertexCount += mNumVerts;

	return success;
}

bool LLVertexBuffer::updateNumIndices(S32 nindices)
{
	bool success = true;

	llassert(nindices >= 0);

	S32 needed_size = sizeof(U16) * nindices;
	if (needed_size > mIndicesSize || needed_size <= mIndicesSize / 2)
	{
		success = createGLIndices(needed_size);
	}

	sIndexCount -= mNumIndices;
	mNumIndices = nindices;
	sIndexCount += mNumIndices;

	return success;
}

bool LLVertexBuffer::allocateBuffer(S32 nverts, S32 nindices, bool create)
{
	if (nverts < 0 || nindices < 0 || nverts > 65536)
	{
		llerrs << "Bad vertex buffer allocation: " << nverts << " : "
			   << nindices << llendl;
	}

	bool success = updateNumVerts(nverts);
	success &= updateNumIndices(nindices);

	if (success && create && (nverts || nindices))
	{
		// Actually allocate space for the vertex buffer if using VBO mapping
		flush();

		if (sUseVAO && gGLManager.mHasVertexArrayObject && useVBOs())
		{
#if GL_ARB_vertex_array_object
			mGLArray = getVAOName();
#endif
			setupVertexArray();
		}
	}

	return success;
}

void LLVertexBuffer::setupVertexArray()
{
	if (!mGLArray)
	{
		return;
	}

	LL_TRACY_TIMER(TRC_SETUP_VERTEX_ARRAY);

#if GL_ARB_vertex_array_object
	glBindVertexArray(mGLArray);
#endif
	sGLRenderArray = mGLArray;

	static const U32 attrib_size[] =
	{
		3, // TYPE_VERTEX,
		3, // TYPE_NORMAL,
		2, // TYPE_TEXCOORD0,
		2, // TYPE_TEXCOORD1,
		2, // TYPE_TEXCOORD2,
		2, // TYPE_TEXCOORD3,
		4, // TYPE_COLOR,
		4, // TYPE_EMISSIVE,
		4, // TYPE_TANGENT,
		1, // TYPE_WEIGHT,
		4, // TYPE_WEIGHT4,
		4, // TYPE_CLOTHWEIGHT,
		1, // TYPE_TEXTURE_INDEX
	};

	static const U32 attrib_type[] =
	{
		GL_FLOAT,			// TYPE_VERTEX,
		GL_FLOAT,			// TYPE_NORMAL,
		GL_FLOAT,			// TYPE_TEXCOORD0,
		GL_FLOAT,			// TYPE_TEXCOORD1,
		GL_FLOAT,			// TYPE_TEXCOORD2,
		GL_FLOAT,			// TYPE_TEXCOORD3,
		GL_UNSIGNED_BYTE,	// TYPE_COLOR,
		GL_UNSIGNED_BYTE,	// TYPE_EMISSIVE,
		GL_FLOAT,			// TYPE_TANGENT,
		GL_FLOAT,			// TYPE_WEIGHT,
		GL_FLOAT,			// TYPE_WEIGHT4,
		GL_FLOAT,			// TYPE_CLOTHWEIGHT,
		GL_UNSIGNED_INT,	// TYPE_TEXTURE_INDEX
	};

	static const bool attrib_integer[] =
	{
		false,	// TYPE_VERTEX,
		false,	// TYPE_NORMAL,
		false,	// TYPE_TEXCOORD0,
		false,	// TYPE_TEXCOORD1,
		false,	// TYPE_TEXCOORD2,
		false,	// TYPE_TEXCOORD3,
		false,	// TYPE_COLOR,
		false,	// TYPE_EMISSIVE,
		false,	// TYPE_TANGENT,
		false,	// TYPE_WEIGHT,
		false,	// TYPE_WEIGHT4,
		false,	// TYPE_CLOTHWEIGHT,
		true,	// TYPE_TEXTURE_INDEX
	};

	static const U32 attrib_normalized[] =
	{
		GL_FALSE,	// TYPE_VERTEX,
		GL_FALSE,	// TYPE_NORMAL,
		GL_FALSE,	// TYPE_TEXCOORD0,
		GL_FALSE,	// TYPE_TEXCOORD1,
		GL_FALSE,	// TYPE_TEXCOORD2,
		GL_FALSE,	// TYPE_TEXCOORD3,
		GL_TRUE,	// TYPE_COLOR,
		GL_TRUE,	// TYPE_EMISSIVE,
		GL_FALSE,	// TYPE_TANGENT,
		GL_FALSE,	// TYPE_WEIGHT,
		GL_FALSE,	// TYPE_WEIGHT4,
		GL_FALSE,	// TYPE_CLOTHWEIGHT,
		GL_FALSE,	// TYPE_TEXTURE_INDEX
	};

	bindGLBuffer(true);
	bindGLIndices(true);

	for (U32 i = 0; i < TYPE_MAX; ++i)
	{
		if (mTypeMask & (1 << i))
		{
			glEnableVertexAttribArrayARB(i);

			if (attrib_integer[i])
			{
#if !LL_DARWIN
				if (gGLManager.mHasVertexAttribIPointer)
				{
					glVertexAttribIPointer(i, attrib_size[i], attrib_type[i],
										   sTypeSize[i],
										   (void*)(ptrdiff_t)mOffsets[i]);
				}
#endif
			}
			else
			{
				glVertexAttribPointerARB(i, attrib_size[i], attrib_type[i],
										 attrib_normalized[i], sTypeSize[i],
										 (void*)(ptrdiff_t)mOffsets[i]);
			}
		}
		else
		{
			glDisableVertexAttribArrayARB(i);
		}
	}

#if 0
	// Draw a dummy triangle to set index array pointer
	glDrawElements(GL_TRIANGLES, 0, GL_UNSIGNED_SHORT, NULL);
#endif

	unbind();
}

bool LLVertexBuffer::resizeBuffer(S32 newnverts, S32 newnindices)
{
	llassert(newnverts >= 0 && newnindices >= 0);

	bool success = updateNumVerts(newnverts);
	success &= updateNumIndices(newnindices);

	if (success && useVBOs())
	{
		flush();

		if (mGLArray)
		{
			// If size changed, offsets changed
			setupVertexArray();
		}
	}

	return success;
}

bool expand_region(LLVertexBuffer::MappedRegion& region, S32 index, S32 count)
{
	S32 end = index + count;
	S32 region_end = region.mIndex + region.mCount;

	if (end < region.mIndex || index > region_end)
	{
		// Gap exists, do not merge
		return false;
	}

	S32 new_end = llmax(end, region_end);
	S32 new_index = llmin(index, region.mIndex);
	region.mIndex = new_index;
	region.mCount = new_end - new_index;
	return true;
}

// Map for data access
U8* LLVertexBuffer::mapVertexBuffer(S32 type, S32 index, S32 count,
									bool map_range)
{
	bindGLBuffer(true);

	if (mFinal)
	{
		llerrs << "Call done on a finalized buffer !" << llendl;
	}
	if (!useVBOs() && !mMappedData && !mMappedIndexData)
	{
		llerrs << "Call done on unallocated buffer." << llendl;
	}

	if (useVBOs())
	{
		if (!mMappable || gGLManager.mHasMapBufferRange ||
			gGLManager.mHasFlushBufferRange)
		{
			if (count == -1)
			{
				count = mNumVerts - index;
			}

			bool mapped = false;
			// See if range is already mapped
			for (U32 i = 0; i < mMappedVertexRegions.size(); ++i)
			{
				MappedRegion& region = mMappedVertexRegions[i];
				if (region.mType == type)
				{
					if (expand_region(region, index, count))
					{
						mapped = true;
						break;
					}
				}
			}

			if (!mapped)
			{
				// Not already mapped, map new region
				mMappedVertexRegions.emplace_back(type,
												  mMappable && map_range ? -1
																		 : index,
												  count);
			}
		}

		if (mVertexLocked && map_range)
		{
			llerrs << "Attempted to map a specific range of a buffer that was already mapped."
				   << llendl;
		}

		if (!mVertexLocked)
		{
			mVertexLocked = true;
			++sMappedCount;
			stop_glerror();

			if (!mMappable)
			{
				map_range = false;
			}
			else
			{
				U8* src = NULL;
				waitFence();
				if (gGLManager.mHasMapBufferRange)
				{
#ifdef GL_ARB_map_buffer_range
					if (map_range)
					{
						LL_TRACY_TIMER(TRC_VBO_MAP_BUFFER_RANGE);
						S32 offset = mOffsets[type] + sTypeSize[type] * index;
						S32 length = (sTypeSize[type] * count + 0xF) & ~0xF;
						src = (U8*)glMapBufferRange(GL_ARRAY_BUFFER_ARB,
													offset, length,
													GL_MAP_WRITE_BIT |
													GL_MAP_FLUSH_EXPLICIT_BIT |
													GL_MAP_INVALIDATE_RANGE_BIT);
					}
					else
					{

						if (gDebugGL)
						{
							GLint size = 0;
							glGetBufferParameterivARB(GL_ARRAY_BUFFER_ARB,
													  GL_BUFFER_SIZE_ARB,
													  &size);
							if (size < mSize)
							{
								llwarns_once << "Invalid buffer size: " << size
											 << " - Expected minimum: "
											 << mSize << llendl;
							}
						}

						LL_TRACY_TIMER(TRC_VBO_MAP_BUFFER);
						src = (U8*)glMapBufferRange(GL_ARRAY_BUFFER_ARB,
													0, mSize,
													GL_MAP_WRITE_BIT |
													GL_MAP_FLUSH_EXPLICIT_BIT);
					}
#endif
				}
				else if (gGLManager.mHasFlushBufferRange)
				{
					if (map_range)
					{
						glBufferParameteriAPPLE(GL_ARRAY_BUFFER_ARB,
												GL_BUFFER_SERIALIZED_MODIFY_APPLE,
												GL_FALSE);
						glBufferParameteriAPPLE(GL_ARRAY_BUFFER_ARB,
												GL_BUFFER_FLUSHING_UNMAP_APPLE,
												GL_FALSE);
						src = (U8*)glMapBufferARB(GL_ARRAY_BUFFER_ARB,
												  GL_WRITE_ONLY_ARB);
					}
					else
					{
						src = (U8*)glMapBufferARB(GL_ARRAY_BUFFER_ARB,
												  GL_WRITE_ONLY_ARB);
					}
				}
				else
				{
					map_range = false;
					src = (U8*)glMapBufferARB(GL_ARRAY_BUFFER_ARB,
											  GL_WRITE_ONLY_ARB);
				}

				llassert(src != NULL);

				mMappedData = LL_NEXT_ALIGNED_ADDRESS<U8>(src);
				mAlignedOffset = mMappedData - src;

				stop_glerror();
			}

			if (!mMappedData)
			{
				// Check the availability of memory
				LLMemory::logMemoryInfo();

				if (mMappable)
				{
					stop_glerror();
					//--------------------
					// Print out more debug info before crash
					llinfos << "Vertex buffer size: (num verts : num indices) = "
							<< getNumVerts() << " : " << getNumIndices()
							<< llendl;
					GLint size;
					glGetBufferParameterivARB(GL_ARRAY_BUFFER_ARB,
											  GL_BUFFER_SIZE_ARB, &size);
					llinfos << "GL_ARRAY_BUFFER_ARB size is " << size
							<< llendl;
					//--------------------

					GLint buff;
					glGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &buff);
					if ((U32)buff != mGLBuffer)
					{
						llerrs << "Invalid GL vertex buffer bound: " << buff
							   << llendl;
					}

					llerrs << "glMapBuffer returned NULL (no vertex data)"
						   << llendl;
					stop_glerror();
				}
				else
				{
					LLMemory::allocationFailed();
					llwarns << "Memory allocation for Vertex data failed. Ignoring, but do expect a crash soon..."
							<< llendl;
					llassert(false);
				}
			}
		}
	}
	else
	{
		map_range = false;
	}

	if (map_range && gGLManager.mHasMapBufferRange && mMappable)
	{
		return mMappedData;
	}

	return mMappedData + mOffsets[type] + sTypeSize[type] * index;
}

U8* LLVertexBuffer::mapIndexBuffer(S32 index, S32 count, bool map_range)
{
	bindGLIndices(true);
	if (mFinal)
	{
		llerrs << "Call done on a finalized buffer." << llendl;
	}
	if (!useVBOs() && !mMappedData && !mMappedIndexData)
	{
		llerrs << "Call done on unallocated buffer." << llendl;
	}

	if (useVBOs())
	{
		if (!mMappable || gGLManager.mHasMapBufferRange ||
			gGLManager.mHasFlushBufferRange)
		{
			if (count == -1)
			{
				count = mNumIndices - index;
			}

			bool mapped = false;
			// See if range is already mapped
			for (U32 i = 0; i < mMappedIndexRegions.size(); ++i)
			{
				MappedRegion& region = mMappedIndexRegions[i];
				if (expand_region(region, index, count))
				{
					mapped = true;
					break;
				}
			}

			if (!mapped)
			{
				// Not already mapped, map new region
				mMappedIndexRegions.emplace_back(TYPE_INDEX,
												 mMappable && map_range ? -1
																	    : index,
												 count);
			}
		}

		if (mIndexLocked && map_range)
		{
			llerrs << "Attempted to map a specific range of a buffer that was already mapped."
				   << llendl;
		}

		if (!mIndexLocked)
		{
			mIndexLocked = true;
			++sMappedCount;

			if (gDebugGL && useVBOs())
			{
				GLint elem = 0;
				glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &elem);
				if ((U32)elem != mGLIndices)
				{
					llwarns_once << "Wrong index buffer bound !" << llendl;
				}
			}

			if (!mMappable)
			{
				map_range = false;
			}
			else
			{
				stop_glerror();
				U8* src = NULL;
				waitFence();
				if (gGLManager.mHasMapBufferRange)
				{
					if (map_range)
					{
#ifdef GL_ARB_map_buffer_range
						LL_TRACY_TIMER(TRC_VBO_MAP_INDEX_RANGE);
						S32 offset = sizeof(U16) * index;
						S32 length = sizeof(U16) * count;
						src = (U8*)glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER_ARB,
													offset, length,
													GL_MAP_WRITE_BIT |
													GL_MAP_FLUSH_EXPLICIT_BIT |
													GL_MAP_INVALIDATE_RANGE_BIT);
					}
					else
					{
						LL_TRACY_TIMER(TRC_VBO_MAP_INDEX);
						src = (U8*)glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER_ARB,
													0,
													sizeof(U16) * mNumIndices,
													GL_MAP_WRITE_BIT |
													GL_MAP_FLUSH_EXPLICIT_BIT);
#endif
					}
				}
				else if (gGLManager.mHasFlushBufferRange)
				{
					if (map_range)
					{
						glBufferParameteriAPPLE(GL_ELEMENT_ARRAY_BUFFER_ARB,
												GL_BUFFER_SERIALIZED_MODIFY_APPLE,
												GL_FALSE);
						glBufferParameteriAPPLE(GL_ELEMENT_ARRAY_BUFFER_ARB,
												GL_BUFFER_FLUSHING_UNMAP_APPLE,
												GL_FALSE);
						src = (U8*)glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
												  GL_WRITE_ONLY_ARB);
					}
					else
					{
						src = (U8*)glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
												  GL_WRITE_ONLY_ARB);
					}
				}
				else
				{
					LL_TRACY_TIMER(TRC_VBO_MAP_INDEX);
					map_range = false;
					src = (U8*)glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
											  GL_WRITE_ONLY_ARB);
				}

				llassert(src != NULL);
				mMappedIndexData = src; //LL_NEXT_ALIGNED_ADDRESS<U8>(src);
				mAlignedIndexOffset = mMappedIndexData - src;
				stop_glerror();
			}
		}

		if (!mMappedIndexData)
		{
			stop_glerror();
			LLMemory::logMemoryInfo();

			if (mMappable)
			{
				GLint buff;
				glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &buff);
				if ((U32)buff != mGLIndices)
				{
					llerrs << "Invalid GL index buffer bound: " << buff
						   << llendl;
				}

				llerrs << "glMapBuffer returned NULL (no index data)"
					   << llendl;
			}
			else
			{
				LLMemory::allocationFailed();
				llwarns << "Memory allocation for Index data failed. Ignoring, but do expect a crash soon..."
						<< llendl;
				llassert(false);
			}
		}
	}
	else
	{
		map_range = false;
	}

	if (map_range && gGLManager.mHasMapBufferRange && mMappable)
	{
		return mMappedIndexData;
	}

	return mMappedIndexData + sizeof(U16) * index;
}

void LLVertexBuffer::flush()
{
	if (!useVBOs())
	{
		return; // Nothing to unmap
	}

	bool updated_all = false;

	if (mMappedData && mVertexLocked)
	{
		LL_TRACY_TIMER(TRC_VBO_UNMAP);
		bindGLBuffer(true);
		// Both vertex and index buffers done updating
		updated_all = mIndexLocked;

		if (mMappable)
		{
			if ((gGLManager.mHasMapBufferRange ||
				 gGLManager.mHasFlushBufferRange) &&
				!mMappedVertexRegions.empty())
			{
				stop_glerror();
				for (U32 i = 0; i < mMappedVertexRegions.size(); ++i)
				{
					const MappedRegion& region = mMappedVertexRegions[i];
					S32 offset;
					if (region.mIndex >= 0)
					{
						offset = mOffsets[region.mType] +
								 sTypeSize[region.mType] * region.mIndex;
					}
					else
					{
						offset = 0;
					}
					S32 length = sTypeSize[region.mType] * region.mCount;
					if (gGLManager.mHasMapBufferRange)
					{
						LL_TRACY_TIMER(TRC_VBO_FLUSH_RANGE);
#ifdef GL_ARB_map_buffer_range
						glFlushMappedBufferRange(GL_ARRAY_BUFFER_ARB, offset,
												 length);
#endif
					}
					else if (gGLManager.mHasFlushBufferRange)
					{
						glFlushMappedBufferRangeAPPLE(GL_ARRAY_BUFFER_ARB,
													  offset, length);
					}
					stop_glerror();
				}

				mMappedVertexRegions.clear();
			}

			glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
			stop_glerror();

			mMappedData = NULL;
		}
		else if (mMappedVertexRegions.empty())
		{
			stop_glerror();
			if (gClearARBbuffer)
			{
				glBufferDataARB(GL_ARRAY_BUFFER_ARB, getSize(), NULL, mUsage);
			}
			glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, getSize(),
							   (U8*)mMappedData);
			stop_glerror();
		}
		else
		{
			stop_glerror();
			for (U32 i = 0; i < mMappedVertexRegions.size(); ++i)
			{
				const MappedRegion& region = mMappedVertexRegions[i];
				S32 offset = 0;
				if (region.mIndex >= 0)
				{
					offset = mOffsets[region.mType] +
							 sTypeSize[region.mType] * region.mIndex;
				}
				S32 length = sTypeSize[region.mType] * region.mCount;
				if (mSize >= length + offset)
				{
					glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, offset, length,
									   (U8*)mMappedData + offset);
				}
				else
				{
					GLint size = 0;
					glGetBufferParameterivARB(GL_ARRAY_BUFFER_ARB,
											  GL_BUFFER_SIZE_ARB, &size);
					llwarns << "Attempted to unmap regions to a buffer that is too small. - mapped size = "
							<< mSize << " - buffer size = " << size
							<< " - length = " << length << " - offset = "
							<< offset << llendl;
				}
				stop_glerror();
			}

			mMappedVertexRegions.clear();
		}

		mVertexLocked = false;
		--sMappedCount;
	}

	if (mMappedIndexData && mIndexLocked)
	{
		LL_TRACY_TIMER(TRC_IBO_UNMAP);
		bindGLIndices();
		if (mMappable)
		{
			if (gGLManager.mHasMapBufferRange ||
				gGLManager.mHasFlushBufferRange)
			{
				if (!mMappedIndexRegions.empty())
				{
					stop_glerror();
					for (U32 i = 0; i < mMappedIndexRegions.size(); ++i)
					{
						const MappedRegion& region = mMappedIndexRegions[i];
						S32 offset =
							region.mIndex > 0 ? sizeof(U16) * region.mIndex : 0;
						S32 length = sizeof(U16) * region.mCount;
						if (gGLManager.mHasMapBufferRange)
						{
							LL_TRACY_TIMER(TRC_IBO_FLUSH_RANGE);
#ifdef GL_ARB_map_buffer_range
							glFlushMappedBufferRange(GL_ELEMENT_ARRAY_BUFFER_ARB,
													 offset, length);
#endif
						}
						else if (gGLManager.mHasFlushBufferRange)
						{
#if defined(GL_APPLE_flush_buffer_range)
							glFlushMappedBufferRangeAPPLE(GL_ELEMENT_ARRAY_BUFFER_ARB,
														  offset, length);
#endif
						}
						stop_glerror();
					}

					mMappedIndexRegions.clear();
				}
			}

			glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
			stop_glerror();

			mMappedIndexData = NULL;
		}
		else if (mMappedIndexRegions.empty())
		{
			stop_glerror();
			if (gClearARBbuffer)
			{
				glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
								getIndicesSize(), NULL, mUsage);
			}
			glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0,
							   getIndicesSize(), (U8*)mMappedIndexData);
			stop_glerror();
		}
		else
		{
			for (U32 i = 0; i < mMappedIndexRegions.size(); ++i)
			{
				const MappedRegion& region = mMappedIndexRegions[i];
				S32 offset = 0;
				if (region.mIndex > 0)
				{
					offset = sizeof(U16) * region.mIndex;
				}
				S32 length = sizeof(U16) * region.mCount;
				if (mIndicesSize >= length + offset)
				{
					glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, offset,
									   length, (U8*)mMappedIndexData + offset);
				}
				else
				{
					GLint size = 0;
					glGetBufferParameterivARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
											  GL_BUFFER_SIZE_ARB, &size);
					llwarns << "Attempted to unmap regions to a buffer that is too small. - mapped size = "
							<< mIndicesSize << " - buffer size = " << size
							<< " - length = " << length << " - offset = "
							<< offset << llendl;
				}
				stop_glerror();
			}

			mMappedIndexRegions.clear();
		}

		mIndexLocked = false;
		--sMappedCount;
	}

	if (updated_all)
	{
		mEmpty = false;
	}
}

template <class T, S32 type>
struct VertexBufferStrider
{
	typedef LLStrider<T> strider_t;
	static bool get(LLVertexBuffer& vbo, strider_t& strider,
					S32 index, S32 count, bool map_range)
	{
		if (type == LLVertexBuffer::TYPE_INDEX)
		{
			U8* ptr = vbo.mapIndexBuffer(index, count, map_range);
			strider = (T*)ptr;
			if (ptr == NULL)
			{
				llwarns << "mapIndexBuffer() failed !" << llendl;
				return false;
			}
			strider.setStride(0);
			return true;
		}
		else if (vbo.hasDataType(type))
		{
			S32 stride = LLVertexBuffer::sTypeSize[type];

			U8* ptr = vbo.mapVertexBuffer(type, index, count, map_range);
			strider = (T*)ptr;
			if (ptr == NULL)
			{
				llwarns << "mapVertexBuffer() failed !" << llendl;
				return false;
			}
			strider.setStride(stride);
			return true;
		}
		else
		{
			llerrs << "VertexBufferStrider could not find valid vertex data."
				   << llendl;
		}
		return false;
	}
};

bool LLVertexBuffer::getVertexStrider(LLStrider<LLVector3>& strider,
									  S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector3, TYPE_VERTEX>::get(*this, strider,
															 index, count,
															 map_range);
}

bool LLVertexBuffer::getVertexStrider(LLStrider<LLVector4a>& strider,
									  S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector4a, TYPE_VERTEX>::get(*this, strider,
															 index, count,
															 map_range);
}

bool LLVertexBuffer::getIndexStrider(LLStrider<U16>& strider,
									 S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<U16, TYPE_INDEX>::get(*this, strider, index,
													 count, map_range);
}

bool LLVertexBuffer::getTexCoord0Strider(LLStrider<LLVector2>& strider,
										 S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector2, TYPE_TEXCOORD0>::get(*this, strider,
															   index, count,
															   map_range);
}

bool LLVertexBuffer::getTexCoord1Strider(LLStrider<LLVector2>& strider,
										 S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector2, TYPE_TEXCOORD1>::get(*this, strider,
															   index, count,
															   map_range);
}

bool LLVertexBuffer::getTexCoord2Strider(LLStrider<LLVector2>& strider,
										 S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector2, TYPE_TEXCOORD2>::get(*this, strider,
															   index, count,
															   map_range);
}

bool LLVertexBuffer::getNormalStrider(LLStrider<LLVector3>& strider,
									  S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector3, TYPE_NORMAL>::get(*this, strider,
															index, count,
															map_range);
}

bool LLVertexBuffer::getTangentStrider(LLStrider<LLVector3>& strider,
									   S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector3, TYPE_TANGENT>::get(*this, strider,
															 index, count,
															 map_range);
}

bool LLVertexBuffer::getTangentStrider(LLStrider<LLVector4a>& strider,
									   S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector4a, TYPE_TANGENT>::get(*this, strider,
															  index, count,
															  map_range);
}

bool LLVertexBuffer::getColorStrider(LLStrider<LLColor4U>& strider,
									 S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLColor4U, TYPE_COLOR>::get(*this, strider,
														   index, count,
														   map_range);
}

bool LLVertexBuffer::getEmissiveStrider(LLStrider<LLColor4U>& strider,
										S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLColor4U, TYPE_EMISSIVE>::get(*this, strider,
															  index, count,
															  map_range);
}

bool LLVertexBuffer::getWeightStrider(LLStrider<F32>& strider,
									  S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<F32, TYPE_WEIGHT>::get(*this, strider, index,
													  count, map_range);
}

bool LLVertexBuffer::getWeight4Strider(LLStrider<LLVector4a>& strider,
									   S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector4a, TYPE_WEIGHT4>::get(*this, strider,
															  index, count,
															  map_range);
}

bool LLVertexBuffer::getClothWeightStrider(LLStrider<LLVector4a>& strider,
										   S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector4a, TYPE_CLOTHWEIGHT>::get(*this,
																  strider,
																  index, count,
																  map_range);
}

bool LLVertexBuffer::bindGLArray()
{
	if (mGLArray && sGLRenderArray != mGLArray)
	{
		{
			LL_TRACY_TIMER(TRC_BIND_GL_ARRAY);
#if GL_ARB_vertex_array_object
			glBindVertexArray(mGLArray);
#endif
			sGLRenderArray = mGLArray;
		}

		// Really shouldn't be necessary, but some drivers do not properly
		// restore the state of GL_ELEMENT_ARRAY_BUFFER_BINDING
		bindGLIndices();

		return true;
	}

	return false;
}

bool LLVertexBuffer::bindGLBuffer(bool force_bind)
{
	bindGLArray();

	if (useVBOs() &&
		(force_bind ||
		 (mGLBuffer && (mGLBuffer != sGLRenderBuffer || !sVBOActive))))
	{
		LL_TRACY_TIMER(TRC_BIND_GL_BUFFER);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, mGLBuffer);
		sGLRenderBuffer = mGLBuffer;
		++sBindCount;
		sVBOActive = true;

		llassert(!mGLArray || sGLRenderArray == mGLArray);

		return true;
	}

	return false;
}

bool LLVertexBuffer::bindGLIndices(bool force_bind)
{
	bindGLArray();

	if (useVBOs() &&
		(force_bind ||
		 (mGLIndices && (mGLIndices != sGLRenderIndices || !sIBOActive))))
	{
		LL_TRACY_TIMER(TRC_BIND_GL_INDICES);
#if 0
		if (sMapped)
		{
			llerrs << "VBO bound while another VBO mapped !" << llendl;
		}
#endif
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, mGLIndices);
		sGLRenderIndices = mGLIndices;
		stop_glerror();
		++sBindCount;
		sIBOActive = true;
		return true;
	}

	return false;
}

//static
std::string LLVertexBuffer::listMissingBits(U32 unsatisfied_mask)
{
	std::string report;
	if (unsatisfied_mask & MAP_VERTEX)
	{
		report = "\n - Missing vert pos";
	}
	if (unsatisfied_mask & MAP_NORMAL)
	{
		report += "\n - Missing normals";
	}
	if (unsatisfied_mask & MAP_TEXCOORD0)
	{
		report += "\n - Missing tex coord 0";
	}
	if (unsatisfied_mask & MAP_TEXCOORD1)
	{
		report += "\n - Missing tex coord 1";
	}
	if (unsatisfied_mask & MAP_TEXCOORD2)
	{
		report += "\n - Missing tex coord 2";
	}
	if (unsatisfied_mask & MAP_TEXCOORD3)
	{
		report += "\n - Missing tex coord 3";
	}
	if (unsatisfied_mask & MAP_COLOR)
	{
		report += "\n - Missing vert color";
	}
	if (unsatisfied_mask & MAP_EMISSIVE)
	{
		report += "\n - Missing emissive";
	}
	if (unsatisfied_mask & MAP_TANGENT)
	{
		report += "\n - Missing tangent";
	}
	if (unsatisfied_mask & MAP_WEIGHT)
	{
		report += "\n - Missing weight";
	}
	if (unsatisfied_mask & MAP_WEIGHT4)
	{
		report += "\n - Missing weight4";
	}
	if (unsatisfied_mask & MAP_CLOTHWEIGHT)
	{
		report += "\n - Missing cloth weight";
	}
	if (unsatisfied_mask & MAP_TEXTURE_INDEX)
	{
		report += "\n - Missing tex index";
	}
	if (unsatisfied_mask & TYPE_INDEX)
	{
		report += "\n - Missing indices";
	}
	return report;
}

// Set for rendering
void LLVertexBuffer::setBuffer(U32 data_mask)
{
	flush();

	// Set up pointers if the data mask is different ...
	bool setup = sLastMask != data_mask;

	if (data_mask && gDebugGL)
	{
		// Make sure data requirements are fulfilled
		LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
		if (shader)
		{
			U32 required_mask = 0;
			for (U32 i = 0; i < LLVertexBuffer::TYPE_TEXTURE_INDEX; ++i)
			{
				if (shader->getAttribLocation(i) > -1)
				{
					U32 required = 1 << i;
					if ((data_mask & required) == 0)
					{
						llwarns << "Missing attribute: "
								<< LLShaderMgr::sReservedAttribs[i] << llendl;
					}

					required_mask |= required;
				}
			}

			U32 unsatisfied_mask = required_mask & ~data_mask;
			if (unsatisfied_mask)
			{
				llwarns << "Shader consumption mismatches data provision:"
						<< listMissingBits(unsatisfied_mask) << llendl;
			}
		}
	}

	if (useVBOs())
	{
		if (mGLArray)
		{
			bindGLArray();
			setup = false; // Do NOT perform pointer setup if using VAO
		}
		else
		{
			bool bind_buffer = bindGLBuffer();
			bool bind_indices = bindGLIndices();
			setup |= bind_buffer || bind_indices;
		}

		if (gDebugGL && !mGLArray)
		{
			GLint buff;
			glGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &buff);
			if ((U32)buff != mGLBuffer)
			{
				llwarns_once << "Invalid GL vertex buffer bound: " << buff
							 << " - Expected: " << mGLBuffer << llendl;
			}

			if (mGLIndices)
			{
				glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &buff);
				if ((U32)buff != mGLIndices)
				{
					llerrs << "Invalid GL index buffer bound: " << buff
						   << llendl;
				}
			}
		}
	}
	else
	{
		if (sGLRenderArray)
		{
#if GL_ARB_vertex_array_object
			glBindVertexArray(0);
#endif
			sGLRenderArray = 0;
			sGLRenderIndices = 0;
			sIBOActive = false;
		}

		if (mGLBuffer)
		{
			if (sVBOActive)
			{
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
				++sBindCount;
				sVBOActive = false;
				setup = true; // ... or a VBO is deactivated
			}
			if (sGLRenderBuffer != mGLBuffer)
			{
				sGLRenderBuffer = mGLBuffer;
				setup = true; // ... or a client memory pointer changed
			}
		}
		if (mGLIndices)
		{
			if (sIBOActive)
			{
				glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
				++sBindCount;
				sIBOActive = false;
			}

			sGLRenderIndices = mGLIndices;
		}
	}

	if (!mGLArray)
	{
		setupClientArrays(data_mask);
	}

	if (setup && data_mask && mGLBuffer)
	{
		setupVertexBuffer(data_mask);
		++sSetCount;
	}
}

//virtual
void LLVertexBuffer::setupVertexBuffer(U32 data_mask)
{
	data_mask &= ~mTypeMaskMask;
	stop_glerror();
	U8* base = useVBOs() ? (U8*)mAlignedOffset : mMappedData;

	if (gDebugGL && (data_mask & mTypeMask) != data_mask)
	{
		for (U32 i = 0; i < LLVertexBuffer::TYPE_MAX; ++i)
		{
			U32 mask = 1 << i;
			if (mask & data_mask && !(mask & mTypeMask))
			{
				// Bit set in data_mask, but not set in mTypeMask
				llwarns << "Missing required component " << vb_type_name[i]
						<< llendl;
			}
		}
		llassert(false);
	}

	if (gNoFixedFunction)
	{
		void* ptr;
		// NOTE: the 'loc' variable is *required* to pass as reference (passing
		// TYPE_* directly to glVertexAttribPointerARB() causes a crash),
		// unlike the OpenGL documentation prototype for this function... Go
		// figure !
		GLint loc;
		if (data_mask & MAP_NORMAL)
		{
			loc = TYPE_NORMAL;
			ptr = (void*)(base + mOffsets[TYPE_NORMAL]);
			glVertexAttribPointerARB(loc, 3, GL_FLOAT, GL_FALSE,
									 sTypeSize[TYPE_NORMAL], ptr);
		}
		if (data_mask & MAP_TEXCOORD3)
		{
			loc = TYPE_TEXCOORD3;
			ptr = (void*)(base + mOffsets[TYPE_TEXCOORD3]);
			glVertexAttribPointerARB(loc, 2, GL_FLOAT, GL_FALSE,
									 sTypeSize[TYPE_TEXCOORD3], ptr);
		}
		if (data_mask & MAP_TEXCOORD2)
		{
			loc = TYPE_TEXCOORD2;
			ptr = (void*)(base + mOffsets[TYPE_TEXCOORD2]);
			glVertexAttribPointerARB(loc, 2, GL_FLOAT, GL_FALSE,
									 sTypeSize[TYPE_TEXCOORD2], ptr);
		}
		if (data_mask & MAP_TEXCOORD1)
		{
			loc = TYPE_TEXCOORD1;
			ptr = (void*)(base + mOffsets[TYPE_TEXCOORD1]);
			glVertexAttribPointerARB(loc, 2, GL_FLOAT, GL_FALSE,
									 sTypeSize[TYPE_TEXCOORD1], ptr);
		}
		if (data_mask & MAP_TANGENT)
		{
			loc = TYPE_TANGENT;
			ptr = (void*)(base + mOffsets[TYPE_TANGENT]);
			glVertexAttribPointerARB(loc, 4, GL_FLOAT, GL_FALSE,
									 sTypeSize[TYPE_TANGENT], ptr);
		}
		if (data_mask & MAP_TEXCOORD0)
		{
			loc = TYPE_TEXCOORD0;
			ptr = (void*)(base + mOffsets[TYPE_TEXCOORD0]);
			glVertexAttribPointerARB(loc, 2, GL_FLOAT, GL_FALSE,
									 sTypeSize[TYPE_TEXCOORD0], ptr);
		}
		if (data_mask & MAP_COLOR)
		{
			loc = TYPE_COLOR;
			// Bind emissive instead of color pointer if emissive is present
			if (data_mask & MAP_EMISSIVE)
			{
				ptr = (void*)(base + mOffsets[TYPE_EMISSIVE]);
			}
			else
			{
				ptr = (void*)(base + mOffsets[TYPE_COLOR]);
			}
			glVertexAttribPointerARB(loc, 4, GL_UNSIGNED_BYTE, GL_TRUE,
									 sTypeSize[TYPE_COLOR], ptr);
		}
		if (data_mask & MAP_EMISSIVE)
		{
			loc = TYPE_EMISSIVE;
			ptr = (void*)(base + mOffsets[TYPE_EMISSIVE]);
			glVertexAttribPointerARB(loc, 4, GL_UNSIGNED_BYTE, GL_TRUE,
									 sTypeSize[TYPE_EMISSIVE], ptr);
			if (!(data_mask & MAP_COLOR))
			{
				// Map emissive to color channel when color is not also being
				// bound to avoid unnecessary shader swaps
				loc = TYPE_COLOR;
				glVertexAttribPointerARB(loc, 4, GL_UNSIGNED_BYTE, GL_TRUE,
										 sTypeSize[TYPE_EMISSIVE], ptr);
			}
		}
		if (data_mask & MAP_WEIGHT)
		{
			loc = TYPE_WEIGHT;
			ptr = (void*)(base + mOffsets[TYPE_WEIGHT]);
			glVertexAttribPointerARB(loc, 1, GL_FLOAT, GL_FALSE,
									 sTypeSize[TYPE_WEIGHT], ptr);
		}
		if (data_mask & MAP_WEIGHT4)
		{
			loc = TYPE_WEIGHT4;
			ptr = (void*)(base + mOffsets[TYPE_WEIGHT4]);
			glVertexAttribPointerARB(loc, 4, GL_FLOAT, GL_FALSE,
									 sTypeSize[TYPE_WEIGHT4], ptr);
		}
		if (data_mask & MAP_CLOTHWEIGHT)
		{
			loc = TYPE_CLOTHWEIGHT;
			ptr = (void*)(base + mOffsets[TYPE_CLOTHWEIGHT]);
			glVertexAttribPointerARB(loc, 4, GL_FLOAT, GL_TRUE,
									 sTypeSize[TYPE_CLOTHWEIGHT], ptr);
		}
#if !LL_DARWIN
		if (data_mask & MAP_TEXTURE_INDEX &&
			gGLManager.mHasVertexAttribIPointer)
		{
			loc = TYPE_TEXTURE_INDEX;
			ptr = (void*)(base + mOffsets[TYPE_VERTEX] + 12);
			glVertexAttribIPointer(loc, 1, GL_UNSIGNED_INT,
								   sTypeSize[TYPE_VERTEX], ptr);
		}
#endif
		if (data_mask & MAP_VERTEX)
		{
			loc = TYPE_VERTEX;
			ptr = (void*)(base + mOffsets[TYPE_VERTEX]);
			glVertexAttribPointerARB(loc, 3, GL_FLOAT, GL_FALSE,
									 sTypeSize[TYPE_VERTEX], ptr);
		}
	}
	else
	{
		if (data_mask & MAP_NORMAL)
		{
			glNormalPointer(GL_FLOAT, sTypeSize[TYPE_NORMAL],
							(void*)(base + mOffsets[TYPE_NORMAL]));
		}
		if (data_mask & MAP_TEXCOORD3)
		{
			glClientActiveTextureARB(GL_TEXTURE3_ARB);
			glTexCoordPointer(2, GL_FLOAT, sTypeSize[TYPE_TEXCOORD3],
							  (void*)(base + mOffsets[TYPE_TEXCOORD3]));
			glClientActiveTextureARB(GL_TEXTURE0_ARB);
		}
		if (data_mask & MAP_TEXCOORD2)
		{
			glClientActiveTextureARB(GL_TEXTURE2_ARB);
			glTexCoordPointer(2, GL_FLOAT, sTypeSize[TYPE_TEXCOORD2],
							  (void*)(base + mOffsets[TYPE_TEXCOORD2]));
			glClientActiveTextureARB(GL_TEXTURE0_ARB);
		}
		if (data_mask & MAP_TEXCOORD1)
		{
			glClientActiveTextureARB(GL_TEXTURE1_ARB);
			glTexCoordPointer(2, GL_FLOAT, sTypeSize[TYPE_TEXCOORD1],
							  (void*)(base + mOffsets[TYPE_TEXCOORD1]));
			glClientActiveTextureARB(GL_TEXTURE0_ARB);
		}
		if (data_mask & MAP_TANGENT)
		{
			glClientActiveTextureARB(GL_TEXTURE2_ARB);
			glTexCoordPointer(4, GL_FLOAT, sTypeSize[TYPE_TANGENT],
							  (void*)(base + mOffsets[TYPE_TANGENT]));
			glClientActiveTextureARB(GL_TEXTURE0_ARB);
		}
		if (data_mask & MAP_TEXCOORD0)
		{
			glTexCoordPointer(2, GL_FLOAT, sTypeSize[TYPE_TEXCOORD0],
							  (void*)(base + mOffsets[TYPE_TEXCOORD0]));
		}
		if (data_mask & MAP_COLOR)
		{
			glColorPointer(4, GL_UNSIGNED_BYTE, sTypeSize[TYPE_COLOR],
						   (void*)(base + mOffsets[TYPE_COLOR]));
		}
		if (data_mask & MAP_VERTEX)
		{
			glVertexPointer(3, GL_FLOAT, sTypeSize[TYPE_VERTEX], (void*)base);
		}
	}

	stop_glerror();
}

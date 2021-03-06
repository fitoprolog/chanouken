/**
 * @file llsdmessagebuilder.cpp
 * @brief LLSDMessageBuilder class implementation.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llsdmessagebuilder.h"

#include "llmessagetemplate.h"
#include "llmath.h"
#include "llquaternion.h"
#include "llsdutil.h"
#include "llsdutil_math.h"
#include "llsdserialize.h"
#include "llvector3d.h"
#include "llvector3.h"
#include "llvector4.h"

LLSDMessageBuilder::LLSDMessageBuilder()
:	mCurrentMessage(LLSD::emptyMap()),
	mCurrentBlock(NULL),
	mSBuilt(false),
	mSClear(true)
{
}

//virtual
void LLSDMessageBuilder::newMessage(const char* name)
{
	mSBuilt = false;
	mSClear = false;

	mCurrentMessage = LLSD::emptyMap();
	mCurrentMessageName = (char*)name;
}

//virtual
void LLSDMessageBuilder::clearMessage()
{
	mSBuilt = false;
	mSClear = true;

	mCurrentMessage = LLSD::emptyMap();
	mCurrentMessageName = "";
}

//virtual
void LLSDMessageBuilder::nextBlock(const char* blockname)
{
	LLSD& block = mCurrentMessage[blockname];
	if (block.isUndefined())
	{
		block[0] = LLSD::emptyMap();
		mCurrentBlock = &(block[0]);
	}
	else if (block.isArray())
	{
		block[block.size()] = LLSD::emptyMap();
		mCurrentBlock = &(block[block.size() - 1]);
	}
	else
	{
		llerrs << "Existing block is not an array" << llendl;
	}
}

void LLSDMessageBuilder::addBinaryData(const char* varname, const void* data,
									   S32 size)
{
	LLSD::Binary v;
	v.resize(size);
	memcpy(v.data(), reinterpret_cast<const U8*>(data), size);
	(*mCurrentBlock)[varname] = v;
}

void LLSDMessageBuilder::addS8(const char* varname, S8 v)
{
	(*mCurrentBlock)[varname] = v;
}

void LLSDMessageBuilder::addU8(const char* varname, U8 v)
{
	(*mCurrentBlock)[varname] = v;
}

void LLSDMessageBuilder::addS16(const char* varname, S16 v)
{
	(*mCurrentBlock)[varname] = v;
}

void LLSDMessageBuilder::addU16(const char* varname, U16 v)
{
	(*mCurrentBlock)[varname] = v;
}

void LLSDMessageBuilder::addF32(const char* varname, F32 v)
{
	(*mCurrentBlock)[varname] = v;
}

void LLSDMessageBuilder::addS32(const char* varname, S32 v)
{
	(*mCurrentBlock)[varname] = v;
}

void LLSDMessageBuilder::addU32(const char* varname, U32 v)
{
	(*mCurrentBlock)[varname] = ll_sd_from_U32(v);
}

void LLSDMessageBuilder::addU64(const char* varname, U64 v)
{
	(*mCurrentBlock)[varname] = ll_sd_from_U64(v);
}

void LLSDMessageBuilder::addF64(const char* varname, F64 v)
{
	(*mCurrentBlock)[varname] = v;
}

void LLSDMessageBuilder::addIPAddr(const char* varname, U32 v)
{
	(*mCurrentBlock)[varname] = ll_sd_from_ipaddr(v);
}

void LLSDMessageBuilder::addIPPort(const char* varname, U16 v)
{
	(*mCurrentBlock)[varname] = v;
}

void LLSDMessageBuilder::addBool(const char* varname, bool v)
{
	(*mCurrentBlock)[varname] = v;
}

void LLSDMessageBuilder::addString(const char* varname, const char* v)
{
	if (v)
	{
		(*mCurrentBlock)[varname] = v;
	}
	else
	{
		(*mCurrentBlock)[varname] = "";
	}
}

void LLSDMessageBuilder::addString(const char* varname, const std::string& v)
{
	if (v.size())
	{
		(*mCurrentBlock)[varname] = v;
	}
	else
	{
		(*mCurrentBlock)[varname] = "";
	}
}

void LLSDMessageBuilder::addVector3(const char* varname, const LLVector3& v)
{
	(*mCurrentBlock)[varname] = ll_sd_from_vector3(v);
}

void LLSDMessageBuilder::addVector4(const char* varname, const LLVector4& v)
{
	(*mCurrentBlock)[varname] = ll_sd_from_vector4(v);
}

void LLSDMessageBuilder::addVector3d(const char* varname, const LLVector3d& v)
{
	(*mCurrentBlock)[varname] = ll_sd_from_vector3d(v);
}

void LLSDMessageBuilder::addQuat(const char* varname, const LLQuaternion& v)
{
	(*mCurrentBlock)[varname] = ll_sd_from_quaternion(v);
}

void LLSDMessageBuilder::addUUID(const char* varname, const LLUUID& v)
{
	(*mCurrentBlock)[varname] = v;
}

void LLSDMessageBuilder::copyFromMessageData(const LLMsgData& data)
{
	// Counting variables used to encode multiple block info
	S32 block_count = 0;
    char* block_name = NULL;

	// Loop through msg blocks to loop through variables, totalling up size
	// data and filling the new (send) message
	for (LLMsgData::msg_blk_data_map_t::const_iterator
			iter = data.mMemberBlocks.begin(),
			end = data.mMemberBlocks.end(); iter != end; ++iter)
	{
		const LLMsgBlkData* mbci = iter->second;
		if (!mbci) continue;

		// Do we need to encode a block code ?
		if (block_count == 0)
		{
			block_count = mbci->mBlockNumber;
			block_name = (char*)mbci->mName;
		}

		// Counting down mutliple blocks
		--block_count;

		nextBlock(block_name);

		// Now loop through the variables
		for (LLMsgBlkData::msg_var_data_map_t::const_iterator
				dit = mbci->mMemberVarData.begin(),
				dend = mbci->mMemberVarData.end();
			 dit != dend; ++dit)
		{
			const LLMsgVarData& mvci = *dit;
			const char* varname = mvci.getName();

			switch (mvci.getType())
			{
				case MVT_FIXED:
					addBinaryData(varname, mvci.getData(), mvci.getSize());
					break;

				case MVT_VARIABLE:
				{
					// Ensure null terminated
					const char end = ((const char*)mvci.getData())[mvci.getSize() - 1];
					if (mvci.getDataSize() == 1 && end == 0)
					{
						addString(varname, (const char*)mvci.getData());
					}
					else
					{
						addBinaryData(varname, mvci.getData(), mvci.getSize());
					}
					break;
				}

				case MVT_U8:
					addU8(varname, *(U8*)mvci.getData());
					break;

				case MVT_U16:
					addU16(varname, *(U16*)mvci.getData());
					break;

				case MVT_U32:
					addU32(varname, *(U32*)mvci.getData());
					break;

				case MVT_U64:
					addU64(varname, *(U64*)mvci.getData());
					break;

				case MVT_S8:
					addS8(varname, *(S8*)mvci.getData());
					break;

				case MVT_S16:
					addS16(varname, *(S16*)mvci.getData());
					break;

				case MVT_S32:
					addS32(varname, *(S32*)mvci.getData());
					break;

				// S64 not supported in LLSD so we just truncate it
				case MVT_S64:
					addS32(varname, *(S64*)mvci.getData());
					break;

				case MVT_F32:
					addF32(varname, *(F32*)mvci.getData());
					break;

				case MVT_F64:
					addF64(varname, *(F64*)mvci.getData());
					break;

				case MVT_LLVector3:
					addVector3(varname, *(LLVector3*)mvci.getData());
					break;

				case MVT_LLVector3d:
					addVector3d(varname, *(LLVector3d*)mvci.getData());
					break;

				case MVT_LLVector4:
					addVector4(varname, *(LLVector4*)mvci.getData());
					break;

				case MVT_LLQuaternion:
				{
					LLVector3 v = *(LLVector3*)mvci.getData();
					LLQuaternion q;
					q.unpackFromVector3(v);
					addQuat(varname, q);
					break;
				}

				case MVT_LLUUID:
					addUUID(varname, *(LLUUID*)mvci.getData());
					break;

				case MVT_BOOL:
					addBool(varname, *(bool*)mvci.getData());
					break;

				case MVT_IP_ADDR:
					addIPAddr(varname, *(U32*)mvci.getData());
					break;

				case MVT_IP_PORT:
					addIPPort(varname, *(U16*)mvci.getData());
					break;

				case MVT_U16Vec3:
					// treated as an array of 6 bytes
					addBinaryData(varname, mvci.getData(), 6);
					break;

				case MVT_U16Quat:
					// treated as an array of 8 bytes
					addBinaryData(varname, mvci.getData(), 8);
					break;

				case MVT_S16Array:
					addBinaryData(varname, mvci.getData(), mvci.getSize());
					break;

				default:
					llwarns << "Unknown type in conversion of message to LLSD"
							<< llendl;
			}
		}
	}
}

//virtual
void LLSDMessageBuilder::copyFromLLSD(const LLSD& msg)
{
	mCurrentMessage = msg;
	LL_DEBUGS("Messaging") << LLSDNotationStreamer(mCurrentMessage) << LL_ENDL;
}

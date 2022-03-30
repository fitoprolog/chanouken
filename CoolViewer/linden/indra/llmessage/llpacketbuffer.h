/**
 * @file llpacketbuffer.h
 * @brief definition of LLPacketBuffer class for implementing a
 * resend, drop, or delay in packet transmissions.
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

#ifndef LL_LLPACKETBUFFER_H
#define LL_LLPACKETBUFFER_H

#include "llnet.h"		// For NET_BUFFER_SIZE
#include "llhost.h"

class LLPacketBuffer
{
public:
	LLPacketBuffer(const LLHost &host, const char* datap, S32 size);
	LLPacketBuffer(S32 socket);

	LL_INLINE S32 getSize() const					{ return mSize; }
	LL_INLINE const char* getData() const			{ return mData; }
	LL_INLINE LLHost getHost() const				{ return mHost; }
	LL_INLINE LLHost getReceivingInterface() const	{ return mReceivingIF; }
	void init(S32 socket);

protected:
	char	mData[NET_BUFFER_SIZE];	// packet data
	S32		mSize;					// size of buffer in bytes
	LLHost	mHost;					// source/dest IP and port
	LLHost	mReceivingIF;			// source/dest IP and port
};

#endif

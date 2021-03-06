/** 
 * @file llxmlrpctransaction.h
 * @brief LLXMLRPCTransaction and related class header file
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LLXMLRPCTRANSACTION_H
#define LLXMLRPCTRANSACTION_H

#include <string>

// Foward decl of types from xmlrpc.h (this usage is type safe)
typedef struct _xmlrpc_request* XMLRPC_REQUEST;
typedef struct _xmlrpc_value* XMLRPC_VALUE;

// A C++ wrapper around XMLRPC_VALUE
class LLXMLRPCValue
{
public:
	LLXMLRPCValue()						: mV(NULL) {}
	LLXMLRPCValue(XMLRPC_VALUE value)	: mV(value) {}

	bool isValid() const;

	std::string	asString()	const;
	int			asInt()		const;
	bool		asBool()	const;
	double		asDouble()	const;

	LLXMLRPCValue operator[](const char*) const;

	LLXMLRPCValue rewind();
	LLXMLRPCValue next();

	static LLXMLRPCValue createArray();
	static LLXMLRPCValue createStruct();

	void append(LLXMLRPCValue&);
	void appendString(const std::string&);
	void appendInt(int);
	void appendBool(bool);
	void appendDouble(double);
	void appendValue(LLXMLRPCValue&);

	void append(const char*, LLXMLRPCValue&);
	void appendString(const char*, const std::string&);
	void appendInt(const char*, int);
	void appendBool(const char*, bool);
	void appendDouble(const char*, double);
	void appendValue(const char*, LLXMLRPCValue&);

	void cleanup();	// only call this on the top level created value

	XMLRPC_VALUE getValue() const;

private:
	XMLRPC_VALUE mV;
};

// An asynchronous request and respones via XML-RPC
class LLXMLRPCTransaction
{
protected:
	LOG_CLASS(LLXMLRPCTransaction);

public:
	// Does not take ownership of the request object: request can be freed as
	// soon as the transaction is constructed.
	LLXMLRPCTransaction(const std::string& uri,
						XMLRPC_REQUEST request,
						bool use_gzip = true);

	// *Does* take control of the request value, you must not free it.
	LLXMLRPCTransaction(const std::string& uri,
						const std::string& method,
						LLXMLRPCValue params,
						bool use_gzip = true);

	~LLXMLRPCTransaction();

	typedef enum
	{
		StatusNotStarted,
		StatusStarted,
		StatusDownloading,	// Not used any more (downloading done in request)
		StatusComplete,
		StatusCURLError,
		StatusXMLRPCError,
		StatusOtherError
	} EStatus;

	// Runs the request a little, returns true when done
	bool process();

	// Returns status, and extended CURL code, if code isn't null.
	EStatus status(S32* curl_code);
	// Returns a message string, suitable for showing the user.
	std::string statusMessage();
	// Returns a URI for the user with more information. Can be empty.
	std::string statusURI();

	XMLRPC_REQUEST response();
	// Only valid if StatusComplete, otherwise NULL.
	// Retains ownership of the result object, don't free it.
	LLXMLRPCValue responseValue();

	// Only valid if StsatusComplete, otherwise 0.0.
	F64 transferRate();

	// Used from newview to set various URLs, parameters and messages,
	// depending on the grid we logged into, the user preferences and the UI
	// language. This avoids dependencies on newview and llui classes.
	static void setSupportURL(const std::string& url);
	static void setWebsiteURL(const std::string& url);
	static void setVerifyCert(bool verify);
	static void setMessages(const std::string& server_down,
							const std::string& not_resolving,
							const std::string& not_verified,
							const std::string& connect_error);

private:
	class Handler;
	class Impl;

	Impl& impl;
};

#endif // LLXMLRPCTRANSACTION_H

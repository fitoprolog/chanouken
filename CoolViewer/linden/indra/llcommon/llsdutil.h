/**
 * @file llsdutil.h
 * @author Phoenix
 * @date 2006-05-24
 * @brief Utility classes, functions, etc, for using structured data.
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

#ifndef LL_LLSDUTIL_H
#define LL_LLSDUTIL_H

#include "llsd.h"

// U32
LL_COMMON_API LLSD ll_sd_from_U32(U32);
LL_COMMON_API U32 ll_U32_from_sd(const LLSD& sd);

// U64
LL_COMMON_API LLSD ll_sd_from_U64(U64);
LL_COMMON_API U64 ll_U64_from_sd(const LLSD& sd);

// IP Address
LL_COMMON_API LLSD ll_sd_from_ipaddr(U32);
LL_COMMON_API U32 ll_ipaddr_from_sd(const LLSD& sd);

// Binary to string
LL_COMMON_API LLSD ll_string_from_binary(const LLSD& sd);

//String to binary
LL_COMMON_API LLSD ll_binary_from_string(const LLSD& sd);

// Serializes sd to static buffer and returns pointer, useful for gdb debugging.
LL_COMMON_API char* ll_print_sd(const LLSD& sd);

// Serializes sd to static buffer and returns pointer, using "pretty printing"
// mode.
LL_COMMON_API char* ll_pretty_print_sd_ptr(const LLSD* sd);
LL_COMMON_API char* ll_pretty_print_sd(const LLSD& sd);

// Compares the structure of an LLSD to a template LLSD and stores the "valid"
// values in a 3rd LLSD. Default values are pulled from the template. Extra
// keys/values in the test are ignored in the resultant LLSD. Ordering of
// arrays matters.
// Returns false if the test is of same type but values differ in type.
// Otherwise, returns true.

LL_COMMON_API bool compare_llsd_with_template(const LLSD& llsd_to_test,
											  const LLSD& template_llsd,
											  LLSD& resultant_llsd);

// filter_llsd_with_template() is a direct clone (copy-n-paste) of 
// compare_llsd_with_template with the following differences:
// (1) bool vs BOOL return types
// (2) A map with the key value "*" is a special value and maps any key in the
//     test llsd that doesn't have an explicitly matching key in the template.
// (3) The element of an array with exactly one element is taken as a template
//     for *all* the elements of the test array.  If the template array is of
//     different size, compare_llsd_with_template() semantics apply.
LL_COMMON_API bool filter_llsd_with_template(const LLSD& llsd_to_test,
											 const LLSD& template_llsd,
											 LLSD& result_llsd);

/**
 * Recursively determine whether a given LLSD data block "matches" another
 * LLSD prototype. The returned string is empty() on success, non-empty() on
 * mismatch.
 *
 * This function tests structure (types) rather than data values. It is
 * intended for when a consumer expects an LLSD block with a particular
 * structure, and must succinctly detect whether the arriving block is
 * well-formed. For instance, a test of the form:
 * @code
 * if (!(data.has("request") && data.has("target") && data.has("modifier") ...))
 * @endcode
 * could instead be expressed by initializing a prototype LLSD map with the
 * required keys and writing:
 * @code
 * if (! llsd_matches(prototype, data).empty())
 * @endcode
 *
 * A non-empty return value is an error-message fragment intended to indicate
 * to (English-speaking) developers where in the prototype structure the
 * mismatch occurred.
 *
 * * If a slot in the prototype isUndefined(), then anything is valid at that
 *   place in the real object. (Passing prototype == LLSD() matches anything
 *   at all.)
 * * An array in the prototype must match a data array at least that large.
 *   (Additional entries in the data array are ignored.) Every isDefined()
 *   entry in the prototype array must match the corresponding entry in the
 *   data array.
 * * A map in the prototype must match a map in the data. Every key in the
 *   prototype map must match a corresponding key in the data map. (Additional
 *   keys in the data map are ignored.) Every isDefined() value in the
 *   prototype map must match the corresponding key's value in the data map.
 * * Scalar values in the prototype are tested for @em type rather than value.
 *   For instance, a String in the prototype matches any String at all. In
 *   effect, storing an Integer at a particular place in the prototype asserts
 *   that the caller intends to apply asInteger() to the corresponding slot in
 *   the data.
 * * A String in the prototype matches String, Boolean, Integer, Real, UUID,
 *   Date and URI, because asString() applied to any of these produces a
 *   meaningful result.
 * * Similarly, a Boolean, Integer or Real in the prototype can match any of
 *   Boolean, Integer or Real in the data -- or even String.
 * * UUID matches UUID or String.
 * * Date matches Date or String.
 * * URI matches URI or String.
 * * Binary in the prototype matches only Binary in the data.
 *
 * @TODO: when a Boolean, Integer or Real in the prototype matches a String in
 * the data, we should examine the String @em value to ensure it can be
 * meaningfully converted to the requested type. The same goes for UUID, Date
 * and URI.
 */
LL_COMMON_API std::string llsd_matches(const LLSD& prototype, const LLSD& data,
									   const std::string& pfx = "");

// Deep equality. If you want to compare LLSD::Real values for approximate
// equality rather than bitwise equality, pass @a bits as for
// is_approx_equal_fraction().
LL_COMMON_API bool llsd_equals(const LLSD& lhs, const LLSD& rhs,
							   S32 bits = -1);

// Simple function to copy data out of input & output iterators if
// there is no need for casting.
template<typename Input> LLSD llsd_copy_array(Input iter, Input end)
{
	LLSD dest;
	for ( ; iter != end; ++iter)
	{
		dest.append(*iter);
	}
	return dest;
}

namespace llsd
{
	LL_COMMON_API LLSD& drill_ref(LLSD& blob, const LLSD& path);

	LL_COMMON_API LL_INLINE LLSD drill(const LLSD& blob, const LLSD& path)
	{
		// Non-const drill_ref() does exactly what we want. Temporarily cast
		// away constness and use that.
		return drill_ref(const_cast<LLSD&>(blob), path);
	}
}

/*****************************************************************************
*   LLSDArray
*****************************************************************************/
/**
 * Construct an LLSD::Array inline, with implicit conversion to LLSD. Usage:
 *
 * @code
 * void somefunc(const LLSD&);
 * ...
 * somefunc(LLSDArray("text")(17)(3.14));
 * @endcode
 *
 * For completeness, LLSDArray() with no args constructs an empty array, so
 * <tt>LLSDArray()("text")(17)(3.14)</tt> produces an array equivalent to the
 * above. But for most purposes, LLSD() is already equivalent to an empty
 * array, and if you explicitly want an empty isArray(), there's
 * LLSD::emptyArray(). However, supporting a no-args LLSDArray() constructor
 * follows the principle of least astonishment.
 */
class LLSDArray
{
public:
	LLSDArray()
	:	_data(LLSD::emptyArray())
	{
	}

	/**
	 * Need an explicit copy constructor. Consider the following:
	 *
	 * @code
	 * LLSD array_of_arrays(LLSDArray(LLSDArray(17)(34))
	 *							   (LLSDArray("x")("y")));
	 * @endcode
	 *
	 * The coder intends to construct [[17, 34], ["x", "y"]].
	 *
	 * With the compiler's implicit copy constructor, s/he gets instead
	 * [17, 34, ["x", "y"]].
	 *
	 * The expression LLSDArray(17)(34) constructs an LLSDArray with those two
	 * values. The reader assumes it should be converted to LLSD, as we always
	 * want with LLSDArray, before passing it to the @em outer LLSDArray
	 * constructor! This copy constructor makes that happen.
	 */
	LLSDArray(const LLSDArray& inner)
	:	_data(LLSD::emptyArray())
	{
		_data.append(inner);
	}

	LLSDArray(const LLSD& value)
	:	_data(LLSD::emptyArray())
	{
		_data.append(value);
	}

	LLSDArray& operator()(const LLSD& value)
	{
		_data.append(value);
		return *this;
	}

	LL_INLINE operator LLSD() const			{ return _data; }
	LL_INLINE LLSD get() const				{ return _data; }

private:
	LLSD _data;
};

/*****************************************************************************
*   LLSDMap
*****************************************************************************/
/**
 * Construct an LLSD::Map inline, with implicit conversion to LLSD. Usage:
 *
 * @code
 * void somefunc(const LLSD&);
 * ...
 * somefunc(LLSDMap("alpha", "abc")("number", 17)("pi", 3.14));
 * @endcode
 *
 * For completeness, LLSDMap() with no args constructs an empty map, so
 * <tt>LLSDMap()("alpha", "abc")("number", 17)("pi", 3.14)</tt> produces a map
 * equivalent to the above. But for most purposes, LLSD() is already
 * equivalent to an empty map, and if you explicitly want an empty isMap(),
 * there's LLSD::emptyMap(). However, supporting a no-args LLSDMap()
 * constructor follows the principle of least astonishment.
 */
class LLSDMap
{
public:
	LLSDMap()
	:	_data(LLSD::emptyMap())
	{
	}

	LLSDMap(const LLSD::String& key, const LLSD& value)
	:	_data(LLSD::emptyMap())
	{
		_data[key] = value;
	}

	LLSDMap& operator()(const LLSD::String& key, const LLSD& value)
	{
		_data[key] = value;
		return *this;
	}

	LL_INLINE operator LLSD() const			{ return _data; }
	LL_INLINE LLSD get() const				{ return _data; }

private:
	LLSD _data;
};

/*****************************************************************************
*   LLSDParam
*****************************************************************************/
/**
 * LLSDParam is a customization point for passing LLSD values to function
 * parameters of more or less arbitrary type. LLSD provides a small set of
 * native conversions; but if a generic algorithm explicitly constructs an
 * LLSDParam object in the function's argument list, a consumer can provide
 * LLSDParam specializations to support more different parameter types than
 * LLSD's native conversions.
 *
 * Usage:
 *
 * @code
 * void somefunc(const paramtype&);
 * ...
 * somefunc(..., LLSDParam<paramtype>(someLLSD), ...);
 * @endcode
 */
template <typename T>
class LLSDParam
{
public:
	/**
	 * Default implementation converts to T on construction, saves converted
	 * value for later retrieval
	 */
	LLSDParam(const LLSD& value)
	:	_value(value)
	{
	}

	LL_INLINE operator T() const			{ return _value; }

private:
	T _value;
};

/**
 * Turns out that several target types could accept an LLSD param using any of
 * a few different conversions, e.g. LLUUID's constructor can accept LLUUID or
 * std::string. Therefore, the compiler can't decide which LLSD conversion
 * operator to choose, even though to us it seems obvious. But that's okay, we
 * can specialize LLSDParam for such target types, explicitly specifying the
 * desired conversion -- that's part of what LLSDParam is all about. Turns out
 * we have to do that enough to make it worthwhile generalizing. Use a macro
 * because I need to specify one of the asReal, etc., explicit conversion
 * methods as well as a type. If I'm overlooking a clever way to implement
 * that using a template instead, feel free to reimplement.
 */
#define LLSDParam_for(T, AS)					\
template <>									 \
class LLSDParam<T>							  \
{											   \
public:										 \
	LLSDParam(const LLSD& value):			   \
		_value(value.AS())					  \
	{}										  \
												\
	operator T() const { return _value; }	   \
												\
private:										\
	T _value;								   \
}

LLSDParam_for(F32, asReal);
LLSDParam_for(LLUUID, asUUID);
LLSDParam_for(LLDate, asDate);
LLSDParam_for(LLURI, asURI);
LLSDParam_for(LLSD::Binary, asBinary);

/**
 * LLSDParam<const char*> is an example of the kind of conversion you can
 * support with LLSDParam beyond native LLSD conversions. Normally you can't
 * pass an LLSD object to a function accepting const char* -- but you can
 * safely pass an LLSDParam<const char*>(yourLLSD).
 */
template <>
class LLSDParam<const char*>
{
private:
	// The difference here is that we store a std::string rather than a const
	// char*. It's important that the LLSDParam object own the std::string.
	std::string _value;
	// We don't bother storing the incoming LLSD object, but we do have to
	// distinguish whether _value is an empty string because the LLSD object
	// contains an empty string or because it's isUndefined().
	bool _undefined;

public:
	LLSDParam(const LLSD& value)
	:	_value(value),
		_undefined(value.isUndefined())
	{
	}

	// The const char* we retrieve is for storage owned by our _value member.
	// That's how we guarantee that the const char* is valid for the lifetime
	// of this LLSDParam object. Constructing your LLSDParam in the argument
	// list should ensure that the LLSDParam object will persist for the
	// duration of the function call.
	operator const char*() const
	{
		if (_undefined)
		{
			// By default, an isUndefined() LLSD object's asString() method
			// will produce an empty string. But for a function accepting
			// const char*, it's often important to be able to pass NULL, and
			// isUndefined() seems like the best way. If you want to pass an
			// empty string, you can still pass LLSD(""). Without this special
			// case, though, no LLSD value could pass NULL.
			return NULL;
		}
		return _value.c_str();
	}
};

namespace llsd
{

/*****************************************************************************
*   BOOST_FOREACH() helpers for LLSD
*****************************************************************************/
/// Usage: BOOST_FOREACH(LLSD item, inArray(someLLSDarray)) { ... }
class inArray
{
public:
	inArray(const LLSD& array)
	:	_array(array)
	{
	}

	typedef LLSD::array_const_iterator const_iterator;
	typedef LLSD::array_iterator iterator;

	LL_INLINE iterator begin()				{ return _array.beginArray(); }
	LL_INLINE iterator end()				{ return _array.endArray(); }
	LL_INLINE const_iterator begin() const	{ return _array.beginArray(); }
	LL_INLINE const_iterator end() const	{ return _array.endArray(); }

private:
	LLSD _array;
};

/// MapEntry is what you get from dereferencing an LLSD::map_[const_]iterator.
typedef std::map<LLSD::String, LLSD>::value_type MapEntry;

/// Usage: BOOST_FOREACH([const] MapEntry& e, inMap(someLLSDmap)) { ... }
class inMap
{
public:
	inMap(const LLSD& map)
	:	_map(map)
	{
	}

	typedef LLSD::map_const_iterator const_iterator;
	typedef LLSD::map_iterator iterator;

	LL_INLINE iterator begin()				{ return _map.beginMap(); }
	LL_INLINE iterator end()				{ return _map.endMap(); }
	LL_INLINE const_iterator begin() const	{ return _map.beginMap(); }
	LL_INLINE const_iterator end() const	{ return _map.endMap(); }

private:
	LLSD _map;
};

} // namespace llsd

// Creates a deep clone of an LLSD object. Maps, Arrays and binary objects are
// duplicated, atomic primitives (Boolean, Integer, Real, etc) simply use a
// shared reference.
// Optionally a filter may be specified to control what is duplicated. The map
// takes the form "keyname/boolean".
// If the value is true the value will be duplicated otherwise it will be
// skipped when encountered in a map. A key name of "*" can be specified as a
// wild card and will specify the default behavior.  If no wild card is given
// and the clone encounters a name not in the filter, that value will be
// skipped.
LLSD LL_COMMON_API llsd_clone(LLSD value, LLSD filter = LLSD());

// Creates a shallow copy of a map or array. If passed any other type of LLSD
// object it simply returns that value.  See llsd_clone for a description of
// the filter parameter.
LLSD LL_COMMON_API llsd_shallow(LLSD value, LLSD filter = LLSD());

// Specialization for generating a hash value from an LLSD block.
size_t LL_COMMON_API hash_value(const LLSD& s) noexcept;

// std::hash implementation for LLSD
namespace std
{
	template<> struct hash<LLSD>
	{
		LL_INLINE size_t operator()(const LLSD& s) const noexcept
		{
			return hash_value(s);
		}
	};
}

#endif // LL_LLSDUTIL_H

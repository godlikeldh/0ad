/* Copyright (C) 2010 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "BinarySerializer.h"

#include "SerializedScriptTypes.h"

#include "lib/byte_order.h"
#include "lib/wchar.h"
#include "ps/CLogger.h"
#include "ps/utf16string.h"

#include "scriptinterface/ScriptInterface.h"
#include "js/jsapi.h"

CBinarySerializer::CBinarySerializer(ScriptInterface& scriptInterface) :
	m_ScriptInterface(scriptInterface)
{

}

CBinarySerializer::~CBinarySerializer()
{
	FreeScriptBackrefs();
}

/*

The Put* implementations here are designed for subclasses
that want an efficient, portable, deserializable representation.
(Subclasses with different requirements should override these methods.)

Numbers are converted to little-endian byte strings, for portability
and efficiency.

Data is not aligned, for storage efficiency.

*/

void CBinarySerializer::PutNumber(const char* name, uint8_t value)
{
	Put(name, (const u8*)&value, sizeof(uint8_t));
}

void CBinarySerializer::PutNumber(const char* name, int32_t value)
{
	int32_t v = (i32)to_le32((u32)value);
	Put(name, (const u8*)&v, sizeof(int32_t));
}

void CBinarySerializer::PutNumber(const char* name, uint32_t value)
{
	uint32_t v = to_le32(value);
	Put(name, (const u8*)&v, sizeof(uint32_t));
}

void CBinarySerializer::PutNumber(const char* name, float value)
{
	Put(name, (const u8*)&value, sizeof(float));
}

void CBinarySerializer::PutNumber(const char* name, double value)
{
	Put(name, (const u8*)&value, sizeof(double));
}

void CBinarySerializer::PutNumber(const char* name, CFixed_23_8 value)
{
	PutNumber(name, value.GetInternalValue());
}

void CBinarySerializer::PutBool(const char* name, bool value)
{
	NumberU8(name, value ? 1 : 0, 0, 1);
}

void CBinarySerializer::PutString(const char* name, const std::string& value)
{
	// TODO: should intern strings, particularly to save space with script property names
	PutNumber("string length", (uint32_t)value.length());
	Put(name, (u8*)value.data(), value.length());
}

void CBinarySerializer::PutScriptVal(const char* UNUSED(name), jsval value)
{
	HandleScriptVal(value);
}

////////////////////////////////////////////////////////////////

// Exception-safety and GC-safety wrapper for JSIdArray
class IdArrayWrapper
{
	JSContext* m_cx;
	JSIdArray* m_ida;
public:
	IdArrayWrapper(JSContext* cx, JSIdArray* ida) :
		m_cx(cx), m_ida(ida)
	{
		for (jsint i = 0; i < m_ida->length; ++i)
			if (!JS_AddRoot(m_cx, &m_ida->vector[i]))
				throw PSERROR_Serialize_ScriptError("JS_AddRoot failed");
	}
	~IdArrayWrapper()
	{
		for (jsint i = 0; i < m_ida->length; ++i)
			if (!JS_RemoveRoot(m_cx, &m_ida->vector[i]))
				throw PSERROR_Serialize_ScriptError("JS_RemoveRoot failed");
		JS_DestroyIdArray(m_cx, m_ida);
	}
};

class RootWrapper
{
	JSContext* m_cx;
	void* m_obj;
public:
	// obj must be a JSObject** or JSString** or jsval* etc
	RootWrapper(JSContext* cx, void* obj) :
		m_cx(cx), m_obj(obj)
	{
		if (!JS_AddRoot(m_cx, m_obj))
			throw PSERROR_Serialize_ScriptError("JS_AddRoot failed");
	}
	~RootWrapper()
	{
		if (!JS_RemoveRoot(m_cx, m_obj))
			throw PSERROR_Serialize_ScriptError("JS_RemoveRoot failed");
	}
};

void CBinarySerializer::HandleScriptVal(jsval val)
{
	JSContext* cx = m_ScriptInterface.GetContext();

	switch (JS_TypeOfValue(cx, val))
	{
	case JSTYPE_VOID:
	{
		NumberU8_Unbounded("type", SCRIPT_TYPE_VOID);
		break;
	}
	case JSTYPE_NULL: // This type is never actually returned (it's a JS2 feature)
	{
		NumberU8_Unbounded("type", SCRIPT_TYPE_NULL);
		break;
	}
	case JSTYPE_OBJECT:
	{
		if (JSVAL_IS_NULL(val))
		{
			NumberU8_Unbounded("type", SCRIPT_TYPE_NULL);
			break;
		}

		JSObject* obj = JSVAL_TO_OBJECT(val);

		// If we've already serialized this object, just output a reference to it
		u32 tag = GetScriptBackrefTag(obj);
		if (tag)
		{
			NumberU8_Unbounded("type", SCRIPT_TYPE_BACKREF);
			NumberU32_Unbounded("tag", tag);
			break;
		}

		if (JS_IsArrayObject(cx, obj))
		{
			NumberU8_Unbounded("type", SCRIPT_TYPE_ARRAY);
			// TODO: probably should have a more efficient storage format
		}
		else
		{
			NumberU8_Unbounded("type", SCRIPT_TYPE_OBJECT);

			//			if (JS_GetClass(cx, obj))
			//			{
			//				LOGERROR("Cannot serialise JS objects of type 'object' with a class");
			//				throw PSERROR_Serialize_InvalidScriptValue();
			//			}
			// TODO: ought to complain only about non-standard classes
			// TODO: probably ought to do something cleverer for classes, prototypes, etc
			// (See Trac #406, #407)
		}

		// Find all properties (ordered by insertion time)
		JSIdArray* ida = JS_Enumerate(cx, obj);
		if (!ida)
		{
			LOGERROR(L"JS_Enumerate failed");
			throw PSERROR_Serialize_ScriptError();
		}
		// For safety, root all the property IDs
		// (This should be unnecessary if we're certain that properties could never
		// get deleted during deserialization)
		IdArrayWrapper idaWrapper(cx, ida);

		NumberU32_Unbounded("num props", (uint32_t)ida->length);

		for (jsint i = 0; i < ida->length; ++i)
		{
			jsval idval, propval;
			uintN attrs;
			JSBool found;

			// Find the attribute name
			// (TODO: just use JS_GetPropertyById if we ever upgrade to Spidermonkey 1.8.1)

			if (!JS_IdToValue(cx, ida->vector[i], &idval))
			{
				LOGERROR(L"JS_IdToValue failed");
				throw PSERROR_Serialize_ScriptError();
			}

			JSString* idstr = JS_ValueToString(cx, idval);
			if (!idstr)
			{
				LOGERROR(L"JS_ValueToString failed");
				throw PSERROR_Serialize_ScriptError();
			}
			RootWrapper idstrWrapper(cx, &idstr);

			if (!JS_GetUCPropertyAttributes(cx, obj, JS_GetStringChars(idstr), JS_GetStringLength(idstr), &attrs, &found))
				throw PSERROR_Serialize_ScriptError("JS_GetUCPropertyAttributes failed");
			if (!found)
				throw PSERROR_Serialize_ScriptError("JS_GetUCPropertyAttributes didn't find enumerated property");

			if (attrs & JSPROP_GETTER)
				throw PSERROR_Serialize_ScriptError("Cannot serialize property getters");

			ScriptString("prop name", idstr);
			if (!JS_GetUCProperty(cx, obj, JS_GetStringChars(idstr), JS_GetStringLength(idstr), &propval))
			{
				LOGERROR(L"JS_GetUCProperty failed");
				throw PSERROR_Serialize_ScriptError();
			}

			HandleScriptVal(propval);
		}
		break;
	}
	case JSTYPE_FUNCTION:
	{
		LOGERROR(L"Cannot serialise JS objects of type 'function'");
		throw PSERROR_Serialize_InvalidScriptValue();
	}
	case JSTYPE_STRING:
	{
		NumberU8_Unbounded("type", SCRIPT_TYPE_STRING);
		ScriptString("string", JSVAL_TO_STRING(val));
		break;
	}
	case JSTYPE_NUMBER:
	{
		// For efficiency, handle ints and doubles separately.
		if (JSVAL_IS_INT(val))
		{
			NumberU8_Unbounded("type", SCRIPT_TYPE_INT);
			// jsvals are limited to JSVAL_INT_BITS == 31 bits, even on 64-bit platforms
			NumberI32("value", (int32_t)JSVAL_TO_INT(val), JSVAL_INT_MIN, JSVAL_INT_MAX);
		}
		else
		{
			debug_assert(JSVAL_IS_DOUBLE(val));
			NumberU8_Unbounded("type", SCRIPT_TYPE_DOUBLE);
			jsdouble* dbl = JSVAL_TO_DOUBLE(val);
			NumberDouble_Unbounded("value", *dbl);
		}
		break;
	}
	case JSTYPE_BOOLEAN:
	{
		NumberU8_Unbounded("type", SCRIPT_TYPE_BOOLEAN);
		JSBool b = JSVAL_TO_BOOLEAN(val);
		NumberU8_Unbounded("value", b ? 1 : 0);
		break;
	}
	case JSTYPE_XML:
	{
		LOGERROR(L"Cannot serialise JS objects of type 'xml'");
		throw PSERROR_Serialize_InvalidScriptValue();
	}
	default:
	{
		debug_warn(L"Invalid TypeOfValue");
		throw PSERROR_Serialize_InvalidScriptValue();
	}
	}
}

void CBinarySerializer::ScriptString(const char* name, JSString* string)
{
	jschar* chars = JS_GetStringChars(string);
	size_t length = JS_GetStringLength(string);

	// Use UTF-8, for storage efficiency
	// TODO: Maybe we should have a utf8_from_utf16string

	utf16string str16(chars, chars + length);
	std::wstring strw(str16.begin(), str16.end());
	LibError err;
	std::string str8 = utf8_from_wstring(strw, &err);
	if (err != INFO::OK)
		throw PSERROR_Serialize_InvalidCharInString();
	PutString(name, str8);
}

u32 CBinarySerializer::GetScriptBackrefTag(JSObject* obj)
{
	// To support non-tree structures (e.g. "var x = []; var y = [x, x];"), we need a way
	// to indicate multiple references to one object(/array). So every time we serialize a
	// new object, we give it a new non-zero tag; when we serialize it a second time we just
	// refer to that tag.
	//
	// The tags are stored in a map. Maybe it'd be more efficient to store it inline in the object
	// somehow? but this works okay for now

	std::pair<std::map<JSObject*, u32>::iterator, bool> it = m_ScriptBackrefs.insert(std::make_pair(obj, (u32)m_ScriptBackrefs.size()+1));

	// If it was already there, return the tag
	if (!it.second)
		return it.first->second;

	// If it was newly inserted, we need to make sure it gets rooted
	// for the duration that it's in m_ScriptBackrefs
	if (!JS_AddRoot(m_ScriptInterface.GetContext(), (void*)&it.first->first))
		throw PSERROR_Serialize_ScriptError("JS_AddRoot failed");
	// Return a non-tag number so callers know they need to serialize the object
	return 0;
}

void CBinarySerializer::FreeScriptBackrefs()
{
	std::map<JSObject*, u32>::iterator it = m_ScriptBackrefs.begin();
	for (; it != m_ScriptBackrefs.end(); ++it)
	{
		if (!JS_RemoveRoot(m_ScriptInterface.GetContext(), (void*)&it->first))
			throw PSERROR_Serialize_ScriptError("JS_RemoveRoot failed");
	}
	m_ScriptBackrefs.clear();
}

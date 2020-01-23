/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#ifndef __EscargotPointerValue__
#define __EscargotPointerValue__

namespace Escargot {

class Value;
class String;
class Symbol;
class Object;
class FunctionObject;
class NativeFunctionObject;
class ScriptFunctionObject;
class ScriptArrowFunctionObject;
class ScriptGeneratorFunctionObject;
class ScriptAsyncFunctionObject;
class ScriptAsyncGeneratorFunctionObject;
class ScriptClassConstructorFunctionObject;
class ArrayObject;
class StringObject;
class SymbolObject;
class NumberObject;
class BooleanObject;
class RegExpObject;
class DateObject;
class ErrorObject;
class GlobalObject;
class BoundFunctionObject;
class PromiseObject;
class ProxyObject;
class ArrayBufferObject;
class ArrayBufferView;
class DoubleInSmallValue;
class JSGetterSetter;
class GlobalRegExpFunctionObject;
class IteratorObject;
class MapObject;
class SetObject;
class WeakMapObject;
class WeakSetObject;
class GeneratorObject;
class AsyncGeneratorObject;

#define POINTER_VALUE_STRING_TAG_IN_DATA 0x1
#define POINTER_VALUE_SYMBOL_TAG_IN_DATA 0x2
// finding what is type of PointerValue operation is used in SmallValue <-> Value and interpreter
// Only Object, String are seen in regular runtime-code
// We figure what type of PointerValue by POINTER_VALUE_STRING_TAG_IN_DATA
//   - Every data area of Object starts with [<vtable>, m_structure...]
//   - Every data area of String starts with [<vtable>, POINTER_VALUE_STRING_TAG_IN_DATA ...]
// finding what is type of PointerValue(Object, String) without accessing vtable provides gives better performance
// but, it uses more memory for String,Symbol type
// POINTER_VALUE_STRING_TAG_IN_DATA is not essential thing for implementing figure type(we can use isObject, isString)
// so, we can remove POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA in very small device future

class PointerValue : public gc {
    friend class ByteCodeInterpreter;
    friend class Object;

public:
    virtual ~PointerValue() {}
    bool isString() const
    {
        return isStringByDataTag();
    }

    virtual bool isStringByVTable() const
    {
        return false;
    }

    bool isSymbol() const
    {
        return isSymbolByDataTag();
    }

    virtual bool isSymbolByVTable() const
    {
        return false;
    }

    bool isObject() const
    {
        return isObjectByDataTag();
    }

    virtual bool isObjectByVTable() const
    {
        return false;
    }

    virtual bool isFunctionObject() const
    {
        return false;
    }

    virtual bool isScriptClassConstructorPrototypeObject() const
    {
        return false;
    }

    virtual bool isNativeFunctionObject() const
    {
        return false;
    }

    virtual bool isScriptFunctionObject() const
    {
        return false;
    }

    virtual bool isScriptArrowFunctionObject() const
    {
        return false;
    }

    virtual bool isScriptGeneratorFunctionObject() const
    {
        return false;
    }

    virtual bool isScriptAsyncFunctionObject() const
    {
        return false;
    }

    virtual bool isScriptAsyncGeneratorFunctionObject() const
    {
        return false;
    }

    virtual bool isScriptClassConstructorFunctionObject() const
    {
        return false;
    }

    virtual bool isArrayObject() const
    {
        return false;
    }

    virtual bool isArrayPrototypeObject() const
    {
        return false;
    }

    virtual bool isStringObject() const
    {
        return false;
    }

    virtual bool isSymbolObject() const
    {
        return false;
    }

    virtual bool isNumberObject() const
    {
        return false;
    }

    virtual bool isBooleanObject() const
    {
        return false;
    }

    virtual bool isDateObject() const
    {
        return false;
    }

    virtual bool isDatePrototypeObject() const
    {
        return false;
    }

    virtual bool isRegExpObject()
    {
        return false;
    }

    virtual bool isRegExpPrototypeObject() const
    {
        return false;
    }

    virtual bool isErrorObject() const
    {
        return false;
    }

    virtual bool isGlobalObject() const
    {
        return false;
    }

    virtual bool isBoundFunctionObject() const
    {
        return false;
    }

    virtual bool isPromiseObject() const
    {
        return false;
    }

    virtual bool isProxyObject() const
    {
        return false;
    }

    virtual bool isArrayBufferView() const
    {
        return false;
    }

    virtual bool isDataViewObject() const
    {
        return false;
    }

    virtual bool isTypedArrayObject() const
    {
        return false;
    }

    virtual bool isArrayBufferObject() const
    {
        return false;
    }

    virtual bool isTypedArrayPrototypeObject() const
    {
        return false;
    }

    virtual bool isDoubleInSmallValue() const
    {
        return false;
    }

    virtual bool isJSGetterSetter() const
    {
        return false;
    }

    virtual bool isGlobalRegExpFunctionObject()
    {
        return false;
    }

    virtual bool isMapObject() const
    {
        return false;
    }

    virtual bool isSetObject() const
    {
        return false;
    }

    virtual bool isSetPrototypeObject() const
    {
        return false;
    }

    virtual bool isWeakMapObject() const
    {
        return false;
    }

    virtual bool isWeakSetObject() const
    {
        return false;
    }

    virtual bool isWeakSetPrototypeObject() const
    {
        return false;
    }

    virtual bool isIteratorObject() const
    {
        return false;
    }

    virtual bool isEnumerateObject() const
    {
        return false;
    }

    virtual bool isArrayIteratorObject() const
    {
        return false;
    }

    virtual bool isArrayIteratorPrototypeObject() const
    {
        return false;
    }

    virtual bool isStringIteratorObject() const
    {
        return false;
    }

    virtual bool isMapIteratorObject() const
    {
        return false;
    }

    virtual bool isSetIteratorObject() const
    {
        return false;
    }

    virtual bool isGeneratorObject() const
    {
        return false;
    }

    virtual bool isAsyncGeneratorObject() const
    {
        return false;
    }

    virtual bool isArgumentsObject() const
    {
        return false;
    }

    virtual bool isCallable() const
    {
        return false;
    }

    virtual bool isConstructor() const
    {
        return false;
    }

    String* asString()
    {
        ASSERT(isStringByVTable());
        return (String*)this;
    }

    Symbol* asSymbol()
    {
        ASSERT(isSymbolByVTable());
        return (Symbol*)this;
    }

    Object* asObject()
    {
        ASSERT(isObject());
        return (Object*)this;
    }

    ArrayObject* asArrayObject()
    {
        ASSERT(isArrayObject());
        return (ArrayObject*)this;
    }

    FunctionObject* asFunctionObject()
    {
        ASSERT(isFunctionObject());
        return (FunctionObject*)this;
    }

    NativeFunctionObject* asNativeFunctionObject()
    {
        ASSERT(isNativeFunctionObject());
        return (NativeFunctionObject*)this;
    }

    ScriptFunctionObject* asScriptFunctionObject()
    {
        ASSERT(isScriptFunctionObject());
        return (ScriptFunctionObject*)this;
    }

    ScriptArrowFunctionObject* asScriptArrowFunctionObject()
    {
        ASSERT(isScriptArrowFunctionObject());
        return (ScriptArrowFunctionObject*)this;
    }

    ScriptGeneratorFunctionObject* asScriptGeneratorFunctionObject()
    {
        ASSERT(isScriptGeneratorFunctionObject());
        return (ScriptGeneratorFunctionObject*)this;
    }

    ScriptAsyncFunctionObject* asScriptAsyncFunctionObject()
    {
        ASSERT(isScriptAsyncFunctionObject());
        return (ScriptAsyncFunctionObject*)this;
    }

    ScriptAsyncGeneratorFunctionObject* asScriptAsyncGeneratorFunctionObject()
    {
        ASSERT(isScriptAsyncGeneratorFunctionObject());
        return (ScriptAsyncGeneratorFunctionObject*)this;
    }

    ScriptClassConstructorFunctionObject* asScriptClassConstructorFunctionObject()
    {
        ASSERT(isScriptClassConstructorFunctionObject());
        return (ScriptClassConstructorFunctionObject*)this;
    }

    StringObject* asStringObject()
    {
        ASSERT(isStringObject());
        return (StringObject*)this;
    }

    SymbolObject* asSymbolObject()
    {
        ASSERT(isSymbolObject());
        return (SymbolObject*)this;
    }

    NumberObject* asNumberObject()
    {
        ASSERT(isNumberObject());
        return (NumberObject*)this;
    }

    BooleanObject* asBooleanObject()
    {
        ASSERT(isBooleanObject());
        return (BooleanObject*)this;
    }

    RegExpObject* asRegExpObject()
    {
        ASSERT(isRegExpObject());
        return (RegExpObject*)this;
    }

    DateObject* asDateObject()
    {
        ASSERT(isDateObject());
        return (DateObject*)this;
    }

    ErrorObject* asErrorObject()
    {
        ASSERT(isErrorObject());
        return (ErrorObject*)this;
    }

    GlobalObject* asGlobalObject()
    {
        ASSERT(isGlobalObject());
        return (GlobalObject*)this;
    }

    BoundFunctionObject* asBoundFunctionObject()
    {
        ASSERT(isBoundFunctionObject());
        return (BoundFunctionObject*)this;
    }

    PromiseObject* asPromiseObject()
    {
        ASSERT(isPromiseObject());
        return (PromiseObject*)this;
    }

    ProxyObject* asProxyObject()
    {
        ASSERT(isProxyObject());
        return (ProxyObject*)this;
    }

    ArrayBufferObject* asArrayBufferObject()
    {
        ASSERT(isArrayBufferObject());
        return (ArrayBufferObject*)this;
    }

    ArrayBufferView* asArrayBufferView()
    {
        ASSERT(isArrayBufferView());
        return (ArrayBufferView*)this;
    }

    DoubleInSmallValue* asDoubleInSmallValue()
    {
        ASSERT(isDoubleInSmallValue());
        return (DoubleInSmallValue*)this;
    }

    JSGetterSetter* asJSGetterSetter()
    {
        ASSERT(isJSGetterSetter());
        return (JSGetterSetter*)this;
    }

    GlobalRegExpFunctionObject* asGlobalRegExpFunctionObject()
    {
        ASSERT(isGlobalRegExpFunctionObject());
        return (GlobalRegExpFunctionObject*)this;
    }

    IteratorObject* asIteratorObject()
    {
        ASSERT(isIteratorObject());
        return (IteratorObject*)this;
    }

    MapObject* asMapObject()
    {
        ASSERT(isMapObject());
        return (MapObject*)this;
    }

    SetObject* asSetObject()
    {
        ASSERT(isSetObject());
        return (SetObject*)this;
    }

    WeakMapObject* asWeakMapObject()
    {
        ASSERT(isWeakMapObject());
        return (WeakMapObject*)this;
    }

    WeakSetObject* asWeakSetObject()
    {
        ASSERT(isWeakSetObject());
        return (WeakSetObject*)this;
    }

    GeneratorObject* asGeneratorObject()
    {
        ASSERT(isGeneratorObject());
        return (GeneratorObject*)this;
    }

    AsyncGeneratorObject* asAsyncGeneratorObject()
    {
        ASSERT(isAsyncGeneratorObject());
        return (AsyncGeneratorObject*)this;
    }

    bool hasTag(const size_t tag) const
    {
        return tag == *((size_t*)(this));
    }

    size_t getTag() const
    {
        return *((size_t*)(this));
    }

    bool hasTagInFirstDataArea(const size_t tag) const
    {
        return tag == *((size_t*)(this) + 1);
    }

    size_t getTagInFirstDataArea() const
    {
        return *((size_t*)(this) + 1);
    }

    bool isObjectByDataTag() const
    {
        return !(getTagInFirstDataArea() & (POINTER_VALUE_STRING_TAG_IN_DATA | POINTER_VALUE_SYMBOL_TAG_IN_DATA));
    }

    bool isStringByDataTag() const
    {
        return getTagInFirstDataArea() & POINTER_VALUE_STRING_TAG_IN_DATA;
    }

    bool isSymbolByDataTag() const
    {
        return getTagInFirstDataArea() & POINTER_VALUE_SYMBOL_TAG_IN_DATA;
    }

private:
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv);
    virtual Object* construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget);
};
}

#endif

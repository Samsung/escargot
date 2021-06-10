/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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
class BigInt;
class Object;
class FunctionObject;
class NativeFunctionObject;
class ExtendedNativeFunctionObject;
class ScriptFunctionObject;
class ScriptArrowFunctionObject;
class ScriptVirtualArrowFunctionObject;
class ScriptGeneratorFunctionObject;
class ScriptAsyncFunctionObject;
class ScriptAsyncGeneratorFunctionObject;
class ScriptClassConstructorFunctionObject;
class ArrayObject;
class StringObject;
class SymbolObject;
class BigIntObject;
class NumberObject;
class BooleanObject;
class RegExpObject;
class DataViewObject;
class DateObject;
class ErrorObject;
class GlobalObject;
class BoundFunctionObject;
class PromiseObject;
class ProxyObject;
class ArrayBufferObject;
class ArrayBufferView;
class DoubleInEncodedValue;
class JSGetterSetter;
class IteratorRecord;
class IteratorObject;
class MapObject;
class SetObject;
class WeakMapObject;
class WeakRefObject;
class WeakSetObject;
class FinalizationRegistryObject;
class GeneratorObject;
class AsyncGeneratorObject;
class AsyncFromSyncIteratorObject;
class GlobalObjectProxyObject;
class TypedArrayObject;
#if defined(ENABLE_THREADING)
class SharedArrayBufferObject;
#endif
#if defined(ENABLE_INTL)
class IntlLocaleObject;
class IntlPluralRulesObject;
class IntlRelativeTimeFormatObject;
#endif
#if defined(ENABLE_WASM)
class WASMModuleObject;
class WASMInstanceObject;
class WASMMemoryObject;
class WASMTableObject;
class WASMGlobalObject;
class ExportedFunctionObject;
#endif

#define POINTER_VALUE_STRING_TAG_IN_DATA 1
#define POINTER_VALUE_SYMBOL_TAG_IN_DATA 1 << 1
#define POINTER_VALUE_BIGINT_TAG_IN_DATA 1 << 2

#define POINTER_VALUE_NOT_OBJECT_TAG_IN_DATA (POINTER_VALUE_STRING_TAG_IN_DATA | POINTER_VALUE_SYMBOL_TAG_IN_DATA | POINTER_VALUE_BIGINT_TAG_IN_DATA)
// Finding the type of PointerValue operation is widely used during the runtime
// Only Object, String and Symbol are seen in regular runtime-code
// We can figure out fastly what the type of PointerValue by tag value
//   - Every data area of Object starts with [<vtable>, m_structure...]
//   - Every data area of String starts with [<vtable>, m_tag ...]
//   - Every data area of Symbol starts with [<vtable>, m_tag ...]
//   - Every data area of BigInt starts with [<vtable>, m_tag ...]
// Finding what is the type of PointerValue(Object, String, Symbol, BigInt) without accessing vtable gives better performance
// but, it uses more memory for String, Symbol type
// POINTER_VALUE_STRING_TAG_IN_DATA is not essential thing for implementing figure type(we can use isObject, isString)
// so, we can remove each m_tag value in very small device future

class PointerValue : public gc {
    friend class Object;
    friend class Context;
    friend class VMInstance;
    friend class ByteCodeInterpreter;

    // tag values for fast type check
    // these values actually have unique virtual table address of each object class
    static size_t g_arrayObjectTag;
    static size_t g_arrayPrototypeObjectTag;
    static size_t g_objectRareDataTag;
    static size_t g_doubleInEncodedValueTag;

public:
    virtual ~PointerValue() {}
    // fast type check with tag comparison
    inline bool isObject() const
    {
        return !(getTagInFirstDataArea() & POINTER_VALUE_NOT_OBJECT_TAG_IN_DATA);
    }

    inline bool isString() const
    {
        return getTagInFirstDataArea() & POINTER_VALUE_STRING_TAG_IN_DATA;
    }

    inline bool isSymbol() const
    {
        return getTagInFirstDataArea() & POINTER_VALUE_SYMBOL_TAG_IN_DATA;
    }

    inline bool isBigInt() const
    {
        return getTagInFirstDataArea() & POINTER_VALUE_BIGINT_TAG_IN_DATA;
    }

    inline bool isArrayObject() const
    {
        return hasTag(g_arrayObjectTag) || hasTag(g_arrayPrototypeObjectTag);
    }

    inline bool isArrayPrototypeObject() const
    {
        return hasTag(g_arrayPrototypeObjectTag);
    }

    inline bool isObjectRareData() const
    {
        return hasTag(g_objectRareDataTag);
    }

    inline bool isDoubleInEncodedValue() const
    {
        return hasTag(g_doubleInEncodedValueTag);
    }

    // type check by virtual function call
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

    virtual bool isExtendedNativeFunctionObject() const
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

    virtual bool isScriptVirtualArrowFunctionObject() const
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

    virtual bool isStringObject() const
    {
        return false;
    }

    virtual bool isSymbolObject() const
    {
        return false;
    }

    virtual bool isBigIntObject() const
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

    virtual bool isRegExpObject() const
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

    virtual bool isJSGetterSetter() const
    {
        return false;
    }

    virtual bool isIteratorRecord() const
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

    virtual bool isWeakMapObject() const
    {
        return false;
    }

    virtual bool isWeakRefObject() const
    {
        return false;
    }

    virtual bool isWeakSetObject() const
    {
        return false;
    }

    virtual bool isFinalizationRegistryObject() const
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

    virtual bool isRegExpStringIteratorObject() const
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

    virtual bool isAsyncFromSyncIteratorObject() const
    {
        return false;
    }

    virtual bool isGlobalObjectProxyObject() const
    {
        return false;
    }

    virtual bool isArgumentsObject() const
    {
        return false;
    }

#if defined(ENABLE_THREADING)
    virtual bool isSharedArrayBufferObject() const
    {
        return false;
    }
#endif

#if defined(ENABLE_INTL)
    virtual bool isIntlLocaleObject() const
    {
        return false;
    }

    virtual bool isIntlPluralRulesObject() const
    {
        return false;
    }

    virtual bool isIntlRelativeTimeFormatObject() const
    {
        return false;
    }
#endif

#if defined(ENABLE_WASM)
    virtual bool isWASMModuleObject() const
    {
        return false;
    }

    virtual bool isWASMInstanceObject() const
    {
        return false;
    }

    virtual bool isWASMMemoryObject() const
    {
        return false;
    }

    virtual bool isWASMTableObject() const
    {
        return false;
    }

    virtual bool isWASMGlobalObject() const
    {
        return false;
    }

    virtual bool isExportedFunctionObject() const
    {
        return false;
    }
#endif

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
        ASSERT(isString());
        return (String*)this;
    }

    Symbol* asSymbol()
    {
        ASSERT(isSymbol());
        return (Symbol*)this;
    }

    BigInt* asBigInt()
    {
        ASSERT(isBigInt());
        return (BigInt*)this;
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

    TypedArrayObject* asTypedArrayObject()
    {
        ASSERT(isTypedArrayObject());
        return (TypedArrayObject*)this;
    }


    ExtendedNativeFunctionObject* asExtendedNativeFunctionObject()
    {
        ASSERT(isExtendedNativeFunctionObject());
        return (ExtendedNativeFunctionObject*)this;
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

    ScriptVirtualArrowFunctionObject* asScriptVirtualArrowFunctionObject()
    {
        ASSERT(isScriptVirtualArrowFunctionObject());
        return (ScriptVirtualArrowFunctionObject*)this;
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

    BigIntObject* asBigIntObject()
    {
        ASSERT(isBigIntObject());
        return (BigIntObject*)this;
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

    DataViewObject* asDataViewObject()
    {
        ASSERT(isDataViewObject());
        return (DataViewObject*)this;
    }

    DoubleInEncodedValue* asDoubleInEncodedValue()
    {
        ASSERT(isDoubleInEncodedValue());
        return (DoubleInEncodedValue*)this;
    }

    JSGetterSetter* asJSGetterSetter()
    {
        ASSERT(isJSGetterSetter());
        return (JSGetterSetter*)this;
    }

    IteratorRecord* asIteratorRecord()
    {
        ASSERT(isIteratorRecord());
        return (IteratorRecord*)this;
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

    WeakRefObject* asWeakRefObject()
    {
        ASSERT(isWeakRefObject());
        return (WeakRefObject*)this;
    }

    WeakSetObject* asWeakSetObject()
    {
        ASSERT(isWeakSetObject());
        return (WeakSetObject*)this;
    }

    FinalizationRegistryObject* asFinalizationRegistryObject()
    {
        ASSERT(isFinalizationRegistryObject());
        return (FinalizationRegistryObject*)this;
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

    AsyncFromSyncIteratorObject* asAsyncFromSyncIteratorObject()
    {
        ASSERT(isAsyncFromSyncIteratorObject());
        return (AsyncFromSyncIteratorObject*)this;
    }

    GlobalObjectProxyObject* asGlobalObjectProxyObject()
    {
        ASSERT(isGlobalObjectProxyObject());
        return (GlobalObjectProxyObject*)this;
    }

#if defined(ENABLE_THREADING)
    SharedArrayBufferObject* asSharedArrayBufferObject()
    {
        ASSERT(isSharedArrayBufferObject());
        return (SharedArrayBufferObject*)this;
    }
#endif

#if defined(ENABLE_INTL)
    IntlLocaleObject* asIntlLocaleObject()
    {
        ASSERT(isIntlLocaleObject());
        return (IntlLocaleObject*)this;
    }

    IntlPluralRulesObject* asIntlPluralRulesObject()
    {
        ASSERT(isIntlPluralRulesObject());
        return (IntlPluralRulesObject*)this;
    }

    IntlRelativeTimeFormatObject* asIntlRelativeTimeFormatObject()
    {
        ASSERT(isIntlRelativeTimeFormatObject());
        return (IntlRelativeTimeFormatObject*)this;
    }
#endif

#if defined(ENABLE_WASM)
    WASMModuleObject* asWASMModuleObject()
    {
        ASSERT(isWASMModuleObject());
        return (WASMModuleObject*)this;
    }

    WASMInstanceObject* asWASMInstanceObject()
    {
        ASSERT(isWASMInstanceObject());
        return (WASMInstanceObject*)this;
    }

    WASMMemoryObject* asWASMMemoryObject()
    {
        ASSERT(isWASMMemoryObject());
        return (WASMMemoryObject*)this;
    }

    WASMTableObject* asWASMTableObject()
    {
        ASSERT(isWASMTableObject());
        return (WASMTableObject*)this;
    }

    WASMGlobalObject* asWASMGlobalObject()
    {
        ASSERT(isWASMGlobalObject());
        return (WASMGlobalObject*)this;
    }

    ExportedFunctionObject* asExportedFunctionObject()
    {
        ASSERT(isExportedFunctionObject());
        return (ExportedFunctionObject*)this;
    }
#endif

    inline size_t getTagInFirstDataArea() const
    {
        return *((size_t*)(this) + 1);
    }

private:
    inline bool hasTag(const size_t tag) const
    {
        ASSERT(!!tag);
        return tag == *((size_t*)(this));
    }

    inline size_t getTag() const
    {
        return *((size_t*)(this));
    }

    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, Value* argv);
    virtual Value construct(ExecutionState& state, const size_t argc, Value* argv, Object* newTarget);
};
} // namespace Escargot

#endif

/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef __ESCARGOT_PUBLIC__
#define __ESCARGOT_PUBLIC__

#include <string>
#include <cstdlib>
#include <vector>
#include <functional>
#include <limits>

#include <GCUtil.h>

#pragma GCC visibility push(default)

namespace Escargot {

class VMInstanceRef;
class StringRef;
class ValueRef;
class PointerValueRef;
class ObjectRef;
class GlobalObjectRef;
class FunctionObjectRef;
class ArrayObjectRef;
class ArrayBufferObjectRef;
class ArrayBufferViewRef;
class PromiseObjectRef;
class ErrorObjectRef;
class DateObjectRef;
class StringObjectRef;
class NumberObjectRef;
class BooleanObjectRef;
class RegExpObjectRef;
class ScriptRef;
class ScriptParserRef;
class ExecutionStateRef;
class ValueVectorRef;
class JobRef;

class Globals {
public:
    static void initialize(bool applyMallOpt = false, bool applyGcOpt = false);
    static void finalize();
};

// `double` value is not presented in PointerValue, but it is stored in heap
class PointerValueRef {
public:
    bool isString();
    StringRef* asString();
    bool isObject();
    ObjectRef* asObject();
    bool isFunctionObject();
    FunctionObjectRef* asFunctionObject();
    bool isArrayObject();
    ArrayObjectRef* asArrayObject();
    bool isStringObject();
    StringObjectRef* asStringObject();
    bool isNumberObject();
    NumberObjectRef* asNumberObject();
    bool isBooleanObject();
    BooleanObjectRef* asBooleanObject();
    bool isRegExpObject();
    RegExpObjectRef* asRegExpObject();
    bool isDateObject();
    DateObjectRef* asDateObject();
    bool isGlobalObject();
    GlobalObjectRef* asGlobalObject();
    bool isErrorObject();
    ErrorObjectRef* asErrorObject();
#if ESCARGOT_ENABLE_TYPEDARRAY
    bool isArrayBufferObject();
    ArrayBufferObjectRef* asArrayBufferObject();
    bool isArrayBufferView();
    ArrayBufferViewRef* asArrayBufferView();
#endif
#if ESCARGOT_ENABLE_PROMISE
    bool isPromiseObject();
    PromiseObjectRef* asPromiseObject();
#endif
};

class StringRef : public PointerValueRef {
public:
    static StringRef* fromASCII(const char* s);
    static StringRef* fromASCII(const char* s, size_t len);
    static StringRef* fromUTF8(const char* s, size_t len);
    static StringRef* fromUTF16(const char16_t* s, size_t len);
    static StringRef* emptyString();

    char16_t charAt(size_t idx);
    size_t length();
    bool equals(StringRef* src);

    std::string toStdUTF8String();
};

class VMInstanceRef {
public:
    static VMInstanceRef* create(const char* locale = nullptr, const char* timezone = nullptr);
    void destroy();

    void clearCachesRelatedWithContext();
    bool addRoot(VMInstanceRef* instanceRef, ValueRef* ptr);
    bool removeRoot(VMInstanceRef* instanceRef, ValueRef* ptr);

#ifdef ESCARGOT_ENABLE_PROMISE
    // if there is an error, executing will be stopped and returns ErrorValue
    // if thres is no job or no error, returns EmptyValue
    ValueRef* drainJobQueue(ExecutionStateRef* state);

    typedef void (*NewPromiseJobListener)(ExecutionStateRef* state, JobRef* job);
    void setNewPromiseJobListener(NewPromiseJobListener l);
#endif
};

class ContextRef {
public:
    static ContextRef* create(VMInstanceRef* vmInstance);
    void destroy();

    ScriptParserRef* scriptParser();
    GlobalObjectRef* globalObject();
    VMInstanceRef* vmInstance();

    typedef ValueRef* (*VirtualIdentifierCallback)(ExecutionStateRef* state, ValueRef* name);

    // this is not compatible with ECMAScript
    // but this callback is needed for browser-implementation
    // if there is a Identifier with that value, callback should return non-empty value
    void setVirtualIdentifierCallback(VirtualIdentifierCallback cb);
    VirtualIdentifierCallback virtualIdentifierCallback();
};

class AtomicStringRef {
public:
    static AtomicStringRef* create(ContextRef* c, const char* src); // from ASCII string
    static AtomicStringRef* create(ContextRef* c, StringRef* src);
    static AtomicStringRef* emptyAtomicString();
    StringRef* string();
};

class ExecutionStateRef {
public:
    // this can not create sandbox
    // use this function only non-exececption area
    static ExecutionStateRef* create(ContextRef* ctx);
    void destroy();

    FunctionObjectRef* resolveCallee(); // resolve nearest callee if exists
    std::vector<std::pair<FunctionObjectRef*, ValueRef*>> resolveCallstack(); // resolve callee, this value

    void throwException(ValueRef* value);

    ContextRef* context();
};

// double, PointerValueRef are stored in heap
// client should root heap values
class ValueRef {
public:
    union PublicValueDescriptor {
        int64_t asInt64;
    };

    static ValueRef* create(bool);
    static ValueRef* create(int);
    static ValueRef* create(unsigned);
    static ValueRef* create(float);
    static ValueRef* create(double);
    static ValueRef* create(long);
    static ValueRef* create(unsigned long);
    static ValueRef* create(long long);
    static ValueRef* create(unsigned long long);
    static ValueRef* create(PointerValueRef* value)
    {
        return reinterpret_cast<ValueRef*>(value);
    }
    static ValueRef* create(ValueRef* value)
    {
        return reinterpret_cast<ValueRef*>(value);
    }
    static ValueRef* createNull();
    static ValueRef* createUndefined();
    static ValueRef* createEmpty();

    bool isStoreInHeap() const;
    bool isBoolean() const;
    bool isNumber() const;
    bool isNull() const;
    bool isUndefined() const;
    bool isInt32() const;
    bool isUInt32() const;
    bool isDouble() const;
    bool isTrue() const;
    bool isFalse() const;
    bool isEmpty() const;
    bool isString() const;
    bool isObject() const;
    bool isFunction() const;
    bool isUndefinedOrNull() const
    {
        return isUndefined() || isNull();
    }

    bool toBoolean(ExecutionStateRef* state);
    double toNumber(ExecutionStateRef* state);
    double toInteger(ExecutionStateRef* state);
    double toLength(ExecutionStateRef* state);
    int32_t toInt32(ExecutionStateRef* state);
    uint32_t toUint32(ExecutionStateRef* state);
    StringRef* toString(ExecutionStateRef* state);
    ObjectRef* toObject(ExecutionStateRef* state);

    enum { InvalidIndexValue = std::numeric_limits<uint32_t>::max() };
    typedef uint64_t ValueIndex;
    ValueIndex toIndex(ExecutionStateRef* state);

    enum { InvalidArrayIndexValue = std::numeric_limits<uint32_t>::max() };
    uint32_t toArrayIndex(ExecutionStateRef* state);

    bool asBoolean();
    double asNumber();
    int32_t asInt32();
    uint32_t asUint32();
    StringRef* asString();
    ObjectRef* asObject();
    FunctionObjectRef* asFunction();

protected:
};

class ValueVectorRef {
public:
    static ValueVectorRef* create(size_t size = 0);

    size_t size();
    void pushBack(ValueRef* val);
    void insert(size_t pos, ValueRef* val);
    void erase(size_t pos);
    void erase(size_t start, size_t end);
    ValueRef* at(const size_t& idx);
    void set(const size_t& idx, ValueRef* newValue);
    void resize(size_t newSize);
};

struct ExposableObjectGetOwnPropertyCallbackResult {
    ExposableObjectGetOwnPropertyCallbackResult()
    {
        m_value = ValueRef::createEmpty();
        m_isWritable = m_isEnumerable = m_isConfigurable = false;
    }

    ExposableObjectGetOwnPropertyCallbackResult(ValueRef* value, bool isWritable, bool isEnumerable, bool isConfigurable)
    {
        m_value = value;
        m_isWritable = isWritable;
        m_isEnumerable = isEnumerable;
        m_isConfigurable = isConfigurable;
    }

    ValueRef* m_value;
    bool m_isWritable;
    bool m_isEnumerable;
    bool m_isConfigurable;
};

struct ExposableObjectEnumerationCallbackResult {
    ExposableObjectEnumerationCallbackResult(ValueRef* name, bool isWritable, bool isEnumerable, bool isConfigurable)
    {
        m_name = name;
        m_isWritable = isWritable;
        m_isEnumerable = isEnumerable;
        m_isConfigurable = isConfigurable;
    }

    ExposableObjectEnumerationCallbackResult() {} // for std vector
    ValueRef* m_name;
    bool m_isWritable;
    bool m_isEnumerable;
    bool m_isConfigurable;
};

typedef ExposableObjectGetOwnPropertyCallbackResult (*ExposableObjectGetOwnPropertyCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* propertyName);
typedef bool (*ExposableObjectDefineOwnPropertyCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* propertyName, ValueRef* value);
typedef std::vector<ExposableObjectEnumerationCallbackResult, GCUtil::gc_malloc_ignore_off_page_allocator<ExposableObjectEnumerationCallbackResult>> ExposableObjectEnumerationCallbackResultVector;
typedef ExposableObjectEnumerationCallbackResultVector (*ExposableObjectEnumerationCallback)(ExecutionStateRef* state, ObjectRef* self);
typedef bool (*ExposableObjectDeleteOwnPropertyCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* propertyName);

class ObjectRef : public PointerValueRef {
public:
    static ObjectRef* create(ExecutionStateRef* state);
    // can not redefine or delete virtual property
    // virtual property does not follow every rule of ECMAScript
    static ObjectRef* createExposableObject(ExecutionStateRef* state,
                                            ExposableObjectGetOwnPropertyCallback getOwnPropertyCallback, ExposableObjectDefineOwnPropertyCallback defineOwnPropertyCallback,
                                            ExposableObjectEnumerationCallback enumerationCallback, ExposableObjectDeleteOwnPropertyCallback deleteOwnPropertyCallback);

    ValueRef* get(ExecutionStateRef* state, ValueRef* propertyName);
    ValueRef* getOwnProperty(ExecutionStateRef* state, ValueRef* propertyName);
    ValueRef* getOwnPropertyDescriptor(ExecutionStateRef* state, ValueRef* propertyName);

    bool set(ExecutionStateRef* state, ValueRef* propertyName, ValueRef* value);

    enum PresentAttribute {
        NotPresent = 0,
        WritablePresent = 1 << 1,
        EnumerablePresent = 1 << 2,
        ConfigurablePresent = 1 << 3,
        NonWritablePresent = 1 << 4,
        NonEnumerablePresent = 1 << 5,
        NonConfigurablePresent = 1 << 6,
        AllPresent = WritablePresent | EnumerablePresent | ConfigurablePresent
    };

    class DataPropertyDescriptor {
    public:
        DataPropertyDescriptor(ValueRef* value, PresentAttribute attr)
            : m_value(value)
            , m_attribute(attr)
        {
        }

        ValueRef* m_value;
        PresentAttribute m_attribute;
    };

    class AccessorPropertyDescriptor {
    public:
        // only undefined, empty, function are allowed to getter, setter parameter
        // empty means unspecified state
        // undefined means truely undefined -> ex) { get: undefined }
        AccessorPropertyDescriptor(ValueRef* getter, ValueRef* setter, PresentAttribute attr)
            : m_getter(getter)
            , m_setter(setter)
            , m_attribute(attr)
        {
        }

        ValueRef* m_getter;
        ValueRef* m_setter;
        PresentAttribute m_attribute;
    };

    bool defineDataProperty(ExecutionStateRef* state, ValueRef* propertyName, const DataPropertyDescriptor& desc);
    bool defineDataProperty(ExecutionStateRef* state, ValueRef* propertyName, ValueRef* value, bool isWritable, bool isEnumerable, bool isConfigurable);
    bool defineAccessorProperty(ExecutionStateRef* state, ValueRef* propertyName, const AccessorPropertyDescriptor& desc);

    struct NativeDataAccessorPropertyData;
    typedef ValueRef* (*NativeDataAccessorPropertyGetter)(ExecutionStateRef* state, ObjectRef* self, NativeDataAccessorPropertyData* data);
    typedef bool (*NativeDataAccessorPropertySetter)(ExecutionStateRef* state, ObjectRef* self, NativeDataAccessorPropertyData* data, ValueRef* setterInputData);
    // client extend this struct to give data for getter, setter if needs
    // this struct must allocated in gc-heap
    // only setter can be null
    struct NativeDataAccessorPropertyData {
        bool m_isWritable : 1;
        bool m_isEnumerable : 1;
        bool m_isConfigurable : 1;
        NativeDataAccessorPropertyGetter m_getter;
        NativeDataAccessorPropertySetter m_setter;

        NativeDataAccessorPropertyData(bool isWritable, bool isEnumerable, bool isConfigurable,
                                       NativeDataAccessorPropertyGetter getter, NativeDataAccessorPropertySetter setter)
            : m_isWritable(isWritable)
            , m_isEnumerable(isEnumerable)
            , m_isConfigurable(isConfigurable)
            , m_getter(getter)
            , m_setter(setter)
        {
        }

        void* operator new(size_t size);
        void* operator new[](size_t size) = delete;
    };
    // this function differ with defineDataPropety and defineAccessorPropety.
    // !hasOwnProperty(state, P) is needed for success
    bool defineNativeDataAccessorProperty(ExecutionStateRef* state, ValueRef* P, NativeDataAccessorPropertyData* data);

    bool deleteOwnProperty(ExecutionStateRef* state, ValueRef* propertyName);
    bool hasOwnProperty(ExecutionStateRef* state, ValueRef* propertyName);

    ValueRef* getPrototype(ExecutionStateRef* state);
    ObjectRef* getPrototypeObject(); // if __proto__ is not object(undefined or null), this function returns nullptr instead of orginal value.
    void setPrototype(ExecutionStateRef* state, ValueRef* value);

    ValueVectorRef* getOwnPropertyKeys(ExecutionStateRef* state);

    bool isExtensible();
    void preventExtensions();

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    const char* internalClassProperty();
    void giveInternalClassProperty(const char* name);

    void* extraData();
    void setExtraData(void* e);

    void removeFromHiddenClassChain(ExecutionStateRef* state);
};

class GlobalObjectRef : public ObjectRef {
public:
    FunctionObjectRef* object();
    ObjectRef* objectPrototype();
    FunctionObjectRef* objectPrototypeToString();
    FunctionObjectRef* function();
    FunctionObjectRef* functionPrototype();
    FunctionObjectRef* error();
    ObjectRef* errorPrototype();
    FunctionObjectRef* referenceError();
    ObjectRef* referenceErrorPrototype();
    FunctionObjectRef* typeError();
    ObjectRef* typeErrorPrototype();
    FunctionObjectRef* rangeError();
    ObjectRef* rangeErrorPrototype();
    FunctionObjectRef* syntaxError();
    ObjectRef* syntaxErrorPrototype();
    FunctionObjectRef* uriError();
    ObjectRef* uriErrorPrototype();
    FunctionObjectRef* evalError();
    ObjectRef* evalErrorPrototype();
    FunctionObjectRef* string();
    ObjectRef* stringPrototype();
    FunctionObjectRef* number();
    ObjectRef* numberPrototype();
    FunctionObjectRef* array();
    ObjectRef* arrayPrototype();
    FunctionObjectRef* boolean();
    ObjectRef* booleanPrototype();
    FunctionObjectRef* date();
    ObjectRef* datePrototype();
    ObjectRef* math();
    FunctionObjectRef* regexp();
    ObjectRef* regexpPrototype();
    ObjectRef* json();
    FunctionObjectRef* jsonStringify();
    FunctionObjectRef* jsonParse();

#if ESCARGOT_ENABLE_PROMISE
    FunctionObjectRef* promise();
    ObjectRef* promisePrototype();
#endif

#if ESCARGOT_ENABLE_TYPEDARRAY
    FunctionObjectRef* arrayBuffer();
    ObjectRef* arrayBufferPrototype();
    FunctionObjectRef* dataView();
    ObjectRef* dataViewPrototype();
    ObjectRef* int8Array();
    ObjectRef* int8ArrayPrototype();
    ObjectRef* uint8Array();
    ObjectRef* uint8ArrayPrototype();
    ObjectRef* int16Array();
    ObjectRef* int16ArrayPrototype();
    ObjectRef* uint16Array();
    ObjectRef* uint16ArrayPrototype();
    ObjectRef* int32Array();
    ObjectRef* int32ArrayPrototype();
    ObjectRef* uint32Array();
    ObjectRef* uint32ArrayPrototype();
    ObjectRef* uint8ClampedArray();
    ObjectRef* uint8ClampedArrayPrototype();
    ObjectRef* float32Array();
    ObjectRef* float32ArrayPrototype();
    ObjectRef* float64Array();
    ObjectRef* float64ArrayPrototype();
#endif
};

class FunctionObjectRef : public ObjectRef {
public:
    typedef ValueRef* (*NativeFunctionPointer)(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isNewExpression);
    typedef ObjectRef* (*NativeFunctionConstructor)(ExecutionStateRef* state, size_t argc, ValueRef** argv);

    struct NativeFunctionInfo {
        bool m_isStrict;
        bool m_isConstructor;
        AtomicStringRef* m_name;
        NativeFunctionPointer m_nativeFunction;
        NativeFunctionConstructor m_nativeFunctionConstructor;
        size_t m_argumentCount;

        NativeFunctionInfo(AtomicStringRef* name, NativeFunctionPointer fn, size_t argc, NativeFunctionConstructor ctor = nullptr, bool isStrict = true, bool isConstructor = true)
            : m_isStrict(isStrict)
            , m_isConstructor(isConstructor)
            , m_name(name)
            , m_nativeFunction(fn)
            , m_nativeFunctionConstructor(ctor)
            , m_argumentCount(argc)
        {
        }
    };

    static FunctionObjectRef* create(ExecutionStateRef* state, NativeFunctionInfo info);
    static FunctionObjectRef* createBuiltinFunction(ExecutionStateRef* state, NativeFunctionInfo info);

    // getter of internal [[Prototype]]
    ValueRef* getFunctionPrototype(ExecutionStateRef* state);
    // setter of internal [[Prototype]]
    bool setFunctionPrototype(ExecutionStateRef* state, ValueRef* v);

    bool isConstructor();
    ValueRef* call(ExecutionStateRef* state, ValueRef* receiver, const size_t& argc, ValueRef** argv);
    // ECMAScript new operation
    ObjectRef* newInstance(ExecutionStateRef* state, const size_t& argc, ValueRef** argv);

    void markFunctionNeedsSlowVirtualIdentifierOperation();
};

class ArrayObjectRef : public ObjectRef {
public:
    static ArrayObjectRef* create(ExecutionStateRef* state);
};

class ErrorObjectRef : public ObjectRef {
public:
    enum Code {
        None,
        ReferenceError,
        TypeError,
        SyntaxError,
        RangeError,
        URIError,
        EvalError
    };
    static ErrorObjectRef* create(ExecutionStateRef* state, ErrorObjectRef::Code code, StringRef* errorMessage);

protected:
};

class ReferenceErrorObjectRef : public ErrorObjectRef {
public:
    static ReferenceErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
};

class TypeErrorObjectRef : public ErrorObjectRef {
public:
    static TypeErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
};

class SyntaxErrorObjectRef : public ErrorObjectRef {
public:
    static SyntaxErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
};

class RangeErrorObjectRef : public ErrorObjectRef {
public:
    static RangeErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
};

class URIErrorObjectRef : public ErrorObjectRef {
public:
    static URIErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
};

class EvalErrorObjectRef : public ErrorObjectRef {
public:
    static EvalErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
};

class DateObjectRef : public ObjectRef {
public:
    static DateObjectRef* create(ExecutionStateRef* state);
    void setTimeValue(ExecutionStateRef* state, ValueRef* str);
    void setTimeValue(int64_t value);
    StringRef* toUTCString(ExecutionStateRef* state);
    double primitiveValue();
};

class StringObjectRef : public ObjectRef {
public:
    static StringObjectRef* create(ExecutionStateRef* state);

    void setPrimitiveValue(ExecutionStateRef* state, ValueRef* value);
    StringRef* primitiveValue();
};

class NumberObjectRef : public ObjectRef {
public:
    static NumberObjectRef* create(ExecutionStateRef* state);

    void setPrimitiveValue(ExecutionStateRef* state, ValueRef* value);
    double primitiveValue();
};

class BooleanObjectRef : public ObjectRef {
public:
    static BooleanObjectRef* create(ExecutionStateRef* state);

    void setPrimitiveValue(ExecutionStateRef* state, ValueRef* value);
    bool primitiveValue();
};

class RegExpObjectRef : public ObjectRef {
public:
    enum RegExpObjectOption {
        None = 1,
        Global = 1 << 1,
        IgnoreCase = 1 << 2,
        MultiLine = 1 << 3,
        // NOTE(ES6): Sticky and Unicode option is added in ES6
        Sticky = 1 << 4,
        Unicode = 1 << 5,
    };

    static RegExpObjectRef* create(ExecutionStateRef* state, ValueRef* source, ValueRef* option);
    StringRef* source();
    RegExpObjectOption option();
};

#ifdef ESCARGOT_ENABLE_TYPEDARRAY
class ArrayBufferObjectRef : public ObjectRef {
public:
    typedef void* (*ArrayBufferObjectBufferMallocFunction)(size_t siz);
    typedef void (*ArrayBufferObjectBufferFreeFunction)(void* buffer);

    static void setMallocFunction(ArrayBufferObjectBufferMallocFunction fn);
    static void setFreeFunction(ArrayBufferObjectBufferFreeFunction fn);
    static void setMallocFunctionNeedsZeroFill(bool n);

    static ArrayBufferObjectRef* create(ExecutionStateRef* state);
    void allocateBuffer(size_t bytelength);
    void attachBuffer(void* buffer, size_t bytelength);
    void detachArrayBuffer();
    uint8_t* rawBuffer();
    unsigned bytelength();
};

class ArrayBufferViewRef : public ObjectRef {
public:
    ArrayBufferObjectRef* buffer();
    uint8_t* rawBuffer();
    unsigned bytelength();
};

#endif

#ifdef ESCARGOT_ENABLE_PROMISE
class PromiseObjectRef : public ObjectRef {
public:
    static PromiseObjectRef* create(ExecutionStateRef* state);
    void fulfill(ExecutionStateRef* state, ValueRef* value);
    void reject(ExecutionStateRef* state, ValueRef* reason);
};
#endif

class SandBoxRef {
public:
    static SandBoxRef* create(ContextRef* ctxRef);
    void destroy();

    struct LOC {
        size_t line;
        size_t column;
        size_t index;

        LOC(size_t line, size_t column, size_t index)
        {
            this->line = line;
            this->column = column;
            this->index = index;
        }
    };

    struct StackTraceData {
        StringRef* fileName;
        LOC loc;
        StackTraceData();
    };

    struct SandBoxResult {
        ValueRef* result;
        ValueRef* error;
        StringRef* msgStr;
        std::vector<StackTraceData, GCUtil::gc_malloc_allocator<StackTraceData>> stackTraceData;
        SandBoxResult();
    };

    SandBoxResult run(const std::function<ValueRef*(ExecutionStateRef* state)>& scriptRunner); // for capsule script executing with try-catch
};

class JobRef {
public:
    SandBoxRef::SandBoxResult run(ExecutionStateRef* state);
};

class ScriptParserRef {
public:
    struct ScriptParserResult {
        ScriptParserResult(ScriptRef* script, StringRef* error)
            : m_script(script)
            , m_error(error)
        {
        }

        ScriptRef* m_script;
        StringRef* m_error;
    };

    ScriptParserResult parse(StringRef* script, StringRef* fileName);
};

class ScriptRef {
public:
    ValueRef* execute(ExecutionStateRef* state);
};

} // namespace Escargot

#pragma GCC visibility pop

#endif

/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
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

#include <GCUtil.h>

#pragma GCC visibility push(default)

namespace Escargot {

class VMInstanceRef;
class ValueRef;
class PointerValueRef;
class ObjectRef;
class GlobalObjectRef;
class FunctionObjectRef;
class ScriptRef;
class ScriptParserRef;
class ExecutionStateRef;

class Globals {
public:
    static void initialize(bool applyMallOpt = false, bool applyGcOpt = false);
    static void finalize();
};

class PointerValueRef {
};

class StringRef : public PointerValueRef {
public:
    static StringRef* fromASCII(const char* s);
    static StringRef* fromASCII(const char* s, size_t len);
    static StringRef* fromUTF8(const char* s, size_t len);
    static StringRef* emptyString();

    char16_t charAt(size_t idx);
    size_t length();
    bool equals(StringRef* src);

    std::string toStdUTF8String();
};

class VMInstanceRef {
public:
    static VMInstanceRef* create();
    void destroy();

    bool addRoot(VMInstanceRef* instanceRef, ValueRef* ptr);
    bool removeRoot(VMInstanceRef* instanceRef, ValueRef* ptr);

#ifdef ESCARGOT_ENABLE_PROMISE
    // if there is an error, executing will be stopped and returns ErrorValue
    // if thres is no job or no error, returns EmptyValue
    ValueRef* drainJobQueue(ExecutionStateRef* state);
#endif
};

class ContextRef {
public:
    static ContextRef* create(VMInstanceRef* vmInstance);
    void destroy();

    ScriptParserRef* scriptParser();
    GlobalObjectRef* globalObject();

    typedef ValueRef* (*VirtualIdentifierCallback)(ExecutionStateRef* state, ValueRef* name);

    // this is not compatible with ECMAScript
    // but this callback is needed for browser-implementation
    // if there is a Identifier with that value, callback should return non-empty value
    void setVirtualIdentifierCallback(VirtualIdentifierCallback cb);
    VirtualIdentifierCallback virtualIdentifierCallback();
    void setVirtualIdentifierInGlobalCallback(VirtualIdentifierCallback cb);
    VirtualIdentifierCallback virtualIdentifierInGlobalCallback();
};

class AtomicStringRef {
public:
    static AtomicStringRef* create(ContextRef* c, const char* src); // from ASCII string
    static AtomicStringRef* create(ContextRef* c, StringRef* src);
    StringRef* string();
};

class ExecutionStateRef {
public:
    static ExecutionStateRef* create(ContextRef* ctx);
    void destroy();

    ContextRef* context();
};

// double, PointerValueRef are stored in heap
// client should root heap values
class ValueRef {
public:
    union PublicValueDescriptor {
        int64_t asInt64;
    };

    static ValueRef* create(bool value);
    static ValueRef* create(int32_t value);
    static ValueRef* create(uint32_t value);
    static ValueRef* create(double value);
    static ValueRef* create(PointerValueRef* value);
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

    bool toBoolean(ExecutionStateRef* es);
    double toNumber(ExecutionStateRef* es);
    double toInteger(ExecutionStateRef* es);
    double toLength(ExecutionStateRef* es);
    int32_t toInt32(ExecutionStateRef* es);
    uint32_t toUint32(ExecutionStateRef* es);
    StringRef* toString(ExecutionStateRef* es);
    ObjectRef* toObject(ExecutionStateRef* es);

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

// return EmptyValue means there is no property with that name in this object
typedef ValueRef* (*ExposableObjectGetOwnPropertyCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* propertyName);
typedef void (*ExposableObjectDefineOwnPropertyCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* propertyName, ValueRef* value);
typedef ValueVectorRef* (*ExposableObjectEnumerationCallback)(ExecutionStateRef* state, ObjectRef* self);

class ObjectRef : public PointerValueRef {
public:
    static ObjectRef* create(ExecutionStateRef* state);
    // can not redefine or delete virtual property
    // virtual property does not follow every rule of ECMAScript
    static ObjectRef* createExposableObject(ExecutionStateRef* state,
                                            ExposableObjectGetOwnPropertyCallback getOwnPropertyCallback, ExposableObjectDefineOwnPropertyCallback defineOwnPropertyCallback,
                                            ExposableObjectEnumerationCallback enumerationCallback, bool isWritable, bool isEnumerable, bool isConfigurable);

    ValueRef* get(ExecutionStateRef* state, ValueRef* propertyName);
    ValueRef* getOwnProperty(ExecutionStateRef* state, ValueRef* propertyName);
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

    bool defineDataPropety(ExecutionStateRef* state, ValueRef* propertyName, const DataPropertyDescriptor& desc);
    bool defineAccessorPropety(ExecutionStateRef* state, ValueRef* propertyName, const AccessorPropertyDescriptor& desc);

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

    bool isExtensible();
    void preventExtensions();

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    const char* internalClassProperty();
    void giveInternalClassProperty(const char* name);

    void* extraData();
    void setExtraData(void* e);
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

    // getter of internal [[Prototype]]
    ValueRef* getFunctionPrototype(ExecutionStateRef* state);
    // setter of internal [[Prototype]]
    bool setFunctionPrototype(ExecutionStateRef* state, ValueRef* v);

    bool isConstructor();
    ValueRef* call(ExecutionStateRef* state, ValueRef* receiver, const size_t& argc, ValueRef** argv);
    // ECMAScript new operation
    ObjectRef* newInstance(ExecutionStateRef* state, const size_t& argc, ValueRef** argv);
};

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

/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotTypedArrayObject__
#define __EscargotTypedArrayObject__

#include "runtime/Context.h"
#include "runtime/Object.h"
#include "runtime/ErrorObject.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/Context.h"
#include "util/Util.h"

namespace Escargot {

#define FOR_EACH_TYPEDARRAY(F)        \
    F(Int8, int8, 1);                 \
    F(Int16, int16, 2);               \
    F(Int32, int32, 4);               \
    F(Uint8, uint8, 1);               \
    F(Uint8Clamped, uint8Clamped, 1); \
    F(Uint16, uint16, 2);             \
    F(Uint32, uint32, 4);             \
    F(Float32, float32, 4);           \
    F(Float64, float64, 8);

class ArrayBufferView : public Object {
public:
    explicit ArrayBufferView(ExecutionState& state, Object* proto)
        : Object(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER)
        , m_buffer(nullptr)
        , m_rawBuffer(nullptr)
        , m_byteLength(0)
        , m_byteOffset(0)
        , m_arrayLength(0)
    {
    }

    virtual TypedArrayType typedArrayType()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual String* typedArrayName(ExecutionState& state)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    ALWAYS_INLINE ArrayBufferObject* buffer() { return m_buffer; }
    ALWAYS_INLINE uint8_t* rawBuffer() { return m_rawBuffer; }
    ALWAYS_INLINE unsigned byteLength() { return m_byteLength; }
    ALWAYS_INLINE unsigned byteOffset() { return m_byteOffset; }
    ALWAYS_INLINE unsigned arrayLength() { return m_arrayLength; }
    ALWAYS_INLINE void setBuffer(ArrayBufferObject* bo, unsigned byteOffset, unsigned byteLength, unsigned arrayLength)
    {
        m_buffer = bo;
        m_byteOffset = byteOffset;
        m_byteLength = byteLength;
        m_arrayLength = arrayLength;
        m_rawBuffer = bo ? (uint8_t*)(bo->data() + m_byteOffset) : nullptr;
    }

    ALWAYS_INLINE void setBuffer(ArrayBufferObject* bo, unsigned byteOffset, unsigned byteLength)
    {
        m_buffer = bo;
        m_byteOffset = byteOffset;
        m_byteLength = byteLength;
        m_rawBuffer = bo ? (uint8_t*)(bo->data() + m_byteOffset) : nullptr;
    }

    virtual bool isArrayBufferView() const
    {
        return true;
    }

    static int getElementSize(TypedArrayType type)
    {
        switch (type) {
        case TypedArrayType::Int8:
        case TypedArrayType::Uint8:
        case TypedArrayType::Uint8Clamped:
            return 1;
        case TypedArrayType::Int16:
        case TypedArrayType::Uint16:
            return 2;
        case TypedArrayType::Int32:
        case TypedArrayType::Uint32:
        case TypedArrayType::Float32:
            return 4;
        case TypedArrayType::Float64:
            return 8;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    template <typename Type>
    Value getValueFromBuffer(ExecutionState& state, unsigned byteindex, bool isLittleEndian = 1)
    {
        // If isLittleEndian is not present, set isLittleEndian to either true or false.
        ASSERT(byteLength());
        size_t elementSize = sizeof(Type);
        ASSERT(byteindex + elementSize <= byteLength());
        uint8_t* rawStart = rawBuffer() + byteindex;
        Type res;
        if (isLittleEndian != 1) { // bigEndian
            for (size_t i = 0; i < elementSize; i++) {
                ((uint8_t*)&res)[elementSize - i - 1] = rawStart[i];
            }
        } else { // littleEndian
            res = *((Type*)rawStart);
        }
        return Value(res);
    }

    template <typename Type>
    void setValueInBuffer(ExecutionState& state, unsigned byteindex, const Value& val, bool isLittleEndian = 1)
    {
        // If isLittleEndian is not present, set isLittleEndian to either true or false.
        ASSERT(byteLength());
        size_t elementSize = sizeof(typename Type::Type);
        ASSERT(byteindex + elementSize <= byteLength());
        uint8_t* rawStart = rawBuffer() + byteindex;
        typename Type::Type littleEndianVal = Type::toNative(state, val);
        if (isLittleEndian != 1) {
            for (size_t i = 0; i < elementSize; i++) {
                rawStart[i] = ((uint8_t*)&littleEndianVal)[elementSize - i - 1];
            }
        } else {
            *((typename Type::Type*)rawStart) = littleEndianVal;
        }
    }

    void* operator new(size_t size)
    {
        static bool typeInited = false;
        static GC_descr descr;
        if (!typeInited) {
            GC_word obj_bitmap[GC_BITMAP_SIZE(ArrayBufferView)] = { 0 };
            GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferView, m_structure));
            GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferView, m_prototype));
            GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferView, m_values));
            GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferView, m_buffer));
            descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArrayBufferView));
            typeInited = true;
        }
        return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
    }
    void* operator new[](size_t size) = delete;

    ALWAYS_INLINE bool isDetached()
    {
        return m_buffer->isDetachedBuffer();
    }

    ALWAYS_INLINE void throwTypeErrorIfDetached(ExecutionState& state)
    {
        if (UNLIKELY(isDetached())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().constructor.string(), ErrorObject::Messages::GlobalObject_DetachedBuffer);
        }
    }

private:
    ArrayBufferObject* m_buffer;
    uint8_t* m_rawBuffer;
    unsigned m_byteLength;
    unsigned m_byteOffset;
    unsigned m_arrayLength;
};

template <typename Adapter>
struct TypedArrayAdaptor {
    typedef typename Adapter::Type Type;
    static Type toNative(ExecutionState& state, const Value& val)
    {
        return Adapter::toNative(state, val);
    }
};

template <typename TypeArg>
struct IntegralTypedArrayAdapter {
    typedef TypeArg Type;

    static Type toNative(ExecutionState& state, const Value& val)
    {
        if (val.isInt32()) {
            return toNativeFromInt32(state, val.asInt32());
        } else if (val.isDouble()) {
            return toNativeFromDouble(state, val.asDouble());
        }
        auto number = val.toNumber(state);
        if (std::isnan(number)) {
            return 0;
        }
        return static_cast<Type>(number);
    }

    static TypeArg toNativeFromInt32(ExecutionState& state, int32_t value)
    {
        return static_cast<TypeArg>(value);
    }
    static TypeArg toNativeFromDouble(ExecutionState& state, double value)
    {
        int32_t result = static_cast<int32_t>(value);
        if (static_cast<double>(result) != value)
            result = Value(value).toInt32(state);
        return static_cast<TypeArg>(result);
    }
};

template <typename TypeArg>
struct FloatTypedArrayAdaptor {
    typedef TypeArg Type;

    static Type toNative(ExecutionState& state, const Value& val)
    {
        if (val.isInt32()) {
            return toNativeFromInt32(state, val.asInt32());
        } else if (val.isDouble()) {
            return toNativeFromDouble(state, val.asDouble());
        }
        return static_cast<Type>(val.toNumber(state));
    }

    static TypeArg toNativeFromInt32(ExecutionState& state, int32_t value)
    {
        return static_cast<TypeArg>(value);
    }
    static TypeArg toNativeFromDouble(ExecutionState& state, double value)
    {
        return value;
    }
};

struct Uint8ClampedAdaptor {
    typedef uint8_t Type;

    static Type toNative(ExecutionState& state, const Value& val)
    {
        if (val.isInt32()) {
            return toNativeFromInt32(state, val.asInt32());
        } else if (val.isDouble()) {
            return toNativeFromDouble(state, val.asDouble());
        }
        return toNativeFromDouble(state, val.toNumber(state));
    }

    static Type toNativeFromInt32(ExecutionState& state, int32_t value)
    {
        if (value < 0) {
            return 0;
        }
        if (value > 255) {
            return 255;
        }
        return static_cast<Type>(value);
    }

    static Type toNativeFromDouble(ExecutionState& state, double value)
    {
        if (std::isnan(value)) {
            return 0;
        }
        if (value < 0) {
            return 0;
        }
        if (value > 255) {
            return 255;
        }
        return static_cast<Type>(lrint(value));
    }
};

struct Int8Adaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<int8_t>> {
};
struct Int16Adaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<int16_t>> {
};
struct Int32Adaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<int32_t>> {
};
struct Uint8Adaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<uint8_t>> {
};
struct Uint16Adaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<uint16_t>> {
};
struct Uint32Adaptor : TypedArrayAdaptor<IntegralTypedArrayAdapter<uint32_t>> {
};
struct Float32Adaptor : TypedArrayAdaptor<FloatTypedArrayAdaptor<float>> {
};
struct Float64Adaptor : TypedArrayAdaptor<FloatTypedArrayAdaptor<double>> {
};

template <typename TypeAdaptor, int typedArrayElementSize>
class TypedArrayObject : public ArrayBufferView {
public:
    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override
    {
        uint64_t index = P.tryToUseAsIndex();
        if (LIKELY(Value::InvalidIndexValue != index)) {
            throwTypeErrorIfDetached(state);

            if ((unsigned)index < arrayLength()) {
                unsigned idxPosition = index * typedArrayElementSize;
                return ObjectHasPropertyResult(ObjectGetResult(getValueFromBuffer<typename TypeAdaptor::Type>(state, idxPosition), true, true, false));
            }
            return ObjectHasPropertyResult();
        }
        return ArrayBufferView::hasProperty(state, P);
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override
    {
        uint64_t index = P.tryToUseAsIndex();
        if (LIKELY(Value::InvalidIndexValue != index)) {
            throwTypeErrorIfDetached(state);

            if ((unsigned)index < arrayLength()) {
                unsigned idxPosition = index * typedArrayElementSize;
                return ObjectGetResult(getValueFromBuffer<typename TypeAdaptor::Type>(state, idxPosition), true, true, false);
            }
            return ObjectGetResult();
        }
        return ArrayBufferView::getOwnProperty(state, P);
    }

    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override
    {
        uint64_t index = P.tryToUseAsIndex();
        if (LIKELY(Value::InvalidIndexValue != index)) {
            throwTypeErrorIfDetached(state);

            if ((unsigned)index >= arrayLength())
                return false;
            unsigned idxPosition = index * typedArrayElementSize;

            if (LIKELY(desc.isValuePresentAlone() || desc.isDataWritableEnumerableConfigurable() || (desc.isWritable() && desc.isEnumerable()))) {
                setValueInBuffer<TypeAdaptor>(state, idxPosition, desc.value());
                return true;
            }
            return false;
        }
        return ArrayBufferView::defineOwnProperty(state, P, desc);
    }

    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override
    {
        return ArrayBufferView::deleteOwnProperty(state, P);
    }

    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE override
    {
        size_t len = arrayLength();
        for (size_t i = 0; i < len; i++) {
            unsigned idxPosition = i * typedArrayElementSize + byteOffset();
            if (!callback(state, this, ObjectPropertyName(state, Value(i)), ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent)), data)) {
                return;
            }
        }
        Object::enumeration(state, callback, data);
    }

    size_t elementSize()
    {
        return typedArrayElementSize;
    }

    virtual bool isTypedArrayObject() const override
    {
        return true;
    }

    virtual void sort(ExecutionState& state, int64_t length, const std::function<bool(const Value& a, const Value& b)>& comp) override
    {
        if (length) {
            Value* tempBuffer = (Value*)GC_MALLOC(sizeof(Value) * length);

            for (int64_t i = 0; i < length; i++) {
                unsigned idxPosition = i * typedArrayElementSize;
                tempBuffer[i] = getValueFromBuffer<typename TypeAdaptor::Type>(state, idxPosition);
            }

            TightVector<Value, GCUtil::gc_malloc_allocator<Value>> tempSpace;
            tempSpace.resizeWithUninitializedValues(length);
            mergeSort(tempBuffer, length, tempSpace.data(), [&](const Value& a, const Value& b, bool* lessOrEqualp) -> bool {
                *lessOrEqualp = comp(a, b);
                return true;
            });

            for (int64_t i = 0; i < length; i++) {
                unsigned idxPosition = i * typedArrayElementSize;
                setValueInBuffer<TypeAdaptor>(state, idxPosition, tempBuffer[i]);
            }
            GC_FREE(tempBuffer);

            return;
        }
    }

    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property) override
    {
        Value::ValueIndex idx = property.tryToUseAsIndex(state);
        if (LIKELY(idx != Value::InvalidIndexValue) && LIKELY((unsigned)idx < arrayLength())) {
            throwTypeErrorIfDetached(state);
            unsigned idxPosition = idx * typedArrayElementSize;
            return ObjectGetResult(getValueFromBuffer<typename TypeAdaptor::Type>(state, idxPosition), true, true, false);
        }
        return get(state, ObjectPropertyName(state, property));
    }

    ObjectHasPropertyResult hasIndexedProperty(ExecutionState& state, const Value& propertyName) override
    {
        Value::ValueIndex idx = propertyName.tryToUseAsIndex(state);
        if (LIKELY(idx != Value::InvalidIndexValue) && LIKELY((unsigned)idx < arrayLength())) {
            throwTypeErrorIfDetached(state);
            unsigned idxPosition = idx * typedArrayElementSize;
            return ObjectHasPropertyResult(ObjectGetResult(getValueFromBuffer<typename TypeAdaptor::Type>(state, idxPosition), true, true, false));
        }
        return hasProperty(state, ObjectPropertyName(state, propertyName));
    }

    virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value) override
    {
        Value::ValueIndex index = property.tryToUseAsIndex(state);
        if (LIKELY(Value::InvalidIndexValue != index) && LIKELY((unsigned)index < arrayLength())) {
            throwTypeErrorIfDetached(state);
            unsigned idxPosition = index * typedArrayElementSize;
            setValueInBuffer<TypeAdaptor>(state, idxPosition, value);
            return true;
        }
        return set(state, ObjectPropertyName(state, property), value, this);
    }

protected:
    explicit TypedArrayObject(ExecutionState& state, Object* proto)
        : ArrayBufferView(state, proto)
    {
    }
};

#define DECLARE_TYPEDARRAY(TYPE, type, siz)                                                                                                            \
    typedef TypedArrayObject<TYPE##Adaptor, siz> TYPE##ArrayObjectWrapper;                                                                             \
    class TYPE##ArrayObject : public TYPE##ArrayObjectWrapper {                                                                                        \
    public:                                                                                                                                            \
        explicit TYPE##ArrayObject(ExecutionState& state)                                                                                              \
            : TYPE##ArrayObject(state, state.context()->globalObject()->type##ArrayPrototype())                                                        \
        {                                                                                                                                              \
        }                                                                                                                                              \
        explicit TYPE##ArrayObject(ExecutionState& state, Object* proto)                                                                               \
            : TYPE##ArrayObjectWrapper(state, proto)                                                                                                   \
        {                                                                                                                                              \
        }                                                                                                                                              \
        virtual TypedArrayType typedArrayType() override                                                                                               \
        {                                                                                                                                              \
            return TypedArrayType::TYPE;                                                                                                               \
        }                                                                                                                                              \
        virtual String* typedArrayName(ExecutionState& state) override                                                                                 \
        {                                                                                                                                              \
            return state.context()->staticStrings().TYPE##Array.string();                                                                              \
        }                                                                                                                                              \
        static TYPE##ArrayObject* allocateTypedArray(ExecutionState& state, Object* newTarget, unsigned length = std::numeric_limits<unsigned>::max()) \
        {                                                                                                                                              \
            ASSERT(!!newTarget);                                                                                                                       \
            Object* proto = Object::getPrototypeFromConstructor(state, newTarget, [](ExecutionState& state, Context* constructorRealm) -> Object* {    \
                return constructorRealm->globalObject()->type##ArrayPrototype();                                                                       \
            });                                                                                                                                        \
            TYPE##ArrayObject* obj = new TYPE##ArrayObject(state, proto);                                                                              \
            if (length == std::numeric_limits<unsigned>::max()) {                                                                                      \
                obj->setBuffer(nullptr, 0, 0, 0);                                                                                                      \
            } else {                                                                                                                                   \
                auto buffer = ArrayBufferObject::allocateArrayBuffer(state, state.context()->globalObject()->arrayBuffer(), length * siz);             \
                obj->setBuffer(buffer, 0, length* siz, length);                                                                                        \
            }                                                                                                                                          \
            return obj;                                                                                                                                \
        }                                                                                                                                              \
    };

FOR_EACH_TYPEDARRAY(DECLARE_TYPEDARRAY)
#undef DECLARE_TYPEDARRAY

class TypedArrayPrototypeObject : public Object {
public:
    TypedArrayPrototypeObject(ExecutionState& state)
        : Object(state)
    {
    }

    virtual bool isTypedArrayPrototypeObject() const
    {
        return true;
    }
};
}

#endif

#ifndef __EscargotArrayObject__
#define __EscargotArrayObject__

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

#define ESCARGOT_ARRAY_NON_FASTMODE_MIN_SIZE 65536 * 2
#define ESCARGOT_ARRAY_NON_FASTMODE_START_MIN_GAP 1024


class ArrayObject : public Object {
    friend class Context;
    friend class ByteCodeInterpreter;
    friend Value builtinArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);

public:
    ArrayObject(ExecutionState& state);
    virtual bool isArrayObject()
    {
        return true;
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual void enumeration(ExecutionState& state, std::function<bool(const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc)> callback) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual uint64_t length(ExecutionState& state)
    {
        return getArrayLength(state);
    }
    virtual void sort(ExecutionState& state, std::function<bool(const Value& a, const Value& b)> comp);

    // Use custom allocator for Array object (for Badtime)
    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    static void iterateArrays(ExecutionState& state, HeapObjectIteratorCallback callback);
    static Value arrayLengthNativeGetter(ExecutionState& state, Object* self);
    static bool arrayLengthNativeSetter(ExecutionState& state, Object* self, const Value& newData);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Array";
    }

protected:
    bool isFastModeArray()
    {
        if (LIKELY(m_rareData == nullptr)) {
            return true;
        }
        return m_rareData->m_isFastModeArrayObject;
    }

    uint32_t getArrayLength(ExecutionState& state)
    {
        return m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER].toUint32(state);
    }

    bool setArrayLength(ExecutionState& state, const uint64_t& newLength);
    bool defineArrayLengthProperty(ExecutionState& state, const ObjectPropertyDescriptor& desc);
    void convertIntoNonFastMode(ExecutionState& state);

    ALWAYS_INLINE ObjectGetResult getFastModeValue(ExecutionState& state, const ObjectPropertyName& P)
    {
        if (LIKELY(isFastModeArray())) {
            uint64_t idx;
            if (LIKELY(P.isUIntType())) {
                idx = P.uintValue();
            } else {
                idx = P.string(state)->tryToUseAsArrayIndex();
            }
            if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
                ASSERT(m_fastModeData.size() == getArrayLength(state));
                if (LIKELY(idx < m_fastModeData.size())) {
                    Value v = m_fastModeData[idx];
                    if (LIKELY(!v.isEmpty())) {
                        return ObjectGetResult(v, true, true, true);
                    }
                    return ObjectGetResult();
                }
            }
        }
        return ObjectGetResult();
    }

    bool setFastModeValue(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc);

    ValueVector m_fastModeData;
};
}

#endif

#ifndef __EscargotArrayObject__
#define __EscargotArrayObject__

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

#define ESCARGOT_ARRAY_NON_FASTMODE_MIN_SIZE 65536 * 2
#define ESCARGOT_ARRAY_NON_FASTMODE_START_MIN_GAP 1024

extern size_t g_arrayObjectTag;

class ArrayObject : public Object {
    friend class Context;
    friend class Object;
    friend class ByteCodeInterpreter;
    friend Value builtinArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
    friend void initializeCustomAllocators();
    friend int getValidValueInArrayObject(void* ptr, GC_mark_custom_result* arr);

public:
    ArrayObject(ExecutionState& state);
    virtual bool isArrayObject() const
    {
        return true;
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual uint64_t length(ExecutionState& state)
    {
        return getArrayLength(state);
    }
    virtual void sort(ExecutionState& state, std::function<bool(const Value& a, const Value& b)> comp);
    virtual ObjectGetResult getIndexedProperty(ExecutionState& state, const Value& property);
    virtual bool setIndexedProperty(ExecutionState& state, const Value& property, const Value& value);

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
    ALWAYS_INLINE bool isFastModeArray()
    {
        if (LIKELY(rareData() == nullptr)) {
            return true;
        }
        return rareData()->m_isFastModeArrayObject;
    }

    ALWAYS_INLINE uint32_t getArrayLength(ExecutionState& state)
    {
        return m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER].toUint32(state);
    }

    bool setArrayLength(ExecutionState& state, const uint64_t& newLength);
    bool defineArrayLengthProperty(ExecutionState& state, const ObjectPropertyDescriptor& desc);
    void convertIntoNonFastMode(ExecutionState& state);

    ObjectGetResult getFastModeValue(ExecutionState& state, const ObjectPropertyName& P);
    bool setFastModeValue(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc);

    VectorWithNoSize<SmallValue, gc_malloc_ignore_off_page_allocator<SmallValue>> m_fastModeData;
};
}

#endif

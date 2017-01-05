#ifndef __EscargotArrayBufferObject__
#define __EscargotArrayBufferObject__

#if ESCARGOT_ENABLE_TYPEDARRAY

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

enum TypedArrayType {
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array
};

class ArrayBufferObject : public Object {
public:
    ArrayBufferObject(ExecutionState& state);
    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "ArrayBuffer";
    }

    void allocateBuffer(size_t bytelength);

    ALWAYS_INLINE const uint8_t* data() { return m_data; }
    ALWAYS_INLINE unsigned bytelength() { return m_bytelength; }
    // $24.1.1.5
    template <typename Type>
    Value getValueFromBuffer(unsigned byteindex, bool isLittleEndian = 1)
    {
        // If isLittleEndian is not present, set isLittleEndian to either true or false.
        ASSERT(m_bytelength);
        size_t elementSize = sizeof(Type);
        ASSERT(byteindex + elementSize <= m_bytelength);
        uint8_t* rawStart = m_data + byteindex;
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
    // $24.1.1.6
    template <typename TypeAdaptor>
    bool setValueInBuffer(unsigned byteindex, Value val, bool isLittleEndian = 1)
    {
        // If isLittleEndian is not present, set isLittleEndian to either true or false.
        ASSERT(m_bytelength);
        size_t elementSize = sizeof(typename TypeAdaptor::Type);
        ASSERT(byteindex + elementSize <= m_bytelength);
        uint8_t* rawStart = m_data + byteindex;
        typename TypeAdaptor::Type littleEndianVal = TypeAdaptor::toNative(val);
        if (isLittleEndian != 1) {
            for (size_t i = 0; i < elementSize; i++) {
                rawStart[i] = ((uint8_t*)&littleEndianVal)[elementSize - i - 1];
            }
        } else {
            *((typename TypeAdaptor::Type*)rawStart) = littleEndianVal;
        }
        return true;
    }

private:
    uint8_t* m_data;
    unsigned m_bytelength;
};
}

#endif

#endif

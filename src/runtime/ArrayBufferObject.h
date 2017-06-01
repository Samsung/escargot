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

#ifndef __EscargotArrayBufferObject__
#define __EscargotArrayBufferObject__

#if ESCARGOT_ENABLE_TYPEDARRAY

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

typedef void* (*ArrayBufferObjectBufferMallocFunction)(size_t siz);
typedef void (*ArrayBufferObjectBufferFreeFunction)(void* buffer);

extern ArrayBufferObjectBufferMallocFunction g_arrayBufferObjectBufferMallocFunction;
extern bool g_arrayBufferObjectBufferMallocFunctionNeedsZeroFill;
extern ArrayBufferObjectBufferFreeFunction g_arrayBufferObjectBufferFreeFunction;

class ArrayBufferObject : public Object {
public:
    ArrayBufferObject(ExecutionState& state);
    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "ArrayBuffer";
    }

    // Clone srcBuffer's srcByteOffset ~ end.
    bool cloneBuffer(ArrayBufferObject* srcBuffer, size_t srcByteOffset);
    // Clone srcBuffer's srcByteOffset ~ (srcByteOffset + cloneLength).
    bool cloneBuffer(ArrayBufferObject* srcBuffer, size_t srcByteOffset, size_t cloneLength);
    void allocateBuffer(size_t bytelength);
    void attachBuffer(void* buffer, size_t bytelength);

    virtual bool isArrayBufferObject() const
    {
        return true;
    }

    ALWAYS_INLINE const uint8_t* data() { return m_data; }
    ALWAYS_INLINE unsigned bytelength() { return m_bytelength; }
    // $24.1.1.5
    template <typename Type>
    Value getValueFromBuffer(ExecutionState& state, unsigned byteindex, bool isLittleEndian = 1)
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
    bool setValueInBuffer(ExecutionState& state, unsigned byteindex, Value val, bool isLittleEndian = 1)
    {
        // If isLittleEndian is not present, set isLittleEndian to either true or false.
        ASSERT(m_bytelength);
        size_t elementSize = sizeof(typename TypeAdaptor::Type);
        ASSERT(byteindex + elementSize <= m_bytelength);
        uint8_t* rawStart = m_data + byteindex;
        typename TypeAdaptor::Type littleEndianVal = TypeAdaptor::toNative(state, val);
        if (isLittleEndian != 1) {
            for (size_t i = 0; i < elementSize; i++) {
                rawStart[i] = ((uint8_t*)&littleEndianVal)[elementSize - i - 1];
            }
        } else {
            *((typename TypeAdaptor::Type*)rawStart) = littleEndianVal;
        }
        return true;
    }

    bool isDetachedBuffer()
    {
        if (data() == NULL)
            return true;
        return false;
    }

    void detachArrayBuffer()
    {
        free(m_data);
        m_data = NULL;
        m_bytelength = 0;
    }

    void fillData(const uint8_t* data, unsigned length)
    {
        ASSERT(!isDetachedBuffer());
        memcpy(m_data, data, length);
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    uint8_t* m_data;
    unsigned m_bytelength;
};
}

#endif

#endif

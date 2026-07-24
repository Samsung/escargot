/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

#if defined(ENABLE_THREADING)

#ifndef __EscargotSerializedSharedArrayBufferObjectValue__
#define __EscargotSerializedSharedArrayBufferObjectValue__

#include "runtime/serialization/SerializedValue.h"
#include "runtime/SharedArrayBufferObject.h"

namespace Escargot {

class SerializedSharedArrayBufferObjectValue : public SerializedValue {
    friend class Serializer;

public:
    virtual Type type() override
    {
        return SerializedValue::SharedArrayBufferObject;
    }

    virtual Value toValue(ExecutionState& state) override
    {
        return Value(new ::Escargot::SharedArrayBufferObject(state, state.context()->globalObject()->sharedArrayBufferPrototype(), m_bufferData));
    }

protected:
    virtual void serializeValueData(std::string& output) override
    {
        using namespace SerializerDetail;
        size_t byteLength = m_bufferData->byteLength();
        writePOD(output, byteLength);
        writeBytes(output, m_bufferData->data(), byteLength);
    }

    static std::unique_ptr<SerializedValue> deserializeFrom(SerializerDetail::Reader& reader)
    {
        size_t size = 0;
        reader.readPOD(size);
        BackingStore* bs = SharedBackingStore::createDefaultSharedBackingStore(size);
        reader.readBytes(bs->data(), size);
        return std::unique_ptr<SerializedValue>(new SerializedSharedArrayBufferObjectValue(bs->sharedDataBlockInfo()));
    }

    SerializedSharedArrayBufferObjectValue(SharedDataBlockInfo* bufferData)
        : m_bufferData(bufferData)
    {
        m_bufferData->ref();
    }

    ~SerializedSharedArrayBufferObjectValue()
    {
        m_bufferData->deref();
    }

    SharedDataBlockInfo* m_bufferData;
};

} // namespace Escargot

#endif
#endif

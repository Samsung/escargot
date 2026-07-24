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

#ifndef __EscargotSerializedBigIntValue__
#define __EscargotSerializedBigIntValue__

#include "runtime/serialization/SerializedValue.h"
#include "runtime/BigInt.h"

namespace Escargot {

class SerializedBigIntValue : public SerializedValue {
    friend class Serializer;

public:
    virtual Type type() override
    {
        return SerializedValue::BigInt;
    }

    virtual Value toValue(ExecutionState& state) override
    {
        BigIntData d(m_value.data(), m_value.size());
        return Value(new ::Escargot::BigInt(std::move(d)));
    }

protected:
    virtual void serializeValueData(std::string& output) override
    {
        using namespace SerializerDetail;
        size_t s = m_value.size();
        writePOD(output, s);
        writeBytes(output, m_value.data(), s);
    }

    static std::unique_ptr<SerializedValue> deserializeFrom(SerializerDetail::Reader& reader)
    {
        size_t s = 0;
        reader.readPOD(s);
        std::string str;
        reader.readString(str, s);
        return std::unique_ptr<SerializedValue>(new SerializedBigIntValue(std::move(str)));
    }

    SerializedBigIntValue(std::string&& value)
        : m_value(value)
    {
    }
    std::string m_value;
};

} // namespace Escargot

#endif

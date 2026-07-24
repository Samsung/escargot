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

#ifndef __EscargotSerializedSymbolValue__
#define __EscargotSerializedSymbolValue__

#include "runtime/serialization/SerializedValue.h"

namespace Escargot {

class SerializedSymbolValue : public SerializedValue {
    friend class Serializer;

public:
    virtual Type type() override
    {
        return SerializedValue::Symbol;
    }

    virtual Value toValue(ExecutionState& state) override
    {
        if (m_value) {
            return Value(new ::Escargot::Symbol(
                String::fromUTF8(m_value.value().data(), m_value.value().size())));
        }
        return Value(new ::Escargot::Symbol());
    }

protected:
    virtual void serializeValueData(std::string& output) override
    {
        using namespace SerializerDetail;
        uint8_t hasValue = m_value ? 1 : 0;
        writePOD(output, hasValue);
        if (m_value) {
            size_t s = m_value.value().size();
            writePOD(output, s);
            writeBytes(output, m_value.value().data(), s);
        }
    }

    static std::unique_ptr<SerializedValue> deserializeFrom(SerializerDetail::Reader& reader)
    {
        uint8_t hasValue = 0;
        reader.readPOD(hasValue);
        if (hasValue) {
            size_t s = 0;
            reader.readPOD(s);
            std::string str;
            reader.readString(str, s);
            return std::unique_ptr<SerializedValue>(new SerializedSymbolValue(std::move(str)));
        }
        return std::unique_ptr<SerializedValue>(new SerializedSymbolValue());
    }

    SerializedSymbolValue(std::string&& value)
        : m_value(value)
    {
    }

    SerializedSymbolValue()
    {
    }

    Optional<std::string> m_value;
};

} // namespace Escargot

#endif

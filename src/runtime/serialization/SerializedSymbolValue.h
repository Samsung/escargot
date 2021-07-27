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
    virtual void serializeValueData(std::ostringstream& outputStream) override
    {
        if (m_value) {
            outputStream << true;
            outputStream << std::endl;
            size_t s = m_value.value().size();
            outputStream << s;
            outputStream << std::endl;
            for (size_t i = 0; i < s; i++) {
                outputStream << (m_value.value())[i];
            }
        } else {
            outputStream << false;
            outputStream << std::endl;
        }
    }

    static std::unique_ptr<SerializedValue> deserializeFrom(std::istringstream& inputStream)
    {
        bool hasValue;
        inputStream >> hasValue;
        if (hasValue) {
            size_t s;
            inputStream >> s;
            std::string str;
            str.reserve(s);

            for (size_t i = 0; i < s; i++) {
                char ch;
                inputStream >> ch;
                str.push_back(ch);
            }

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

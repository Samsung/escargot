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

#ifndef __EscargotSerializedNumberValue__
#define __EscargotSerializedNumberValue__

#include "runtime/serialization/SerializedValue.h"

namespace Escargot {

class SerializedNumberValue : public SerializedValue {
    friend class Serializer;

public:
    virtual Type type() override
    {
        return SerializedValue::Number;
    }

    virtual Value toValue(ExecutionState& state) override
    {
        return Value(Value::DoubleToIntConvertibleTestNeeds, m_value);
    }

protected:
    virtual void serializeValueData(std::string& output) override
    {
        using namespace SerializerDetail;
        writePOD(output, m_value);
    }

    static std::unique_ptr<SerializedValue> deserializeFrom(SerializerDetail::Reader& reader)
    {
        double v = 0;
        reader.readPOD(v);
        return std::unique_ptr<SerializedValue>(new SerializedNumberValue(v));
    }

    SerializedNumberValue(double value)
        : m_value(value)
    {
    }
    double m_value;
};

} // namespace Escargot

#endif

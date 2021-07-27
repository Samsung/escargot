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

#ifndef __EscargotSerializedValue__
#define __EscargotSerializedValue__

#include "runtime/Value.h"

namespace Escargot {

class SerializedValue {
    friend class Serializer;

public:
#define FOR_EACH_SERIALIZABLE_TYPE(F) \
    F(Undefined)                      \
    F(Null)                           \
    F(Boolean)                        \
    F(Number)                         \
    F(String)                         \
    F(Symbol)                         \
    F(BigInt)

    enum Type {
#define DECLARE_SERIALIZABLE_TYPE(name) name,
        FOR_EACH_SERIALIZABLE_TYPE(DECLARE_SERIALIZABLE_TYPE)
#undef DECLARE_SERIALIZABLE_TYPE
#if defined(ENABLE_THREADING)
            SharedArrayBufferObject
#endif
    };
    virtual ~SerializedValue() {}
    virtual Type type() = 0;
    virtual Value toValue(ExecutionState& state) = 0;

    void serializeInto(std::ostringstream& outputStream)
    {
        serializeValueType(outputStream);
        serializeValueData(outputStream);
    }

protected:
    virtual void serializeValueData(std::ostringstream& outputStream) {}
    void serializeValueType(std::ostringstream& outputStream)
    {
        outputStream << static_cast<char>(type());
    }
};

} // namespace Escargot

#endif

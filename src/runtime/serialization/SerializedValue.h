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
    enum Type {
        Undefined,
        Null,
        Boolean,
        Number,
        String,
    };
    virtual ~SerializedValue() {}
    virtual Type type() = 0;
    virtual Value toValue() = 0;

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

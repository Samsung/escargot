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

#ifndef __EscargotSerializer__
#define __EscargotSerializer__

#include "runtime/Value.h"
#include "runtime/serialization/SerializedValue.h"

namespace Escargot {

class Serializer {
public:
    // this function can return nullptr if serialize failed
    static std::unique_ptr<SerializedValue> serialize(const Value& value);
    // returns the serialization was successful
    static bool serializeInto(const Value& value, std::ostringstream& output);

    static std::unique_ptr<SerializedValue> deserializeFrom(std::istringstream& input);
};

} // namespace Escargot

#endif

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

#include "Escargot.h"
#include "Serializer.h"

#include "runtime/serialization/SerializedBigIntValue.h"
#include "runtime/serialization/SerializedBooleanValue.h"
#include "runtime/serialization/SerializedNullValue.h"
#include "runtime/serialization/SerializedNumberValue.h"
#include "runtime/serialization/SerializedSharedArrayBufferObjectValue.h"
#include "runtime/serialization/SerializedStringValue.h"
#include "runtime/serialization/SerializedSymbolValue.h"
#include "runtime/serialization/SerializedUndefinedValue.h"

namespace Escargot {

std::unique_ptr<SerializedValue> Serializer::serialize(const Value& value)
{
    if (value.isUndefined()) {
        return std::unique_ptr<SerializedValue>(new SerializedUndefinedValue());
    } else if (value.isNull()) {
        return std::unique_ptr<SerializedValue>(new SerializedNullValue());
    } else if (value.isBoolean()) {
        return std::unique_ptr<SerializedValue>(new SerializedBooleanValue(value.asBoolean()));
    } else if (value.isNumber()) {
        return std::unique_ptr<SerializedValue>(new SerializedNumberValue(value.asNumber()));
    } else if (value.isString()) {
        return std::unique_ptr<SerializedValue>(new SerializedStringValue(value.asString()->toNonGCUTF8StringData()));
    } else if (value.isBigInt()) {
        return std::unique_ptr<SerializedValue>(new SerializedBigIntValue(value.asBigInt()->toString()->toNonGCUTF8StringData()));
    } else if (value.isSymbol()) {
        if (value.asSymbol()->descriptionString()->length()) {
            return std::unique_ptr<SerializedValue>(new SerializedSymbolValue(value.asSymbol()->descriptionString()->toNonGCUTF8StringData()));
        } else {
            return std::unique_ptr<SerializedValue>(new SerializedSymbolValue());
        }
    } else if (value.isObject()) {
#if defined(ENABLE_THREADING)
        if (value.asObject()->isSharedArrayBufferObject()) {
            return std::unique_ptr<SerializedValue>(new SerializedSharedArrayBufferObjectValue(
                value.asObject()->asSharedArrayBufferObject()->backingStore()->sharedDataBlockInfo()));
        }
#endif
    }

    return nullptr;
}

bool Serializer::serializeInto(const Value& value, std::string& output)
{
    auto sv = serialize(value);
    if (sv) {
        sv->serializeInto(output);
        return true;
    }
    return false;
}

std::unique_ptr<SerializedValue> Serializer::deserializeFrom(const char* data, size_t len, size_t& offset)
{
    SerializerDetail::Reader reader(data, len, offset);
    uint8_t type = 0;
    if (!reader.readPOD(type)) {
        return nullptr;
    }
    std::unique_ptr<SerializedValue> result;
    switch (type) {
#define DECLARE_SERIALIZABLE_TYPE(name)                                 \
    case SerializedValue::Type::name:                                  \
        result = Serialized##name##Value::deserializeFrom(reader); \
        break;
        FOR_EACH_SERIALIZABLE_TYPE(DECLARE_SERIALIZABLE_TYPE)
#undef DECLARE_SERIALIZABLE_TYPE
#if defined(ENABLE_THREADING)
    case SerializedValue::Type::SharedArrayBufferObject:
        result = SerializedSharedArrayBufferObjectValue::deserializeFrom(reader);
        break;
#endif
    default:
        break;
    }
    offset = reader.pos;
    return result;
}

} // namespace Escargot
